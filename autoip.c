#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

static DWORD WINAPI PingThread(LPVOID param) {
    PingResult *r = (PingResult *)param;
    HANDLE h = IcmpCreateFile();
    if (h == INVALID_HANDLE_VALUE) { r->reachable = TRUE; return 1; }
    unsigned char data[32] = {0};
    DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(data) + 8;
    void *buf = malloc(replySize);
    if (!buf) { IcmpCloseHandle(h); r->reachable = TRUE; return 1; }
    DWORD ret = IcmpSendEcho(h, r->ip, data, sizeof(data), NULL, buf, replySize, PING_TIMEOUT);
    r->reachable = (ret > 0);
    free(buf);
    IcmpCloseHandle(h);
    return 0;
}

typedef struct {
    WCHAR name[256];
    char ipStr[32];
    char maskStr[32];
    char gwStr[32];
    char dnsStr[32];
    BOOL hasGateway;
    BOOL hasDns;
    BOOL found;
} AdapterConfig;

static void ParseIP4(const char *str, int *a, int *b, int *c, int *d) {
    *a = *b = *c = *d = 0;
    sscanf(str, "%d.%d.%d.%d", a, b, c, d);
}

static void ReadConfig(AdapterConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));

    ULONG outBufLen = 0;
    if (GetAdaptersInfo(NULL, &outBufLen) != ERROR_BUFFER_OVERFLOW) return;
    void *pBuf = malloc(outBufLen);
    if (!pBuf) return;
    if (GetAdaptersInfo((PIP_ADAPTER_INFO)pBuf, &outBufLen) != NO_ERROR) { free(pBuf); return; }

    char targetGuid[256] = {0};
    PIP_ADAPTER_INFO pInfo = (PIP_ADAPTER_INFO)pBuf;
    while (pInfo) {
        if (pInfo->Type != MIB_IF_TYPE_ETHERNET) { pInfo = pInfo->Next; continue; }
        if (strstr(pInfo->Description, "VMware")) { pInfo = pInfo->Next; continue; }
        if (strcmp(pInfo->IpAddressList.IpAddress.String, "0.0.0.0") == 0) { pInfo = pInfo->Next; continue; }

        strcpy_s(cfg->ipStr, 32, pInfo->IpAddressList.IpAddress.String);
        strcpy_s(cfg->maskStr, 32, pInfo->IpAddressList.IpMask.String);
        strcpy_s(cfg->gwStr, 32, pInfo->GatewayList.IpAddress.String);
        if (cfg->gwStr[0] && strcmp(cfg->gwStr, "0.0.0.0") != 0) cfg->hasGateway = TRUE;
        strcpy_s(targetGuid, 256, pInfo->AdapterName);

        ULONG perLen = 4096;
        void *pPer = malloc(perLen);
        if (pPer) {
            if (GetPerAdapterInfo(pInfo->Index, (PIP_PER_ADAPTER_INFO)pPer, &perLen) == NO_ERROR) {
                PIP_PER_ADAPTER_INFO ppi = (PIP_PER_ADAPTER_INFO)pPer;
                if (ppi->DnsServerList.IpAddress.String[0] &&
                    strcmp(ppi->DnsServerList.IpAddress.String, "0.0.0.0") != 0) {
                    strcpy_s(cfg->dnsStr, 32, ppi->DnsServerList.IpAddress.String);
                    cfg->hasDns = TRUE;
                }
            }
            free(pPer);
        }
        break;
    }
    free(pBuf);

    if (!cfg->ipStr[0]) return;

    ULONG gaLen = 0;
    if (GetAdaptersAddresses(AF_INET, 0, NULL, NULL, &gaLen) != ERROR_BUFFER_OVERFLOW) return;
    void *pGa = malloc(gaLen);
    if (!pGa) return;
    if (GetAdaptersAddresses(AF_INET, 0, NULL, (PIP_ADAPTER_ADDRESSES)pGa, &gaLen) != NO_ERROR) { free(pGa); return; }
    PIP_ADAPTER_ADDRESSES pAddr = (PIP_ADAPTER_ADDRESSES)pGa;
    while (pAddr) {
        if (strcmp(pAddr->AdapterName, targetGuid) == 0) {
            wcscpy_s(cfg->name, 256, pAddr->FriendlyName);
            cfg->found = TRUE;
            break;
        }
        pAddr = pAddr->Next;
    }
    free(pGa);
}

static BOOL RunCommand(LPCWSTR cmd) {
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    WCHAR full[2048];
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

    int ip0, ip1, ip2, ip3;
    ParseIP4(cfg.ipStr, &ip0, &ip1, &ip2, &ip3);
    int m0, m1, m2, m3;
    ParseIP4(cfg.maskStr, &m0, &m1, &m2, &m3);
    int g0 = 0, g1 = 0, g2 = 0, g3 = 0;
    if (cfg.hasGateway) ParseIP4(cfg.gwStr, &g0, &g1, &g2, &g3);
    int d0 = 0, d1 = 0, d2 = 0, d3 = 0;
    if (cfg.hasDns) ParseIP4(cfg.dnsStr, &d0, &d1, &d2, &d3);

    int used[256] = {0};
    used[ip3] = 1;
    used[g3] = 1;
    used[0] = 1;
    used[255] = 1;

    int newLast = 0;
    BOOL found = FALSE;

    for (int round = 0; round < MAX_ROUNDS && !found; round++) {
        PingResult results[NUM_CANDIDATES];
        HANDLE threads[NUM_CANDIDATES];
        int candidates[NUM_CANDIDATES];

        for (int i = 0; i < NUM_CANDIDATES; i++) {
            int last;
            do { last = 1 + rand() % 253; } while (used[last]);
            used[last] = 1;
            candidates[i] = last;

            char ipBuf[32];
            sprintf(ipBuf, "%d.%d.%d.%d", ip0, ip1, ip2, last);
            results[i].ip = inet_addr(ipBuf);
            results[i].reachable = TRUE;
            threads[i] = CreateThread(NULL, 0, PingThread, &results[i], 0, NULL);
            if (!threads[i]) results[i].reachable = TRUE;
        }

        int vc = 0;
        HANDLE vh[NUM_CANDIDATES];
        for (int i = 0; i < NUM_CANDIDATES; i++) {
            if (threads[i]) vh[vc++] = threads[i];
        }
        if (vc > 0) WaitForMultipleObjects(vc, vh, TRUE, 15000);
        for (int i = 0; i < NUM_CANDIDATES; i++) {
            if (threads[i]) CloseHandle(threads[i]);
        }

        int unreach[NUM_CANDIDATES];
        int uc = 0;
        for (int i = 0; i < NUM_CANDIDATES; i++) {
            if (!results[i].reachable) unreach[uc++] = i;
        }
        if (uc > 0) {
            int pick = unreach[rand() % uc];
            newLast = candidates[pick];
            found = TRUE;
        }
    }

    if (!found) {
        MessageBoxW(NULL, L"未能找到可用IP，请重试", L"自动设IP", MB_OK | MB_ICONWARNING);
        return 1;
    }

    WCHAR addrCmd[2048];
    if (cfg.hasGateway) {
        wsprintfW(addrCmd,
            L"netsh interface ip set address name=\"%s\" source=static addr=%d.%d.%d.%d mask=%d.%d.%d.%d gateway=%d.%d.%d.%d gwmetric=1",
            cfg.name, ip0, ip1, ip2, newLast, m0, m1, m2, m3, g0, g1, g2, g3);
    } else {
        wsprintfW(addrCmd,
            L"netsh interface ip set address name=\"%s\" source=static addr=%d.%d.%d.%d mask=%d.%d.%d.%d",
            cfg.name, ip0, ip1, ip2, newLast, m0, m1, m2, m3);
    }
    BOOL ok = RunCommand(addrCmd);

    if (cfg.hasDns) {
        WCHAR dnsCmd[512];
        wsprintfW(dnsCmd,
            L"netsh interface ip set dns name=\"%s\" source=static addr=%d.%d.%d.%d register=primary",
            cfg.name, d0, d1, d2, d3);
        RunCommand(dnsCmd);
    }

    WCHAR msg[1024];
    if (ok) {
        wsprintfW(msg, L"IP已更新\n\n旧IP: %d.%d.%d.%d\n新IP: %d.%d.%d.%d\n掩码: %d.%d.%d.%d\n网关: %d.%d.%d.%d\nDNS: %d.%d.%d.%d",
            ip0, ip1, ip2, ip3, ip0, ip1, ip2, newLast,
            m0, m1, m2, m3, g0, g1, g2, g3, d0, d1, d2, d3);
        MessageBoxW(NULL, msg, L"自动设IP - 成功", MB_OK | MB_ICONINFORMATION);
    } else {
        wsprintfW(msg, L"设置IP失败\n\n目标IP: %d.%d.%d.%d", ip0, ip1, ip2, newLast);
        MessageBoxW(NULL, msg, L"自动设IP - 失败", MB_OK | MB_ICONERROR);
    }
    return 0;
}
