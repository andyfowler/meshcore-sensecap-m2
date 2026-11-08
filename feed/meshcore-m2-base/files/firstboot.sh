#!/bin/sh
# /usr/libexec/meshcore-m2/firstboot [--force]
#
# Applies /etc/config/meshcore once: Wi-Fi client setup, then renders the openHop
# repeater config (/etc/openhop_repeater/config.yaml) and seeds regions.
# Runs from /etc/uci-defaults on first boot; re-run by hand with --force.

. /lib/functions.sh

STAMP=/etc/meshcore-m2.applied
OH_DIR=/etc/openhop_repeater
LOG="logger -t meshcore-m2"

[ "$1" = "--force" ] || [ ! -e "$STAMP" ] || exit 0

config_load meshcore

# ---------------------------------------------------------------- Wi-Fi client
config_get_bool wifi_on wifi enabled 0
if [ "$wifi_on" = 1 ]; then
	config_get ssid wifi ssid
	config_get key wifi key
	config_get enc wifi encryption psk2
	config_get_bool disable_ap wifi disable_ap 1
	if [ -n "$ssid" ]; then
		$LOG "configuring Wi-Fi client for '$ssid'"
		uci -q batch <<-EOI
			set network.wwan=interface
			set network.wwan.proto='dhcp'
			delete network.wwan.ipaddr
			delete network.wwan.netmask
			set wireless.radio0.disabled='0'
			set wireless.meshcore_sta=wifi-iface
			set wireless.meshcore_sta.device='radio0'
			set wireless.meshcore_sta.network='wwan'
			set wireless.meshcore_sta.mode='sta'
			set wireless.meshcore_sta.ssid='$ssid'
			set wireless.meshcore_sta.encryption='$enc'
			set wireless.meshcore_sta.key='$key'
		EOI
		if [ "$disable_ap" = 1 ]; then
			# drop any AP interfaces (factory OpenWrt / ChirpStackAP)
			for s in $(uci -q show wireless | sed -n "s/^wireless\.\([^.]*\)\.mode='ap'$/\1/p"); do
				uci -q set wireless."$s".disabled='1'
			done
		fi
		# wwan must be in the wan zone (DHCP client on the house network)
		wan_zone=$(uci -q show firewall | sed -n "s/^firewall\.\([^.]*\)\.name='wan'$/\1/p" | head -1)
		[ -n "$wan_zone" ] && {
			uci -q del_list firewall."$wan_zone".network='wwan'
			uci -q add_list firewall."$wan_zone".network='wwan'
		}
		# remove wwan from the lan zone if a previous AP setup put it there
		lan_zone=$(uci -q show firewall | sed -n "s/^firewall\.\([^.]*\)\.name='lan'$/\1/p" | head -1)
		[ -n "$lan_zone" ] && uci -q del_list firewall."$lan_zone".network='wwan'
		uci commit network; uci commit wireless; uci commit firewall
	fi
fi

# ---------------------------------------------------------- openHop repeater config
mkdir -p "$OH_DIR"
config_get admin node admin_password
config_get rootpw node root_password
[ -n "$rootpw" ] || rootpw="$admin"
if [ -n "$rootpw" ]; then
	printf '%s\n%s\n' "$rootpw" "$rootpw" | passwd root >/dev/null 2>&1 && $LOG "root password set from site config"
	/usr/libexec/meshcore-m2/ssh-access unlock
else
	# no password yet: keep SSH password logins off until the setup wizard sets one
	/usr/libexec/meshcore-m2/ssh-access lock
fi

/usr/bin/python3 /usr/libexec/meshcore-m2/render_config.py \
	--template /usr/share/openhop_repeater/config.yaml.example \
	--out "$OH_DIR/config.yaml" || { $LOG "config render failed"; exit 1; }
chmod 600 "$OH_DIR/config.yaml"

# open the companion TCP port to the house network (wan zone on this device)
config_get_bool comp_on companion enabled 0
if [ "$comp_on" = 1 ]; then
	config_get cport companion tcp_port 5000
	if ! uci -q show firewall | grep -q "name='Allow-MeshCore-companion'"; then
		uci -q batch <<-EOI
			add firewall rule
			set firewall.@rule[-1].name='Allow-MeshCore-companion'
			set firewall.@rule[-1].src='wan'
			set firewall.@rule[-1].dest_port='$cport'
			set firewall.@rule[-1].proto='tcp'
			set firewall.@rule[-1].target='ACCEPT'
		EOI
		uci commit firewall
	fi
fi

touch "$STAMP"
$LOG "site configuration applied"
exit 0
