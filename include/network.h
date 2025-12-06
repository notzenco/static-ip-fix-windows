/*
 * network.h - Network configuration (IP, DNS, DoH)
 */

#ifndef NETWORK_H
#define NETWORK_H

#include "utils.h"
#include "config.h"

/* ============================================================================
 * DNS SERVER CONSTANTS
 * ============================================================================ */

/* Cloudflare DNS */
extern const wchar_t *CF_DNS_IPV4_1;
extern const wchar_t *CF_DNS_IPV4_2;
extern const wchar_t *CF_DNS_IPV6_1;
extern const wchar_t *CF_DNS_IPV6_2;
extern const wchar_t *CF_DOH_TEMPLATE;

/* Google DNS */
extern const wchar_t *GOOGLE_DNS_IPV4_1;
extern const wchar_t *GOOGLE_DNS_IPV4_2;
extern const wchar_t *GOOGLE_DNS_IPV6_1;
extern const wchar_t *GOOGLE_DNS_IPV6_2;
extern const wchar_t *GOOGLE_DOH_TEMPLATE;

/* All DNS servers (for rollback) */
extern const wchar_t *ALL_DNS_SERVERS[];

/* ============================================================================
 * INTERFACE FUNCTIONS
 * ============================================================================ */

/*
 * List available network interfaces
 */
void network_list_interfaces(void);

/* ============================================================================
 * ROLLBACK
 * ============================================================================ */

/*
 * Rollback all changes - restore DHCP and remove DoH templates
 */
void network_rollback(void);

/* ============================================================================
 * STATIC IP CONFIGURATION
 * ============================================================================ */

/*
 * Configure static IPv4 address
 * Returns 0 on success, -1 on failure
 */
int network_apply_static_ipv4(void);

/*
 * Configure static IPv6 address
 * Returns 0 on success, -1 on failure
 */
int network_apply_static_ipv6(void);

/* ============================================================================
 * DNS CONFIGURATION
 * ============================================================================ */

/*
 * Configure IPv4 DNS servers
 * Returns 0 on success, -1 on failure
 */
int network_apply_dns_ipv4(const wchar_t *dns1, const wchar_t *dns2);

/*
 * Configure IPv6 DNS servers
 * Returns 0 on success, -1 on failure
 */
int network_apply_dns_ipv6(const wchar_t *dns1, const wchar_t *dns2);

/* ============================================================================
 * DNS-OVER-HTTPS CONFIGURATION
 * ============================================================================ */

/*
 * Configure DoH for all specified DNS servers
 * Returns 0 on success, -1 on failure
 */
int network_apply_doh(const wchar_t *dns_ipv4_1, const wchar_t *dns_ipv4_2,
                      const wchar_t *dns_ipv6_1, const wchar_t *dns_ipv6_2,
                      const wchar_t *doh_template);

#endif /* NETWORK_H */
