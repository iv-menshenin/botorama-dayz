# Research: песчаная буря (sandstorm) — DayZ 1.30

Статус: исследование ванильного API 1.30. Источник — `/home/devalio/dayz/Work/DayZ-Script-Diff-130/`
(дамп скриптов **1.30**; в старом референсе `DayZ Projects/scripts` этой фичи нет).

## Цель

Разобраться, как устроена песчаная буря из 1.30: что это за явление, какими API узнать
интенсивность бури в конкретной точке мира, и можно ли спрогнозировать, что скоро
начнётся/накроет буря в конкретной точке (для ботов botorama).

---

## 1. Что это такое (архитектура)

Песчаная буря — **движущийся фронт (стена песка)**, а не «вспышка по всей карте сразу».
Она имеет позицию, направление (от старта к финишу), скорость, «край» и интенсивность,
которая зависит от позиции относительно фронта.

Слои:

| Слой | Файл | Роль |
|---|---|---|
| Native-интерфейс | `3_game/sandstorm.c` | `SandstormController` (алиас `Sandstorm`) — движок, сервер-управляемый, реплицируется клиентам |
| Скриптовый контроллер | `4_world/systems/sandstorm/scriptedsandstormcontroller.c` | `ScriptedSandstormController : SandstormController` — модификаторы интенсивности (укрытия), пост-процесс |
| Пер-игрок данные | `4_world/systems/sandstorm/playersandstormdata.c` | `PlayerSandstormData` — таймеры переходов (крыша/машина/подземелье) |
| Погодная логика мира | `4_world/classes/worlds/nasdara.c` | мир **Nasdara** (пустыня): решение «начнётся ли буря», ветер, volfog-пыль |
| Интеграция в WorldData | `3_game/worlddata.c` | `StartSandstorm()`, `WeatherSandstormStart/Stop()`, настройки |
| Геттер | `3_game/weather.c` | `Weather::GetSandstorm()` |
| Влияние на игрока | `4_world/classes/playermodifiers/modifiers/conditions/sandstormexposure.c` | модификаторы экспозиции (силикоз, раздражение глаз, кашель, щурение) |
| Пост-процесс | `3_game/ppemanager/requesters/ppersandstorm.c` | `PPERequester_SandstormEffect` — грейдинг, god rays |
| Шумовая модель | `3_game/weather.c` (`GetSandstormNoiseReduction`) | буря приглушает шум (`SANDSTORM_NOISE_REDUCTION_WEIGHT = 0.5`, `constants.c:1145`) |

Доп. упоминания: `squintstate.c`, `emodifiers.c`, `modifiersmanager.c`, `playerbase.c`,
`dayzplayercamera_base.c` (NV-оптика для бури), `zombiebase.c`, `cfggameplayhandler.c` /
`cfggameplaydatajson.c` (`GetSandstormFrequency`), `noise.c`, `object.c`, `erpcs.c`
(DEV RPC), интро-сцены, debug-вкладка погоды.

---

## 2. API контроллера (`SandstormController`, алиас `Sandstorm`)

Доступ: `Sandstorm sandstorm = g_Game.GetWeather().GetSandstorm();`

Конструктор/деструктор **запрещены** из скриптов (инстанс только через `GetSandstorm()`).

| Метод | Возвращает | Назначение |
|---|---|---|
| `Start(float duration, bool calledByWeather)` | bool | Старт бури (Server-only). Наращивает величину за `duration` сек |
| `Stop(float duration, bool calledByWeather)` | void | Стоп бури, затухание за `duration` сек (Server-only) |
| `IsActive()` | bool | Буря активна где-то на карте |
| `GetDirection()` | vector | Направление от стартовой позиции к конечной (куда движется фронт) |
| `GetPosition()` | vector | Текущая позиция фронта в мировых координатах |
| `GetSpeed()` | float | Скорость продвижения фронта, м/с |
| `GetRemainingMovementDuration()` | float | Секунд до достижения конечной позиции (при текущей скорости) |
| `IsPositionAtEnd()` | bool | Фронт дошёл до конечной позиции |
| `GetPointOnEdge(worldPos, out pos)` | float | Проецирует точку на край бури. **Возвращает расстояние до края** (отрицательное = точка внутри бури) |
| `GetIntensity(worldPos)` | float | Интенсивность бури в точке мира |
| `GetIntensityForPlayer(Man)` | float | Интенсивность у игрока (с учётом укрытий, см. §4) |
| `GetMagnitude()` | float | Пиковая величина бури, **не зависящая от позиции** |

События (наследуются в `ScriptedSandstormController`):
- `OnStart(bool continuedSession, bool calledByWeather = false)` — буря стала активной (сервер + клиент; клиенту при джойне — даже если буря уже идёт).
- `OnUpdate(float deltaTime)` — каждый тик, пока активна (сервер + клиент).
- `OnStop(bool calledByWeather = false)` — буря затухла.

**Native** (движок) `OnStart/OnStop` форвардят на сервере в `WorldData.WeatherSandstormStart/Stop`
(sealed), которые зовут `OnSandstormStart/OnSandstormStop` (protected, переопределяются миром).

### Преобразование ветра ↔ направление

- `Weather.WindDirectionToAngle(vector dir)` → float (радианы)
- `Weather.AngleToWindDirection(float angle)` → vector

Используется в `dayzgame.c` (DEV_SET_SANDSTORM) чтобы выровнять ветер под направление фронта.

---

## 3. Как узнать интенсивность бури в конкретной точке

**Базовый вызов:**

```c
Sandstorm ss = g_Game.GetWeather().GetSandstorm();
if (ss.IsActive())
{
    float intensity = ss.GetIntensity(worldPosition);   // <0,1>, позиционно-зависимая
    vector edgePos;
    float distToEdge = ss.GetPointOnEdge(worldPosition, edgePos); // <0 => точка внутри бури
}
```

Для точки у **игрока** — `ss.GetIntensityForPlayer(player)` (см. §4: добавляет затухание
под землёй / под крышей / в машине).

Пик бури целиком (не зависит от точки) — `ss.GetMagnitude()`.

**Замечание по «0»:** `GetIntensityForPlayer` возвращает 0, если `baseIntensity <= 0`
(точка вне фронта/интенсивность нулевая), либо если игрок полностью укрыт.

---

## 4. Как считается интенсивность у игрока (`ScriptedSandstormController`)

`GetIntensityForPlayer(Man)` (переопределён в `scriptedsandstormcontroller.c`) делает:

1. `baseIntensity = GetIntensity(headPosition)` (голова игрока).
2. Если `<= 0` → вернуть 0.
3. Последовательно применяет модификаторы (каждый может обнулить/снизить):
   - **Подземелье** (`ApplyUndergroundModifier`) — по `player.GetUndergroundPresence()`
     (`EUndergroundPresence.FULL/TRANSITIONING`), лерп к 0 за 1.5 c.
   - **Ниже поверхности** (`ApplySurfaceDistanceModifier`) — если `playerPos[1] < SurfaceY(x,z)`,
     гасит пропорционально глубине (`MAX_SURFACE_DISTANCE = 2.0`).
   - **Под крышей/в интерьере** (`ApplyBuildingCheckModifier`) — `MiscGameplayFunctions.IsUnder(...)`
     (без `CrashBase`, `Plant`) + `player.IsSoundInsideBuilding()`, лерп за 1.5 c.
   - **В машине** (`ApplyVehicleModifier`) — только `Car` (не любой `Transport`), лерп за 1.5 c.

Переходы сглажены таймерами из `PlayerSandstormData` (1.5 c — `INTERIOR_TRANSITION_DURATION`),
чтобы не было резких скачков при входе/выходе из укрытия.

---

## 5. Где и как буря начинается (важно для бота)

**Решение «начнётся ли буря» — приватное, серверное, только в мире Nasdara.**

- `Start()` вызывается только из двух мест:
  - `worlddata.c:486` — `protected void StartSandstorm(float timeToGather)`, который зовёт
    `m_Weather.GetSandstorm().Start(timeToGather, true)`.
  - `dayzgame.c:3436` — debug RPC `DEV_SET_SANDSTORM` (`Start(fadeInTime, false)`).
- `StartSandstorm()` вызывается только из `nasdara.c::WeatherOnBeforeChange` (случай OVERCAST),
  когда `m_IsSandstorm && !previousWeatherHadSandstorm`.
- `m_IsSandstorm` (`protected bool`) ставится в `NasdaraData::CalculateWind()` в ветке
  **bad weather** (`m_ChoosenWeather == BAD_WEATHER`), при `windChance < sandstormChance`,
  где `sandstormChance = clamp(8 * m_sandstormFrequency, 0, 100)`.
- `m_sandstormFrequency` приходит из `cfgGameplay.json` (`CfgGameplayHandler.GetSandstormFrequency()`,
  `cfggameplaydatajson.c:458`, дефолт `1.0`; `<0` → `0`).

**Ключевой вывод:** код, который **естественно** стартует бурю, есть **только в `nasdara.c`**.
Миры Chernarus+/Enoch(Livonia)/Sakhal в дампе 1.30 **не упоминают** `GetSandstorm`/`Start`.
Значит на текущих картах botorama буря сама не возникает — только если сервер форсирует
через debug RPC (`DEV_SET_SANDSTORM`/`DEV_STOP_SANDSTORM`) или админ-командой. При этом
модификаторы экспозиции (`MDF_SANDSTORM_EXPOSURE_*`) зарегистрированы **глобально**
(`modifiersmanager.c:146-147`) — их `ActivateCondition` просто вернёт false, пока буря неактивна.

Сопутствующие настройки (в `worlddata.c:594-597`, переопределяются миром):
- `m_SandstormMinWindDirection = -1.77` рад, `m_SandstormMaxWindDirection = -1.20` рад.
- `m_SandstormMinWindMagnitude = 18` м/с (верхний предел — `GetWindMaximumSpeed()` = 20 у Nasdara).

---

## 6. Прогнозирование (ответ на вопрос «можно ли предсказать»)

### 6.1 Прямого «прогноза бури» НЕТ

- `SandstormController` **не имеет** `GetForecast()`/«скоро начнётся». Есть только текущее
  состояние (§2).
- Решение стартовать бурю — приватное (`protected bool m_IsSandstorm` внутри `WorldData`),
  снаружи скрипта недоступно; `CalculateWind`/`WeatherOnBeforeChange` — серверные и приватные.

### 6.2 Косвенные сигналы (доступны через `Weather`)

Все феномены (`Overcast`, `WindMagnitude`, `WindDirection`) — это `WeatherPhenomenon` с
`GetActual()` и **`GetForecast()`** («куда ползёт значение»):

- **Облачность** `GetOvercast().GetForecast()` → к высоким значениям (bad weather).
  В Nasdara буря возможна ТОЛЬКО в ветке bad weather; фактический overcast на время бури
  форсируется к `≥0.75`.
- **Ветер** `GetWindMagnitude().GetForecast()` / `GetActual()` → приближение к порогу
  бури (≥18 м/с = `m_SandstormMinWindMagnitude`).
- **Направление ветра** `GetWindDirection().GetForecast()` → выравнивание в диапазон
  направления бури `-1.77..-1.20` рад.

Это слабые/нестрогие предикторы: они дают «похоже на надвигающуюся бурю», но не гарантию
и не точное время. Реальное решение и момент `Start()` приватны.

### 6.3 Когда буря УЖЕ активна — точный прогноз прихода в точку

- **Время до прихода фронта в точку:** `distToEdge = GetPointOnEdge(worldPos, out pos)`;
  для точки впереди фронта `distToEdge > 0`, тогда `time ≈ distToEdge / GetSpeed()`.
- **До конца маршрута фронта:** `GetRemainingMovementDuration()`.
- **Текущая интенсивность в точке:** `GetIntensity(worldPos)`.

Т.е. «накроет ли буря конкретную точку и когда» — вычислимо по `GetPointOnEdge` + `GetSpeed`
**пока буря активна**; «начнётся ли буря вообще и когда» — напрямую недоступно, только
эвристика по forecast облачности/ветра.

---

## 7. Влияние на игрока (симптомы/агенты)

`4_world/classes/playermodifiers/modifiers/conditions/sandstormexposure.c`:

- `SandstormExposureStaticMdfr` — только для тест-зоны `SandstormZoneSmall_TESTONLY`
  (`ContaminatedArea_Static`): на enter/exit активирует/деактивирует
  `MDF_SANDSTORM_EXPOSURE_STATIC`.
- `SandstormExposureDynamicMdfr` — динамический:
  - `ActivateCondition`: `sandstorm.IsActive() && GetIntensityForPlayer(player) > 0`.
  - `DeactivateCondition`: `IsActive() && GetIntensityForPlayer(player) <= 0`.
  - OnTick транслирует агентов **`SILICOSIS`** и **`EYES_IRRITATION`** (airborne solid,
    `TransmitAgentsEx`) с масштабом `sandstormIntensity * AGENTS_PER_SEC * deltaT`.
  - **Кашель** (`SYMPTOM_COUGH`): интервал `lerp(25s, 3s, coughIntensity)`, где
    `coughIntensity = clamp(sandstormIntensity - protectionLevel, 0, 1)`;
    `protectionLevel` — защита от `DEF_DUST_PARTICLE_BREATH` (маска/очки через
    `GetCachedEquipment().GetEntitiesByCategory(EYEWEAR|MASK, ATTACHMENT, depth 2)`).
  - **Щурение** (`SYMPTOM_SQUINT`) — вторичный симптом на активации.

При бессознанке агенты не передаются (`IsUnconscious()` → 0).

Также в `dayzplayercamera_base.c` — отдельные NV-режимы `NV_SANDSTORM_OPTICS`,
`NV_UNDERGROUND_SANDSTORM_OPTICS`, `NV_SANDSTORM_GLASSES`, `NV_SANDSTORM_PUMPKIN` (и
`NV_UNDERGROUND_*` варианты) — очки/NV против бури.

---

## 8. Пост-процесс (`PPERequester_SandstormEffect`)

`3_game/ppemanager/requesters/ppersandstorm.c`. Регистрируется как
`PPERequesterBank.REQ_SANDSTORMEFFECT`, управляется из `ScriptedSandstormController.OnStart/OnUpdate/OnStop`
(только на клиенте, не на выделенном сервере). Ключевые константы:

- Порог активации `SANDSTORM_THRESHOLD_MIN = 0.20`.
- Цвет (HSV): `SANDSTORM_COLOR_LIGHT = "1.0 0.80 0.5"`, `SANDSTORM_COLOR_HEAVY = "1.0 0.85 0.6"`,
  alpha 0.2→0.5.
- Сатурация 1.0→0.78, контраст 1.0→0.92, яркость 1.0→0.97.
- God rays 0→0.10 (overburn 0→0.05), видимость солнца 0.5→0.35.

`m_PPESandstormRequester.SetTargetIntensity(localIntensity)` — куда клиент передаёт
рассчитанную для себя интенсивность.

---

## 9. Конфиг (`cfgGameplay.json`)

`3_game/cfggameplaydatajson.c`:
```c
class ITEM_SandstormData : ITEM_DataBase {
    // Rate at which the sandstorm should occur.
    //   0.0 : No sandstorms
    //   0.5 : half as likely
    //   1.0 : normal
    //   2.0 : twice as likely
    float sandstormFrequency = 1.0;
}
```
Доступ: `CfgGameplayHandler.GetSandstormFrequency()` (`cfggameplayhandler.c:533`).

---

## 10. Открытые вопросы / [нужно проверить]

1. **[нужно подтвердить]** Геометрия фронта (start/end позиции, ширина «стены», форма края)
   — native, в скриптах не описана. `GetPointOnEdge` отдаёт расстояние до края, но точная
   форма (плоскость? цилиндр? движущаяся полоса?) неизвестна. Проверяемо эмпирически через
   `DEV_SET_SANDSTORM` + `GetPointOnEdge`/`GetIntensity` в нескольких точках.
2. **[нужно подтвердить]** `GetIntensity`/`GetPointOnEdge` на **клиенте** vs **сервере**:
   реплицируется ли позиция фронта на клиент в реальном времени (для предиктора бота на
   сервере — точно доступно; для клиентских эффектов — движок сам, но боту это не нужно).
3. **[нужно проверить]** `GetPointOnEdge` возвращает расстояние **вдоль направления фронта**
   (до ближайшей точки края по нормали) или проекцию на направление движения? Влияет на
   формулу `time ≈ dist / speed` в §6.3.
4. **[нужно подтвердить]** Порог `m_SandstormMinWindMagnitude = 18` и диапазон направления
   `-1.77..-1.20` — специфичны для Nasdara или это дефолты `WorldData`, применимые и к другим
   мирам (актуально, если захотим форсировать бурю на Chernarus/Livonia).

---

## Вывод

- **Буря = движущийся фронт** (позиция/направление/скорость/край), интенсивность зависит от
  точки; `GetMagnitude()` — пик, `GetIntensity(pos)`/`GetIntensityForPlayer(player)` — локальная.
- **Интенсивность в точке**: `GetIntensity(worldPos)` (+ `GetIntensityForPlayer` с учётом
  укрытий: подземелье/под поверхностью/под крышей/в машине).
- **Прогноз начала — напрямую НЕТ** (нет `GetForecast()` у бури, решение приватное в
  `WorldData`). Косвенно — по `GetOvercast().GetForecast()` (bad weather) и
  `GetWindMagnitude().GetForecast()` (→18 м/с). **Прогноз прихода в точку** — только когда
  буря уже активна: `GetPointOnEdge(pos)` / `GetSpeed()`.
- **Для botorama:** естественный запуск бури есть только у мира Nasdara; на Chernarus/Livonia
  буря сама не появится (только debug RPC `DEV_SET_SANDSTORM`). Для бота это значит: (а) на
  текущих картах релевантность низкая; (б) если решим учитывать — можно опрашивать
  `IsActive()` + `GetIntensity`/`GetPointOnEdge` как «погодный фактор» (шум приглушён на
  `SANDSTORM_NOISE_REDUCTION_WEIGHT = 0.5`, видимость падает через пост-процесс).

---

## Существенные развилки / принятые решения (для docs/decisions.md)

1. **Бурю рассматривать как «погодный фактор», а не как источник позиционного урона.**
   Почему: на картах botorama (Chernarus/Livonia) она естественно не возникает, а движок уже
   сам гасит шум (`SANDSTORM_NOISE_REDUCTION_WEIGHT`) и видимость (пост-процесс). Решение:
   при необходимости только читать `IsActive()`/`GetIntensity()` и не писать свою логику спавна.
2. **Не строить «предсказатель начала бури» на косвенных forecast-сигналах.** Почему: решение
   приватно в `WorldData`, forecast облачности/ветра даёт лишь вероятность, а не факт/время;
   для botorama это over-engineering. Решение: если понадобится — только «приход в точку»
   через `GetPointOnEdge`/`GetSpeed` при уже активной буре.

---

## Эмпирика (E2E, ветка feat/sandstorm-shelter)

- **Фронт движется со скоростью магнитуды ветра**: при `wind 18 м/с` — ровно ~18 м/с
  (558 м / 31 с), без ветра — ~2.86 м/с (ползёт, до зданий десятки минут). Для теста
  обязательно форсить ветер ≥18.
- **Фронт — резкая «стена»**: `GetIntensity` ≈ 1.0 позади фронта, ≈ 0.03 впереди, перепад
  в ~300–500 м. Гистерезис «вход 0.6 / выход 0.1» даёт узкое позиционное окно — бот
  входит/выходит фактически по пересечению фронта (поэтому и нужен гистерезис, а не один порог).
- **Фронт накрывает весь диапазон Z** (проверено Z=6000…14000 при X=18500 — intensity 1.0).
- **Направление фронта нативный** (`dir=<-1,0,0>`, старт `(18300,0,10000)` на Nasdara);
  выравнивание ветра через `Weather.WindDirectionToAngle(GetDirection())` на фронт не влияет —
  только магнитуда ветра задаёт скорость.
- **`GetNearestBuildingWithDoors` на Nasdara работает** (реестр живых зданий; Shelter нашёл
  `Land_Shed_M3`).
