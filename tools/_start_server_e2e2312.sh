#!/bin/bash
# Запуск СВОЕГО e2e-сервера botorama на порту 2312 (изоляция от пользователя,
# у которого может работать свой DayZ на 2302). Профиль profiles-e2e2312,
# конфиг nasdaraDZ-e2e2312.cfg (instanceId=2), мод только @Botorama, headless.
# Останавливать ТОЛЬКО свой процесс (PID берётся через pgrep по -port=2312).
set -euo pipefail
cd /mnt/deep-space/Steam/steamapps/common/DayZServer/

# Чистим старые логи своего профиля (чтобы маркеры нового прогона читались однозначно).
rm -f profiles-e2e2312/*.RPT profiles-e2e2312/*.ADM profiles-e2e2312/*.mdmp profiles-e2e2312/error*.log 2>/dev/null || true

setsid nohup ./DayZServer -cpuCount=6 -config=./nasdaraDZ-e2e2312.cfg "-mod=@Botorama;" \
  -profiles=./profiles-e2e2312 -doLogs -adminlog -port=2312 \
  > /tmp/dayz_e2e_console.log 2>&1 < /dev/null & disown
echo "e2e server started, pid=$!"
