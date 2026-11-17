  
                                                                              
  
                                                                              
                                                                             
                                                                    
  
                                                                            
                                                                            
   
#ifndef LOGO_RENDER_H
#define LOGO_RENDER_H

#include <stdint.h>
#include <stddef.h>

                                                               
#define LOGO_IDX_ERROR_OV      3u                                           
#define LOGO_IDX_LOW_BATT      2u
#define LOGO_IDX_FULL         37u                            
#define LOGO_IDX_DIGIT0        4u
#define LOGO_IDX_PERCENT      14u
#define LOGO_IDX_WAVE0        15u
#define LOGO_IDX_LOW_RED0     25u
#define LOGO_IDX_GREEN0       39u
#define LOGO_IDX_LAST         60u

extern int logo_last_present_rc;
extern unsigned logo_last_present_idx;

                                                                            
#define LOGO_DIGIT_W   42u
#define LOGO_DIGIT_H   60u
#define LOGO_PERCENT_W 48u
#define LOGO_PERCENT_H 60u
#define LOGO_WAVE_W    42u
#define LOGO_WAVE_H    64u
#define LOGO_LINE_W    14u
#define LOGO_LINE_H    16u

                                                             
typedef struct LogoFB {
    uint32_t width;                    
    uint32_t height;                   
    uint32_t yres_virtual;                                       
    uint32_t line_length;                           
    uint32_t bpp;                                                 
    uint8_t  red_offset;
    uint8_t  green_offset;
    uint8_t  blue_offset;
                                                                             
                                                                            
    int (*present)(void *ctx, uint32_t yoffset, const uint8_t *pixels,
                   uint32_t w, uint32_t h, uint32_t bytes_per_pixel);
    void *ctx;
} LogoFB;

typedef struct LogoRenderer {
    const uint8_t  *file_buf;                 
    size_t          file_len;
    uint32_t        count;
    uint32_t       *offsets;                         
    const LogoFB   *fb;                       
    int             version;                                    
    int             ready;
    unsigned        wave_frame;

                                                                               
                                                                 
    unsigned idx_ov;
    unsigned idx_low;
    unsigned idx_full;
    unsigned idx_low_red0;
    unsigned idx_green0;
} LogoRenderer;

#ifdef __cplusplus
extern "C" {
#endif

                                                             
                                                      
int  logo_render_init(LogoRenderer *r, const uint8_t *file_buf, size_t len,
                      const LogoFB *fb);
void logo_render_deinit(LogoRenderer *r);

                                                                             
                                                      
int  logo_decode(LogoRenderer *r, unsigned index, uint8_t **out,
                 uint32_t *w, uint32_t *h);

                                                                    
int  logo_present(LogoRenderer *r, unsigned index);

                                                             
void logo_set_version(LogoRenderer *r, int version);
void logo_show_capacity(LogoRenderer *r, unsigned capacity);
void logo_show_ov(LogoRenderer *r);
void logo_show_low(LogoRenderer *r);

                                                                               
int  logo_read_file(const char *path, uint8_t **out, size_t *len);

                                                                                 
int  kpoc_fb_open(LogoFB *out);
void kpoc_fb_close(LogoFB *fb);

#ifdef __cplusplus
}
#endif

#endif                    
