#!/bin/bash
# Observer launch with a FRESH wine prefix (avoids devalio's launcher holding prefix 221100).
set -euo pipefail
export STEAM_COMPAT_CLIENT_INSTALL_PATH=/home/devalio/.local/share/Steam
export STEAM_COMPAT_DATA_PATH=/tmp/opencode/dayz-obs-pfx
export STEAM_COMPAT_INSTALL_PATH="/home/devalio/.local/share/Steam/steamapps/common/DayZ"
export STEAM_COMPAT_LIBRARY_PATHS="/home/devalio/.local/share/Steam/steamapps:/mnt/deep-space/Steam/steamapps"
export STEAM_COMPAT_MOUNTS="/home/devalio/.local/share/Steam/steamapps/common/Steamworks Shared:/home/devalio/.local/share/Steam/steamapps/common/Proton Hotfix:/home/devalio/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4"
export STEAM_COMPAT_TOOL_PATHS="/home/devalio/.local/share/Steam/steamapps/common/Proton Hotfix:/home/devalio/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4"
export SteamAppId=221100
export SteamGameId=221100
RT="/home/devalio/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4/_v2-entry-point"
PROTON="/home/devalio/.local/share/Steam/steamapps/common/Proton Hotfix/proton"
GAME="/home/devalio/.local/share/Steam/steamapps/common/DayZ"
MOD='Z:\mnt\deep-space\Steam\steamapps\common\DayZServer\@Botorama'
rm -rf /tmp/opencode/dayz-obs-pfx
mkdir -p /tmp/opencode/dayz-obs-pfx
timeout 600 "$RT" --verb=waitforexitandrun -- \
    "$PROTON" waitforexitandrun \
    "$GAME/DayZ_x64.exe" \
    "-connect=127.0.0.1:2302" \
    "-name=e2e-observer" \
    "-mod=$MOD" \
    -window -noPause -nosplash -skipIntro
