# doi — suckless notification system
#
# Typical install workflow:
#
#   cp config.def.h config.h   # first time only — then edit config.h
#   sudo make dmon-install     # build + install binaries to /usr/local/bin
#   make dmon-enable           # register + start the systemd user unit
#                                (run WITHOUT sudo, as your normal user)
#
# Other targets:
#   make              build doi (client) only
#   make dmon         build doid (daemon) only
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

# ── config.h (suckless pattern) ────────────────────────────────────────
# config.h is the user's copy — never overwrite it if it already exists.
# config.def.h is the upstream default tracked in git.

config.h:
	cp config.def.h config.h

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

install: doi config.h
	install -Dm755 doi $(DESTDIR)$(BINDIR)/doi
	@echo "installed -> $(BINDIR)/doi"

# Installs binaries only — does NOT touch systemd.
# After this, run  make dmon-enable  (without sudo) to register the unit.
dmon-install: doid doi config.h
	install -Dm755 doid $(DESTDIR)$(BINDIR)/doid
	install -Dm755 doi  $(DESTDIR)$(BINDIR)/doi
	@echo "installed -> $(BINDIR)/doid  $(BINDIR)/doi"
	@echo ""
	@echo "Next step (WITHOUT sudo):"
	@echo "  make dmon-enable"

# ── systemd user unit (run WITHOUT sudo) ──────────────────────────────

UNIT_DIR  = $(HOME)/.config/systemd/user
UNIT_FILE = $(UNIT_DIR)/doid.service

dmon-enable:
	@if [ -n "$$SUDO_USER" ]; then \
		USER_HOME="$$(getent passwd "$$SUDO_USER" | cut -d: -f6)"; \
		echo "Running dmon-enable as $$SUDO_USER"; \
		install -Dm644 doid.service "$$USER_HOME/.config/systemd/user/doid.service"; \
		sudo -u "$$SUDO_USER" XDG_RUNTIME_DIR="/run/user/$$(id -u "$$SUDO_USER")" \
			systemctl --user daemon-reload; \
		sudo -u "$$SUDO_USER" XDG_RUNTIME_DIR="/run/user/$$(id -u "$$SUDO_USER")" \
			systemctl --user enable --now doid; \
		echo "doid enabled and started for user $$SUDO_USER"; \
	else \
		install -Dm644 doid.service $(UNIT_FILE); \
		systemctl --user daemon-reload; \
		systemctl --user enable --now doid; \
		echo "doid enabled and started for user $$USER"; \
	fi

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
# Does NOT remove config.h — that's the user's file.

clean: clean-doi clean-dmon

clean-doi:
	rm -f doi

clean-dmon:
	rm -f doid

.PHONY: all doi dmon doid \
        install dmon-install dmon-enable dmon-disable \
        clean clean-doi clean-dmon
