#!/bin/bash
# Сборка всех модулей botorama в подписанные PBO (мульти-PBO структура).
#
# Цепочка запуска AddonBuilder под Proton (восстановлена по ps/environ из Steam):
#   SteamLinuxRuntime_4/_v2-entry-point --verb=waitforexitandrun -- \
#     'Proton Hotfix'/proton waitforexitandrun '<exe>' <args>
#
# Для каждого модуля src/{cons,reg,core,map,loadout,test}:
#   AddonBuilder <src> <build_dir> -prefix=dm_<модуль> -clear \
#     -include=tools/include.lst -sign=<devalio.biprivatekey>
# → build/<модуль>.pbo + build/<модуль>.pbo.devalio.bisign
#
# Зависимости: Steam-клиент, DayZ Tools (appid 830640), Proton Hotfix,
# SteamLinuxRuntime_4. Запускать, когда DayZ Tools НЕ открыт в Steam
# (wineserver префикса должен быть свободен).
set -euo pipefail

# --- Steam / Proton runtime environment ---
export STEAM_COMPAT_CLIENT_INSTALL_PATH=/home/devalio/.local/share/Steam
export STEAM_COMPAT_DATA_PATH=/mnt/deep-space/Steam/steamapps/compatdata/830640
export STEAM_COMPAT_INSTALL_PATH="/mnt/deep-space/Steam/steamapps/common/DayZ Tools"
export STEAM_COMPAT_LIBRARY_PATHS="/home/devalio/.local/share/Steam/steamapps:/mnt/deep-space/Steam/steamapps"
export STEAM_COMPAT_MOUNTS="/home/devalio/.local/share/Steam/steamapps/common/Steamworks Shared:/home/devalio/.local/share/Steam/steamapps/common/Proton Hotfix:/home/devalio/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4"
export STEAM_COMPAT_TOOL_PATHS="/home/devalio/.local/share/Steam/steamapps/common/Proton Hotfix:/home/devalio/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4"
export SteamAppId=830640
export SteamGameId=830640

RT="/home/devalio/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4/_v2-entry-point"
PROTON="/home/devalio/.local/share/Steam/steamapps/common/Proton Hotfix/proton"
AB="/mnt/deep-space/Steam/steamapps/common/DayZ Tools/Bin/AddonBuilder/AddonBuilder.exe"

ROOT="/home/devalio/dayz/Work/botorama"
OUT="$ROOT/build"
SIGN_KEY='Z:\home\devalio\dayz\Work\Keys\devalio.biprivatekey'
INC='Z:\home\devalio\dayz\Work\botorama\tools\include.lst'

MODULES="cons reg core map loadout test"

rm -rf "$OUT"
mkdir -p "$OUT"

for m in $MODULES; do
    echo "=== building $m ==="
    SRC="Z:\\home\\devalio\\dayz\\Work\\botorama\\src\\$m"
    DST="Z:\\home\\devalio\\dayz\\Work\\botorama\\build"
    timeout 300 "$RT" --verb=waitforexitandrun -- \
        "$PROTON" waitforexitandrun \
        "$AB" "$SRC" "$DST" -prefix="dm_$m" -clear -include="$INC" -sign="$SIGN_KEY" \
        > "/tmp/ab_$m.log" 2>&1
    echo "  exit=$?"
done

echo "=== results ==="
ls -la "$OUT"
