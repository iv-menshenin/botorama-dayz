#!/bin/bash
pkill -f DayZServer 2>/dev/null || true
# Только локального e2e-наблюдателя (он коннектится как -name=e2e-observer).
# НЕ трогать произвольный DayZ_x64.exe — это может быть живой клиент человека.
pkill -f "DayZ_x64.*e2e-observer" 2>/dev/null || true
pkill -f "timeout 600.*DayZ_x64.*e2e-observer" 2>/dev/null || true
sleep 3
echo "DayZServer: $(pgrep -f DayZServer | wc -l) procs"
echo "e2e-observer: $(pgrep -f 'DayZ_x64.*e2e-observer' | wc -l) procs"
