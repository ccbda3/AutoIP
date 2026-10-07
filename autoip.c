#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

#define NUM_CANDIDATES 5
#define PING_TIMEOUT 2000
#define MAX_ROUNDS 3

#define ADAPTER_NAME L"以太网"
#define IP0 10
#define IP1 172
#define IP2 201
#define MASK0 255
#define MASK1 255
#define MASK2 254
#define MASK3 0
#define GW0 10
#define GW1 172
#define GW2 201
#define GW3 254
#define DNS0 10
#define DNS1 1
#define DNS2 0
#define DNS3 62

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

    int used[256] = {0};
    used[GW3] = 1;
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
            sprintf(ipBuf, "%d.%d.%d.%d", IP0, IP1, IP2, last);
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
    wsprintfW(addrCmd,
        L"netsh interface ip set address name=\"%s\" source=static addr=%d.%d.%d.%d mask=%d.%d.%d.%d gateway=%d.%d.%d.%d gwmetric=1",
        ADAPTER_NAME, IP0, IP1, IP2, newLast, MASK0, MASK1, MASK2, MASK3, GW0, GW1, GW2, GW3);
    BOOL ok = RunCommand(addrCmd);

    WCHAR dnsCmd[512];
    wsprintfW(dnsCmd,
        L"netsh interface ip set dns name=\"%s\" source=static addr=%d.%d.%d.%d register=primary",
        ADAPTER_NAME, DNS0, DNS1, DNS2, DNS3);
    RunCommand(dnsCmd);

    WCHAR msg[1024];
    if (ok) {
        wsprintfW(msg, L"IP已更新\n\n新IP: %d.%d.%d.%d\n掩码: %d.%d.%d.%d\n网关: %d.%d.%d.%d\nDNS: %d.%d.%d.%d",
            IP0, IP1, IP2, newLast, MASK0, MASK1, MASK2, MASK3,
            GW0, GW1, GW2, GW3, DNS0, DNS1, DNS2, DNS3);
        MessageBoxW(NULL, msg, L"自动设IP - 成功", MB_OK | MB_ICONINFORMATION);
    } else {
        wsprintfW(msg, L"设置IP失败\n\n目标IP: %d.%d.%d.%d", IP0, IP1, IP2, newLast);
        MessageBoxW(NULL, msg, L"自动设IP - 失败", MB_OK | MB_ICONERROR);
    }
    return 0;
}
