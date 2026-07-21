/* render.c — per-notification X11 render child
 *
 * Receives fixed-size NotifMsg packets over a pipe from doid.
 * Uses poll(2) to multiplex the pipe fd and the X11 connection fd.
 *
 * Config resolution rule (applied in paint/measure/render_loop):
 *   Every NotifMsg field that overrides a config.h value uses the
 *   sentinel DOI_DEFAULT (-999) to mean "use the compiled-in default".
 *   The daemon zeroes NotifMsg with memset, then sets only the fields
 *   it wants to override — so EVERY field that has a config.h default
 *   must be initialised to DOI_DEFAULT by the daemon before sending,
 *   not left at 0 (which is a valid value, e.g. BORDER=0).
 *
 *   Macros CFG_I / CFG_S below handle the resolution in one place.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <X11/Xft/Xft.h>
#include "notif.h"
#include "log.h"
#include "../config.h"

/* Sentinel: field value meaning "fall back to the config.h default". */
#define DOI_DEFAULT (-999)

/* Resolve an integer field from a NotifMsg pointer P. */
#define CFG_I(p, field, def) (((p)->field == DOI_DEFAULT) ? (def) : (p)->field)

/* Resolve a string field from a NotifMsg pointer P. */
#define CFG_S(p, field, def) ((p)->field[0] ? (p)->field : (def))

/* ── X11 colour helpers ───────────────────────────────────────────────── */

static unsigned long xcolor(Display *dpy, int screen, const char *name) {
    XColor exact, alloc;
    if (!XAllocNamedColor(dpy, DefaultColormap(dpy, screen),
                          name, &alloc, &exact))
        return BlackPixel(dpy, screen);
    return alloc.pixel;
}

/* ── text measurement ────────────────────────────────────────────────── */

static int text_width(Display *dpy, XftFont *font, const char *s, int len) {
    XGlyphInfo ext;
    if (!s || len <= 0) return 0;
    XftTextExtentsUtf8(dpy, font, (const FcChar8 *)s, len, &ext);
    return ext.xOff;
}

/* ── rounded-corner shape mask ───────────────────────────────────────── */

static void apply_rounded_corners(Display *dpy, Window win,
                                  int w, int h, int r) {
    Pixmap mask;
    GC gc;
    int d2;
    if (r <= 0) return;
    mask = XCreatePixmap(dpy, win, w, h, 1);
    gc   = XCreateGC(dpy, mask, 0, NULL);
    d2   = r * 2;

    XSetForeground(dpy, gc, 0);
    XFillRectangle(dpy, mask, gc, 0, 0, w, h);
    XSetForeground(dpy, gc, 1);

    XFillArc(dpy, mask, gc, 0,    0,    d2, d2, 90*64,  90*64);
    XFillArc(dpy, mask, gc, w-d2, 0,    d2, d2, 0,      90*64);
    XFillArc(dpy, mask, gc, 0,    h-d2, d2, d2, 180*64, 90*64);
    XFillArc(dpy, mask, gc, w-d2, h-d2, d2, d2, 270*64, 90*64);
    XFillRectangle(dpy, mask, gc, r, 0,   w-d2, h);
    XFillRectangle(dpy, mask, gc, 0, r,   w,    h-d2);

    XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, mask);
}

/* ── window placement ────────────────────────────────────────────────── */

static int window_x(Display *dpy, int screen,
                    int win_w, int pos_x, int offset_x) {
    int sw = DisplayWidth(dpy, screen);
    if (pos_x == LEFT)  return offset_x;
    if (pos_x == RIGHT) return sw - win_w - offset_x;
    return (sw - win_w) / 2;
}

static int window_y(Display *dpy, int screen,
                    int win_h, int pos_y, int offset_y, int stack_index) {
    int sh   = DisplayHeight(dpy, screen);
    int step = NOTIF_HEIGHT + STACK_GAP;
    if (pos_y == TOP)    return offset_y + stack_index * step;
    if (pos_y == BOTTOM) return sh - win_h - offset_y - stack_index * step;
    return (sh - win_h) / 2 + stack_index * step;
}

/* ── layout measurement ──────────────────────────────────────────────── */

static void measure(Display *dpy, XftFont *font,
                    const NotifMsg *m, int *out_w, int *out_h) {
    int border    = CFG_I(m, border, BORDER);
    int min_w     = CFG_I(m, min_width, MIN_WIDTH);
    int bar_w     = CFG_I(m, bar_width, BAR_WIDTH);
    int show_icon = CFG_I(m, show_icon, SHOW_ICON);
    int show_body = CFG_I(m, show_body, SHOW_BODY);
    int layout    = CFG_I(m, layout, LAYOUT);
    int inset     = border + MARGIN;
    int screen    = DefaultScreen(dpy);
    int max_w     = DisplayWidth(dpy, screen) * MAX_WIDTH_PCT / 100;
    int win_w, win_h;

    if (layout == 1) {
        win_w = min_w;
    } else {
        int content_w = 0;
        int row_w     = 0;

        if (show_icon && m->icon[0])
            row_w += text_width(dpy, font, m->icon,
                                (int)strlen(m->icon)) + PADDING;
        if (m->summary[0])
            row_w += text_width(dpy, font, m->summary,
                                (int)strlen(m->summary));
        content_w = row_w;

        if (show_body && m->body[0]) {
            const char *line = m->body;
            while (line) {
                const char *nl = strchr(line, '\n');
                int len = nl ? (int)(nl - line) : (int)strlen(line);
                int w   = text_width(dpy, font, line, len);
                if (w > content_w) content_w = w;
                line = nl ? nl + 1 : NULL;
            }
        }

        if (m->show_bar && bar_w > content_w)
            content_w = bar_w;

        win_w = inset * 2 + PADDING * 2 + content_w;
        if (win_w < min_w) win_w = min_w;
        if (win_w > max_w) win_w = max_w;
    }

    win_h = CFG_I(m, min_height, NOTIF_HEIGHT);
    if (win_h < NOTIF_HEIGHT) win_h = NOTIF_HEIGHT;

    *out_w = win_w;
    *out_h = win_h;
}

/* ── paint ───────────────────────────────────────────────────────────── */

static void paint(Display *dpy, int screen, Window win, GC gc,
                  XftFont *font,
                  int win_w, int win_h,
                  const NotifMsg *m,
                  int pos_x, int pos_y, int stack_index) {
    /* resolve every configurable field in one place */
    int border    = CFG_I(m, border, BORDER);
    int radius    = CFG_I(m, border_radius, BORDER_RADIUS);
    int bar_w     = CFG_I(m, bar_width, BAR_WIDTH);
    int bar_h     = CFG_I(m, bar_height, BAR_HEIGHT);
    int offset_x  = CFG_I(m, offset_x, OFFSET_X);
    int offset_y  = CFG_I(m, offset_y, OFFSET_Y);
    int show_icon = CFG_I(m, show_icon, SHOW_ICON);
    int show_body = CFG_I(m, show_body, SHOW_BODY);

    const char *bg  = CFG_S(m, bg, BG);
    const char *fg  = CFG_S(m, fg, FG);
    const char *brc = CFG_S(m, border_color, BORDER_COLOR);
    const char *bfg = CFG_S(m, bar_fg, BAR_FG);
    const char *bbg = CFG_S(m, bar_bg, BAR_BG);

    /* move window */
    {
        int wx = window_x(dpy, screen, win_w, pos_x, offset_x);
        int wy = window_y(dpy, screen, win_h, pos_y, offset_y, stack_index);
        XMoveWindow(dpy, win, wx, wy);
    }

    /* shape mask for rounded corners */
    apply_rounded_corners(dpy, win, win_w, win_h, radius);

    /* background fill */
    XSetForeground(dpy, gc, xcolor(dpy, screen, bg));
    XFillRectangle(dpy, win, gc, 0, 0, win_w, win_h);

    /* border */
    if (border > 0) {
        int b;
        XSetForeground(dpy, gc, xcolor(dpy, screen, brc));
        for (b = 0; b < border; b++)
            XDrawRectangle(dpy, win, gc,
                           b, b, win_w - 1 - b*2, win_h - 1 - b*2);
    }

    /* ── text + bar ───────────────────────────────────────────────── */
    {
        XftDraw  *xd;
        XftColor  col;
        int line_h     = font->ascent + font->descent;
        int pad        = border + PADDING;   /* left margin from window edge */
        int body_lines = 0;
        int bar_lines  = 0;
        int total_h, start_y, cur_y;
        const char *line;
        int text_x;

        if (show_body && m->body[0]) {
            const char *p = m->body;
            body_lines = 1;
            while ((p = strchr(p, '\n'))) { body_lines++; p++; }
        }
        if (m->show_bar) bar_lines = 1;

        /* total pixel height of all rendered rows */
        total_h = line_h
                + body_lines * (line_h + PADDING)
                + bar_lines  * (bar_h  + PADDING);

        /* vertically centre total_h inside the window */
        start_y = (win_h - total_h) / 2 + font->ascent;
        if (start_y < font->ascent + border)
            start_y = font->ascent + border;

        xd = XftDrawCreate(dpy, win,
                DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));
        XftColorAllocName(dpy, DefaultVisual(dpy, screen),
                DefaultColormap(dpy, screen), fg, &col);

        cur_y  = start_y;
        text_x = pad;

        /* icon glyph (same baseline as summary) */
        if (show_icon && m->icon[0]) {
            int iw = text_width(dpy, font, m->icon, (int)strlen(m->icon));
            XftDrawStringUtf8(xd, &col, font, text_x, cur_y,
                    (const FcChar8 *)m->icon, (int)strlen(m->icon));
            text_x += iw + PADDING;
        }

        /* summary */
        if (m->summary[0])
            XftDrawStringUtf8(xd, &col, font, text_x, cur_y,
                    (const FcChar8 *)m->summary, (int)strlen(m->summary));

        /* body lines */
        if (show_body && m->body[0]) {
            line = m->body;
            while (line && *line) {
                const char *nl = strchr(line, '\n');
                int len = nl ? (int)(nl - line) : (int)strlen(line);
                cur_y += line_h + PADDING;
                if (len > 0)
                    XftDrawStringUtf8(xd, &col, font, pad, cur_y,
                            (const FcChar8 *)line, len);
                line = nl ? nl + 1 : NULL;
            }
        }

        /* progress bar */
        if (m->show_bar) {
            int bar_x  = pad;
            int bar_y  = cur_y + font->descent + PADDING;
            int filled = (bar_w * m->bar_value) / 100;
            if (filled < 0) filled = 0;
            if (filled > bar_w) filled = bar_w;

            XSetForeground(dpy, gc, xcolor(dpy, screen, bbg));
            XFillRectangle(dpy, win, gc, bar_x, bar_y, bar_w, bar_h);

            if (filled > 0) {
                XSetForeground(dpy, gc, xcolor(dpy, screen, bfg));
                XFillRectangle(dpy, win, gc, bar_x, bar_y, filled, bar_h);
            }
        }

        XftColorFree(dpy, DefaultVisual(dpy, screen),
                DefaultColormap(dpy, screen), &col);
        XftDrawDestroy(xd);
    }

    XFlush(dpy);
}

/* ── render_loop ─────────────────────────────────────────────────────── */

void render_loop(int read_fd, Notif *initial) {
    Display *dpy;
    int      screen;
    Window   win;
    GC       gc;
    XSetWindowAttributes attrs;
    XGCValues            gcv;
    XftFont *font;
    XEvent   ev;
    int      x11_fd;
    int      win_w = 0, win_h = 0;
    int      visible = 0;
    NotifMsg cur;
    int      pos_x       = initial->pos_x;
    int      pos_y       = initial->pos_y;
    int      stack_index = initial->stack_index;
    int      timeout_ms  = -1;

    struct pollfd fds[2];

    setlocale(LC_ALL, "");

    dpy = XOpenDisplay(NULL);
    if (!dpy) { log_write("render: cannot open display"); return; }
    screen = DefaultScreen(dpy);

    font = XftFontOpenName(dpy, screen, FONT);
    if (!font) font = XftFontOpenName(dpy, screen, "monospace:size=10");
    if (!font) {
        log_write("render: could not load font");
        XCloseDisplay(dpy);
        return;
    }

    attrs.background_pixel  = xcolor(dpy, screen, BG);
    attrs.override_redirect = True;
    win = XCreateWindow(dpy, DefaultRootWindow(dpy),
        0, 0, 1, 1, 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWBackPixel | CWOverrideRedirect, &attrs);
    XSelectInput(dpy, win, ButtonPressMask | ExposureMask);
    gcv.foreground = xcolor(dpy, screen, FG);
    gc = XCreateGC(dpy, win, GCForeground, &gcv);

    x11_fd = ConnectionNumber(dpy);
    fcntl(read_fd, F_SETFL, O_NONBLOCK);
    XFlush(dpy);

    fds[0].fd     = read_fd;
    fds[0].events = POLLIN;
    fds[1].fd     = x11_fd;
    fds[1].events = POLLIN;

    memset(&cur, 0, sizeof(cur));

    for (;;) {
        NotifMsg msg;
        ssize_t  n;

        /* non-blocking drain: handle all queued messages before blocking */
        n = read(read_fd, &msg, sizeof(msg));
        if (n == (ssize_t)sizeof(msg)) {
            switch (msg.msg_type) {

            case MSG_CLOSE:
                goto done;

            case MSG_REPOSITION:
                stack_index = msg.new_stack_index;
                if (visible)
                    paint(dpy, screen, win, gc, font,
                          win_w, win_h, &cur,
                          pos_x, pos_y, stack_index);
                break;

            case MSG_UPDATE:
            default:
                cur = msg;
                /* pos_x/pos_y can change on update (module reposition) */
                if (msg.pos_x != DOI_DEFAULT) pos_x = msg.pos_x;
                if (msg.pos_y != DOI_DEFAULT) pos_y = msg.pos_y;

                measure(dpy, font, &cur, &win_w, &win_h);
                XResizeWindow(dpy, win, win_w, win_h);
                if (!visible) { XMapRaised(dpy, win); visible = 1; }
                paint(dpy, screen, win, gc, font,
                      win_w, win_h, &cur,
                      pos_x, pos_y, stack_index);
                {
                    int t = CFG_I(&cur, timeout, TIMEOUT);
                    timeout_ms = (t > 0) ? t * 1000 : -1;
                }
                break;
            }
            continue;
        }

        {
            int r = poll(fds, 2, timeout_ms);
            if (r == 0) goto done;
            if (r < 0)  continue;
        }

        if (fds[0].revents & POLLIN) continue;

        while (XPending(dpy)) {
            XNextEvent(dpy, &ev);
            if (ev.type == Expose && ev.xexpose.count == 0 && visible)
                paint(dpy, screen, win, gc, font,
                      win_w, win_h, &cur,
                      pos_x, pos_y, stack_index);
            if (ev.type == ButtonPress)
                goto done;
        }
    }

done:
    if (visible) XUnmapWindow(dpy, win);
    XDestroyWindow(dpy, win);
    XFreeGC(dpy, gc);
    XftFontClose(dpy, font);
    XCloseDisplay(dpy);
}
