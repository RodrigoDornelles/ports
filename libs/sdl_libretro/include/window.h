#ifndef DOPO_WINDOW_H
#define DOPO_WINDOW_H

#include <stdbool.h>
#include <stdint.h>

typedef struct win_window win_window;
typedef struct win_cursor win_cursor;

/* GL attributes; the first sixteen match both SDL_GLattr enumerations */
typedef enum {
    WIN_GL_RED_SIZE = 0,
    WIN_GL_GREEN_SIZE,
    WIN_GL_BLUE_SIZE,
    WIN_GL_ALPHA_SIZE,
    WIN_GL_BUFFER_SIZE,
    WIN_GL_DOUBLEBUFFER,
    WIN_GL_DEPTH_SIZE,
    WIN_GL_STENCIL_SIZE,
    WIN_GL_ACCUM_RED_SIZE,
    WIN_GL_ACCUM_GREEN_SIZE,
    WIN_GL_ACCUM_BLUE_SIZE,
    WIN_GL_ACCUM_ALPHA_SIZE,
    WIN_GL_STEREO,
    WIN_GL_MULTISAMPLEBUFFERS,
    WIN_GL_MULTISAMPLESAMPLES,
    WIN_GL_ACCELERATED_VISUAL,
    WIN_GL_CONTEXT_MAJOR_VERSION,
    WIN_GL_CONTEXT_MINOR_VERSION,
    WIN_GL_CONTEXT_FLAGS,
    WIN_GL_CONTEXT_PROFILE_MASK,
    WIN_GL_SHARE_WITH_CURRENT_CONTEXT,
    WIN_GL_ATTR_COUNT,
} win_gl_attr_t;

/* profile mask bits, matching SDL2 */
#define WIN_GL_PROFILE_CORE          0x0001
#define WIN_GL_PROFILE_COMPATIBILITY 0x0002
#define WIN_GL_PROFILE_ES            0x0004

/* context flag bits, matching SDL2 */
#define WIN_GL_CONTEXT_DEBUG              0x0001
#define WIN_GL_CONTEXT_FORWARD_COMPATIBLE 0x0002

typedef enum {
    WIN_CURSOR_ARROW = 0,
    WIN_CURSOR_IBEAM,
    WIN_CURSOR_WAIT,
    WIN_CURSOR_CROSSHAIR,
    WIN_CURSOR_SIZENWSE,
    WIN_CURSOR_SIZENESW,
    WIN_CURSOR_SIZEWE,
    WIN_CURSOR_SIZENS,
    WIN_CURSOR_SIZEALL,
    WIN_CURSOR_NO,
    WIN_CURSOR_HAND,
    WIN_CURSOR_COUNT,
} win_cursor_shape_t;

typedef struct {
    const char *title;
    int         x, y, w, h;
    bool        fullscreen;
    bool        hidden;
    bool        opengl;
} win_config_t;

/* raised by win_pump(), implemented by each frontend */
void win_on_close(void);
void win_on_resize(int w, int h);
void win_on_move(int x, int y);
void win_on_expose(void);
void win_on_focus(bool gained);
void win_on_map(bool mapped);

bool win_init(void);
bool win_gl_init(void);
void win_quit(void);
void win_pump(void);
bool win_has_x(void);
bool win_synthetic_focus(void);
void win_desktop_size(int *w, int *h);
int  win_display_dpi(float *dpi);
const char *win_driver(void);

win_window *win_create(const win_config_t *cfg);
void        win_destroy(win_window *win);
void        win_geometry(const win_window *win, int *x, int *y, int *w, int *h);
void        win_set_title(win_window *win, const char *title);
void        win_set_size(win_window *win, int w, int h);
void        win_set_position(win_window *win, int x, int y);
void        win_set_fullscreen(win_window *win, bool enable);
void        win_set_visible(win_window *win, bool visible);
void        win_raise(win_window *win);
void        win_set_grab(win_window *win, bool grabbed);

int   win_gl_set_attribute(win_gl_attr_t attr, int value);
int   win_gl_get_attribute(win_gl_attr_t attr, int *value);
void  win_gl_reset_attributes(void);
void *win_gl_create_context(win_window *win);
int   win_gl_make_current(win_window *win, void *context);
void  win_gl_delete_context(void *context);
void *win_gl_current_context(void);
void  win_gl_swap(win_window *win);
int   win_gl_set_swap_interval(int interval);
int   win_gl_get_swap_interval(void);
void *win_gl_proc_address(const char *proc);
void  win_gl_drawable_size(win_window *win, int *w, int *h);

win_cursor *win_cursor_system(win_cursor_shape_t shape);
win_cursor *win_cursor_color(const uint32_t *argb, int w, int h, int hot_x, int hot_y);
void        win_cursor_free(win_cursor *cursor);
void        win_cursor_set(win_cursor *cursor);
win_cursor *win_cursor_default(void);
win_cursor *win_cursor_current(void);
void        win_cursor_show(bool shown);
bool        win_cursor_shown(void);

#endif
