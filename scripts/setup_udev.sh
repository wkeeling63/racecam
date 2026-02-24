#!/bin/bash
# CURRENT_USER=$(id -u -n)
CURRENT_USER=${SUDO_USER:-${USER}}
sudo groupadd -f racecam
sudo usermod -aG racecam $CURRENT_USER
echo "ACTION==\"add\", KERNEL==\"LED1\", SUBSYSTEM==\"leds\", RUN+=\"/bin/chown -R root:racecam /sys%p\", RUN+=\"/bin/chmod -R g+rw /sys%p\""  | sudo tee /etc/udev/rules.d/99-wpc-racecam-leds.rules > /dev/null

