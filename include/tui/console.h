/*
 * console.h - Windows Console API abstraction for TUI
 *
 * Provides VT100/ANSI escape sequence support, colors, cursor control,
 * and keyboard input handling for Windows 10+.
 */

#ifndef TUI_CONSOLE_H
#define TUI_CONSOLE_H

#include "../utils.h"

/* ============================================================================
 * ANSI COLOR CODES
 * ============================================================================ */

typedef enum {
    COLOR_BLACK = 0,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_YELLOW,
    COLOR_BLUE,
    COLOR_MAGENTA,
    COLOR_CYAN,
    COLOR_WHITE,
    COLOR_BRIGHT_BLACK,
    COLOR_BRIGHT_RED,
    COLOR_BRIGHT_GREEN,
    COLOR_BRIGHT_YELLOW,
    COLOR_BRIGHT_BLUE,
    COLOR_BRIGHT_MAGENTA,
    COLOR_BRIGHT_CYAN,
    COLOR_BRIGHT_WHITE,
    COLOR_RESET = 99
} ConsoleColor;

/* ============================================================================
 * KEY CODES
 * ============================================================================ */

typedef enum {
    KEY_NONE = 0,
    KEY_UP = 256,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_ENTER,
    KEY_ESC,
    KEY_TAB,
    KEY_BACKSPACE,
    KEY_SPACE,
    KEY_CHAR    /* Regular character - check key_char field */
} KeyType;

typedef struct {
    KeyType type;
    wchar_t ch;     /* Character value for KEY_CHAR */
} KeyEvent;

/* ============================================================================
 * CONSOLE SIZE
 * ============================================================================ */

typedef struct {
    int rows;
    int cols;
} ConsoleSize;

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

/*
 * Initialize console for TUI mode
 * Enables VT100 processing, saves original state
 * Returns 1 on success, 0 on failure
 */
int console_init(void);

/*
 * Restore console to original state
 */
void console_restore(void);

/*
 * Check if VT100 is supported
 */
int console_is_vt100_supported(void);

/* ============================================================================
 * CURSOR CONTROL
 * ============================================================================ */

void console_cursor_hide(void);
void console_cursor_show(void);
void console_cursor_move(int row, int col);
void console_cursor_save(void);
void console_cursor_restore_pos(void);

/* ============================================================================
 * SCREEN OPERATIONS
 * ============================================================================ */

void console_clear(void);
void console_clear_line(void);
void console_get_size(ConsoleSize *size);

/* ============================================================================
 * COLOR AND STYLE
 * ============================================================================ */

void console_set_fg(ConsoleColor color);
void console_set_bg(ConsoleColor color);
void console_set_bold(void);
void console_set_dim(void);
void console_set_reverse(void);
void console_reset_style(void);

/* ============================================================================
 * INPUT HANDLING
 * ============================================================================ */

/*
 * Check if input is available (non-blocking)
 */
int console_input_available(void);

/*
 * Read a key event (blocking)
 */
KeyEvent console_read_key(void);

/*
 * Read a key event with timeout (milliseconds)
 * Returns KEY_NONE if timeout expires
 */
KeyEvent console_read_key_timeout(int timeout_ms);

/*
 * Flush input buffer
 */
void console_flush_input(void);

#endif /* TUI_CONSOLE_H */
