/*
 * console.c - Windows Console API abstraction for TUI
 */

#include "tui/console.h"
#include <conio.h>

/* ============================================================================
 * CONSOLE STATE
 * ============================================================================ */

static struct {
    HANDLE hStdout;
    HANDLE hStdin;
    DWORD original_stdout_mode;
    DWORD original_stdin_mode;
    int initialized;
    int vt100_enabled;
} g_console = {0};

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

int console_init(void)
{
    DWORD stdout_mode, stdin_mode;

    if (g_console.initialized) {
        return 1;
    }

    g_console.hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    g_console.hStdin = GetStdHandle(STD_INPUT_HANDLE);

    if (g_console.hStdout == INVALID_HANDLE_VALUE ||
        g_console.hStdin == INVALID_HANDLE_VALUE) {
        return 0;
    }

    /* Save original modes */
    if (!GetConsoleMode(g_console.hStdout, &g_console.original_stdout_mode) ||
        !GetConsoleMode(g_console.hStdin, &g_console.original_stdin_mode)) {
        return 0;
    }

    /* Enable VT100 processing for colors and cursor control */
    stdout_mode = g_console.original_stdout_mode;
    stdout_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    stdout_mode |= DISABLE_NEWLINE_AUTO_RETURN;

    if (!SetConsoleMode(g_console.hStdout, stdout_mode)) {
        /* VT100 not supported (Windows < 10 1511) */
        g_console.vt100_enabled = 0;
        return 0;
    }

    g_console.vt100_enabled = 1;

    /* Configure input for raw key events */
    stdin_mode = ENABLE_WINDOW_INPUT;
    SetConsoleMode(g_console.hStdin, stdin_mode);

    g_console.initialized = 1;

    /* Hide cursor and clear screen */
    console_cursor_hide();
    console_clear();

    return 1;
}

void console_restore(void)
{
    if (!g_console.initialized) {
        return;
    }

    /* Show cursor */
    console_cursor_show();

    /* Reset colors */
    console_reset_style();

    /* Restore original console modes */
    SetConsoleMode(g_console.hStdout, g_console.original_stdout_mode);
    SetConsoleMode(g_console.hStdin, g_console.original_stdin_mode);

    g_console.initialized = 0;
}

int console_is_vt100_supported(void)
{
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;

    if (hStdout == INVALID_HANDLE_VALUE) {
        return 0;
    }

    if (!GetConsoleMode(hStdout, &mode)) {
        return 0;
    }

    /* Try to enable VT100, then restore */
    if (!SetConsoleMode(hStdout, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        return 0;
    }

    SetConsoleMode(hStdout, mode);
    return 1;
}

/* ============================================================================
 * CURSOR CONTROL
 * ============================================================================ */

void console_cursor_hide(void)
{
    wprintf(L"\x1b[?25l");
    fflush(stdout);
}

void console_cursor_show(void)
{
    wprintf(L"\x1b[?25h");
    fflush(stdout);
}

void console_cursor_move(int row, int col)
{
    /* VT100 uses 1-based coordinates */
    wprintf(L"\x1b[%d;%dH", row + 1, col + 1);
    fflush(stdout);
}

void console_cursor_save(void)
{
    wprintf(L"\x1b[s");
    fflush(stdout);
}

void console_cursor_restore_pos(void)
{
    wprintf(L"\x1b[u");
    fflush(stdout);
}

/* ============================================================================
 * SCREEN OPERATIONS
 * ============================================================================ */

void console_clear(void)
{
    wprintf(L"\x1b[2J\x1b[H");
    fflush(stdout);
}

void console_clear_line(void)
{
    wprintf(L"\x1b[2K\r");
    fflush(stdout);
}

void console_get_size(ConsoleSize *size)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    if (GetConsoleScreenBufferInfo(g_console.hStdout, &csbi)) {
        size->cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        size->rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        /* Default fallback */
        size->cols = 80;
        size->rows = 24;
    }
}

/* ============================================================================
 * COLOR AND STYLE
 * ============================================================================ */

void console_set_fg(ConsoleColor color)
{
    if (color == COLOR_RESET) {
        console_reset_style();
        return;
    }

    if (color < 8) {
        wprintf(L"\x1b[%dm", 30 + color);
    } else {
        wprintf(L"\x1b[%dm", 90 + (color - 8));
    }
    fflush(stdout);
}

void console_set_bg(ConsoleColor color)
{
    if (color == COLOR_RESET) {
        console_reset_style();
        return;
    }

    if (color < 8) {
        wprintf(L"\x1b[%dm", 40 + color);
    } else {
        wprintf(L"\x1b[%dm", 100 + (color - 8));
    }
    fflush(stdout);
}

void console_set_bold(void)
{
    wprintf(L"\x1b[1m");
    fflush(stdout);
}

void console_set_dim(void)
{
    wprintf(L"\x1b[2m");
    fflush(stdout);
}

void console_set_reverse(void)
{
    wprintf(L"\x1b[7m");
    fflush(stdout);
}

void console_reset_style(void)
{
    wprintf(L"\x1b[0m");
    fflush(stdout);
}

/* ============================================================================
 * INPUT HANDLING
 * ============================================================================ */

int console_input_available(void)
{
    return _kbhit();
}

KeyEvent console_read_key(void)
{
    KeyEvent event = {KEY_NONE, 0};
    int ch;

    ch = _getch();

    /* Handle special keys (arrow keys, etc.) */
    if (ch == 0 || ch == 0xE0) {
        ch = _getch();
        switch (ch) {
            case 72: event.type = KEY_UP; break;
            case 80: event.type = KEY_DOWN; break;
            case 75: event.type = KEY_LEFT; break;
            case 77: event.type = KEY_RIGHT; break;
            default: event.type = KEY_NONE; break;
        }
        return event;
    }

    /* Handle regular keys */
    switch (ch) {
        case 13:
            event.type = KEY_ENTER;
            break;
        case 27:
            event.type = KEY_ESC;
            break;
        case 9:
            event.type = KEY_TAB;
            break;
        case 8:
            event.type = KEY_BACKSPACE;
            break;
        case 32:
            event.type = KEY_SPACE;
            break;
        default:
            event.type = KEY_CHAR;
            event.ch = (wchar_t)ch;
            break;
    }

    return event;
}

KeyEvent console_read_key_timeout(int timeout_ms)
{
    KeyEvent event = {KEY_NONE, 0};
    ULONGLONG start_time = GetTickCount64();
    ULONGLONG elapsed;

    while (1) {
        if (_kbhit()) {
            return console_read_key();
        }

        elapsed = GetTickCount64() - start_time;
        if (elapsed >= (ULONGLONG)timeout_ms) {
            break;
        }

        Sleep(50);  /* Poll every 50ms */
    }

    return event;
}

void console_flush_input(void)
{
    while (_kbhit()) {
        _getch();
    }
}
