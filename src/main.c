/*
 * static-ip-fix.exe
 *
 * A Windows tool to configure static IP addresses and DNS-over-HTTPS.
 *
 * Usage:
 *   static-ip-fix.exe [options] <mode>
 *
 * Modes:
 *   cloudflare   - Configure DNS with Cloudflare + DoH
 *   google       - Configure DNS with Google + DoH
 *   status       - Show current DNS encryption status
 *
 * Options:
 *   -h, --help              Show help message
 *   -c, --config FILE       Load configuration from FILE
 *   -l, --list-interfaces   List available network interfaces
 *   -i, --interface NAME    Specify network interface
 *   --dns-only              Only configure DNS (skip static IP)
 *   --ipv4 ADDR             IPv4 address
 *   --ipv4-mask MASK        IPv4 subnet mask
 *   --ipv4-gateway GW       IPv4 gateway
 *   --ipv6 ADDR             IPv6 address
 *   --ipv6-prefix LEN       IPv6 prefix length
 *   --ipv6-gateway GW       IPv6 gateway
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
#include <ctype.h>

#ifdef _MSC_VER
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ws2_32.lib")
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define MAX_IFACE_LEN 128
#define MAX_PATH_LEN 512
#define MAX_ADDR_LEN 64
#define CMD_BUFFER_SIZE 2048
#define CONFIG_LINE_SIZE 512

#define DEFAULT_CONFIG_FILE L"static-ip-fix.ini"

/* ============================================================================
 * CONFIGURATION STRUCTURE
 * ============================================================================ */

typedef struct {
    /* Interface */
    wchar_t interface_name[MAX_IFACE_LEN];

    /* IPv4 */
    wchar_t ipv4_address[MAX_ADDR_LEN];
    wchar_t ipv4_mask[MAX_ADDR_LEN];
    wchar_t ipv4_gateway[MAX_ADDR_LEN];

    /* IPv6 */
    wchar_t ipv6_address[MAX_ADDR_LEN];
    wchar_t ipv6_prefix[16];
    wchar_t ipv6_gateway[MAX_ADDR_LEN];

    /* Flags */
    int dns_only;
    int has_ipv4;
    int has_ipv6;
} Config;

/* Global configuration */
static Config g_config;

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
    L"1.1.1.1", L"1.0.0.1",
    L"2606:4700:4700::1111", L"2606:4700:4700::1001",
    L"8.8.8.8", L"8.8.4.4",
    L"2001:4860:4860::8888", L"2001:4860:4860::8844",
    NULL
};

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

static void print_error(const wchar_t *msg)
{
    fwprintf(stderr, L"[ERROR] %ls\n", msg);
}

static void print_info(const wchar_t *msg)
{
    wprintf(L"[INFO] %ls\n", msg);
}

static void print_success(const wchar_t *msg)
{
    wprintf(L"[OK] %ls\n", msg);
}

/*
 * Trim whitespace from both ends of a string (in place)
 */
static wchar_t *trim(wchar_t *str)
{
    wchar_t *end;

    /* Trim leading */
    while (iswspace(*str)) str++;

    if (*str == L'\0') return str;

    /* Trim trailing */
    end = str + wcslen(str) - 1;
    while (end > str && iswspace(*end)) end--;
    end[1] = L'\0';

    return str;
}

/*
 * Validate interface alias - allow only safe characters
 */
static int validate_interface_alias(const wchar_t *alias)
{
    size_t len = 0;

    if (!alias || *alias == L'\0') {
        return 0;
    }

    if (FAILED(StringCchLengthW(alias, MAX_IFACE_LEN + 1, &len)) || len > MAX_IFACE_LEN) {
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
            return 0;
        }
    }

    return 1;
}

/*
 * Execute a process and wait for completion
 */
static int run_process(wchar_t *cmdline, int silent)
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

static int run_netsh(const wchar_t *args)
{
    wchar_t cmdline[CMD_BUFFER_SIZE];

    if (FAILED(StringCchPrintfW(cmdline, CMD_BUFFER_SIZE, L"netsh.exe %ls", args))) {
        print_error(L"Command line too long");
        return -1;
    }

    return run_process(cmdline, 0);
}

static void run_netsh_silent(const wchar_t *args)
{
    wchar_t cmdline[CMD_BUFFER_SIZE];

    if (SUCCEEDED(StringCchPrintfW(cmdline, CMD_BUFFER_SIZE, L"netsh.exe %ls", args))) {
        run_process(cmdline, 1);
    }
}

/* ============================================================================
 * INTERFACE LISTING
 * ============================================================================ */

static void list_interfaces(void)
{
    ULONG bufLen = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    PIP_ADAPTER_ADDRESSES pCurrAddr = NULL;
    ULONG ret;
    int count = 0;

    pAddresses = (IP_ADAPTER_ADDRESSES *)HeapAlloc(GetProcessHeap(), 0, bufLen);
    if (!pAddresses) {
        print_error(L"Memory allocation failed");
        return;
    }

    ret = GetAdaptersAddresses(AF_UNSPEC,
        GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST,
        NULL, pAddresses, &bufLen);

    if (ret == ERROR_BUFFER_OVERFLOW) {
        HeapFree(GetProcessHeap(), 0, pAddresses);
        pAddresses = (IP_ADAPTER_ADDRESSES *)HeapAlloc(GetProcessHeap(), 0, bufLen);
        if (!pAddresses) {
            print_error(L"Memory allocation failed");
            return;
        }
        ret = GetAdaptersAddresses(AF_UNSPEC,
            GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST,
            NULL, pAddresses, &bufLen);
    }

    if (ret != NO_ERROR) {
        print_error(L"GetAdaptersAddresses failed");
        HeapFree(GetProcessHeap(), 0, pAddresses);
        return;
    }

    wprintf(L"\nAvailable network interfaces:\n");
    wprintf(L"========================================\n\n");

    pCurrAddr = pAddresses;
    while (pCurrAddr) {
        /* Skip loopback and tunnel adapters */
        if (pCurrAddr->IfType != IF_TYPE_SOFTWARE_LOOPBACK &&
            pCurrAddr->IfType != IF_TYPE_TUNNEL &&
            pCurrAddr->OperStatus == IfOperStatusUp) {

            count++;
            wprintf(L"  [%d] %ls\n", count, pCurrAddr->FriendlyName);
            wprintf(L"      Type: ");

            switch (pCurrAddr->IfType) {
                case IF_TYPE_ETHERNET_CSMACD:
                    wprintf(L"Ethernet\n");
                    break;
                case IF_TYPE_IEEE80211:
                    wprintf(L"Wi-Fi\n");
                    break;
                default:
                    wprintf(L"Other (%lu)\n", pCurrAddr->IfType);
            }

            wprintf(L"      Status: Up\n");

            /* Show IP addresses */
            PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddr->FirstUnicastAddress;
            while (pUnicast) {
                char ipStr[64];
                DWORD ipStrLen = sizeof(ipStr);

                if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                    struct sockaddr_in *sa = (struct sockaddr_in *)pUnicast->Address.lpSockaddr;
                    inet_ntop(AF_INET, &sa->sin_addr, ipStr, ipStrLen);
                    wprintf(L"      IPv4: %S\n", ipStr);
                } else if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6) {
                    struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)pUnicast->Address.lpSockaddr;
                    inet_ntop(AF_INET6, &sa6->sin6_addr, ipStr, ipStrLen);
                    /* Skip link-local for cleaner output */
                    if (strncmp(ipStr, "fe80:", 5) != 0) {
                        wprintf(L"      IPv6: %S\n", ipStr);
                    }
                }
                pUnicast = pUnicast->Next;
            }
            wprintf(L"\n");
        }
        pCurrAddr = pCurrAddr->Next;
    }

    if (count == 0) {
        wprintf(L"  No active network interfaces found.\n\n");
    }

    HeapFree(GetProcessHeap(), 0, pAddresses);
}

/* ============================================================================
 * CONFIGURATION FILE PARSER
 * ============================================================================ */

static int parse_config_file(const wchar_t *filepath)
{
    FILE *fp;
    wchar_t line[CONFIG_LINE_SIZE];
    wchar_t section[64] = L"";

    if (_wfopen_s(&fp, filepath, L"r, ccs=UTF-8") != 0 || !fp) {
        return -1;
    }

    while (fgetws(line, CONFIG_LINE_SIZE, fp)) {
        wchar_t *trimmed = trim(line);

        /* Skip empty lines and comments */
        if (*trimmed == L'\0' || *trimmed == L';' || *trimmed == L'#') {
            continue;
        }

        /* Section header */
        if (*trimmed == L'[') {
            wchar_t *end = wcschr(trimmed, L']');
            if (end) {
                *end = L'\0';
                StringCchCopyW(section, 64, trimmed + 1);
            }
            continue;
        }

        /* Key = Value */
        wchar_t *eq = wcschr(trimmed, L'=');
        if (eq) {
            *eq = L'\0';
            wchar_t *key = trim(trimmed);
            wchar_t *value = trim(eq + 1);

            /* Remove surrounding quotes if present */
            size_t vlen = wcslen(value);
            if (vlen >= 2 && ((value[0] == L'"' && value[vlen-1] == L'"') ||
                              (value[0] == L'\'' && value[vlen-1] == L'\''))) {
                value[vlen-1] = L'\0';
                value++;
            }

            /* Parse based on section */
            if (_wcsicmp(section, L"interface") == 0) {
                if (_wcsicmp(key, L"name") == 0) {
                    StringCchCopyW(g_config.interface_name, MAX_IFACE_LEN, value);
                }
            }
            else if (_wcsicmp(section, L"ipv4") == 0) {
                if (_wcsicmp(key, L"address") == 0) {
                    StringCchCopyW(g_config.ipv4_address, MAX_ADDR_LEN, value);
                    g_config.has_ipv4 = 1;
                }
                else if (_wcsicmp(key, L"netmask") == 0 || _wcsicmp(key, L"mask") == 0) {
                    StringCchCopyW(g_config.ipv4_mask, MAX_ADDR_LEN, value);
                }
                else if (_wcsicmp(key, L"gateway") == 0) {
                    StringCchCopyW(g_config.ipv4_gateway, MAX_ADDR_LEN, value);
                }
            }
            else if (_wcsicmp(section, L"ipv6") == 0) {
                if (_wcsicmp(key, L"address") == 0) {
                    StringCchCopyW(g_config.ipv6_address, MAX_ADDR_LEN, value);
                    g_config.has_ipv6 = 1;
                }
                else if (_wcsicmp(key, L"prefix") == 0) {
                    StringCchCopyW(g_config.ipv6_prefix, 16, value);
                }
                else if (_wcsicmp(key, L"gateway") == 0) {
                    StringCchCopyW(g_config.ipv6_gateway, MAX_ADDR_LEN, value);
                }
            }
        }
    }

    fclose(fp);
    return 0;
}

/* ============================================================================
 * COMMAND LINE PARSER
 * ============================================================================ */

typedef enum {
    MODE_NONE,
    MODE_HELP,
    MODE_LIST,
    MODE_CLOUDFLARE,
    MODE_GOOGLE,
    MODE_STATUS
} RunMode;

static void print_help(void)
{
    wprintf(L"\n");
    wprintf(L"static-ip-fix - Configure static IP and DNS-over-HTTPS on Windows\n");
    wprintf(L"\n");
    wprintf(L"USAGE:\n");
    wprintf(L"    static-ip-fix.exe [OPTIONS] <MODE>\n");
    wprintf(L"\n");
    wprintf(L"MODES:\n");
    wprintf(L"    cloudflare    Configure DNS with Cloudflare (1.1.1.1) + DoH\n");
    wprintf(L"    google        Configure DNS with Google (8.8.8.8) + DoH\n");
    wprintf(L"    status        Show current DNS encryption status\n");
    wprintf(L"\n");
    wprintf(L"OPTIONS:\n");
    wprintf(L"    -h, --help              Show this help message\n");
    wprintf(L"    -c, --config FILE       Load configuration from FILE\n");
    wprintf(L"    -l, --list-interfaces   List available network interfaces\n");
    wprintf(L"    -i, --interface NAME    Specify network interface name\n");
    wprintf(L"    --dns-only              Only configure DNS (skip static IP setup)\n");
    wprintf(L"\n");
    wprintf(L"IP OVERRIDE OPTIONS:\n");
    wprintf(L"    --ipv4 ADDR             IPv4 address (e.g., 192.168.1.100)\n");
    wprintf(L"    --ipv4-mask MASK        IPv4 subnet mask (e.g., 255.255.255.0)\n");
    wprintf(L"    --ipv4-gateway GW       IPv4 gateway (e.g., 192.168.1.1)\n");
    wprintf(L"    --ipv6 ADDR             IPv6 address\n");
    wprintf(L"    --ipv6-prefix LEN       IPv6 prefix length (e.g., 64)\n");
    wprintf(L"    --ipv6-gateway GW       IPv6 gateway (link-local address)\n");
    wprintf(L"\n");
    wprintf(L"CONFIGURATION FILE:\n");
    wprintf(L"    The program looks for 'static-ip-fix.ini' in the current directory.\n");
    wprintf(L"    Use -c/--config to specify a different file.\n");
    wprintf(L"\n");
    wprintf(L"    Example config file:\n");
    wprintf(L"\n");
    wprintf(L"        [interface]\n");
    wprintf(L"        name = Ethernet\n");
    wprintf(L"\n");
    wprintf(L"        [ipv4]\n");
    wprintf(L"        address = 192.168.1.100\n");
    wprintf(L"        netmask = 255.255.255.0\n");
    wprintf(L"        gateway = 192.168.1.1\n");
    wprintf(L"\n");
    wprintf(L"        [ipv6]\n");
    wprintf(L"        address = 2001:db8::100\n");
    wprintf(L"        prefix = 64\n");
    wprintf(L"        gateway = fe80::1\n");
    wprintf(L"\n");
    wprintf(L"PRIORITY:\n");
    wprintf(L"    Command line arguments override config file values.\n");
    wprintf(L"\n");
    wprintf(L"EXAMPLES:\n");
    wprintf(L"    static-ip-fix.exe -l\n");
    wprintf(L"    static-ip-fix.exe -i \"Wi-Fi\" --dns-only cloudflare\n");
    wprintf(L"    static-ip-fix.exe -c myconfig.ini cloudflare\n");
    wprintf(L"    static-ip-fix.exe --interface Ethernet status\n");
    wprintf(L"\n");
    wprintf(L"NOTE:\n");
    wprintf(L"    The cloudflare and google modes require Administrator privileges.\n");
    wprintf(L"\n");
}

static RunMode parse_args(int argc, wchar_t *argv[], wchar_t *config_file)
{
    RunMode mode = MODE_NONE;
    config_file[0] = L'\0';

    for (int i = 1; i < argc; i++) {
        wchar_t *arg = argv[i];

        /* Help */
        if (_wcsicmp(arg, L"-h") == 0 || _wcsicmp(arg, L"--help") == 0) {
            return MODE_HELP;
        }

        /* List interfaces */
        if (_wcsicmp(arg, L"-l") == 0 || _wcsicmp(arg, L"--list-interfaces") == 0) {
            return MODE_LIST;
        }

        /* Config file */
        if (_wcsicmp(arg, L"-c") == 0 || _wcsicmp(arg, L"--config") == 0) {
            if (i + 1 < argc) {
                StringCchCopyW(config_file, MAX_PATH_LEN, argv[++i]);
            } else {
                print_error(L"--config requires a file path");
                return MODE_NONE;
            }
            continue;
        }

        /* Interface */
        if (_wcsicmp(arg, L"-i") == 0 || _wcsicmp(arg, L"--interface") == 0) {
            if (i + 1 < argc) {
                StringCchCopyW(g_config.interface_name, MAX_IFACE_LEN, argv[++i]);
            } else {
                print_error(L"--interface requires a name");
                return MODE_NONE;
            }
            continue;
        }

        /* DNS only */
        if (_wcsicmp(arg, L"--dns-only") == 0) {
            g_config.dns_only = 1;
            continue;
        }

        /* IPv4 overrides */
        if (_wcsicmp(arg, L"--ipv4") == 0) {
            if (i + 1 < argc) {
                StringCchCopyW(g_config.ipv4_address, MAX_ADDR_LEN, argv[++i]);
                g_config.has_ipv4 = 1;
            }
            continue;
        }
        if (_wcsicmp(arg, L"--ipv4-mask") == 0) {
            if (i + 1 < argc) {
                StringCchCopyW(g_config.ipv4_mask, MAX_ADDR_LEN, argv[++i]);
            }
            continue;
        }
        if (_wcsicmp(arg, L"--ipv4-gateway") == 0) {
            if (i + 1 < argc) {
                StringCchCopyW(g_config.ipv4_gateway, MAX_ADDR_LEN, argv[++i]);
            }
            continue;
        }

        /* IPv6 overrides */
        if (_wcsicmp(arg, L"--ipv6") == 0) {
            if (i + 1 < argc) {
                StringCchCopyW(g_config.ipv6_address, MAX_ADDR_LEN, argv[++i]);
                g_config.has_ipv6 = 1;
            }
            continue;
        }
        if (_wcsicmp(arg, L"--ipv6-prefix") == 0) {
            if (i + 1 < argc) {
                StringCchCopyW(g_config.ipv6_prefix, 16, argv[++i]);
            }
            continue;
        }
        if (_wcsicmp(arg, L"--ipv6-gateway") == 0) {
            if (i + 1 < argc) {
                StringCchCopyW(g_config.ipv6_gateway, MAX_ADDR_LEN, argv[++i]);
            }
            continue;
        }

        /* Modes */
        if (_wcsicmp(arg, L"cloudflare") == 0) {
            mode = MODE_CLOUDFLARE;
            continue;
        }
        if (_wcsicmp(arg, L"google") == 0) {
            mode = MODE_GOOGLE;
            continue;
        }
        if (_wcsicmp(arg, L"status") == 0) {
            mode = MODE_STATUS;
            continue;
        }

        /* Unknown argument */
        wchar_t errmsg[256];
        StringCchPrintfW(errmsg, 256, L"Unknown argument: %ls", arg);
        print_error(errmsg);
        return MODE_NONE;
    }

    return mode;
}

/* ============================================================================
 * ROLLBACK FUNCTION
 * ============================================================================ */

static void rollback(void)
{
    wchar_t cmd[CMD_BUFFER_SIZE];

    wprintf(L"\n");
    print_info(L"Rolling back changes...");

    /* Reset IPv4 DNS to DHCP */
    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"interface ipv4 set dnsservers name=\"%ls\" source=dhcp",
        g_config.interface_name);
    run_netsh_silent(cmd);
    print_info(L"IPv4 DNS reset to DHCP");

    /* Reset IPv6 DNS to DHCP */
    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"interface ipv6 set dnsservers name=\"%ls\" source=dhcp",
        g_config.interface_name);
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

static int apply_static_ipv4(void)
{
    wchar_t cmd[CMD_BUFFER_SIZE];
    int ret;

    if (!g_config.has_ipv4 || g_config.ipv4_address[0] == L'\0') {
        print_info(L"No IPv4 configuration specified, skipping");
        return 0;
    }

    print_info(L"Configuring static IPv4 address...");

    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"interface ipv4 set address name=\"%ls\" static %ls %ls %ls",
        g_config.interface_name, g_config.ipv4_address,
        g_config.ipv4_mask, g_config.ipv4_gateway);

    ret = run_netsh(cmd);
    if (ret != 0) {
        print_error(L"Failed to set static IPv4 address");
        return -1;
    }

    wchar_t msg[256];
    StringCchPrintfW(msg, 256, L"IPv4: %ls/%ls gateway %ls",
        g_config.ipv4_address, g_config.ipv4_mask, g_config.ipv4_gateway);
    print_success(msg);

    return 0;
}

static int apply_static_ipv6(void)
{
    wchar_t cmd[CMD_BUFFER_SIZE];
    int ret;

    if (!g_config.has_ipv6 || g_config.ipv6_address[0] == L'\0') {
        print_info(L"No IPv6 configuration specified, skipping");
        return 0;
    }

    print_info(L"Configuring static IPv6 address...");

    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"interface ipv6 set address interface=\"%ls\" address=%ls/%ls",
        g_config.interface_name, g_config.ipv6_address, g_config.ipv6_prefix);

    ret = run_netsh(cmd);
    if (ret != 0) {
        print_error(L"Failed to set static IPv6 address");
        return -1;
    }

    /* Add default route via link-local gateway */
    if (g_config.ipv6_gateway[0] != L'\0') {
        wchar_t delcmd[CMD_BUFFER_SIZE];
        StringCchPrintfW(delcmd, CMD_BUFFER_SIZE,
            L"interface ipv6 delete route ::/0 interface=\"%ls\"",
            g_config.interface_name);
        run_netsh_silent(delcmd);

        StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
            L"interface ipv6 add route ::/0 interface=\"%ls\" nexthop=%ls",
            g_config.interface_name, g_config.ipv6_gateway);

        ret = run_netsh(cmd);
        if (ret != 0) {
            print_error(L"Warning: Could not add IPv6 default route");
        }
    }

    wchar_t msg[256];
    StringCchPrintfW(msg, 256, L"IPv6: %ls/%ls gateway %ls",
        g_config.ipv6_address, g_config.ipv6_prefix, g_config.ipv6_gateway);
    print_success(msg);

    return 0;
}

/* ============================================================================
 * DNS CONFIGURATION
 * ============================================================================ */

static int apply_dns_ipv4(const wchar_t *dns1, const wchar_t *dns2)
{
    wchar_t cmd[CMD_BUFFER_SIZE];
    int ret;

    print_info(L"Configuring IPv4 DNS servers...");

    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"interface ipv4 set dnsservers name=\"%ls\" static %ls primary validate=no",
        g_config.interface_name, dns1);

    ret = run_netsh(cmd);
    if (ret != 0) {
        print_error(L"Failed to set primary IPv4 DNS");
        return -1;
    }

    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"interface ipv4 add dnsservers name=\"%ls\" %ls index=2 validate=no",
        g_config.interface_name, dns2);

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

static int apply_dns_ipv6(const wchar_t *dns1, const wchar_t *dns2)
{
    wchar_t cmd[CMD_BUFFER_SIZE];
    int ret;

    print_info(L"Configuring IPv6 DNS servers...");

    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"interface ipv6 set dnsservers name=\"%ls\" static %ls primary validate=no",
        g_config.interface_name, dns1);

    ret = run_netsh(cmd);
    if (ret != 0) {
        print_error(L"Failed to set primary IPv6 DNS");
        return -1;
    }

    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"interface ipv6 add dnsservers name=\"%ls\" %ls index=2 validate=no",
        g_config.interface_name, dns2);

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

static int add_doh_template(const wchar_t *server, const wchar_t *doh_template)
{
    wchar_t cmd[CMD_BUFFER_SIZE];
    int ret;

    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"dns delete encryption server=%ls", server);
    run_netsh_silent(cmd);

    StringCchPrintfW(cmd, CMD_BUFFER_SIZE,
        L"dns add encryption server=%ls dohtemplate=%ls autoupgrade=yes udpfallback=no",
        server, doh_template);

    ret = run_netsh(cmd);
    if (ret != 0) {
        wchar_t errmsg[512];
        StringCchPrintfW(errmsg, 512, L"Failed to add DoH template for %ls", server);
        print_error(errmsg);
        return -1;
    }

    return 0;
}

static int apply_doh(const wchar_t *dns_ipv4_1, const wchar_t *dns_ipv4_2,
                     const wchar_t *dns_ipv6_1, const wchar_t *dns_ipv6_2,
                     const wchar_t *doh_template)
{
    print_info(L"Configuring DNS-over-HTTPS encryption...");

    if (add_doh_template(dns_ipv4_1, doh_template) != 0) return -1;
    if (add_doh_template(dns_ipv4_2, doh_template) != 0) return -1;
    if (add_doh_template(dns_ipv6_1, doh_template) != 0) return -1;
    if (add_doh_template(dns_ipv6_2, doh_template) != 0) return -1;

    wchar_t msg[512];
    StringCchPrintfW(msg, 512, L"DoH template: %ls (autoupgrade=yes, udpfallback=no)", doh_template);
    print_success(msg);

    return 0;
}

/* ============================================================================
 * STATUS MODE
 * ============================================================================ */

typedef struct {
    wchar_t address[MAX_ADDR_LEN];
    int has_template;
    int autoupgrade;
    int udpfallback;
} DnsServerInfo;

static void query_doh_info(const wchar_t *server, DnsServerInfo *info)
{
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t cmdline[CMD_BUFFER_SIZE];
    char buffer[4096];
    DWORD bytesRead, totalRead;

    StringCchCopyW(info->address, MAX_ADDR_LEN, server);
    info->has_template = 0;
    info->autoupgrade = 0;
    info->udpfallback = 1;

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

    /* Read all output in a loop */
    ZeroMemory(buffer, sizeof(buffer));
    totalRead = 0;
    while (totalRead < sizeof(buffer) - 1) {
        if (!ReadFile(hReadPipe, buffer + totalRead,
                     (DWORD)(sizeof(buffer) - 1 - totalRead), &bytesRead, NULL) || bytesRead == 0) {
            break;
        }
        totalRead += bytesRead;
    }
    buffer[totalRead] = '\0';

    WaitForSingleObject(pi.hProcess, 5000);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);

    /* Parse output */
    if (totalRead > 0) {
        if (strstr(buffer, "Encryption settings") != NULL ||
            strstr(buffer, "DNS-over-HTTPS template") != NULL ||
            strstr(buffer, "dohtemplate") != NULL) {
            info->has_template = 1;
        }

        /* Check for Auto-upgrade: yes */
        char *auto_line = strstr(buffer, "Auto-upgrade");
        if (auto_line) {
            char *colon = strchr(auto_line, ':');
            if (colon && strstr(colon, "yes")) {
                info->autoupgrade = 1;
            }
        }

        /* Check for UDP-fallback: no */
        char *fallback_line = strstr(buffer, "UDP-fallback");
        if (fallback_line) {
            char *colon = strchr(fallback_line, ':');
            if (colon && strstr(colon, "no")) {
                info->udpfallback = 0;
            }
        }
    }
}

/*
 * Helper to find and extract an IPv4 address from a string
 * Returns pointer to start of IP or NULL if not found
 */
static char *find_ipv4(char *str)
{
    char *p = str;
    while (*p) {
        /* Look for digit that could start an IP */
        if (*p >= '0' && *p <= '9') {
            /* Check if it looks like an IPv4 (has dots) */
            char *start = p;
            int dots = 0;
            while ((*p >= '0' && *p <= '9') || *p == '.') {
                if (*p == '.') dots++;
                p++;
            }
            if (dots == 3) {
                return start;
            }
        } else {
            p++;
        }
    }
    return NULL;
}

/*
 * Helper to find and extract an IPv6 address from a string
 * Returns pointer to start of IP or NULL if not found
 */
static char *find_ipv6(char *str)
{
    char *p = str;
    while (*p) {
        /* Look for hex digit or colon that could start IPv6 */
        if ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
            (*p >= 'A' && *p <= 'F') || *p == ':') {
            char *start = p;
            int colons = 0;
            while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
                   (*p >= 'A' && *p <= 'F') || *p == ':') {
                if (*p == ':') colons++;
                p++;
            }
            /* IPv6 has at least 2 colons */
            if (colons >= 2) {
                return start;
            }
        } else {
            p++;
        }
    }
    return NULL;
}

static int get_configured_dns(DnsServerInfo *ipv4_servers, int *ipv4_count,
                              DnsServerInfo *ipv6_servers, int *ipv6_count)
{
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t cmdline[CMD_BUFFER_SIZE];
    char buffer[8192];
    DWORD bytesRead, totalRead;

    *ipv4_count = 0;
    *ipv6_count = 0;

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    /* Get IPv4 DNS servers */
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

    StringCchPrintfW(cmdline, CMD_BUFFER_SIZE,
        L"netsh.exe interface ipv4 show dnsservers name=\"%ls\"", g_config.interface_name);

    if (CreateProcessW(NULL, cmdline, NULL, NULL, TRUE,
                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hWritePipe);

        /* Read all output */
        ZeroMemory(buffer, sizeof(buffer));
        totalRead = 0;
        while (totalRead < sizeof(buffer) - 1) {
            if (!ReadFile(hReadPipe, buffer + totalRead,
                         (DWORD)(sizeof(buffer) - 1 - totalRead), &bytesRead, NULL) || bytesRead == 0) {
                break;
            }
            totalRead += bytesRead;
        }
        buffer[totalRead] = '\0';

        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hReadPipe);

        /* Parse each line for IPv4 addresses */
        char *line = strtok(buffer, "\r\n");
        while (line && *ipv4_count < 4) {
            char *ip_start = find_ipv4(line);
            if (ip_start) {
                char ip[64] = {0};
                int i = 0;
                while (ip_start[i] && ((ip_start[i] >= '0' && ip_start[i] <= '9') || ip_start[i] == '.') && i < 63) {
                    ip[i] = ip_start[i];
                    i++;
                }
                ip[i] = '\0';

                if (strlen(ip) >= 7) {
                    MultiByteToWideChar(CP_ACP, 0, ip, -1,
                        ipv4_servers[*ipv4_count].address, MAX_ADDR_LEN);
                    query_doh_info(ipv4_servers[*ipv4_count].address,
                        &ipv4_servers[*ipv4_count]);
                    (*ipv4_count)++;
                }
            }
            line = strtok(NULL, "\r\n");
        }
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
        L"netsh.exe interface ipv6 show dnsservers name=\"%ls\"", g_config.interface_name);

    if (CreateProcessW(NULL, cmdline, NULL, NULL, TRUE,
                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hWritePipe);

        /* Read all output */
        ZeroMemory(buffer, sizeof(buffer));
        totalRead = 0;
        while (totalRead < sizeof(buffer) - 1) {
            if (!ReadFile(hReadPipe, buffer + totalRead,
                         (DWORD)(sizeof(buffer) - 1 - totalRead), &bytesRead, NULL) || bytesRead == 0) {
                break;
            }
            totalRead += bytesRead;
        }
        buffer[totalRead] = '\0';

        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hReadPipe);

        /* Parse each line for IPv6 addresses */
        char *line = strtok(buffer, "\r\n");
        while (line && *ipv6_count < 4) {
            char *ip_start = find_ipv6(line);
            if (ip_start) {
                char ip[64] = {0};
                int i = 0;
                while (ip_start[i] && (
                       (ip_start[i] >= '0' && ip_start[i] <= '9') ||
                       (ip_start[i] >= 'a' && ip_start[i] <= 'f') ||
                       (ip_start[i] >= 'A' && ip_start[i] <= 'F') ||
                       ip_start[i] == ':') && i < 63) {
                    ip[i] = ip_start[i];
                    i++;
                }
                ip[i] = '\0';

                if (strlen(ip) >= 3 && strchr(ip, ':')) {
                    MultiByteToWideChar(CP_ACP, 0, ip, -1,
                        ipv6_servers[*ipv6_count].address, MAX_ADDR_LEN);
                    query_doh_info(ipv6_servers[*ipv6_count].address,
                        &ipv6_servers[*ipv6_count]);
                    (*ipv6_count)++;
                }
            }
            line = strtok(NULL, "\r\n");
        }
    } else {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
    }

    return 0;
}

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
    wprintf(L"Status for interface: %ls\n", g_config.interface_name);
    wprintf(L"========================================\n\n");

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

    if (ipv4_total == 0) {
        wprintf(L"  IPv4: NO DNS CONFIGURED\n");
    } else if (ipv4_encrypted == ipv4_total) {
        wprintf(L"  IPv4: ENCRYPTED (%d/%d servers)\n", ipv4_encrypted, ipv4_total);
    } else {
        wprintf(L"  IPv4: PARTIALLY ENCRYPTED (%d/%d servers)\n", ipv4_encrypted, ipv4_total);
    }

    if (ipv6_total == 0) {
        wprintf(L"  IPv6: NO DNS CONFIGURED\n");
    } else if (ipv6_encrypted == ipv6_total) {
        wprintf(L"  IPv6: ENCRYPTED (%d/%d servers)\n", ipv6_encrypted, ipv6_total);
    } else {
        wprintf(L"  IPv6: PARTIALLY ENCRYPTED (%d/%d servers)\n", ipv6_encrypted, ipv6_total);
    }

    wprintf(L"  Fallback: %ls\n", any_fallback ? L"ENABLED (insecure)" : L"DISABLED");
    wprintf(L"  Unencrypted DNS: %ls\n", any_unencrypted ? L"YES (insecure)" : L"NONE");

    wprintf(L"\n");

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
    if (g_config.dns_only) {
        wprintf(L"  Cloudflare DNS + DoH (DNS only mode)\n");
    } else {
        wprintf(L"  Static IP + Cloudflare DNS + DoH\n");
    }
    wprintf(L"  Interface: %ls\n", g_config.interface_name);
    wprintf(L"========================================\n\n");

    if (!g_config.dns_only) {
        if (apply_static_ipv4() != 0) {
            rollback();
            return 1;
        }

        if (apply_static_ipv6() != 0) {
            rollback();
            return 1;
        }
    }

    if (apply_dns_ipv4(CF_DNS_IPV4_1, CF_DNS_IPV4_2) != 0) {
        rollback();
        return 1;
    }

    if (apply_dns_ipv6(CF_DNS_IPV6_1, CF_DNS_IPV6_2) != 0) {
        rollback();
        return 1;
    }

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
    if (g_config.dns_only) {
        wprintf(L"  Google DNS + DoH (DNS only mode)\n");
    } else {
        wprintf(L"  Static IP + Google DNS + DoH\n");
    }
    wprintf(L"  Interface: %ls\n", g_config.interface_name);
    wprintf(L"========================================\n\n");

    if (!g_config.dns_only) {
        if (apply_static_ipv4() != 0) {
            rollback();
            return 1;
        }

        if (apply_static_ipv6() != 0) {
            rollback();
            return 1;
        }
    }

    if (apply_dns_ipv4(GOOGLE_DNS_IPV4_1, GOOGLE_DNS_IPV4_2) != 0) {
        rollback();
        return 1;
    }

    if (apply_dns_ipv6(GOOGLE_DNS_IPV6_1, GOOGLE_DNS_IPV6_2) != 0) {
        rollback();
        return 1;
    }

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

int wmain(int argc, wchar_t *argv[])
{
    wchar_t config_file[MAX_PATH_LEN] = L"";
    RunMode mode;

    /* Initialize config with empty values */
    ZeroMemory(&g_config, sizeof(g_config));

    /* Parse command line arguments first (to get config file path) */
    mode = parse_args(argc, argv, config_file);

    /* Handle help and list modes immediately */
    if (mode == MODE_HELP) {
        print_help();
        return 0;
    }

    if (mode == MODE_LIST) {
        list_interfaces();
        return 0;
    }

    /* Try to load config file */
    if (config_file[0] != L'\0') {
        /* Explicit config file specified */
        if (parse_config_file(config_file) != 0) {
            wchar_t errmsg[512];
            StringCchPrintfW(errmsg, 512, L"Cannot read config file: %ls", config_file);
            print_error(errmsg);
            return 1;
        }
        wchar_t msg[512];
        StringCchPrintfW(msg, 512, L"Loaded config from: %ls", config_file);
        print_info(msg);
    } else {
        /* Try default config file (optional) */
        if (parse_config_file(DEFAULT_CONFIG_FILE) == 0) {
            print_info(L"Loaded config from: static-ip-fix.ini");
        }
    }

    /* Re-parse args to override config file values */
    wchar_t dummy[MAX_PATH_LEN];
    mode = parse_args(argc, argv, dummy);

    /* Validate we have a mode */
    if (mode == MODE_NONE) {
        print_error(L"No mode specified. Use --help for usage information.");
        return 1;
    }

    /* Validate interface is set */
    if (g_config.interface_name[0] == L'\0') {
        print_error(L"No interface specified. Use -i/--interface or set in config file.");
        wprintf(L"\nTip: Use -l/--list-interfaces to see available interfaces.\n");
        return 1;
    }

    /* Validate interface name */
    if (!validate_interface_alias(g_config.interface_name)) {
        print_error(L"Invalid interface name");
        return 1;
    }

    /* Set default values if not specified */
    if (g_config.ipv4_mask[0] == L'\0' && g_config.has_ipv4) {
        StringCchCopyW(g_config.ipv4_mask, MAX_ADDR_LEN, L"255.255.255.0");
    }
    if (g_config.ipv6_prefix[0] == L'\0' && g_config.has_ipv6) {
        StringCchCopyW(g_config.ipv6_prefix, 16, L"64");
    }

    /* Execute mode */
    switch (mode) {
        case MODE_CLOUDFLARE:
            return run_cloudflare();
        case MODE_GOOGLE:
            return run_google();
        case MODE_STATUS:
            return run_status();
        default:
            print_error(L"Invalid mode");
            return 1;
    }
}
