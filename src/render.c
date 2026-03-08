#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <X11/Xft/Xft.h>
#include "notif.h"
#include "log.h"
#include "../config.h"

/* ── helpers ──────────────────────────────────────────────────────────── */

static unsigned long xcolor(Display* d, int s, const char* n) {
        XColor c, dummy;
        XAllocNamedColor(d, DefaultColormap(d,s), n, &c, &dummy);
        return c.pixel;
}

static int win_x(Display* d, int s, int w, int px, int ox) {
        int sw = DisplayWidth(d,s);
        if (px == DOI_LEFT)  return ox;
        if (px == DOI_RIGHT) return sw - w - ox;
        return (sw - w) / 2;
}

static int win_y(Display* d, int s, int h, int py, int oy, int idx) {
        int sh = DisplayHeight(d,s);
        int step = DOI_NOTIF_HEIGHT + DOI_STACK_GAP;
        if (py == DOI_TOP)    return oy + idx * step;
        if (py == DOI_BOTTOM) return sh - h - oy - idx * step;
        return (sh - h) / 2 + idx * step;
}

static int utf8w(Display* d, XftFont* f, const char* s, int len) {
        XGlyphInfo e;
        if (!s || len <= 0) return 0;
        XftTextExtentsUtf8(d, f, (const FcChar8*)s, len, &e);
        return e.xOff;
}

/* ── rounded-rect clip mask ───────────────────────────────────────────── */

static void apply_border_radius(Display* dpy, Window win, int w, int h, int r) {
        if (r <= 0) return;
        Pixmap mask = XCreatePixmap(dpy, win, w, h, 1);
        GC mgc = XCreateGC(dpy, mask, 0, NULL);

        /* clear to transparent */
        XSetForeground(dpy, mgc, 0);
        XFillRectangle(dpy, mask, mgc, 0, 0, w, h);

        XSetForeground(dpy, mgc, 1);
        /* four corner arcs */
        int d2 = r * 2;
        XFillArc(dpy, mask, mgc, 0,     0,     d2, d2, 90*64,  90*64);
        XFillArc(dpy, mask, mgc, w-d2,  0,     d2, d2, 0,      90*64);
        XFillArc(dpy, mask, mgc, 0,     h-d2,  d2, d2, 180*64, 90*64);
        XFillArc(dpy, mask, mgc, w-d2,  h-d2,  d2, d2, 270*64, 90*64);
        /* fill inner rects */
        XFillRectangle(dpy, mask, mgc, r, 0,   w-d2, h);
        XFillRectangle(dpy, mask, mgc, 0, r,   w,    h-d2);

        XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
        XFreeGC(dpy, mgc);
        XFreePixmap(dpy, mask);
}

/* ── measure ──────────────────────────────────────────────────────────── */

static void measure(Display* dpy, XftFont* font, int text_h,
                const NotifUpdate* u, int* out_w, int* out_h) {
        int border    = (u->border >= 0)    ? u->border    : DOI_BORDER;
        int min_width = (u->min_width > 0)  ? u->min_width : DOI_MIN_WIDTH;
        int bw        = (u->bar_width  > 0) ? u->bar_width : DOI_BAR_WIDTH;
        int bh        = (u->bar_height > 0) ? u->bar_height: DOI_BAR_HEIGHT;
        int show_icon = (u->show_icon >= 0) ? u->show_icon : DOI_SHOW_ICON;
        int show_body = (u->show_body >= 0) ? u->show_body : DOI_SHOW_BODY;
        int layout    = u->layout;  /* 0=brick, 1=block */
        int inner     = border + DOI_MARGIN + DOI_PADDING;
        int screen    = DefaultScreen(dpy);

        int win_w;
        if (layout == 1) {
                /* block: fixed width = min_width */
                win_w = min_width;
        } else {
                /* brick: auto width from content */
                int sum_w = 0, body_w = 0;
                if (show_icon && u->icon[0])
                        sum_w += utf8w(dpy, font, u->icon, strlen(u->icon)) + DOI_PADDING;
                if (u->summary[0])
                        sum_w += utf8w(dpy, font, u->summary, strlen(u->summary));
                if (show_body && u->body[0]) {
                        const char* line = u->body; const char* nl;
                        while (line) {
                                nl = strchr(line, '\n');
                                int len = nl ? (int)(nl-line) : (int)strlen(line);
                                int w = utf8w(dpy, font, line, len);
                                if (w > body_w) body_w = w;
                                line = nl ? nl+1 : NULL;
                        }
                }
                int content_w = sum_w > body_w ? sum_w : body_w;
                if (u->show_bar && bw > content_w) content_w = bw;
                win_w = inner * 2 + content_w;
                if (win_w < min_width) win_w = min_width;
                int max_w = DisplayWidth(dpy, screen) * DOI_MAX_WIDTH_PCT / 100;
                if (win_w > max_w) win_w = max_w;
        }

        /* height: always fixed to DOI_NOTIF_HEIGHT */
        int win_h = DOI_NOTIF_HEIGHT;
        if (u->min_height > 0 && win_h < u->min_height)
                win_h = u->min_height;
        (void)bh; (void)text_h;
        *out_w = win_w;
        *out_h = win_h;
}

/* ── paint ────────────────────────────────────────────────────────────── */

static void paint(Display* dpy, int screen, Window win, GC gc,
                XftFont* font, int text_h,
                int win_w, int win_h,
                const NotifUpdate* u,
                int pos_x, int pos_y, int stack_idx) {
        int border   = (u->border >= 0)      ? u->border      : DOI_BORDER;
        int radius   = (u->border_radius > 0) ? u->border_radius : DOI_BORDER_RADIUS;
        const char* bg  = u->bg[0]           ? u->bg           : DOI_BG;
        const char* fg  = u->fg[0]           ? u->fg           : DOI_FG;
        const char* brc = u->border_color[0] ? u->border_color : DOI_BORDER_COLOR;
        const char* bfg = u->bar_fg[0]       ? u->bar_fg       : DOI_BAR_FG;
        const char* bbg = u->bar_bg[0]       ? u->bar_bg       : DOI_BAR_BG;
        int bw  = (u->bar_width  > 0) ? u->bar_width  : DOI_BAR_WIDTH;
        int bh  = (u->bar_height > 0) ? u->bar_height : DOI_BAR_HEIGHT;
        int ox  = (u->offset_x >= 0)  ? u->offset_x  : DOI_OFFSET_X;
        int oy  = (u->offset_y >= 0)  ? u->offset_y  : DOI_OFFSET_Y;
        int show_icon = (u->show_icon >= 0) ? u->show_icon : DOI_SHOW_ICON;
        int show_body = (u->show_body >= 0) ? u->show_body : DOI_SHOW_BODY;
        int i;

        /* move */
        int wx = win_x(dpy, screen, win_w, pos_x, ox);
        int wy = win_y(dpy, screen, win_h, pos_y, oy, stack_idx);
        XMoveWindow(dpy, win, wx, wy);

        /* apply rounded clip */
        apply_border_radius(dpy, win, win_w, win_h, radius);

        /* clear bg */
        XSetForeground(dpy, gc, xcolor(dpy, screen, bg));
        XFillRectangle(dpy, win, gc, 0, 0, win_w, win_h);

        /* border */
        if (border > 0) {
                XSetForeground(dpy, gc, xcolor(dpy, screen, brc));
                for (i = 0; i < border; i++)
                        XDrawRectangle(dpy, win, gc, i, i,
                                win_w-1-i*2, win_h-1-i*2);
        }

        /* text */
        XftDraw* xd = XftDrawCreate(dpy, win,
                DefaultVisual(dpy,screen), DefaultColormap(dpy,screen));
        XftColor col;
        XftColorAllocName(dpy, DefaultVisual(dpy,screen),
                DefaultColormap(dpy,screen), fg, &col);

        int border_m = border + DOI_MARGIN;
        int tx = border_m + DOI_PADDING;

        /* count body lines for vertical centering */
        int body_lines = 0;
        if (show_body && u->body[0]) {
                const char* p = u->body;
                while (p) {
                        body_lines++;
                        p = strchr(p, '\n');
                        if (p) p++;
                }
        }
        int content_h = text_h
                + (body_lines ? body_lines * (text_h + DOI_PADDING) : 0)
                + (u->show_bar ? bh + DOI_PADDING * 2 : 0);

        int cur_y = border_m + (win_h - border_m * 2 - content_h) / 2 + text_h;
        if (cur_y < border_m + text_h) cur_y = border_m + text_h;

        if (show_icon && u->icon[0]) {
                int iw = utf8w(dpy, font, u->icon, strlen(u->icon));
                XftDrawStringUtf8(xd, &col, font, tx, cur_y,
                        (const FcChar8*)u->icon, strlen(u->icon));
                tx += iw + DOI_PADDING;
        }
        if (u->summary[0])
                XftDrawStringUtf8(xd, &col, font, tx, cur_y,
                        (const FcChar8*)u->summary, strlen(u->summary));

        cur_y += DOI_PADDING;

        if (show_body && u->body[0]) {
                const char* line = u->body; const char* nl;
                while (line && *line) {
                        nl = strchr(line, '\n');
                        int len = nl ? (int)(nl-line) : (int)strlen(line);
                        cur_y += text_h;
                        if (len > 0)
                                XftDrawStringUtf8(xd, &col, font,
                                        border_m + DOI_PADDING, cur_y,
                                        (const FcChar8*)line, len);
                        cur_y += DOI_PADDING;
                        line = nl ? nl+1 : NULL;
                }
        }

        if (u->show_bar) {
                int filled = bw * u->bar_value / 100;
                cur_y += DOI_PADDING;
                XSetForeground(dpy, gc, xcolor(dpy, screen, bbg));
                XFillRectangle(dpy, win, gc, border_m + DOI_PADDING, cur_y, bw, bh);
                if (filled > 0) {
                        XSetForeground(dpy, gc, xcolor(dpy, screen, bfg));
                        XFillRectangle(dpy, win, gc,
                                border_m + DOI_PADDING, cur_y, filled, bh);
                }
        }

        XftColorFree(dpy, DefaultVisual(dpy,screen),
                DefaultColormap(dpy,screen), &col);
        XftDrawDestroy(xd);
        XFlush(dpy);
}

/* ── render loop ──────────────────────────────────────────────────────── */

void render_loop(int read_fd, Notif* initial) {
        Display* dpy;
        int screen;
        Window win;
        GC gc;
        XSetWindowAttributes att;
        XGCValues gcv;
        XftFont* font;
        XEvent ev;
        int x11_fd;
        int win_w = 0, win_h = 0;
        int visible = 0;
        NotifUpdate cur;
        int pos_x     = initial->pos_x;
        int pos_y     = initial->pos_y;
        int stack_idx = initial->stack_index;
        struct timeval tv_store, *tv = NULL;

        setlocale(LC_ALL, "");

        dpy = XOpenDisplay(NULL);
        if (!dpy) { w_log("render: cannot open display"); return; }
        screen = DefaultScreen(dpy);

        font = XftFontOpenName(dpy, screen, DOI_FONT);
        if (!font) font = XftFontOpenName(dpy, screen, "monospace:size=10");
        if (!font) { w_log("render: no font"); XCloseDisplay(dpy); return; }

        int text_h = font->ascent + font->descent;

        att.background_pixel  = xcolor(dpy, screen, DOI_BG);
        att.override_redirect = True;
        win = XCreateWindow(dpy, DefaultRootWindow(dpy),
                0, 0, 200, 40, 0,
                CopyFromParent, InputOutput, CopyFromParent,
                CWBackPixel|CWOverrideRedirect, &att);
        XSelectInput(dpy, win,
                ButtonPressMask|StructureNotifyMask|ExposureMask);
        gcv.foreground = xcolor(dpy, screen, DOI_FG);
        gc = XCreateGC(dpy, win, GCForeground, &gcv);
        x11_fd = ConnectionNumber(dpy);
        fcntl(read_fd, F_SETFL, O_NONBLOCK);
        XFlush(dpy);

        while (1) {
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(x11_fd, &fds);
                FD_SET(read_fd, &fds);
                int maxfd = x11_fd > read_fd ? x11_fd : read_fd;

                /* drain pipe */
                {
                        NotifUpdate u;
                        ssize_t n = read(read_fd, &u, sizeof(u));
                        if (n == (ssize_t)sizeof(u)) {
                                if (u.msg_type == DOI_MSG_REPOSITION) {
                                        /* just move window, keep current content */
                                        stack_idx = u.new_stack_idx;
                                        if (visible)
                                                paint(dpy, screen, win, gc, font,
                                                        text_h, win_w, win_h, &cur,
                                                        pos_x, pos_y, stack_idx);
                                } else {
                                        /* full update */
                                        cur = u;
                                        measure(dpy, font, text_h, &u, &win_w, &win_h);
                                        XResizeWindow(dpy, win, win_w, win_h);
                                        if (!visible) {
                                                XMapRaised(dpy, win);
                                                visible = 1;
                                        }
                                        int timeout = (u.timeout > 0) ? u.timeout : DOI_TIMEOUT;
                                        if (timeout > 0) {
                                                tv_store.tv_sec  = timeout;
                                                tv_store.tv_usec = 0;
                                                tv = &tv_store;
                                        } else {
                                                tv = NULL;
                                        }
                                        paint(dpy, screen, win, gc, font, text_h,
                                                win_w, win_h, &cur,
                                                pos_x, pos_y, stack_idx);
                                }
                                continue;
                        }
                }

                if (!XPending(dpy)) {
                        int r = select(maxfd+1, &fds, NULL, NULL, tv);
                        if (r == 0) {
                                w_log("render: timeout pid=%d sidx=%d",
                                        (int)getpid(), stack_idx);
                                goto done;
                        }
                        if (r < 0) continue;
                        if (FD_ISSET(read_fd, &fds)) continue;
                }

                while (XPending(dpy)) {
                        XNextEvent(dpy, &ev);
                        switch (ev.type) {
                        case Expose:
                                if (visible && ev.xexpose.count == 0)
                                        paint(dpy, screen, win, gc, font,
                                                text_h, win_w, win_h, &cur,
                                                pos_x, pos_y, stack_idx);
                                break;
                        case ButtonPress:
                                goto done;
                        }
                }
        }
done:
        if (visible) XUnmapWindow(dpy, win);
        XFreeGC(dpy, gc);
        XftFontClose(dpy, font);
        XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);
}
