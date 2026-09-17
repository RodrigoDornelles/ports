#define _GNU_SOURCE
#include <stdarg.h>

#include "sdl1.h"

int SDL_CDNumDrives(void) {
    return 0;
}

const char *SDL_CDName(int drive) {
    (void)drive;
    return NULL;
}

SDL_CD *SDL_CDOpen(int drive) {
    (void)drive;
    shim_set_error("no cdrom drives available");
    return NULL;
}

CDstatus SDL_CDStatus(SDL_CD *cdrom) {
    (void)cdrom;
    return CD_ERROR;
}

int SDL_CDPlay(SDL_CD *cdrom, int start, int length) {
    (void)cdrom; (void)start; (void)length;
    return -1;
}

int SDL_CDPlayTracks(SDL_CD *cdrom, int start_track, int start_frame,
                     int ntracks, int nframes) {
    (void)cdrom; (void)start_track; (void)start_frame; (void)ntracks; (void)nframes;
    return -1;
}

int SDL_CDPause(SDL_CD *cdrom)  { (void)cdrom; return -1; }
int SDL_CDResume(SDL_CD *cdrom) { (void)cdrom; return -1; }
int SDL_CDStop(SDL_CD *cdrom)   { (void)cdrom; return -1; }
int SDL_CDEject(SDL_CD *cdrom)  { (void)cdrom; return -1; }
void SDL_CDClose(SDL_CD *cdrom) { (void)cdrom; }

int SDL_SetGamma(float red, float green, float blue) {
    (void)red; (void)green; (void)blue;
    return -1;
}

int SDL_SetGammaRamp(const Uint16 *red, const Uint16 *green, const Uint16 *blue) {
    (void)red; (void)green; (void)blue;
    return -1;
}

int SDL_GetGammaRamp(Uint16 *red, Uint16 *green, Uint16 *blue) {
    (void)red; (void)green; (void)blue;
    return -1;
}

SDL_Overlay *SDL_CreateYUVOverlay(int width, int height, Uint32 format, SDL_Surface *display) {
    (void)width; (void)height; (void)format; (void)display;
    shim_set_error("yuv overlays are not supported");
    return NULL;
}

int SDL_LockYUVOverlay(SDL_Overlay *overlay) {
    (void)overlay;
    return -1;
}

void SDL_UnlockYUVOverlay(SDL_Overlay *overlay) {
    (void)overlay;
}

int SDL_DisplayYUVOverlay(SDL_Overlay *overlay, SDL_Rect *dstrect) {
    (void)overlay; (void)dstrect;
    return -1;
}

void SDL_FreeYUVOverlay(SDL_Overlay *overlay) {
    (void)overlay;
}

int SDL_BuildAudioCVT(SDL_AudioCVT *cvt, Uint16 src_format, Uint8 src_channels, int src_rate,
                      Uint16 dst_format, Uint8 dst_channels, int dst_rate) {
    (void)cvt; (void)src_format; (void)src_channels; (void)src_rate;
    (void)dst_format; (void)dst_channels; (void)dst_rate;
    shim_set_error("audio conversion is not supported");
    return -1;
}

int SDL_ConvertAudio(SDL_AudioCVT *cvt) {
    (void)cvt;
    shim_set_error("audio conversion is not supported");
    return -1;
}

/* SDL_syswm.h needs a video driver macro to shape SDL_SysWMinfo, so the
   argument stays opaque here; there is no native window to hand out anyway */
int SDL_GetWMInfo(void *info) {
    (void)info;
    return 0;
}

size_t SDL_strlcpy(char *dst, const char *src, size_t maxlen) {
    size_t length = strlen(src);
    if (maxlen) {
        size_t copy = length < maxlen - 1 ? length : maxlen - 1;
        memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return length;
}

size_t SDL_strlcat(char *dst, const char *src, size_t maxlen) {
    size_t used = strlen(dst);
    if (used >= maxlen) return used + strlen(src);
    return used + SDL_strlcpy(dst + used, src, maxlen - used);
}

int SDL_vsnprintf(char *text, size_t maxlen, const char *fmt, va_list ap) {
    return vsnprintf(text, maxlen, fmt, ap);
}

int SDL_snprintf(char *text, size_t maxlen, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(text, maxlen, fmt, ap);
    va_end(ap);
    return n;
}

void SDL_qsort(void *base, size_t nmemb, size_t size,
               int (*compare)(const void *, const void *)) {
    qsort(base, nmemb, size, compare);
}
