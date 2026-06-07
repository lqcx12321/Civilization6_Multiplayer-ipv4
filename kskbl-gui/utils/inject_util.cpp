#include <winsock2.h>

#include "inject.h"
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>

bool runas_admin(LPCWSTR exename) {
    SHELLEXECUTEINFOW sei;
    memset(&sei, 0, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_FLAG_DDEWAIT | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"runas";
    sei.lpFile = exename;
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei);
}

DWORD get_civ6_dx11_proc() {
    return find_pid_by_name(L"CivilizationVI.exe");
}

DWORD get_civ6_dx12_proc() {
    return find_pid_by_name(L"CivilizationVI_DX12.exe");
}

DWORD get_civ6_proc() {
    DWORD pid = get_civ6_dx11_proc();
    if (pid == 0) pid = get_civ6_dx12_proc();
    return pid;
}

bool get_proc_path(DWORD pid, LPWSTR buf, DWORD bufsize) {
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProc == NULL) {
        DWORD err = GetLastError();
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, bufsize, NULL);
        CloseHandle(hProc);
        return false;
    }
    DWORD result = GetModuleFileNameExW(hProc, NULL, buf, bufsize);
    if (result == 0) {
        DWORD err = GetLastError();
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, bufsize, NULL);
        CloseHandle(hProc);
        return false;
    }
    buf[result] = '\0';
    CloseHandle(hProc);
    return true;
}

bool get_civ6_path(LPWSTR buf, DWORD bufsize) {
    DWORD pid = get_civ6_proc();
    if (pid == 0) return false;
    return get_proc_path(pid, buf, bufsize);
}
