#!/usr/bin/env bash
# Reset the RAK2287 (SX1302 + two SX1250) concentrator on the RAK Pi HAT carrier through GPIO.
# The modem daemon runs it with no argument at start (-R option) and its service unit should run
# it with --hold at stop.
#
#   reset_concentrator.sh          reset, then release: chip runs afterwards (daemon start)
#   reset_concentrator.sh --hold   reset and leave the chip held in reset (daemon stop)
#
# The SX1302 reset input is ACTIVE HIGH: pin high = chip held in reset, pin low = chip runs.
# The reset line of the RAK2287 on the RAK Pi HAT is BCM GPIO 17 (RAK's rak_common_for_gateway
# lora/rak2287/reset_lgw.sh drives GPIO 17 low, high, low). Some HAT revisions route it to
# GPIO 25, so both lines are driven together, as the working units do; an unconnected line is
# unaffected. Semtech's CoreCell reference design uses GPIO 23 (sx1302_hal tools/reset_lgw.sh,
# high then low); that board is not this carrier.
#
# Needs the Raspberry Pi "pinctrl" tool and read/write access to /dev/gpiomem (group gpio), so
# it runs as the service user with no root. RESET_GPIO overrides the pin list (space separated
# BCM numbers), for example RESET_GPIO=17.
set -u

RESET_PINS="${RESET_GPIO:-17 25}"
HOLD=0
for arg in "$@"; do
    case "$arg" in
        --hold) HOLD=1 ;;
        *) echo "usage: $0 [--hold]" >&2; exit 2 ;;
    esac
done

for pin in $RESET_PINS; do
    case "$pin" in ''|*[!0-9]*) echo "ERROR: bad GPIO number '$pin' in RESET_GPIO" >&2; exit 2 ;; esac
done

command -v pinctrl >/dev/null 2>&1 || { echo "ERROR: pinctrl not found (Raspberry Pi OS package raspi-utils)" >&2; exit 1; }

drive() { # high|low
    local op=dh pin
    [ "$1" = low ] && op=dl
    for pin in $RESET_PINS; do
        pinctrl set "$pin" op "$op" || { echo "ERROR: pinctrl could not set GPIO $pin (is the user in group gpio?)" >&2; exit 1; }
    done
}

drive high
sleep 0.5
if [ "$HOLD" -eq 1 ]; then
    echo "Concentrator held in reset via pinctrl GPIO [${RESET_PINS}]"
else
    drive low
    sleep 0.5
    echo "Concentrator reset via pinctrl GPIO [${RESET_PINS}]"
fi
