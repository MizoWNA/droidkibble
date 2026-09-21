/*
 * phoneserverd: the small supervisor that stands in for the parts of Android that go away when
 * the Android UI (zygote / system_server) is stopped in headless mode.
 *
 *   - feeds the second hardware watchdog (/dev/watchdog1) that Samsung's system_server normally
 *     feeds; without it the phone resets about 100 s after the UI stops
 *   - renews the Wi-Fi DHCP lease (the framework normally does that), with its own tiny DHCP client
 *   - watches the connection to the router and, if it stays down, brings the Android UI back
 *   - writes a live status file and a log
 *   - runs the on-screen display (display/) and toggles it with the power button
 *
 * It runs outside the chroot, as root, and is built statically (see Makefile and
 * scripts/pc/build-daemon.sh). Single-threaded: nothing in the main loop may block for long,
 * because feeding the watchdog is the one job that must never stall.
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <linux/input.h>
#include <linux/watchdog.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define VERSION "1.0.0"

/* ---- tuning ------------------------------------------------------------------------------ */
#define WDT_FEED_EVERY_S      5      /* how often to pet the watchdog */
#define WDT_TIMEOUT_S         60     /* what we ask the driver for (it may ignore this) */
#define STATS_EVERY_S         5
#define PING_EVERY_S          10
#define SUMMARY_EVERY_S       300    /* one line into the log */
#define DOWN_RESTORE_AFTER_S  300    /* router unreachable this long -> bring the UI back */
#define STABLE_AFTER_S        600    /* after this long we consider the headless session healthy */
#define DHCP_FIRST_AFTER_S    10
#define DHCP_MIN_RENEW_S      60
#define DHCP_MAX_RENEW_S      (6 * 3600)
#define DHCP_NAKS_TO_RESTORE  3
#define LOG_MAX_BYTES         (256 * 1024)
#define DISP_BRIGHTNESS       120    /* of the panel's max (365 on the test phone) */
#define DISP_TIMEOUT_S        600    /* the screen turns itself off after this long; 0 = never */

/* ---- config and state -------------------------------------------------------------------- */
struct cfg {
    const char *dir;          /* state directory */
    char iface[IFNAMSIZ];
    const char *wdt_dev;
    int use_wdt, use_dhcp, use_restore, foreground, once;
    int use_display, disp_brightness, disp_timeout;
    char disp_theme[16];
    const char *backlight;
} cfg = { "/data/adb/phoneserver", "wlan0", "/dev/watchdog1", 1, 1, 1, 0, 0,
          1, DISP_BRIGHTNESS, DISP_TIMEOUT_S, "paper", "/sys/class/backlight/panel" };

static volatile sig_atomic_t g_stop = 0, g_dump = 0;
static int64_t t_start;

struct status {
    int64_t wall, uptime_s;
    int bat_pct, bat_temp_tenths; char bat_state[24];
    long mem_total_mb, mem_used_mb, mem_avail_mb;
    char ip[16], gateway[16]; int link_up;
    int gw_ok; int64_t down_for_s;
    int ssh_root, ssh_arch;
};
static struct status st;

static int64_t mono(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec; }

/* ---- logging ----------------------------------------------------------------------------- */
static void logmsg(const char *lvl, const char *fmt, ...) {
    char path[300], line[512], ts[32];
    va_list ap; va_start(ap, fmt); vsnprintf(line, sizeof line, fmt, ap); va_end(ap);
    time_t w = time(NULL); struct tm tm; gmtime_r(&w, &tm);
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tm);
    snprintf(path, sizeof path, "%s/phoneserverd.log", cfg.dir);
    struct stat s;
    if (stat(path, &s) == 0 && s.st_size > LOG_MAX_BYTES) {
        char old[310]; snprintf(old, sizeof old, "%s.1", path); rename(path, old);
    }
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%s UTC %-5s %s\n", ts, lvl, line); fclose(f); }
    if (cfg.foreground) fprintf(stderr, "%s %-5s %s\n", ts, lvl, line);
}

/* ---- small helpers ----------------------------------------------------------------------- */
static int read_str(const char *path, char *buf, size_t n) {
    int fd = open(path, O_RDONLY | O_CLOEXEC); if (fd < 0) return -1;
    ssize_t r = read(fd, buf, n - 1); close(fd); if (r < 0) return -1;
    while (r > 0 && (buf[r - 1] == '\n' || buf[r - 1] == '\r')) r--;
    buf[r] = 0; return 0;
}
static long read_long(const char *path, long def) {
    char b[64]; return read_str(path, b, sizeof b) == 0 ? strtol(b, NULL, 10) : def;
}
static int is_ipv4(const char *s) { struct in_addr a; return s && inet_pton(AF_INET, s, &a) == 1; }
static int valid_iface(const char *s) {
    if (!*s || strlen(s) >= IFNAMSIZ) return 0;
    for (; *s; s++) if (!(*s == '_' || *s == '-' || (*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z'))) return 0;
    return 1;
}

/* Run a program (no shell) and capture its output; blocking, bounded by timeout_s. */
static int run_capture(char *const argv[], char *out, size_t n, int timeout_s) {
    int p[2]; if (pipe(p)) return -1;
    pid_t pid = fork();
    if (pid < 0) { close(p[0]); close(p[1]); return -1; }
    if (pid == 0) { dup2(p[1], 1); dup2(p[1], 2); close(p[0]); close(p[1]); execv(argv[0], argv); _exit(127); }
    close(p[1]);
    size_t len = 0; int64_t deadline = mono() + timeout_s;
    while (len + 1 < n) {
        struct pollfd pf = { p[0], POLLIN, 0 };
        int left = (int)(deadline - mono()) * 1000; if (left <= 0) break;
        if (poll(&pf, 1, left) <= 0) break;
        ssize_t r = read(p[0], out + len, n - 1 - len); if (r <= 0) break; len += (size_t)r;
    }
    out[len] = 0; close(p[0]); kill(pid, SIGKILL); waitpid(pid, NULL, 0);
    return (int)len;
}

/* Start a script detached from us (survives our exit). */
static void run_detached(const char *script, const char *arg) {
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        int nul = open("/dev/null", O_RDWR); dup2(nul, 0); dup2(nul, 1); dup2(nul, 2);
        execl("/system/bin/sh", "sh", script, arg, (char *)NULL); _exit(127);
    }
}

/* ---- watchdog ---------------------------------------------------------------------------- */
static int wdt_fd = -1; static int64_t wdt_last_feed, wdt_warned;

static void wdt_feed(void) {
    if (wdt_fd < 0) return;
    if (ioctl(wdt_fd, WDIOC_KEEPALIVE, 0) != 0 && write(wdt_fd, "k", 1) != 1) { logmsg("ERROR", "watchdog feed failed: %s", strerror(errno)); return; }
    wdt_last_feed = mono();
}
static void wdt_tick(void) {
    if (!cfg.use_wdt) return;
    if (wdt_fd < 0) {                       /* system_server may still hold it: keep trying */
        wdt_fd = open(cfg.wdt_dev, O_WRONLY | O_CLOEXEC);
        if (wdt_fd < 0) {
            if (mono() - wdt_warned > 30) { wdt_warned = mono(); logmsg("WARN", "cannot open %s yet: %s", cfg.wdt_dev, strerror(errno)); }
            return;
        }
        int t = WDT_TIMEOUT_S; ioctl(wdt_fd, WDIOC_SETTIMEOUT, &t);
        int cur = 0; ioctl(wdt_fd, WDIOC_GETTIMEOUT, &cur);
        logmsg("INFO", "watchdog %s opened, timeout %d s", cfg.wdt_dev, cur);
        wdt_feed(); return;
    }
    if (mono() - wdt_last_feed >= WDT_FEED_EVERY_S) wdt_feed();
}
static void wdt_release(void) {
    if (wdt_fd < 0) return;
    if (write(wdt_fd, "V", 1) != 1) { /* magic close; ignored by nowayout drivers */ }
    close(wdt_fd); wdt_fd = -1; logmsg("INFO", "watchdog released");
}

/* ---- system status ----------------------------------------------------------------------- */
static int port_listening(int port) {
    static const char *files[] = { "/proc/net/tcp", "/proc/net/tcp6" };
    for (int i = 0; i < 2; i++) {
        FILE *f = fopen(files[i], "r"); if (!f) continue;
        char l[256]; int found = 0;
        if (!fgets(l, sizeof l, f)) { fclose(f); continue; }
        while (fgets(l, sizeof l, f)) {
            unsigned lp, state;
            if (sscanf(l, " %*d: %*[0-9A-Fa-f]:%x %*[0-9A-Fa-f]:%*x %x", &lp, &state) == 2 && state == 0x0A && (int)lp == port) { found = 1; break; }
        }
        fclose(f); if (found) return 1;
    }
    return 0;
}
static int iface_ipv4(char *out, size_t n) {
    struct ifaddrs *ifa, *p; int ok = 0; out[0] = 0;
    if (getifaddrs(&ifa)) return 0;
    for (p = ifa; p; p = p->ifa_next)
        if (p->ifa_addr && p->ifa_addr->sa_family == AF_INET && !strcmp(p->ifa_name, cfg.iface)) {
            inet_ntop(AF_INET, &((struct sockaddr_in *)p->ifa_addr)->sin_addr, out, (socklen_t)n); ok = 1; break;
        }
    freeifaddrs(ifa); return ok;
}
static void collect(void) {
    char p[128], b[64]; struct status *s = &st;
    s->wall = time(NULL);
    s->uptime_s = (int64_t)read_long("/proc/uptime", 0);       /* leading integer part */
    s->bat_pct = (int)read_long("/sys/class/power_supply/battery/capacity", -1);
    s->bat_temp_tenths = (int)read_long("/sys/class/power_supply/battery/temp", -1000);
    if (read_str("/sys/class/power_supply/battery/status", s->bat_state, sizeof s->bat_state)) strcpy(s->bat_state, "unknown");
    FILE *f = fopen("/proc/meminfo", "r"); long tot = 0, av = 0;
    if (f) { char l[128]; while (fgets(l, sizeof l, f)) { sscanf(l, "MemTotal: %ld", &tot); sscanf(l, "MemAvailable: %ld", &av); } fclose(f); }
    s->mem_total_mb = tot / 1024; s->mem_avail_mb = av / 1024; s->mem_used_mb = (tot - av) / 1024;
    snprintf(p, sizeof p, "/sys/class/net/%s/operstate", cfg.iface);
    s->link_up = read_str(p, b, sizeof b) == 0 && !strcmp(b, "up");
    if (!iface_ipv4(s->ip, sizeof s->ip)) s->ip[0] = 0;
    s->ssh_root = port_listening(22); s->ssh_arch = port_listening(2222);
}

/* ---- gateway reachability ---------------------------------------------------------------- */
static void detect_gateway(void) {
    char out[1024]; char *argv[] = { "/system/bin/ip", "route", "show", "table", cfg.iface, NULL };
    if (run_capture(argv, out, sizeof out, 5) <= 0) return;
    char *d = strstr(out, "default via "); if (!d) return;
    char gw[16]; if (sscanf(d, "default via %15s", gw) == 1 && is_ipv4(gw)) { strcpy(st.gateway, gw); logmsg("INFO", "gateway %s", gw); }
}
static int ping_once(const char *ip, int timeout_ms) {
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);          /* unprivileged ICMP echo socket */
    if (fd < 0) fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (fd < 0) return 0;
    setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, cfg.iface, (socklen_t)strlen(cfg.iface) + 1);
    struct sockaddr_in to = { .sin_family = AF_INET }; inet_pton(AF_INET, ip, &to.sin_addr);
    struct icmphdr h; memset(&h, 0, sizeof h); h.type = ICMP_ECHO; h.un.echo.id = (uint16_t)getpid(); h.un.echo.sequence = 1;
    uint32_t sum = 0; const uint16_t *w = (const uint16_t *)&h; for (size_t i = 0; i < sizeof h / 2; i++) sum += w[i];
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    h.checksum = (uint16_t)~sum;
    int ok = 0;
    if (sendto(fd, &h, sizeof h, 0, (struct sockaddr *)&to, sizeof to) == (ssize_t)sizeof h) {
        int64_t end = mono() * 1000 + timeout_ms;
        struct pollfd pf = { fd, POLLIN, 0 };
        if (poll(&pf, 1, timeout_ms) > 0) { char buf[128]; if (recv(fd, buf, sizeof buf, 0) > 0) ok = 1; }
        (void)end;
    }
    close(fd); return ok;
}

/* ---- DHCP renewal ------------------------------------------------------------------------ */
/* A small client that only refreshes the lease we already hold (DHCPREQUEST for our own address).
 * It never configures the interface: Android already did. (BusyBox udhcpc can't be used here: it
 * needs an ioctl that Android's SELinux policy denies, even for root.) */
struct bootp {
    uint8_t op, htype, hlen, hops; uint32_t xid; uint16_t secs, flags;
    uint32_t ciaddr, yiaddr, siaddr, giaddr; uint8_t chaddr[16], sname[64], file[128];
    uint32_t cookie; uint8_t opts[312];
} __attribute__((packed));

static struct {
    int fd, waiting, tries, fails, naks;
    uint32_t xid; int64_t sent_at, next_at, lease_s, last_ok_mono; int64_t last_ok_wall;
    char server[16];
} dh = { .fd = -1 };

static int read_mac(uint8_t mac[6]) {
    char p[96], b[32]; snprintf(p, sizeof p, "/sys/class/net/%s/address", cfg.iface);
    if (read_str(p, b, sizeof b)) return -1;
    unsigned m[6]; if (sscanf(b, "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6) return -1;
    for (int i = 0; i < 6; i++)
        mac[i] = (uint8_t)m[i];
    return 0;
}
static void dhcp_close(void) { if (dh.fd >= 0) { close(dh.fd); dh.fd = -1; } dh.waiting = 0; }
static void dhcp_schedule(int64_t in_s) { dh.next_at = mono() + in_s; }

static int dhcp_send(void) {
    uint8_t mac[6]; struct in_addr me;
    if (!st.ip[0] || inet_pton(AF_INET, st.ip, &me) != 1) return -1;
    if (read_mac(mac)) return -1;
    dhcp_close();
    dh.fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0); if (dh.fd < 0) return -1;
    int one = 1; setsockopt(dh.fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one); setsockopt(dh.fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof one);
    setsockopt(dh.fd, SOL_SOCKET, SO_BINDTODEVICE, cfg.iface, (socklen_t)strlen(cfg.iface) + 1);
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(68) };
    if (bind(dh.fd, (struct sockaddr *)&a, sizeof a)) { logmsg("WARN", "dhcp: bind :68 failed: %s", strerror(errno)); dhcp_close(); return -1; }

    struct bootp m; memset(&m, 0, sizeof m);
    m.op = 1; m.htype = 1; m.hlen = 6; dh.xid = (uint32_t)(mono() * 2654435761u) ^ (uint32_t)getpid(); m.xid = htonl(dh.xid);
    m.flags = htons(0x8000);                        /* ask for a broadcast reply: we have no raw socket */
    memcpy(m.chaddr, mac, 6); m.cookie = htonl(0x63825363);
    uint8_t *o = m.opts;
    *o++ = 53; *o++ = 1; *o++ = 3;                                  /* DHCPREQUEST */
    *o++ = 50; *o++ = 4; memcpy(o, &me, 4); o += 4;                 /* requested address = the one we have */
    *o++ = 55; *o++ = 6; *o++ = 1; *o++ = 3; *o++ = 6; *o++ = 51; *o++ = 54; *o++ = 58;
    *o++ = 255;
    struct sockaddr_in to = { .sin_family = AF_INET, .sin_port = htons(67) }; to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    if (sendto(dh.fd, &m, sizeof m, 0, (struct sockaddr *)&to, sizeof to) < 0) { logmsg("WARN", "dhcp: send failed: %s", strerror(errno)); dhcp_close(); return -1; }
    dh.waiting = 1; dh.sent_at = mono(); dh.tries++; return 0;
}
static void dhcp_reply(const struct bootp *m, size_t len) {
    if (len < 240 || m->op != 2 || ntohl(m->xid) != dh.xid || ntohl(m->cookie) != 0x63825363) return;
    int type = 0; uint32_t lease = 0, t1 = 0; char server[16] = "", router[16] = "";
    const uint8_t *o = m->opts, *end = (const uint8_t *)m + len;
    while (o < end) {
        uint8_t c = *o++; if (c == 255) break; if (c == 0) continue; if (o >= end) break;
        uint8_t l = *o++; if (o + l > end) break;
        if (c == 53 && l == 1) type = o[0];
        else if (c == 51 && l == 4) { memcpy(&lease, o, 4); lease = ntohl(lease); }
        else if (c == 58 && l == 4) { memcpy(&t1, o, 4); t1 = ntohl(t1); }
        else if (c == 54 && l == 4) inet_ntop(AF_INET, o, server, sizeof server);
        else if (c == 3 && l >= 4 && !router[0]) inet_ntop(AF_INET, o, router, sizeof router);
        o += l;
    }
    if (type == 5) {                                                    /* ACK */
        dh.naks = 0; dh.fails = 0; dh.lease_s = lease; dh.last_ok_wall = time(NULL); dh.last_ok_mono = mono();
        if (server[0]) strcpy(dh.server, server);
        if (router[0] && !st.gateway[0]) { strcpy(st.gateway, router); logmsg("INFO", "gateway %s (from DHCP)", router); }
        int64_t renew = t1 ? t1 : lease / 2;
        if (renew < DHCP_MIN_RENEW_S) renew = DHCP_MIN_RENEW_S;
        if (renew > DHCP_MAX_RENEW_S) renew = DHCP_MAX_RENEW_S;
        dhcp_schedule(renew);
        logmsg("INFO", "dhcp: lease renewed by %s, lease %u s, next renewal in %lld s", server, lease, (long long)renew);
        dhcp_close();
    } else if (type == 6) {                                             /* NAK */
        dh.naks++; logmsg("WARN", "dhcp: %s refused our address (NAK %d of %d)", server[0] ? server : "server", dh.naks, DHCP_NAKS_TO_RESTORE);
        dhcp_schedule(30); dhcp_close();
    }
}
static void dhcp_tick(void) {
    if (!cfg.use_dhcp) return;
    if (dh.waiting) {
        struct bootp m; ssize_t r;
        while ((r = recv(dh.fd, &m, sizeof m, 0)) > 0) { dhcp_reply(&m, (size_t)r); if (!dh.waiting) return; }
        if (mono() - dh.sent_at >= 5) {
            if (dh.tries < 3) { logmsg("WARN", "dhcp: no reply, retrying (%d)", dh.tries); if (dhcp_send()) { dhcp_close(); dhcp_schedule(60); } }
            else { dh.fails++; dh.tries = 0; logmsg("WARN", "dhcp: no reply after 3 tries (%d in a row); next attempt in 2 min", dh.fails); dhcp_close(); dhcp_schedule(120); }
        }
        return;
    }
    if (mono() >= dh.next_at && st.ip[0]) { dh.tries = 0; if (dhcp_send()) dhcp_schedule(60); }
}

/* ---- restore the Android UI if the network stays broken ---------------------------------- */
static int restore_started;
static void restore_ui(const char *why) {
    if (!cfg.use_restore || restore_started) return;
    restore_started = 1; logmsg("ERROR", "restoring the Android UI: %s", why);
    char path[300]; snprintf(path, sizeof path, "%s/ui.sh", cfg.dir);
    run_detached(path, "on");                     /* ui.sh stops us, then starts the framework */
}

/* ---- connectivity ------------------------------------------------------------------------ */
static int64_t down_since;
static void net_tick(void) {
    static int64_t last;
    if (mono() - last < PING_EVERY_S) return;
    last = mono();
    if (!st.gateway[0] && st.ip[0]) detect_gateway();
    int ok = st.link_up && st.ip[0] && st.gateway[0] && ping_once(st.gateway, 1500);
    if (!st.gateway[0]) ok = st.link_up && st.ip[0];              /* can't ping without a gateway; fall back to link state */
    st.gw_ok = ok;
    if (ok) { if (down_since) logmsg("INFO", "network back after %lld s", (long long)(mono() - down_since)); down_since = 0; st.down_for_s = 0; }
    else { if (!down_since) { down_since = mono(); logmsg("WARN", "network down (link %s, ip '%s', gateway '%s')", st.link_up ? "up" : "down", st.ip, st.gateway); } st.down_for_s = mono() - down_since; }
    if (st.down_for_s >= DOWN_RESTORE_AFTER_S) restore_ui("router unreachable for too long");
    if (dh.naks >= DHCP_NAKS_TO_RESTORE) restore_ui("DHCP server keeps refusing our address");
}

/* ---- one-line summaries (for the log and --once) ------------------------------------------ */
#define LINE 160
static int format_lines(char lines[][LINE], int max) {
    int n = 0; struct status *s = &st;
    if (n < max) {
        if (s->bat_pct < 0)
            snprintf(lines[n++], LINE, "battery n/a");
        else
            snprintf(lines[n++], LINE, "battery %d%%  %.24s  %d.%dC", s->bat_pct, s->bat_state, s->bat_temp_tenths / 10, abs(s->bat_temp_tenths % 10));
    }
    if (n < max) snprintf(lines[n++], LINE, "ram %ld/%ld MB  up %lldh%02lldm", s->mem_used_mb, s->mem_total_mb, (long long)(s->uptime_s / 3600), (long long)(s->uptime_s / 60 % 60));
    if (n < max) snprintf(lines[n++], LINE, "net %s %.15s gw %.15s", s->gw_ok ? "ok" : "DOWN", s->ip[0] ? s->ip : "-", s->gateway[0] ? s->gateway : "-");
    if (n < max) snprintf(lines[n++], LINE, "ssh root:%s arch:%s  dhcp lease %llds", s->ssh_root ? "up" : "DOWN", s->ssh_arch ? "up" : "DOWN", (long long)(dh.last_ok_mono ? dh.lease_s - (mono() - dh.last_ok_mono) : 0));
    return n;
}

/* ---- on-screen display -------------------------------------------------------------------- */
/* The picture is drawn by a small Java program (display/, run with app_process) through SurfaceFlinger,
 * which keeps running while the Android UI is off. We start it and stop it, and set the backlight.
 * The power button toggles it: with system_server gone nothing else reads that key. The screen stays
 * dark until the first press, and turns itself off again after cfg.disp_timeout seconds (OLED burn-in,
 * battery). See docs/how-it-works.md and display/README.md. */
#define MAX_KEYDEVS 8
static struct {
    int on;                   /* screen is meant to be lit */
    pid_t pid;                /* the display program's process group, or -1 */
    int64_t on_since, kill_at;
    int keys[MAX_KEYDEVS], nkeys;
    int64_t last_press_ms;
} dp = { 0, -1, 0, 0, { 0 }, 0, 0 };

static int64_t mono_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000; }

static int disp_files_ok(void) {
    char a[320], b[320];
    snprintf(a, sizeof a, "%s/display/run.sh", cfg.dir); snprintf(b, sizeof b, "%s/display/status.jar", cfg.dir);
    return access(a, R_OK) == 0 && access(b, R_OK) == 0;
}

static void set_backlight(int v) {
    char path[300], val[16];
    snprintf(path, sizeof path, "%s/brightness", cfg.backlight);
    int fd = open(path, O_WRONLY | O_CLOEXEC); if (fd < 0) return;
    int n = snprintf(val, sizeof val, "%d", v);
    if (write(fd, val, (size_t)n) < 0) logmsg("WARN", "cannot set the backlight: %s", strerror(errno));
    close(fd);
}

static int max_backlight(void) {
    char path[300]; snprintf(path, sizeof path, "%s/max_brightness", cfg.backlight);
    return (int)read_long(path, 255);
}

static void disp_on(void) {
    if (dp.on || !cfg.use_display) return;
    if (!disp_files_ok()) { logmsg("WARN", "no display files in %s/display (see display/README.md); nothing to show", cfg.dir); return; }
    pid_t pid = fork();
    if (pid < 0) { logmsg("ERROR", "display: fork failed: %s", strerror(errno)); return; }
    if (pid == 0) {
        char run[320], log[320];
        snprintf(run, sizeof run, "%s/display/run.sh", cfg.dir); snprintf(log, sizeof log, "%s/display.log", cfg.dir);
        setsid();
        int nul = open("/dev/null", O_RDONLY); if (nul >= 0) dup2(nul, 0);
        int lf = open(log, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (lf >= 0) { dup2(lf, 1); dup2(lf, 2); }
        setenv("NO_BACKLIGHT", "1", 1);            /* we own the backlight */
        execl("/system/bin/sh", "sh", run, "--theme", cfg.disp_theme, "--interval", "5", (char *)NULL);
        _exit(127);
    }
    int mx = max_backlight(), b = cfg.disp_brightness > mx ? mx : cfg.disp_brightness;
    dp.pid = pid; dp.on = 1; dp.on_since = mono(); dp.kill_at = 0;
    set_backlight(b);
    logmsg("INFO", "display on (theme %s, brightness %d/%d, auto-off %d s)", cfg.disp_theme, b, mx, cfg.disp_timeout);
}

static void disp_off(int hard) {
    if (!dp.on && dp.pid < 0) return;
    set_backlight(0);
    if (dp.pid > 0) { kill(-dp.pid, hard ? SIGKILL : SIGTERM); dp.kill_at = hard ? 0 : mono() + 3; if (hard) dp.pid = -1; }
    if (dp.on) logmsg("INFO", "display off");
    dp.on = 0;
}

static void disp_tick(void) {
    if (dp.pid > 0 && dp.kill_at && mono() >= dp.kill_at) {          /* it ignored SIGTERM */
        if (kill(dp.pid, 0) == 0) kill(-dp.pid, SIGKILL);
        dp.pid = -1; dp.kill_at = 0;
    }
    if (!dp.on) return;
    if (kill(dp.pid, 0) != 0) {                                        /* SIGCHLD is ignored, so a dead child is just gone */
        logmsg("WARN", "the display program exited on its own (see display.log); screen off");
        set_backlight(0); dp.on = 0; dp.pid = -1;
    } else if (cfg.disp_timeout > 0 && mono() - dp.on_since >= cfg.disp_timeout) {
        logmsg("INFO", "display auto-off after %d s", cfg.disp_timeout);
        disp_off(0);
    }
}

/* Find every input device that can send KEY_POWER (on the test phone: gpio_keys, event14). */
static void input_open(void) {
    for (int i = 0; i < 32 && dp.nkeys < MAX_KEYDEVS; i++) {
        char path[40]; snprintf(path, sizeof path, "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC); if (fd < 0) continue;
        unsigned char ev[EV_MAX / 8 + 1], key[KEY_MAX / 8 + 1]; memset(ev, 0, sizeof ev); memset(key, 0, sizeof key);
        if (ioctl(fd, EVIOCGBIT(0, sizeof ev), ev) < 0 || !(ev[EV_KEY / 8] & (1 << (EV_KEY % 8))) ||
            ioctl(fd, EVIOCGBIT(EV_KEY, sizeof key), key) < 0 || !(key[KEY_POWER / 8] & (1 << (KEY_POWER % 8)))) { close(fd); continue; }
        char name[64] = "?"; if (ioctl(fd, EVIOCGNAME(sizeof name), name) < 0) snprintf(name, sizeof name, "?");
        dp.keys[dp.nkeys++] = fd;
        logmsg("INFO", "power button: %s (%s)", path, name);
    }
    if (!dp.nkeys) logmsg("WARN", "no input device with a power key found; the display can't be toggled");
}

static void input_read(void) {
    struct input_event e[16];
    for (int k = 0; k < dp.nkeys; k++) {
        ssize_t n;
        while ((n = read(dp.keys[k], e, sizeof e)) > 0) {
            for (size_t j = 0; j < (size_t)n / sizeof e[0]; j++) {
                if (e[j].type != EV_KEY || e[j].code != KEY_POWER || e[j].value != 1) continue;
                int64_t now = mono_ms();
                if (now - dp.last_press_ms < 400) continue;             /* bounce */
                dp.last_press_ms = now;
                logmsg("INFO", "power button pressed");
                if (dp.on) disp_off(0); else disp_on();
            }
        }
        if (n < 0 && errno != EAGAIN && errno != EINTR) {               /* device went away */
            close(dp.keys[k]); dp.keys[k--] = dp.keys[--dp.nkeys];
        }
    }
}

/* ---- status file ------------------------------------------------------------------------- */
static void write_status(void) {
    char json[2560], tmp[300], path[300]; struct status *s = &st;
    int64_t fed = wdt_fd >= 0 ? mono() - wdt_last_feed : -1;
    int64_t next = cfg.use_dhcp && dh.next_at > mono() ? dh.next_at - mono() : 0;
    char tempbuf[16];
    if (s->bat_temp_tenths <= -1000) snprintf(tempbuf, sizeof tempbuf, "null");
    else snprintf(tempbuf, sizeof tempbuf, "%d.%d", s->bat_temp_tenths / 10, abs(s->bat_temp_tenths % 10));
    snprintf(json, sizeof json,
        "{\n \"version\": \"" VERSION "\",\n \"time\": %lld,\n \"uptime_s\": %lld,\n"
        " \"battery\": {\"percent\": %d, \"temp_c\": %s, \"state\": \"%s\"},\n"
        " \"memory\": {\"used_mb\": %ld, \"available_mb\": %ld, \"total_mb\": %ld},\n"
        " \"network\": {\"iface\": \"%s\", \"link_up\": %s, \"ip\": \"%s\", \"gateway\": \"%s\", \"gateway_reachable\": %s, \"down_for_s\": %lld},\n"
        " \"dhcp\": {\"enabled\": %s, \"last_ok\": %lld, \"lease_s\": %lld, \"next_renewal_in_s\": %lld, \"server\": \"%s\", \"naks\": %d, \"failures\": %d},\n"
        " \"services\": {\"ssh_android_22\": %s, \"ssh_chroot_2222\": %s},\n"
        " \"watchdog\": {\"enabled\": %s, \"device\": \"%s\", \"open\": %s, \"fed_ago_s\": %lld},\n"
        " \"display\": {\"available\": %s, \"on\": %s, \"auto_off_s\": %d, \"theme\": \"%s\"},\n"
        " \"restore_started\": %s\n}\n",
        (long long)s->wall, (long long)s->uptime_s, s->bat_pct, tempbuf, s->bat_state,
        s->mem_used_mb, s->mem_avail_mb, s->mem_total_mb,
        cfg.iface, s->link_up ? "true" : "false", s->ip, s->gateway, s->gw_ok ? "true" : "false", (long long)s->down_for_s,
        cfg.use_dhcp ? "true" : "false", (long long)dh.last_ok_wall, (long long)dh.lease_s, (long long)next, dh.server, dh.naks, dh.fails,
        s->ssh_root ? "true" : "false", s->ssh_arch ? "true" : "false",
        cfg.use_wdt ? "true" : "false", cfg.wdt_dev, wdt_fd >= 0 ? "true" : "false", (long long)fed,
        cfg.use_display && disp_files_ok() ? "true" : "false", dp.on ? "true" : "false", cfg.disp_timeout, cfg.disp_theme,
        restore_started ? "true" : "false");
    snprintf(tmp, sizeof tmp, "%s/status.json.tmp", cfg.dir); snprintf(path, sizeof path, "%s/status.json", cfg.dir);
    FILE *f = fopen(tmp, "w"); if (!f) return; fputs(json, f); fclose(f); rename(tmp, path);
}

/* ---- main -------------------------------------------------------------------------------- */
static void on_signal(int sig) { if (sig == SIGUSR1) g_dump = 1; else g_stop = 1; }

static void usage(void) {
    puts("phoneserverd " VERSION "\n"
         "usage: phoneserverd [options]\n"
         "  --dir DIR        state directory (default /data/adb/phoneserver)\n"
         "  --iface NAME     network interface (default wlan0)\n"
         "  --no-watchdog    don't feed /dev/watchdog1      (for testing beside another feeder)\n"
         "  --no-dhcp        don't renew the DHCP lease\n"
         "  --no-restore     never bring the Android UI back automatically\n"
         "  --no-display     don't manage the on-screen display or read the power button\n"
         "  --display-theme NAME       paper or night (default paper)\n"
         "  --display-brightness N     backlight level while the display is on (default 120)\n"
         "  --display-timeout SECONDS  screen turns itself off after this long, 0 = never (default 600)\n"
         "  --backlight DIR  backlight sysfs directory (default /sys/class/backlight/panel)\n"
         "  --foreground     also log to stderr\n"
         "  --once           collect status once, print it, and exit\n"
         "  --status         print the running daemon's status.json (and say if it looks stale)");
}

int main(int argc, char **argv) {
    int status_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--dir") && i + 1 < argc) cfg.dir = argv[++i];
        else if (!strcmp(argv[i], "--iface") && i + 1 < argc) { snprintf(cfg.iface, sizeof cfg.iface, "%s", argv[++i]); }
        else if (!strcmp(argv[i], "--watchdog-device") && i + 1 < argc) cfg.wdt_dev = argv[++i];
        else if (!strcmp(argv[i], "--no-watchdog")) cfg.use_wdt = 0;
        else if (!strcmp(argv[i], "--no-dhcp")) cfg.use_dhcp = 0;
        else if (!strcmp(argv[i], "--no-restore")) cfg.use_restore = 0;
        else if (!strcmp(argv[i], "--no-display")) cfg.use_display = 0;
        else if (!strcmp(argv[i], "--display-theme") && i + 1 < argc) snprintf(cfg.disp_theme, sizeof cfg.disp_theme, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--display-brightness") && i + 1 < argc) cfg.disp_brightness = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--display-timeout") && i + 1 < argc) cfg.disp_timeout = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--backlight") && i + 1 < argc) cfg.backlight = argv[++i];
        else if (!strcmp(argv[i], "--foreground")) cfg.foreground = 1;
        else if (!strcmp(argv[i], "--once")) cfg.once = 1;
        else if (!strcmp(argv[i], "--status")) status_mode = 1;
        else if (!strcmp(argv[i], "--version")) { puts("phoneserverd " VERSION); return 0; }
        else { usage(); return !strcmp(argv[i], "--help") ? 0 : 2; }
    }
    if (!valid_iface(cfg.iface)) { fprintf(stderr, "bad interface name\n"); return 2; }
    for (const char *c = cfg.disp_theme; *c; c++) if (*c < 'a' || *c > 'z') { fprintf(stderr, "bad theme name\n"); return 2; }
    if (cfg.disp_brightness < 1) cfg.disp_brightness = 1;
    if (cfg.disp_timeout < 0) cfg.disp_timeout = 0;

    if (status_mode) {
        char path[300], buf[4096]; snprintf(path, sizeof path, "%s/status.json", cfg.dir);
        struct stat sb; FILE *f = fopen(path, "r");
        if (!f || stat(path, &sb)) { fprintf(stderr, "no status file at %s (is the daemon running?)\n", path); return 1; }
        size_t n; while ((n = fread(buf, 1, sizeof buf, f)) > 0) fwrite(buf, 1, n, stdout);
        fclose(f);
        long age = (long)(time(NULL) - sb.st_mtime);
        if (age > 30) fprintf(stderr, "warning: status is %ld s old; the daemon may not be running\n", age);
        return 0;
    }

    if (cfg.once) {
        cfg.use_wdt = 0; cfg.use_dhcp = 0; cfg.use_restore = 0;
        collect(); if (!strcmp(cfg.iface, "lo")) { /* nothing to detect */ } else detect_gateway();
        st.gw_ok = st.gateway[0] ? ping_once(st.gateway, 1500) : 0;
        char l[8][LINE]; int n = format_lines(l, 8); for (int i = 0; i < n; i++) puts(l[i]);
        return 0;
    }

    char lockp[300]; snprintf(lockp, sizeof lockp, "%s/phoneserverd.lock", cfg.dir);
    int lk = open(lockp, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (lk < 0 || flock(lk, LOCK_EX | LOCK_NB)) { fprintf(stderr, "phoneserverd: already running, or %s not writable\n", cfg.dir); return 1; }
    { char pid[16]; int n = snprintf(pid, sizeof pid, "%d\n", getpid()); if (ftruncate(lk, 0) == 0 && write(lk, pid, (size_t)n) < 0) { /* best effort */ } }

    struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_handler = on_signal;
    sigaction(SIGTERM, &sa, NULL); sigaction(SIGINT, &sa, NULL); sigaction(SIGUSR1, &sa, NULL);
    signal(SIGPIPE, SIG_IGN); signal(SIGHUP, SIG_IGN); signal(SIGCHLD, SIG_IGN);

    t_start = mono(); dhcp_schedule(DHCP_FIRST_AFTER_S);
    logmsg("INFO", "phoneserverd " VERSION " started (iface %s, watchdog %s, dhcp %s)", cfg.iface, cfg.use_wdt ? cfg.wdt_dev : "off", cfg.use_dhcp ? "on" : "off");
    collect(); wdt_tick();
    if (cfg.use_display) { input_open(); set_backlight(0); }

    int64_t last_stats = 0, last_summary = mono(); int stable_marked = 0;
    while (!g_stop) {
        wdt_tick();
        dhcp_tick();
        if (mono() - last_stats >= STATS_EVERY_S || g_dump) {
            last_stats = mono(); g_dump = 0; collect(); net_tick(); write_status();
        } else net_tick();
        if (mono() - last_summary >= SUMMARY_EVERY_S) {
            last_summary = mono(); char l[8][LINE]; int n = format_lines(l, 8); char all[640] = "";
            for (int i = 0; i < n; i++) { strncat(all, i ? " | " : "", sizeof all - strlen(all) - 1); strncat(all, l[i], sizeof all - strlen(all) - 1); }
            logmsg("INFO", "%s", all);
        }
        if (!stable_marked && mono() - t_start >= STABLE_AFTER_S) {       /* boot-loop breaker: this session was healthy */
            char p[300]; snprintf(p, sizeof p, "%s/headless.pending", cfg.dir); unlink(p); stable_marked = 1;
            logmsg("INFO", "stable for %d s, cleared the headless.pending marker", STABLE_AFTER_S);
        }
        disp_tick();
        struct pollfd pf[MAX_KEYDEVS]; int np = 0;                        /* sleep up to 1 s, but wake for the power button */
        for (int i = 0; i < dp.nkeys; i++) { pf[np].fd = dp.keys[i]; pf[np].events = POLLIN; pf[np].revents = 0; np++; }
        if (poll(pf, (nfds_t)np, 1000) > 0) input_read();
    }
    logmsg("INFO", "stopping (signal)"); disp_off(1); write_status(); wdt_release(); dhcp_close();
    unlink(lockp); return 0;
}
