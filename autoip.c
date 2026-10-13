#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

#define SCAN_FIRST 1
#define SCAN_LAST 253
#define PING_COUNT 5
#define PING_TIMEOUT 2000
#define PING_INTERVAL 500
#define VERIFY_COUNT 10
#define VERIFY_TIMEOUT 3000
#define VERIFY_INTERVAL 1000

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

#define WM_APPENDLOG   (WM_APP + 1)
#define WM_WORKDONE     (WM_APP + 2)

#define ID_EDIT_LOG  1001
#define ID_BTN_CLOSE 1002

static HWND hwndLog;
static HWND hwndClose;
static HWND hwndMain;
static BOOL workDone = FALSE;

typedef struct {
    DWORD ip;
    BOOL reachable;
    int replyCount;
    int arpAlive;
    int lastOctet;
} PingResult;

static int ArpProbe(DWORD ip) {
    ULONG mac[2] = {0, 0};
    ULONG macLen = 6;
    return (SendARP(ip, 0, mac, &macLen) == NO_ERROR) ? 1 : 0;
}

static DWORD WINAPI PingThread(LPVOID param) {
    PingResult *r = (PingResult *)param;
    r->arpAlive = ArpProbe(r->ip);
    if (r->arpAlive) { r->reachable = TRUE; r->replyCount = 0; return 0; }
    HANDLE h = IcmpCreateFile();
    if (h == INVALID_HANDLE_VALUE) { r->reachable = TRUE; r->replyCount = PING_COUNT; return 1; }
    unsigned char data[32] = {0};
    DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(data) + 8;
    void *buf = malloc(replySize);
    if (!buf) { IcmpCloseHandle(h); r->reachable = TRUE; r->replyCount = PING_COUNT; return 1; }
    int replies = 0;
    for (int i = 0; i < PING_COUNT; i++) {
        DWORD ret = IcmpSendEcho(h, r->ip, data, sizeof(data), NULL, buf, replySize, PING_TIMEOUT);
        if (ret > 0) replies++;
        if (i < PING_COUNT - 1) Sleep(PING_INTERVAL);
    }
    r->replyCount = replies;
    r->reachable = (replies > 0);
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

static int VerifyBaidu(int *replies) {
    HANDLE h = IcmpCreateFile();
    if (h == INVALID_HANDLE_VALUE) { *replies = 0; return 0; }
    unsigned char data[32] = {0};
    DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(data) + 8;
    void *buf = malloc(replySize);
    if (!buf) { IcmpCloseHandle(h); *replies = 0; return 0; }
    DWORD baiduIp = inet_addr("110.242.74.102");
    int got = 0;
    for (int i = 0; i < VERIFY_COUNT; i++) {
        DWORD ret = IcmpSendEcho(h, baiduIp, data, sizeof(data), NULL, buf, replySize, VERIFY_TIMEOUT);
        if (ret > 0) got++;
        if (i < VERIFY_COUNT - 1) Sleep(VERIFY_INTERVAL);
    }
    free(buf);
    IcmpCloseHandle(h);
    *replies = got;
    return got;
}

static void AppendLog(const wchar_t *fmt, ...) {
    wchar_t buf[2048];
    va_list args;
    va_start(args, fmt);
    vswprintf(buf, 2048, fmt, args);
    va_end(args);
    wchar_t *heap = _wcsdup(buf);
    PostMessageW(hwndMain, WM_APPENDLOG, 0, (LPARAM)heap);
}

static BOOL SetIP(int newLast) {
    WCHAR addrCmd[2048];
    wsprintfW(addrCmd,
        L"netsh interface ip set address name=\"%s\" source=static addr=%d.%d.%d.%d mask=%d.%d.%d.%d gateway=%d.%d.%d.%d gwmetric=1",
        ADAPTER_NAME, IP0, IP1, IP2, newLast, MASK0, MASK1, MASK2, MASK3, GW0, GW1, GW2, GW3);
    BOOL ok = RunCommand(addrCmd);
    AppendLog(L"设置IP: %s\r\n", ok ? L"成功" : L"失败");

    WCHAR dnsCmd[512];
    wsprintfW(dnsCmd,
        L"netsh interface ip set dns name=\"%s\" source=static addr=%d.%d.%d.%d register=primary",
        ADAPTER_NAME, DNS0, DNS1, DNS2, DNS3);
    RunCommand(dnsCmd);
    AppendLog(L"设置DNS: 完成\r\n");
    return ok;
}

static DWORD WINAPI WorkerThread(LPVOID param) {
    AppendLog(L"=== 自动设IP 启动 ===\r\n");
    AppendLog(L"适配器: %s\r\n", ADAPTER_NAME);
    AppendLog(L"IP段: %d.%d.%d.x (只改最后一位)\r\n", IP0, IP1, IP2);
    AppendLog(L"掩码: %d.%d.%d.%d\r\n", MASK0, MASK1, MASK2, MASK3);
    AppendLog(L"网关: %d.%d.%d.%d\r\n", GW0, GW1, GW2, GW3);
    AppendLog(L"DNS:  %d.%d.%d.%d\r\n", DNS0, DNS1, DNS2, DNS3);
    AppendLog(L"探测: ARP + ping%d包 (任一有响应即视为占用)\r\n", PING_COUNT);
    AppendLog(L"验证: ping百度%d包, 全通才算成功\r\n", VERIFY_COUNT);
    AppendLog(L"\r\n");

    srand(GetTickCount());

    int total = SCAN_LAST - SCAN_FIRST + 1;
    PingResult *results = (PingResult *)malloc(sizeof(PingResult) * total);
    HANDLE *threads = (HANDLE *)malloc(sizeof(HANDLE) * total);
    if (!results || !threads) {
        AppendLog(L"*** 内存不足 ***\r\n");
        PostMessageW(hwndMain, WM_WORKDONE, 0, 1);
        return 1;
    }

    AppendLog(L"并发扫描 %d.%d.%d.%d - %d.%d.%d.%d (共%d个地址)...\r\n\r\n",
        IP0, IP1, IP2, SCAN_FIRST, IP0, IP1, IP2, SCAN_LAST, total);

    for (int i = 0; i < total; i++) {
        int last = SCAN_FIRST + i;
        char ipBuf[32];
        sprintf(ipBuf, "%d.%d.%d.%d", IP0, IP1, IP2, last);
        results[i].ip = inet_addr(ipBuf);
        results[i].reachable = TRUE;
        results[i].replyCount = 0;
        results[i].arpAlive = 0;
        results[i].lastOctet = last;
        threads[i] = CreateThread(NULL, 0, PingThread, &results[i], 0, NULL);
        if (!threads[i]) results[i].reachable = TRUE;
    }

    for (int i = 0; i < total; i++) {
        if (threads[i]) {
            WaitForSingleObject(threads[i], 60000);
            CloseHandle(threads[i]);
        }
    }

    int occupied = 0;
    int freeCount = 0;
    for (int i = 0; i < total; i++) {
        if (results[i].reachable) occupied++;
        else freeCount++;
    }
    AppendLog(L"扫描完成: 共%d个地址, 被占用 %d 个, 空闲 %d 个\r\n", total, occupied, freeCount);

    if (freeCount <= 0) {
        AppendLog(L"\r\n*** 全段地址均被占用, 请稍后重试 ***\r\n");
        free(results);
        free(threads);
        PostMessageW(hwndMain, WM_WORKDONE, 0, 1);
        return 1;
    }

    int *freeList = (int *)malloc(sizeof(int) * freeCount);
    if (!freeList) {
        AppendLog(L"*** 内存不足 ***\r\n");
        free(results);
        free(threads);
        PostMessageW(hwndMain, WM_WORKDONE, 0, 1);
        return 1;
    }

    int fc = 0;
    for (int i = 0; i < total; i++) {
        if (!results[i].reachable) freeList[fc++] = results[i].lastOctet;
    }

    int show = fc < 40 ? fc : 40;
    AppendLog(L"空闲地址(前%d个): ", show);
    for (int i = 0; i < show; i++) {
        AppendLog(L"%d.%d.%d.%d  ", IP0, IP1, IP2, freeList[i]);
    }
    AppendLog(L"\r\n\r\n");

    for (int i = fc - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = freeList[i]; freeList[i] = freeList[j]; freeList[j] = t;
    }

    BOOL success = FALSE;
    int finalLast = 0;

    for (int idx = 0; idx < fc && !success; idx++) {
        int tryLast = freeList[idx];
        AppendLog(L">> 尝试IP: %d.%d.%d.%d\r\n", IP0, IP1, IP2, tryLast);

        AppendLog(L"正在执行 netsh 设置IP...\r\n");
        if (!SetIP(tryLast)) {
            AppendLog(L"设置失败, 换下一个\r\n\r\n");
            continue;
        }

        AppendLog(L"等待网络生效 (3秒)...\r\n");
        Sleep(3000);

        AppendLog(L"验证稳定性: ping百度 %d 包...\r\n", VERIFY_COUNT);
        int replies = 0;
        VerifyBaidu(&replies);
        AppendLog(L"百度验证结果: 收到 %d/%d\r\n", replies, VERIFY_COUNT);

        if (replies == VERIFY_COUNT) {
            success = TRUE;
            finalLast = tryLast;
            AppendLog(L">> 验证通过! IP稳定可用\r\n");
        } else {
            AppendLog(L">> 有丢包, 换下一个\r\n\r\n");
        }
    }

    free(freeList);
    free(results);
    free(threads);

    if (!success) {
        AppendLog(L"\r\n*** 未能找到稳定可用IP, 请重试 ***\r\n");
        PostMessageW(hwndMain, WM_WORKDONE, 0, 1);
        return 1;
    }

    AppendLog(L"\r\n=== 设置完成 ===\r\n");
    AppendLog(L"新IP:  %d.%d.%d.%d\r\n", IP0, IP1, IP2, finalLast);
    AppendLog(L"掩码:  %d.%d.%d.%d\r\n", MASK0, MASK1, MASK2, MASK3);
    AppendLog(L"网关:  %d.%d.%d.%d\r\n", GW0, GW1, GW2, GW3);
    AppendLog(L"DNS:   %d.%d.%d.%d\r\n", DNS0, DNS1, DNS2, DNS3);
    AppendLog(L"百度验证: %d/%d 全通\r\n", VERIFY_COUNT, VERIFY_COUNT);

    PostMessageW(hwndMain, WM_WORKDONE, 0, 0);
    return 0;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
            hwndLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                10, 10, cs->cx - 30, cs->cy - 70, hwnd, (HMENU)(INT_PTR)ID_EDIT_LOG, cs->hInstance, NULL);

            HFONT hFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                FIXED_PITCH | FF_MODERN, L"Consolas");
            SendMessageW(hwndLog, WM_SETFONT, (WPARAM)hFont, FALSE);

            hwndClose = CreateWindowExW(0, L"BUTTON", L"关闭",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                cs->cx - 100, cs->cy - 50, 80, 30, hwnd, (HMENU)(INT_PTR)ID_BTN_CLOSE, cs->hInstance, NULL);

            hwndMain = hwnd;
            HANDLE h = CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
            CloseHandle(h);
            return 0;
        }
        case WM_APPENDLOG: {
            wchar_t *text = (wchar_t *)lParam;
            int len = GetWindowTextLengthW(hwndLog);
            SendMessageW(hwndLog, EM_SETSEL, len, len);
            SendMessageW(hwndLog, EM_REPLACESEL, 0, (LPARAM)text);
            SendMessageW(hwndLog, EM_SCROLLCARET, 0, 0);
            free(text);
            return 0;
        }
        case WM_WORKDONE: {
            workDone = TRUE;
            EnableWindow(hwndClose, TRUE);
            if (lParam == 0) {
                SetWindowTextW(hwndClose, L"完成");
            }
            return 0;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_BTN_CLOSE) {
                DestroyWindow(hwnd);
            }
            return 0;
        }
        case WM_CLOSE: {
            if (workDone) {
                DestroyWindow(hwnd);
            }
            return 0;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"AutoIPClass";
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.hIconSm = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, L"AutoIPClass", L"自动设IP - 日志",
        (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX),
        CW_USEDEFAULT, CW_USEDEFAULT, 560, 500,
        NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
