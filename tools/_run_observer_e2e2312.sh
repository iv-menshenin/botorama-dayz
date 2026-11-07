#!/bin/bash
# Observer launch for the E2E test server on PORT 2312 with a FRESH wine prefix.
# Why fresh: the human's live DayZLauncher.exe holds prefix 221100 (pfx.lock),
# so run-client.sh (compatdata 221100) would conflict with its wineserver and
# kill the user's session. This script mirrors run-client.sh (mod @Botorama ONLY,
# nasdara is built-in terrain) but under a private prefix + port 2312.
set -euo pipefail

export STEAM_COMPAT_CLIENT_INSTALL_PATH=/home/devalio/.local/share/Steam
export STEAM_COMPAT_DATA_PATH=/tmp/opencode/dayz-obs-pfx-2312
export STEAM_COMPAT_INSTALL_PATH="/home/devalio/.local/share/Steam/steamapps/common/DayZ"
export STEAM_COMPAT_LIBRARY_PATHS="/home/devalio/.local/share/Steam/steamapps:/mnt/deep-space/Steam/steamapps"
export STEAM_COMPAT_MOUNTS="/home/devalio/.local/share/Steam/steamapps/common/Steamworks Shared:/home/devalio/.local/share/Steam/steamapps/common/Proton Hotfix:/home/devalio/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4"
export STEAM_COMPAT_TOOL_PATHS="/home/devalio/.local/share/Steam/steamapps/common/Proton Hotfix:/home/devalio/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4"
export SteamAppId=221100
export SteamGameId=221100

RT="/home/devalio/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4/_v2-entry-point"
PROTON="/home/devalio/.local/share/Steam/steamapps/common/Proton Hotfix/proton"
GAME="/home/devalio/.local/share/Steam/steamapps/common/DayZ"

# Мод клиенту нужен ТОЛЬКО @Botorama (nasdara — встроенные файлы игры, НЕ в -mod=).
# Тот же -mod=@Botorama, что и на сервере 2312 (см. docs/ai-testing-guide.md §6).
MOD='Z:\mnt\deep-space\Steam\steamapps\common\DayZServer\@Botorama'

PORT="${PORT:-2312}"

rm -rf "$STEAM_COMPAT_DATA_PATH"
mkdir -p "$STEAM_COMPAT_DATA_PATH"

echo "=== launching DayZ observer (fresh pfx, connect 127.0.0.1:$PORT) ==="
# 1500s (25 min): fresh-prefix client может грузиться до 15 мин (Fossilize/стриминг мира).
timeout 1500 "$RT" --verb=waitforexitandrun -- \
    "$PROTON" waitforexitandrun \
    "$GAME/DayZ_x64.exe" \
    "-connect=127.0.0.1:$PORT" \
    "-name=e2e-observer" \
    "-mod=$MOD" \
    -window -noPause -nosplash -skipIntro
