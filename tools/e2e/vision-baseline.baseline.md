# vision-baseline — BASELINE текущего зрения ДО реворта `dmVision`

> Прогон: `dayz-tester`, дата Mon Nov 09 2026, ветка `feature/vision-rework`.
> Сборка: **v3.506** (`DM_BOTORAMA_VERSION`), `bash tools/build.sh --test`
> (defines содержат `DM_BOT_DEBUG_VISION`, `DM_PERCEPTION_DEBUG`, `DM_BOT_DEBUG_E2E` —
> подтверждено в RPT).
> RPT: `profiles-e2e2312/DayZServer_2026-11-09_14-56-33.RPT`.
> Сервер: **свой порт 2312**, профиль `profiles-e2e2312`, конфиг `nasdaraDZ-e2e2312.cfg`
> (миссия `dayzOffline.nasdara`), мод только `@Botorama`, headless (без наблюдателя).
> Чужой сервер пользователя (port **2302**, PID 208591) НЕ тронут и остался живым.

Цель — зафиксировать наблюдаемое поведение ТЕКУЩЕГО зрения как эталон: **бот видит
неподвижного игрока на ~20 м днём → `HasLOS=true`, `EverSeen=true`**. После каждого
атома реворта `dmVision` (освещённость + 4 проба-точки + вероятность) сверять с цифрами ниже.

---

## Сценарий — `tools/e2e/vision-baseline.json` (НОВЫЙ, минимальный)

```
ping
spawn  "bot"    [4117,0,10827] Yaw=0   (без пресета = без FSM, стоит на месте)
spawn  "player" [4117,0,10807] Yaw=180 (без пресета = без FSM, стоит на месте; 20 м по -Z)
look   "bot" -> "player" Turn=full     (полный доворот корпуса к цели)
sleep  8
targetdump "bot"
botdump    "bot"
snapshot
clearall
```

Локация — `ballistics-range.ref` `[4117,10827]` (известное открытое поле). Серверное время —
**день** (`serverTime="2015/4/8/10/13"`, `serverTimePersistent=0` → всегда 10:13, светло).

Результат: **`Status: ok`**, все шаги `Ok=1`. Job window: 14:57:18 → 14:57:42 (~24 с).

---

## Что увидел в дампе целей (`targetdump`)

```
target[2] type=dmAI_SurvivorM_Denis cls=dmTargetPlayer
          pos=<4116.978516, 281.119690, 10806.968750>
          LOS=1  everSeen=1  threat=0.6  attraction=0  cooldown=0
```

| Поле | Значение | Расшифровка |
|---|---|---|
| `type` | `dmAI_SurvivorM_Denis` | целевой «игрок» (бот-пешка, модель Denis) |
| `cls` | `dmTargetPlayer` | опознан как игрок |
| `pos` | `<4116.98, 281.12, 10806.97>` | живая позиция цели (голова, ~1.8 м над землёй) |
| **`LOS`** | **`1`** | **цель видна в последнем скане (`m_HasLOS=true`)** ✓ |
| **`everSeen`** | **`1`** | **бот хоть раз видел цель (`m_EverSeen=true`)** ✓ |
| `threat` | `0.6` | = `DM_TARGET_THREAT_PLAYER` (≥0.5 → high-threat refresh 0.2 с) |

Соседние строки дампа (для контекста — фоновые животные карты):
```
target[0] type=Animal_SusScrofa cls=dmTargetAnimal pos=<4205.65,…> LOS=0 everSeen=1 threat=0.2
target[1] type=Animal_SusScrofa cls=dmTargetAnimal pos=<4203.65,…> LOS=0 everSeen=1 threat=0.2
```
(два кабана, которых бот видел раньше — `everSeen=1`, но сейчас отвернулся → `LOS=0`).

## Снапшот (позиции/неподвижность)

```
bot    Alive=1 Pos=[4117.00, 278.47, 10827.00] Moving=0
player Alive=1 Pos=[4117.00, 279.51, 10807.00] Moving=0
```

- Дистанция по Z: `10827 → 10807` = **20.0 м**. Оба неподвижны (`Moving=0`), живы.
- `botdump` наблюдателя: `orient=<179.055,0,0>` (корпус довернулся на север ≈180°,
  к цели на `-Z`), `vel=<0,0,0>`, `fsm=none` (без пресета), `health=1`.

---

## RPT: цепочка решения зрения (эталон)

Домен `DM_BOT_DEBUG_VISION` (`[Vision]`) показывает каждый гейт `UpdateLOS`:

```
14:57:28 [bot] [Vision] scan: targets=3  registry: players=2 animals=87
14:57:28 [bot] [Vision] Update LOS target=dmAI_SurvivorM_Denis dist=20
14:57:28 [bot] [Vision] Angle too big dmAI_SurvivorM_Denis ang:179.63>80   ← FOV-конус, бот ещё не довернулся
14:57:30 [bot] [Vision] Angle too big dmAI_SurvivorM_Denis ang:85.78>80    ← доворот в процессе
14:57:30 … (далее БЕЗ «Angle too big» — конус пройден, raycast LOS=true)
```

Цепочка гейтов текущего зрения (порядок в `UpdateLOS`):
1. **Радиус/дистанция**: игрок на 20 м внутри `DM_PERCEPTION_PLAYER_RADIUS=1000`.
2. **FOV-конус** (`DM_PERCEPTION_FOV=160`, половинный угол ±80°): сначала `ang=179.63>80`
   (спавн-ориентация Yaw=0 смотрит на юг, цель на севере) → после `look Turn=full` угол падает
   ниже 80° и гейт проходит.
3. **`ProbabilityOfDetection`**: для свежей цели НЕ рулетка — см. ниже.
4. **`HasLOS` raycast**: глаз бота (1.4 м) → голова цели; ближайший хит = сама цель → `LOS=true`.

### Почему `ProbabilityOfDetection` не дал случайный промах (важный факт baseline)

`DiscoverTarget` ставит `t.m_LastContact = GetTickTime()` при открытии цели, а в
`ProbabilityOfDetection`:
```
forgettingTime = GetTickTime() - lastContact;
if (forgettingTime < 30.0) return true;   // первые 30 с после контакта — всегда «заметил»
```
Поэтому в первые 30 с после обнаружения неподвижный игрок на 20 м **детерминированно виден**
(`LOS=1`, `everSeen=1`), без вероятностной рулетки (`Chance:` в RPT не логируется для свежей
цели — `return true` ранний). Вероятностная ветка (`probAng·probVel·probForg`) включается
только когда `forgettingTime ≥ 30`, и видна в RPT у фоновых settlement-ботов на дальних
целях, напр.:
```
14:58:31 [Snezhana Belyaev] [Vision] Chance: 0.00164738 probAng=0.1 probVel=0.0281386 probForg=0.58545 [2.31]
14:58:31 [Snezhana Belyaev] [Vision] Цель dmAI_SurvivorF_Eva не замечена!
```

## Метрика регрессии (для сравнения ПОСЛЕ реворта)

ПОСЛЕ атома реворта гонять тот же `vision-baseline.json` (день, 20 м, неподвижный игрок
лицом к лицу) и сверять:
- `targetdump` → цель-игрок должна иметь **`LOS=1` и `everSeen=1`** (PASS-критерий).
- Не должно быть долгого «не замечен» при ясном дне и конусе < 80° (если реворт вводит
  освещённость — при ясном дне она не должна занижать вероятность до провала на 20 м).

---

## Файлы (создано в этом прогоне)

- `tools/e2e/vision-baseline.json` — НОВЫЙ минимальный baseline-сценарий.
- `tools/e2e/vision-baseline.baseline.md` — этот файл (замеры).

> NB: `tools/e2e/` в `.gitignore` (строка 18) — новые сценарии/baseline в этой папке не
> трекаются git'ом (как и 275 других untracked-сценариев). При необходимости коммита —
> `git add -f`.

## Прочее (для журнала решений / рефлексии)

- Сервер: свой порт **2312** (профиль `profiles-e2e2312`), т.к. порт 2302 занимает живой
  сервер пользователя (PID 208591, запущен `devalio` сегодня 14:31). Свой процесс остановлен
  после прогона (`pgrep -f 'port=2312'` → пусто); чужой 2302 не трогал.
- **Побочный эффект деплоя**: `@Botorama/Addons` — общий для обоих серверов (оба `-mod=@Botorama`).
  Я задеплоил туда **`--test`-сборку** (та же ветка/версия, тот же код, только больше
  DEBUG-доменов). Живой сервер 2302 не затронут (hot-reload отсутствует, PBO в памяти старые),
  но его **следующий рестарт подхватит `--test`-сборку**. Оркестратору/пользователю при желании
  пересобрать `--prod` для чистого продакшн-билда.
- Развилка (мелкая): использован **порт 2312** вместо дефолтного 2302 из-за живого сервера
  пользователя — по конвенции `_start_server_e2e2312.sh`.
- Развилка: сценарий использует **ботов без пресета** (`fsm=none`) — так цель гарантированно
  неподвижна (`Moving=0`) и не портит замер, а наблюдатель всё равно доворачивается через
  `look Turn=full` (command-интент тикается без FSM).
