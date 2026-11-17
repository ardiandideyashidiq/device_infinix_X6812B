#include "harness.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

TestState g_test;

static FakeOps *g_active;

void test_begin(const char *name)
{
    printf("  %-46s ", name);
    fflush(stdout);
}

void test_end(void)
{
}

void test_fail(const char *fmt, ...)
{
    va_list ap;

    g_test.failures++;
    printf("FAILED\n");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

static void stub_log_str(int prio, const char *msg)
{
    (void)prio;
    if (!g_active)
        return;
    g_active->log_calls++;
    if (msg)
        snprintf(g_active->last_log, sizeof(g_active->last_log), "%s", msg);
}

static void stub_reboot(uint32_t cmd) KPOC_NORETURN;
static void stub_reboot(uint32_t cmd)
{
    (void)cmd;
    __builtin_unreachable();
}
static int stub_set_nice(int nice) { (void)nice; return 0; }
static int stub_write_file(const char *p, const char *t) { (void)p; (void)t; return 0; }
static uint64_t stub_now_ms(void) { return 0; }
static void stub_sleep_ms(unsigned ms) { (void)ms; }
static void stub_sleep_us(unsigned us) { (void)us; }
static int stub_read_int_file(const char *p, int *out) { (void)p; (void)out; return -1; }
static int stub_health_init(void) { return 0; }
static int stub_uevent_init(void) { return 0; }
static void stub_fb_close(int fd) { (void)fd; }
static int stub_health_get_info(KpocHealthInfo *o) { (void)o; return -1; }
static int stub_health_get_capacity(void) { return -1; }
static int stub_light_init(void) { return 0; }
static int stub_light_set(uint32_t t, uint32_t a) { (void)t; (void)a; return 0; }
static void stub_void(void) {}
static void stub_anim_set_version(int v) { (void)v; }
static void stub_anim_set_draw_mode(int m) { (void)m; }
static void stub_anim_show_capacity(uint32_t c) { (void)c; }
static int stub_uevent_next(char *b, size_t l) { (void)b; (void)l; return -1; }
static int stub_input_scan_begin(void) { return -1; }
static int stub_input_scan_next(char *n, size_t l) { (void)n; (void)l; return -1; }
static int stub_input_open(const char *dev) { (void)dev; return -1; }
static int stub_input_next_event(KpocInputEvent *e) { (void)e; return -1; }
static int stub_input_watch_add(const char *dev) { (void)dev; return -1; }
static int stub_input_watch_remove(const char *dev) { (void)dev; return -1; }
static int stub_input_dispatch(void) { return -1; }
static int stub_fb_open(const char *p) { (void)p; return -1; }
static int stub_fb_blank(int fd, int m) { (void)fd; (void)m; return -1; }

static int stub_alarm_wait(void) { return 0; }

void fake_ops_init(FakeOps *f)
{
    memset(f, 0, sizeof(*f));
    g_active = f;

    f->ops.log_str = stub_log_str;
    f->ops.reboot = stub_reboot;
    f->ops.set_nice = stub_set_nice;
    f->ops.write_file = stub_write_file;
    f->ops.now_ms = stub_now_ms;
    f->ops.sleep_ms = stub_sleep_ms;
    f->ops.sleep_us = stub_sleep_us;
    f->ops.read_int_file = stub_read_int_file;
    f->ops.health_init = stub_health_init;
    f->ops.health_get_info = stub_health_get_info;
    f->ops.health_get_capacity = stub_health_get_capacity;
    f->ops.light_init = stub_light_init;
    f->ops.light_set = stub_light_set;
    f->ops.anim_init = stub_void;
    f->ops.anim_set_version = stub_anim_set_version;
    f->ops.anim_set_draw_mode = stub_anim_set_draw_mode;
    f->ops.anim_show_capacity = stub_anim_show_capacity;
    f->ops.anim_show_ov_logo = stub_void;
    f->ops.anim_show_low_battery = stub_void;
    f->ops.autosuspend_enable = stub_void;
    f->ops.autosuspend_disable = stub_void;
    f->ops.uevent_init = stub_uevent_init;
    f->ops.uevent_next = stub_uevent_next;
    f->ops.input_scan_begin = stub_input_scan_begin;
    f->ops.input_scan_next = stub_input_scan_next;
    f->ops.input_scan_end = stub_void;
    f->ops.input_open = stub_input_open;
    f->ops.input_next_event = stub_input_next_event;
    f->ops.input_watch_add = stub_input_watch_add;
    f->ops.input_watch_remove = stub_input_watch_remove;
    f->ops.input_dispatch = stub_input_dispatch;
    f->ops.fb_open = stub_fb_open;
    f->ops.fb_blank = stub_fb_blank;
    f->ops.fb_close = stub_fb_close;
    f->ops.alarm_wait_power_on = stub_alarm_wait;
}

void fake_ops_free(FakeOps *f)
{
    (void)f;
}
