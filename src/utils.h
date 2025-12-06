/*
 * utils.h - Common utilities, constants, and helpers
 */

#ifndef UTILS_H
#define UTILS_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <strsafe.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define MAX_IFACE_LEN       128
#define MAX_PATH_LEN        512
#define MAX_ADDR_LEN        64
#define CMD_BUFFER_SIZE     2048
#define CONFIG_LINE_SIZE    512
#define PIPE_BUFFER_SIZE    8192

#define DEFAULT_CONFIG_FILE L"static-ip-fix.ini"

/* ============================================================================
 * PRINTING FUNCTIONS
 * ============================================================================ */

void print_error(const wchar_t *msg);
void print_info(const wchar_t *msg);
void print_success(const wchar_t *msg);

/* ============================================================================
 * STRING HELPERS
 * ============================================================================ */

/*
 * Trim whitespace from both ends of a string (in place)
 * Returns pointer to trimmed string
 */
wchar_t *trim(wchar_t *str);

/*
 * Find IPv4 address in a string
 * Returns pointer to start of IP or NULL if not found
 */
char *find_ipv4(char *str);

/*
 * Find IPv6 address in a string
 * Returns pointer to start of IP or NULL if not found
 */
char *find_ipv6(char *str);

/* ============================================================================
 * VALIDATION
 * ============================================================================ */

/*
 * Validate interface alias - allow only safe characters
 * Returns 1 if valid, 0 if invalid
 */
int validate_interface_alias(const wchar_t *alias);

#endif /* UTILS_H */
