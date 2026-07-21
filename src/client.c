/* doi — notification client
 *
 * Sends a single org.freedesktop.Notifications.Notify call over the
 * session D-Bus, targeting the doid daemon.
 *
 * Usage:
 *   doi [OPTIONS] SUMMARY
 *
 * Options:
 *   -t SECONDS   auto-dismiss after N seconds (0 = wait for click)
 *   -i GLYPH     icon glyph or emoji shown before SUMMARY
 *   -b BODY      secondary body text shown below SUMMARY
 *   -m MODULE    activate a built-in module hint
 *                  vol    VALUE   volume bar (0-100)
 *                  bright VALUE   brightness bar (0-100)
 *                  media  TEXT    now-playing line
 *   -h           print this help and exit
 *
 * Module hints tell doid to use per-module geometry / colours from
 * config.h and to reuse a persistent window slot instead of stacking.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>
#include "../config.h"

/* ── helpers ─────────────────────────────────────────────────────────── */

static void usage(FILE *out) {
	fputs(
		"usage: doi [OPTIONS] SUMMARY\n"
		"\n"
		"options:\n"
		"  -t SECONDS   timeout (0 = click to dismiss, default: "
		"5" ")\n"
		"  -i GLYPH     icon glyph / emoji before summary\n"
		"  -b BODY      body text below summary\n"
		"  -m MODULE    use a built-in module slot:\n"
		"                 vol VALUE      volume bar     (0-100)\n"
		"                 bright VALUE   brightness bar (0-100)\n"
		"                 media TEXT     now-playing text\n"
		"  -h           show this help\n"
		"\n"
		"module examples:\n"
		"  doi -m vol 75\n"
		"  doi -m bright 40\n"
		"  doi -m media \"Radiohead — Karma Police\"\n",
		out);
}

/* Append a single {string, variant} entry to an open DBUS_TYPE_ARRAY
 * dict iterator.                                                       */
static void hint_str(DBusMessageIter *dict,
                     const char *key, const char *val) {
	DBusMessageIter entry, var;
	dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY,
	                                 NULL, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT,
	                                 DBUS_TYPE_STRING_AS_STRING, &var);
	dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &val);
	dbus_message_iter_close_container(&entry, &var);
	dbus_message_iter_close_container(dict, &entry);
}

static void hint_int(DBusMessageIter *dict,
                     const char *key, int val) {
	DBusMessageIter entry, var;
	dbus_int32_t v = (dbus_int32_t)val;
	dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY,
	                                 NULL, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT,
	                                 DBUS_TYPE_INT32_AS_STRING, &var);
	dbus_message_iter_append_basic(&var, DBUS_TYPE_INT32, &v);
	dbus_message_iter_close_container(&entry, &var);
	dbus_message_iter_close_container(dict, &entry);
}

/* ── module hint tables ──────────────────────────────────────────────── */

/* Emit x-doi-* hints for the "vol" module. */
static void hints_vol(DBusMessageIter *dict, int value) {
	hint_str(dict, "x-doi-bg",           VOL_BG);
	hint_str(dict, "x-doi-fg",           VOL_FG);
	hint_str(dict, "x-doi-border-color", VOL_BORDER_COLOR);
	hint_int(dict, "x-doi-border",       VOL_BORDER);
	hint_int(dict, "x-doi-timeout",      VOL_TIMEOUT);
	hint_int(dict, "x-doi-pos-x",        VOL_POS_X);
	hint_int(dict, "x-doi-pos-y",        VOL_POS_Y);
	hint_int(dict, "x-doi-offset-x",     VOL_OFFSET_X);
	hint_int(dict, "x-doi-offset-y",     VOL_OFFSET_Y);
	hint_int(dict, "x-doi-min-width",    VOL_MIN_WIDTH);
	hint_int(dict, "x-doi-min-height",   VOL_MIN_HEIGHT);
	hint_int(dict, "x-doi-show-bar",     1);
	hint_int(dict, "x-doi-bar-value",    value);
	hint_int(dict, "x-doi-bar-width",    VOL_BAR_WIDTH);
	hint_int(dict, "x-doi-bar-height",   VOL_BAR_HEIGHT);
	hint_str(dict, "x-doi-bar-bg",       VOL_BAR_BG);
	hint_str(dict, "x-doi-bar-fg",       VOL_BAR_FG);
	hint_int(dict, "x-doi-show-body",    0);
	hint_int(dict, "x-doi-layout",       1);  /* block: fixed width  */
}

static void hints_bright(DBusMessageIter *dict, int value) {
	hint_str(dict, "x-doi-bg",           BRIGHT_BG);
	hint_str(dict, "x-doi-fg",           BRIGHT_FG);
	hint_str(dict, "x-doi-border-color", BRIGHT_BORDER_COLOR);
	hint_int(dict, "x-doi-border",       BRIGHT_BORDER);
	hint_int(dict, "x-doi-timeout",      BRIGHT_TIMEOUT);
	hint_int(dict, "x-doi-pos-x",        BRIGHT_POS_X);
	hint_int(dict, "x-doi-pos-y",        BRIGHT_POS_Y);
	hint_int(dict, "x-doi-offset-x",     BRIGHT_OFFSET_X);
	hint_int(dict, "x-doi-offset-y",     BRIGHT_OFFSET_Y);
	hint_int(dict, "x-doi-min-width",    BRIGHT_MIN_WIDTH);
	hint_int(dict, "x-doi-min-height",   BRIGHT_MIN_HEIGHT);
	hint_int(dict, "x-doi-show-bar",     1);
	hint_int(dict, "x-doi-bar-value",    value);
	hint_int(dict, "x-doi-bar-width",    BRIGHT_BAR_WIDTH);
	hint_int(dict, "x-doi-bar-height",   BRIGHT_BAR_HEIGHT);
	hint_str(dict, "x-doi-bar-bg",       BRIGHT_BAR_BG);
	hint_str(dict, "x-doi-bar-fg",       BRIGHT_BAR_FG);
	hint_int(dict, "x-doi-show-body",    0);
	hint_int(dict, "x-doi-layout",       1);
}

static void hints_media(DBusMessageIter *dict) {
	hint_str(dict, "x-doi-bg",           MEDIA_BG);
	hint_str(dict, "x-doi-fg",           MEDIA_FG);
	hint_str(dict, "x-doi-border-color", MEDIA_BORDER_COLOR);
	hint_int(dict, "x-doi-border",       MEDIA_BORDER);
	hint_int(dict, "x-doi-timeout",      MEDIA_TIMEOUT);
	hint_int(dict, "x-doi-pos-x",        MEDIA_POS_X);
	hint_int(dict, "x-doi-pos-y",        MEDIA_POS_Y);
	hint_int(dict, "x-doi-offset-x",     MEDIA_OFFSET_X);
	hint_int(dict, "x-doi-offset-y",     MEDIA_OFFSET_Y);
	hint_int(dict, "x-doi-min-width",    MEDIA_MIN_WIDTH);
	hint_int(dict, "x-doi-min-height",   MEDIA_MIN_HEIGHT);
	hint_int(dict, "x-doi-show-body",    0);
	hint_int(dict, "x-doi-layout",       1);
}

/* ── main ────────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
	int i;

	/* parsed arguments */
	const char *summary    = NULL;
	const char *body       = "";
	const char *icon       = "";
	const char *module     = NULL;  /* "vol", "bright", "media", or NULL */
	const char *mod_arg    = NULL;  /* module's VALUE or TEXT argument    */
	int         timeout_s  = TIMEOUT;
	int         module_val = 0;     /* numeric value for vol/bright       */

	/* D-Bus objects */
	DBusConnection *conn;
	DBusError       err;
	DBusMessage    *msg, *reply;
	DBusMessageIter args, hints_arr;
	dbus_uint32_t   replace_id  = 0;
	dbus_uint32_t   notif_id;
	dbus_int32_t    timeout_ms;
	const char     *app_name    = "doi";

	/* ── argument parsing ──────────────────────────────────────────── */
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			usage(stdout);
			return EXIT_SUCCESS;
		}
		if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
			timeout_s = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
			icon = argv[++i];
		} else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
			body = argv[++i];
		} else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
			module = argv[++i];
			/* modules that take a follow-on argument */
			if ((strcmp(module, "vol")    == 0 ||
			     strcmp(module, "bright") == 0 ||
			     strcmp(module, "media")  == 0) && i + 1 < argc) {
				mod_arg = argv[++i];
			}
		} else if (argv[i][0] != '-' && !summary) {
			summary = argv[i];
		} else {
			usage(stderr);
			return EXIT_FAILURE;
		}
	}

	/* modules supply their own summary from mod_arg when no explicit
	 * SUMMARY was given on the command line */
	if (!summary) {
		if (mod_arg) {
			summary = mod_arg;
		} else {
			usage(stderr);
			return EXIT_FAILURE;
		}
	}

	/* for vol/bright the mod_arg is a number, not the summary */
	if (module && (strcmp(module, "vol") == 0 ||
	               strcmp(module, "bright") == 0)) {
		if (mod_arg)
			module_val = atoi(mod_arg);
		/* provide a human-readable summary if none given */
		if (summary == mod_arg) {
			static char auto_summary[32];
			const char *label = (strcmp(module, "vol") == 0)
			                    ? "Volume" : "Brightness";
			snprintf(auto_summary, sizeof(auto_summary),
			         "%s: %d%%", label, module_val);
			summary = auto_summary;
		}
	}

	/* ── D-Bus connect ─────────────────────────────────────────────── */
	dbus_error_init(&err);
	conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
	if (dbus_error_is_set(&err)) {
		fprintf(stderr, "doi: dbus connect: %s\n", err.message);
		dbus_error_free(&err);
		return EXIT_FAILURE;
	}

	/* ── build Notify call ─────────────────────────────────────────── */
	msg = dbus_message_new_method_call(
		"org.freedesktop.Notifications",
		"/org/freedesktop/Notifications",
		"org.freedesktop.Notifications",
		"Notify");
	if (!msg) {
		fputs("doi: dbus_message_new_method_call failed\n", stderr);
		return EXIT_FAILURE;
	}

	timeout_ms = (dbus_int32_t)(timeout_s * 1000);

	dbus_message_iter_init_append(msg, &args);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &app_name);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &replace_id);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &icon);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &summary);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &body);

	/* actions array (empty) */
	{
		DBusMessageIter act;
		dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY,
			DBUS_TYPE_STRING_AS_STRING, &act);
		dbus_message_iter_close_container(&args, &act);
	}

	/* hints dict — empty for plain notifications; module hints added
	 * below when -m is given                                           */
	dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY,
		"{sv}", &hints_arr);

	if (module) {
		if (strcmp(module, "vol") == 0) {
			hints_vol(&hints_arr, module_val);
		} else if (strcmp(module, "bright") == 0) {
			hints_bright(&hints_arr, module_val);
		} else if (strcmp(module, "media") == 0) {
			hints_media(&hints_arr);
		} else {
			fprintf(stderr, "doi: unknown module '%s' "
			        "(try: vol, bright, media)\n", module);
			dbus_message_unref(msg);
			return EXIT_FAILURE;
		}
	}

	dbus_message_iter_close_container(&args, &hints_arr);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &timeout_ms);

	/* ── send and wait for notification ID ────────────────────────── */
	reply = dbus_connection_send_with_reply_and_block(conn, msg, 2000, &err);
	if (dbus_error_is_set(&err)) {
		fprintf(stderr, "doi: send failed: %s\n", err.message);
		dbus_error_free(&err);
		dbus_message_unref(msg);
		return EXIT_FAILURE;
	}

	dbus_message_get_args(reply, &err,
		DBUS_TYPE_UINT32, &notif_id, DBUS_TYPE_INVALID);
	(void)notif_id;

	dbus_message_unref(reply);
	dbus_message_unref(msg);
	return EXIT_SUCCESS;
}
