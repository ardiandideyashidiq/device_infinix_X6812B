/*
 * fb_logo.c - direct fbdev writer for the charging screen.
 *
 * Two-plane double buffer on /dev/graphics/fb0. KDSETMODE(KD_GRAPHICS) on the
 * VT is what hands the panel over: without it fbcon keeps ownership and every
 * write lands on a surface nothing scans out, while every ioctl still reports
 * success. The present callback copies one frame into the back plane and pans.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "logo_render.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <linux/fb.h>
#include <linux/kd.h>

#define KPOC_FB_PATH   "/dev/graphics/fb0"
#define KPOC_VT_PATH   "/dev/tty0"
#define KPOC_FB_PLANES 2u

/*
 * Two-plane double buffer on /dev/graphics/fb0. KDSETMODE(KD_GRAPHICS) on the
 * VT is what hands the panel to us: without it the fbcon owns the display and
 * every write lands on a surface nobody scans out, while every ioctl still
 * reports success.
 */
typedef struct {
    int fd;
    int vt_fd;
    uint8_t *mem;
    size_t map_len;
    size_t plane_bytes;
    uint32_t line_length;
    uint32_t xres;
    uint32_t yres;
    uint32_t cur_plane;
} FbCtx;

static int fb_present(void *opaque, uint32_t yoffset, const uint8_t *pixels,
                      uint32_t w, uint32_t h, uint32_t bytes_pp)
{
    FbCtx *f = (FbCtx *)opaque;
    struct fb_var_screeninfo vi;
    uint8_t *plane;
    size_t row;
    uint32_t y;

    (void)yoffset;

    if (!f || !pixels || !f->mem)
        return -EINVAL;
    if (bytes_pp == 0 || w > f->xres || h > f->yres)
        return -EINVAL;
    if (w > SIZE_MAX / bytes_pp)
        return -EOVERFLOW;
    row = (size_t)w * bytes_pp;
    if (row > f->line_length)
        return -EINVAL;

    f->cur_plane = (f->cur_plane == 0u) ? 1u : 0u;
    plane = f->mem + f->cur_plane * f->plane_bytes;
    for (y = 0; y < h; ++y)
        memcpy(plane + (size_t)y * f->line_length,
               pixels + (size_t)y * row, row);

    if (ioctl(f->fd, FBIOGET_VSCREENINFO, &vi) != 0)
        return -errno;
    vi.yoffset = f->cur_plane * f->yres;
    vi.activate |= FB_ACTIVATE_FORCE | FB_ACTIVATE_NOW;
    if (ioctl(f->fd, FBIOPUT_VSCREENINFO, &vi) != 0)
        return -errno;
    return 0;
}

int kpoc_fb_open(LogoFB *out)
{
    FbCtx *f = NULL;
    struct fb_var_screeninfo vi;
    struct fb_fix_screeninfo fi;
    size_t need;
    int rc;

    if (!out)
        return -EINVAL;
    memset(out, 0, sizeof(*out));

    f = (FbCtx *)calloc(1, sizeof(*f));
    if (!f)
        return -ENOMEM;
    f->fd = -1;
    f->vt_fd = -1;

    f->vt_fd = open(KPOC_VT_PATH, O_RDWR | O_CLOEXEC);
    if (f->vt_fd >= 0 && ioctl(f->vt_fd, KDSETMODE, KD_GRAPHICS) != 0) {
        rc = -errno;
        goto fail;
    }

    f->fd = open(KPOC_FB_PATH, O_RDWR | O_CLOEXEC);
    if (f->fd < 0) {
        rc = -errno;
        goto fail;
    }
    if (ioctl(f->fd, FBIOGET_FSCREENINFO, &fi) != 0 ||
        ioctl(f->fd, FBIOGET_VSCREENINFO, &vi) != 0) {
        rc = -errno;
        goto fail;
    }

    if (vi.bits_per_pixel != 32u || vi.grayscale != 0u) {
        vi.bits_per_pixel = 32u;
        vi.grayscale = 0u;
        vi.transp.offset = 24u;
        vi.transp.length = 8u;
        vi.red.offset = 16u;
        vi.red.length = 8u;
        vi.green.offset = 8u;
        vi.green.length = 8u;
        vi.blue.offset = 0u;
        vi.blue.length = 8u;
        vi.activate = FB_ACTIVATE_NOW;
        if (ioctl(f->fd, FBIOPUT_VSCREENINFO, &vi) != 0) {
            rc = -errno;
            goto fail;
        }
    }

    if (fi.line_length == 0 || vi.xres == 0 || vi.yres == 0) {
        rc = -EINVAL;
        goto fail;
    }
    f->line_length = fi.line_length;
    f->xres = vi.xres;
    f->yres = vi.yres;

    if ((size_t)f->line_length > SIZE_MAX / f->yres) {
        rc = -EOVERFLOW;
        goto fail;
    }
    f->plane_bytes = (size_t)f->line_length * f->yres;
    if (f->plane_bytes > SIZE_MAX / KPOC_FB_PLANES) {
        rc = -EOVERFLOW;
        goto fail;
    }
    need = f->plane_bytes * KPOC_FB_PLANES;

    f->mem = (uint8_t *)mmap(NULL, need, PROT_READ | PROT_WRITE,
                             MAP_SHARED, f->fd, 0);
    if (f->mem == MAP_FAILED) {
        f->mem = NULL;
        rc = -errno;
        goto fail;
    }
    f->map_len = need;
    f->cur_plane = 1u;

    out->width = f->xres;
    out->height = f->yres;
    out->yres_virtual = f->yres;
    out->line_length = f->line_length;
    out->bpp = 32;
    out->red_offset = vi.red.offset;
    out->green_offset = vi.green.offset;
    out->blue_offset = vi.blue.offset;
    out->present = fb_present;
    out->ctx = f;
    return 0;

fail:
    if (f->mem)
        munmap(f->mem, f->map_len);
    if (f->fd >= 0)
        close(f->fd);
    if (f->vt_fd >= 0) {
        (void)ioctl(f->vt_fd, KDSETMODE, KD_TEXT);
        close(f->vt_fd);
    }
    free(f);
    return rc;
}

void kpoc_fb_close(LogoFB *fb)
{
    FbCtx *f;

    if (!fb || !fb->ctx)
        return;
    f = (FbCtx *)fb->ctx;
    if (f->mem)
        munmap(f->mem, f->map_len);
    if (f->fd >= 0)
        close(f->fd);
    if (f->vt_fd >= 0) {
        (void)ioctl(f->vt_fd, KDSETMODE, KD_TEXT);
        close(f->vt_fd);
    }
    free(f);
    fb->ctx = NULL;
    fb->present = NULL;
}
