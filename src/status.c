/*
 * status.c - DNS status detection and display
 */

#include <string.h>
#include "status.h"
#include "process.h"

/* ============================================================================
 * DOH INFO QUERY
 * ============================================================================ */

void status_query_doh_info(const wchar_t *server, DnsServerInfo *info)
{
    wchar_t args[CMD_BUFFER_SIZE];
    char buffer[4096];

    StringCchCopyW(info->address, MAX_ADDR_LEN, server);
    info->has_template = 0;
    info->autoupgrade = 0;
    info->udpfallback = 1; /* Default to insecure */

    StringCchPrintfW(args, CMD_BUFFER_SIZE, L"dns show encryption server=%ls", server);

    if (run_netsh_capture(args, buffer, sizeof(buffer)) < 0) {
        return;
    }

    /* Parse output */
    if (strlen(buffer) > 0) {
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

/* ============================================================================
 * DNS DETECTION
 * ============================================================================ */

static void parse_ipv4_dns(const char *buffer, DnsServerInfo *servers, int *count)
{
    char temp[PIPE_BUFFER_SIZE];
    StringCchCopyA(temp, PIPE_BUFFER_SIZE, buffer);

    char *line = strtok(temp, "\r\n");
    while (line && *count < 4) {
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
                MultiByteToWideChar(CP_ACP, 0, ip, -1, servers[*count].address, MAX_ADDR_LEN);
                status_query_doh_info(servers[*count].address, &servers[*count]);
                (*count)++;
            }
        }
        line = strtok(NULL, "\r\n");
    }
}

static void parse_ipv6_dns(const char *buffer, DnsServerInfo *servers, int *count)
{
    char temp[PIPE_BUFFER_SIZE];
    StringCchCopyA(temp, PIPE_BUFFER_SIZE, buffer);

    char *line = strtok(temp, "\r\n");
    while (line && *count < 4) {
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
                MultiByteToWideChar(CP_ACP, 0, ip, -1, servers[*count].address, MAX_ADDR_LEN);
                status_query_doh_info(servers[*count].address, &servers[*count]);
                (*count)++;
            }
        }
        line = strtok(NULL, "\r\n");
    }
}

int status_get_configured_dns(DnsServerInfo *ipv4_servers, int *ipv4_count,
                              DnsServerInfo *ipv6_servers, int *ipv6_count)
{
    wchar_t args[CMD_BUFFER_SIZE];
    char buffer[PIPE_BUFFER_SIZE];

    *ipv4_count = 0;
    *ipv6_count = 0;

    /* Get IPv4 DNS servers */
    StringCchPrintfW(args, CMD_BUFFER_SIZE,
        L"interface ipv4 show dnsservers name=\"%ls\"", g_config.interface_name);

    if (run_netsh_capture(args, buffer, sizeof(buffer)) >= 0) {
        parse_ipv4_dns(buffer, ipv4_servers, ipv4_count);
    }

    /* Get IPv6 DNS servers */
    StringCchPrintfW(args, CMD_BUFFER_SIZE,
        L"interface ipv6 show dnsservers name=\"%ls\"", g_config.interface_name);

    if (run_netsh_capture(args, buffer, sizeof(buffer)) >= 0) {
        parse_ipv6_dns(buffer, ipv6_servers, ipv6_count);
    }

    return 0;
}

/* ============================================================================
 * STATUS DISPLAY
 * ============================================================================ */

int status_run(void)
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

    status_get_configured_dns(ipv4_servers, &ipv4_count, ipv6_servers, &ipv6_count);

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
