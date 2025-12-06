/*
 * dns.h - DNS provider definitions and configuration
 */

#ifndef DNS_H
#define DNS_H

#include "utils.h"

/* ============================================================================
 * DNS PROVIDER STRUCT
 * ============================================================================ */

typedef struct {
    const wchar_t *name;
    const wchar_t *ipv4_primary;
    const wchar_t *ipv4_secondary;
    const wchar_t *ipv6_primary;
    const wchar_t *ipv6_secondary;
    const wchar_t *doh_template;
} DnsProvider;

/* ============================================================================
 * BUILT-IN PROVIDERS
 * ============================================================================ */

extern const DnsProvider DNS_CLOUDFLARE;
extern const DnsProvider DNS_GOOGLE;

/* ============================================================================
 * PROVIDER FUNCTIONS
 * ============================================================================ */

/*
 * Run DNS configuration for the given provider
 * Returns 0 on success, non-zero on failure
 */
int dns_run_provider(const DnsProvider *provider);

#endif /* DNS_H */
