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
#include <X11/Xft/Xft.h>
#include "notif.h"
#include "log.h"
#include "../config.h"

/* ── colour/geometry helpers ──────────────────────────────────────────── */

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
        /* Use fixed step so stacking is stable regardless of window height.
         * DOI_STACK_HEIGHT is the estimated notification height from config. */
        int step = DOI_STACK_HEIGHT + DOI_STACK_GAP;
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

/* ── paint ────────────────────────────────────────────────────────────── */

static void paint(Display* dpy, int screen, Window win, GC gc,
                XftFont* font, int text_h,
                int win_w, int win_h,
                const NotifUpdate* u,
                int pos_x, int pos_y, int stack_idx) {
        int border   = (u->border >= 0) ? u->border : DOI_BORDER;
        const char* bg  = u->bg[0]           ? u->bg           : DOI_BG;
        const char* fg  = u->fg[0]           ? u->fg           : DOI_FG;
        const char* brc = u->border_color[0] ? u->border_color : DOI_BORDER_COLOR;
        const char* bfg = u->bar_fg[0]       ? u->bar_fg       : DOI_BAR_FG;
        const char* bbg = u->bar_bg[0]       ? u->bar_bg       : DOI_BAR_BG;
        int bw  = (u->bar_width  > 0) ? u->bar_width  : DOI_BAR_WIDTH;
        int ox  = (u->offset_x >= 0) ? u->offset_x : DOI_OFFSET_X;
        int oy  = (u->offset_y >= 0) ? u->offset_y : DOI_OFFSET_Y;
        int bh  = (u->bar_height > 0) ? u->bar_height : DOI_BAR_HEIGHT;
        int show_icon = (u->show_icon >= 0) ? u->show_icon : DOI_SHOW_ICON;
        int show_body = (u->show_body >= 0) ? u->show_body : DOI_SHOW_BODY;
        int i;

        /* reposition window */
        int wx = win_x(dpy, screen, win_w, pos_x, ox);
        int wy = win_y(dpy, screen, win_h, pos_y, oy, stack_idx);
        XMoveWindow(dpy, win, wx, wy);

        /* clear */
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
        XftDraw*  xd  = XftDrawCreate(dpy, win,
                DefaultVisual(dpy,screen), DefaultColormap(dpy,screen));
        XftColor  col;
        XftColorAllocName(dpy, DefaultVisual(dpy,screen),
                DefaultColormap(dpy,screen), fg, &col);

        int border_m = border + DOI_MARGIN;
        int tx = border_m + DOI_PADDING;

        /* measure content height for vertical centering */
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

        /* vertically center the block within win_h */
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
                const char* line = u->body;
                const char* nl;
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

/* ── measure window size from update ─────────────────────────────────── */

static void measure(Display* dpy, XftFont* font, int text_h,
                const NotifUpdate* u,
                int pos_x, int pos_y, int stack_idx,
                int* out_w, int* out_h) {
        int border   = (u->border >= 0) ? u->border : DOI_BORDER;
        int min_width = (u->min_width > 0) ? u->min_width : DOI_MIN_WIDTH;
        int bw = (u->bar_width  > 0) ? u->bar_width  : DOI_BAR_WIDTH;
        int bh = (u->bar_height > 0) ? u->bar_height : DOI_BAR_HEIGHT;
        int show_icon = (u->show_icon >= 0) ? u->show_icon : DOI_SHOW_ICON;
        int show_body = (u->show_body >= 0) ? u->show_body : DOI_SHOW_BODY;
        int inner = border + DOI_MARGIN + DOI_PADDING;
        int screen = DefaultScreen(dpy);

        int sum_w = 0, body_w = 0, body_lines = 0;
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
                        body_lines++;
                        line = nl ? nl+1 : NULL;
                }
        }

        int content_w = sum_w > body_w ? sum_w : body_w;
        if (u->show_bar && bw > content_w) content_w = bw;

        int win_w = inner * 2 + content_w;
        if (win_w < min_width) win_w = min_width;
        int max_w = DisplayWidth(dpy,screen) * DOI_MAX_WIDTH_PCT / 100;
        if (win_w > max_w) win_w = max_w;

        int win_h = inner + text_h + DOI_PADDING
                + (body_lines ? body_lines*(text_h+DOI_PADDING) : 0)
                + (u->show_bar ? bh + DOI_PADDING*2 : 0)
                + border + DOI_MARGIN;

        /* apply per-notification min_height */
        if (u->min_height > 0 && win_h < u->min_height)
                win_h = u->min_height;
        /* enforce uniform notification height */
        if (win_h < DOI_NOTIF_HEIGHT)
                win_h = DOI_NOTIF_HEIGHT;
        win_h = DOI_NOTIF_HEIGHT;   /* hard-fix: all windows same height */

        (void)pos_x; (void)pos_y; (void)stack_idx;
        *out_w = win_w;
        *out_h = win_h;
}

/* ── main render loop ─────────────────────────────────────────────────── */

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
        int pos_x = initial->pos_x;
        int pos_y = initial->pos_y;
        int stack_idx = initial->stack_index;
        struct timeval tv_store, *tv = NULL;
        int timeout = 0;

        setlocale(LC_ALL, "");

        dpy = XOpenDisplay(NULL);
        if (!dpy) { w_log("render: cannot open display"); return; }
        screen = DefaultScreen(dpy);

        font = XftFontOpenName(dpy, screen, DOI_FONT);
        if (!font) font = XftFontOpenName(dpy, screen, "monospace:size=10");
        if (!font) { w_log("render: no font"); XCloseDisplay(dpy); return; }

        int text_h = font->ascent + font->descent;

        /* create hidden window initially */
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

        /* set read_fd non-blocking */
        fcntl(read_fd, F_SETFL, O_NONBLOCK);

        XFlush(dpy);

        while (1) {
                fd_set fds;
                int maxfd;
                FD_ZERO(&fds);
                FD_SET(x11_fd, &fds);
                FD_SET(read_fd, &fds);
                maxfd = x11_fd > read_fd ? x11_fd : read_fd;

                /* check for new update from pipe */
                {
                        NotifUpdate u;
                        ssize_t n = read(read_fd, &u, sizeof(u));
                        if (n == (ssize_t)sizeof(u)) {
                                cur = u;
                                timeout = (u.timeout > 0) ? u.timeout : DOI_TIMEOUT;

                                /* resize window for new content */
                                measure(dpy, font, text_h, &u,
                                        pos_x, pos_y, stack_idx,
                                        &win_w, &win_h);
                                XResizeWindow(dpy, win, win_w, win_h);

                                if (!visible) {
                                        XMapRaised(dpy, win);
                                        visible = 1;
                                }

                                /* reset timeout */
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
                                continue;
                        }
                }

                if (!XPending(dpy)) {
                        int r = select(maxfd+1, &fds, NULL, NULL, tv);
                        if (r == 0) {
                                /* timeout — exit so daemon frees stack slot.
                                 * Persistent slots (timeout==0) never reach here. */
                                w_log("render: timeout pid=%d", (int)getpid());
                                goto done;
                        }
                        if (r < 0) continue;  /* EINTR or other — loop */

                        /* check pipe first */
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
