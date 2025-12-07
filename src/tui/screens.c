/*
 * screens.c - TUI screen implementations
 *
 * Contains: Interface selector, Provider selector, Configuration, Status dashboard
 */

/* Must include winsock2.h before windows.h */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include "tui/tui.h"
#include "tui/ui.h"
#include "config.h"
#include "dns.h"
#include "network.h"
#include "status.h"

/* ============================================================================
 * INTERFACE ENUMERATION (TUI-specific)
 * ============================================================================ */

typedef struct {
    wchar_t name[MAX_IFACE_LEN];
    wchar_t type[32];
    wchar_t ipv4[MAX_ADDR_LEN];
    wchar_t ipv6[MAX_ADDR_LEN];
} InterfaceInfo;

static int get_interfaces(InterfaceInfo *interfaces, int max_count)
{
    ULONG bufLen = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    PIP_ADAPTER_ADDRESSES pCurrAddr = NULL;
    ULONG ret;
    int count = 0;

    pAddresses = (IP_ADAPTER_ADDRESSES *)HeapAlloc(GetProcessHeap(), 0, bufLen);
    if (!pAddresses) {
        return 0;
    }

    ret = GetAdaptersAddresses(AF_UNSPEC,
        GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST,
        NULL, pAddresses, &bufLen);

    if (ret == ERROR_BUFFER_OVERFLOW) {
        HeapFree(GetProcessHeap(), 0, pAddresses);
        pAddresses = (IP_ADAPTER_ADDRESSES *)HeapAlloc(GetProcessHeap(), 0, bufLen);
        if (!pAddresses) {
            return 0;
        }
        ret = GetAdaptersAddresses(AF_UNSPEC,
            GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST,
            NULL, pAddresses, &bufLen);
    }

    if (ret != NO_ERROR) {
        HeapFree(GetProcessHeap(), 0, pAddresses);
        return 0;
    }

    pCurrAddr = pAddresses;
    while (pCurrAddr && count < max_count) {
        if (pCurrAddr->IfType != IF_TYPE_SOFTWARE_LOOPBACK &&
            pCurrAddr->IfType != IF_TYPE_TUNNEL &&
            pCurrAddr->OperStatus == IfOperStatusUp) {

            StringCchCopyW(interfaces[count].name, MAX_IFACE_LEN,
                          pCurrAddr->FriendlyName);

            switch (pCurrAddr->IfType) {
                case IF_TYPE_ETHERNET_CSMACD:
                    StringCchCopyW(interfaces[count].type, 32, L"Ethernet");
                    break;
                case IF_TYPE_IEEE80211:
                    StringCchCopyW(interfaces[count].type, 32, L"Wi-Fi");
                    break;
                default:
                    StringCchCopyW(interfaces[count].type, 32, L"Other");
                    break;
            }

            interfaces[count].ipv4[0] = L'\0';
            interfaces[count].ipv6[0] = L'\0';

            PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddr->FirstUnicastAddress;
            while (pUnicast) {
                char ipStr[64];
                if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                    struct sockaddr_in *sa = (struct sockaddr_in *)pUnicast->Address.lpSockaddr;
                    inet_ntop(AF_INET, &sa->sin_addr, ipStr, sizeof(ipStr));
                    MultiByteToWideChar(CP_ACP, 0, ipStr, -1,
                                       interfaces[count].ipv4, MAX_ADDR_LEN);
                } else if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6) {
                    struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)pUnicast->Address.lpSockaddr;
                    inet_ntop(AF_INET6, &sa6->sin6_addr, ipStr, sizeof(ipStr));
                    if (strncmp(ipStr, "fe80:", 5) != 0 &&
                        interfaces[count].ipv6[0] == L'\0') {
                        MultiByteToWideChar(CP_ACP, 0, ipStr, -1,
                                           interfaces[count].ipv6, MAX_ADDR_LEN);
                    }
                }
                pUnicast = pUnicast->Next;
            }

            count++;
        }
        pCurrAddr = pCurrAddr->Next;
    }

    HeapFree(GetProcessHeap(), 0, pAddresses);
    return count;
}

/* ============================================================================
 * MAIN MENU SCREEN
 * ============================================================================ */

TuiScreen tui_screen_main_menu(void)
{
    const wchar_t *items[] = {
        L"Configure DNS (with DoH encryption)",
        L"View DNS Status",
        L"List Network Interfaces",
        L"Exit"
    };
    int choice;

    console_clear();

    /* Header */
    console_cursor_move(0, 0);
    console_set_fg(COLOR_CYAN);
    console_set_bold();
    wprintf(L"  static-ip-fix - DNS Configuration Tool\n");
    console_reset_style();
    console_set_fg(COLOR_BRIGHT_BLACK);
    wprintf(L"  Configure static IP and DNS-over-HTTPS on Windows\n");
    console_reset_style();

    choice = ui_menu_select(L"Main Menu", items, 4);

    switch (choice) {
        case 0: return TUI_SCREEN_INTERFACE_SELECT;
        case 1: return TUI_SCREEN_STATUS;
        case 2:
            /* Show interface list */
            console_clear();
            console_cursor_show();
            network_list_interfaces();
            console_cursor_move(20, 0);
            console_set_fg(COLOR_BRIGHT_BLACK);
            wprintf(L"\nPress any key to continue...");
            console_reset_style();
            console_flush_input();
            console_read_key();
            console_cursor_hide();
            return TUI_SCREEN_MAIN_MENU;
        case 3:
        case -1:
        default:
            return TUI_SCREEN_EXIT;
    }
}

/* ============================================================================
 * INTERFACE SELECT SCREEN
 * ============================================================================ */

int tui_screen_interface_select(void)
{
    InterfaceInfo interfaces[16];
    int iface_count;
    UIList list;
    KeyEvent key;
    wchar_t item_text[512];

    console_clear();

    /* Get interfaces */
    iface_count = get_interfaces(interfaces, 16);

    if (iface_count == 0) {
        ui_message(L"Error", L"No active network interfaces found", BADGE_ERROR);
        return 0;
    }

    /* Build list */
    ui_list_init(&list, L"Select Network Interface", iface_count > 8 ? 8 : iface_count);

    for (int i = 0; i < iface_count; i++) {
        StringCchPrintfW(item_text, 512, L"%ls (%ls) - %ls",
                        interfaces[i].name,
                        interfaces[i].type,
                        interfaces[i].ipv4[0] ? interfaces[i].ipv4 : L"No IPv4");
        ui_list_add(&list, item_text);
    }

    /* Header */
    console_cursor_move(0, 0);
    console_set_fg(COLOR_CYAN);
    console_set_bold();
    wprintf(L"  Select Network Interface\n");
    console_reset_style();

    while (1) {
        ui_list_render(&list, 2, 2);

        /* Show hint */
        console_cursor_move(list.visible_rows + 5, 2);
        console_set_fg(COLOR_BRIGHT_BLACK);
        wprintf(L"Arrow keys: navigate | Enter: select | Esc: cancel");
        console_reset_style();
        fflush(stdout);

        key = console_read_key();

        switch (key.type) {
            case KEY_UP:
                ui_list_move_up(&list);
                break;
            case KEY_DOWN:
                ui_list_move_down(&list);
                break;
            case KEY_ENTER: {
                int selected = ui_list_get_selected(&list);
                StringCchCopyW(g_config.interface_name, MAX_IFACE_LEN,
                              interfaces[selected].name);
                return 1;
            }
            case KEY_ESC:
                return 0;
            default:
                break;
        }
    }
}

/* ============================================================================
 * PROVIDER SELECT SCREEN
 * ============================================================================ */

int tui_screen_provider_select(void)
{
    const wchar_t *items[] = {
        L"Cloudflare (1.1.1.1) - Fast, privacy-focused",
        L"Google (8.8.8.8) - Reliable, widely used",
        L"Cancel"
    };
    int choice;

    console_clear();

    /* Header */
    console_cursor_move(0, 0);
    console_set_fg(COLOR_CYAN);
    console_set_bold();
    wprintf(L"  Select DNS Provider\n");
    console_reset_style();
    console_set_fg(COLOR_BRIGHT_BLACK);
    wprintf(L"  Interface: %ls\n", g_config.interface_name);
    console_reset_style();

    choice = ui_menu_select(L"DNS Provider", items, 3);

    if (choice == 2 || choice == -1) {
        return -1;  /* Cancel */
    }

    return choice;  /* 0 = Cloudflare, 1 = Google */
}

/* ============================================================================
 * CONFIGURATION SCREEN
 * ============================================================================ */

int tui_screen_configure(int provider_index)
{
    const DnsProvider *provider;
    UISpinner spinner;
    int result;
    wchar_t msg[256];

    console_clear();

    /* Select provider */
    if (provider_index == 0) {
        provider = &DNS_CLOUDFLARE;
    } else {
        provider = &DNS_GOOGLE;
    }

    /* Header */
    console_cursor_move(0, 0);
    console_set_fg(COLOR_CYAN);
    console_set_bold();
    wprintf(L"  Configuring DNS\n");
    console_reset_style();

    /* Show configuration details */
    console_cursor_move(2, 2);
    console_set_fg(COLOR_WHITE);
    wprintf(L"Interface: ");
    console_set_fg(COLOR_YELLOW);
    wprintf(L"%ls\n", g_config.interface_name);

    console_cursor_move(3, 2);
    console_set_fg(COLOR_WHITE);
    wprintf(L"Provider:  ");
    console_set_fg(COLOR_YELLOW);
    wprintf(L"%ls\n", provider->name);

    console_cursor_move(4, 2);
    console_set_fg(COLOR_WHITE);
    wprintf(L"DNS IPv4:  ");
    console_set_fg(COLOR_BRIGHT_BLACK);
    wprintf(L"%ls, %ls\n", provider->ipv4_primary, provider->ipv4_secondary);

    console_cursor_move(5, 2);
    console_set_fg(COLOR_WHITE);
    wprintf(L"DNS IPv6:  ");
    console_set_fg(COLOR_BRIGHT_BLACK);
    wprintf(L"%ls\n", provider->ipv6_primary);

    console_cursor_move(6, 2);
    console_set_fg(COLOR_WHITE);
    wprintf(L"DoH URL:   ");
    console_set_fg(COLOR_BRIGHT_BLACK);
    wprintf(L"%ls\n", provider->doh_template);

    console_reset_style();

    /* Confirmation */
    console_cursor_move(8, 2);
    if (!ui_confirm(L"Apply this configuration?")) {
        return 0;
    }

    /* Clear and show progress */
    console_clear();
    console_cursor_move(0, 0);
    console_set_fg(COLOR_CYAN);
    console_set_bold();
    wprintf(L"  Applying Configuration...\n");
    console_reset_style();

    /* Show spinner */
    ui_spinner_init(&spinner, L"Configuring DNS servers and DoH encryption...");

    /* Set DNS-only mode for TUI */
    g_config.dns_only = 1;

    /* Run configuration with spinner animation */
    console_cursor_show();  /* Show cursor during netsh output */

    /* Start spinner in background (simple version - just show message) */
    console_cursor_move(3, 2);
    ui_spinner_render(&spinner, 3, 2);
    fflush(stdout);

    /* Run the actual configuration */
    result = dns_run_provider(provider);

    console_cursor_hide();

    /* Show result */
    console_cursor_move(12, 2);

    if (result == 0) {
        StringCchPrintfW(msg, 256, L"DNS configured with %ls + DoH encryption",
                        provider->name);
        ui_badge_render(BADGE_SUCCESS, msg, 12, 2);

        console_cursor_move(14, 2);
        console_set_fg(COLOR_GREEN);
        wprintf(L"Your DNS queries are now encrypted!");
        console_reset_style();
    } else {
        ui_badge_render(BADGE_ERROR, L"Configuration failed - changes rolled back",
                       12, 2);
    }

    console_cursor_move(16, 2);
    console_set_fg(COLOR_BRIGHT_BLACK);
    wprintf(L"Press any key to continue...");
    console_reset_style();
    fflush(stdout);

    console_flush_input();
    console_read_key();

    return (result == 0) ? 1 : 0;
}

/* ============================================================================
 * STATUS SCREEN
 * ============================================================================ */

void tui_screen_status(void)
{
    KeyEvent key;
    int refresh_interval = 5000;  /* 5 seconds */

    /* First, we need to select an interface */
    if (!tui_screen_interface_select()) {
        return;
    }

    while (1) {
        console_clear();

        /* Header */
        console_cursor_move(0, 0);
        console_set_fg(COLOR_CYAN);
        console_set_bold();
        wprintf(L"  DNS Status Dashboard\n");
        console_reset_style();
        console_set_fg(COLOR_BRIGHT_BLACK);
        wprintf(L"  Interface: %ls | Auto-refresh every 5s | Press Q or Esc to exit\n",
               g_config.interface_name);
        console_reset_style();

        wprintf(L"\n");

        /* Show cursor and run status check */
        console_cursor_show();

        /* Call the existing status function */
        status_run();

        console_cursor_hide();

        /* Wait for key or timeout */
        key = console_read_key_timeout(refresh_interval);

        if (key.type == KEY_ESC) {
            break;
        }
        if (key.type == KEY_CHAR && (key.ch == L'q' || key.ch == L'Q')) {
            break;
        }
    }
}
