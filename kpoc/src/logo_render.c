  
                                                                    
                                                                    
   
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
    r->canvas_w = fb->width;
    r->canvas_h = fb->height;
    r->canvas = (uint32_t *)calloc((size_t)fb->width * fb->height,
                                   sizeof(uint32_t));
    r->packed = (uint8_t *)malloc((size_t)fb->width * fb->height *
                                  (fb->bpp / 8u));
    if (!r->canvas || !r->packed) {
        free(r->canvas);
        free(r->packed);
        r->canvas = NULL;
        r->packed = NULL;
        free(r->offsets);
        r->offsets = NULL;
        return -ENOMEM;
    }
    r->ready = 1;
    return 0;
}

void logo_render_deinit(LogoRenderer *r)
{
    if (!r)
        return;
    free(r->offsets);
    free(r->canvas);
    free(r->packed);
    r->offsets = NULL;
    r->canvas = NULL;
    r->packed = NULL;
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

                                                                           
static void convert_pixel(uint32_t px_rgba, uint8_t *dst, const LogoFB *fb)
{
    uint8_t red = (uint8_t)(px_rgba & 0xffu);
    uint8_t g   = (uint8_t)((px_rgba >> 8) & 0xffu);
    uint8_t b   = (uint8_t)((px_rgba >> 16) & 0xffu);

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
            const uint32_t *src =
                (const uint32_t *)(bgra + ((size_t)y * w + x) * LOGO_BPP);
            uint8_t *dst = native + ((size_t)y * w + x) * bytes_pp;
            convert_pixel(*src, dst, r->fb);
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

int logo_compose_begin(LogoRenderer *r)
{
    if (!r || !r->ready || !r->canvas)
        return -EINVAL;
    memset(r->canvas, 0, (size_t)r->canvas_w * r->canvas_h *
                         sizeof(uint32_t));
    return 0;
}

static void blend_sprite(uint32_t *dst, const uint32_t *src,
                         uint32_t cw, uint32_t ch,
                         int32_t ox, int32_t oy, uint32_t w, uint32_t h)
{
    uint32_t y, x;

    for (y = 0; y < h; ++y) {
        int64_t ty = (int64_t)oy + y;
        if (ty < 0 || ty >= (int64_t)ch)
            continue;
        for (x = 0; x < w; ++x) {
            int64_t tx = (int64_t)ox + x;
            uint32_t s, d, a, inv, r, g, b;
            if (tx < 0 || tx >= (int64_t)cw)
                continue;
            s = src[(size_t)y * w + x];
            a = (s >> 24) & 0xffu;
            if (a == 0)
                continue;
            d = dst[(size_t)ty * cw + (size_t)tx];
            if (a == 0xff) {
                dst[(size_t)ty * cw + (size_t)tx] = s | 0xff000000u;
                continue;
            }
            inv = 255u - a;
            r = (((s & 0xffu) * a) + ((d & 0xffu) * inv)) / 255u;
            g = ((((s >> 8) & 0xffu) * a) + (((d >> 8) & 0xffu) * inv)) / 255u;
            b = ((((s >> 16) & 0xffu) * a) + (((d >> 16) & 0xffu) * inv)) / 255u;
            dst[(size_t)ty * cw + (size_t)tx] =
                0xff000000u | (b << 16) | (g << 8) | r;
        }
    }
}

int logo_compose_sprite(LogoRenderer *r, unsigned index, int32_t x, int32_t y)
{
    uint8_t *bgra = NULL;
    uint32_t w = 0, h = 0;
    int rc;

    if (!r || !r->ready || !r->canvas)
        return -EINVAL;
    rc = logo_decode(r, index, &bgra, &w, &h);
    if (rc != 0)
        return rc;
    blend_sprite(r->canvas, (const uint32_t *)bgra,
                 r->canvas_w, r->canvas_h, x, y, w, h);
    free(bgra);
    return 0;
}

static int keyed_sprite(LogoRenderer *r, unsigned index, int32_t x, int32_t y)
{
    uint8_t *bgra = NULL;
    uint32_t w = 0, h = 0, y2, x2;
    const uint32_t *src;
    uint32_t key;
    int rc;

    if (!r || !r->ready || !r->canvas)
        return -EINVAL;
    rc = logo_decode(r, index, &bgra, &w, &h);
    if (rc != 0)
        return rc;
    if (w == 0 || h == 0) {
        free(bgra);
        return -EINVAL;
    }
    src = (const uint32_t *)bgra;
    key = src[0] & 0x00ffffffu;

    for (y2 = 0; y2 < h; ++y2) {
        int64_t ty = (int64_t)y + y2;
        if (ty < 0 || ty >= (int64_t)r->canvas_h)
            continue;
        for (x2 = 0; x2 < w; ++x2) {
            int64_t tx = (int64_t)x + x2;
            uint32_t s;
            if (tx < 0 || tx >= (int64_t)r->canvas_w)
                continue;
            s = src[(size_t)y2 * w + x2];
            if ((s & 0x00ffffffu) == key)
                continue;
            r->canvas[(size_t)ty * r->canvas_w + (size_t)tx] = s | 0xff000000u;
        }
    }
    free(bgra);
    return 0;
}

int logo_compose_keyed(LogoRenderer *r, unsigned index, int32_t x, int32_t y)
{
    return keyed_sprite(r, index, x, y);
}

int logo_compose_line(LogoRenderer *r, int32_t x0, int32_t y, int32_t x1)
{
    uint8_t *bgra = NULL;
    uint32_t w = 0, h = 0;
    int32_t x;

    if (!r || !r->ready || !r->canvas)
        return -EINVAL;
    if (x1 <= x0)
        return -EINVAL;
    if (logo_decode(r, LOGO_IDX_LINE, &bgra, &w, &h) != 0)
        return -EINVAL;
    if (h == 0 || w == 0) {
        free(bgra);
        return -EINVAL;
    }

    {
        const uint32_t *row = (const uint32_t *)(bgra +
                              (size_t)(h - 1u) * w * LOGO_BPP);
        int64_t ty = y;
        if (ty >= 0 && ty < (int64_t)r->canvas_h) {
            uint32_t *dst = r->canvas + (size_t)ty * r->canvas_w;
            uint32_t sx = 0;
            for (x = x0; x < x1; ++x, ++sx) {
                uint32_t s, d, a, inv, cr, cg, cb;
                if (x < 0 || x >= (int32_t)r->canvas_w)
                    continue;
                if (sx == w)
                    sx = 0;
                s = row[sx];
                a = (s >> 24) & 0xffu;
                if (a == 0)
                    continue;
                d = dst[x];
                inv = 255u - a;
                cr = (((s & 0xffu) * a) + ((d & 0xffu) * inv)) / 255u;
                cg = ((((s >> 8) & 0xffu) * a) + (((d >> 8) & 0xffu) * inv)) / 255u;
                cb = ((((s >> 16) & 0xffu) * a) + (((d >> 16) & 0xffu) * inv)) / 255u;
                dst[x] = 0xff000000u | (cb << 16) | (cg << 8) | cr;
            }
        }
    }
    free(bgra);
    return 0;
}

int logo_compose_flush(LogoRenderer *r)
{
    uint32_t y, x, bytes_pp;

    if (!r || !r->ready || !r->fb || !r->fb->present || !r->packed)
        return -EINVAL;
    bytes_pp = r->fb->bpp / 8u;
    if (bytes_pp == 0)
        return -EINVAL;

    for (y = 0; y < r->canvas_h; ++y) {
        for (x = 0; x < r->canvas_w; ++x) {
            uint32_t px = r->canvas[(size_t)y * r->canvas_w + x];
            convert_pixel(px, r->packed +
                          ((size_t)y * r->canvas_w + x) * bytes_pp, r->fb);
        }
    }
    return r->fb->present(r->fb->ctx, 0, r->packed, r->canvas_w,
                          r->canvas_h, bytes_pp);
}

static int draw_number(LogoRenderer *r, unsigned digit, unsigned pos,
                       int32_t x0, int32_t ytop, unsigned base)
{
    int32_t x = x0 + (int32_t)(pos * LOGO_DIGIT_W);

    if (digit > 9u)
        return -EINVAL;
    return logo_compose_keyed(r, base + digit, x, ytop);
}

static void draw_grids(LogoRenderer *r, unsigned grids)
{
    int32_t x0 = (int32_t)LOGO_AREA_X0;
    int32_t x1 = (int32_t)LOGO_AREA_X1;
    int32_t top = (int32_t)LOGO_AREA_Y0;
    int32_t bottom = (int32_t)LOGO_AREA_Y1;
    uint8_t *bgra = NULL;
    uint32_t w = 0, h = 0;
    const uint32_t *row;
    int32_t y;

    if (grids > LOGO_MAX_GRIDS)
        grids = LOGO_MAX_GRIDS;
    if (grids == 0)
        return;
    if (logo_decode(r, LOGO_IDX_LINE, &bgra, &w, &h) != 0)
        return;
    if (w == 0 || h == 0) {
        free(bgra);
        return;
    }
    row = (const uint32_t *)(bgra + (size_t)(h - 1u) * w * LOGO_BPP);

    y = bottom - (int32_t)((uint64_t)grids *
                           (uint64_t)(bottom - top) / LOGO_MAX_GRIDS);

    for (; y < bottom; ++y) {
        uint32_t *dst;
        int32_t x;

        if (y < 0 || y >= (int32_t)r->canvas_h)
            continue;
        dst = r->canvas + (size_t)y * r->canvas_w;
        for (x = x0; x < x1 && x < (int32_t)r->canvas_w; ++x) {
            uint32_t px;
            if (x < 0)
                continue;
            px = row[(uint32_t)(x - x0) % w];
            if ((px >> 24) == 0)
                continue;
            dst[x] = px | 0xff000000u;
        }
    }
    free(bgra);
}

/* Draws the battery body, the capacity bar, the percentage digits and the
 * wave overlay as separate sprites, matching how stock composes a frame.
 */
void logo_show_layout(LogoRenderer *r, unsigned cap)
{
    int64_t ytop;
    unsigned grids;

    if (!r || !r->ready)
        return;
    if (logo_compose_begin(r) != 0)
        return;

    if (cap > 100u)
        cap = 100u;

    if (cap >= 100u) {
        (void)logo_compose_sprite(r, LOGO_IDX_GREEN_FULL, 0, 0);
        (void)logo_compose_flush(r);
        return;
    }

    (void)logo_compose_sprite(r, LOGO_IDX_BATTERY, 0, 0);

    grids = (cap * LOGO_MAX_GRIDS) / 100u;
    draw_grids(r, grids);

    {
        unsigned span = (cap >= 10u) ? (2u * LOGO_DIGIT_W + LOGO_PERCENT_W)
                                     : (LOGO_DIGIT_W + LOGO_PERCENT_W);
        int32_t xstart = (int32_t)(LOGO_AREA_X0 +
                                   ((LOGO_AREA_X1 - LOGO_AREA_X0) - span) / 2u);
        unsigned ndig = (cap >= 10u) ? 2u : 1u;

        ytop = (int64_t)LOGO_DIGIT_TOP;
        if (cap >= 10u) {
            (void)draw_number(r, cap / 10u, 0, xstart, (int32_t)ytop, LOGO_IDX_DIGIT0);
            (void)draw_number(r, cap % 10u, 1, xstart, (int32_t)ytop, LOGO_IDX_DIGIT0);
        } else {
            (void)draw_number(r, cap, 0, xstart, (int32_t)ytop, LOGO_IDX_DIGIT0);
        }
        (void)logo_compose_keyed(r, LOGO_IDX_PERCENT,
                                 xstart + (int32_t)(ndig * LOGO_DIGIT_W),
                                 (int32_t)ytop);
    }

    {
        int rc = logo_compose_flush(r);
        logo_last_present_rc = rc;
        logo_last_present_idx = LOGO_IDX_BATTERY;
    }
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
        idx = LOGO_IDX_BATTERY;
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
