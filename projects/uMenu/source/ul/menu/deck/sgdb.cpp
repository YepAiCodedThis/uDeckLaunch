#include "sgdb.h"

extern "C" {
#include "dlog.h"
#include "titles.h"
}

#define JSON_NOEXCEPTION 1
#include <json.hpp>
#include <curl/curl.h>
#include <SDL_image.h>

#include <arpa/inet.h>
#include <cctype>
#include <cstddef>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <strings.h>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#define DH_SGDB_DIR "sdmc:/ulaunch/cache/sgdb"
#define DH_SGDB_PICK_DIR "sdmc:/ulaunch/cache/sgdb/pick"
#define DH_SGDB_INDEX "sdmc:/ulaunch/cache/sgdb/index.txt"
#define DH_SGDB_MAP "sdmc:/ulaunch/cache/sgdb/map.txt"
#define DH_SGDB_UA "uDeckLaunch"

namespace {

enum { JOB_FETCH = 0, JOB_TEST, JOB_PICK, JOB_APPLY, JOB_EASYAPI };
#define DH_EASYAPI_URL "https://udeck.botaweso.me/api/easyapi/redeem"
#define DH_UDECK_LOOKUP "https://udeck.botaweso.me/api/covers/lookup"

struct Job {
    int kind;
    int fmt;
    u64 app_id;
    char name[128];
    char url[320];
    char title_id[17];
};

struct CacheEnt {
    u64 app_id;
    int sgdb_id;
    int fmt;
    SDL_Texture *tex;
    int w;
    int h;
    int loaded;
};

struct IdEnt {
    u64 app_id;
    int sgdb_id;
};

struct HttpRes {
    std::string body;
    long http;
    CURLcode curl;
    char err[CURL_ERROR_SIZE];
};

struct PickEnt {
    SDL_Texture *tex;
    int w;
    int h;
    int loaded;
    char url[320];
    char thumb[320];
    char label[40];
    char path[96];
};

struct ArtCand {
    std::string url;
    std::string thumb;
    int w;
    int h;
    int score;
};

char g_key[128];
int g_imported;
int g_on;
int g_busy;
int g_done;
int g_fail;
int g_total;
int g_stop;
int g_mx_ok;
int g_th_ok;
int g_curl_ok;
int g_sock_ok;
int g_ssl_ok;
int g_pick_n;
int g_pick_ready;
int g_pick_gen;
int g_fmt;
int g_src;
int g_index_ok;
u64 g_hint[64];
int g_hint_n;
CURL *g_easy;
curl_slist *g_hdr;
curl_slist *g_resolve;
Mutex g_mx;
Thread g_th;
char g_phase[48];
char g_current[80];
char g_error[128];
std::vector<Job> g_jobs;
std::vector<CacheEnt> g_cache;
std::vector<IdEnt> g_ids;
PickEnt g_pick[DH_SGDB_PICK_MAX];

void ensure_dir(void)
{
    mkdir("sdmc:/ulaunch", 0755);
    mkdir("sdmc:/ulaunch/cache", 0755);
    mkdir(DH_SGDB_DIR, 0755);
    mkdir(DH_SGDB_PICK_DIR, 0755);
}

void set_phase(const char *s)
{
    snprintf(g_phase, sizeof(g_phase), "%s", s ? s : "");
}

void set_current(const char *s)
{
    snprintf(g_current, sizeof(g_current), "%s", s ? s : "");
}

void set_err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_error, sizeof(g_error), fmt, ap);
    va_end(ap);
    dlog("sgdb", "%s", g_error);
}

void clear_err(void)
{
    g_error[0] = 0;
}

int is_square(int fmt)
{
    return fmt == DH_FMT_SQUARE;
}

void cache_path(u64 app_id, int fmt, char *out, size_t cap)
{
    snprintf(out, cap, DH_SGDB_DIR "/%016llx.%s.png",
        (unsigned long long)app_id, is_square(fmt) ? "sq" : "cap");
}

void legacy_path(u64 app_id, char *out, size_t cap)
{
    snprintf(out, cap, DH_SGDB_DIR "/%016llx.png", (unsigned long long)app_id);
}

int file_ok(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && st.st_size > 32;
}

void drop_legacy(u64 app_id)
{
    char p[96];
    legacy_path(app_id, p, sizeof(p));
    if (file_ok(p)) {
        remove(p);
    }
}

CacheEnt *find_ent(u64 app_id)
{
    for (CacheEnt &e : g_cache) {
        if (e.app_id == app_id) {
            return &e;
        }
    }
    return nullptr;
}

CacheEnt *ensure_ent(u64 app_id)
{
    CacheEnt *e = find_ent(app_id);
    if (e) {
        return e;
    }
    CacheEnt n{};
    n.app_id = app_id;
    n.sgdb_id = -1;
    n.fmt = -1;
    g_cache.push_back(n);
    return &g_cache.back();
}

void unload_ent(CacheEnt *e)
{
    if (!e) {
        return;
    }
    if (e->tex) {
        SDL_DestroyTexture(e->tex);
        e->tex = nullptr;
    }
    e->w = 0;
    e->h = 0;
    e->loaded = 0;
    e->fmt = -1;
}

void unload_all_tex(void)
{
    for (CacheEnt &e : g_cache) {
        unload_ent(&e);
    }
}

int id_get(u64 app_id)
{
    CacheEnt *e = find_ent(app_id);
    if (e && e->sgdb_id > 0) {
        return e->sgdb_id;
    }
    for (const IdEnt &it : g_ids) {
        if (it.app_id == app_id) {
            if (e) {
                e->sgdb_id = it.sgdb_id;
            }
            return it.sgdb_id;
        }
    }
    return 0;
}

void index_save(void)
{
    FILE *f = fopen(DH_SGDB_INDEX, "w");
    if (!f) {
        return;
    }
    for (const IdEnt &it : g_ids) {
        if (it.sgdb_id > 0) {
            fprintf(f, "%016llx %d\n", (unsigned long long)it.app_id, it.sgdb_id);
        }
    }
    fclose(f);
}

void index_add_line(u64 app_id, int sgdb_id)
{
    if (!app_id || sgdb_id <= 0) {
        return;
    }
    for (IdEnt &it : g_ids) {
        if (it.app_id == app_id) {
            it.sgdb_id = sgdb_id;
            return;
        }
    }
    g_ids.push_back({app_id, sgdb_id});
}

void index_upsert(u64 app_id, int sgdb_id)
{
    index_add_line(app_id, sgdb_id);
    CacheEnt *e = ensure_ent(app_id);
    e->sgdb_id = sgdb_id;
    index_save();
}

void index_load(void)
{
    if (g_index_ok) {
        return;
    }
    g_index_ok = 1;
    const char *paths[2] = {DH_SGDB_INDEX, DH_SGDB_MAP};
    for (int p = 0; p < 2; p++) {
        FILE *f = fopen(paths[p], "r");
        if (!f) {
            continue;
        }
        char line[80];
        while (fgets(line, sizeof(line), f)) {
            unsigned long long id = 0;
            int gid = 0;
            if (sscanf(line, "%llx %d", &id, &gid) == 2 && id && gid > 0) {
                index_add_line((u64)id, gid);
            }
        }
        fclose(f);
        if (p == 1 && !g_ids.empty()) {
            index_save();
        }
        if (!g_ids.empty()) {
            break;
        }
    }
}

int copy_file(const char *from, const char *to)
{
    FILE *in = fopen(from, "rb");
    if (!in) {
        return -1;
    }
    FILE *out = fopen(to, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            remove(to);
            return -1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

void clean_name(const char *in, char *out, size_t cap)
{
    if (!in || cap < 2) {
        if (cap) {
            out[0] = 0;
        }
        return;
    }
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && o + 1 < cap; p++) {
        unsigned char c = *p;
        if (c < 0x20) {
            break;
        }
        if (c == 0xE2 && p[1] == 0x80 && (p[2] == 0x99 || p[2] == 0x98)) {
            out[o++] = '\'';
            p += 2;
            continue;
        }
        if (c == 0xC2 && p[1] == 0xAE) {
            p++;
            continue;
        }
        out[o++] = (char)c;
    }
    while (o > 0 && out[o - 1] == ' ') {
        o--;
    }
    out[o] = 0;
}

std::size_t write_str(char *in, std::size_t size, std::size_t count, void *ud)
{
    const auto n = size * count;
    static_cast<std::string *>(ud)->append(in, n);
    return n;
}

std::size_t write_file(char *in, std::size_t size, std::size_t count, void *ud)
{
    return fwrite(in, size, count, static_cast<FILE *>(ud));
}

struct DnsEnt {
    char host[80];
    char ip[16];
};

DnsEnt g_dns[24];
int g_dns_n;

void ipv4_dots(u32 ip, char *out, size_t cap)
{
    snprintf(out, cap, "%u.%u.%u.%u", ip & 255u, (ip >> 8) & 255u,
        (ip >> 16) & 255u, (ip >> 24) & 255u);
}

int dns_skip_name(const unsigned char *msg, int len, int *pos)
{
    int p = *pos;
    int hops = 0;
    int jumped = 0;
    int end = *pos;
    while (p < len && hops++ < 16) {
        unsigned char l = msg[p];
        if ((l & 0xC0) == 0xC0) {
            if (p + 1 >= len) {
                return -1;
            }
            if (!jumped) {
                end = p + 2;
            }
            jumped = 1;
            p = ((l & 0x3F) << 8) | msg[p + 1];
            continue;
        }
        if (l == 0) {
            p++;
            if (!jumped) {
                end = p;
            }
            *pos = end;
            return 0;
        }
        p += l + 1;
    }
    return -1;
}

int dns_parse_a(const unsigned char *msg, int len, char *ip, size_t cap)
{
    if (len < 12) {
        return -1;
    }
    int qd = (msg[4] << 8) | msg[5];
    int an = (msg[6] << 8) | msg[7];
    int pos = 12;
    for (int i = 0; i < qd; i++) {
        if (dns_skip_name(msg, len, &pos) != 0 || pos + 4 > len) {
            return -1;
        }
        pos += 4;
    }
    for (int i = 0; i < an; i++) {
        if (dns_skip_name(msg, len, &pos) != 0 || pos + 10 > len) {
            return -1;
        }
        int type = (msg[pos] << 8) | msg[pos + 1];
        int rdlen = (msg[pos + 8] << 8) | msg[pos + 9];
        pos += 10;
        if (pos + rdlen > len) {
            return -1;
        }
        if (type == 1 && rdlen == 4) {
            snprintf(ip, cap, "%u.%u.%u.%u", msg[pos], msg[pos + 1],
                msg[pos + 2], msg[pos + 3]);
            return 0;
        }
        pos += rdlen;
    }
    return -1;
}

int dns_query_udp(const char *server, const char *host, char *ip, size_t cap)
{
    unsigned char q[512];
    memset(q, 0, sizeof(q));
    q[0] = 0x12;
    q[1] = 0x34;
    q[2] = 0x01;
    q[5] = 1;
    int o = 12;
    const char *h = host;
    while (*h) {
        const char *dot = strchr(h, '.');
        int n = dot ? (int)(dot - h) : (int)strlen(h);
        if (n <= 0 || n > 63 || o + n + 1 >= 480) {
            return -1;
        }
        q[o++] = (unsigned char)n;
        memcpy(q + o, h, (size_t)n);
        o += n;
        h += n;
        if (*h == '.') {
            h++;
        }
    }
    q[o++] = 0;
    q[o++] = 0;
    q[o++] = 1;
    q[o++] = 0;
    q[o++] = 1;

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }
    struct sockaddr_in sa {};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(53);
    if (inet_pton(AF_INET, server, &sa.sin_addr) != 1) {
        close(fd);
        return -1;
    }
    if (sendto(fd, q, (size_t)o, 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        close(fd);
        return -1;
    }
    fd_set rs;
    FD_ZERO(&rs);
    FD_SET(fd, &rs);
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    if (select(fd + 1, &rs, nullptr, nullptr, &tv) <= 0) {
        close(fd);
        return -1;
    }
    unsigned char ans[512];
    int n = (int)recvfrom(fd, ans, sizeof(ans), 0, nullptr, nullptr);
    close(fd);
    if (n < 12) {
        return -1;
    }
    return dns_parse_a(ans, n, ip, cap);
}

int dns_getaddrinfo(const char *host, char *ip, size_t cap)
{
    struct addrinfo hints {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = nullptr;
    if (getaddrinfo(host, nullptr, &hints, &res) != 0 || !res) {
        return -1;
    }
    int ok = -1;
    if (res->ai_family == AF_INET && res->ai_addr) {
        auto *in = (struct sockaddr_in *)res->ai_addr;
        if (inet_ntop(AF_INET, &in->sin_addr, ip, cap)) {
            ok = 0;
        }
    }
    freeaddrinfo(res);
    return ok;
}

int dns_lookup(const char *host, char *ip, size_t cap)
{
    if (!host || !host[0] || cap < 8) {
        return -1;
    }
    for (int i = 0; i < g_dns_n; i++) {
        if (strcmp(g_dns[i].host, host) == 0) {
            snprintf(ip, cap, "%s", g_dns[i].ip);
            return 0;
        }
    }
    const char *via = nullptr;
    if (dns_getaddrinfo(host, ip, cap) == 0) {
        via = "sfdns";
    }
    if (!via && dns_query_udp("1.1.1.1", host, ip, cap) == 0) {
        via = "1.1.1.1";
    }
    if (!via && dns_query_udp("8.8.8.8", host, ip, cap) == 0) {
        via = "8.8.8.8";
    }
    if (!via) {
        u32 addr = 0, mask = 0, gw = 0, d1 = 0, d2 = 0;
        if (R_SUCCEEDED(nifmGetCurrentIpConfigInfo(&addr, &mask, &gw, &d1, &d2))) {
            char ds[16];
            if (d1) {
                ipv4_dots(d1, ds, sizeof(ds));
                if (dns_query_udp(ds, host, ip, cap) == 0) {
                    via = "nifm1";
                }
            }
            if (!via && d2) {
                ipv4_dots(d2, ds, sizeof(ds));
                if (dns_query_udp(ds, host, ip, cap) == 0) {
                    via = "nifm2";
                }
            }
        }
    }
    if (!via) {
        dlog("sgdb", "dns fail %s", host);
        return -1;
    }
    dlog("sgdb", "dns %s -> %s (%s)", host, ip, via);
    if (g_dns_n < (int)(sizeof(g_dns) / sizeof(g_dns[0]))) {
        snprintf(g_dns[g_dns_n].host, sizeof(g_dns[0].host), "%s", host);
        snprintf(g_dns[g_dns_n].ip, sizeof(g_dns[0].ip), "%s", ip);
        g_dns_n++;
    }
    return 0;
}

int parse_url_host(const char *url, char *host, size_t cap, int *port)
{
    *port = 443;
    const char *p = url;
    if (strncmp(p, "https://", 8) == 0) {
        p += 8;
        *port = 443;
    } else if (strncmp(p, "http://", 7) == 0) {
        p += 7;
        *port = 80;
    }
    const char *end = p;
    while (*end && *end != '/' && *end != '?' && *end != '#') {
        end++;
    }
    size_t n = (size_t)(end - p);
    const char *colon = (const char *)memchr(p, ':', n);
    if (colon) {
        *port = atoi(colon + 1);
        n = (size_t)(colon - p);
    }
    if (n == 0 || n >= cap) {
        return -1;
    }
    memcpy(host, p, n);
    host[n] = 0;
    return 0;
}

curl_slist *make_resolve(const char *url)
{
    char host[80];
    char ip[16];
    int port = 443;
    if (parse_url_host(url, host, sizeof(host), &port) != 0) {
        return nullptr;
    }
    int a, b, c, d;
    if (sscanf(host, "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
        return nullptr;
    }
    if (dns_lookup(host, ip, sizeof(ip)) != 0) {
        return nullptr;
    }
    curl_slist *lst = nullptr;
    char rule[128];
    snprintf(rule, sizeof(rule), "%s:%d:%s", host, port, ip);
    lst = curl_slist_append(lst, rule);
    if (port == 443) {
        snprintf(rule, sizeof(rule), "%s:80:%s", host, ip);
        lst = curl_slist_append(lst, rule);
    } else if (port == 80) {
        snprintf(rule, sizeof(rule), "%s:443:%s", host, ip);
        lst = curl_slist_append(lst, rule);
    }
    return lst;
}

int wifi_up(void)
{
    u32 stren = 0;
    NifmInternetConnectionStatus st = NifmInternetConnectionStatus_ConnectingUnknown1;
    Result rc = nifmGetInternetConnectionStatus(nullptr, &stren, &st);
    static int logged;
    if (!logged) {
        dlog("sgdb", "nifm rc=0x%x st=%u wifi=%u", rc, (unsigned)st, stren);
        logged = 1;
    }
    return R_SUCCEEDED(rc) && st == NifmInternetConnectionStatus_Connected;
}

int ensure_net(void)
{
    if (!g_sock_ok) {
        SocketInitConfig cfg = *socketGetDefaultInitConfig();
        cfg.bsd_service_type = BsdServiceType_User;
        cfg.num_bsd_sessions = 4;
        cfg.tcp_rx_buf_size = 0x40000;
        cfg.tcp_rx_buf_max_size = 0x100000;
        cfg.tcp_tx_buf_size = 0x10000;
        cfg.tcp_tx_buf_max_size = 0x40000;
        cfg.sb_efficiency = 4;
        Result rc = socketInitialize(&cfg);
        if (R_FAILED(rc)) {
            rc = socketInitializeDefault();
        }
        dlog_rc("sgdb", rc, "socket");
        if (R_FAILED(rc)) {
            set_err("Network init failed (0x%x)", rc);
            return 0;
        }
        g_sock_ok = 1;
    }
    if (!g_ssl_ok) {
        Result rc = sslInitialize(3);
        dlog_rc("sgdb", rc, "ssl");
        if (R_FAILED(rc)) {
            set_err("SSL init failed (0x%x)", rc);
            return 0;
        }
        g_ssl_ok = 1;
    }
    if (!g_curl_ok) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        g_curl_ok = 1;
    }
    if (!g_easy) {
        g_easy = curl_easy_init();
        if (!g_easy) {
            set_err("curl_easy_init failed");
            return 0;
        }
    }
    return 1;
}

void curl_drop_lists(void)
{
    if (g_hdr) {
        curl_slist_free_all(g_hdr);
        g_hdr = nullptr;
    }
    if (g_resolve) {
        curl_slist_free_all(g_resolve);
        g_resolve = nullptr;
    }
}

void curl_prep(HttpRes *r, const char *url, int with_auth)
{
    curl_drop_lists();
    curl_easy_reset(g_easy);
    r->err[0] = 0;
    curl_easy_setopt(g_easy, CURLOPT_USERAGENT, DH_SGDB_UA);
    curl_easy_setopt(g_easy, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(g_easy, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(g_easy, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(g_easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(g_easy, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    curl_easy_setopt(g_easy, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(g_easy, CURLOPT_CONNECTTIMEOUT, 12L);
    curl_easy_setopt(g_easy, CURLOPT_TIMEOUT, 25L);
    curl_easy_setopt(g_easy, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(g_easy, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(g_easy, CURLOPT_BUFFERSIZE, 256L * 1024L);
    curl_easy_setopt(g_easy, CURLOPT_ERRORBUFFER, r->err);
    curl_easy_setopt(g_easy, CURLOPT_URL, url);
    if (with_auth && g_key[0]) {
        char auth[192];
        snprintf(auth, sizeof(auth), "Authorization: Bearer %s", g_key);
        g_hdr = curl_slist_append(g_hdr, auth);
        g_hdr = curl_slist_append(g_hdr, "Accept: application/json");
        curl_easy_setopt(g_easy, CURLOPT_HTTPHEADER, g_hdr);
    }
    g_resolve = make_resolve(url);
    if (g_resolve) {
        curl_easy_setopt(g_easy, CURLOPT_RESOLVE, g_resolve);
    }
}

HttpRes http_post_json(const std::string &url, const std::string &json)
{
    HttpRes r{};
    r.curl = CURLE_FAILED_INIT;
    if (!wifi_up()) {
        snprintf(r.err, sizeof(r.err), "Wi-Fi not connected");
        set_err("Wi-Fi not connected");
        return r;
    }
    if (!g_easy) {
        snprintf(r.err, sizeof(r.err), "curl_easy_init failed");
        return r;
    }
    curl_prep(&r, url.c_str(), 0);
    if (!g_resolve) {
        char host[80];
        int port = 0;
        parse_url_host(url.c_str(), host, sizeof(host), &port);
        snprintf(r.err, sizeof(r.err), "Could not resolve %s", host[0] ? host : "host");
        set_err("%s", r.err);
        return r;
    }
    g_hdr = curl_slist_append(g_hdr, "Content-Type: application/json");
    g_hdr = curl_slist_append(g_hdr, "Accept: application/json");
    curl_easy_setopt(g_easy, CURLOPT_HTTPHEADER, g_hdr);
    curl_easy_setopt(g_easy, CURLOPT_POSTFIELDS, json.c_str());
    curl_easy_setopt(g_easy, CURLOPT_POSTFIELDSIZE, (long)json.size());
    curl_easy_setopt(g_easy, CURLOPT_WRITEFUNCTION, write_str);
    curl_easy_setopt(g_easy, CURLOPT_WRITEDATA, &r.body);
    r.curl = curl_easy_perform(g_easy);
    curl_easy_getinfo(g_easy, CURLINFO_RESPONSE_CODE, &r.http);
    return r;
}

HttpRes http_get(const std::string &url, int with_auth)
{
    HttpRes r{};
    r.curl = CURLE_FAILED_INIT;
    if (!wifi_up()) {
        snprintf(r.err, sizeof(r.err), "Wi-Fi not connected");
        set_err("Wi-Fi not connected");
        return r;
    }
    if (!g_easy) {
        snprintf(r.err, sizeof(r.err), "curl_easy_init failed");
        return r;
    }
    curl_prep(&r, url.c_str(), with_auth);
    if (!g_resolve) {
        char host[80];
        int port = 0;
        parse_url_host(url.c_str(), host, sizeof(host), &port);
        snprintf(r.err, sizeof(r.err), "Could not resolve %s", host[0] ? host : "host");
        set_err("%s", r.err);
        return r;
    }
    curl_easy_setopt(g_easy, CURLOPT_WRITEFUNCTION, write_str);
    curl_easy_setopt(g_easy, CURLOPT_WRITEDATA, &r.body);
    r.curl = curl_easy_perform(g_easy);
    curl_easy_getinfo(g_easy, CURLINFO_RESPONSE_CODE, &r.http);
    return r;
}

int http_to_file(const std::string &url, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        set_err("Could not write cache file");
        return -1;
    }
    HttpRes r{};
    if (!g_easy) {
        fclose(f);
        set_err("curl_easy_init failed");
        return -1;
    }
    curl_prep(&r, url.c_str(), 0);
    if (!g_resolve) {
        fclose(f);
        set_err("Could not resolve image host");
        return -1;
    }
    curl_easy_setopt(g_easy, CURLOPT_WRITEFUNCTION, write_file);
    curl_easy_setopt(g_easy, CURLOPT_WRITEDATA, f);
    r.curl = curl_easy_perform(g_easy);
    curl_easy_getinfo(g_easy, CURLINFO_RESPONSE_CODE, &r.http);
    fclose(f);
    if (r.curl != CURLE_OK || r.http >= 400) {
        remove(path);
        if (r.curl != CURLE_OK) {
            set_err("Download: %s", r.err[0] ? r.err : curl_easy_strerror(r.curl));
        } else {
            set_err("Download: HTTP %ld", r.http);
        }
        return -1;
    }
    return 0;
}

int apply_http(const HttpRes &r, const char *what)
{
    if (r.curl != CURLE_OK) {
        set_err("%s: %s", what, r.err[0] ? r.err : curl_easy_strerror(r.curl));
        return -1;
    }
    if (r.http == 401 || r.http == 403) {
        set_err("Invalid API key (HTTP %ld)", r.http);
        return -2;
    }
    if (r.http >= 400) {
        set_err("%s: HTTP %ld", what, r.http);
        return -1;
    }
    if (r.body.empty()) {
        set_err("%s: empty body (HTTP %ld)", what, r.http);
        return -1;
    }
    return 0;
}

int parse_api(const HttpRes &r, nlohmann::json &j, const char *what)
{
    const char *p = r.body.c_str();
    size_t n = r.body.size();
    if (n >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
        p += 3;
        n -= 3;
    }
    if (n >= 2 && (unsigned char)p[0] == 0x1F && (unsigned char)p[1] == 0x8B) {
        set_err("%s: gzip body", what);
        dlog("sgdb", "%s gzip %u bytes", what, (unsigned)n);
        return -1;
    }
    while (n && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) {
        p++;
        n--;
    }
    if (!n || (*p != '{' && *p != '[')) {
        char head[48];
        size_t hn = n < 40 ? n : 40;
        memcpy(head, p, hn);
        head[hn] = 0;
        for (size_t i = 0; i < hn; i++) {
            if ((unsigned char)head[i] < 32 || (unsigned char)head[i] > 126) {
                head[i] = '.';
            }
        }
        set_err("%s: not JSON (%u b)", what, (unsigned)r.body.size());
        dlog("sgdb", "%s http=%ld notjson %.40s", what, r.http, head);
        return -1;
    }
    const char *end = p + n;
    while (end > p && (end[-1] == ' ' || end[-1] == '\n' || end[-1] == '\r' || end[-1] == '\t')) {
        end--;
    }
    j = nlohmann::json::parse(p, end, nullptr, false);
    if (j.is_discarded()) {
        set_err("%s: bad JSON (%u b)", what, (unsigned)r.body.size());
        dlog("sgdb", "%s http=%ld parse fail %u bytes", what, r.http, (unsigned)r.body.size());
        return -1;
    }
    bool ok = false;
    if (j.contains("success") && j["success"].is_boolean()) {
        ok = j["success"].get<bool>();
    } else if (j.contains("data") && j["data"].is_array()) {
        ok = true;
    }
    if (!ok) {
        char detail[96];
        detail[0] = 0;
        if (j.contains("errors") && j["errors"].is_array() && !j["errors"].empty() && j["errors"][0].is_string()) {
            snprintf(detail, sizeof(detail), "%s", j["errors"][0].get_ref<const std::string &>().c_str());
        }
        if (detail[0]) {
            set_err("%s: %s", what, detail);
        } else {
            set_err("%s: API error", what);
        }
        dlog("sgdb", "%s success=0 %s", what, detail);
        return -1;
    }
    if (!j.contains("data") || !j["data"].is_array()) {
        set_err("%s: no data", what);
        return -1;
    }
    return 0;
}

std::string url_encode(const char *s)
{
    if (!g_easy || !s) {
        return "";
    }
    char *esc = curl_easy_escape(g_easy, s, 0);
    std::string out = esc ? esc : "";
    if (esc) {
        curl_free(esc);
    }
    return out;
}

int name_score(const std::string &got, const char *want)
{
    if (got.empty() || !want || !want[0]) {
        return 0;
    }
    if (strcasecmp(got.c_str(), want) == 0) {
        return 1000;
    }
    std::string a = got;
    std::string b = want;
    for (char &ch : a) {
        ch = (char)tolower((unsigned char)ch);
    }
    for (char &ch : b) {
        ch = (char)tolower((unsigned char)ch);
    }
    if (a == b) {
        return 900;
    }
    if (a.find(b) != std::string::npos || b.find(a) != std::string::npos) {
        return 500;
    }
    return 10;
}

int search_game(const char *name, int *auth_fail)
{
    *auth_fail = 0;
    char cleaned[128];
    clean_name(name, cleaned, sizeof(cleaned));
    if (!cleaned[0]) {
        return 0;
    }
    std::string enc = url_encode(cleaned);
    if (enc.empty()) {
        set_err("Could not encode title name");
        return 0;
    }
    set_phase("Searching");
    std::string url = "https://www.steamgriddb.com/api/v2/search/autocomplete/" + enc;
    HttpRes r = http_get(url, 1);
    int ae = apply_http(r, "Search");
    if (ae == -2) {
        *auth_fail = 1;
        return 0;
    }
    if (ae != 0) {
        return 0;
    }
    auto j = nlohmann::json{};
    if (parse_api(r, j, "Search") != 0) {
        return 0;
    }
    int best_id = 0;
    int best = -1;
    for (const auto &it : j["data"]) {
        int id = it.value("id", 0);
        std::string nm = it.value("name", std::string());
        int sc = name_score(nm, cleaned);
        if (id > 0 && sc > best) {
            best = sc;
            best_id = id;
        }
    }
    return best_id;
}

int resolve_gid(u64 app_id, const char *name, int *auth_fail)
{
    *auth_fail = 0;
    int gid = id_get(app_id);
    if (gid > 0) {
        return gid;
    }
    gid = search_game(name, auth_fail);
    if (gid > 0) {
        index_upsert(app_id, gid);
    }
    return gid;
}

int score_art(int fmt, int w, int h, const std::string &style)
{
    int sc = 0;
    if (is_square(fmt)) {
        sc = w;
    } else if (w == 600 && h == 900) {
        sc = 1000;
    } else if (w == 660 && h == 930) {
        sc = 800;
    } else if (w == 342 && h == 482) {
        sc = 500;
    }
    if (style == "official") {
        sc += 50;
    }
    return sc;
}

int list_art(int sgdb_id, int fmt, std::vector<ArtCand> &out, int *auth_fail)
{
    *auth_fail = 0;
    out.clear();
    set_phase("Loading covers");
    char url[360];
    if (is_square(fmt)) {
        snprintf(url, sizeof(url),
            "https://www.steamgriddb.com/api/v2/icons/game/%d"
            "?dimensions=512,768,1024&mimes=image%%2Fpng&types=static&nsfw=false&limit=50",
            sgdb_id);
    } else {
        snprintf(url, sizeof(url),
            "https://www.steamgriddb.com/api/v2/grids/game/%d"
            "?dimensions=600x900,660x930,342x482&types=static&nsfw=false&limit=50",
            sgdb_id);
    }
    HttpRes r = http_get(url, 1);
    int ae = apply_http(r, is_square(fmt) ? "Icons" : "Grids");
    if (ae == -2) {
        *auth_fail = 1;
        return -1;
    }
    if (ae != 0) {
        return -1;
    }
    auto j = nlohmann::json{};
    if (parse_api(r, j, is_square(fmt) ? "Icons" : "Grids") != 0) {
        return -1;
    }
    for (const auto &it : j["data"]) {
        if (it.value("nsfw", false)) {
            continue;
        }
        ArtCand c;
        c.w = it.value("width", 0);
        c.h = it.value("height", 0);
        c.url = it.value("url", std::string());
        c.thumb = it.value("thumb", std::string());
        if (c.url.empty()) {
            continue;
        }
        if (c.thumb.empty()) {
            c.thumb = c.url;
        }
        c.score = score_art(fmt, c.w, c.h, it.value("style", std::string()));
        out.push_back(c);
        if ((int)out.size() >= DH_SGDB_PICK_MAX) {
            break;
        }
    }
    for (size_t i = 0; i < out.size(); i++) {
        for (size_t k = i + 1; k < out.size(); k++) {
            if (out[k].score > out[i].score) {
                ArtCand tmp = out[i];
                out[i] = out[k];
                out[k] = tmp;
            }
        }
    }
    return 0;
}

void fmt_label(char *out, size_t cap, int fmt, int w, int h)
{
    if (w > 0 && w == h) {
        snprintf(out, cap, "%d\xC2\xB2", w);
        return;
    }
    if (w > 0 && h > 0) {
        snprintf(out, cap, "%d\xC3\x97%d", w, h);
        return;
    }
    snprintf(out, cap, "%s", is_square(fmt) ? "icon" : "grid");
}

void fetch_udeck(const Job &job)
{
    char path[96];
    cache_path(job.app_id, job.fmt, path, sizeof(path));
    drop_legacy(job.app_id);
    set_phase("Searching");
    set_current(job.name);
    clear_err();
    if (!job.name[0] && !job.title_id[0]) {
        mutexLock(&g_mx);
        g_fail++;
        mutexUnlock(&g_mx);
        return;
    }
    std::string enc = url_encode(job.name[0] ? job.name : job.title_id);
    if (enc.empty() && !job.title_id[0]) {
        mutexLock(&g_mx);
        g_fail++;
        mutexUnlock(&g_mx);
        return;
    }
    std::string url = DH_UDECK_LOOKUP;
    url += "?name=";
    url += enc;
    if (job.title_id[0]) {
        url += "&titleId=";
        url += job.title_id;
    }
    HttpRes r = http_get(url, 0);
    if (r.curl != CURLE_OK) {
        set_err("uDeck: %s", r.err[0] ? r.err : curl_easy_strerror(r.curl));
        mutexLock(&g_mx);
        g_fail++;
        g_jobs.clear();
        mutexUnlock(&g_mx);
        return;
    }
    if (r.http == 404) {
        mutexLock(&g_mx);
        g_fail++;
        mutexUnlock(&g_mx);
        return;
    }
    if (apply_http(r, "uDeck") != 0) {
        mutexLock(&g_mx);
        g_fail++;
        if (g_error[0]) {
            g_jobs.clear();
        }
        mutexUnlock(&g_mx);
        return;
    }
    auto j = nlohmann::json::parse(r.body.c_str(), r.body.c_str() + r.body.size(), nullptr, false);
    if (j.is_discarded() || !j.contains("url") || !j["url"].is_string()) {
        mutexLock(&g_mx);
        g_fail++;
        mutexUnlock(&g_mx);
        return;
    }
    std::string art = j["url"].get<std::string>();
    if (art.empty()) {
        mutexLock(&g_mx);
        g_fail++;
        mutexUnlock(&g_mx);
        return;
    }
    set_phase("Downloading");
    if (http_to_file(art, path) == 0) {
        mutexLock(&g_mx);
        CacheEnt *e = ensure_ent(job.app_id);
        unload_ent(e);
        g_done++;
        mutexUnlock(&g_mx);
    } else {
        mutexLock(&g_mx);
        g_fail++;
        mutexUnlock(&g_mx);
    }
}

void fetch_one(const Job &job)
{
    char path[96];
    cache_path(job.app_id, job.fmt, path, sizeof(path));
    drop_legacy(job.app_id);
    if (file_ok(path)) {
        mutexLock(&g_mx);
        g_done++;
        mutexUnlock(&g_mx);
        return;
    }
    if (g_src == DH_COVER_UDECK) {
        fetch_udeck(job);
        return;
    }
    if (!g_key[0] || !job.name[0]) {
        mutexLock(&g_mx);
        g_fail++;
        mutexUnlock(&g_mx);
        return;
    }
    set_current(job.name);
    int auth = 0;
    int gid = resolve_gid(job.app_id, job.name, &auth);
    if (auth) {
        mutexLock(&g_mx);
        g_jobs.clear();
        mutexUnlock(&g_mx);
        return;
    }
    if (gid <= 0) {
        mutexLock(&g_mx);
        g_fail++;
        if (g_error[0]) {
            g_jobs.clear();
        }
        mutexUnlock(&g_mx);
        return;
    }
    std::vector<ArtCand> arts;
    if (list_art(gid, job.fmt, arts, &auth) != 0) {
        if (auth) {
            mutexLock(&g_mx);
            g_jobs.clear();
            mutexUnlock(&g_mx);
        } else {
            mutexLock(&g_mx);
            g_fail++;
            mutexUnlock(&g_mx);
        }
        return;
    }
    if (arts.empty()) {
        mutexLock(&g_mx);
        g_fail++;
        mutexUnlock(&g_mx);
        return;
    }
    set_phase("Downloading");
    if (http_to_file(arts[0].url, path) == 0) {
        mutexLock(&g_mx);
        CacheEnt *e = ensure_ent(job.app_id);
        unload_ent(e);
        e->sgdb_id = gid;
        g_done++;
        mutexUnlock(&g_mx);
    } else {
        mutexLock(&g_mx);
        g_fail++;
        mutexUnlock(&g_mx);
    }
}

void run_easyapi(const Job &job)
{
    set_phase("Importing EasyAPI");
    set_current(job.name);
    clear_err();
    if (!job.name[0]) {
        set_err("Enter a 4-character code");
        set_phase("");
        return;
    }
    nlohmann::json req;
    req["code"] = std::string(job.name);
    std::string payload = req.dump();
    HttpRes r = http_post_json(DH_EASYAPI_URL, payload);
    if (r.curl != CURLE_OK) {
        set_err("EasyAPI: %s", r.err[0] ? r.err : curl_easy_strerror(r.curl));
        set_phase("");
        return;
    }
    if (r.http == 404) {
        set_err("Code expired or already used");
        set_phase("");
        return;
    }
    if (r.http >= 400) {
        set_err("EasyAPI: HTTP %ld", r.http);
        set_phase("");
        return;
    }
    auto j = nlohmann::json::parse(r.body.c_str(), r.body.c_str() + r.body.size(), nullptr, false);
    if (j.is_discarded() || !j.contains("apiKey") || !j["apiKey"].is_string()) {
        set_err("EasyAPI: bad response");
        set_phase("");
        return;
    }
    std::string key = j["apiKey"].get<std::string>();
    while (!key.empty() && isspace((unsigned char)key.front())) {
        key.erase(key.begin());
    }
    while (!key.empty() && isspace((unsigned char)key.back())) {
        key.pop_back();
    }
    if (key.size() < 8) {
        set_err("EasyAPI: no key in response");
        set_phase("");
        return;
    }
    mutexLock(&g_mx);
    snprintf(g_key, sizeof(g_key), "%s", key.c_str());
    g_imported = 1;
    clear_err();
    set_phase("EasyAPI ok");
    set_current("");
    mutexUnlock(&g_mx);
    dlog("sgdb", "EasyAPI imported key_len=%u", (unsigned)key.size());
}

void run_test(void)
{
    set_phase("Testing API key");
    set_current("Mario Kart");
    clear_err();
    HttpRes r = http_get("https://www.steamgriddb.com/api/v2/search/autocomplete/Mario", 1);
    int ae = apply_http(r, "Test");
    if (ae == 0) {
        auto j = nlohmann::json{};
        if (parse_api(r, j, "Test") == 0) {
            mutexLock(&g_mx);
            clear_err();
            set_phase("Key works");
            set_current("");
            mutexUnlock(&g_mx);
            return;
        }
        return;
    }
}

void run_pick(const Job &job)
{
    int gen = g_pick_gen;
    int fmt = job.fmt;
    set_phase("Searching");
    set_current(job.name);
    mutexLock(&g_mx);
    g_pick_n = 0;
    g_pick_ready = 0;
    mutexUnlock(&g_mx);
    int auth = 0;
    int gid = resolve_gid(job.app_id, job.name, &auth);
    if (auth || gid <= 0) {
        if (!auth && !g_error[0]) {
            set_err("No SteamGridDB match");
        }
        mutexLock(&g_mx);
        if (g_pick_gen == gen) {
            g_pick_ready = 1;
        }
        mutexUnlock(&g_mx);
        return;
    }
    std::vector<ArtCand> arts;
    if (list_art(gid, fmt, arts, &auth) != 0) {
        mutexLock(&g_mx);
        if (g_pick_gen == gen) {
            g_pick_ready = 1;
        }
        mutexUnlock(&g_mx);
        return;
    }
    int n = (int)arts.size();
    if (n > DH_SGDB_PICK_MAX) {
        n = DH_SGDB_PICK_MAX;
    }
    mutexLock(&g_mx);
    if (g_pick_gen != gen) {
        mutexUnlock(&g_mx);
        return;
    }
    for (int i = 0; i < n; i++) {
        snprintf(g_pick[i].url, sizeof(g_pick[i].url), "%s", arts[i].url.c_str());
        snprintf(g_pick[i].thumb, sizeof(g_pick[i].thumb), "%s", arts[i].thumb.c_str());
        snprintf(g_pick[i].path, sizeof(g_pick[i].path), DH_SGDB_PICK_DIR "/%d.bin", i);
        fmt_label(g_pick[i].label, sizeof(g_pick[i].label), fmt, arts[i].w, arts[i].h);
        g_pick[i].loaded = 0;
        g_pick[i].tex = nullptr;
        remove(g_pick[i].path);
    }
    g_pick_n = n;
    g_pick_ready = 1;
    set_phase(n ? "Choose a cover" : "No covers");
    mutexUnlock(&g_mx);
    if (!n && !g_error[0]) {
        set_err(is_square(fmt) ? "No 1:1 icons" : "No capsule grids");
        return;
    }
    for (int i = 0; i < n; i++) {
        if (g_stop || g_pick_gen != gen) {
            break;
        }
        char path[96];
        char src[320];
        mutexLock(&g_mx);
        snprintf(path, sizeof(path), "%s", g_pick[i].path);
        snprintf(src, sizeof(src), "%s", g_pick[i].thumb[0] ? g_pick[i].thumb : g_pick[i].url);
        mutexUnlock(&g_mx);
        http_to_file(src, path);
    }
}

void run_apply(const Job &job)
{
    if (!job.url[0]) {
        return;
    }
    char dest[96];
    cache_path(job.app_id, job.fmt, dest, sizeof(dest));
    drop_legacy(job.app_id);
    set_phase("Saving cover");
    if (http_to_file(job.url, dest) != 0) {
        return;
    }
    mutexLock(&g_mx);
    CacheEnt *e = ensure_ent(job.app_id);
    unload_ent(e);
    g_done++;
    mutexUnlock(&g_mx);
}

void worker(void *)
{
    for (;;) {
        mutexLock(&g_mx);
        if (g_stop) {
            mutexUnlock(&g_mx);
            break;
        }
        Job job{};
        int have = 0;
        if (!g_jobs.empty()) {
            job = g_jobs.front();
            g_jobs.erase(g_jobs.begin());
            g_busy = 1;
            have = 1;
        } else {
            g_busy = 0;
            set_phase("");
            set_current("");
        }
        mutexUnlock(&g_mx);
        if (!have) {
            svcSleepThread(120'000'000ull);
            continue;
        }
        if (!ensure_net()) {
            continue;
        }
        if (job.kind == JOB_TEST) {
            run_test();
        } else if (job.kind == JOB_EASYAPI) {
            run_easyapi(job);
        } else if (job.kind == JOB_PICK) {
            run_pick(job);
        } else if (job.kind == JOB_APPLY) {
            run_apply(job);
        } else {
            fetch_one(job);
        }
    }
}

void ensure_worker(void)
{
    if (!g_mx_ok) {
        mutexInit(&g_mx);
        g_mx_ok = 1;
    }
    ensure_dir();
    index_load();
    ensure_net();
    if (!g_th_ok) {
        g_stop = 0;
        if (R_SUCCEEDED(threadCreate(&g_th, worker, nullptr, nullptr, 0x100000, 0x2C, -2))
            && R_SUCCEEDED(threadStart(&g_th))) {
            g_th_ok = 1;
        } else {
            set_err("Could not start network thread");
        }
    }
}

void push_job(const Job &job, int front)
{
    mutexLock(&g_mx);
    if (front) {
        g_jobs.insert(g_jobs.begin(), job);
    } else {
        g_jobs.push_back(job);
    }
    mutexUnlock(&g_mx);
}

int already_queued(u64 app_id, int fmt)
{
    for (const Job &j : g_jobs) {
        if (j.kind == JOB_FETCH && j.app_id == app_id && j.fmt == fmt) {
            return 1;
        }
    }
    return 0;
}

void queue_missing(int reset_counts, int skip_existing)
{
    int fmt = g_fmt;
    if (reset_counts) {
        g_done = 0;
        g_fail = 0;
        g_total = 0;
    }
    for (size_t i = 0; i < g_jobs.size();) {
        if (g_jobs[i].kind == JOB_FETCH && (reset_counts || g_jobs[i].fmt != fmt)) {
            g_jobs.erase(g_jobs.begin() + (std::ptrdiff_t)i);
        } else {
            i++;
        }
    }
    int n = dh_titles_count();
    for (int i = 0; i < n; i++) {
        DhTitle *t = dh_titles_get(i);
        if (!t) {
            continue;
        }
        drop_legacy(t->application_id);
        char path[96];
        cache_path(t->application_id, fmt, path, sizeof(path));
        if (!skip_existing && file_ok(path)) {
            remove(path);
        }
        if (skip_existing && file_ok(path)) {
            if (reset_counts) {
                g_done++;
                g_total++;
            }
            continue;
        }
        if (already_queued(t->application_id, fmt)) {
            continue;
        }
        Job job{};
        job.kind = JOB_FETCH;
        job.fmt = fmt;
        job.app_id = t->application_id;
        clean_name(t->name, job.name, sizeof(job.name));
        snprintf(job.title_id, sizeof(job.title_id), "%s", t->id_hex);
        g_jobs.push_back(job);
        if (reset_counts) {
            g_total++;
        }
    }
}

} // namespace

extern "C" void dh_sgdb_apply(const DhSettings *s)
{
    if (!g_mx_ok) {
        mutexInit(&g_mx);
        g_mx_ok = 1;
    }
    int prev = g_fmt;
    g_on = 0;
    g_fmt = s ? s->games.format : DH_FMT_CAPSULE;
    g_src = s ? s->cover_src : DH_COVER_UDECK;
    mutexLock(&g_mx);
    if (g_imported && g_key[0]) {
        /* Keep the EasyAPI key until the UI persists it over settings. */
    } else if (s && s->sgdb_key[0]) {
        snprintf(g_key, sizeof(g_key), "%s", s->sgdb_key);
    } else {
        g_key[0] = 0;
    }
    mutexUnlock(&g_mx);
    index_load();
    if (prev != g_fmt) {
        unload_all_tex();
    }
}

extern "C" void dh_sgdb_exit(void)
{
    if (!g_mx_ok) {
        return;
    }
    mutexLock(&g_mx);
    g_stop = 1;
    mutexUnlock(&g_mx);
    if (g_th_ok) {
        threadWaitForExit(&g_th);
        threadClose(&g_th);
        g_th_ok = 0;
    }
    dh_sgdb_pick_clear();
    unload_all_tex();
    g_cache.clear();
    g_jobs.clear();
    curl_drop_lists();
    if (g_easy) {
        curl_easy_cleanup(g_easy);
        g_easy = nullptr;
    }
    if (g_curl_ok) {
        curl_global_cleanup();
        g_curl_ok = 0;
    }
    if (g_ssl_ok) {
        sslExit();
        g_ssl_ok = 0;
    }
    if (g_sock_ok) {
        socketExit();
        g_sock_ok = 0;
    }
}

extern "C" void dh_sgdb_enqueue_all(int skip_existing)
{
    if (g_src != DH_COVER_UDECK && !g_key[0]) {
        set_err("Set an API key first");
        return;
    }
    ensure_worker();
    mutexLock(&g_mx);
    clear_err();
    set_phase("Queued");
    queue_missing(1, skip_existing);
    mutexUnlock(&g_mx);
}

extern "C" void dh_sgdb_hint_visible(const u64 *ids, int n)
{
    if (!ids || n <= 0) {
        return;
    }
    if (n > (int)(sizeof(g_hint) / sizeof(g_hint[0]))) {
        n = (int)(sizeof(g_hint) / sizeof(g_hint[0]));
    }
    if (!g_mx_ok) {
        memcpy(g_hint, ids, (size_t)n * sizeof(u64));
        g_hint_n = n;
        return;
    }
    mutexLock(&g_mx);
    memcpy(g_hint, ids, (size_t)n * sizeof(u64));
    g_hint_n = n;
    std::vector<Job> pri;
    std::vector<Job> vis;
    std::vector<Job> rest;
    pri.reserve(g_jobs.size());
    vis.reserve(g_jobs.size());
    rest.reserve(g_jobs.size());
    for (const Job &j : g_jobs) {
        if (j.kind != JOB_FETCH) {
            pri.push_back(j);
            continue;
        }
        int hit = 0;
        for (int i = 0; i < n; i++) {
            if (ids[i] == j.app_id) {
                hit = 1;
                break;
            }
        }
        if (hit) {
            vis.push_back(j);
        } else {
            rest.push_back(j);
        }
    }
    g_jobs.clear();
    g_jobs.insert(g_jobs.end(), pri.begin(), pri.end());
    g_jobs.insert(g_jobs.end(), vis.begin(), vis.end());
    g_jobs.insert(g_jobs.end(), rest.begin(), rest.end());
    mutexUnlock(&g_mx);
}

extern "C" void dh_sgdb_pump(SDL_Renderer *r)
{
    if (!r) {
        return;
    }
    mutexLock(&g_mx);
    int thumbs = 0;
    for (int i = 0; i < g_pick_n && thumbs < 4; i++) {
        if (g_pick[i].loaded || !g_pick[i].path[0] || !file_ok(g_pick[i].path)) {
            continue;
        }
        mutexUnlock(&g_mx);
        SDL_Texture *tex = IMG_LoadTexture(r, g_pick[i].path);
        mutexLock(&g_mx);
        if (tex) {
            SDL_QueryTexture(tex, nullptr, nullptr, &g_pick[i].w, &g_pick[i].h);
            g_pick[i].tex = tex;
        }
        g_pick[i].loaded = 1;
        thumbs++;
    }
    mutexUnlock(&g_mx);
    u64 hint[64];
    int hint_n = 0;
    if (g_mx_ok) {
        mutexLock(&g_mx);
        hint_n = g_hint_n;
        if (hint_n > 64) {
            hint_n = 64;
        }
        memcpy(hint, g_hint, (size_t)hint_n * sizeof(u64));
        mutexUnlock(&g_mx);
    }
    int loaded = 0;
    auto try_load = [&](u64 app_id) {
        if (loaded >= 12) {
            return;
        }
        CacheEnt *e = ensure_ent(app_id);
        if (e->loaded && e->fmt == g_fmt) {
            return;
        }
        if (e->loaded && e->fmt != g_fmt) {
            unload_ent(e);
        }
        char path[96];
        cache_path(app_id, g_fmt, path, sizeof(path));
        if (!file_ok(path)) {
            return;
        }
        SDL_Texture *tex = IMG_LoadTexture(r, path);
        if (tex) {
            SDL_QueryTexture(tex, nullptr, nullptr, &e->w, &e->h);
            e->tex = tex;
        }
        e->loaded = 1;
        e->fmt = g_fmt;
        loaded++;
    };
    for (int i = 0; i < hint_n; i++) {
        try_load(hint[i]);
    }
    int n = dh_titles_count();
    for (int i = 0; i < n && loaded < 12; i++) {
        DhTitle *t = dh_titles_get(i);
        if (t) {
            try_load(t->application_id);
        }
    }
}

extern "C" SDL_Texture *dh_sgdb_icon(u64 app_id, int *w, int *h)
{
    CacheEnt *e = find_ent(app_id);
    if (!e || !e->tex || e->fmt != g_fmt) {
        return nullptr;
    }
    if (w) {
        *w = e->w;
    }
    if (h) {
        *h = e->h;
    }
    return e->tex;
}

extern "C" int dh_sgdb_busy(void)
{
    if (!g_mx_ok) {
        return 0;
    }
    mutexLock(&g_mx);
    int b = g_busy || !g_jobs.empty();
    mutexUnlock(&g_mx);
    return b;
}

extern "C" int dh_sgdb_done_count(void)
{
    int n = 0;
    int t = dh_titles_count();
    for (int i = 0; i < t; i++) {
        DhTitle *e = dh_titles_get(i);
        if (!e) {
            continue;
        }
        char path[96];
        cache_path(e->application_id, g_fmt, path, sizeof(path));
        if (file_ok(path)) {
            n++;
        }
    }
    return n;
}

extern "C" int dh_sgdb_has_cover(u64 app_id)
{
    if (!app_id) {
        return 0;
    }
    char path[96];
    cache_path(app_id, g_fmt, path, sizeof(path));
    return file_ok(path);
}

extern "C" void dh_sgdb_status(DhSgdbStatus *out)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!g_mx_ok) {
        return;
    }
    mutexLock(&g_mx);
    out->busy = g_busy || !g_jobs.empty();
    out->queued = (int)g_jobs.size();
    out->done = g_done;
    out->total = g_total;
    out->fail = g_fail;
    out->pick_n = g_pick_n;
    out->pick_ready = g_pick_ready;
    snprintf(out->phase, sizeof(out->phase), "%s", g_phase);
    snprintf(out->current, sizeof(out->current), "%s", g_current);
    snprintf(out->error, sizeof(out->error), "%s", g_error);
    mutexUnlock(&g_mx);
}

extern "C" void dh_sgdb_test_key(void)
{
    if (!g_key[0]) {
        set_err("Set an API key first");
        return;
    }
    ensure_worker();
    mutexLock(&g_mx);
    clear_err();
    set_phase("Testing API key");
    mutexUnlock(&g_mx);
    Job job{};
    job.kind = JOB_TEST;
    snprintf(job.name, sizeof(job.name), "Mario");
    push_job(job, 1);
}

extern "C" void dh_sgdb_redeem_easyapi(const char *code)
{
    char norm[8];
    int n = 0;
    if (code) {
        for (const char *p = code; *p && n < 4; p++) {
            char c = *p;
            if (c >= 'a' && c <= 'z') {
                c = (char)(c - 32);
            }
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                norm[n++] = c;
            }
        }
    }
    norm[n] = 0;
    if (n != 4) {
        set_err("Enter a 4-character code");
        return;
    }
    ensure_worker();
    mutexLock(&g_mx);
    clear_err();
    set_phase("Importing EasyAPI");
    mutexUnlock(&g_mx);
    Job job{};
    job.kind = JOB_EASYAPI;
    snprintf(job.name, sizeof(job.name), "%s", norm);
    push_job(job, 1);
}

extern "C" int dh_sgdb_copy_key(char *out, size_t cap)
{
    if (!out || cap < 2) {
        return 0;
    }
    mutexLock(&g_mx);
    int ok = g_key[0] ? 1 : 0;
    snprintf(out, cap, "%s", g_key);
    mutexUnlock(&g_mx);
    return ok;
}

extern "C" int dh_sgdb_take_imported(char *out, size_t cap)
{
    if (!out || cap < 2 || !g_mx_ok) {
        return 0;
    }
    mutexLock(&g_mx);
    if (!g_imported || !g_key[0]) {
        mutexUnlock(&g_mx);
        return 0;
    }
    snprintf(out, cap, "%s", g_key);
    g_imported = 0;
    mutexUnlock(&g_mx);
    return 1;
}

extern "C" void dh_sgdb_pick_begin(u64 app_id, const char *name)
{
    ensure_worker();
    if (g_src == DH_COVER_UDECK) {
        Job job{};
        job.kind = JOB_FETCH;
        job.fmt = g_fmt;
        job.app_id = app_id;
        clean_name(name, job.name, sizeof(job.name));
        snprintf(job.title_id, sizeof(job.title_id), "%016llX", (unsigned long long)app_id);
        mutexLock(&g_mx);
        clear_err();
        set_phase("Searching");
        set_current(job.name);
        mutexUnlock(&g_mx);
        push_job(job, 1);
        return;
    }
    if (!g_key[0]) {
        set_err("Set an API key first");
        return;
    }
    ensure_worker();
    mutexLock(&g_mx);
    g_pick_gen++;
    for (size_t i = 0; i < g_jobs.size();) {
        if (g_jobs[i].kind == JOB_PICK) {
            g_jobs.erase(g_jobs.begin() + (std::ptrdiff_t)i);
        } else {
            i++;
        }
    }
    mutexUnlock(&g_mx);
    dh_sgdb_pick_clear();
    Job job{};
    job.kind = JOB_PICK;
    job.fmt = g_fmt;
    job.app_id = app_id;
    clean_name(name, job.name, sizeof(job.name));
    mutexLock(&g_mx);
    clear_err();
    g_pick_ready = 0;
    g_pick_n = 0;
    set_phase("Searching");
    set_current(job.name);
    mutexUnlock(&g_mx);
    push_job(job, 1);
}

extern "C" SDL_Texture *dh_sgdb_pick_icon(int i, int *w, int *h)
{
    if (i < 0 || i >= DH_SGDB_PICK_MAX || !g_pick[i].tex) {
        return nullptr;
    }
    if (w) {
        *w = g_pick[i].w;
    }
    if (h) {
        *h = g_pick[i].h;
    }
    return g_pick[i].tex;
}

extern "C" const char *dh_sgdb_pick_label(int i)
{
    if (i < 0 || i >= DH_SGDB_PICK_MAX) {
        return "";
    }
    return g_pick[i].label;
}

extern "C" int dh_sgdb_pick_apply(int i, u64 app_id)
{
    if (i < 0 || i >= g_pick_n || !g_pick[i].url[0]) {
        return -1;
    }
    Job job{};
    job.kind = JOB_APPLY;
    job.fmt = g_fmt;
    job.app_id = app_id;
    snprintf(job.url, sizeof(job.url), "%s", g_pick[i].url);
    mutexLock(&g_mx);
    g_pick_gen++;
    mutexUnlock(&g_mx);
    push_job(job, 1);
    CacheEnt *e = ensure_ent(app_id);
    unload_ent(e);
    return 0;
}

extern "C" void dh_sgdb_pick_clear(void)
{
    if (g_mx_ok) {
        mutexLock(&g_mx);
        g_pick_gen++;
        mutexUnlock(&g_mx);
    } else {
        g_pick_gen++;
    }
    for (int i = 0; i < DH_SGDB_PICK_MAX; i++) {
        if (g_pick[i].tex) {
            SDL_DestroyTexture(g_pick[i].tex);
        }
        g_pick[i] = PickEnt{};
    }
    g_pick_n = 0;
    g_pick_ready = 0;
}

extern "C" void dh_sgdb_reset_all(void)
{
    if (!g_mx_ok) {
        mutexInit(&g_mx);
        g_mx_ok = 1;
    }
    dh_sgdb_pick_clear();
    mutexLock(&g_mx);
    for (size_t i = 0; i < g_jobs.size();) {
        if (g_jobs[i].kind == JOB_FETCH || g_jobs[i].kind == JOB_APPLY) {
            g_jobs.erase(g_jobs.begin() + (std::ptrdiff_t)i);
        } else {
            i++;
        }
    }
    unload_all_tex();
    g_cache.clear();
    g_ids.clear();
    g_done = 0;
    g_fail = 0;
    g_total = 0;
    g_index_ok = 0;
    clear_err();
    set_phase("");
    set_current("");
    mutexUnlock(&g_mx);
    remove(DH_SGDB_INDEX);
    remove(DH_SGDB_MAP);
    int n = dh_titles_count();
    for (int i = 0; i < n; i++) {
        DhTitle *t = dh_titles_get(i);
        if (!t) {
            continue;
        }
        char path[96];
        cache_path(t->application_id, DH_FMT_SQUARE, path, sizeof(path));
        remove(path);
        cache_path(t->application_id, DH_FMT_CAPSULE, path, sizeof(path));
        remove(path);
    }
    for (int i = 0; i < DH_SGDB_PICK_MAX; i++) {
        char path[96];
        snprintf(path, sizeof(path), DH_SGDB_PICK_DIR "/%d.bin", i);
        remove(path);
    }
}
