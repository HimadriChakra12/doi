# doi Makefile
# Usage:
#   make                          - build doid and doi
#   make MODULES="volume bright"  - build with modules
#   make MODULES=all              - build all modules
#   make install                  - install doid + doi
#   make install-modules          - install built modules
#   make install-all              - install everything
#   sudo make himadri             - build + install + modules + restart doid
#   make clean

CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -Wmissing-prototypes \
          -Wstrict-prototypes -Wold-style-definition \
          -D_POSIX_C_SOURCE=200809L -std=c99

DBUS_FLAGS  = $(shell pkg-config --cflags --libs dbus-1)
X11_FLAGS   = -lX11 -lXft $(shell pkg-config --cflags --libs fontconfig freetype2 2>/dev/null)

FLAGS_C = $(CFLAGS) $(DBUS_FLAGS)
FLAGS_D = $(CFLAGS) $(DBUS_FLAGS) $(X11_FLAGS)
FLAGS_M = $(CFLAGS) $(DBUS_FLAGS)

SRC     = src
MOD     = modules

DAEMON_SRC  = $(SRC)/daemon.c $(SRC)/render.c $(SRC)/log.c
CLIENT_SRC  = $(SRC)/client.c
MODULE_SRCS = $(MOD)/module.c

ALL_MODULES = volume bright

ifeq ($(MODULES),all)
MODULES = $(ALL_MODULES)
endif

HIMADRI_MODULES = bright volume

OUTPUT_C = doi
OUTPUT_D = doid

PREFIX    = /usr/local
BINDIR    = $(PREFIX)/bin

.PHONY: all clean install install-modules install-all himadri

all: $(OUTPUT_C) $(OUTPUT_D) modules

$(OUTPUT_C): $(CLIENT_SRC) config.h
	$(CC) $(CLIENT_SRC) $(FLAGS_C) -o $(OUTPUT_C)

$(OUTPUT_D): $(DAEMON_SRC) src/notif.h src/log.h config.h
	$(CC) $(DAEMON_SRC) $(FLAGS_D) -o $(OUTPUT_D)

modules: $(foreach m,$(MODULES),doi-$(m))

doi-volume: $(MOD)/volume.c $(MODULE_SRCS) config.h
	$(CC) $(MOD)/volume.c $(MODULE_SRCS) $(FLAGS_M) -o doi-volume

doi-bright: $(MOD)/bright.c $(MODULE_SRCS) config.h
	$(CC) $(MOD)/bright.c $(MODULE_SRCS) $(FLAGS_M) -o doi-bright

clean:
	@rm -fv $(OUTPUT_C) $(OUTPUT_D) $(foreach m,$(ALL_MODULES),doi-$(m))

install: $(OUTPUT_C) $(OUTPUT_D)
	@install -Dm755 $(OUTPUT_C) $(BINDIR)/doi
	@install -Dm755 $(OUTPUT_D) $(BINDIR)/doid
	@echo "installed doi  -> $(BINDIR)/doi"
	@echo "installed doid -> $(BINDIR)/doid"

install-modules:
	@for m in $(MODULES); do \
		if [ -f "doi-$$m" ]; then \
			install -Dm755 "doi-$$m" "$(BINDIR)/doi-$$m"; \
			echo "installed doi-$$m -> $(BINDIR)/doi-$$m"; \
		else \
			echo "SKIP: doi-$$m not built"; \
		fi; \
	done
	@if [ -f "$(MOD)/screenshot.sh" ]; then \
		install -Dm755 $(MOD)/screenshot.sh $(BINDIR)/doi-screenshot; \
		echo "installed doi-screenshot -> $(BINDIR)/doi-screenshot"; \
	fi

install-all: install install-modules

himadri: MODULES = $(HIMADRI_MODULES)
himadri: $(OUTPUT_C) $(OUTPUT_D) $(foreach m,$(HIMADRI_MODULES),doi-$(m))
	@echo "--- installing doi + doid ---"
	@install -Dm755 $(OUTPUT_C) $(BINDIR)/doi
	@install -Dm755 $(OUTPUT_D) $(BINDIR)/doid
	@echo "--- installing modules: $(HIMADRI_MODULES) ---"
	@for m in $(HIMADRI_MODULES); do \
		install -Dm755 "doi-$$m" "$(BINDIR)/doi-$$m"; \
		echo "installed doi-$$m -> $(BINDIR)/doi-$$m"; \
	done
	@install -Dm755 $(MOD)/screenshot.sh $(BINDIR)/doi-screenshot
	@echo "--- creating ~/.doi ---"
	@su $(SUDO_USER) -c "mkdir -p /home/$(SUDO_USER)/.doi"
	@echo "--- restarting doid ---"
	@pkill -TERM doid 2>/dev/null || true
	@sleep 0.3
	@pkill -KILL doid 2>/dev/null || true
	@pkill -KILL -f "doi-bright" 2>/dev/null || true
	@pkill -KILL -f "doi-volume" 2>/dev/null || true
	@sleep 0.2
	@DBUS_SESSION_BUS_ADDRESS="$$(cat /proc/$$(pgrep -u $(SUDO_USER) i3 \
		| head -1)/environ 2>/dev/null | tr '\0' '\n' \
		| grep DBUS_SESSION_BUS_ADDRESS | cut -d= -f2-)" \
	 DISPLAY="$$(cat /proc/$$(pgrep -u $(SUDO_USER) i3 \
		| head -1)/environ 2>/dev/null | tr '\0' '\n' \
		| grep ^DISPLAY= | cut -d= -f2-)" \
	 HOME="/home/$(SUDO_USER)" \
	 su $(SUDO_USER) -c "$(BINDIR)/doid" || true
	@echo "--- done ---"
