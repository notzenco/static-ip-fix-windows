/*
 * dns.c - DNS provider definitions and configuration
 */

#include "dns.h"
#include "config.h"
#include "network.h"

/* ============================================================================
 * BUILT-IN PROVIDERS
 * ============================================================================ */

const DnsProvider DNS_CLOUDFLARE = {
    .name = L"Cloudflare",
    .ipv4_primary = L"1.1.1.1",
    .ipv4_secondary = L"1.0.0.1",
    .ipv6_primary = L"2606:4700:4700::1111",
    .ipv6_secondary = L"2606:4700:4700::1001",
    .doh_template = L"https://cloudflare-dns.com/dns-query"
};

const DnsProvider DNS_GOOGLE = {
    .name = L"Google",
    .ipv4_primary = L"8.8.8.8",
    .ipv4_secondary = L"8.8.4.4",
    .ipv6_primary = L"2001:4860:4860::8888",
    .ipv6_secondary = L"2001:4860:4860::8844",
    .doh_template = L"https://dns.google/dns-query"
};

/* ============================================================================
 * PROVIDER FUNCTIONS
 * ============================================================================ */

int dns_run_provider(const DnsProvider *provider) {
    wprintf(L"\n");
    wprintf(L"========================================\n");
    if (g_config.dns_only) {
        wprintf(L"  %ls DNS + DoH (DNS only mode)\n", provider->name);
    } else {
        wprintf(L"  Static IP + %ls DNS + DoH\n", provider->name);
    }
    wprintf(L"  Interface: %ls\n", g_config.interface_name);
    wprintf(L"========================================\n\n");

    if (!g_config.dns_only) {
        if (network_apply_static_ipv4() != 0) {
            network_rollback();
            return 1;
        }

        if (network_apply_static_ipv6() != 0) {
            network_rollback();
            return 1;
        }
    }

    if (network_apply_dns_ipv4(provider->ipv4_primary, provider->ipv4_secondary) != 0) {
        network_rollback();
        return 1;
    }

    if (network_apply_dns_ipv6(provider->ipv6_primary, provider->ipv6_secondary) != 0) {
        network_rollback();
        return 1;
    }

    if (network_apply_doh(provider->ipv4_primary, provider->ipv4_secondary,
                          provider->ipv6_primary, provider->ipv6_secondary,
                          provider->doh_template) != 0) {
        network_rollback();
        return 1;
    }

    wprintf(L"\n");
    print_success(L"Configuration complete!");
    wprintf(L"\n");

    return 0;
}
