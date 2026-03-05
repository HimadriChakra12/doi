/*
 * doi wifi module stub
 *
 * Implement this to get wifi connect/disconnect notifications.
 * This is intentionally left as a stub — wifi event sources vary
 * widely (NetworkManager, iwd, wpa_supplicant, dhcpcd hooks, etc.)
 * and are best handled by the user's own setup.
 *
 * IMPLEMENTATION OPTIONS:
 *
 * Option A: NetworkManager dispatcher script
 *   Place a script in /etc/NetworkManager/dispatcher.d/ that calls:
 *     doi -i "📶" "WiFi" "Connected to $CONNECTION_ID"
 *
 * Option B: iwd state change hook
 *   Poll or watch /var/lib/iwd/*.psk via inotify.
 *
 * Option C: DBus monitor (NetworkManager)
 *   Watch org.freedesktop.NetworkManager on DBus for StateChanged signals.
 *
 * Option D: dhcpcd hook
 *   Add to /etc/dhcpcd.exit-hook:
 *     [ "$reason" = "BOUND" ] && doi "📶 Connected" "$interface"
 *
 * QUICK EXAMPLE (NetworkManager dispatcher):
 *
 *   #!/bin/sh
 *   # /etc/NetworkManager/dispatcher.d/99-doi-wifi
 *   IFACE=$1
 *   ACTION=$2
 *   case "$ACTION" in
 *     up)   doi -i "📶" "WiFi connected"   "$CONNECTION_ID on $IFACE" ;;
 *     down) doi -i "📵" "WiFi disconnected" "$IFACE" ;;
 *   esac
 *
 * CONFIGURATION: see BND_WIFI_* in config.h
 */

#ifndef WIFI_H
#define WIFI_H

/* Call this from your wifi event handler to show a notification */
int doi_wifi_notify(const char* event, const char* iface, const char* ssid);

#endif
