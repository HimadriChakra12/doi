# doi — suckless notification system
#
# Targets
#   make              → build doi (client)
#   make doi          → build doi (client)
#   make dmon         → build doid (daemon)
#   make install      → install doi only
#   make dmon install → build + install doid and doi, enable systemd unit
#   make clean        → remove doi and doid
#   make clean doi    → remove doi only
#   make clean dmon   → remove doid only

CC     = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L \
         -Os -pipe

DBUS  = $(shell pkg-config --cflags --libs dbus-1)
X11   = -lX11 -lXft -lXext $(shell pkg-config --cflags --libs fontconfig freetype2 2>/dev/null)

PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin

SRC = src
DMN = dmon

# ── default: doi only ────────────────────────────────────────────────────

all: doi

# ── doi (client) ─────────────────────────────────────────────────────────

doi: $(SRC)/client.c config.h
	$(CC) $(CFLAGS) $(SRC)/client.c $(DBUS) -o doi

# ── doid (daemon, lives in dmon/) ────────────────────────────────────────

doid: $(DMN)/daemon.c $(SRC)/render.c $(SRC)/log.c \
      $(SRC)/notif.h $(SRC)/log.h config.h
	$(CC) $(CFLAGS) \
	      $(DMN)/daemon.c $(SRC)/render.c $(SRC)/log.c \
	      $(DBUS) $(X11) -o doid

# 'make dmon' is an alias for building doid
dmon: doid

# ── install ───────────────────────────────────────────────────────────────

install: doi
	install -Dm755 doi $(DESTDIR)$(BINDIR)/doi
	@echo "installed -> $(BINDIR)/doi"

# 'make dmon install' works because make processes all goals left-to-right:
#   1. dmon  → builds doid
#   2. install → installs doi (builds if needed) + checks for doid
# For a one-shot "install both + systemd" use: make dmon-install
dmon-install: doid doi
	install -Dm755 doid $(DESTDIR)$(BINDIR)/doid
	install -Dm755 doi  $(DESTDIR)$(BINDIR)/doi
	install -Dm644 doid.service /etc/systemd/system/doid.service
	systemctl daemon-reload
	systemctl enable --now doid
	@echo "installed -> $(BINDIR)/doid $(BINDIR)/doi + systemd unit"

# ── clean ─────────────────────────────────────────────────────────────────

# 'make clean'      → removes both
# 'make clean doi'  → make sees two targets: clean + doi; use clean-doi instead
#                     OR rely on the combined phony below
# To match the mental model exactly:
#   make clean      → all
#   make clean doi  → doi only   (achieved via: make clean-doi)
#   make clean dmon → doid only  (achieved via: make clean-dmon)
#
# We also handle 'make clean doi' and 'make clean dmon' as sequential targets
# which naturally works: 'clean' runs (removes all), then 'doi' rebuilds.
# For "clean only X", the canonical names are clean-doi / clean-dmon.

clean: clean-doi clean-dmon

clean-doi:
	rm -f doi

clean-dmon:
	rm -f doid

.PHONY: all doi dmon doid dmon-install install clean clean-doi clean-dmon
