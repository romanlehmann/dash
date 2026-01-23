#!/usr/bin/env bash
set -euo pipefail

WLAN_IFACE="wlan0"
WAN_IFACE="eth1"
WLAN_ADDR="192.168.50.1/24"

iptables -t nat -D POSTROUTING -o "${WAN_IFACE}" -j MASQUERADE 2>/dev/null || true
iptables -D FORWARD -i "${WLAN_IFACE}" -o "${WAN_IFACE}" -j ACCEPT 2>/dev/null || true
iptables -D FORWARD -i "${WAN_IFACE}" -o "${WLAN_IFACE}" -m state --state RELATED,ESTABLISHED -j ACCEPT 2>/dev/null || true

if ip addr show dev "${WLAN_IFACE}" | grep -q "${WLAN_ADDR}"; then
    ip addr del "${WLAN_ADDR}" dev "${WLAN_IFACE}"
fi

ip link set "${WLAN_IFACE}" down || true
