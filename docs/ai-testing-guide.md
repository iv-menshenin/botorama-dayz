# Руководство ИИ-агента: E2E-тестирование botorama перед фиксацией

Назначение — операционный ранбук **для ИИ-агента** (оркестратора). Перед коммитом
результата работы агент обязан пройти полный цикл: собрать мод → задеплоить →
запустить сервер → прогнать проверку → сверить логи → и только потом фиксировать.

В отличие от `docs/tester-guide.md` (чат-команды для человека-тестировщика), здесь
описано, как агент **сам** выполняет цикл на Linux-хосте, без Steam-GUI и без игрока.

## Содержание

1. [Сборка мода](#1-сборка-мода)
2. [Деплой](#2-деплой)
3. [Запуск сервера](#3-запуск-сервера)
4. [Чтение логов](#4-чтение-логов)
5. [Автотесты (файловый мост)](#5-автотесты-файловый-мост)
6. [Клиент-наблюдатель (опционально)](#6-клиент-наблюдатель-опционально-только-по-запросу)

---

## 1. Сборка мода

Мод собран из 6 PBO (`cons`, `reg`, `core`, `map`, `loadout`, `test`) в `src/`.
Сборка идёт Windows-тулзами DayZ Tools под Proton — headless, без открытия Steam-GUI.

### 1.1 Тулчейн

Всё лежит в Steam-библиотеках:

| Что | Путь |
|---|---|
| DayZ Tools | `/mnt/deep-space/Steam/steamapps/common/DayZ Tools` (appid `830640`) |
| AddonBuilder | `…/DayZ Tools/Bin/AddonBuilder/AddonBuilder.exe` |
| CfgConvert | `…/DayZ Tools/Bin/CfgConvert/CfgConvert.exe` (вызывается AddonBuilder'ом) |
| DSSignFile | `…/DayZ Tools/Bin/DsUtils/DSSignFile.exe` (вызывается AddonBuilder'ом) |
| Proton | `/home/devalio/.local/share/Steam/steamapps/common/Proton Hotfix` |
| Рантайм | `/home/devalio/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4` |
| Wine-префикс | `/mnt/deep-space/Steam/steamapps/compatdata/830640/pfx` |
| Ключ подписи | `/home/devalio/dayz/Work/Keys/devalio.biprivatekey` (+ `devalio.bikey`) |

### 1.2 Цепочка запуска (восстановлена из живой Steam-сессии)

```
SteamLinuxRuntime_4/_v2-entry-point --verb=waitforexitandrun -- \
  'Proton Hotfix'/proton waitforexitandrun '<exe>' <args>
```

Окружение, которое поднимает Steam (полный набор — в `tools/build.sh`):

```
STEAM_COMPAT_DATA_PATH=/mnt/deep-space/Steam/steamapps/compatdata/830640
STEAM_COMPAT_CLIENT_INSTALL_PATH=/home/devalio/.local/share/Steam
STEAM_COMPAT_INSTALL_PATH="/mnt/deep-space/Steam/steamapps/common/DayZ Tools"
STEAM_COMPAT_LIBRARY_PATHS=/home/devalio/.local/share/Steam/steamapps:/mnt/deep-space/Steam/steamapps
STEAM_COMPAT_TOOL_PATHS=…/Proton Hotfix:…/SteamLinuxRuntime_4
SteamAppId=830640
```

### 1.3 CLI AddonBuilder

```
AddonBuilder.exe <source_dir> <dest_dir> [-prefix=<prefix>] [-clear]
                 [-include=<файл>] [-sign=<biprivatekey>]
```

- `<dest_dir>` — **каталог**, имя `.pbo` берётся из имени папки-исходника
  (для `src/cons` → `build/cons.pbo`). НЕ передавать имя файла.
- `-include` — include-список расширений (обязателен, см. ниже).
- `-sign` — ключ подписи (`.biprivatekey`); AddonBuilder сам вызывает DSSignFile.

### 1.4 Include-список

Файл `tools/include.lst` (semicolon-разделитель, с пробелами):

```
*.emat; *.edds; *.ptc; *.c; *.imageset; *.layout; *.ogg; *.agr; *.csv
```

**Обязателен**: без него полный билд (не `-packonly`) выкидывает `.c`-скрипты,
и остаётся только `config.bin`.

### 1.5 Запуск сборки

```
bash tools/build.sh
```

Собирает 6 модулей → `build/<модуль>.pbo` + `build/<модуль>.pbo.devalio.bisign`.

Перед сборкой `build.sh` очищает Temp wine-префикса
(`compatdata/830640/pfx/drive_c/users/steamuser/AppData/Local/Temp`) — AddonBuilder
чистит его сам, но медленно; ручная очистка ускоряет компиляцию и надёжнее.

### 1.6 Готчи сборки

- **Рантайм — только `SteamLinuxRuntime_4`.** `SteamLinuxRuntime_sniper`/`soldier`
  дают glibc 2.36, а Proton 11 требует ≥2.38 → `could not load ntdll.so:
  GLIBC_2.38 not found`. Правильная цепочка снимает проблему.
- **`-packonly` НЕ подходит** — он не делает `config.cpp`→`config.bin`.
  Нужен полный билд (binarize) + `-include` (иначе `.c` пропадают).
- **Include-файл — semicolon-разделитель**, не «одно расширение на строку»
  (переводы строк ломают разбор — билд молча ничего не производит).
- **Wineserver префикса один на процесс.** Пока в Steam открыт DayZ Tools
  (AddonBuilder/лаунчер), второй headless-запуск зависает на `wineserver -w`.
  Перед сборкой закрыть DayZ Tools (или убить его процессы — разрешено).
- Дополнительно проверить: `strings -a <pbo> | grep config.bin` — в каждом PBO
  должен быть `config.bin`, а в `core.pbo` — ещё `.agr` и `.ogg`.

---

## 2. Деплой

Скопировать 6×`.pbo` + 6×`.bisign` в `@Botorama/Addons`:

```
AD=/mnt/deep-space/Steam/steamapps/common/DayZServer/@Botorama/Addons
cp build/*.pbo build/*.bisign "$AD/"
```

Старый одиночный `botorama.pbo` уже удалён (заменён на мульти-PBO). Ключ
`devalio.bikey` лежит в `@Botorama/Keys/` и в `DayZServer/keys/` — сервер с
`verifySignatures=2` принимает подпись.

---

## 3. Запуск сервера

Скрипт пользователя: `~/dayz-cherno` (запускается из-под `nohup`, т.к. работает
в foreground):

```
nohup ~/dayz-cherno > /tmp/dayz_console.log 2>&1 &
```

Под капотом (скрипт чистит старые логи и стартует):

```
cd /mnt/deep-space/Steam/steamapps/common/DayZServer/
rm -rf ./profiles-cherno/*.log *.ADM *.RPT *.mdmp
./DayZServer -cpuCount="6" -config=./chernoDZ.cfg -profiles=./profiles-cherno \
  -mod=@Botorama; -doLogs -adminlog -port=2302
```

- Конфиг: `chernoDZ.cfg` (карта Чернорусь), профиль `profiles-cherno`.
- Рестарт: `kill <pid>` → `~/dayz-cherno` (сервер перечитывает новые PBO только
  при рестарте — hot-reload отсутствует).
- **Остановка (обязательно после прогона)**: `pkill -f DayZServer` + проверить
  `pgrep -f DayZServer` == пусто. Сервер не оставлять работающим после тестов.
  Если запускал клиент-наблюдателя — `pkill -f DayZ_x64.exe`.

---

## 4. Чтение логов

RPT-лог сервера — `DayZServer/profiles-cherno/DayZServer_<дата>.RPT` (последний —
по `ls -t`). Ключевые маркеры:

| Что ищем | Пример | Значение |
|---|---|---|
| Загрузка PBO | `Adding package '…@Botorama/Addons/<модуль>.pbo'` | все 6 PBO загрузились |
| Defines | `…dmBotorama_Cons,dmBotorama_Reg,dmBotorama_Core…` | цепочка `requiredAddons` встала |
| Версия мода | `[dmBot] Botorama initialized: 3.160` | `dmBotLog.LogVersion` (совпадает с `DM_BOTORAMA_VERSION`) |
| Ошибки скриптов | `SCRIPT (E)` / `[dmBot][error]` | реальные проблемы (не `(W)` warning'и) |
| Краш | `crash_*.log`, `error.log`, `*.mdmp` | смотреть stack |

Известные не-проблемы (были и до рефакторинга, игнорировать):
- `ANIMATION (E): Can't load @Botorama/Anims/cfg/skeletons.anim.xml` — фолбэк на
  ванильный скелет `DZ/Anims/cfg/skeletons.anim.xml`, боты работают.
- `[POIRegistry] read error …/world_poi.json` — нет данных карты в `$profile`
  (к `data/map/`, не к модулям).
- `No entry 'bin\config.bin/CfgVehicles/enfAnimSys.*'` — штатные config-варнинги
  наследования; `STR_DN_* not found` — ванильные локализационные варнинги.

---

## 5. Автотесты (файловый мост)

Автотесты управляются **файловым мостом** — без игрока и без HTTP-сервиса. Агент
кладёт JSON-сценарий, мод исполняет его на сервере, агент читает JSON-результат и
tail'ит RPT. Протокол, словарь команд и схема результата — `docs/plans/e2e-automation.md`;
каталог регрессионных сценариев — `tools/e2e/`.

Цикл (см. §5.1–5.3): собрать → задеплоить → старт → сценарий → результат → фиксация.

### 5.1 Выбор defines при сборке

`tools/build.sh` принимает флаг, задающий набор `defines[]` (debug-домены логирования):

| Режим | Флаг | defines[] |
|---|---|---|
| **Прод** | `--prod` | только `DM_BOT_PROFILE` |
| **Функциональный** (дефолт) | *(без флага)* | как закоммитил `dayz-dev` (релевантные домены фичи) |
| **Регрессионный** | `--test` | все event-домены из `tools/defines_test.txt` |

Регрессию тестировщик гоняет с `--test` (полное покрытие event-доменов); per-frame
домены (`DM_BOT_TRACE_LOOK`, `DM_BOT_DEBUG_PERFRAME_MOVING_LOG`) в `--test` НЕ входят
(спам) — их включает `dayz-dev` точечно, если конкретный сценарий их проверяет.

### 5.2 Запуск сценария

```bash
# маркер уже лежит (см. e2e-automation.md)
cat > "$PROFILE/dmBotorama/e2e/in/<job>.json" <<'EOF'   # атомарно: .tmp → rename
{ "Name": "<job>", "Timeout": 60, "Steps": [ ... ] }
EOF
# дождаться out/<job>.result.json; при провале — tail RPT
```

Результат: `out/<job>.result.json` (`Status`: `ok`/`error`/`timeout`, `Steps[].Ok`,
`Snapshot[]`). `bool` в JSON = `1`/`0`.

### 5.3 RPT-канал моста

Жизненный цикл моста пишется в RPT под доменом `DM_BOT_DEBUG_E2E` (входит в `--test`):
`enabled`, `job X picked up`, `step N: op=…`, `job X → status=…`, ошибки.

## 6. Клиент-наблюдатель (опционально, только по запросу)

Для визуального подтверждения человек на этом хосте может смотреть тест в реальном
времени: тестировщик запускает рабочий клиент DayZ, который автоподключается к
локальному серверу, а сценарий телепортирует/поворачивает игрока к сцене.

> **Правило**: наблюдатель нужен ТОЛЬКО когда (а) оркестратор/человек явно сказал
> «с наблюдателем», или (б) тест требует визуального подтверждения. По умолчанию
> e2e-тесты **headless** — без клиента.

- Запуск клиента: `bash tools/run-client.sh` (Proton, appid 221100, `-connect=127.0.0.1:2302`,
  `-name=e2e-observer`, `-mod=Z:\...\DayZServer\@Botorama`, `-window -noPause -nosplash -skipIntro`).
- **Ожидание входа наблюдателя — ПОЛЛИНГОМ ЛОГА, не фиксированным sleep.** Игрок заходит
  за ~20–30 с. Вместо `sleep 60` гоняй цикл: grep свежего RPT на `Player "e2e-observer"`
  (или `is connected`) раз в ~2 с, таймаут ~120 с; как только строка появилась — наблюдатель
  в игре, можно дропать сценарий. Аналогично сервер: ждать по RPT `Botorama initialized`,
  а не по таймеру. Это режет пустое ожидание «минута+» до реального времени входа.
- Op `observe` (в сценарии): телепорт **первого** подключённого игрока
  (`Pos` + `Yaw`) — `SetPosition(SnapToGroundExactly)` + `SetOrientation(Vector(Yaw,0,0))`.
  Если игрок ещё не зашёл — `Reason="no player connected"`, повторить после подключения.
- Первый прогон — проверить эмпирически: нужен ли запущенный Steam, `DayZ_x64.exe`
  напрямую или `DayZ_BE.exe`, формат `-connect` (`:2302` vs `:2302:2303`).
