/*
 * static-ip-fix - Configure static IP and DNS-over-HTTPS on Windows
 *
 * Usage:
 *   static-ip-fix.exe [options] <mode>
 *
 * Modes:
 *   cloudflare   - Configure DNS with Cloudflare + DoH
 *   google       - Configure DNS with Google + DoH
 *   status       - Show current DNS encryption status
 *
 * Requires Administrator privileges for cloudflare/google modes.
 *
 * Build: make (MinGW-w64)
 */

#include "config.h"
#include "dns.h"
#include "network.h"
#include "status.h"
#include "utils.h"

/* ============================================================================
 * MAIN ENTRY POINT
 * ============================================================================
 */

int wmain(int argc, wchar_t *argv[]) {
    wchar_t config_file[MAX_PATH_LEN] = L"";
    RunMode mode;

    /* Initialize config */
    config_init();

    /* Parse command line arguments (first pass to get config file) */
    mode = config_parse_args(argc, argv, config_file);

    /* Handle help and list modes immediately */
    if (mode == MODE_HELP) {
        config_print_help();
        return 0;
    }

    if (mode == MODE_LIST) {
        network_list_interfaces();
        return 0;
    }

    /* Load config file */
    if (config_file[0] != L'\0') {
        if (config_parse_file(config_file) != 0) {
            wchar_t errmsg[512];
            StringCchPrintfW(errmsg, 512, L"Cannot read config file: %ls",
                             config_file);
            print_error(errmsg);
            return 1;
        }
        wchar_t msg[512];
        StringCchPrintfW(msg, 512, L"Loaded config from: %ls", config_file);
        print_info(msg);
    } else {
        /* Try default config file (optional) */
        if (config_parse_file(DEFAULT_CONFIG_FILE) == 0) {
            print_info(L"Loaded config from: static-ip-fix.ini");
        }
    }

    /* Re-parse args to override config file values */
    wchar_t dummy[MAX_PATH_LEN];
    mode = config_parse_args(argc, argv, dummy);

    /* Validate mode */
    if (mode == MODE_NONE) {
        print_error(L"No mode specified. Use --help for usage information.");
        return 1;
    }

    /* Validate interface */
    if (g_config.interface_name[0] == L'\0') {
        print_error(
            L"No interface specified. Use -i/--interface or set in config file.");
        wprintf(L"\nTip: Use -l/--list-interfaces to see available interfaces.\n");
        return 1;
    }

    if (!validate_interface_alias(g_config.interface_name)) {
        print_error(L"Invalid interface name");
        return 1;
    }

    /* Set defaults */
    config_set_defaults();

    /* Execute mode */
    switch (mode) {
    case MODE_CLOUDFLARE:
        return dns_run_provider(&DNS_CLOUDFLARE);
    case MODE_GOOGLE:
        return dns_run_provider(&DNS_GOOGLE);
    case MODE_CUSTOM:
        if (!g_config.has_custom_dns) {
            print_error(L"Custom mode requires [dns] section in config file.");
            return 1;
        }
        if (g_config.doh_template[0] == L'\0') {
            print_error(L"Custom mode requires [doh] template in config file.");
            return 1;
        }
        DnsProvider custom = {
            .name = L"Custom",
            .ipv4_primary = g_config.dns_ipv4_primary,
            .ipv4_secondary = g_config.dns_ipv4_secondary,
            .ipv6_primary = g_config.dns_ipv6_primary,
            .ipv6_secondary = g_config.dns_ipv6_secondary,
            .doh_template = g_config.doh_template
        };
        return dns_run_provider(&custom);
    case MODE_STATUS:
        return status_run();
    default:
        print_error(L"Invalid mode");
        return 1;
    }
}
