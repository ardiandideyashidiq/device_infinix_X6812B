#ifndef KPOC_TEST_HARNESS_H
#define KPOC_TEST_HARNESS_H

#include "kpoc.h"

typedef struct {
    int      failures;
    int      checks;
    const char *current;
} TestState;

extern TestState g_test;

void test_begin(const char *name);
void test_end(void);
void test_fail(const char *fmt, ...);

#define CHECK(cond)                                                          \
    do {                                                                     \
        g_test.checks++;                                                     \
        if (!(cond))                                                         \
            test_fail("%s:%d: CHECK(%s)", __FILE__, __LINE__, #cond);        \
    } while (0)

#define CHECK_EQ_I(exp, got)                                                 \
    do {                                                                     \
        long long _e = (long long)(exp), _g = (long long)(got);              \
        g_test.checks++;                                                     \
        if (_e != _g)                                                        \
            test_fail("%s:%d: %s == %lld, expected %lld",                    \
                      __FILE__, __LINE__, #got, _g, _e);                     \
    } while (0)

#define CHECK_EQ_U(exp, got)                                                 \
    do {                                                                     \
        unsigned long long _e = (unsigned long long)(exp);                   \
        unsigned long long _g = (unsigned long long)(got);                   \
        g_test.checks++;                                                     \
        if (_e != _g)                                                        \
            test_fail("%s:%d: %s == %llu, expected %llu",                    \
                      __FILE__, __LINE__, #got, _g, _e);                     \
    } while (0)

/* A KpocOps whose every slot is a recording stub. Tests override only the
 * slots they exercise, so an unexpected call is loud rather than silent.
 */
typedef struct {
    KpocOps ops;

    unsigned log_calls;
    unsigned reboot_calls;
    uint32_t reboot_cmd;

    unsigned write_calls;
    char     last_write_path[128];
    char     last_write_text[64];

    int      read_int_result;
    int      read_int_value;
    unsigned read_int_calls;

    int      health_capacity;
    int      health_source;
    int      health_info_result;

    int      light_set_result;
    unsigned light_set_calls;
    uint32_t light_set_type;
    uint32_t light_set_argb;

    unsigned anim_capacity_calls;
    uint32_t anim_capacity_last;
    unsigned anim_init_calls;

    int      input_scan_result;
    int      uevent_init_result;

    uint64_t now_value;

    char     input_name[32];
    char     last_log[256];
} FakeOps;

void fake_ops_init(FakeOps *f);
void fake_ops_free(FakeOps *f);

#endif
