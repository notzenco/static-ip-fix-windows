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

#include "utils.h"
#include "config.h"
#include "process.h"
#include "network.h"
#include "status.h"

/* ============================================================================
 * MODE HANDLERS
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
        if (network_apply_static_ipv4() != 0) {
            network_rollback();
            return 1;
        }

        if (network_apply_static_ipv6() != 0) {
            network_rollback();
            return 1;
        }
    }

    if (network_apply_dns_ipv4(CF_DNS_IPV4_1, CF_DNS_IPV4_2) != 0) {
        network_rollback();
        return 1;
    }

    if (network_apply_dns_ipv6(CF_DNS_IPV6_1, CF_DNS_IPV6_2) != 0) {
        network_rollback();
        return 1;
    }

    if (network_apply_doh(CF_DNS_IPV4_1, CF_DNS_IPV4_2,
                          CF_DNS_IPV6_1, CF_DNS_IPV6_2, CF_DOH_TEMPLATE) != 0) {
        network_rollback();
        return 1;
    }

    wprintf(L"\n");
    print_success(L"Configuration complete!");
    wprintf(L"\n");

    return 0;
}

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
        if (network_apply_static_ipv4() != 0) {
            network_rollback();
            return 1;
        }

        if (network_apply_static_ipv6() != 0) {
            network_rollback();
            return 1;
        }
    }

    if (network_apply_dns_ipv4(GOOGLE_DNS_IPV4_1, GOOGLE_DNS_IPV4_2) != 0) {
        network_rollback();
        return 1;
    }

    if (network_apply_dns_ipv6(GOOGLE_DNS_IPV6_1, GOOGLE_DNS_IPV6_2) != 0) {
        network_rollback();
        return 1;
    }

    if (network_apply_doh(GOOGLE_DNS_IPV4_1, GOOGLE_DNS_IPV4_2,
                          GOOGLE_DNS_IPV6_1, GOOGLE_DNS_IPV6_2, GOOGLE_DOH_TEMPLATE) != 0) {
        network_rollback();
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
            StringCchPrintfW(errmsg, 512, L"Cannot read config file: %ls", config_file);
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
        print_error(L"No interface specified. Use -i/--interface or set in config file.");
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
            return run_cloudflare();
        case MODE_GOOGLE:
            return run_google();
        case MODE_STATUS:
            return status_run();
        default:
            print_error(L"Invalid mode");
            return 1;
    }
}
