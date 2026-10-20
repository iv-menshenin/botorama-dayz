#!/bin/bash
# Клиент-наблюдатель DayZ для карты NASDARA (appid 221100) под Proton.
# Адаптация tools/run-client.sh: -mod включает карту Nasdara (клиентская копия
# в DayZ/nasdara/) + @Botorama (серверная копия, Z:-путь). Подключается к
# локальному тест-серверу Nasdara (порт 2302), имя игрока e2e-observer.
set -euo pipefail

export STEAM_COMPAT_CLIENT_INSTALL_PATH=/home/devalio/.local/share/Steam
export STEAM_COMPAT_DATA_PATH=/home/devalio/.local/share/Steam/steamapps/compatdata/221100
export STEAM_COMPAT_INSTALL_PATH="/home/devalio/.local/share/Steam/steamapps/common/DayZ"
export STEAM_COMPAT_LIBRARY_PATHS="/home/devalio/.local/share/Steam/steamapps:/mnt/deep-space/Steam/steamapps"
export STEAM_COMPAT_MOUNTS="/home/devalio/.local/share/Steam/steamapps/common/Steamworks Shared:/home/devalio/.local/share/Steam/steamapps/common/Proton Hotfix:/home/devalio/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4"
export STEAM_COMPAT_TOOL_PATHS="/home/devalio/.local/share/Steam/steamapps/common/Proton Hotfix:/home/devalio/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4"
export SteamAppId=221100
export SteamGameId=221100

RT="/home/devalio/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4/_v2-entry-point"
PROTON="/home/devalio/.local/share/Steam/steamapps/common/Proton Hotfix/proton"
GAME="/home/devalio/.local/share/Steam/steamapps/common/DayZ"

# Nasdara (клиентская копия карты) + @Botorama (серверная копия мода).
MOD='Z:\home\devalio\.local\share\Steam\steamapps\common\DayZ\nasdara;Z:\mnt\deep-space\Steam\steamapps\common\DayZServer\@Botorama'

echo "=== launching DayZ NASDARA observer client (connect 127.0.0.1:2302) ==="
"$RT" --verb=waitforexitandrun -- \
    "$PROTON" waitforexitandrun \
    "$GAME/DayZ_x64.exe" \
    "-connect=127.0.0.1:2302" \
    "-name=e2e-observer" \
    "-mod=$MOD" \
    -window -noPause -nosplash -skipIntro
