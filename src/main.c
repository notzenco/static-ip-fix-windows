/*
 * static-ip-fix.exe
 *
 * A Windows-only tool to configure static IP addresses and DNS-over-HTTPS.
 *
 * Usage:
 *   static-ip-fix.exe cloudflare   - Configure static IP + Cloudflare DNS + DoH
 *   static-ip-fix.exe google       - Configure static IP + Google DNS + DoH
 *   static-ip-fix.exe status       - Show current DNS encryption status
 *
 * Requires Administrator privileges for cloudflare/google modes.
 *
 * Build: make (MinGW-w64)
 */

#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <strsafe.h>
#include <stdio.h>
#include <stdlib.h>

/* MSVC auto-linking (ignored by GCC) */
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ws2_32.lib")

/* ============================================================================
 * CONFIGURATION CONSTANTS
 * ============================================================================ */

/* Network interface name */
static const wchar_t *INTERFACE_ALIAS = L"Ethernet";

/* Static IPv4 configuration */
static const wchar_t *STATIC_IPV4_ADDR    = L"192.168.20.101";
static const wchar_t *STATIC_IPV4_MASK    = L"255.255.255.0";
static const wchar_t *STATIC_IPV4_GATEWAY = L"192.168.20.1";

/* Static IPv6 configuration */
static const wchar_t *STATIC_IPV6_ADDR    = L"2407:5400:621a:4f00::101";
static const wchar_t *STATIC_IPV6_PREFIX  = L"64";
static const wchar_t *STATIC_IPV6_GATEWAY = L"fe80::faca:59ff:fea3:5208";

/* Cloudflare DNS servers */
static const wchar_t *CF_DNS_IPV4_1 = L"1.1.1.1";
static const wchar_t *CF_DNS_IPV4_2 = L"1.0.0.1";
static const wchar_t *CF_DNS_IPV6_1 = L"2606:4700:4700::1111";
static const wchar_t *CF_DNS_IPV6_2 = L"2606:4700:4700::1001";
static const wchar_t *CF_DOH_TEMPLATE = L"https://cloudflare-dns.com/dns-query";

/* Google DNS servers */
static const wchar_t *GOOGLE_DNS_IPV4_1 = L"8.8.8.8";
static const wchar_t *GOOGLE_DNS_IPV4_2 = L"8.8.4.4";
static const wchar_t *GOOGLE_DNS_IPV6_1 = L"2001:4860:4860::8888";
static const wchar_t *GOOGLE_DNS_IPV6_2 = L"2001:4860:4860::8844";
static const wchar_t *GOOGLE_DOH_TEMPLATE = L"https://dns.google/dns-query";

/* All DNS servers for rollback cleanup */
static const wchar_t *ALL_DNS_SERVERS[] = {
    L"1.1.1.1",
    L"1.0.0.1",
    L"2606:4700:4700::1111",
    L"2606:4700:4700::1001",
    L"8.8.8.8",
    L"8.8.4.4",
    L"2001:4860:4860::8888",
    L"2001:4860:4860::8844",
    NULL
};

/* Input validation */
#define MAX_IFACE_LEN 128
#define CMD_BUFFER_SIZE 2048

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/*
 * Print error message to stderr
 */
static void print_error(const wchar_t *msg)
{
    fwprintf(stderr, L"[ERROR] %ls\n", msg);
}

/*
 * Print info message to stdout
 */
static void print_info(const wchar_t *msg)
{
    wprintf(L"[INFO] %ls\n", msg);
}

/*
 * Print success message to stdout
 */
static void print_success(const wchar_t *msg)
{
    wprintf(L"[OK] %ls\n", msg);
}

/*
 * Validate interface alias - allow only safe characters
 * Allowed: letters, digits, space, hyphen, underscore, parentheses, dot
 */
static int validate_interface_alias(const wchar_t *alias)
{
    size_t len = 0;

    if (!alias || *alias == L'\0') {
        print_error(L"Interface alias is empty");
        return 0;
    }

    if (FAILED(StringCchLengthW(alias, MAX_IFACE_LEN + 1, &len)) || len > MAX_IFACE_LEN) {
        print_error(L"Interface alias too long (max 128 chars)");
        return 0;
    }

    for (size_t i = 0; i < len; i++) {
        wchar_t c = alias[i];
        int valid = (c >= L'A' && c <= L'Z') ||
                    (c >= L'a' && c <= L'z') ||
                    (c >= L'0' && c <= L'9') ||
                    c == L' ' || c == L'-' || c == L'_' ||
                    c == L'(' || c == L')' || c == L'.';
        if (!valid) {
            wchar_t errmsg[256];
            StringCchPrintfW(errmsg, 256, L"Invalid character in interface alias: '%lc'", c);
            print_error(errmsg);
            return 0;
        }
    }

    return 1;
}

/*
 * Execute a process and wait for completion
 * Returns the process exit code, or -1 on failure to launch
 */
static int run_process(wchar_t *cmdline, int silent)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD exit_code = (DWORD)-1;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    /* Suppress output if silent mode */
    if (silent) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = NULL;
        si.hStdOutput = NULL;
        si.hStdError = NULL;
    }

    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(
            NULL,           /* Use cmdline for executable */
            cmdline,        /* Full command line */
            NULL,           /* Process security */
            NULL,           /* Thread security */
            FALSE,          /* Don't inherit handles */
            CREATE_NO_WINDOW, /* No console window */
            NULL,           /* Use parent environment */
            NULL,           /* Use parent directory */
            &si,
            &pi)) {
        DWORD err = GetLastError();
        wchar_t errmsg[512];
        StringCchPrintfW(errmsg, 512, L"CreateProcessW failed (error %lu): %ls", err, cmdline);
        print_error(errmsg);
        return -1;
    }

    /* Wait for process to complete */
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int)exit_code;
}

/*
 * Execute netsh command
 * Returns 0 on success, non-zero on failure
 */
static int run_netsh(const wchar_t *args)
{
    wchar_t cmdline[CMD_BUFFER_SIZE];

    if (FAILED(StringCchPrintfW(cmdline, CMD_BUFFER_SIZE, L"netsh.exe %ls", args))) {
        print_error(L"Command line too long");
        return -1;
    }

    return run_process(cmdline, 0);
}

/*
 * Execute netsh command silently (ignore errors)
 */
static void run_netsh_silent(const wchar_t *args)
{
    wchar_t cmdline[CMD_BUFFER_SIZE];

    if (SUCCEEDED(StringCchPrintfW(cmdline, CMD_BUFFER_SIZE, L"netsh.exe %ls", args))) {
        run_process(cmdline, 1);
    }
}

/* ============================================================================
 * ROLLBACK FUNCTION
 * ============================================================================ */

/*
 * Rollback all changes - restore DHCP and remove DoH templates
 * This function ignores all errors to ensure maximum cleanup
 */
static void rollback(void)
{
    wchar_t cmd[CMD_BUFFER_SIZE];

    wprintf(L"\n");
    print_info(L"Rolling back changes...");

    /* Reset IPv4 DNS to DHCP */
    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"interface ipv4 set dnsservers name=\"%ls\" source=dhcp",
        INTERFACE_ALIAS);
    run_netsh_silent(cmd);
    print_info(L"IPv4 DNS reset to DHCP");

    /* Reset IPv6 DNS to DHCP */
    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"interface ipv6 set dnsservers name=\"%ls\" source=dhcp",
        INTERFACE_ALIAS);
    run_netsh_silent(cmd);
    print_info(L"IPv6 DNS reset to DHCP");

    /* Delete all DoH encryption templates */
    for (int i = 0; ALL_DNS_SERVERS[i] != NULL; i++) {
        StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
            L"dns delete encryption server=%ls",
            ALL_DNS_SERVERS[i]);
        run_netsh_silent(cmd);
    }
    print_info(L"DoH encryption templates removed");

    print_info(L"Rollback complete");
}

/* ============================================================================
 * STATIC IP CONFIGURATION
 * ============================================================================ */

/*
 * Configure static IPv4 address
 */
static int apply_static_ipv4(void)
{
    wchar_t cmd[CMD_BUFFER_SIZE];
    int ret;

    print_info(L"Configuring static IPv4 address...");

    /* Set static IPv4 address */
    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"interface ipv4 set address name=\"%ls\" static %ls %ls %ls",
        INTERFACE_ALIAS, STATIC_IPV4_ADDR, STATIC_IPV4_MASK, STATIC_IPV4_GATEWAY);

    ret = run_netsh(cmd);
    if (ret != 0) {
        print_error(L"Failed to set static IPv4 address");
        return -1;
    }

    wchar_t msg[256];
    StringCchPrintfW(msg, 256, L"IPv4: %ls/%ls gateway %ls",
        STATIC_IPV4_ADDR, STATIC_IPV4_MASK, STATIC_IPV4_GATEWAY);
    print_success(msg);

    return 0;
}

/*
 * Configure static IPv6 address
 */
static int apply_static_ipv6(void)
{
    wchar_t cmd[CMD_BUFFER_SIZE];
    int ret;

    print_info(L"Configuring static IPv6 address...");

    /* Set static IPv6 address */
    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"interface ipv6 set address interface=\"%ls\" address=%ls/%ls",
        INTERFACE_ALIAS, STATIC_IPV6_ADDR, STATIC_IPV6_PREFIX);

    ret = run_netsh(cmd);
    if (ret != 0) {
        print_error(L"Failed to set static IPv6 address");
        return -1;
    }

    /* Add default route via link-local gateway */
    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"interface ipv6 add route ::/0 interface=\"%ls\" nexthop=%ls",
        INTERFACE_ALIAS, STATIC_IPV6_GATEWAY);

    /* Route may already exist, so we try delete first then add */
    wchar_t delcmd[CMD_BUFFER_SIZE];
    StringCchPrintfW(delcmd, CMD_BUFFER_SIZE,
        L"interface ipv6 delete route ::/0 interface=\"%ls\"",
        INTERFACE_ALIAS);
    run_netsh_silent(delcmd);

    ret = run_netsh(cmd);
    if (ret != 0) {
        /* Try without nexthop in case of issues */
        print_error(L"Warning: Could not add IPv6 default route (may already exist)");
    }

    wchar_t msg[256];
    StringCchPrintfW(msg, 256, L"IPv6: %ls/%ls gateway %ls",
        STATIC_IPV6_ADDR, STATIC_IPV6_PREFIX, STATIC_IPV6_GATEWAY);
    print_success(msg);

    return 0;
}

/* ============================================================================
 * DNS CONFIGURATION
 * ============================================================================ */

/*
 * Configure DNS servers for IPv4
 */
static int apply_dns_ipv4(const wchar_t *dns1, const wchar_t *dns2)
{
    wchar_t cmd[CMD_BUFFER_SIZE];
    int ret;

    print_info(L"Configuring IPv4 DNS servers...");

    /* Set primary DNS */
    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"interface ipv4 set dnsservers name=\"%ls\" static %ls primary validate=no",
        INTERFACE_ALIAS, dns1);

    ret = run_netsh(cmd);
    if (ret != 0) {
        print_error(L"Failed to set primary IPv4 DNS");
        return -1;
    }

    /* Add secondary DNS */
    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"interface ipv4 add dnsservers name=\"%ls\" %ls index=2 validate=no",
        INTERFACE_ALIAS, dns2);

    ret = run_netsh(cmd);
    if (ret != 0) {
        print_error(L"Failed to add secondary IPv4 DNS");
        return -1;
    }

    wchar_t msg[256];
    StringCchPrintfW(msg, 256, L"IPv4 DNS: %ls, %ls", dns1, dns2);
    print_success(msg);

    return 0;
}

/*
 * Configure DNS servers for IPv6
 */
static int apply_dns_ipv6(const wchar_t *dns1, const wchar_t *dns2)
{
    wchar_t cmd[CMD_BUFFER_SIZE];
    int ret;

    print_info(L"Configuring IPv6 DNS servers...");

    /* Set primary DNS */
    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"interface ipv6 set dnsservers name=\"%ls\" static %ls primary validate=no",
        INTERFACE_ALIAS, dns1);

    ret = run_netsh(cmd);
    if (ret != 0) {
        print_error(L"Failed to set primary IPv6 DNS");
        return -1;
    }

    /* Add secondary DNS */
    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"interface ipv6 add dnsservers name=\"%ls\" %ls index=2 validate=no",
        INTERFACE_ALIAS, dns2);

    ret = run_netsh(cmd);
    if (ret != 0) {
        print_error(L"Failed to add secondary IPv6 DNS");
        return -1;
    }

    wchar_t msg[256];
    StringCchPrintfW(msg, 256, L"IPv6 DNS: %ls, %ls", dns1, dns2);
    print_success(msg);

    return 0;
}

/* ============================================================================
 * DNS-OVER-HTTPS CONFIGURATION
 * ============================================================================ */

/*
 * Add DoH encryption template for a DNS server
 */
static int add_doh_template(const wchar_t *server, const wchar_t *template)
{
    wchar_t cmd[CMD_BUFFER_SIZE];
    int ret;

    /* First delete any existing template (ignore errors) */
    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"dns delete encryption server=%ls", server);
    run_netsh_silent(cmd);

    /* Add new template with autoupgrade=yes and udpfallback=no */
    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"dns add encryption server=%ls dohtemplate=%ls autoupgrade=yes udpfallback=no",
        server, template);

    ret = run_netsh(cmd);
    if (ret != 0) {
        wchar_t errmsg[512];
        StringCchPrintfW(errmsg, 512, L"Failed to add DoH template for %ls", server);
        print_error(errmsg);
        return -1;
    }

    return 0;
}

/*
 * Configure DoH for all DNS servers
 */
static int apply_doh(const wchar_t *dns_ipv4_1, const wchar_t *dns_ipv4_2,
                     const wchar_t *dns_ipv6_1, const wchar_t *dns_ipv6_2,
                     const wchar_t *template)
{
    print_info(L"Configuring DNS-over-HTTPS encryption...");

    if (add_doh_template(dns_ipv4_1, template) != 0) return -1;
    if (add_doh_template(dns_ipv4_2, template) != 0) return -1;
    if (add_doh_template(dns_ipv6_1, template) != 0) return -1;
    if (add_doh_template(dns_ipv6_2, template) != 0) return -1;

    wchar_t msg[512];
    StringCchPrintfW(msg, 512, L"DoH template: %ls (autoupgrade=yes, udpfallback=no)", template);
    print_success(msg);

    return 0;
}

/* ============================================================================
 * STATUS MODE
 * ============================================================================ */

/* Structure to hold DNS server info */
typedef struct {
    wchar_t address[64];
    int has_template;
    int autoupgrade;
    int udpfallback;
    wchar_t template[256];
} DnsServerInfo;

/*
 * Query DoH encryption info for a specific server
 */
static void query_doh_info(const wchar_t *server, DnsServerInfo *info)
{
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t cmdline[CMD_BUFFER_SIZE];
    char buffer[4096];
    DWORD bytesRead;

    StringCchCopyW(info->address, 64, server);
    info->has_template = 0;
    info->autoupgrade = 0;
    info->udpfallback = 1; /* Default to yes (insecure) */
    info->template[0] = L'\0';

    /* Create pipe for reading output */
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return;
    }

    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = NULL;

    ZeroMemory(&pi, sizeof(pi));

    StringCchPrintfW(cmdline, CMD_BUFFER_SIZE,
        L"netsh.exe dns show encryption server=%ls", server);

    if (!CreateProcessW(NULL, cmdline, NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return;
    }

    CloseHandle(hWritePipe);

    /* Read output */
    ZeroMemory(buffer, sizeof(buffer));
    if (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';

        /* Parse output - look for key indicators */
        if (strstr(buffer, "Encryption settings") != NULL ||
            strstr(buffer, "DNS-over-HTTPS template") != NULL ||
            strstr(buffer, "dohtemplate") != NULL) {
            info->has_template = 1;
        }

        if (strstr(buffer, "Auto-upgrade") != NULL) {
            if (strstr(buffer, "yes") != NULL || strstr(buffer, "Yes") != NULL) {
                info->autoupgrade = 1;
            }
        }

        if (strstr(buffer, "UDP-fallback") != NULL || strstr(buffer, "udpfallback") != NULL) {
            if (strstr(buffer, "no") != NULL || strstr(buffer, "No") != NULL) {
                info->udpfallback = 0;
            }
        }

        /* Extract template URL */
        char *templateStart = strstr(buffer, "https://");
        if (templateStart) {
            char templateUrl[256];
            int i = 0;
            while (templateStart[i] && !isspace((unsigned char)templateStart[i]) && i < 255) {
                templateUrl[i] = templateStart[i];
                i++;
            }
            templateUrl[i] = '\0';
            MultiByteToWideChar(CP_UTF8, 0, templateUrl, -1, info->template, 256);
        }
    }

    WaitForSingleObject(pi.hProcess, 5000);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);
}

/*
 * Get configured DNS servers for the interface
 */
static int get_configured_dns(DnsServerInfo *ipv4_servers, int *ipv4_count,
                              DnsServerInfo *ipv6_servers, int *ipv6_count)
{
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t cmdline[CMD_BUFFER_SIZE];
    char buffer[8192];
    DWORD bytesRead;

    *ipv4_count = 0;
    *ipv6_count = 0;

    /* Create pipe */
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

    /* Get IPv4 DNS servers */
    StringCchPrintfW(cmdline, CMD_BUFFER_SIZE,
        L"netsh.exe interface ipv4 show dnsservers name=\"%ls\"", INTERFACE_ALIAS);

    if (CreateProcessW(NULL, cmdline, NULL, NULL, TRUE,
                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hWritePipe);

        ZeroMemory(buffer, sizeof(buffer));
        if (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
            buffer[bytesRead] = '\0';

            /* Parse for IP addresses */
            char *line = strtok(buffer, "\r\n");
            while (line && *ipv4_count < 4) {
                /* Look for lines containing IP addresses */
                char *p = line;
                while (*p == ' ') p++;

                /* Check if line starts with a digit (IP address) */
                if (*p >= '0' && *p <= '9') {
                    /* Extract the IP */
                    char ip[64] = {0};
                    int i = 0;
                    while (*p && *p != ' ' && *p != '\r' && *p != '\n' && i < 63) {
                        ip[i++] = *p++;
                    }
                    ip[i] = '\0';

                    if (strlen(ip) >= 7) { /* Minimum valid IP length */
                        MultiByteToWideChar(CP_UTF8, 0, ip, -1,
                            ipv4_servers[*ipv4_count].address, 64);
                        query_doh_info(ipv4_servers[*ipv4_count].address,
                            &ipv4_servers[*ipv4_count]);
                        (*ipv4_count)++;
                    }
                }
                line = strtok(NULL, "\r\n");
            }
        }

        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hReadPipe);
    } else {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
    }

    /* Get IPv6 DNS servers */
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return 0;
    }

    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = NULL;

    ZeroMemory(&pi, sizeof(pi));

    StringCchPrintfW(cmdline, CMD_BUFFER_SIZE,
        L"netsh.exe interface ipv6 show dnsservers name=\"%ls\"", INTERFACE_ALIAS);

    if (CreateProcessW(NULL, cmdline, NULL, NULL, TRUE,
                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hWritePipe);

        ZeroMemory(buffer, sizeof(buffer));
        if (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
            buffer[bytesRead] = '\0';

            char *line = strtok(buffer, "\r\n");
            while (line && *ipv6_count < 4) {
                char *p = line;
                while (*p == ' ') p++;

                /* IPv6 addresses start with hex digit or contain :: */
                if (((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
                     (*p >= 'A' && *p <= 'F')) && strchr(p, ':')) {
                    char ip[64] = {0};
                    int i = 0;
                    while (*p && *p != ' ' && *p != '\r' && *p != '\n' && i < 63) {
                        ip[i++] = *p++;
                    }
                    ip[i] = '\0';

                    if (strlen(ip) >= 3 && strchr(ip, ':')) {
                        MultiByteToWideChar(CP_UTF8, 0, ip, -1,
                            ipv6_servers[*ipv6_count].address, 64);
                        query_doh_info(ipv6_servers[*ipv6_count].address,
                            &ipv6_servers[*ipv6_count]);
                        (*ipv6_count)++;
                    }
                }
                line = strtok(NULL, "\r\n");
            }
        }

        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hReadPipe);
    } else {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
    }

    return 0;
}

/*
 * Run status mode - display encryption status
 */
static int run_status(void)
{
    DnsServerInfo ipv4_servers[4];
    DnsServerInfo ipv6_servers[4];
    int ipv4_count = 0, ipv6_count = 0;
    int ipv4_encrypted = 0, ipv4_total = 0;
    int ipv6_encrypted = 0, ipv6_total = 0;
    int any_fallback = 0;
    int any_unencrypted = 0;

    wprintf(L"\n");
    wprintf(L"Status for interface: %ls\n", INTERFACE_ALIAS);
    wprintf(L"========================================\n\n");

    /* Get DNS servers and their encryption status */
    get_configured_dns(ipv4_servers, &ipv4_count, ipv6_servers, &ipv6_count);

    /* Display IPv4 DNS */
    wprintf(L"IPv4 DNS: ");
    if (ipv4_count == 0) {
        wprintf(L"(none configured)\n");
    } else {
        for (int i = 0; i < ipv4_count; i++) {
            wprintf(L"%ls%ls", ipv4_servers[i].address,
                (i < ipv4_count - 1) ? L", " : L"\n");
        }
    }

    /* Display IPv6 DNS */
    wprintf(L"IPv6 DNS: ");
    if (ipv6_count == 0) {
        wprintf(L"(none configured)\n");
    } else {
        for (int i = 0; i < ipv6_count; i++) {
            wprintf(L"%ls%ls", ipv6_servers[i].address,
                (i < ipv6_count - 1) ? L", " : L"\n");
        }
    }

    wprintf(L"\n");
    wprintf(L"Encryption:\n");
    wprintf(L"----------------------------------------\n");

    /* Analyze IPv4 */
    for (int i = 0; i < ipv4_count; i++) {
        ipv4_total++;
        int encrypted = ipv4_servers[i].has_template &&
                        ipv4_servers[i].autoupgrade &&
                        !ipv4_servers[i].udpfallback;

        if (encrypted) {
            ipv4_encrypted++;
        } else {
            any_unencrypted = 1;
        }

        if (ipv4_servers[i].udpfallback) {
            any_fallback = 1;
        }

        wprintf(L"  %ls: %ls", ipv4_servers[i].address,
            encrypted ? L"ENCRYPTED" : L"NOT ENCRYPTED");

        if (ipv4_servers[i].has_template && ipv4_servers[i].udpfallback) {
            wprintf(L" (fallback enabled)");
        } else if (!ipv4_servers[i].has_template) {
            wprintf(L" (no DoH template)");
        }
        wprintf(L"\n");
    }

    /* Analyze IPv6 */
    for (int i = 0; i < ipv6_count; i++) {
        ipv6_total++;
        int encrypted = ipv6_servers[i].has_template &&
                        ipv6_servers[i].autoupgrade &&
                        !ipv6_servers[i].udpfallback;

        if (encrypted) {
            ipv6_encrypted++;
        } else {
            any_unencrypted = 1;
        }

        if (ipv6_servers[i].udpfallback) {
            any_fallback = 1;
        }

        wprintf(L"  %ls: %ls", ipv6_servers[i].address,
            encrypted ? L"ENCRYPTED" : L"NOT ENCRYPTED");

        if (ipv6_servers[i].has_template && ipv6_servers[i].udpfallback) {
            wprintf(L" (fallback enabled)");
        } else if (!ipv6_servers[i].has_template) {
            wprintf(L" (no DoH template)");
        }
        wprintf(L"\n");
    }

    wprintf(L"\n");
    wprintf(L"Summary:\n");
    wprintf(L"----------------------------------------\n");

    /* IPv4 status */
    if (ipv4_total == 0) {
        wprintf(L"  IPv4: NO DNS CONFIGURED\n");
    } else if (ipv4_encrypted == ipv4_total) {
        wprintf(L"  IPv4: ENCRYPTED (%d/%d servers)\n", ipv4_encrypted, ipv4_total);
    } else {
        wprintf(L"  IPv4: PARTIALLY ENCRYPTED (%d/%d servers)\n", ipv4_encrypted, ipv4_total);
    }

    /* IPv6 status */
    if (ipv6_total == 0) {
        wprintf(L"  IPv6: NO DNS CONFIGURED\n");
    } else if (ipv6_encrypted == ipv6_total) {
        wprintf(L"  IPv6: ENCRYPTED (%d/%d servers)\n", ipv6_encrypted, ipv6_total);
    } else {
        wprintf(L"  IPv6: PARTIALLY ENCRYPTED (%d/%d servers)\n", ipv6_encrypted, ipv6_total);
    }

    /* Fallback status */
    wprintf(L"  Fallback: %ls\n", any_fallback ? L"ENABLED (insecure)" : L"DISABLED");

    /* Unencrypted servers */
    wprintf(L"  Unencrypted DNS: %ls\n", any_unencrypted ? L"YES (insecure)" : L"NONE");

    wprintf(L"\n");

    /* Overall result */
    int fully_encrypted = (ipv4_total > 0 || ipv6_total > 0) &&
                          (ipv4_encrypted == ipv4_total) &&
                          (ipv6_encrypted == ipv6_total) &&
                          !any_fallback;

    if (ipv4_total == 0 && ipv6_total == 0) {
        wprintf(L"Overall result: NO DNS CONFIGURED\n");
        return 1;
    } else if (fully_encrypted) {
        wprintf(L"Overall result: OK (fully encrypted)\n");
        return 0;
    } else {
        wprintf(L"Overall result: NOT FULLY ENCRYPTED\n");
        return 1;
    }
}

/* ============================================================================
 * CLOUDFLARE MODE
 * ============================================================================ */

static int run_cloudflare(void)
{
    wprintf(L"\n");
    wprintf(L"========================================\n");
    wprintf(L"  Static IP + Cloudflare DNS + DoH\n");
    wprintf(L"========================================\n\n");

    /* Apply static IPv4 */
    if (apply_static_ipv4() != 0) {
        rollback();
        return 1;
    }

    /* Apply static IPv6 */
    if (apply_static_ipv6() != 0) {
        rollback();
        return 1;
    }

    /* Apply IPv4 DNS */
    if (apply_dns_ipv4(CF_DNS_IPV4_1, CF_DNS_IPV4_2) != 0) {
        rollback();
        return 1;
    }

    /* Apply IPv6 DNS */
    if (apply_dns_ipv6(CF_DNS_IPV6_1, CF_DNS_IPV6_2) != 0) {
        rollback();
        return 1;
    }

    /* Apply DoH encryption */
    if (apply_doh(CF_DNS_IPV4_1, CF_DNS_IPV4_2,
                  CF_DNS_IPV6_1, CF_DNS_IPV6_2, CF_DOH_TEMPLATE) != 0) {
        rollback();
        return 1;
    }

    wprintf(L"\n");
    print_success(L"Configuration complete!");
    wprintf(L"\n");

    return 0;
}

/* ============================================================================
 * GOOGLE MODE
 * ============================================================================ */

static int run_google(void)
{
    wprintf(L"\n");
    wprintf(L"========================================\n");
    wprintf(L"  Static IP + Google DNS + DoH\n");
    wprintf(L"========================================\n\n");

    /* Apply static IPv4 */
    if (apply_static_ipv4() != 0) {
        rollback();
        return 1;
    }

    /* Apply static IPv6 */
    if (apply_static_ipv6() != 0) {
        rollback();
        return 1;
    }

    /* Apply IPv4 DNS */
    if (apply_dns_ipv4(GOOGLE_DNS_IPV4_1, GOOGLE_DNS_IPV4_2) != 0) {
        rollback();
        return 1;
    }

    /* Apply IPv6 DNS */
    if (apply_dns_ipv6(GOOGLE_DNS_IPV6_1, GOOGLE_DNS_IPV6_2) != 0) {
        rollback();
        return 1;
    }

    /* Apply DoH encryption */
    if (apply_doh(GOOGLE_DNS_IPV4_1, GOOGLE_DNS_IPV4_2,
                  GOOGLE_DNS_IPV6_1, GOOGLE_DNS_IPV6_2, GOOGLE_DOH_TEMPLATE) != 0) {
        rollback();
        return 1;
    }

    wprintf(L"\n");
    print_success(L"Configuration complete!");
    wprintf(L"\n");

    return 0;
}

/* ============================================================================
 * MAIN ENTRY POINT
 * ============================================================================ */

static void print_usage(void)
{
    wprintf(L"\n");
    wprintf(L"static-ip-fix.exe - Configure static IP and DNS-over-HTTPS\n");
    wprintf(L"\n");
    wprintf(L"Usage:\n");
    wprintf(L"  static-ip-fix.exe cloudflare   Configure with Cloudflare DNS\n");
    wprintf(L"  static-ip-fix.exe google       Configure with Google DNS\n");
    wprintf(L"  static-ip-fix.exe status       Show current encryption status\n");
    wprintf(L"\n");
    wprintf(L"The cloudflare and google modes require Administrator privileges.\n");
    wprintf(L"\n");
}

int wmain(int argc, wchar_t *argv[])
{
    /* Validate interface alias */
    if (!validate_interface_alias(INTERFACE_ALIAS)) {
        return 1;
    }

    /* Check arguments */
    if (argc != 2) {
        print_usage();
        return 1;
    }

    /* Handle modes */
    if (_wcsicmp(argv[1], L"cloudflare") == 0) {
        return run_cloudflare();
    }
    else if (_wcsicmp(argv[1], L"google") == 0) {
        return run_google();
    }
    else if (_wcsicmp(argv[1], L"status") == 0) {
        return run_status();
    }
    else {
        print_error(L"Unknown mode");
        print_usage();
        return 1;
    }
}
