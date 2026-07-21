# doi — suckless notification system
#
# Typical install workflow:
#
#   sudo make dmon-install    # build + install binaries to /usr/local/bin
#   make dmon-enable          # register + start the systemd user unit
#                               (run WITHOUT sudo, as your normal user)
#
# Other targets:
#   make              build doi (notification client) only
#   make dmon         build doid (notification daemon) only
#   make install      install doi only (no daemon)
#   make dmon-enable  enable/start doid.service for the current user
#   make dmon-disable stop + disable doid.service for the current user
#   make clean        remove built binaries

CC     = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L \
         -Os -pipe

DBUS = $(shell pkg-config --cflags --libs dbus-1)
X11  = -lX11 -lXft -lXext \
       $(shell pkg-config --cflags --libs fontconfig freetype2 2>/dev/null)

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

SRC = src
DMN = dmon

# ── build ──────────────────────────────────────────────────────────────

all: doi

doi: $(SRC)/client.c config.h
	$(CC) $(CFLAGS) $(SRC)/client.c $(DBUS) -o doi

doid: $(DMN)/daemon.c $(SRC)/render.c $(SRC)/log.c \
      $(SRC)/notif.h $(SRC)/log.h config.h
	$(CC) $(CFLAGS) \
	      $(DMN)/daemon.c $(SRC)/render.c $(SRC)/log.c \
	      $(DBUS) $(X11) -o doid

dmon: doid

# ── install (run with sudo) ────────────────────────────────────────────

install: doi
	install -Dm755 doi $(DESTDIR)$(BINDIR)/doi
	@echo "installed -> $(BINDIR)/doi"

# Installs binaries only — does NOT touch systemd.
# Requires root because BINDIR is /usr/local/bin.
# After this, run  make dmon-enable  (without sudo) to set up the unit.
dmon-install: doid doi
	install -Dm755 doid $(DESTDIR)$(BINDIR)/doid
	install -Dm755 doi  $(DESTDIR)$(BINDIR)/doi
	@echo "installed -> $(BINDIR)/doid  $(BINDIR)/doi"
	@echo ""
	@echo "Next step (run WITHOUT sudo):"
	@echo "  make dmon-enable"

# ── systemd user unit (run WITHOUT sudo) ──────────────────────────────

# Detect the real user even when called through sudo.
# SUDO_USER is set by sudo; fall back to USER if running normally.
REAL_USER  = $(firstword $(SUDO_USER) $(USER))
REAL_HOME  = $(shell getent passwd $(REAL_USER) | cut -d: -f6)
UNIT_DIR   = $(REAL_HOME)/.config/systemd/user
UNIT_FILE  = $(UNIT_DIR)/doid.service

# Install the unit file and start the service as the real (non-root) user.
# Must be run without sudo so systemctl --user can reach the session bus.
dmon-enable:
	@if [ "$$(id -u)" = "0" ]; then \
	  echo "ERROR: run  make dmon-enable  WITHOUT sudo (as your normal user)." >&2; \
	  exit 1; \
	fi
	install -Dm644 doid.service $(UNIT_FILE)
	systemctl --user daemon-reload
	systemctl --user enable --now doid
	@echo "doid enabled and started for user $$USER"

# Stop and remove the unit (also without sudo).
dmon-disable:
	@if [ "$$(id -u)" = "0" ]; then \
	  echo "ERROR: run  make dmon-disable  WITHOUT sudo." >&2; \
	  exit 1; \
	fi
	-systemctl --user disable --now doid
	rm -f $(UNIT_FILE)
	systemctl --user daemon-reload
	@echo "doid disabled for user $$USER"

# ── clean ─────────────────────────────────────────────────────────────

clean: clean-doi clean-dmon

clean-doi:
	rm -f doi

clean-dmon:
	rm -f doid

.PHONY: all doi dmon doid \
        install dmon-install dmon-enable dmon-disable \
        clean clean-doi clean-dmon
