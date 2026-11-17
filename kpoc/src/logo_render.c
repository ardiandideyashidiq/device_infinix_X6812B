  
                                                                    
                                                                    
   
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "logo_render.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>

#define LOGO_TABLE_OFFSET 0x200u                                               
#define LOGO_BPP          4u

int logo_last_present_rc;
unsigned logo_last_present_idx;

int logo_render_init(LogoRenderer *r, const uint8_t *file_buf, size_t len,
                     const LogoFB *fb)
{
    uint32_t count, table_len, i;

    if (!r || !file_buf || !fb)
        return -EINVAL;

    memset(r, 0, sizeof(*r));
    if (len < LOGO_TABLE_OFFSET + 8u)
        return -EINVAL;

    count = (uint32_t)file_buf[LOGO_TABLE_OFFSET] |
            ((uint32_t)file_buf[LOGO_TABLE_OFFSET + 1] << 8) |
            ((uint32_t)file_buf[LOGO_TABLE_OFFSET + 2] << 16) |
            ((uint32_t)file_buf[LOGO_TABLE_OFFSET + 3] << 24);
    if (count == 0 || count > 4096u)
        return -EINVAL;
    table_len = 8u + count * 4u;
    if ((size_t)LOGO_TABLE_OFFSET + table_len > len)
        return -EINVAL;

    r->offsets = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!r->offsets)
        return -ENOMEM;

    for (i = 0; i < count; ++i) {
        const uint8_t *p = file_buf + LOGO_TABLE_OFFSET + 8u + i * 4u;
        r->offsets[i] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        if (i > 0 && r->offsets[i] <= r->offsets[i - 1]) {
            free(r->offsets);
            r->offsets = NULL;
            return -EINVAL;
        }
        if ((size_t)LOGO_TABLE_OFFSET + r->offsets[i] > len) {
            free(r->offsets);
            r->offsets = NULL;
            return -EINVAL;
        }
    }
    r->file_buf = file_buf;
    r->file_len = len;
    r->count = count;
    r->fb = fb;
    r->version = 1;
    r->idx_ov = LOGO_IDX_ERROR_OV;
    r->idx_low = LOGO_IDX_LOW_BATT;
    r->idx_full = LOGO_IDX_FULL;
    r->idx_low_red0 = LOGO_IDX_LOW_RED0;
    r->idx_green0 = LOGO_IDX_GREEN0;
    r->ready = 1;
    return 0;
}

void logo_render_deinit(LogoRenderer *r)
{
    if (!r)
        return;
    free(r->offsets);
    r->offsets = NULL;
    r->ready = 0;
}

static int dims_for_size(const LogoFB *fb, size_t bytes,
                         uint32_t *w, uint32_t *h)
{
    uint64_t full = (uint64_t)fb->width * fb->height * LOGO_BPP;

    if (bytes == full) { *w = fb->width; *h = fb->height; return 0; }
    if (bytes == LOGO_DIGIT_W * LOGO_DIGIT_H * LOGO_BPP) {
        *w = LOGO_DIGIT_W; *h = LOGO_DIGIT_H; return 0;
    }
    if (bytes == LOGO_PERCENT_W * LOGO_PERCENT_H * LOGO_BPP) {
        *w = LOGO_PERCENT_W; *h = LOGO_PERCENT_H; return 0;
    }
    if (bytes == LOGO_WAVE_W * LOGO_WAVE_H * LOGO_BPP) {
        *w = LOGO_WAVE_W; *h = LOGO_WAVE_H; return 0;
    }
    if (bytes == LOGO_LINE_W * LOGO_LINE_H * LOGO_BPP) {
        *w = LOGO_LINE_W; *h = LOGO_LINE_H; return 0;
    }
    return -EINVAL;
}

int logo_decode(LogoRenderer *r, unsigned index, uint8_t **out,
                uint32_t *w, uint32_t *h)
{
    size_t start, end, src_len, cap;
    uint8_t *dst;
    uLongf dst_len;
    int zr;

    if (!r || !r->ready || !out || index >= r->count)
        return -EINVAL;

    start = LOGO_TABLE_OFFSET + r->offsets[index];
    end = (index + 1 < r->count)
              ? LOGO_TABLE_OFFSET + r->offsets[index + 1]
              : r->file_len;
    if (end <= start || start >= r->file_len)
        return -EINVAL;
    src_len = end - start;

    cap = (size_t)r->fb->width * r->fb->height * LOGO_BPP;
    if (cap < 1024u)
        cap = 1024u;
    dst = (uint8_t *)malloc(cap);
    if (!dst)
        return -ENOMEM;

    dst_len = (uLongf)cap;
    zr = uncompress(dst, &dst_len, r->file_buf + start, (uLong)src_len);
    if (zr != Z_OK || dst_len == 0) {
        free(dst);
        return -EINVAL;
    }
    if (dims_for_size(r->fb, (size_t)dst_len, w, h) != 0) {
        free(dst);
        return -EINVAL;
    }
    *out = dst;
    return 0;
}

                                                                           
static void convert_pixel(const uint8_t *src, uint8_t *dst, const LogoFB *fb)
{
    uint8_t b = src[0], g = src[1], red = src[2];

    if (fb->bpp == 16) {
        uint16_t rgb565 = (uint16_t)(((red >> 3) << 11) |
                                     ((g >> 2) << 5) | (b >> 3));
        dst[0] = (uint8_t)(rgb565 & 0xff);
        dst[1] = (uint8_t)(rgb565 >> 8);
        return;
    }
    dst[fb->red_offset / 8u]   = red;
    dst[fb->green_offset / 8u] = g;
    dst[fb->blue_offset / 8u]  = b;
    dst[3] = 0xff;
}

int logo_present(LogoRenderer *r, unsigned index)
{
    uint8_t *bgra = NULL;
    uint8_t *native;
    uint32_t w, h, y, x, bytes_pp;
    int rc;

    if (!r || !r->ready || !r->fb || !r->fb->present)
        return -EINVAL;
    rc = logo_decode(r, index, &bgra, &w, &h);
    if (rc != 0)
        return rc;

    bytes_pp = r->fb->bpp / 8u;
    if (bytes_pp == 0 || w > r->fb->width || h > r->fb->height) {
        free(bgra);
        return -EINVAL;
    }
    native = (uint8_t *)malloc((size_t)w * h * bytes_pp);
    if (!native) {
        free(bgra);
        return -ENOMEM;
    }
    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            const uint8_t *src = bgra + ((size_t)y * w + x) * LOGO_BPP;
            uint8_t *dst = native + ((size_t)y * w + x) * bytes_pp;
            convert_pixel(src, dst, r->fb);
        }
    }
    rc = r->fb->present(r->fb->ctx, 0, native, w, h, bytes_pp);
    free(native);
    free(bgra);
    return rc;
}

void logo_set_version(LogoRenderer *r, int version)
{
    if (!r)
        return;
    r->version = (version < 0 || version > 2) ? 0 : version;
}

void logo_show_capacity(LogoRenderer *r, unsigned cap)
{
    unsigned idx;

    if (!r || !r->ready)
        return;
    if (cap >= 100u)
        idx = r->idx_full;
    else if (cap <= 10u)
        idx = r->idx_low_red0 + cap;
    else
        idx = r->idx_green0 + (cap - 10u) * 10u / 90u;
    if (idx > LOGO_IDX_LAST)
        idx = LOGO_IDX_LAST;
    {
        int rc = logo_present(r, idx);
        logo_last_present_rc = rc;
        logo_last_present_idx = idx;
    }
}

void logo_show_ov(LogoRenderer *r)
{
    if (r && r->ready)
        (void)logo_present(r, r->idx_ov);
}

void logo_show_low(LogoRenderer *r)
{
    if (r && r->ready)
        (void)logo_present(r, r->idx_low);
}

int logo_read_file(const char *path, uint8_t **out, size_t *len)
{
    int fd;
    size_t cap = 64u * 1024u, used = 0;
    uint8_t *buf;

    if (!path || !out || !len)
        return -EINVAL;
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -errno;
    buf = (uint8_t *)malloc(cap);
    if (!buf) {
        close(fd);
        return -ENOMEM;
    }
    for (;;) {
        ssize_t n;
        if (used == cap) {
            uint8_t *bigger;
            if (cap > SIZE_MAX / 2u) { free(buf); close(fd); return -ENOMEM; }
            bigger = (uint8_t *)realloc(buf, cap * 2u);
            if (!bigger) { free(buf); close(fd); return -ENOMEM; }
            buf = bigger;
            cap *= 2u;
        }
        n = read(fd, buf + used, cap - used);
        if (n < 0) {
            if (errno == EINTR) continue;
            free(buf); close(fd); return -errno;
        }
        if (n == 0) break;
        used += (size_t)n;
    }
    close(fd);
    *out = buf;
    *len = used;
    return 0;
}
