/*
 * process.h - Process execution and output capture
 */

#ifndef PROCESS_H
#define PROCESS_H

#include "utils.h"

/* ============================================================================
 * PROCESS EXECUTION
 * ============================================================================ */

/*
 * Execute a process and wait for completion
 * Returns exit code, or -1 on failure to launch
 */
int run_process(wchar_t *cmdline, int silent);

/*
 * Execute a process and capture its output
 * Returns exit code, output is written to buffer
 * buffer_size should be the size of the buffer
 * Returns -1 on failure to launch
 */
int run_process_capture(wchar_t *cmdline, char *buffer, size_t buffer_size);

/*
 * Execute netsh command
 * Returns 0 on success, non-zero on failure
 */
int run_netsh(const wchar_t *args);

/*
 * Execute netsh command silently (ignore errors)
 */
void run_netsh_silent(const wchar_t *args);

/*
 * Execute netsh command and capture output
 * Returns 0 on success, output in buffer
 */
int run_netsh_capture(const wchar_t *args, char *buffer, size_t buffer_size);

#endif /* PROCESS_H */
