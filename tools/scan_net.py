"""AutoIP 网段探测工具

两种模式，判定逻辑与 AutoIP 程序完全一致：
  清该地址邻居缓存 -> SendARP -> 无响应补 1 包 ICMP -> 两者都无才判空闲

用法：
    python scan_net.py                    # 全段扫描（低并发，准但慢）
    python scan_net.py --rounds 5         # 批次抽样 5 轮（与程序策略一致，快）
    python scan_net.py --first 1 --last 253 --net 10.172.201 --workers 6

为什么默认低并发：不少教室网关会替全网段代答 ARP（VRRP 虚拟 MAC）且有速率
限制，并发一高就丢包，导致把在用的地址判成空闲。实测 253 并发时 121 个在线
地址只探出 70 个。
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import os
import socket
import struct
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

iphlpapi = ctypes.WinDLL("iphlpapi.dll")

MAXLEN_PHYSADDR = 8
MIB_IPNET_TYPE_DYNAMIC = 3


class MIB_IPNETROW(ctypes.Structure):
    _fields_ = [
        ("dwIndex", wt.DWORD),
        ("dwPhysAddrLen", wt.DWORD),
        ("bPhysAddr", ctypes.c_ubyte * MAXLEN_PHYSADDR),
        ("dwAddr", wt.DWORD),
        ("dwType", wt.DWORD),
    ]


iphlpapi.SendARP.argtypes = [wt.ULONG, wt.ULONG, ctypes.POINTER(wt.ULONG), ctypes.POINTER(wt.ULONG)]
iphlpapi.SendARP.restype = wt.DWORD
iphlpapi.DeleteIpNetEntry.argtypes = [ctypes.POINTER(MIB_IPNETROW)]
iphlpapi.DeleteIpNetEntry.restype = wt.DWORD
iphlpapi.GetBestInterface.argtypes = [wt.ULONG, ctypes.POINTER(wt.ULONG)]
iphlpapi.GetBestInterface.restype = wt.DWORD


def aton(ip):
    return struct.unpack("<I", socket.inet_aton(ip))[0]


def if_index_for(ip):
    idx = wt.ULONG()
    if iphlpapi.GetBestInterface(aton(ip), ctypes.byref(idx)) != 0:
        return 0
    return idx.value


def purge_neighbor(ifindex, ip):
    """SendARP 命中本机邻居表会直接返回旧 MAC，先删掉逼它真的发包。"""
    row = MIB_IPNETROW()
    row.dwIndex = ifindex
    row.dwPhysAddrLen = 6
    row.dwAddr = aton(ip)
    row.dwType = MIB_IPNET_TYPE_DYNAMIC
    iphlpapi.DeleteIpNetEntry(ctypes.byref(row))


def send_arp(ip):
    mac = (wt.ULONG * 2)(0, 0)
    length = wt.ULONG(6)
    if iphlpapi.SendARP(aton(ip), 0, mac, ctypes.byref(length)) != 0 or length.value != 6:
        return None
    raw = struct.pack("<II", mac[0], mac[1])[:6]
    if raw == b"\x00" * 6:
        return None
    return ":".join(f"{b:02X}" for b in raw)


def ping_once(ip, timeout_ms=800):
    r = subprocess.run(
        ["ping", "-n", "1", "-w", str(timeout_ms), ip],
        capture_output=True,
    )
    return r.returncode == 0


def probe(net, last, ifindex):
    """返回 (last, kind, mac)，kind: arp / icmp / free"""
    ip = f"{net}.{last}"
    purge_neighbor(ifindex, ip)
    mac = send_arp(ip)
    if mac:
        return last, "arp", mac
    if ping_once(ip):
        return last, "icmp", None
    return last, "free", None


def do_full_scan(net, first, last, ifindex, workers):
    targets = list(range(first, last + 1))
    print(f"全段扫描 {net}.{first} - {net}.{last}，共 {len(targets)} 个，并发 {workers}")
    t0 = time.time()
    with ThreadPoolExecutor(max_workers=workers) as pool:
        results = list(pool.map(lambda n: probe(net, n, ifindex), targets))
    elapsed = time.time() - t0

    occupied = [(n, mac) for n, kind, mac in results if kind == "arp"]
    icmp_only = [n for n, kind, _ in results if kind == "icmp"]
    free = [n for n, kind, _ in results if kind == "free"]

    print(f"耗时 {elapsed:.1f}s\n")
    print("=" * 60)
    print(f"ARP 判定占用  : {len(occupied)}")
    print(f"仅 ICMP 有响应: {len(icmp_only)}")
    print(f"空闲          : {len(free)}")
    print("=" * 60)

    if icmp_only:
        print("\n仅 ICMP 有响应（ARP 静默，可能是安全软件或跨网段）:")
        print("  " + ", ".join(f"{net}.{n}" for n in icmp_only))

    print("\n--- 空闲地址 ---")
    for i in range(0, len(free), 10):
        print("  " + "  ".join(f"{net}.{n}" for n in free[i:i + 10]))


def do_batch_rounds(net, first, last, ifindex, rounds, batch):
    span = last - first + 1
    print(f"批次抽样 {rounds} 轮，每轮随机 {batch} 个（与 AutoIP 程序策略一致）\n")
    total_free = 0
    for rnd in range(1, rounds + 1):
        picks, seen = [], set()
        while len(picks) < batch:
            n = first + int.from_bytes(os.urandom(2), "little") % span
            if n in seen:
                continue
            seen.add(n)
            picks.append(n)

        t0 = time.time()
        with ThreadPoolExecutor(max_workers=batch) as pool:
            results = list(pool.map(lambda n: probe(net, n, ifindex), picks))
        elapsed = time.time() - t0

        free = [n for n, kind, _ in results if kind == "free"]
        total_free += len(free)
        detail = "  ".join(
            f"{net}.{n} {'空' if kind == 'free' else '占'}" for n, kind, _ in results
        )
        print(f"第 {rnd:2} 轮  {elapsed:4.1f}s  空闲 {len(free)}/{batch}   {detail}")

    print()
    print(f"合计: 探测 {rounds * batch} 个，平均每轮空闲 {total_free / rounds:.1f}/{batch}")


def main():
    ap = argparse.ArgumentParser(description="AutoIP 网段探测工具")
    ap.add_argument("--net", default="10.172.201", help="网段前缀，默认 10.172.201")
    ap.add_argument("--first", type=int, default=1)
    ap.add_argument("--last", type=int, default=253)
    ap.add_argument("--workers", type=int, default=6, help="全段扫描并发，默认 6，别调太高")
    ap.add_argument("--rounds", type=int, default=0, help=">0 则改用批次抽样模式")
    ap.add_argument("--batch", type=int, default=10, help="批次抽样每轮个数，默认 10")
    args = ap.parse_args()

    ifindex = if_index_for(f"{args.net}.{args.first}")
    if ifindex == 0:
        print("无法确定出接口，检查本机是否在该网段", file=sys.stderr)
        return 1
    print(f"接口索引: {ifindex}\n")

    if args.rounds > 0:
        do_batch_rounds(args.net, args.first, args.last, ifindex, args.rounds, args.batch)
    else:
        do_full_scan(args.net, args.first, args.last, ifindex, args.workers)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
