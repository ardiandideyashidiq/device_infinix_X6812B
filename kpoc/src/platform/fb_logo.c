#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "logo_render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <stdint.h>

#define KPOC_MINIUI_LIB "/vendor/lib64/libminiui.so"

/*
 * Symbol resolution is reported as its own errno so the caller can tell a
 * missing library (ENOENT) from a library that lacks an entry point (ENOSYS).
 */
typedef struct {
    int (*gr_init)(void);
    void (*gr_exit)(void);
    void *(*gr_fb_data)(void);
    int (*gr_fb_width)(void);
    int (*gr_fb_height)(void);
    void (*gr_flip)(void);
} MiniUI;

typedef struct {
    MiniUI api;
    void *handle;
    uint8_t *fb;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
} FbCtx;

static int resolve(MiniUI *api, void *handle)
{
    struct {
        const char *name;
        void *slot;
        size_t size;
    } syms[] = {
        { "gr_init",      &api->gr_init,      sizeof(api->gr_init)      },
        { "gr_exit",      &api->gr_exit,      sizeof(api->gr_exit)      },
        { "gr_fb_data",   &api->gr_fb_data,   sizeof(api->gr_fb_data)   },
        { "gr_fb_width",  &api->gr_fb_width,  sizeof(api->gr_fb_width)  },
        { "gr_fb_height", &api->gr_fb_height, sizeof(api->gr_fb_height) },
        { "gr_flip",      &api->gr_flip,      sizeof(api->gr_flip)      },
    };
    size_t i;
    void *sym;

    for (i = 0; i < sizeof(syms) / sizeof(syms[0]); ++i) {
        dlerror();
        sym = dlsym(handle, syms[i].name);
        if (sym == NULL)
            return -ENOSYS;
        memcpy(syms[i].slot, &sym, syms[i].size);
    }
    return 0;
}

static int fb_present(void *opaque, uint32_t yoffset, const uint8_t *pixels,
                      uint32_t w, uint32_t h, uint32_t bytes_pp)
{
    FbCtx *f = (FbCtx *)opaque;
    const uint8_t *src;
    uint8_t *dst;
    size_t row;
    uint32_t y;

    /* The miniui surface is single-buffered; panning is handled by gr_flip. */
    (void)yoffset;

    if (!f || !pixels || !f->fb)
        return -EINVAL;
    if (bytes_pp == 0 || w > f->width || h > f->height)
        return -EINVAL;
    if (w > SIZE_MAX / bytes_pp)
        return -EOVERFLOW;
    row = (size_t)w * bytes_pp;
    if (row > f->stride)
        return -EINVAL;

    for (y = 0; y < h; ++y) {
        dst = f->fb + (size_t)y * f->stride;
        src = pixels + (size_t)y * row;
        memcpy(dst, src, row);
    }
    f->api.gr_flip();
    return 0;
}

int kpoc_fb_open(LogoFB *out)
{
    FbCtx *f = NULL;
    uint8_t *fb;
    int w, h, rc;

    if (!out)
        return -EINVAL;
    memset(out, 0, sizeof(*out));

    f = (FbCtx *)calloc(1, sizeof(*f));
    if (!f)
        return -ENOMEM;

    f->handle = dlopen(KPOC_MINIUI_LIB, RTLD_NOW);
    if (!f->handle) {
        rc = -ENOENT;
        goto fail;
    }

    rc = resolve(&f->api, f->handle);
    if (rc != 0)
        goto fail;

    rc = f->api.gr_init();
    if (rc != 0) {
        rc = -EIO;
        goto fail_init;
    }

    fb = (uint8_t *)f->api.gr_fb_data();
    w = f->api.gr_fb_width();
    h = f->api.gr_fb_height();
    if (!fb || w <= 0 || h <= 0) {
        rc = -ENODEV;
        goto fail_init;
    }
    if ((size_t)w > SIZE_MAX / 4u) {
        rc = -EOVERFLOW;
        goto fail_init;
    }

    f->fb = fb;
    f->width = (uint32_t)w;
    f->height = (uint32_t)h;
    f->stride = (uint32_t)w * 4u;

    out->width = f->width;
    out->height = f->height;
    out->yres_virtual = f->height;
    out->line_length = f->stride;
    out->bpp = 32;
    out->red_offset = 16;
    out->green_offset = 8;
    out->blue_offset = 0;
    out->present = fb_present;
    out->ctx = f;
    return 0;

fail_init:
    if (f->api.gr_exit)
        f->api.gr_exit();
fail:
    if (f->handle)
        dlclose(f->handle);
    free(f);
    return rc;
}

void kpoc_fb_close(LogoFB *fb)
{
    FbCtx *f;

    if (!fb || !fb->ctx)
        return;
    f = (FbCtx *)fb->ctx;
    if (f->api.gr_exit)
        f->api.gr_exit();
    if (f->handle)
        dlclose(f->handle);
    free(f);
    fb->ctx = NULL;
    fb->present = NULL;
}
