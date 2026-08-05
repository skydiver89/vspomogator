#!/bin/bash
sed -i "s/User=root/User=$SUDO_USER/g" /etc/systemd/system/vspomogator.service
systemctl daemon-reload
systemctl enable vspomogator
systemctl start vspomogator
exit 0
