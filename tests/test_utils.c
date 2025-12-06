/*
 * test_utils.c - Tests for utility functions
 */

#include "utils.h"
#include "test.h"
#include <string.h>

/* ============================================================================
 * TRIM TESTS
 * ============================================================================ */

TEST(test_trim_leading_spaces) {
    wchar_t str[] = L"  hello";
    wchar_t *result = trim(str);
    ASSERT_WSTR_EQ(L"hello", result);
}

TEST(test_trim_trailing_spaces) {
    wchar_t str[] = L"hello  ";
    wchar_t *result = trim(str);
    ASSERT_WSTR_EQ(L"hello", result);
}

TEST(test_trim_both_sides) {
    wchar_t str[] = L"  hello  ";
    wchar_t *result = trim(str);
    ASSERT_WSTR_EQ(L"hello", result);
}

TEST(test_trim_tabs_newlines) {
    wchar_t str[] = L"\t\nhello\r\n";
    wchar_t *result = trim(str);
    ASSERT_WSTR_EQ(L"hello", result);
}

TEST(test_trim_no_whitespace) {
    wchar_t str[] = L"hello";
    wchar_t *result = trim(str);
    ASSERT_WSTR_EQ(L"hello", result);
}

TEST(test_trim_all_whitespace) {
    wchar_t str[] = L"   ";
    wchar_t *result = trim(str);
    ASSERT_WSTR_EQ(L"", result);
}

TEST(test_trim_empty_string) {
    wchar_t str[] = L"";
    wchar_t *result = trim(str);
    ASSERT_WSTR_EQ(L"", result);
}

/* ============================================================================
 * FIND_IPV4 TESTS
 * ============================================================================ */

TEST(test_find_ipv4_just_ip) {
    char str[] = "192.168.1.1";
    char *result = find_ipv4(str);
    ASSERT_NOT_NULL(result);
    ASSERT_EQ(0, strncmp(result, "192.168.1.1", 11));
}

TEST(test_find_ipv4_with_label) {
    char str[] = "DNS Server: 8.8.8.8";
    char *result = find_ipv4(str);
    ASSERT_NOT_NULL(result);
    ASSERT_EQ(0, strncmp(result, "8.8.8.8", 7));
}

TEST(test_find_ipv4_at_end) {
    char str[] = "Server is 1.1.1.1";
    char *result = find_ipv4(str);
    ASSERT_NOT_NULL(result);
    ASSERT_EQ(0, strncmp(result, "1.1.1.1", 7));
}

TEST(test_find_ipv4_cloudflare) {
    char str[] = "Primary: 1.0.0.1";
    char *result = find_ipv4(str);
    ASSERT_NOT_NULL(result);
    ASSERT_EQ(0, strncmp(result, "1.0.0.1", 7));
}

TEST(test_find_ipv4_none) {
    char str[] = "no ip address here";
    char *result = find_ipv4(str);
    ASSERT_NULL(result);
}

TEST(test_find_ipv4_partial) {
    char str[] = "192.168.1";
    char *result = find_ipv4(str);
    ASSERT_NULL(result);
}

/* ============================================================================
 * FIND_IPV6 TESTS
 * ============================================================================ */

TEST(test_find_ipv6_full) {
    char str[] = "2001:4860:4860::8888";
    char *result = find_ipv6(str);
    ASSERT_NOT_NULL(result);
    ASSERT_EQ(0, strncmp(result, "2001:4860:4860::8888", 20));
}

TEST(test_find_ipv6_with_label) {
    char str[] = "IPv6 DNS: 2606:4700:4700::1111";
    char *result = find_ipv6(str);
    ASSERT_NOT_NULL(result);
    ASSERT_EQ(0, strncmp(result, "2606:4700:4700::1111", 20));
}

TEST(test_find_ipv6_cloudflare) {
    char str[] = "Server: 2606:4700:4700::1001";
    char *result = find_ipv6(str);
    ASSERT_NOT_NULL(result);
    ASSERT_EQ(0, strncmp(result, "2606:4700:4700::1001", 20));
}

TEST(test_find_ipv6_none) {
    char str[] = "just some text without ipv6";
    char *result = find_ipv6(str);
    ASSERT_NULL(result);
}

/* ============================================================================
 * VALIDATE_INTERFACE_ALIAS TESTS
 * ============================================================================ */

TEST(test_validate_interface_simple) {
    ASSERT_EQ(1, validate_interface_alias(L"Ethernet"));
}

TEST(test_validate_interface_with_space) {
    ASSERT_EQ(1, validate_interface_alias(L"Wi-Fi 2"));
}

TEST(test_validate_interface_with_hyphen) {
    ASSERT_EQ(1, validate_interface_alias(L"Local-Area"));
}

TEST(test_validate_interface_with_number) {
    ASSERT_EQ(1, validate_interface_alias(L"Ethernet0"));
}

TEST(test_validate_interface_injection_semicolon) {
    ASSERT_EQ(0, validate_interface_alias(L"Eth; rm -rf"));
}

TEST(test_validate_interface_injection_ampersand) {
    ASSERT_EQ(0, validate_interface_alias(L"Eth && del"));
}

TEST(test_validate_interface_injection_pipe) {
    ASSERT_EQ(0, validate_interface_alias(L"Eth | cmd"));
}

TEST(test_validate_interface_quotes) {
    ASSERT_EQ(0, validate_interface_alias(L"Eth\""));
}

TEST(test_validate_interface_empty) {
    ASSERT_EQ(0, validate_interface_alias(L""));
}

TEST(test_validate_interface_null) {
    ASSERT_EQ(0, validate_interface_alias(NULL));
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main(void) {
    TEST_INIT();

    /* trim tests */
    RUN_TEST(test_trim_leading_spaces);
    RUN_TEST(test_trim_trailing_spaces);
    RUN_TEST(test_trim_both_sides);
    RUN_TEST(test_trim_tabs_newlines);
    RUN_TEST(test_trim_no_whitespace);
    RUN_TEST(test_trim_all_whitespace);
    RUN_TEST(test_trim_empty_string);

    /* find_ipv4 tests */
    RUN_TEST(test_find_ipv4_just_ip);
    RUN_TEST(test_find_ipv4_with_label);
    RUN_TEST(test_find_ipv4_at_end);
    RUN_TEST(test_find_ipv4_cloudflare);
    RUN_TEST(test_find_ipv4_none);
    RUN_TEST(test_find_ipv4_partial);

    /* find_ipv6 tests */
    RUN_TEST(test_find_ipv6_full);
    RUN_TEST(test_find_ipv6_with_label);
    RUN_TEST(test_find_ipv6_cloudflare);
    RUN_TEST(test_find_ipv6_none);

    /* validate_interface_alias tests */
    RUN_TEST(test_validate_interface_simple);
    RUN_TEST(test_validate_interface_with_space);
    RUN_TEST(test_validate_interface_with_hyphen);
    RUN_TEST(test_validate_interface_with_number);
    RUN_TEST(test_validate_interface_injection_semicolon);
    RUN_TEST(test_validate_interface_injection_ampersand);
    RUN_TEST(test_validate_interface_injection_pipe);
    RUN_TEST(test_validate_interface_quotes);
    RUN_TEST(test_validate_interface_empty);
    RUN_TEST(test_validate_interface_null);

    TEST_REPORT();
    return TEST_EXIT_CODE();
}
