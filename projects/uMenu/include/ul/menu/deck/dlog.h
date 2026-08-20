#pragma once

#include <switch.h>

/* Short error/boot log at sdmc:/switch/DeckHome/dh.log (rotated at 256 KiB). */
void dlog(const char *tag, const char *fmt, ...);
void dlog_rc(const char *tag, Result rc, const char *fmt, ...);
void dlog_hex(const char *tag, const void *data, unsigned n);
void dlog_banner(const char *who);
