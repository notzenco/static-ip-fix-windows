/*
 * tui.c - TUI main orchestrator
 *
 * Initializes the TUI subsystem and manages the main loop.
 */

#include "tui/tui.h"
#include "tui/console.h"
#include "tui/ui.h"

/* ============================================================================
 * TUI STATE
 * ============================================================================ */

static int g_tui_initialized = 0;

/* ============================================================================
 * PUBLIC API
 * ============================================================================ */

int tui_is_supported(void)
{
    return console_is_vt100_supported();
}

int tui_init(void)
{
    if (g_tui_initialized) {
        return 1;
    }

    if (!console_init()) {
        return 0;
    }

    g_tui_initialized = 1;
    return 1;
}

void tui_cleanup(void)
{
    if (!g_tui_initialized) {
        return;
    }

    console_clear();
    console_cursor_move(0, 0);
    console_restore();

    g_tui_initialized = 0;
}

int tui_run(void)
{
    TuiScreen current_screen = TUI_SCREEN_MAIN_MENU;
    int provider_index = -1;
    int result = 0;

    if (!g_tui_initialized) {
        if (!tui_init()) {
            print_error(L"Failed to initialize TUI mode");
            return 1;
        }
    }

    /* Main loop */
    while (current_screen != TUI_SCREEN_EXIT) {
        switch (current_screen) {
            case TUI_SCREEN_MAIN_MENU:
                current_screen = tui_screen_main_menu();
                break;

            case TUI_SCREEN_INTERFACE_SELECT:
                if (tui_screen_interface_select()) {
                    current_screen = TUI_SCREEN_PROVIDER_SELECT;
                } else {
                    current_screen = TUI_SCREEN_MAIN_MENU;
                }
                break;

            case TUI_SCREEN_PROVIDER_SELECT:
                provider_index = tui_screen_provider_select();
                if (provider_index >= 0) {
                    current_screen = TUI_SCREEN_CONFIGURE;
                } else {
                    current_screen = TUI_SCREEN_MAIN_MENU;
                }
                break;

            case TUI_SCREEN_CONFIGURE:
                result = tui_screen_configure(provider_index);
                (void)result;  /* Result shown in screen */
                current_screen = TUI_SCREEN_MAIN_MENU;
                break;

            case TUI_SCREEN_STATUS:
                tui_screen_status();
                current_screen = TUI_SCREEN_MAIN_MENU;
                break;

            case TUI_SCREEN_EXIT:
            default:
                break;
        }
    }

    tui_cleanup();

    /* Show goodbye message */
    wprintf(L"Thank you for using static-ip-fix!\n");

    return 0;
}
