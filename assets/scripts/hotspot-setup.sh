#!/usr/bin/env bash
set -euo pipefail

WLAN_IFACE="wlan0"
WAN_IFACE="eth1"
WLAN_ADDR="192.168.50.1/24"
COUNTRY_CODE="DE"

max_wait=15
while ! ip link show dev "${WLAN_IFACE}" >/dev/null 2>&1; do
    if [ "${max_wait}" -le 0 ]; then
        echo "Interface ${WLAN_IFACE} not ready" >&2
        exit 1
    fi
    sleep 1
    max_wait=$((max_wait - 1))
done

iw reg set "${COUNTRY_CODE}"

ip link set "${WLAN_IFACE}" up

if ! ip addr show dev "${WLAN_IFACE}" | grep -q "${WLAN_ADDR}"; then
    ip addr add "${WLAN_ADDR}" dev "${WLAN_IFACE}"
fi

sysctl -w net.ipv4.ip_forward=1 >/dev/null

iptables -t nat -C POSTROUTING -o "${WAN_IFACE}" -j MASQUERADE 2>/dev/null \
    || iptables -t nat -A POSTROUTING -o "${WAN_IFACE}" -j MASQUERADE

iptables -C FORWARD -i "${WLAN_IFACE}" -o "${WAN_IFACE}" -j ACCEPT 2>/dev/null \
    || iptables -A FORWARD -i "${WLAN_IFACE}" -o "${WAN_IFACE}" -j ACCEPT

iptables -C FORWARD -i "${WAN_IFACE}" -o "${WLAN_IFACE}" -m state --state RELATED,ESTABLISHED -j ACCEPT 2>/dev/null \
    || iptables -A FORWARD -i "${WAN_IFACE}" -o "${WLAN_IFACE}" -m state --state RELATED,ESTABLISHED -j ACCEPT
