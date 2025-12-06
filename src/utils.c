/*
 * utils.c - Common utilities, constants, and helpers
 */

#include "utils.h"

/* ============================================================================
 * PRINTING FUNCTIONS
 * ============================================================================ */

void print_error(const wchar_t *msg)
{
    fwprintf(stderr, L"[ERROR] %ls\n", msg);
}

void print_info(const wchar_t *msg)
{
    wprintf(L"[INFO] %ls\n", msg);
}

void print_success(const wchar_t *msg)
{
    wprintf(L"[OK] %ls\n", msg);
}

/* ============================================================================
 * STRING HELPERS
 * ============================================================================ */

wchar_t *trim(wchar_t *str)
{
    wchar_t *end;

    /* Trim leading */
    while (iswspace(*str)) str++;

    if (*str == L'\0') return str;

    /* Trim trailing */
    end = str + wcslen(str) - 1;
    while (end > str && iswspace(*end)) end--;
    end[1] = L'\0';

    return str;
}

char *find_ipv4(char *str)
{
    char *p = str;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            char *start = p;
            int dots = 0;
            while ((*p >= '0' && *p <= '9') || *p == '.') {
                if (*p == '.') dots++;
                p++;
            }
            if (dots == 3) {
                return start;
            }
        } else {
            p++;
        }
    }
    return NULL;
}

char *find_ipv6(char *str)
{
    char *p = str;
    while (*p) {
        if ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
            (*p >= 'A' && *p <= 'F') || *p == ':') {
            char *start = p;
            int colons = 0;
            while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
                   (*p >= 'A' && *p <= 'F') || *p == ':') {
                if (*p == ':') colons++;
                p++;
            }
            if (colons >= 2) {
                return start;
            }
        } else {
            p++;
        }
    }
    return NULL;
}

/* ============================================================================
 * VALIDATION
 * ============================================================================ */

int validate_interface_alias(const wchar_t *alias)
{
    size_t len = 0;

    if (!alias || *alias == L'\0') {
        return 0;
    }

    if (FAILED(StringCchLengthW(alias, MAX_IFACE_LEN + 1, &len)) || len > MAX_IFACE_LEN) {
        return 0;
    }

    for (size_t i = 0; i < len; i++) {
        wchar_t c = alias[i];
        int valid = (c >= L'A' && c <= L'Z') ||
                    (c >= L'a' && c <= L'z') ||
                    (c >= L'0' && c <= L'9') ||
                    c == L' ' || c == L'-' || c == L'_' ||
                    c == L'(' || c == L')' || c == L'.';
        if (!valid) {
            return 0;
        }
    }

    return 1;
}
