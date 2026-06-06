# doi — build system
# make                   build doid + doi
# make MODULES="vol bri" build with modules (vol, bri, med)
# make MODULES=all       build all modules
# make install           install to PREFIX (default /usr/local)
# make install-modules   install built modules
# make clean

CC     = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L

DBUS  = $(shell pkg-config --cflags --libs dbus-1)
X11   = -lX11 -lXft -lXext $(shell pkg-config --cflags --libs fontconfig freetype2 2>/dev/null)

PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin

SRC = src
MOD = modules

ALL_MODULES = vol bri med

ifeq ($(MODULES),all)
MODULES = $(ALL_MODULES)
endif

.PHONY: all modules clean install install-modules

all: doid doi modules

doid: $(SRC)/daemon.c $(SRC)/render.c $(SRC)/log.c $(SRC)/notif.h $(SRC)/log.h config.h
	$(CC) $(CFLAGS) $(SRC)/daemon.c $(SRC)/render.c $(SRC)/log.c $(DBUS) $(X11) -o $@

doi: $(SRC)/client.c config.h
	$(CC) $(CFLAGS) $< $(DBUS) -o $@

modules: $(foreach m,$(MODULES),doi-$(m))

doi-vol: $(MOD)/volume.c $(MOD)/module.c $(MOD)/module.h config.h
	$(CC) $(CFLAGS) $(MOD)/volume.c $(MOD)/module.c $(DBUS) -o $@

doi-bri: $(MOD)/bright.c $(MOD)/module.c $(MOD)/module.h config.h
	$(CC) $(CFLAGS) $(MOD)/bright.c $(MOD)/module.c $(DBUS) -o $@

doi-med: $(MOD)/media.c $(MOD)/module.c $(MOD)/module.h config.h
	$(CC) $(CFLAGS) $(MOD)/media.c $(MOD)/module.c $(DBUS) -o $@

install: doid doi
	install -Dm755 doid $(DESTDIR)$(BINDIR)/doid
	install -Dm755 doi  $(DESTDIR)$(BINDIR)/doi
	install -Dm644 doid.service /etc/systemd/system/doid.service
	systemctl daemon-reload
	systemctl enable --now doid
	@echo "installed -> $(BINDIR)/{doi,doid}"

install-modules:
	@for m in vol bri med; do \
		[ -f "doi-$$m" ] && install -Dm755 "doi-$$m" "$(DESTDIR)$(BINDIR)/doi-$$m" \
		                 && echo "installed doi-$$m" || true; \
	done
	@[ -f "$(MOD)/screenshot.sh" ] && \
		install -Dm755 $(MOD)/screenshot.sh $(DESTDIR)$(BINDIR)/doi-screenshot || true
	@[ -f "$(MOD)/media.sh" ] && \
		install -Dm755 $(MOD)/media.sh $(DESTDIR)$(BINDIR)/doi-media-sh || true

clean:
	rm -f doid doi doi-vol doi-bri doi-med
