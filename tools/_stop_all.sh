#!/bin/bash
pkill -f DayZServer 2>/dev/null || true
pkill -f DayZ_x64.exe 2>/dev/null || true
pkill -f "timeout 600.*DayZ_x64" 2>/dev/null || true
sleep 3
echo "DayZServer: $(pgrep -f DayZServer | wc -l) procs"
echo "DayZ_x64: $(pgrep -f DayZ_x64.exe | wc -l) procs"
