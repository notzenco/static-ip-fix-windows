/*
 * ui.h - Reusable TUI components
 *
 * Provides list selectors, spinners, progress bars, panels, and badges
 * for building interactive console interfaces.
 */

#ifndef TUI_UI_H
#define TUI_UI_H

#include "console.h"

/* ============================================================================
 * LIST COMPONENT
 * ============================================================================ */

#define UI_LIST_MAX_ITEMS 32
#define UI_LIST_ITEM_LEN 128

typedef struct {
    wchar_t items[UI_LIST_MAX_ITEMS][UI_LIST_ITEM_LEN];
    int item_count;
    int selected;
    int scroll_offset;
    int visible_rows;
    wchar_t title[64];
} UIList;

void ui_list_init(UIList *list, const wchar_t *title, int visible_rows);
void ui_list_add(UIList *list, const wchar_t *item);
void ui_list_clear(UIList *list);
void ui_list_move_up(UIList *list);
void ui_list_move_down(UIList *list);
int ui_list_get_selected(const UIList *list);
const wchar_t *ui_list_get_selected_text(const UIList *list);
void ui_list_render(const UIList *list, int row, int col);

/* ============================================================================
 * SPINNER COMPONENT
 * ============================================================================ */

typedef struct {
    int frame;
    wchar_t text[128];
} UISpinner;

void ui_spinner_init(UISpinner *spinner, const wchar_t *text);
void ui_spinner_update(UISpinner *spinner);
void ui_spinner_render(const UISpinner *spinner, int row, int col);

/* ============================================================================
 * PROGRESS BAR COMPONENT
 * ============================================================================ */

typedef struct {
    int total;
    int current;
    int width;
    wchar_t label[64];
} UIProgress;

void ui_progress_init(UIProgress *bar, int total, int width, const wchar_t *label);
void ui_progress_update(UIProgress *bar, int current);
void ui_progress_render(const UIProgress *bar, int row, int col);

/* ============================================================================
 * PANEL COMPONENT (Box with title)
 * ============================================================================ */

void ui_panel_render(int row, int col, int width, int height, const wchar_t *title);

/* ============================================================================
 * STATUS BADGE
 * ============================================================================ */

typedef enum {
    BADGE_SUCCESS,
    BADGE_ERROR,
    BADGE_WARNING,
    BADGE_INFO,
    BADGE_PENDING
} UIBadgeType;

void ui_badge_render(UIBadgeType type, const wchar_t *text, int row, int col);

/* ============================================================================
 * MENU (High-level list with navigation)
 * ============================================================================ */

/*
 * Display a menu and wait for selection
 * Returns selected index (0-based) or -1 if cancelled
 */
int ui_menu_select(const wchar_t *title, const wchar_t **items, int item_count);

/* ============================================================================
 * CONFIRMATION DIALOG
 * ============================================================================ */

/*
 * Show a yes/no confirmation dialog
 * Returns 1 for yes, 0 for no
 */
int ui_confirm(const wchar_t *message);

/* ============================================================================
 * MESSAGE BOX
 * ============================================================================ */

/*
 * Show a message and wait for any key
 */
void ui_message(const wchar_t *title, const wchar_t *message, UIBadgeType type);

#endif /* TUI_UI_H */
