/*
 * status.h - DNS status detection and display
 */

#ifndef STATUS_H
#define STATUS_H

#include "utils.h"
#include "config.h"

/* ============================================================================
 * DNS SERVER INFO
 * ============================================================================ */

typedef struct {
    wchar_t address[MAX_ADDR_LEN];
    int has_template;
    int autoupgrade;
    int udpfallback;
} DnsServerInfo;

/* ============================================================================
 * FUNCTIONS
 * ============================================================================ */

/*
 * Query DoH encryption info for a DNS server
 */
void status_query_doh_info(const wchar_t *server, DnsServerInfo *info);

/*
 * Get configured DNS servers for the interface
 * Returns 0 on success
 */
int status_get_configured_dns(DnsServerInfo *ipv4_servers, int *ipv4_count,
                              DnsServerInfo *ipv6_servers, int *ipv6_count);

/*
 * Run status mode - display encryption status
 * Returns 0 if fully encrypted, 1 otherwise
 */
int status_run(void);

#endif /* STATUS_H */
