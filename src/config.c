/*
 * config.c - Configuration structure and parsing
 */

#include "config.h"

/* Global configuration instance */
Config g_config;

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

void config_init(void)
{
    ZeroMemory(&g_config, sizeof(g_config));
}

/* ============================================================================
 * INI FILE PARSING
 * ============================================================================ */

int config_parse_file(const wchar_t *filepath)
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
            else if (_wcsicmp(section, L"dns") == 0) {
                if (_wcsicmp(key, L"ipv4_servers") == 0) {
                    /* Parse comma-separated: "1.1.1.1, 1.0.0.1" */
                    wchar_t *comma = wcschr(value, L',');
                    if (comma) {
                        *comma = L'\0';
                        StringCchCopyW(g_config.dns_ipv4_primary, MAX_ADDR_LEN, trim(value));
                        StringCchCopyW(g_config.dns_ipv4_secondary, MAX_ADDR_LEN, trim(comma + 1));
                    } else {
                        StringCchCopyW(g_config.dns_ipv4_primary, MAX_ADDR_LEN, trim(value));
                    }
                    g_config.has_custom_dns = 1;
                }
                else if (_wcsicmp(key, L"ipv6_servers") == 0) {
                    /* Parse comma-separated IPv6 addresses */
                    wchar_t *comma = wcschr(value, L',');
                    if (comma) {
                        *comma = L'\0';
                        StringCchCopyW(g_config.dns_ipv6_primary, MAX_ADDR_LEN, trim(value));
                        StringCchCopyW(g_config.dns_ipv6_secondary, MAX_ADDR_LEN, trim(comma + 1));
                    } else {
                        StringCchCopyW(g_config.dns_ipv6_primary, MAX_ADDR_LEN, trim(value));
                    }
                }
            }
            else if (_wcsicmp(section, L"doh") == 0) {
                if (_wcsicmp(key, L"template") == 0) {
                    StringCchCopyW(g_config.doh_template, 256, value);
                }
                else if (_wcsicmp(key, L"autoupgrade") == 0) {
                    g_config.doh_autoupgrade = (_wcsicmp(value, L"yes") == 0 || _wcsicmp(value, L"true") == 0 || _wcsicmp(value, L"1") == 0);
                }
                else if (_wcsicmp(key, L"fallback") == 0) {
                    g_config.doh_fallback = (_wcsicmp(value, L"yes") == 0 || _wcsicmp(value, L"true") == 0 || _wcsicmp(value, L"1") == 0);
                }
            }
        }
    }

    fclose(fp);
    return 0;
}

/* ============================================================================
 * COMMAND LINE PARSING
 * ============================================================================ */

RunMode config_parse_args(int argc, wchar_t *argv[], wchar_t *config_file)
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
        if (_wcsicmp(arg, L"custom") == 0) {
            mode = MODE_CUSTOM;
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
 * HELP
 * ============================================================================ */

void config_print_help(void)
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
    wprintf(L"    custom        Configure DNS with custom servers from config file\n");
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

/* ============================================================================
 * DEFAULTS
 * ============================================================================ */

void config_set_defaults(void)
{
    if (g_config.ipv4_mask[0] == L'\0' && g_config.has_ipv4) {
        StringCchCopyW(g_config.ipv4_mask, MAX_ADDR_LEN, L"255.255.255.0");
    }
    if (g_config.ipv6_prefix[0] == L'\0' && g_config.has_ipv6) {
        StringCchCopyW(g_config.ipv6_prefix, 16, L"64");
    }
    /* DoH defaults: autoupgrade=yes, fallback=no */
    if (g_config.has_custom_dns && g_config.doh_template[0] != L'\0') {
        /* doh_autoupgrade defaults to 1 if not explicitly set to 0 */
        /* Config parsing already handles this */
    }
}
