#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "shim.h"

/*
 * A fake libXrandr to go with the fake libX11: one output, one crtc, one mode,
 * the size the display reports. Mode switching is accepted and ignored, the
 * core scales whatever comes out.
 */

#define RR_MODE_ID   1
#define RR_CRTC_ID   1
#define RR_OUTPUT_ID 1

static void screen_size(int *w, int *h) {
    const char *ew = getenv(DOPO_ENV_WIDTH);
    const char *eh = getenv(DOPO_ENV_HEIGHT);
    *w = ew ? atoi(ew) : 1920;
    *h = eh ? atoi(eh) : 1080;
    if (*w <= 0) *w = 1920;
    if (*h <= 0) *h = 1080;
}

Bool XRRQueryExtension(Display *dpy, int *event_base_return, int *error_base_return) {
    (void)dpy;
    if (event_base_return) *event_base_return = 0;
    if (error_base_return) *error_base_return = 0;
    return True;
}

Status XRRQueryVersion(Display *dpy, int *major_version_return, int *minor_version_return) {
    (void)dpy;
    if (major_version_return) *major_version_return = 1;
    if (minor_version_return) *minor_version_return = 5;
    return 1;
}

XRRScreenResources *XRRGetScreenResources(Display *dpy, Window window) {
    (void)dpy; (void)window;

    XRRScreenResources *res = calloc(1, sizeof(*res));
    if (!res) return NULL;

    res->crtcs   = calloc(1, sizeof(RRCrtc));
    res->outputs = calloc(1, sizeof(RROutput));
    res->modes   = calloc(1, sizeof(XRRModeInfo));
    if (!res->crtcs || !res->outputs || !res->modes) {
        free(res->crtcs);
        free(res->outputs);
        free(res->modes);
        free(res);
        return NULL;
    }

    int width, height;
    screen_size(&width, &height);

    res->ncrtc      = 1;
    res->noutput    = 1;
    res->nmode      = 1;
    res->crtcs[0]   = RR_CRTC_ID;
    res->outputs[0] = RR_OUTPUT_ID;

    res->modes[0].id     = RR_MODE_ID;
    res->modes[0].width  = (unsigned)width;
    res->modes[0].height = (unsigned)height;
    res->modes[0].dotClock  = (unsigned long)width * (unsigned long)height * 60;
    res->modes[0].hTotal    = (unsigned)width;
    res->modes[0].vTotal    = (unsigned)height;
    res->modes[0].name      = (char *)"default";
    res->modes[0].nameLength = 7;
    return res;
}

XRRScreenResources *XRRGetScreenResourcesCurrent(Display *dpy, Window window) {
    return XRRGetScreenResources(dpy, window);
}

void XRRFreeScreenResources(XRRScreenResources *resources) {
    if (!resources) return;
    free(resources->crtcs);
    free(resources->outputs);
    free(resources->modes);
    free(resources);
}

XRROutputInfo *XRRGetOutputInfo(Display *dpy, XRRScreenResources *resources, RROutput output) {
    (void)dpy; (void)resources; (void)output;

    XRROutputInfo *info = calloc(1, sizeof(*info));
    if (!info) return NULL;

    info->crtcs = calloc(1, sizeof(RRCrtc));
    info->modes = calloc(1, sizeof(RRMode));
    info->name  = calloc(8, 1);
    if (!info->crtcs || !info->modes || !info->name) {
        free(info->crtcs);
        free(info->modes);
        free(info->name);
        free(info);
        return NULL;
    }

    memcpy(info->name, "default", 7);
    info->nameLen    = 7;
    info->crtc       = RR_CRTC_ID;
    info->ncrtc      = 1;
    info->crtcs[0]   = RR_CRTC_ID;
    info->nmode      = 1;
    info->npreferred = 1;
    info->modes[0]   = RR_MODE_ID;
    info->connection = RR_Connected;
    return info;
}

void XRRFreeOutputInfo(XRROutputInfo *outputInfo) {
    if (!outputInfo) return;
    free(outputInfo->crtcs);
    free(outputInfo->modes);
    free(outputInfo->clones);
    free(outputInfo->name);
    free(outputInfo);
}

RROutput XRRGetOutputPrimary(Display *dpy, Window window) {
    (void)dpy; (void)window;
    return RR_OUTPUT_ID;
}

XRRCrtcInfo *XRRGetCrtcInfo(Display *dpy, XRRScreenResources *resources, RRCrtc crtc) {
    (void)dpy; (void)resources; (void)crtc;

    XRRCrtcInfo *info = calloc(1, sizeof(*info));
    if (!info) return NULL;

    info->outputs = calloc(1, sizeof(RROutput));
    if (!info->outputs) {
        free(info);
        return NULL;
    }

    int width, height;
    screen_size(&width, &height);

    info->mode       = RR_MODE_ID;
    info->rotation   = RR_Rotate_0;
    info->rotations  = RR_Rotate_0;
    info->width      = (unsigned)width;
    info->height     = (unsigned)height;
    info->noutput    = 1;
    info->outputs[0] = RR_OUTPUT_ID;
    return info;
}

void XRRFreeCrtcInfo(XRRCrtcInfo *crtcInfo) {
    if (!crtcInfo) return;
    free(crtcInfo->outputs);
    free(crtcInfo->possible);
    free(crtcInfo);
}

Status XRRSetCrtcConfig(Display *dpy, XRRScreenResources *resources, RRCrtc crtc,
                        Time timestamp, int x, int y, RRMode mode, Rotation rotation,
                        RROutput *outputs, int noutputs) {
    (void)dpy; (void)resources; (void)crtc; (void)timestamp; (void)x; (void)y;
    (void)mode; (void)rotation; (void)outputs; (void)noutputs;
    return RRSetConfigSuccess;
}

XRRCrtcGamma *XRRGetCrtcGamma(Display *dpy, RRCrtc crtc) {
    (void)dpy; (void)crtc;
    return XRRAllocGamma(256);
}

XRRCrtcGamma *XRRAllocGamma(int size) {
    if (size <= 0) return NULL;

    XRRCrtcGamma *gamma = calloc(1, sizeof(*gamma));
    unsigned short *ramp = calloc((size_t)size * 3, sizeof(unsigned short));
    if (!gamma || !ramp) {
        free(gamma);
        free(ramp);
        return NULL;
    }
    gamma->size  = size;
    gamma->red   = ramp;
    gamma->green = ramp + size;
    gamma->blue  = ramp + size * 2;
    return gamma;
}

void XRRSetCrtcGamma(Display *dpy, RRCrtc crtc, XRRCrtcGamma *gamma) {
    (void)dpy; (void)crtc; (void)gamma;
}

int XRRGetCrtcGammaSize(Display *dpy, RRCrtc crtc) {
    (void)dpy; (void)crtc;
    return 256;
}

void XRRFreeGamma(XRRCrtcGamma *gamma) {
    if (!gamma) return;
    free(gamma->red);
    free(gamma);
}

void XRRSelectInput(Display *dpy, Window window, int mask) {
    (void)dpy; (void)window; (void)mask;
}
