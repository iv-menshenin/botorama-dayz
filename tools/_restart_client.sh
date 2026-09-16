#!/bin/bash
pkill -f DayZ_x64.exe 2>/dev/null || true
pkill -f "timeout 600.*DayZ_x64" 2>/dev/null || true
sleep 3
cd /home/devalio/dayz/Work/botorama
nohup bash tools/run-client.sh > /home/devalio/dayz/Work/botorama/build/client_console2.log 2>&1 &
echo "client restarted pid=$!"
