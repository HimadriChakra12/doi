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

static unsigned long xcolor(Display *dpy, int screen, const char *name) {
        XColor exact, alloc;
        XAllocNamedColor(dpy, DefaultColormap(dpy, screen), name, &alloc, &exact);
        return alloc.pixel;
}

static int text_width(Display *dpy, XftFont *font, const char *s, int len) {
        XGlyphInfo ext;
        if (!s || len <= 0) return 0;
        XftTextExtentsUtf8(dpy, font, (const FcChar8 *)s, len, &ext);
        return ext.xOff;
}

static void apply_rounded_corners(Display *dpy, Window win, int w, int h, int r) {
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

static int window_x(Display *dpy, int screen, int win_w, int pos_x, int offset_x) {
        int screen_w = DisplayWidth(dpy, screen);
        if (pos_x == LEFT)  return offset_x;
        if (pos_x == RIGHT) return screen_w - win_w - offset_x;
        return (screen_w - win_w) / 2;
}

static int window_y(Display *dpy, int screen, int win_h, int pos_y, int offset_y, int stack_index) {
        int screen_h = DisplayHeight(dpy, screen);
        int step     = NOTIF_HEIGHT + STACK_GAP;
        if (pos_y == TOP)    return offset_y + stack_index * step;
        if (pos_y == BOTTOM) return screen_h - win_h - offset_y - stack_index * step;
        return (screen_h - win_h) / 2 + stack_index * step;
}

static void measure(Display *dpy, XftFont *font,
                    const NotifMsg *m, int *out_w, int *out_h) {
        int border    = m->border    >= 0 ? m->border    : BORDER;
        int min_w     = m->min_width  > 0 ? m->min_width  : MIN_WIDTH;
        int bar_w     = m->bar_width  > 0 ? m->bar_width  : BAR_WIDTH;
        int show_icon = m->show_icon >= 0 ? m->show_icon : SHOW_ICON;
        int show_body = m->show_body >= 0 ? m->show_body : SHOW_BODY;
        int inset     = border + MARGIN;  /* from edge to content area start */
        int screen    = DefaultScreen(dpy);
        int max_w     = DisplayWidth(dpy, screen) * MAX_WIDTH_PCT / 100;
        int win_w, win_h;

        if (m->layout == 1) {
                win_w = min_w;
        } else {
                int content_w = 0;
                int row_w     = 0;

                if (show_icon && m->icon[0])
                        row_w += text_width(dpy, font, m->icon, strlen(m->icon)) + PADDING;
                if (m->summary[0])
                        row_w += text_width(dpy, font, m->summary, strlen(m->summary));
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

        win_h = NOTIF_HEIGHT;
        if (m->min_height > 0 && win_h < m->min_height)
                win_h = m->min_height;

        *out_w = win_w;
        *out_h = win_h;
}

static void paint(Display *dpy, int screen, Window win, GC gc,
                  XftFont *font, int line_h,
                  int win_w, int win_h,
                  const NotifMsg *m,
                  int pos_x, int pos_y, int stack_index) {
        int border   = m->border        >= 0 ? m->border        : BORDER;
        int radius   = m->border_radius  > 0 ? m->border_radius  : BORDER_RADIUS;
        int bar_w    = m->bar_width      > 0 ? m->bar_width      : BAR_WIDTH;
        int bar_h    = m->bar_height     > 0 ? m->bar_height     : BAR_HEIGHT;
        int offset_x = m->offset_x      >= 0 ? m->offset_x      : OFFSET_X;
        int offset_y = m->offset_y      >= 0 ? m->offset_y      : OFFSET_Y;
        int show_icon = m->show_icon    >= 0 ? m->show_icon      : SHOW_ICON;
        int show_body = m->show_body    >= 0 ? m->show_body      : SHOW_BODY;

        const char *bg  = m->bg[0]           ? m->bg           : BG;
        const char *fg  = m->fg[0]           ? m->fg           : FG;
        const char *brc = m->border_color[0] ? m->border_color : BORDER_COLOR;
        const char *bfg = m->bar_fg[0]       ? m->bar_fg       : BAR_FG;
        const char *bbg = m->bar_bg[0]       ? m->bar_bg       : BAR_BG;

        int wx = window_x(dpy, screen, win_w, pos_x, offset_x);
        int wy = window_y(dpy, screen, win_h, pos_y, offset_y, stack_index);
        XMoveWindow(dpy, win, wx, wy);

        apply_rounded_corners(dpy, win, win_w, win_h, radius);

        XSetForeground(dpy, gc, xcolor(dpy, screen, bg));
        XFillRectangle(dpy, win, gc, 0, 0, win_w, win_h);

        if (border > 0) {
                int i;
                XSetForeground(dpy, gc, xcolor(dpy, screen, brc));
                for (i = 0; i < border; i++)
                        XDrawRectangle(dpy, win, gc, i, i, win_w-1-i*2, win_h-1-i*2);
        }

        /* text content */
        {
                XftDraw  *xd;
                XftColor  col;
                int inset    = border + MARGIN;
                int text_x   = inset + PADDING;
                int body_lines = 0;
                int bar_lines  = 0;
                int total_h, start_y, cur_y;
                const char *line;

                if (show_body && m->body[0]) {
                        const char *p = m->body;
                        while (p) { body_lines++; p = strchr(p, '\n'); if (p) p++; }
                }
                if (m->show_bar) bar_lines = 1;

                /* total height of all rendered content */
                total_h = line_h                                  /* summary row */
                        + (body_lines * (line_h + PADDING))
                        + (bar_lines  * (bar_h  + PADDING * 2));

                /* vertically centre within inner box */
                start_y = inset + (win_h - inset * 2 - total_h) / 2 + line_h;
                if (start_y < inset + line_h) start_y = inset + line_h;

                xd = XftDrawCreate(dpy, win,
                        DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));
                XftColorAllocName(dpy, DefaultVisual(dpy, screen),
                        DefaultColormap(dpy, screen), fg, &col);

                cur_y = start_y;

                if (show_icon && m->icon[0]) {
                        int iw = text_width(dpy, font, m->icon, strlen(m->icon));
                        XftDrawStringUtf8(xd, &col, font, text_x, cur_y,
                                (const FcChar8 *)m->icon, strlen(m->icon));
                        text_x += iw + PADDING;
                }

                if (m->summary[0])
                        XftDrawStringUtf8(xd, &col, font, text_x, cur_y,
                                (const FcChar8 *)m->summary, strlen(m->summary));

                if (show_body && m->body[0]) {
                        line = m->body;
                        while (line && *line) {
                                const char *nl = strchr(line, '\n');
                                int len = nl ? (int)(nl - line) : (int)strlen(line);
                                cur_y += line_h + PADDING;
                                if (len > 0)
                                        XftDrawStringUtf8(xd, &col, font,
                                                inset + PADDING, cur_y,
                                                (const FcChar8 *)line, len);
                                line = nl ? nl + 1 : NULL;
                        }
                }

                if (m->show_bar) {
                        int bar_x  = inset + PADDING;
                        int bar_y  = cur_y + PADDING * 2;
                        int filled = bar_w * m->bar_value / 100;

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

void render_loop(int read_fd, Notif *initial) {
        Display *dpy;
        int screen;
        Window win;
        GC gc;
        XSetWindowAttributes attrs;
        XGCValues gcv;
        XftFont *font;
        XEvent ev;
        int x11_fd;
        int win_w = 0, win_h = 0;
        int visible = 0;
        NotifMsg cur;
        int pos_x      = initial->pos_x;
        int pos_y      = initial->pos_y;
        int stack_index = initial->stack_index;
        struct timeval tv_buf, *tv = NULL;

        setlocale(LC_ALL, "");

        dpy = XOpenDisplay(NULL);
        if (!dpy) { log_write("render: cannot open display"); return; }
        screen = DefaultScreen(dpy);

        font = XftFontOpenName(dpy, screen, FONT);
        if (!font) font = XftFontOpenName(dpy, screen, "monospace:size=10");
        if (!font) { log_write("render: no font"); XCloseDisplay(dpy); return; }

        attrs.background_pixel  = xcolor(dpy, screen, BG);
        attrs.override_redirect = True;
        win = XCreateWindow(dpy, DefaultRootWindow(dpy),
                0, 0, 200, 40, 0,
                CopyFromParent, InputOutput, CopyFromParent,
                CWBackPixel | CWOverrideRedirect, &attrs);
        XSelectInput(dpy, win, ButtonPressMask | ExposureMask);
        gcv.foreground = xcolor(dpy, screen, FG);
        gc = XCreateGC(dpy, win, GCForeground, &gcv);

        x11_fd = ConnectionNumber(dpy);
        fcntl(read_fd, F_SETFL, O_NONBLOCK);
        XFlush(dpy);

        for (;;) {
                fd_set fds;
                int maxfd;
                NotifMsg msg;
                ssize_t n;

                FD_ZERO(&fds);
                FD_SET(x11_fd, &fds);
                FD_SET(read_fd, &fds);
                maxfd = x11_fd > read_fd ? x11_fd : read_fd;

                n = read(read_fd, &msg, sizeof(msg));
                if (n == (ssize_t)sizeof(msg)) {
                        if (msg.msg_type == MSG_REPOSITION) {
                                stack_index = msg.new_stack_index;
                                if (visible)
                                        paint(dpy, screen, win, gc, font,
                                              font->ascent + font->descent,
                                              win_w, win_h, &cur,
                                              pos_x, pos_y, stack_index);
                        } else {
                                int timeout;
                                cur = msg;
                                measure(dpy, font, &msg, &win_w, &win_h);
                                XResizeWindow(dpy, win, win_w, win_h);
                                if (!visible) {
                                        XMapRaised(dpy, win);
                                        visible = 1;
                                }
                                paint(dpy, screen, win, gc, font,
                                      font->ascent + font->descent,
                                      win_w, win_h, &cur,
                                      pos_x, pos_y, stack_index);
                                timeout = msg.timeout > 0 ? msg.timeout : TIMEOUT;
                                if (timeout > 0) {
                                        tv_buf.tv_sec  = timeout;
                                        tv_buf.tv_usec = 0;
                                        tv = &tv_buf;
                                } else {
                                        tv = NULL;
                                }
                        }
                        continue;
                }

                if (!XPending(dpy)) {
                        int r = select(maxfd + 1, &fds, NULL, NULL, tv);
                        if (r == 0) goto done;
                        if (r < 0)  continue;
                        if (FD_ISSET(read_fd, &fds)) continue;
                }

                while (XPending(dpy)) {
                        XNextEvent(dpy, &ev);
                        if (ev.type == Expose && ev.xexpose.count == 0 && visible)
                                paint(dpy, screen, win, gc, font,
                                      font->ascent + font->descent,
                                      win_w, win_h, &cur,
                                      pos_x, pos_y, stack_index);
                        if (ev.type == ButtonPress)
                                goto done;
                }
        }

done:
        if (visible) XUnmapWindow(dpy, win);
        XFreeGC(dpy, gc);
        XftFontClose(dpy, font);
        XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);
}
