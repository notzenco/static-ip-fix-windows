// dnsconfig_safe.c
// Build: gcc -Wall -Wextra -DUNICODE -D_UNICODE -O2 -municode src/main.c -o bin/dnsconfig_safe.exe -liphlpapi -ladvapi32
//
// Usage:
//   dnsconfig_safe.exe cloudflare [InterfaceAlias]
//   dnsconfig_safe.exe google [InterfaceAlias]
//
// Behavior:
//   - Applies Cloudflare or Google DNS (IPv4 + IPv6) + DoH templates (no UDP fallback).
//   - If ANY netsh step fails, rolls DNS back to DHCP (IPv4 + IPv6) and clears DoH templates.

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>

static void print_usage(const wchar_t *exe) {
    fwprintf(stderr,
        L"Usage:\n"
        L"  %ls cloudflare [InterfaceAlias]\n"
        L"  %ls google [InterfaceAlias]\n\n"
        L"If InterfaceAlias is omitted, defaults to \"Ethernet\".\n",
        exe, exe);
}

// Run a process (no shell), wait for it, return exit code (or -1 on error)
static int run_process(const wchar_t *app, const wchar_t *cmdLine) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    // cmdLine must be mutable for CreateProcessW
    size_t len = wcslen(cmdLine);
    wchar_t *cmdBuf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(wchar_t));
    if (!cmdBuf) {
        fwprintf(stderr, L"[!] HeapAlloc failed for cmdLine.\n");
        return -1;
    }
    wcscpy(cmdBuf, cmdLine);

    if (!CreateProcessW(
            app,           // lpApplicationName
            cmdBuf,        // lpCommandLine (mutable)
            NULL,          // lpProcessAttributes
            NULL,          // lpThreadAttributes
            FALSE,         // bInheritHandles
            0,             // dwCreationFlags
            NULL,          // lpEnvironment
            NULL,          // lpCurrentDirectory
            &si,
            &pi))
    {
        DWORD err = GetLastError();
        fwprintf(stderr, L"[!] CreateProcessW failed (%lu) for '%ls' '%ls'\n", err, app, cmdLine);
        HeapFree(GetProcessHeap(), 0, cmdBuf);
        return -1;
    }

    HeapFree(GetProcessHeap(), 0, cmdBuf);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
        fwprintf(stderr, L"[!] GetExitCodeProcess failed.\n");
        exitCode = (DWORD)-1;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)exitCode;
}

// Wrapper around netsh.exe to keep logging centralized
static int run_netsh(const wchar_t *cmdLine) {
    const wchar_t *NETSH = L"netsh.exe";
    int rc = run_process(NETSH, cmdLine);
    if (rc != 0) {
        fwprintf(stderr, L"[!] netsh failed (%d) for: %ls\n", rc, cmdLine);
    }
    return rc;
}

// Very simple admin check
static BOOL is_running_as_admin(void) {
    BOOL isAdmin = FALSE;
    HANDLE token = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elev;
        DWORD size = sizeof(elev);
        if (GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &size)) {
            isAdmin = elev.TokenIsElevated;
        }
        CloseHandle(token);
    }
    return isAdmin;
}

// Tiny helper: compare wide strings case-insensitively
static int wci_equals(const wchar_t *a, const wchar_t *b) {
    return _wcsicmp(a, b) == 0;
}

// Basic sanity: reject quotes/control chars in interface alias
static BOOL validate_interface_alias(const wchar_t *alias) {
    for (const wchar_t *p = alias; *p; ++p) {
        if (*p < L' ' || *p == L'\"') {
            return FALSE;
        }
    }
    return TRUE;
}

// Rollback: DNS back to DHCP on both IPv4/IPv6 + clear DoH template state
static void rollback_to_dhcp(const wchar_t *iface) {
    wchar_t cmd[256];

    fwprintf(stderr, L"[!] Rolling back DNS configuration on \"%ls\" to DHCP...\n", iface);

    // IPv4 DNS -> DHCP
    _snwprintf(cmd, _countof(cmd),
               L"netsh interface ipv4 set dnsservers name=\"%ls\" dhcp",
               iface);
    cmd[_countof(cmd) - 1] = L'\0';
    run_netsh(cmd);

    // IPv6 DNS -> DHCP
    _snwprintf(cmd, _countof(cmd),
               L"netsh interface ipv6 set dnsservers name=\"%ls\" dhcp",
               iface);
    cmd[_countof(cmd) - 1] = L'\0';
    run_netsh(cmd);

    // Clear all DNS encryption templates
    run_netsh(L"netsh dns delete encryption server=all");

    fwprintf(stderr, L"[!] Rollback completed. DNS is now set to automatic (DHCP).\n");
}

// Apply Cloudflare DNS + DoH; return 0 on success, non-zero on failure
static int apply_cloudflare(const wchar_t *iface) {
    wchar_t cmd[512];

    wprintf(L"[*] Setting Cloudflare DNS servers on \"%ls\"...\n", iface);

    // IPv4 primary: 1.1.1.1
    _snwprintf(cmd, _countof(cmd),
               L"netsh interface ipv4 set dnsservers name=\"%ls\" static 1.1.1.1 primary",
               iface);
    cmd[_countof(cmd) - 1] = L'\0';
    if (run_netsh(cmd) != 0) return -1;

    // IPv4 secondary: 1.0.0.1
    _snwprintf(cmd, _countof(cmd),
               L"netsh interface ipv4 add dnsservers name=\"%ls\" address=1.0.0.1 index=2",
               iface);
    cmd[_countof(cmd) - 1] = L'\0';
    if (run_netsh(cmd) != 0) return -1;

    // IPv6 primary: 2606:4700:4700::1111
    _snwprintf(cmd, _countof(cmd),
               L"netsh interface ipv6 set dnsservers name=\"%ls\" static 2606:4700:4700::1111 primary",
               iface);
    cmd[_countof(cmd) - 1] = L'\0';
    if (run_netsh(cmd) != 0) return -1;

    // IPv6 secondary: 2606:4700:4700::1001
    _snwprintf(cmd, _countof(cmd),
               L"netsh interface ipv6 add dnsservers name=\"%ls\" address=2606:4700:4700::1001 index=2",
               iface);
    cmd[_countof(cmd) - 1] = L'\0';
    if (run_netsh(cmd) != 0) return -1;

    wprintf(L"[*] Adding Cloudflare DNS-over-HTTPS templates (no UDP fallback)...\n");
    if (run_netsh(L"netsh dns add encryption server=1.1.1.1 dohtemplate=https://cloudflare-dns.com/dns-query autoupgrade=yes udpfallback=no") != 0) return -1;
    if (run_netsh(L"netsh dns add encryption server=1.0.0.1 dohtemplate=https://cloudflare-dns.com/dns-query autoupgrade=yes udpfallback=no") != 0) return -1;
    if (run_netsh(L"netsh dns add encryption server=2606:4700:4700::1111 dohtemplate=https://cloudflare-dns.com/dns-query autoupgrade=yes udpfallback=no") != 0) return -1;
    if (run_netsh(L"netsh dns add encryption server=2606:4700:4700::1001 dohtemplate=https://cloudflare-dns.com/dns-query autoupgrade=yes udpfallback=no") != 0) return -1;

    return 0;
}

// Apply Google DNS + DoH; return 0 on success, non-zero on failure
static int apply_google(const wchar_t *iface) {
    wchar_t cmd[512];

    wprintf(L"[*] Setting Google Public DNS servers on \"%ls\"...\n", iface);

    // IPv4 primary: 8.8.8.8
    _snwprintf(cmd, _countof(cmd),
               L"netsh interface ipv4 set dnsservers name=\"%ls\" static 8.8.8.8 primary",
               iface);
    cmd[_countof(cmd) - 1] = L'\0';
    if (run_netsh(cmd) != 0) return -1;

    // IPv4 secondary: 8.8.4.4
    _snwprintf(cmd, _countof(cmd),
               L"netsh interface ipv4 add dnsservers name=\"%ls\" address=8.8.4.4 index=2",
               iface);
    cmd[_countof(cmd) - 1] = L'\0';
    if (run_netsh(cmd) != 0) return -1;

    // IPv6 primary: 2001:4860:4860::8888
    _snwprintf(cmd, _countof(cmd),
               L"netsh interface ipv6 set dnsservers name=\"%ls\" static 2001:4860:4860::8888 primary",
               iface);
    cmd[_countof(cmd) - 1] = L'\0';
    if (run_netsh(cmd) != 0) return -1;

    // IPv6 secondary: 2001:4860:4860::8844
    _snwprintf(cmd, _countof(cmd),
               L"netsh interface ipv6 add dnsservers name=\"%ls\" address=2001:4860:4860::8844 index=2",
               iface);
    cmd[_countof(cmd) - 1] = L'\0';
    if (run_netsh(cmd) != 0) return -1;

    wprintf(L"[*] Adding Google DNS-over-HTTPS templates (no UDP fallback)...\n");
    if (run_netsh(L"netsh dns add encryption server=8.8.8.8 dohtemplate=https://dns.google/dns-query autoupgrade=yes udpfallback=no") != 0) return -1;
    if (run_netsh(L"netsh dns add encryption server=8.8.4.4 dohtemplate=https://dns.google/dns-query autoupgrade=yes udpfallback=no") != 0) return -1;
    if (run_netsh(L"netsh dns add encryption server=2001:4860:4860::8888 dohtemplate=https://dns.google/dns-query autoupgrade=yes udpfallback=no") != 0) return -1;
    if (run_netsh(L"netsh dns add encryption server=2001:4860:4860::8844 dohtemplate=https://dns.google/dns-query autoupgrade=yes udpfallback=no") != 0) return -1;

    return 0;
}

int wmain(int argc, wchar_t *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (!is_running_as_admin()) {
        fwprintf(stderr,
                 L"[!] This tool should be run as Administrator.\n"
                 L"    Most netsh commands will fail otherwise.\n\n");
    }

    const wchar_t *mode = argv[1];
    const wchar_t *iface = (argc >= 3) ? argv[2] : L"Ethernet";

    if (!validate_interface_alias(iface)) {
        fwprintf(stderr, L"[!] Interface alias contains invalid characters.\n");
        return 1;
    }

    enum { MODE_CLOUDFLARE, MODE_GOOGLE } selectedMode;
    if (wci_equals(mode, L"cloudflare")) {
        selectedMode = MODE_CLOUDFLARE;
    } else if (wci_equals(mode, L"google")) {
        selectedMode = MODE_GOOGLE;
    } else {
        fwprintf(stderr, L"[!] Unknown mode: %ls\n\n", mode);
        print_usage(argv[0]);
        return 1;
    }

    wprintf(L"[*] Interface : \"%ls\"\n", iface);
    wprintf(L"[*] Mode      : %ls\n\n",
            selectedMode == MODE_CLOUDFLARE ? L"Cloudflare" : L"Google");

    // 1) Clear existing DNS encryption templates up front
    wprintf(L"[*] Clearing existing DNS-over-HTTPS encryption templates...\n");
    if (run_netsh(L"netsh dns delete encryption server=all") != 0) {
        rollback_to_dhcp(iface);
        return 1;
    }

    int rc;
    if (selectedMode == MODE_CLOUDFLARE) {
        rc = apply_cloudflare(iface);
    } else {
        rc = apply_google(iface);
    }

    if (rc != 0) {
        fwprintf(stderr, L"[!] DNS configuration failed. Initiating rollback...\n");
        rollback_to_dhcp(iface);
        return 1;
    }

    wprintf(L"\n[+] DNS configuration applied successfully.\n");
    wprintf(L"    You can verify with:\n");
    wprintf(L"      netsh dns show encryption\n");
    wprintf(L"      Get-DnsClientServerAddress -InterfaceAlias '%ls'\n", iface);

    return 0;
}
