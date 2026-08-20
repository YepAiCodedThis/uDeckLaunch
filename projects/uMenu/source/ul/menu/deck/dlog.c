#include "dlog.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <switch.h>

#define DH_LOG_PATH "/switch/DeckHome/dh.log"
#define DH_LOG_OLD  "/switch/DeckHome/dh.log.old"
#define DH_LOG_MAX  (256u * 1024u)

static int g_sd_ready;
static int g_writes;

static int write_line(const char *line, int n)
{
    int tries = g_sd_ready ? 1 : 40;
    int i;
    for (i = 0; i < tries; i++) {
        FsFileSystem sd;
        if (R_SUCCEEDED(fsOpenSdCardFileSystem(&sd))) {
            fsFsCreateDirectory(&sd, "/switch");
            fsFsCreateDirectory(&sd, "/switch/DeckHome");

            s64 sz = 0;
            FsFile probe;
            if (R_SUCCEEDED(fsFsOpenFile(&sd, DH_LOG_PATH, FsOpenMode_Read, &probe))) {
                fsFileGetSize(&probe, &sz);
                fsFileClose(&probe);
                if (sz > (s64)DH_LOG_MAX) {
                    fsFsDeleteFile(&sd, DH_LOG_OLD);
                    fsFsRenameFile(&sd, DH_LOG_PATH, DH_LOG_OLD);
                    sz = 0;
                }
            }

            fsFsCreateFile(&sd, DH_LOG_PATH, 0, 0);
            FsFile f;
            if (R_SUCCEEDED(fsFsOpenFile(&sd, DH_LOG_PATH, FsOpenMode_Write | FsOpenMode_Append, &f))) {
                s64 off = 0;
                fsFileGetSize(&f, &off);
                g_writes++;
                u32 opt = (g_writes >= 8) ? FsWriteOption_Flush : 0;
                if (g_writes >= 8) {
                    g_writes = 0;
                }
                fsFileWrite(&f, off, line, (size_t)n, opt);
                fsFileClose(&f);
                fsFsClose(&sd);
                g_sd_ready = 1;
                return 1;
            }
            fsFsClose(&sd);
        }
        if (!g_sd_ready) {
            svcSleepThread(100000000ull);
        }
    }
    return 0;
}

static int format_prefix(char *out, size_t cap, const char *tag)
{
    u64 tick = armGetSystemTick();
    u64 sec = tick / 19200000ull;
    u64 ms = (tick % 19200000ull) * 1000ull / 19200000ull;
    return snprintf(out, cap, "%llu.%03llu %s: ",
        (unsigned long long)sec, (unsigned long long)ms, tag);
}

void dlog(const char *tag, const char *fmt, ...)
{
    char line[280];
    int p = format_prefix(line, sizeof(line), tag ? tag : "?");
    if (p < 0) {
        p = 0;
    }
    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(line + p, sizeof(line) - (size_t)p, fmt, ap);
    va_end(ap);
    if (m < 0) {
        m = 0;
    }
    int n = p + m;
    if (n > (int)sizeof(line) - 2) {
        n = (int)sizeof(line) - 2;
    }
    line[n++] = '\n';
    line[n] = '\0';
    write_line(line, n);
}

void dlog_rc(const char *tag, Result rc, const char *fmt, ...)
{
    char msg[180];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    if (rc) {
        dlog(tag, "%s rc=0x%x mod=%u desc=%u", msg, rc, R_MODULE(rc), R_DESCRIPTION(rc));
    }
}

void dlog_hex(const char *tag, const void *data, unsigned n)
{
    const u8 *p = (const u8 *)data;
    char hex[96];
    unsigned i, o = 0;
    if (n > 24) {
        n = 24;
    }
    for (i = 0; i < n && o + 3 < sizeof(hex); i++) {
        o += (unsigned)snprintf(hex + o, sizeof(hex) - o, "%02x", p[i]);
    }
    hex[o] = '\0';
    dlog(tag, "hex[%u] %s", n, hex);
}

void dlog_banner(const char *who)
{
    dlog(who, "======== boot ========");
}
