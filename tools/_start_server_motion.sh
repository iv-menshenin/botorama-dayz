#!/bin/bash
# Запуск СВОЕГО e2e-сервера botorama для кейса motion-trouble (бот на крыше) на
# порту 2313, профиль profiles-motion, конфиг nasdaraDZ-motion.cfg (instanceId=3).
# Мод только @Botorama, headless. Останавливать ТОЛЬКО свой процесс (PID через
# pgrep по -port=2313).
set -euo pipefail
cd /mnt/deep-space/Steam/steamapps/common/DayZServer/

# Чистим старые логи своего профиля (чтобы маркеры нового прогона читались однозначно).
rm -f profiles-motion/*.RPT profiles-motion/*.ADM profiles-motion/*.mdmp profiles-motion/error*.log 2>/dev/null || true

setsid nohup ./DayZServer -cpuCount=6 -config=./nasdaraDZ-motion.cfg "-mod=@Botorama;" \
  -profiles=./profiles-motion -doLogs -adminlog -port=2313 \
  > /tmp/dayz_motion_console.log 2>&1 < /dev/null & disown
echo "motion e2e server started (port 2313), pid=$!"
