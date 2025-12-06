/*
 * network.c - Network configuration (IP, DNS, DoH)
 */

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include "network.h"
#include "process.h"

#ifdef _MSC_VER
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#endif

/* ============================================================================
 * DNS SERVER CONSTANTS
 * ============================================================================ */

const wchar_t *CF_DNS_IPV4_1 = L"1.1.1.1";
const wchar_t *CF_DNS_IPV4_2 = L"1.0.0.1";
const wchar_t *CF_DNS_IPV6_1 = L"2606:4700:4700::1111";
const wchar_t *CF_DNS_IPV6_2 = L"2606:4700:4700::1001";
const wchar_t *CF_DOH_TEMPLATE = L"https://cloudflare-dns.com/dns-query";

const wchar_t *GOOGLE_DNS_IPV4_1 = L"8.8.8.8";
const wchar_t *GOOGLE_DNS_IPV4_2 = L"8.8.4.4";
const wchar_t *GOOGLE_DNS_IPV6_1 = L"2001:4860:4860::8888";
const wchar_t *GOOGLE_DNS_IPV6_2 = L"2001:4860:4860::8844";
const wchar_t *GOOGLE_DOH_TEMPLATE = L"https://dns.google/dns-query";

const wchar_t *ALL_DNS_SERVERS[] = {
    L"1.1.1.1", L"1.0.0.1",
    L"2606:4700:4700::1111", L"2606:4700:4700::1001",
    L"8.8.8.8", L"8.8.4.4",
    L"2001:4860:4860::8888", L"2001:4860:4860::8844",
    NULL
};

/* ============================================================================
 * INTERFACE LISTING
 * ============================================================================ */

void network_list_interfaces(void)
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
 * ROLLBACK
 * ============================================================================ */

void network_rollback(void)
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

int network_apply_static_ipv4(void)
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

int network_apply_static_ipv6(void)
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

int network_apply_dns_ipv4(const wchar_t *dns1, const wchar_t *dns2)
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

int network_apply_dns_ipv6(const wchar_t *dns1, const wchar_t *dns2)
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

int network_apply_doh(const wchar_t *dns_ipv4_1, const wchar_t *dns_ipv4_2,
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
