  
                                                                                
                                                                           
                                                                                
                                                                            
                             
   
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "kpoc.h"
#ifdef KPOC_OWN_LOGO
#  include "logo_render.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <pthread.h>
#include <linux/input.h>
#include <linux/rtc.h>

                                                                                     
#ifdef KPOC_ANDROID
extern void klog_write(int prio, const char *fmt, ...);
extern int  reboot(int cmd);                                                         
extern int  kpoc_aidl_health_init(void);
extern int  kpoc_aidl_health_get_info(void *out);
extern int  kpoc_aidl_health_get_capacity(void);
extern int  kpoc_aidl_light_init(void);
extern int  kpoc_aidl_light_set(uint32_t type, uint32_t argb);
extern void anim_init(void);
extern void set_anim_version(int version);
extern void set_draw_mode(int mode);
extern void show_battery_capacity(unsigned int capacity);
extern void show_charger_ov_logo(void);
extern void show_low_battery(void);
extern void autosuspend_enable(void);
extern void autosuspend_disable(void);
extern int  uevent_init(void);
extern int  uevent_next_event(char *buf, int len);
#endif

#define KPOC_LOG_MAX_BYTES (512u * 1024u)

static int log_open_once(void)
{
    static int fd = -2;
    struct stat st;

    if (fd != -2)
        return fd;

    /* Keep the previous session for post-mortem, drop anything older. */
    if (stat(KPOC_PATH_LOGFILE, &st) == 0)
        (void)rename(KPOC_PATH_LOGFILE, KPOC_PATH_LOGFILE ".1");

    fd = open(KPOC_PATH_LOGFILE, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0)
        fd = -1;
    return fd;
}

static void log_to_file(const char *msg)
{
    static int fd = -2;
    static off_t written;
    size_t len = strlen(msg);
    char line[576];
    struct timespec ts;
    int n;

    if (fd == -2) {
        fd = log_open_once();
        written = 0;
    }
    if (fd < 0)
        return;
    if (written >= (off_t)KPOC_LOG_MAX_BYTES)
        return;

    if (clock_gettime(CLOCK_BOOTTIME, &ts) != 0) {
        ts.tv_sec = 0;
        ts.tv_nsec = 0;
    }
    n = snprintf(line, sizeof(line), "[%5ld.%06ld] %s%s",
                 (long)ts.tv_sec, (long)(ts.tv_nsec / 1000), msg,
                 (len && msg[len - 1] == '\n') ? "" : "\n");
    if (n > 0) {
        if ((size_t)n > sizeof(line) - 1)
            n = (int)(sizeof(line) - 1);
        if (write(fd, line, (size_t)n) == (ssize_t)n)
            written += n;
    }
    (void)fsync(fd);
}

static void platform_log(int prio, const char *msg)
{
#ifdef KPOC_ANDROID
    klog_write(prio, "%s", msg);
    log_to_file(msg);
#else
    (void)prio;
    fputs(msg, stderr);
    if (msg[0] != '\0' && msg[strlen(msg) - 1] != '\n')
        fputc('\n', stderr);
#endif
}



static void op_log_str(int prio, const char *msg) { platform_log(prio, msg); }

static void op_reboot(uint32_t cmd) KPOC_NORETURN;
static void op_reboot(uint32_t cmd)
{
#ifdef KPOC_ANDROID
    (void)reboot((int)cmd);
#else
    fprintf(stderr, "[cleanroom] would reboot(0x%08x)\n", cmd);
    _exit(99);
#endif
    for (;;) { }                  
}

static int op_set_nice(int nice) { return setpriority(PRIO_PROCESS, 0, nice); }

static int op_write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    int r;
    if (!f) return -1;
    r = fputs(text, f);
    fclose(f);
    return r < 0 ? -1 : 0;
}

static uint64_t op_now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000u + (uint64_t)tv.tv_usec / 1000u;
}

static void op_sleep_ms(unsigned ms) { usleep(ms * 1000u); }
static void op_sleep_us(unsigned us) { usleep(us); }

static int op_read_int_file(const char *path, int *out)
{
    char buf[KPOC_READ_BUF_SIZE + 1];
    int fd, n, i;
    if (!path || !out) return -1;
    fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    n = (int)read(fd, buf, KPOC_READ_BUF_SIZE);
    close(fd);
    if (n < 1) { *out = 0; return -1; }
    i = n < (int)KPOC_READ_BUF_SIZE ? n : (int)KPOC_READ_BUF_SIZE;
    while (i > 0 && buf[i - 1] == '\n') --i;
    if (i == 0) { *out = 0; return -1; }                          
    buf[i] = '\0';
    *out = atoi(buf);
    return 0;
}

static int op_read_boot_mode(const char *path, uint32_t *size,
                             uint32_t *tag, uint32_t *mode)
{
    uint32_t raw[8] = {0};                                                     
    int fd, r = -1;
    if (!path) return -1;
    fd = open(path, O_RDONLY);
    if (fd < 0) return -errno;
    if (read(fd, raw, sizeof(raw)) > 0) {
        char msg[160];
        *size = raw[0]; *tag = raw[1]; *mode = raw[2];
        snprintf(msg, sizeof(msg),
                 "<3>charger: boot mode struct:size=%d,tag=%d,mode=%d\n",
                 (int)*size, (int)*tag, (int)*mode);
        platform_log(3, msg);
        r = 0;
    } else {
        platform_log(3, "<3>charger: read boot mode struct:size <=0");
        r = -EIO;
    }
    close(fd);
    return r;
}

static int op_health_init(void)
{
#ifdef KPOC_ANDROID
    return kpoc_aidl_health_init();
#else
    return -1;
#endif
}

static int op_health_get_info(KpocHealthInfo *out)
{
#ifdef KPOC_ANDROID
    return kpoc_aidl_health_get_info(out);
#else
    (void)out; return -1;
#endif
}

static int op_health_get_capacity(void)
{
#ifdef KPOC_ANDROID
    return kpoc_aidl_health_get_capacity();
#else
    return -1;
#endif
}

static int op_light_init(void)
{
#ifdef KPOC_ANDROID
    return kpoc_aidl_light_init();
#else
    return -1;
#endif
}

static int op_light_set(uint32_t type, uint32_t argb)
{
#ifdef KPOC_ANDROID
    return kpoc_aidl_light_set(type, argb);
#else
    (void)type; (void)argb;
    return -1;
#endif
}

#if defined(KPOC_OWN_LOGO)
                                                             
static LogoRenderer g_logo;
static LogoFB g_logo_fb;
static uint8_t *g_logo_buf;
static size_t g_logo_len;
static int g_logo_ready;

static void op_anim_init(void)
{
    static const char *const paths[] = {
        "/dev/block/by-name/logo",
        "/dev/block/by-name/logo_a",
        "/logo/logo.bin"
    };
    size_t i;
    int rc;
    int used = -1;

    for (i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        if (logo_read_file(paths[i], &g_logo_buf, &g_logo_len) == 0) {
            used = (int)i;
            break;
        }
    }
    if (g_logo_buf == NULL) {
        platform_log(3, "<3>charger: anim_init: no logo partition readable\n");
        return;
    }
    platform_log(3, used == 0
                        ? "<3>charger: anim_init: logo partition ok\n"
                        : "<3>charger: anim_init: logo_a partition ok\n");

    rc = kpoc_fb_open(&g_logo_fb);
    if (rc != 0) {
        platform_log(3, "<3>charger: anim_init: kpoc_fb_open failed\n");
        platform_log(3, rc == -ENOENT    ? "<3>charger:   no fb0 or tty0\n"     :
                         rc == -EINVAL    ? "<3>charger:   bad fb geometry\n"   :
                         rc == -EOVERFLOW ? "<3>charger:   size overflow\n"     :
                         rc == -ENOMEM    ? "<3>charger:   out of memory\n"     :
                                            "<3>charger:   fb0/tty0 setup failed\n");
        goto fail;
    }
    rc = logo_render_init(&g_logo, g_logo_buf, g_logo_len, &g_logo_fb);
    if (rc != 0) {
        platform_log(3, "<3>charger: anim_init: logo_render_init failed\n");
        kpoc_fb_close(&g_logo_fb);
        goto fail;
    }
    platform_log(3, "<3>charger: anim_init: ready\n");
    g_logo_ready = 1;
    return;
fail:
    free(g_logo_buf);
    g_logo_buf = NULL;
}

static void op_anim_ver(int v) { if (g_logo_ready) logo_set_version(&g_logo, v); }
static void op_anim_mode(int m) { (void)m;                                           }
static void op_anim_cap(uint32_t c) { if (g_logo_ready) logo_show_layout(&g_logo, c); }
static void op_anim_ov(void) { if (g_logo_ready) logo_show_ov(&g_logo); }
static void op_anim_low(void) { if (g_logo_ready) logo_show_low(&g_logo); }
#elif defined(KPOC_ANDROID)
static void op_anim_init(void) { anim_init(); }
static void op_anim_ver(int v) { set_anim_version(v); }
static void op_anim_mode(int m) { set_draw_mode(m); }
static void op_anim_cap(uint32_t c) { show_battery_capacity(c); }
static void op_anim_ov(void) { show_charger_ov_logo(); }
static void op_anim_low(void) { show_low_battery(); }
#else
static void op_anim_init(void) {}
static void op_anim_ver(int v) { (void)v; }
static void op_anim_mode(int m) { (void)m; }
static void op_anim_cap(uint32_t c) { (void)c; }
static void op_anim_ov(void) {}
static void op_anim_low(void) {}
#endif

static void op_autosuspend_enable(void)
{
#ifdef KPOC_ANDROID
    autosuspend_enable();
#endif
}
static void op_autosuspend_disable(void)
{
#ifdef KPOC_ANDROID
    autosuspend_disable();
#endif
}

static int op_uevent_init(void)
{
#ifdef KPOC_ANDROID
    return uevent_init();
#else
    return 0;              
#endif
}

static int op_uevent_next(char *buf, size_t len)
{
#ifdef KPOC_ANDROID
    return uevent_next_event(buf, (int)len);
#else
    (void)buf; (void)len; return -1;
#endif
}

                                                                              
struct InputState {
    DIR *dir;
    int epoll_fd;
    int inotify_fd;
    KpocInputEvent queue[64];
    int qhead, qtail;
    char watched[32][128];                             
    int  watched_fd[32];
    int  nwatch;
};
static struct InputState g_input;

static int device_wanted(const char *name)
{
    char path[256];
    char devname[128];
    int fd;
    if (!name || strncmp(name, "event", 5) != 0) return 0;
    snprintf(path, sizeof(path), "%s/%s", KPOC_PATH_INPUT, name);
    fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return 0;
    memset(devname, 0, sizeof(devname));
    if (ioctl(fd, KPOC_EVIOCGNAME, devname) < 0) {
        close(fd); return 0;
    }
    close(fd);
    return strcasestr(devname, "kpd") || strcasestr(devname, "keypad") ||
           strcasestr(devname, "pmic-keys");
}

static int op_input_scan_begin(void)
{
    memset(&g_input, 0, sizeof(g_input));
    g_input.dir = opendir(KPOC_PATH_INPUT);
    if (!g_input.dir) return -1;
    g_input.epoll_fd = epoll_create(1);
    g_input.inotify_fd = inotify_init1(IN_NONBLOCK);
    if (g_input.epoll_fd >= 0 && g_input.inotify_fd >= 0) {
        epoll_ctl(g_input.epoll_fd, EPOLL_CTL_ADD, g_input.inotify_fd,
                  &(struct epoll_event){ .events = EPOLLIN,
                                         .data.fd = g_input.inotify_fd });
    }
    if (g_input.inotify_fd >= 0)
        inotify_add_watch(g_input.inotify_fd, KPOC_PATH_INPUT, IN_CREATE | IN_DELETE);
    return 0;
}

static int op_input_scan_next(char *name, size_t len)
{
    struct dirent *de;
    while ((de = readdir(g_input.dir)) != NULL) {
        if (!device_wanted(de->d_name)) continue;
        snprintf(name, len, "%s", de->d_name);
        return 0;
    }
    return -1;
}

static void op_input_scan_end(void) { if (g_input.dir) closedir(g_input.dir); g_input.dir = NULL; }

static int register_device(const char *name)
{
    char path[256];
    int fd;
    struct epoll_event ev;
    snprintf(path, sizeof(path), "%s/%s", KPOC_PATH_INPUT, name);
    fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return -1;
    ev.events = KPOC_EPOLL_MASK;                            
    ev.data.fd = fd;
    if (epoll_ctl(g_input.epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        close(fd); return -1;
    }
    if (g_input.nwatch < 32) {
        snprintf(g_input.watched[g_input.nwatch], 128, "%s", name);
        g_input.watched_fd[g_input.nwatch] = fd;
        ++g_input.nwatch;
    }
    return 0;
}

static int op_input_open(const char *dev) { return register_device(dev); }
static int op_input_watch_add(const char *dev) { return register_device(dev); }

static int unregister_device(const char *name)
{
    int i;
    for (i = 0; i < g_input.nwatch; ++i) {
        if (strcmp(g_input.watched[i], name) == 0) {
            epoll_ctl(g_input.epoll_fd, EPOLL_CTL_DEL, g_input.watched_fd[i], NULL);
            close(g_input.watched_fd[i]);
            g_input.watched[i][0] = '\0';
            return 0;
        }
    }
    return -1;
}
static int op_input_watch_remove(const char *dev) { return unregister_device(dev); }

static void drain_fd(int fd)
{
    unsigned char buf[32 * sizeof(struct input_event)];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        size_t off;
        for (off = 0; off + sizeof(struct input_event) <= (size_t)n;
             off += sizeof(struct input_event)) {
            struct input_event e;
            int next;
            memcpy(&e, buf + off, sizeof(e));                                   
            next = (g_input.qtail + 1) % 64;
            if (next == g_input.qhead) break;
            g_input.queue[g_input.qtail].type = e.type;
            g_input.queue[g_input.qtail].code = e.code;
            g_input.queue[g_input.qtail].value = e.value;
            g_input.qtail = next;
        }
    }
}

static void drain_inotify(void)
{
    char ibuf[4096];
    ssize_t n;
    while ((n = read(g_input.inotify_fd, ibuf, sizeof(ibuf))) > 0) {
        size_t off = 0;
        while (off + sizeof(struct inotify_event) <= (size_t)n) {
            struct inotify_event ie;
            memcpy(&ie, ibuf + off, sizeof(ie));
            if (ie.len > 0 && (ie.mask & (IN_CREATE | IN_DELETE))) {
                char devname[128];
                size_t len = ie.len < sizeof(devname) - 1
                                 ? ie.len : sizeof(devname) - 1;
                                                                           
                                                                      
                memcpy(devname, ibuf + off + sizeof(struct inotify_event), len);
                devname[len] = '\0';
                if (ie.mask & IN_CREATE)
                    register_device(devname);
                else
                    unregister_device(devname);
            }
            off += sizeof(struct inotify_event) + ie.len;
        }
    }
}

static int op_input_dispatch(void)
{
    struct epoll_event evs[KPOC_EPOLL_MAX_EVENTS];
    int n = epoll_wait(g_input.epoll_fd, evs, KPOC_EPOLL_MAX_EVENTS, -1);
    int i;
    if (n < 0) return -errno;
    if (n == 0) return 0;
    for (i = 0; i < n; ++i) {
        if (evs[i].data.fd == g_input.inotify_fd)
            drain_inotify();
        else
            drain_fd(evs[i].data.fd);
    }
    return n;
}

static int op_input_next_event(KpocInputEvent *ev)
{
    if (g_input.qhead == g_input.qtail) return -1;
    *ev = g_input.queue[g_input.qhead];
    g_input.qhead = (g_input.qhead + 1) % 64;
    return 0;
}

                                                                              
static int op_fb_open(const char *path) { return open(path, O_RDWR); }
static int op_fb_blank(int fd, int mode) { return ioctl(fd, KPOC_FBIOBLANK, mode); }
static void op_fb_close(int fd) { if (fd >= 0) close(fd); }

                                                                              
#ifndef ANDROID_ALARM_GET_POWER_ON
#define ANDROID_ALARM_GET_POWER_ON _IOR('a', 7, struct rtc_wkalrm)
#endif

static int op_alarm_wait_power_on(void)
{
    int fd_rtc = -1, fd_alarm = -1, result = 0;
    unsigned long data = 0;
    struct rtc_wkalrm alrm;

    memset(&alrm, 0, sizeof(alrm));
    fd_rtc = open(KPOC_PATH_RTC0, O_RDONLY | O_CLOEXEC);
    if (fd_rtc < 0)
        return -errno;
    fd_alarm = open(KPOC_PATH_ALARM, O_RDWR | O_CLOEXEC);
    if (fd_alarm < 0) {
        result = -errno;
        goto out;
    }

    errno = 0;
    if (read(fd_rtc, &data, sizeof(data)) != (ssize_t)sizeof(data)) {
        result = errno ? -errno : -EIO;
        goto out;
    }
    if (!(data & RTC_AF))
        goto out;                         

    if (ioctl(fd_alarm, ANDROID_ALARM_GET_POWER_ON, &alrm) < 0) {
        alrm.pending = 0;
        alrm.enabled = 0;
    }
    result = (alrm.pending == 1 && alrm.enabled == 0) ? 1 : 0;

out:
    if (fd_alarm >= 0) close(fd_alarm);
    if (fd_rtc >= 0) close(fd_rtc);
    return result;
}

static const KpocOps k_real_ops = {
    .log_str = op_log_str, .reboot = op_reboot, .set_nice = op_set_nice,
    .write_file = op_write_file, .now_ms = op_now_ms, .sleep_ms = op_sleep_ms,
    .sleep_us = op_sleep_us, .read_int_file = op_read_int_file,
    .read_boot_mode = op_read_boot_mode, .health_init = op_health_init,
    .health_get_info = op_health_get_info,
    .health_get_capacity = op_health_get_capacity, .light_init = op_light_init,
    .light_set = op_light_set, .anim_init = op_anim_init,
    .anim_set_version = op_anim_ver, .anim_set_draw_mode = op_anim_mode,
    .anim_show_capacity = op_anim_cap, .anim_show_ov_logo = op_anim_ov,
    .anim_show_low_battery = op_anim_low,
    .autosuspend_enable = op_autosuspend_enable,
    .autosuspend_disable = op_autosuspend_disable,
    .uevent_init = op_uevent_init, .uevent_next = op_uevent_next,
    .input_scan_begin = op_input_scan_begin, .input_scan_next = op_input_scan_next,
    .input_scan_end = op_input_scan_end, .input_open = op_input_open,
    .input_next_event = op_input_next_event, .input_watch_add = op_input_watch_add,
    .input_watch_remove = op_input_watch_remove, .input_dispatch = op_input_dispatch,
    .fb_open = op_fb_open, .fb_blank = op_fb_blank, .fb_close = op_fb_close,
    .alarm_wait_power_on = op_alarm_wait_power_on
};

                                                                             
static int op_thread_create(KpocWorkerKind kind, KpocThreadFn fn, KpocCtx *arg)
{
    pthread_t tid;
    pthread_attr_t attr;
    (void)kind;
    pthread_attr_init(&attr);
    return pthread_create(&tid, &attr, fn, arg);
}
static int op_mutex_init(void *m) { return pthread_mutex_init((pthread_mutex_t *)m, NULL); }
static int op_mutex_destroy(void *m) { return pthread_mutex_destroy((pthread_mutex_t *)m); }
static int op_mutex_lock(void *m) { return pthread_mutex_lock((pthread_mutex_t *)m); }
static int op_mutex_unlock(void *m) { return pthread_mutex_unlock((pthread_mutex_t *)m); }
static int op_cond_init(void *c) { return pthread_cond_init((pthread_cond_t *)c, NULL); }
static int op_cond_destroy(void *c) { return pthread_cond_destroy((pthread_cond_t *)c); }
static int op_cond_wait(void *c, void *m)
{ return pthread_cond_wait((pthread_cond_t *)c, (pthread_mutex_t *)m); }
static int op_cond_signal(void *c) { return pthread_cond_signal((pthread_cond_t *)c); }

static const KpocThreadOps k_real_threads = {
    .create = op_thread_create, .mutex_init = op_mutex_init,
    .mutex_destroy = op_mutex_destroy,
    .mutex_lock = op_mutex_lock, .mutex_unlock = op_mutex_unlock,
    .cond_init = op_cond_init, .cond_destroy = op_cond_destroy,
    .cond_wait = op_cond_wait, .cond_signal = op_cond_signal
};

const KpocOps *kpoc_ops_real(void) { return &k_real_ops; }
const KpocThreadOps *kpoc_threads_real(void) { return &k_real_threads; }
