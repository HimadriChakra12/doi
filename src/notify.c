#include <sys/time.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "notify.h"
#include "log.h"
#include "../config.h"

#ifdef BND_USE_IMLIB2
#include <Imlib2.h>
#endif

static XFontSet get_font(Display* dpy) {
        int nmissing;
        char** missing;
        char* def;
        XFontSet font = XCreateFontSet(dpy, BND_FONT,
                &missing, &nmissing, &def);
        XFreeStringList(missing);
        return font;
}

static int calc_x(Display* dpy, int screen, int win_w, int pos_x) {
        int sw = DisplayWidth(dpy, screen);
        switch (pos_x) {
                case BND_LEFT:   return BND_OFFSET_X;
                case BND_CENTER: return (sw - win_w) / 2;
                case BND_RIGHT:  return sw - win_w - BND_OFFSET_X;
                default:         return BND_OFFSET_X;
        }
}

static int calc_y(Display* dpy, int screen, int win_h, int pos_y,
                int stack_index) {
        int sh = DisplayHeight(dpy, screen);
        int step = BND_STACK_HEIGHT + BND_STACK_GAP;
        switch (pos_y) {
                case BND_TOP:
                        return BND_OFFSET_Y + stack_index * step;
                case BND_MIDDLE:
                        return (sh - win_h) / 2 + stack_index * step;
                case BND_BOTTOM:
                        return sh - win_h - BND_OFFSET_Y - stack_index * step;
                default:
                        return BND_OFFSET_Y + stack_index * step;
        }
}

static void draw_border(Display* dpy, Window win, GC gc,
                int win_w, int win_h, int border, const char* color) {
        XColor col, dummy;
        Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));
        int i;
        if (border <= 0) return;
        XAllocNamedColor(dpy, cmap, color, &col, &dummy);
        XSetForeground(dpy, gc, col.pixel);
        for (i = 0; i < border; ++i)
                XDrawRectangle(dpy, win, gc,
                        i, i,
                        win_w - 1 - (i * 2),
                        win_h - 1 - (i * 2));
}

static void draw_bar(Display* dpy, Window win, GC gc,
                int x, int y, int value) {
        XColor fg, bg, dummy;
        Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));
        int filled = (BND_BAR_WIDTH * value) / 100;
        XAllocNamedColor(dpy, cmap, BND_BAR_BG, &bg, &dummy);
        XSetForeground(dpy, gc, bg.pixel);
        XFillRectangle(dpy, win, gc, x, y, BND_BAR_WIDTH, BND_BAR_HEIGHT);
        XAllocNamedColor(dpy, cmap, BND_BAR_FG, &fg, &dummy);
        XSetForeground(dpy, gc, fg.pixel);
        XFillRectangle(dpy, win, gc, x, y, filled, BND_BAR_HEIGHT);
}

static void draw_content(Display* dpy, Window win, GC gc,
                XFontSet font, const Notification* n,
                int win_w, int win_h,
                int text_h,
                int border, const char* fg, const char* border_color,
                int show_icon, int show_body) {
        int cur_y, tx;
        XColor fgcol, dummy;

        draw_border(dpy, win, gc, win_w, win_h, border, border_color);

        XAllocNamedColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)),
                fg, &fgcol, &dummy);
        XSetForeground(dpy, gc, fgcol.pixel);

        cur_y = (BND_MARGIN + border) + text_h;
        tx    = (BND_MARGIN + border) + BND_PADDING;

        if (show_icon && n->icon && n->icon[0]) {
#ifdef BND_USE_IMLIB2
                Imlib_Image img = imlib_load_image(n->icon);
                if (img) {
                        int s = DefaultScreen(dpy);
                        imlib_context_set_image(img);
                        imlib_context_set_display(dpy);
                        imlib_context_set_visual(DefaultVisual(dpy, s));
                        imlib_context_set_colormap(DefaultColormap(dpy, s));
                        imlib_context_set_drawable(win);
                        imlib_render_image_on_drawable_at_size(
                                BND_MARGIN + border, BND_MARGIN + border,
                                BND_ICON_SIZE, BND_ICON_SIZE);
                        imlib_free_image();
                }
                tx += BND_ICON_SIZE + BND_PADDING;
#else
                Xutf8DrawString(dpy, win, font, gc,
                        BND_MARGIN + border, cur_y,
                        n->icon, strlen(n->icon));
                tx += text_h + BND_PADDING;
#endif
        }

        if (n->summary && n->summary[0])
                Xutf8DrawString(dpy, win, font, gc,
                        tx, cur_y, n->summary, strlen(n->summary));

        if (show_body && n->body && n->body[0]) {
                const char* line = n->body;
                const char* nl;
                cur_y += text_h + BND_PADDING;
                while (line && *line) {
                        nl = strchr(line, '\n');
                        int len = nl ? (int)(nl - line) : (int)strlen(line);
                        if (len > 0)
                                Xutf8DrawString(dpy, win, font, gc,
                                        tx, cur_y, line, len);
                        cur_y += text_h + BND_PADDING;
                        line = nl ? nl + 1 : NULL;
                }
        }

        if (n->show_bar) {
                cur_y += BND_PADDING;
                draw_bar(dpy, win, gc,
                        BND_MARGIN + border, cur_y, n->bar_value);
        }
}

int notify(Notification* n) {
        int j, nfonts;
        int x11_fd;
        struct timeval rtv;
        struct timeval* tv = &rtv;
        int timeout, border;
        fd_set in_fds;
        int win_x, win_y, win_w, win_h, screen;
        int text_w, text_h, body_w, body_h;
        Window win;
        XEvent ev;
        Display* dpy;
        GC gc;
        XSetWindowAttributes att;
        XGCValues val;
        XColor col, dummy;
        XFontSet font;
        XFontStruct** fonts;
        char** font_names;
        const char* bg;
        const char* fg;
        const char* border_color;
        int show_icon, show_body;
        int mapped = 0;

        setlocale(LC_ALL, getenv("LANG"));

        bg           = (n->bg)           ? n->bg           : BND_BG;
        fg           = (n->fg)           ? n->fg           : BND_FG;
        border_color = (n->border_color) ? n->border_color : BND_BORDER_COLOR;
        border       = (n->border >= 0)  ? n->border       : BND_BORDER;
        timeout      = (n->timeout > 0)  ? n->timeout      : BND_TIMEOUT;
        show_icon    = (n->show_icon >= 0) ? n->show_icon  : BND_SHOW_ICON;
        show_body    = (n->show_body >= 0) ? n->show_body  : BND_SHOW_BODY;

        w_log("notify: summary=%s body=%s bg=%s fg=%s border=%d pos=%d,%d bar=%d val=%d",
                n->summary ? n->summary : "",
                n->body    ? n->body    : "",
                bg, fg, border, n->pos_x, n->pos_y,
                n->show_bar, n->bar_value);

        dpy = XOpenDisplay(NULL);
        if (!dpy) { w_log("ERROR: cannot open display."); return 1; }
        screen = DefaultScreen(dpy);

        font   = get_font(dpy);
        nfonts = XFontsOfFontSet(font, &fonts, &font_names);
        text_h = 0;
        for (j = 0; j < nfonts; ++j) {
                int h = fonts[j]->ascent + fonts[j]->descent;
                if (h > text_h) text_h = h;
        }

        text_w = n->summary ? Xutf8TextEscapement(font, n->summary,
                strlen(n->summary)) : 0;
        body_w = (show_body && n->body && n->body[0]) ?
                Xutf8TextEscapement(font, n->body, strlen(n->body)) : 0;
        body_h = (show_body && n->body && n->body[0]) ? text_h : 0;

        /* count newlines in body for multiline height */
        if (show_body && n->body) {
                const char* p = n->body;
                while (*p) { if (*p++ == '\n') body_h += text_h + BND_PADDING; }
        }

        win_w = (BND_MARGIN + border) * 2 + BND_PADDING * 2;
        if (show_icon && n->icon && n->icon[0])
                win_w += BND_ICON_SIZE + BND_PADDING;
        win_w += (text_w > body_w ? text_w : body_w);
        /* cap width at screen width * 0.4 */
        {
                int maxw = (int)(DisplayWidth(dpy, screen) * 0.4);
                if (win_w > maxw) win_w = maxw;
        }
        if (n->show_bar && win_w < BND_BAR_WIDTH + (BND_MARGIN + border) * 2)
                win_w = BND_BAR_WIDTH + (BND_MARGIN + border) * 2;

        win_h = (BND_MARGIN + border) * 2 + text_h;
        if (body_h)      win_h += body_h + BND_PADDING;
        if (n->show_bar) win_h += BND_BAR_HEIGHT + BND_PADDING * 2;

        win_x = calc_x(dpy, screen, win_w, n->pos_x);
        win_y = calc_y(dpy, screen, win_h, n->pos_y, n->stack_index);

        XAllocNamedColor(dpy, DefaultColormap(dpy, screen),
                bg, &col, &dummy);
        att.background_pixel  = col.pixel;
        att.override_redirect = True;

        win = XCreateWindow(dpy, DefaultRootWindow(dpy),
                win_x, win_y, win_w, win_h,
                0, CopyFromParent, InputOutput, CopyFromParent,
                CWBackPixel|CWOverrideRedirect, &att);

        /* listen for Expose and MapNotify too */
        XSelectInput(dpy, win,
                ButtonPressMask|StructureNotifyMask|ExposureMask);
        XMapWindow(dpy, win);

        val.foreground = XWhitePixel(dpy, screen);
        gc = XCreateGC(dpy, win, GCForeground, &val);

        /* timing */
        x11_fd = ConnectionNumber(dpy);
        FD_ZERO(&in_fds);
        FD_SET(x11_fd, &in_fds);

        if (timeout > 0) {
                tv->tv_usec = 0;
                tv->tv_sec  = timeout;
        } else {
                tv = NULL;
        }

        XFlush(dpy);

        while (1) {
                if (!XPending(dpy)) {
                        if (!select(x11_fd + 1, &in_fds, NULL, NULL, tv)) {
                                XCloseDisplay(dpy);
                                w_log("timeout");
                                return 0;
                        }
                }

                while (XPending(dpy)) {
                        XNextEvent(dpy, &ev);
                        switch (ev.type) {
                        case MapNotify:
                                mapped = 1;
                                draw_content(dpy, win, gc, font, n,
                                        win_w, win_h, text_h,
                                        border, fg, border_color,
                                        show_icon, show_body);
                                XFlush(dpy);
                                break;
                        case Expose:
                                if (mapped && ev.xexpose.count == 0) {
                                        draw_content(dpy, win, gc, font, n,
                                                win_w, win_h, text_h,
                                                border, fg, border_color,
                                                show_icon, show_body);
                                        XFlush(dpy);
                                }
                                break;
                        case ButtonPress:
                                XCloseDisplay(dpy);
                                return 0;
                        }
                }
        }
}
