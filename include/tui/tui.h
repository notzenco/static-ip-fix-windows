/*
 * tui.h - Text User Interface for static-ip-fix
 *
 * Provides an interactive console interface with:
 * - Interface selector (arrow-key navigation)
 * - Live status dashboard (auto-refresh)
 * - Configuration wizard (step-by-step)
 * - Colorful progress indicators
 */

#ifndef TUI_H
#define TUI_H

#include "../utils.h"

/* ============================================================================
 * TUI STATE
 * ============================================================================ */

typedef enum {
    TUI_SCREEN_MAIN_MENU,
    TUI_SCREEN_INTERFACE_SELECT,
    TUI_SCREEN_PROVIDER_SELECT,
    TUI_SCREEN_CONFIGURE,
    TUI_SCREEN_STATUS,
    TUI_SCREEN_EXIT
} TuiScreen;

/* ============================================================================
 * PUBLIC API
 * ============================================================================ */

/*
 * Check if TUI mode is supported on this system
 * Returns 1 if supported, 0 otherwise
 */
int tui_is_supported(void);

/*
 * Initialize TUI subsystem
 * Returns 1 on success, 0 on failure
 */
int tui_init(void);

/*
 * Run the TUI main loop
 * Returns exit code (0 = success)
 */
int tui_run(void);

/*
 * Cleanup TUI and restore console state
 */
void tui_cleanup(void);

/* ============================================================================
 * SCREEN FUNCTIONS (called by tui_run)
 * ============================================================================ */

/*
 * Show main menu and return selected screen
 */
TuiScreen tui_screen_main_menu(void);

/*
 * Interface selector - returns 1 if interface was selected
 * Selected interface is stored in g_config.interface_name
 */
int tui_screen_interface_select(void);

/*
 * DNS provider selector - returns 1 if provider was selected
 * Returns provider index: 0=Cloudflare, 1=Google, 2=Custom, -1=Cancel
 */
int tui_screen_provider_select(void);

/*
 * Configuration screen - applies selected configuration
 * Returns 1 on success, 0 on failure
 */
int tui_screen_configure(int provider_index);

/*
 * Status dashboard - shows live DNS status
 * Returns when user presses Esc or Q
 */
void tui_screen_status(void);

#endif /* TUI_H */
