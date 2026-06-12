/*
 - Generates a random password on startup and prints it to stdout
 - Accepts up to 3 concurrent clients (each in its own thread)
 - Authenticates each client with the password before streaming data
 - Streams length-prefixed JSON payloads (4-byte big-endian length + JSON)

 Build:
   gcc -O2 -Wall -Wextra -Werror -pthread -o test_server test_server.c

 Run:
   ./test_server [--host 0.0.0.0] [--port 4242] [--interval 1000]
   (interval is in milliseconds, default 1000)
 */

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* ─────────────────────────────────────────────
 * Config / limits
 * ───────────────────────────────────────────── */

#define MAX_CLIENTS      3
#define PASSWORD_LEN     16
#define JSON_BUF_SIZE    (256 * 1024)   /* 256 KB should be ample */
#define RECV_BUF_SIZE    1024
#define DEFAULT_PORT     4242
#define DEFAULT_INTERVAL 1000           /* ms */
#define TOP_PROCS        10

static volatile sig_atomic_t g_stop = 0;
static char                  g_password[PASSWORD_LEN + 1];
static int                   g_interval_ms = DEFAULT_INTERVAL;

/* ─────────────────────────────────────────────
 * Signal handling
 * ───────────────────────────────────────────── */

static void handle_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

/* ─────────────────────────────────────────────
 * Password generation
 * ───────────────────────────────────────────── */

static void gen_password(char *out, int len)
{
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        /* fallback — not cryptographically strong, fine for a test server */
        srand((unsigned)time(NULL));
        for (int i = 0; i < len; i++)
            out[i] = charset[rand() % (sizeof(charset) - 1)];
    } else {
        unsigned char buf[PASSWORD_LEN];
        ssize_t _r = read(fd, buf, len); (void)_r;
        close(fd);
        for (int i = 0; i < len; i++)
            out[i] = charset[buf[i] % (sizeof(charset) - 1)];
    }
    out[len] = '\0';
}

/* ─────────────────────────────────────────────
 * Small JSON builder (append-only string buffer)
 * ───────────────────────────────────────────── */

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} jbuf_t;

static void jb_init(jbuf_t *j, char *buf, size_t cap)
{
    j->buf = buf;
    j->len = 0;
    j->cap = cap;
    buf[0] = '\0';
}

static void jb_append(jbuf_t *j, const char *fmt, ...)
{
    if (j->len >= j->cap - 1) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(j->buf + j->len, j->cap - j->len, fmt, ap);
    va_end(ap);
    if (n > 0) j->len += (size_t)n < j->cap - j->len
                         ? (size_t)n : j->cap - j->len - 1;
}

/* ─────────────────────────────────────────────
 * 1. Network  —  /proc/net/dev
 * ───────────────────────────────────────────── */

typedef struct {
    char     name[32];
    uint64_t rx_bytes, rx_packets, rx_errs, rx_drop;
    uint64_t tx_bytes, tx_packets, tx_errs, tx_drop;
} net_iface_t;

static int read_net_dev(net_iface_t *ifaces, int max)
{
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) return 0;

    char line[256];
    /* skip two header lines */
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }

    int count = 0;
    while (count < max && fgets(line, sizeof(line), f)) {
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        char *name = line;
        while (isspace((unsigned char)*name)) name++;

        net_iface_t *i = &ifaces[count];
        snprintf(i->name, sizeof(i->name), "%.31s", name);
        i->name[sizeof(i->name) - 1] = '\0';

        /* skip loopback */
        if (strcmp(i->name, "lo") == 0) continue;

        sscanf(colon + 1,
               "%llu %llu %llu %llu %*u %*u %*u %*u "
               "%llu %llu %llu %llu",
               (unsigned long long *)&i->rx_bytes,
               (unsigned long long *)&i->rx_packets,
               (unsigned long long *)&i->rx_errs,
               (unsigned long long *)&i->rx_drop,
               (unsigned long long *)&i->tx_bytes,
               (unsigned long long *)&i->tx_packets,
               (unsigned long long *)&i->tx_errs,
               (unsigned long long *)&i->tx_drop);
        count++;
    }
    fclose(f);
    return count;
}

static void collect_network(jbuf_t *j, double interval_sec)
{
    net_iface_t before[16], after[16];
    int nb = read_net_dev(before, 16);
    struct timespec ts = { .tv_sec  = (time_t)interval_sec,
                           .tv_nsec = (long)((interval_sec - (int)interval_sec) * 1e9) };
    nanosleep(&ts, NULL);
    int na = read_net_dev(after, 16);

    jb_append(j, "\"network\":{");
    int first = 1;
    for (int a = 0; a < na; a++) {
        /* find matching before entry */
        net_iface_t *bp = NULL;
        for (int b = 0; b < nb; b++) {
            if (strcmp(before[b].name, after[a].name) == 0) { bp = &before[b]; break; }
        }
        if (!bp) continue;

        double rx_bs = (after[a].rx_bytes   - bp->rx_bytes)   / interval_sec;
        double tx_bs = (after[a].tx_bytes   - bp->tx_bytes)   / interval_sec;
        double rx_ps = (after[a].rx_packets - bp->rx_packets) / interval_sec;
        double tx_ps = (after[a].tx_packets - bp->tx_packets) / interval_sec;

        if (!first) jb_append(j, ",");
        jb_append(j,
            "\"%s\":{\"rx_bytes_sec\":%.2f,\"tx_bytes_sec\":%.2f,"
            "\"rx_packets_sec\":%.2f,\"tx_packets_sec\":%.2f}",
            after[a].name, rx_bs, tx_bs, rx_ps, tx_ps);
        first = 0;
    }
    jb_append(j, "}");
}

/* ─────────────────────────────────────────────
 * 2. Disk  —  /proc/diskstats
 * ───────────────────────────────────────────── */

typedef struct {
    char     name[32];
    uint64_t reads_completed, sectors_read;
    uint64_t writes_completed, sectors_written;
} disk_dev_t;

static int read_diskstats(disk_dev_t *devs, int max)
{
    FILE *f = fopen("/proc/diskstats", "r");
    if (!f) return 0;

    char line[256];
    int count = 0;
    while (count < max && fgets(line, sizeof(line), f)) {
        unsigned int maj, min2;
        char name[32];
        unsigned long long rc, rm, sr, ms_r, wc, wm, sw, ms_w, io_prog, ms_io;
        int n = sscanf(line,
            "%u %u %31s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
            &maj, &min2, name,
            &rc, &rm, &sr, &ms_r,
            &wc, &wm, &sw, &ms_w,
            &io_prog, &ms_io);
        if (n < 13) continue;

        /* skip loop, ram, and partitions (sda1, nvme0n1p1, etc.) */
        if (strncmp(name, "loop", 4) == 0) continue;
        if (strncmp(name, "ram",  3) == 0) continue;
        /* skip partitions: name ends in a digit AND parent base exists */
        // int nlen = strlen(name);
        // if (nlen > 1 && isdigit((unsigned char)name[nlen - 1])) {
        //     int j = nlen - 1;
        //     while (j > 0 && isdigit((unsigned char)name[j])) j--;
        //     if (name[j] == 'p' && j > 0)
        //         continue;
            // if (isalpha((unsigned char)name[nlen - 2]))
            //     continue;
        // }

        disk_dev_t *d = &devs[count];
        strncpy(d->name, name, sizeof(d->name) - 1);
        d->name[sizeof(d->name) - 1] = '\0';
        d->reads_completed  = (uint64_t)rc;
        d->sectors_read     = (uint64_t)sr;
        d->writes_completed = (uint64_t)wc;
        d->sectors_written  = (uint64_t)sw;
        count++;
    }
    fclose(f);
    return count;
}

static void collect_disk(jbuf_t *j, double interval_sec)
{
    disk_dev_t before[16], after[16];
    int nb = read_diskstats(before, 16);
    struct timespec ts = { .tv_sec  = (time_t)interval_sec,
                           .tv_nsec = (long)((interval_sec - (int)interval_sec) * 1e9) };
    nanosleep(&ts, NULL);
    int na = read_diskstats(after, 16);

    const double sector = 512.0;
    jb_append(j, "\"disk\":{");
    int first = 1;
    for (int a = 0; a < na; a++) {
        disk_dev_t *bp = NULL;
        for (int b = 0; b < nb; b++) {
            if (strcmp(before[b].name, after[a].name) == 0) { bp = &before[b]; break; }
        }
        if (!bp) continue;

        double rb = (after[a].sectors_read    - bp->sectors_read)    * sector / interval_sec;
        double wb = (after[a].sectors_written - bp->sectors_written) * sector / interval_sec;
        double rs = (after[a].reads_completed - bp->reads_completed) / interval_sec;
        double ws = (after[a].writes_completed- bp->writes_completed)/ interval_sec;

        if (!first) jb_append(j, ",");
        jb_append(j,
            "\"%s\":{\"read_bytes_sec\":%.2f,\"write_bytes_sec\":%.2f,"
            "\"reads_sec\":%.2f,\"writes_sec\":%.2f}",
            after[a].name, rb, wb, rs, ws);
        first = 0;
    }
    jb_append(j, "}");
}

/* ─────────────────────────────────────────────
 * 3. Processes  —  /proc/[pid]/io
 * ───────────────────────────────────────────── */

typedef struct {
    int      pid;
    char     name[64];
    uint64_t rchar, wchar, read_bytes, write_bytes;
} proc_io_t;

static int read_one_proc_io(int pid, proc_io_t *out)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/io", pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[128];
    out->pid = pid;
    out->rchar = out->wchar = out->read_bytes = out->write_bytes = 0;
    while (fgets(line, sizeof(line), f)) {
        unsigned long long val;
        if (sscanf(line, "rchar: %llu",       &val) == 1) out->rchar       = val;
        if (sscanf(line, "wchar: %llu",       &val) == 1) out->wchar       = val;
        if (sscanf(line, "read_bytes: %llu",  &val) == 1) out->read_bytes  = val;
        if (sscanf(line, "write_bytes: %llu", &val) == 1) out->write_bytes = val;
    }
    fclose(f);

    /* read name from /proc/pid/comm */
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    f = fopen(path, "r");
    if (f) {
        if (fgets(out->name, sizeof(out->name), f)) {
            out->name[strcspn(out->name, "\n")] = '\0';
        }
        fclose(f);
    } else {
        strcpy(out->name, "?");
    }
    return 0;
}

static int cmp_total_desc(const void *a, const void *b)
{
    /* sort by read_bytes + write_bytes descending */
    const proc_io_t *pa = (const proc_io_t *)a;
    const proc_io_t *pb = (const proc_io_t *)b;
    uint64_t ta = pa->read_bytes + pa->write_bytes;
    uint64_t tb = pb->read_bytes + pb->write_bytes;
    if (tb > ta) return  1;
    if (tb < ta) return -1;
    return 0;
}

/* We allocate snapshot arrays on the heap to avoid blowing the stack. */
static void collect_processes(jbuf_t *j, double interval_sec)
{
    /* snapshot helper — fills array, returns count */
    #define MAX_PIDS 1024
    proc_io_t *before = malloc(sizeof(proc_io_t) * MAX_PIDS);
    proc_io_t *after  = malloc(sizeof(proc_io_t) * MAX_PIDS);
    if (!before || !after) { free(before); free(after); return; }

    int nb = 0, na = 0;

    DIR *dp = opendir("/proc");
    if (dp) {
        struct dirent *de;
        while ((de = readdir(dp)) && nb < MAX_PIDS) {
            int pid = atoi(de->d_name);
            if (pid <= 0) continue;
            if (read_one_proc_io(pid, &before[nb]) == 0) nb++;
        }
        closedir(dp);
    }

    struct timespec ts = { .tv_sec  = (time_t)interval_sec,
                           .tv_nsec = (long)((interval_sec - (int)interval_sec) * 1e9) };
    nanosleep(&ts, NULL);

    dp = opendir("/proc");
    if (dp) {
        struct dirent *de;
        while ((de = readdir(dp)) && na < MAX_PIDS) {
            int pid = atoi(de->d_name);
            if (pid <= 0) continue;
            if (read_one_proc_io(pid, &after[na]) == 0) na++;
        }
        closedir(dp);
    }

    /* compute deltas; store results back into after[] in-place */
    int valid = 0;
    for (int a = 0; a < na; a++) {
        proc_io_t *ap = &after[a];
        proc_io_t *bp = NULL;
        for (int b = 0; b < nb; b++) {
            if (before[b].pid == ap->pid) { bp = &before[b]; break; }
        }
        if (!bp) continue;

        int64_t dr = (int64_t)(ap->read_bytes  - bp->read_bytes);
        int64_t dw = (int64_t)(ap->write_bytes - bp->write_bytes);
        if (dr < 0) dr = 0;
        if (dw < 0) dw = 0;

        /* store rates back into the fields for sorting */
        ap->read_bytes  = (uint64_t)dr;
        ap->write_bytes = (uint64_t)dw;
        after[valid++] = *ap;
    }

    qsort(after, valid, sizeof(proc_io_t), cmp_total_desc);
    int top = valid < TOP_PROCS ? valid : TOP_PROCS;

    jb_append(j, "\"processes\":[");
    for (int i = 0; i < top; i++) {
        proc_io_t *p = &after[i];
        double rb  = p->read_bytes  / interval_sec;
        double wb  = p->write_bytes / interval_sec;

        /* escape name (basic — strip non-printable) */
        char safe[64];
        int si = 0;
        for (int k = 0; p->name[k] && si < 62; k++)
            if (isprint((unsigned char)p->name[k]) && p->name[k] != '"' && p->name[k] != '\\')
                safe[si++] = p->name[k];
        safe[si] = '\0';

        if (i) jb_append(j, ",");
        jb_append(j,
            "{\"pid\":%d,\"name\":\"%s\","
            "\"read_bytes_sec\":%.2f,\"write_bytes_sec\":%.2f,"
            "\"total_bytes_sec\":%.2f,"
            "\"total_rchar\":%llu,\"total_wchar\":%llu}",
            p->pid, safe, rb, wb, rb + wb,
            (unsigned long long)p->rchar,
            (unsigned long long)p->wchar);
    }
    jb_append(j, "]");

    free(before);
    free(after);
    #undef MAX_PIDS
}

/* ─────────────────────────────────────────────
 * 4. Open files  —  /proc/[pid]/fd
 * ───────────────────────────────────────────── */

static const char *INTERESTING[] = {
    "/home", "/etc", "/tmp", "/var", "/root", "/opt", NULL
};

static int is_interesting(const char *path)
{
    for (int i = 0; INTERESTING[i]; i++)
        if (strncmp(path, INTERESTING[i], strlen(INTERESTING[i])) == 0) return 1;
    return 0;
}

static void collect_open_files(jbuf_t *j)
{
    jb_append(j, "\"open_files\":[");
    int first = 1;

    DIR *dp = opendir("/proc");
    if (!dp) { jb_append(j, "]"); return; }

    struct dirent *de;
    while ((de = readdir(dp))) {
        int pid = atoi(de->d_name);
        if (pid <= 0) continue;

        /* process name */
        char comm[64] = "?";
        char comm_path[64];
        snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
        FILE *cf = fopen(comm_path, "r");
        if (cf) {
            if (fgets(comm, sizeof(comm), cf))
                comm[strcspn(comm, "\n")] = '\0';
            fclose(cf);
        }

        char fd_dir[64];
        snprintf(fd_dir, sizeof(fd_dir), "/proc/%d/fd", pid);
        DIR *fdp = opendir(fd_dir);
        if (!fdp) continue;

        struct dirent *fde;
        while ((fde = readdir(fdp))) {
            if (fde->d_name[0] == '.') continue;
            char fd_path[512], target[512];
            snprintf(fd_path, sizeof(fd_path), "/proc/%d/fd/%s", pid, fde->d_name);
            ssize_t len = readlink(fd_path, target, sizeof(target) - 1);
            if (len <= 0) continue;
            target[len] = '\0';

            if (target[0] != '/') continue;
            if (!is_interesting(target)) continue;

            if (!first) jb_append(j, ",");
            jb_append(j,
                "{\"pid\":%d,\"name\":\"%s\",\"fd\":\"%s\",\"path\":\"%s\"}",
                pid, comm, fde->d_name, target);
            first = 0;
        }
        closedir(fdp);
    }
    closedir(dp);
    jb_append(j, "]");
}

/* ─────────────────────────────────────────────
 * Collect all  (parallel via pthreads)
 * ───────────────────────────────────────────── */

typedef struct { jbuf_t *j; double interval; } worker_arg_t;

static void *worker_network(void *arg)
{
    worker_arg_t *a = arg;
    collect_network(a->j, a->interval);
    return NULL;
}
static void *worker_disk(void *arg)
{
    worker_arg_t *a = arg;
    collect_disk(a->j, a->interval);
    return NULL;
}
static void *worker_procs(void *arg)
{
    worker_arg_t *a = arg;
    collect_processes(a->j, a->interval);
    return NULL;
}

static char *collect_all(double interval_sec)
{
    /* Each worker writes into its own small buffer; we merge at the end. */
    char *nb = malloc(32 * 1024);
    char *db = malloc(8  * 1024);
    char *pb = malloc(64 * 1024);
    char *fb = malloc(JSON_BUF_SIZE);
    char *out = malloc(JSON_BUF_SIZE);
    if (!nb || !db || !pb || !fb || !out) {
        free(nb); free(db); free(pb); free(fb); free(out);
        return NULL;
    }

    jbuf_t jn, jd, jp, jf;
    jb_init(&jn, nb, 32 * 1024);
    jb_init(&jd, db, 8  * 1024);
    jb_init(&jp, pb, 64 * 1024);

    worker_arg_t wn = { &jn, interval_sec };
    worker_arg_t wd = { &jd, interval_sec };
    worker_arg_t wp = { &jp, interval_sec };

    pthread_t tn, td, tp;
    pthread_create(&tn, NULL, worker_network, &wn);
    pthread_create(&td, NULL, worker_disk,    &wd);
    pthread_create(&tp, NULL, worker_procs,   &wp);
    pthread_join(tn, NULL);
    pthread_join(td, NULL);
    pthread_join(tp, NULL);

    /* open_files is cheap — no sleep needed */
    jb_init(&jf, fb, JSON_BUF_SIZE);
    collect_open_files(&jf);

    /* assemble final JSON */
    jbuf_t jo;
    jb_init(&jo, out, JSON_BUF_SIZE);
    jb_append(&jo, "{%s,%s,%s,%s}", nb, db, pb, fb);

    free(nb); free(db); free(pb); free(fb);
    return out;   /* caller must free() */
}

/* ─────────────────────────────────────────────
 * sendall — keep writing until done or error
 * ───────────────────────────────────────────── */

static int sendall(int fd, const void *buf, size_t len)
{
    const char *p = buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, MSG_NOSIGNAL);
        if (n <= 0) return -1;
        p   += n;
        len -= (size_t)n;
    }
    return 0;
}

/* ─────────────────────────────────────────────
 * Client handler thread
 * ───────────────────────────────────────────── */

static void *handle_client(void *arg)
{
    int fd = *(int *)arg;
    free(arg);

    /* ── password check ── */
    char recv_buf[RECV_BUF_SIZE];
    ssize_t n = recv(fd, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0) { close(fd); return NULL; }
    recv_buf[n] = '\0';

    /* strip trailing whitespace/newline */
    for (ssize_t i = n - 1; i >= 0 && isspace((unsigned char)recv_buf[i]); i--)
        recv_buf[i] = '\0';

    if (strcmp(recv_buf, g_password) != 0) {
        printf("Wrong password from client, closing\n");
        close(fd);
        return NULL;
    }
    printf("Password accepted, starting data stream\n");

    /* ── data loop ── */
    double interval_sec = g_interval_ms / 1000.0;
    while (!g_stop) {
        char *json = collect_all(interval_sec);
        if (!json) break;

        size_t jlen = strlen(json);
        uint8_t hdr[4];
        hdr[0] = (jlen >> 24) & 0xFF;
        hdr[1] = (jlen >> 16) & 0xFF;
        hdr[2] = (jlen >>  8) & 0xFF;
        hdr[3] =  jlen        & 0xFF;

        int err = sendall(fd, hdr,  4)
               || sendall(fd, json, jlen);
        free(json);
        if (err) { printf("Client disconnected\n"); break; }

        if (interval_sec > 0) {
            struct timespec ts = {
                .tv_sec  = (time_t)interval_sec,
                .tv_nsec = (long)((interval_sec - (int)interval_sec) * 1e9)
            };
            nanosleep(&ts, NULL);
        }
    }

    close(fd);
    return NULL;
}

/* ─────────────────────────────────────────────
 * Main / accept loop
 * ───────────────────────────────────────────── */

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [--host HOST] [--port PORT] [--interval MS]\n"
        "  --host      bind address      (default: 0.0.0.0)\n"
        "  --port      TCP port          (default: %d)\n"
        "  --interval  ms between sends  (default: %d)\n",
        prog, DEFAULT_PORT, DEFAULT_INTERVAL);
}

int main(int argc, char **argv)
{
    const char *host = "0.0.0.0";
    int         port = DEFAULT_PORT;
    g_interval_ms    = DEFAULT_INTERVAL;

    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "--host")     == 0 && i+1 < argc) host          = argv[++i];
        else if (strcmp(argv[i], "--port")     == 0 && i+1 < argc) port          = atoi(argv[++i]);
        else if (strcmp(argv[i], "--interval") == 0 && i+1 < argc) g_interval_ms = atoi(argv[++i]);
        else if (strcmp(argv[i], "--help")     == 0) { print_usage(argv[0]); return 0; }
        else { fprintf(stderr, "Unknown argument: %s\n", argv[i]); print_usage(argv[0]); return 1; }
    }

    gen_password(g_password, PASSWORD_LEN);
    printf("Password: %s\n", g_password);
    fflush(stdout);

    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGPIPE, SIG_IGN);   /* don't crash on broken pipe */

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons((uint16_t)port),
        .sin_addr.s_addr = inet_addr(host),
    };
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(server_fd); return 1;
    }
    if (listen(server_fd, MAX_CLIENTS + 1) < 0) {
        perror("listen"); close(server_fd); return 1;
    }
    printf("Listening on %s:%d\n", host, port);

    /* track active client threads so we don't exceed MAX_CLIENTS */
    pthread_t threads[MAX_CLIENTS];
    int       active[MAX_CLIENTS];
    memset(active, 0, sizeof(active));

    while (!g_stop) {
        /* find a free slot */
        int slot = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!active[i]) { slot = i; break; }
        }
        if (slot < 0) {
            /* all slots busy — try to reap finished threads */
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (pthread_tryjoin_np(threads[i], NULL) == 0)
                    active[i] = 0;
            }
            usleep(50000);   /* 50 ms back-off */
            continue;
        }

        struct sockaddr_in client_addr;
        socklen_t clen = sizeof(client_addr);
        int *cfd = malloc(sizeof(int));
        if (!cfd) continue;

        *cfd = accept(server_fd, (struct sockaddr *)&client_addr, &clen);
        if (*cfd < 0) { free(cfd); if (errno == EINTR) continue; break; }

        printf("Client connected from %s:%d (slot %d)\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), slot);

        pthread_create(&threads[slot], NULL, handle_client, cfd);
        active[slot] = 1;
    }

    close(server_fd);

    /* wait for all client threads to finish */
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (active[i]) pthread_join(threads[i], NULL);

    printf("Server stopped\n");
    return 0;
}