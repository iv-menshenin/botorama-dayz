#!/bin/bash
pkill -f DayZServer 2>/dev/null || true
sleep 2
nohup /home/devalio/dayz-cherno > /tmp/dayz_console.log 2>&1 &
echo "server started pid=$!"
