#!/bin/bash
# Запуск КЛИЕНТА-НАБЛЮДАТЕЛЯ DayZ (appid 221100) под Proton, с автоподключением
# к локальному тест-серверу (~/dayz-cherno, порт 2302).
#
# Назначение: тестировщик запускает рабочий инстанс игры, чтобы ЧЕЛОВЕК (смотрящий
# на этом хосте, DISPLAY=:1) видел тест в реальном времени. Сценарий затем использует
# Op `observe` моста e2e, чтобы телепортировать/повернуть игрока к сцене.
#
# Использовать ТОЛЬКО когда нужно визуальное подтверждение или оркестратор явно сказал
# «с наблюдателем». По умолчанию e2e-тесты headless (без клиента).
#
# Первый прогон — проверить эмпирически:
#   - Steam должен/не должен быть запущен (steam_appid.txt=221100 в каталоге игры
#     позволяет запуск без Steam-клиента, но сетевая авторизация может требовать Steam);
#   - DayZ_x64.exe напрямую (сервер без BattlEye — должно хватить). Если BE мешает —
#     использовать DayZ_BE.exe (закомментированный вариант ниже) или -noBE на сервере;
#   - формат -connect: 127.0.0.1:2302 (при неудаче попробовать 127.0.0.1:2302:2303).
set -euo pipefail

# --- Steam / Proton runtime environment (как в tools/build.sh, но для игры) ---
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

# Мод клиенту нужен, чтобы совпасть с -mod=@Botorama на сервере. Берём ту же папку
# мода (Z:-путь, как в crash-логе клиента: Z:\...\DayZServer\@OneShot), без копии.
MOD='Z:\mnt\deep-space\Steam\steamapps\common\DayZServer\@Botorama'

echo "=== launching DayZ observer client (connect 127.0.0.1:2302) ==="
timeout 600 "$RT" --verb=waitforexitandrun -- \
    "$PROTON" waitforexitandrun \
    "$GAME/DayZ_x64.exe" \
    "-connect=127.0.0.1:2302" \
    "-name=e2e-observer" \
    "-mod=$MOD" \
    -window -noPause -nosplash -skipIntro

# Вариант через BattlEye-обёртку (если прямой запуск упрётся в BE):
# "$GAME/DayZ_BE.exe" "-connect=127.0.0.1:2302" "-name=e2e-observer" "-mod=$MOD" ...
