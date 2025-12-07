/*
 * ui.c - Reusable TUI components
 */

#include <string.h>
#include "tui/ui.h"

/* ============================================================================
 * SPINNER FRAMES
 * ============================================================================ */

static const wchar_t *SPINNER_FRAMES[] = {L"|", L"/", L"-", L"\\"};
#define SPINNER_FRAME_COUNT 4

/* ============================================================================
 * BOX DRAWING CHARACTERS (ASCII for compatibility)
 * ============================================================================ */

#define BOX_TL L"+"
#define BOX_TR L"+"
#define BOX_BL L"+"
#define BOX_BR L"+"
#define BOX_H  L"-"
#define BOX_V  L"|"

/* ============================================================================
 * LIST COMPONENT
 * ============================================================================ */

void ui_list_init(UIList *list, const wchar_t *title, int visible_rows)
{
    memset(list, 0, sizeof(UIList));
    StringCchCopyW(list->title, 64, title);
    list->visible_rows = visible_rows;
}

void ui_list_add(UIList *list, const wchar_t *item)
{
    if (list->item_count >= UI_LIST_MAX_ITEMS) {
        return;
    }
    StringCchCopyW(list->items[list->item_count], UI_LIST_ITEM_LEN, item);
    list->item_count++;
}

void ui_list_clear(UIList *list)
{
    list->item_count = 0;
    list->selected = 0;
    list->scroll_offset = 0;
}

void ui_list_move_up(UIList *list)
{
    if (list->selected > 0) {
        list->selected--;
        if (list->selected < list->scroll_offset) {
            list->scroll_offset = list->selected;
        }
    }
}

void ui_list_move_down(UIList *list)
{
    if (list->selected < list->item_count - 1) {
        list->selected++;
        if (list->selected >= list->scroll_offset + list->visible_rows) {
            list->scroll_offset = list->selected - list->visible_rows + 1;
        }
    }
}

int ui_list_get_selected(const UIList *list)
{
    return list->selected;
}

const wchar_t *ui_list_get_selected_text(const UIList *list)
{
    if (list->item_count == 0 || list->selected < 0 ||
        list->selected >= list->item_count) {
        return NULL;
    }
    return list->items[list->selected];
}

void ui_list_render(const UIList *list, int row, int col)
{
    int end_idx;
    int display_row;

    /* Render title */
    console_cursor_move(row, col);
    console_set_bold();
    console_set_fg(COLOR_CYAN);
    wprintf(L"%ls", list->title);
    console_reset_style();

    /* Calculate visible range */
    end_idx = list->scroll_offset + list->visible_rows;
    if (end_idx > list->item_count) {
        end_idx = list->item_count;
    }

    /* Render items */
    for (int i = list->scroll_offset; i < end_idx; i++) {
        display_row = row + 1 + (i - list->scroll_offset);
        console_cursor_move(display_row, col);
        console_clear_line();

        if (i == list->selected) {
            console_set_fg(COLOR_BLACK);
            console_set_bg(COLOR_CYAN);
            wprintf(L" > %ls ", list->items[i]);
            console_reset_style();
        } else {
            console_set_fg(COLOR_WHITE);
            wprintf(L"   %ls ", list->items[i]);
            console_reset_style();
        }
    }

    /* Clear remaining lines */
    for (int i = end_idx; i < list->scroll_offset + list->visible_rows; i++) {
        display_row = row + 1 + (i - list->scroll_offset);
        console_cursor_move(display_row, col);
        console_clear_line();
    }

    /* Show scroll indicators if needed */
    if (list->scroll_offset > 0) {
        console_cursor_move(row + 1, col + 40);
        console_set_fg(COLOR_BRIGHT_BLACK);
        wprintf(L"^ more");
        console_reset_style();
    }
    if (end_idx < list->item_count) {
        console_cursor_move(row + list->visible_rows, col + 40);
        console_set_fg(COLOR_BRIGHT_BLACK);
        wprintf(L"v more");
        console_reset_style();
    }
}

/* ============================================================================
 * SPINNER COMPONENT
 * ============================================================================ */

void ui_spinner_init(UISpinner *spinner, const wchar_t *text)
{
    spinner->frame = 0;
    StringCchCopyW(spinner->text, 128, text);
}

void ui_spinner_update(UISpinner *spinner)
{
    spinner->frame = (spinner->frame + 1) % SPINNER_FRAME_COUNT;
}

void ui_spinner_render(const UISpinner *spinner, int row, int col)
{
    console_cursor_move(row, col);
    console_set_fg(COLOR_CYAN);
    wprintf(L"%ls ", SPINNER_FRAMES[spinner->frame]);
    console_set_fg(COLOR_WHITE);
    wprintf(L"%ls", spinner->text);
    console_reset_style();
    fflush(stdout);
}

/* ============================================================================
 * PROGRESS BAR COMPONENT
 * ============================================================================ */

void ui_progress_init(UIProgress *bar, int total, int width, const wchar_t *label)
{
    bar->total = total;
    bar->current = 0;
    bar->width = width;
    StringCchCopyW(bar->label, 64, label);
}

void ui_progress_update(UIProgress *bar, int current)
{
    bar->current = current;
    if (bar->current > bar->total) {
        bar->current = bar->total;
    }
}

void ui_progress_render(const UIProgress *bar, int row, int col)
{
    int filled;
    int percent;

    console_cursor_move(row, col);

    /* Label */
    console_set_fg(COLOR_WHITE);
    wprintf(L"%ls ", bar->label);

    /* Calculate fill */
    percent = (bar->total > 0) ? (bar->current * 100 / bar->total) : 0;
    filled = (bar->total > 0) ? (bar->current * bar->width / bar->total) : 0;

    /* Draw bar */
    wprintf(L"[");
    console_set_fg(COLOR_GREEN);
    for (int i = 0; i < filled; i++) {
        wprintf(L"=");
    }
    console_set_fg(COLOR_BRIGHT_BLACK);
    for (int i = filled; i < bar->width; i++) {
        wprintf(L"-");
    }
    console_reset_style();
    wprintf(L"] %3d%%", percent);

    fflush(stdout);
}

/* ============================================================================
 * PANEL COMPONENT
 * ============================================================================ */

void ui_panel_render(int row, int col, int width, int height, const wchar_t *title)
{
    int title_len = (int)wcslen(title);
    int padding = (width - title_len - 4) / 2;

    /* Top border */
    console_cursor_move(row, col);
    console_set_fg(COLOR_CYAN);
    wprintf(L"%ls", BOX_TL);
    for (int i = 0; i < padding; i++) wprintf(L"%ls", BOX_H);
    console_set_bold();
    wprintf(L" %ls ", title);
    console_reset_style();
    console_set_fg(COLOR_CYAN);
    for (int i = 0; i < width - padding - title_len - 4; i++) wprintf(L"%ls", BOX_H);
    wprintf(L"%ls", BOX_TR);

    /* Side borders */
    for (int i = 1; i < height - 1; i++) {
        console_cursor_move(row + i, col);
        wprintf(L"%ls", BOX_V);
        console_cursor_move(row + i, col + width - 1);
        wprintf(L"%ls", BOX_V);
    }

    /* Bottom border */
    console_cursor_move(row + height - 1, col);
    wprintf(L"%ls", BOX_BL);
    for (int i = 0; i < width - 2; i++) wprintf(L"%ls", BOX_H);
    wprintf(L"%ls", BOX_BR);

    console_reset_style();
    fflush(stdout);
}

/* ============================================================================
 * STATUS BADGE
 * ============================================================================ */

void ui_badge_render(UIBadgeType type, const wchar_t *text, int row, int col)
{
    const wchar_t *symbol;
    ConsoleColor color;

    switch (type) {
        case BADGE_SUCCESS:
            symbol = L"[OK]";
            color = COLOR_GREEN;
            break;
        case BADGE_ERROR:
            symbol = L"[X]";
            color = COLOR_RED;
            break;
        case BADGE_WARNING:
            symbol = L"[!]";
            color = COLOR_YELLOW;
            break;
        case BADGE_INFO:
            symbol = L"[i]";
            color = COLOR_CYAN;
            break;
        case BADGE_PENDING:
        default:
            symbol = L"[...]";
            color = COLOR_BRIGHT_BLACK;
            break;
    }

    console_cursor_move(row, col);
    console_set_fg(color);
    console_set_bold();
    wprintf(L"%ls", symbol);
    console_reset_style();
    wprintf(L" %ls", text);
    fflush(stdout);
}

/* ============================================================================
 * MENU (High-level list with navigation)
 * ============================================================================ */

int ui_menu_select(const wchar_t *title, const wchar_t **items, int item_count)
{
    UIList list;
    KeyEvent key;
    ConsoleSize size;

    ui_list_init(&list, title, item_count > 10 ? 10 : item_count);

    for (int i = 0; i < item_count; i++) {
        ui_list_add(&list, items[i]);
    }

    console_get_size(&size);

    while (1) {
        ui_list_render(&list, 2, 2);

        /* Show hint */
        console_cursor_move(list.visible_rows + 4, 2);
        console_set_fg(COLOR_BRIGHT_BLACK);
        wprintf(L"Use arrow keys to navigate, Enter to select, Esc to cancel");
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
            case KEY_ENTER:
                return ui_list_get_selected(&list);
            case KEY_ESC:
                return -1;
            default:
                break;
        }
    }
}

/* ============================================================================
 * CONFIRMATION DIALOG
 * ============================================================================ */

int ui_confirm(const wchar_t *message)
{
    KeyEvent key;

    console_cursor_move(10, 2);
    console_set_fg(COLOR_YELLOW);
    wprintf(L"%ls", message);
    console_reset_style();

    console_cursor_move(12, 2);
    wprintf(L"Press ");
    console_set_fg(COLOR_GREEN);
    console_set_bold();
    wprintf(L"Y");
    console_reset_style();
    wprintf(L" for Yes, ");
    console_set_fg(COLOR_RED);
    console_set_bold();
    wprintf(L"N");
    console_reset_style();
    wprintf(L" for No: ");
    fflush(stdout);

    while (1) {
        key = console_read_key();

        if (key.type == KEY_CHAR) {
            if (key.ch == L'y' || key.ch == L'Y') {
                return 1;
            }
            if (key.ch == L'n' || key.ch == L'N') {
                return 0;
            }
        }
        if (key.type == KEY_ENTER) {
            return 1;  /* Default to yes */
        }
        if (key.type == KEY_ESC) {
            return 0;
        }
    }
}

/* ============================================================================
 * MESSAGE BOX
 * ============================================================================ */

void ui_message(const wchar_t *title, const wchar_t *message, UIBadgeType type)
{
    ConsoleSize size;
    int width = 60;
    int height = 7;

    console_get_size(&size);
    int row = (size.rows - height) / 2;
    int col = (size.cols - width) / 2;

    ui_panel_render(row, col, width, height, title);

    /* Message with badge */
    ui_badge_render(type, message, row + 2, col + 3);

    /* Press any key hint */
    console_cursor_move(row + height - 2, col + 3);
    console_set_fg(COLOR_BRIGHT_BLACK);
    wprintf(L"Press any key to continue...");
    console_reset_style();
    fflush(stdout);

    /* Wait for key */
    console_flush_input();
    console_read_key();
}
