/*
 * config.h - Configuration structure and parsing
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "utils.h"

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

    /* DNS servers */
    wchar_t dns_ipv4_primary[MAX_ADDR_LEN];
    wchar_t dns_ipv4_secondary[MAX_ADDR_LEN];
    wchar_t dns_ipv6_primary[MAX_ADDR_LEN];
    wchar_t dns_ipv6_secondary[MAX_ADDR_LEN];

    /* DoH settings */
    wchar_t doh_template[256];
    int doh_autoupgrade;
    int doh_fallback;

    /* Flags */
    int dns_only;
    int has_ipv4;
    int has_ipv6;
    int has_custom_dns;
} Config;

/* Global configuration instance */
extern Config g_config;

/* ============================================================================
 * RUN MODES
 * ============================================================================ */

typedef enum {
    MODE_NONE,
    MODE_HELP,
    MODE_LIST,
    MODE_CLOUDFLARE,
    MODE_GOOGLE,
    MODE_CUSTOM,
    MODE_STATUS
} RunMode;

/* ============================================================================
 * FUNCTIONS
 * ============================================================================ */

/*
 * Initialize config with default values
 */
void config_init(void);

/*
 * Parse configuration from INI file
 * Returns 0 on success, -1 on failure
 */
int config_parse_file(const wchar_t *filepath);

/*
 * Parse command line arguments
 * config_file receives the -c/--config path if specified
 * Returns the run mode
 */
RunMode config_parse_args(int argc, wchar_t *argv[], wchar_t *config_file);

/*
 * Print help message
 */
void config_print_help(void);

/*
 * Set default values for missing config options
 */
void config_set_defaults(void);

#endif /* CONFIG_H */
