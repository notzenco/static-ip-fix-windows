/*
 * process.c - Process execution and output capture
 */

#include "process.h"

/* ============================================================================
 * PROCESS EXECUTION
 * ============================================================================ */

int run_process(wchar_t *cmdline, int silent)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD exit_code = (DWORD)-1;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    if (silent) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = NULL;
        si.hStdOutput = NULL;
        si.hStdError = NULL;
    }

    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(NULL, cmdline, NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int)exit_code;
}

int run_process_capture(wchar_t *cmdline, char *buffer, size_t buffer_size)
{
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD bytesRead, totalRead;
    DWORD exit_code = (DWORD)-1;

    if (!buffer || buffer_size == 0) {
        return -1;
    }

    buffer[0] = '\0';

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return -1;
    }

    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = NULL;

    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(NULL, cmdline, NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return -1;
    }

    CloseHandle(hWritePipe);

    /* Read all output in a loop */
    totalRead = 0;
    while (totalRead < buffer_size - 1) {
        if (!ReadFile(hReadPipe, buffer + totalRead,
                     (DWORD)(buffer_size - 1 - totalRead), &bytesRead, NULL) || bytesRead == 0) {
            break;
        }
        totalRead += bytesRead;
    }
    buffer[totalRead] = '\0';

    WaitForSingleObject(pi.hProcess, 5000);
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);

    return (int)exit_code;
}

int run_netsh(const wchar_t *args)
{
    wchar_t cmdline[CMD_BUFFER_SIZE];

    if (FAILED(StringCchPrintfW(cmdline, CMD_BUFFER_SIZE, L"netsh.exe %ls", args))) {
        print_error(L"Command line too long");
        return -1;
    }

    return run_process(cmdline, 0);
}

void run_netsh_silent(const wchar_t *args)
{
    wchar_t cmdline[CMD_BUFFER_SIZE];

    if (SUCCEEDED(StringCchPrintfW(cmdline, CMD_BUFFER_SIZE, L"netsh.exe %ls", args))) {
        run_process(cmdline, 1);
    }
}

int run_netsh_capture(const wchar_t *args, char *buffer, size_t buffer_size)
{
    wchar_t cmdline[CMD_BUFFER_SIZE];

    if (FAILED(StringCchPrintfW(cmdline, CMD_BUFFER_SIZE, L"netsh.exe %ls", args))) {
        print_error(L"Command line too long");
        return -1;
    }

    return run_process_capture(cmdline, buffer, buffer_size);
}
