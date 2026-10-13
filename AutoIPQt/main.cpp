#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <shellapi.h>
#include <QApplication>
#include <QIcon>
#include "mainwindow.h"

#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

static bool IsRunningAsAdmin() {
    BOOL fIsRunAsAdmin = FALSE;
    DWORD dwError = ERROR_SUCCESS;
    PSID pAdminSid = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

    if (!AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdminSid)) {
        dwError = GetLastError();
        goto Cleanup;
    }

    if (!CheckTokenMembership(NULL, pAdminSid, &fIsRunAsAdmin)) {
        dwError = GetLastError();
    }

Cleanup:
    if (pAdminSid) FreeSid(pAdminSid);
    SetLastError(dwError);
    return fIsRunAsAdmin;
}

static void RelaunchAsAdmin() {
    WCHAR szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath))) {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = szPath;
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL;
        ShellExecuteExW(&sei);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    if (!IsRunningAsAdmin()) {
        RelaunchAsAdmin();
        return 0;
    }

    int argc = 0;
    QApplication app(argc, nullptr);
    app.setApplicationName("AutoIP");
    app.setWindowIcon(QIcon(":/icons/autoip.ico"));

    MainWindow w;
    w.show();

    return app.exec();
}
