#!/bin/bash
if (( EUID != 0 )); then
    echo "This script should be as root or with sudo. Exiting."
    exit 1
fi
mkdir -p /var/log/racecam/
mkdir -p /usr/local/etc/racecam/data/
chgrp racecam /var/log/racecam/
chgrp racecam /usr/local/etc/racecam/data/
chmod 775 /var/log/racecam/
chmod 775 /usr/local/etc/racecam/data/
