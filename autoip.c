#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdlib.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

#define NUM_CANDIDATES 5
#define PING_TIMEOUT 2000
#define MAX_ROUNDS 3

typedef struct {
    DWORD ip;
    BOOL reachable;
} PingResult;

typedef struct {
    WCHAR name[256];
    DWORD ip;
    DWORD mask;
    DWORD gateway;
    DWORD dns;
    BOOL hasGateway;
    BOOL hasDns;
    BOOL found;
} AdapterConfig;

static DWORD WINAPI PingThread(LPVOID param) {
    PingResult *r = (PingResult *)param;
    HANDLE h = IcmpCreateFile();
    if (h == INVALID_HANDLE_VALUE) {
        r->reachable = TRUE;
        return 1;
    }
    unsigned char data[32] = {0};
    DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(data) + 8;
    void *buf = malloc(replySize);
    if (!buf) {
        IcmpCloseHandle(h);
        r->reachable = TRUE;
        return 1;
    }
    DWORD ret = IcmpSendEcho(h, r->ip, data, sizeof(data), NULL, buf, replySize, PING_TIMEOUT);
    r->reachable = (ret > 0);
    free(buf);
    IcmpCloseHandle(h);
    return 0;
}

static void ReadConfig(AdapterConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    ULONG len = 0;
    ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_PREFIX;
    if (GetAdaptersAddresses(AF_INET, flags, NULL, NULL, &len) != ERROR_BUFFER_OVERFLOW) return;
    void *buf = malloc(len);
    if (!buf) return;
    if (GetAdaptersAddresses(AF_INET, flags, NULL, (PIP_ADAPTER_ADDRESSES)buf, &len) != NO_ERROR) {
        free(buf);
        return;
    }
    PIP_ADAPTER_ADDRESSES a = (PIP_ADAPTER_ADDRESSES)buf;
    while (a) {
        if (a->OperStatus != IfOperStatusUp || a->IfType != IF_TYPE_ETHERNET_CSMACD) {
            a = a->Next;
            continue;
        }
        if (a->FriendlyName && wcsstr(a->FriendlyName, L"VMware")) {
            a = a->Next;
            continue;
        }
        PIP_ADAPTER_UNICAST_ADDRESS ua = a->FirstUnicastAddress;
        if (!ua) { a = a->Next; continue; }
        struct sockaddr_in *sa = (struct sockaddr_in *)ua->Address.lpSockaddr;
        cfg->ip = sa->sin_addr.S_un.S_addr;
        BYTE pfx = ua->OnLinkPrefixLength;
        DWORD m = 0;
        if (pfx > 0 && pfx < 32) m = htonl(~((1U << (32 - pfx)) - 1));
        else if (pfx == 32) m = htonl(0xFFFFFFFF);
        cfg->mask = m;
        wcscpy_s(cfg->name, 256, a->FriendlyName);
        PIP_ADAPTER_GATEWAY_ADDRESS gw = a->FirstGatewayAddress;
        if (gw) {
            struct sockaddr_in *gsa = (struct sockaddr_in *)gw->Address.lpSockaddr;
            cfg->gateway = gsa->sin_addr.S_un.S_addr;
            cfg->hasGateway = TRUE;
        }
        PIP_ADAPTER_DNS_SERVER_ADDRESS dns = a->FirstDnsServerAddress;
        if (dns) {
            struct sockaddr_in *dsa = (struct sockaddr_in *)dns->Address.lpSockaddr;
            cfg->dns = dsa->sin_addr.S_un.S_addr;
            cfg->hasDns = TRUE;
        }
        cfg->found = TRUE;
        break;
    }
    free(buf);
}

static BOOL RunCommand(LPCWSTR cmd) {
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    WCHAR full[1024];
    wsprintfW(full, L"cmd.exe /c %s", cmd);
    if (!CreateProcessW(NULL, full, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return FALSE;
    WaitForSingleObject(pi.hProcess, 30000);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (code == 0);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    srand(GetTickCount());
    AdapterConfig cfg;
    ReadConfig(&cfg);
    if (!cfg.found) {
        MessageBoxW(NULL, L"未找到活动的以太网适配器", L"自动设IP", MB_OK | MB_ICONERROR);
        return 1;
    }
    BYTE *ipB = (BYTE *)&cfg.ip;
    BYTE a1 = ipB[0], a2 = ipB[1], a3 = ipB[2], a4 = ipB[3];
    BYTE gwLast = 255;
    if (cfg.hasGateway) {
        BYTE *gwB = (BYTE *)&cfg.gateway;
        gwLast = gwB[3];
    }
    int used[256] = {0};
    used[a4] = 1;
    used[gwLast] = 1;
    used[0] = 1;
    used[255] = 1;

    DWORD newIp = 0;
    BOOL found = FALSE;

    for (int round = 0; round < MAX_ROUNDS && !found; round++) {
        PingResult results[NUM_CANDIDATES];
        HANDLE threads[NUM_CANDIDATES];
        for (int i = 0; i < NUM_CANDIDATES; i++) {
            int last;
            do {
                last = 1 + rand() % 253;
            } while (used[last]);
            used[last] = 1;
            results[i].ip = (cfg.ip & 0x00FFFFFF) | ((DWORD)last << 24);
            results[i].reachable = TRUE;
            threads[i] = CreateThread(NULL, 0, PingThread, &results[i], 0, NULL);
            if (!threads[i]) results[i].reachable = TRUE;
        }
        int validCount = 0;
        HANDLE validHandles[NUM_CANDIDATES];
        for (int i = 0; i < NUM_CANDIDATES; i++) {
            if (threads[i]) validHandles[validCount++] = threads[i];
        }
        if (validCount > 0)
            WaitForMultipleObjects(validCount, validHandles, TRUE, 15000);
        for (int i = 0; i < NUM_CANDIDATES; i++) {
            if (threads[i]) CloseHandle(threads[i]);
        }
        int unreach[NUM_CANDIDATES];
        int count = 0;
        for (int i = 0; i < NUM_CANDIDATES; i++) {
            if (!results[i].reachable)
                unreach[count++] = i;
        }
        if (count > 0) {
            int pick = unreach[rand() % count];
            newIp = results[pick].ip;
            found = TRUE;
        }
    }

    if (!found) {
        MessageBoxW(NULL, L"未能找到可用IP，请重试", L"自动设IP", MB_OK | MB_ICONWARNING);
        return 1;
    }

    BYTE *nB = (BYTE *)&newIp;
    BYTE *mB = (BYTE *)&cfg.mask;
    WCHAR addrCmd[512];
    if (cfg.hasGateway) {
        BYTE *gB = (BYTE *)&cfg.gateway;
        wsprintfW(addrCmd,
            L"netsh interface ip set address name=\"%s\" source=static addr=%d.%d.%d.%d mask=%d.%d.%d.%d gateway=%d.%d.%d.%d gwmetric=1",
            cfg.name, nB[0], nB[1], nB[2], nB[3], mB[0], mB[1], mB[2], mB[3],
            gB[0], gB[1], gB[2], gB[3]);
    } else {
        wsprintfW(addrCmd,
            L"netsh interface ip set address name=\"%s\" source=static addr=%d.%d.%d.%d mask=%d.%d.%d.%d",
            cfg.name, nB[0], nB[1], nB[2], nB[3], mB[0], mB[1], mB[2], mB[3]);
    }
    BOOL ok = RunCommand(addrCmd);

    if (cfg.hasDns) {
        BYTE *dB = (BYTE *)&cfg.dns;
        WCHAR dnsCmd[512];
        wsprintfW(dnsCmd,
            L"netsh interface ip set dns name=\"%s\" source=static addr=%d.%d.%d.%d register=primary",
            cfg.name, dB[0], dB[1], dB[2], dB[3]);
        RunCommand(dnsCmd);
    }

    WCHAR msg[512];
    if (ok) {
        wsprintfW(msg, L"IP地址已更新\n\n旧IP: %d.%d.%d.%d\n新IP: %d.%d.%d.%d\n适配器: %s",
            a1, a2, a3, a4, nB[0], nB[1], nB[2], nB[3], cfg.name);
        MessageBoxW(NULL, msg, L"自动设IP - 成功", MB_OK | MB_ICONINFORMATION);
    } else {
        wsprintfW(msg, L"设置IP失败\n\n目标IP: %d.%d.%d.%d", nB[0], nB[1], nB[2], nB[3]);
        MessageBoxW(NULL, msg, L"自动设IP - 失败", MB_OK | MB_ICONERROR);
    }
    return 0;
}
