#!/bin/bash
# Сборка всех модулей botorama в подписанные PBO (мульти-PBO структура).
#
# Цепочка запуска AddonBuilder под Proton (восстановлена по ps/environ из Steam):
#   SteamLinuxRuntime_4/_v2-entry-point --verb=waitforexitandrun -- \
#     'Proton Hotfix'/proton waitforexitandrun '<exe>' <args>
#
# Для каждого модуля src/{cons,reg,core,map,loadout,roads,test}:
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

# --- Очистить Temp префикса перед сборкой (быстрее + надёжнее) ---
# AddonBuilder и сам чистит Temp, но делает это очень медленно; ручная очистка
# содержимого каталога ускоряет компиляцию и снижает гонки старых артефактов.
# Удаляем только содержимое, не сам каталог (wine-префикс должен остаться валидным).
TMP="/mnt/deep-space/Steam/steamapps/compatdata/830640/pfx/drive_c/users/steamuser/AppData/Local/Temp"
if [ -d "$TMP" ]; then
    find "$TMP" -mindepth 1 -delete 2>/dev/null || true
fi

MODULES="cons reg core map loadout roads test"

# --- Режим defines: --prod | --test | --define | (default, functional) ---
#   --prod             -> только DM_BOT_PROFILE (минимальный прод-билд)
#   --test             -> регрессионный: все event-домены из tools/defines_test.txt
#   --define DOM1,DOM2 -> DM_BOT_PROFILE + свои домены (через запятую)
#   (без флага)        -> функциональный: defines[] как закоммитил dayz-dev
MODE="func"
CUSTOM_DEFINES=""
case "${1:-}" in
    --prod) MODE="prod" ;;
    --test) MODE="test" ;;
    --define) MODE="define"; CUSTOM_DEFINES="${2:-}" ;;
    ""|--) MODE="func" ;;
    *) echo "usage: build.sh [--prod|--test|--define DOM1,DOM2]  (default: functional, keeps committed defines)" >&2; exit 2 ;;
esac

# Отклонить молча игнорируемые лишние аргументы. Footgun: `--prod --define DM_BOT_DEBUG_CAR`
# берёт $1=--prod и ТИХО отбрасывает `--define DM_BOT_DEBUG_CAR` (собирается только
# DM_BOT_PROFILE, нужный DEBUG-домен пропадает). Только `--define` легитимно берёт
# второй токен (список доменов через запятую); всё остальное — ошибка.
if [ $# -gt 0 ]; then shift; fi
if [ "$MODE" = "define" ] && [ $# -gt 0 ]; then shift; fi
if [ $# -gt 0 ]; then
    echo "build.sh: unexpected argument(s): $*" >&2
    echo "usage: build.sh [--prod|--test|--define DOM1,DOM2]" >&2
    exit 2
fi

# Модули, у которых есть defines[] (gated call sites; reg — без defines).
DEFINES_FILES="cons core map loadout roads test"

build_defines_list() {
    if [ "$MODE" = "prod" ]; then
        echo '"DM_BOT_PROFILE"'
    elif [ "$MODE" = "test" ]; then
        sed -e 's/#.*$//' -e '/^[[:space:]]*$/d' "$ROOT/tools/defines_test.txt" \
            | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' \
            | sed 's/^/"/; s/$/"/' \
            | paste -sd ',' - | sed 's/,/, /g'
    elif [ "$MODE" = "define" ]; then
        local defs='"DM_BOT_PROFILE"'
        if [ -n "$CUSTOM_DEFINES" ]; then
            local extra
            extra=$(printf '%s' "$CUSTOM_DEFINES" | tr ',' '\n' \
                | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' -e '/^$/d' \
                      -e 's/^/"/' -e 's/$/"/' \
                | paste -sd ',' - | sed 's/,/, /g')
            if [ -n "$extra" ]; then
                defs="$defs, $extra"
            fi
        fi
        echo "$defs"
    fi
}

patch_defines() {
    local defines="$1"
    for m in $DEFINES_FILES; do
        local cfg="$ROOT/src/$m/config.cpp"
        cp "$cfg" "/tmp/build_defines_backup_$m.cpp"
        sed -i -E "s|^([[:space:]]*)defines\[\][[:space:]]*=.*$|\1defines[] = { $defines };|" "$cfg"
    done
}

restore_defines() {
    for m in $DEFINES_FILES; do
        local cfg="$ROOT/src/$m/config.cpp"
        if [ -f "/tmp/build_defines_backup_$m.cpp" ]; then
            cp "/tmp/build_defines_backup_$m.cpp" "$cfg"
            rm -f "/tmp/build_defines_backup_$m.cpp"
        fi
    done
}

if [ "$MODE" != "func" ]; then
    DEFINES=$(build_defines_list)
    patch_defines "$DEFINES"
    trap restore_defines EXIT
fi

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
    #! Proton's `waitforexitandrun` does not propagate AddonBuilder's real exit
    #! code (it can return 0 even when no PBO was produced — seen as a silent
    #! flake). Verify the artifacts actually landed; otherwise fail loudly.
    if [ ! -f "$OUT/$m.pbo" ] || [ ! -f "$OUT/$m.pbo.devalio.bisign" ]; then
        echo "BUILD FAILED: $m.pbo (and/or bisign) missing after AddonBuilder (see /tmp/ab_$m.log)" >&2
        exit 1
    fi
done

echo "=== results ==="
ls -la "$OUT"
