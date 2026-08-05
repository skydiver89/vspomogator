#!/bin/bash
systemctl stop vspomogator.service
systemctl disable vspomogator.service
systemctl daemon-reload
exit 0
