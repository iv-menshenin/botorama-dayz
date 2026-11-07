# Research: фактический угол прицела оружия (где физически смотрит ствол) и синхронизация со скриптовым AIM

Статус: API подтверждено чтением ванили (`DayZ-Script-Diff`) и Expansion. Ведёт `dayz-research`.

## Цель

Разобраться, какой нативный API отвечает за **физическую** ориентацию ствола (куда реально
смотрит ствол = `GetBarrelDirection()` по мемори-поинтам `usti hlavne → konec hlavne`), чем
она ведётся («движковый aim-модел»), и как её синхронизировать со скриптовым прицелом
серверного AI-бота (`m_AimWorldDirection`/`m_AimRelAngleLR/UD` из `SetAim`). Заказчик нашёл
ванильный фикс «Camera Aim Sync Fix» (синхронизирует КАМЕРУ игрока с `GetBaseAimingAngleUD()`),
но для нас задача **обратная** — у игрока aim-модел авторитетен (по нему летит пуля), а у бота
авторитетен скриптовый прицел, и нужно заставить ствол следовать ему.

---

## 1. `HumanCommandWeapons` API — точные сигнатуры

Файл `3_game/human.c` (класс `HumanCommandWeapons`, L997+). Все — `proto native`:

| Метод | Строка | Семантика |
|---|---|---|
| `float GetBaseAimingAngleUD()` | `human.c:1095` | базовый угол прицела вверх/вниз (pitch), **без** sway/offsets |
| `float GetBaseAimingAngleLR()` | `human.c:1098` | базовый угол прицела влево/вправо (yaw), **без** sway/offsets |
| `float GetAimingHandsOffsetUD()` | `human.c:1044` | оффсет рук вверх/вниз (дыхание/шум/отдача/kuru) |
| `float GetAimingHandsOffsetLR()` | `human.c:1047` | оффсет рук влево/вправо |
| `void SetADS(bool)` | `human.c:1029` | «head tilt to optics» — пишет engine-var `ADS` |
| `void LiftWeapon(bool)` / `bool IsWeaponLifted()` | `human.c:1035/1039` | подъём ствола у препятствия |
| `void ObstructWeapon(float)` / `float GetWeaponObstruction()` | `human.c:1042/1045` | обструкция ствола |

Горизонтальный аналог есть: **`GetBaseAimingAngleLR()`** (`human.c:1098`).

### Единицы и система отсчёта (подтверждено)

- **Градусы.** Камера (`dayzplayercamera_base.c`) сравнивает `GetBaseAimingAngleUD()` напрямую
  с `pAngle` (pitch камеры в градусах); дельты ввода переводятся через `Math.RAD2DEG`
  (`dayzplayercamera_base.c:171,203`). Лимиты `CONST_UD_MIN/MAX = -85/85`,
  `CONST_LR_MIN/MAX = -160/160` (`dayzplayercamera1stperson.c:6-10`) — тоже градусы.
- **`GetBaseAimingAngleUD()` — АБСОЛЮТНЫЙ pitch** (мировой/от уровня горизонта, НЕ от корпуса).
  Доказательство — `weapon_base.c:1708` строит направление как
  `Vector(hcw.GetBaseAimingAngleLR() + GetOrientation()[0], hcw.GetBaseAimingAngleUD(), 0)`:
  к LR добавляется ориентация корпуса, а UD подставляется как есть.
- **`GetBaseAimingAngleLR()` — ОТНОСИТЕЛЬНЫЙ yaw** (относительно ориентации корпуса).
  Тот же код: `LR + GetOrientation()[0]` даёт абсолютный yaw. Аналогично
  `dayzplayerimplementthrowing.c:55-58` (`aimOrientation[0] += lr`).
- **«Базовый» = без sway/offsets.** Итоговое фактическое направление ствола =
  `Vector(LR + yaw_тела, UD, 0) + (GetAimingHandsOffsetLR(), GetAimingHandsOffsetUD())`
  (`weapon_base.c:1708-1713`, `ApproximateWeaponLiftTransform` `weapon_base.c:1585-1599`).

---

## 2. Движковый aim-модел: `SDayZPlayerAimingModel` и откуда берётся «ствол»

Структура `SDayZPlayerAimingModel` (`3_game/dayzplayer.c:1100-1119`):

```c
float m_fCurrentAimX;  //[in] horizontal aim angle - in degrees  (== базовый LR)
float m_fCurrentAimY;  //[in] vertical aim angle   - in degrees  (== базовый UD)
float m_fAimXCamOffset / m_fAimYCamOffset;        //[out] оффсеты камеры
float m_fAimXHandsOffset / m_fAimYHandsOffset;    //[out] оффсеты РУК (sway/recoil/kuru)
float m_fAimXMouseShift / m_fAimYMouseShift;      //[out] «сдвиг как мышь»
```

**`GetBaseAimingAngleUD/LR()` читает `m_fCurrentAimY/X`** — это **базовый aim**, который
движок интегрирует из **входного контроллера**, а НЕ из скрипта:

- Клиент: мышь → `HumanInputController.OverrideAimChangeX/Y(HumanInputControllerOverrideType, float)`
  (`human.c:240/243`; значение в радианах, как и `GetAimChange()`/`GetAimDelta(dt)`
  `human.c:31/34`).
- Сервер (человек): сетевой ввод от клиента (`ScriptInputUserData`).
- Серверный AI (`INSTANCETYPE_AI_SERVER`): **входа нет** → `m_fCurrentAimX/Y` не обновляется
  (остаётся 0/прошлым значением).

Скриптовый `DayZPlayerImplement.AimingModel(float, SDayZPlayerAimingModel)`
(`dayzplayerimplement.c:1707`) вызывается движком **только при raised-стойке**
(`m_MovementState.IsRaised()`, `:1726`) и пишет лишь **оффсеты** (`ProcessAimFilters`,
`dayzplayerimplementaiming.c:162`, `m_fAimXHandsOffset/Y` L236-237, камера L248-249,
mouse L271-272). Абсолютные `AimX/AimY` скрипт не пишет — их пишет движок как
`base aim + offset` и проецирует в граф-переменные `AimX`/`AimY`/`AimIKX`.

**Граф-переменные `AimX`/`AimY`/`AimIKX` — engine-driven** (см. `combat.md` §«Визуальный
прицел»): их пишет движок из своего aim-модела; `AnimSetFloat("AimX"/"AimY")` из скрипта —
no-op. `HumanST` (класс, который биндит и пишет их) — в движковом скрипт-модуле, в
`DayZ-Script-Diff` его нет.

**Где в графе живёт «физический ствол»** (ваниль `DZ/anims/workspaces/player/player_main/`):

| Файл | Строка | Узел | Переменные (движковые) |
|---|---|---|---|
| `master.agr` | 23 | `AnimNodeWeaponIK` "InventoryIK" (главный IK оружия) | `"AimIKX" "AimX"`, гейт `isbitset(ArmIK, 0/1/2)` |
| `combat.agr` | 3 / 709 | `AnimNodeWeaponIK` "DamageFullBodyIK" / "MeleeWeaponIK" | `"AimIKX" "AimY"` |
| `combat.agr` | 241 | выбор damage-позы | `AimX` |
| `gestures.agr` | 417 | `AnimNodeWeaponIK` "UnconsciousIK" | `"AimIKX" "AimY"` |
| `vehicles.agr` | 367/905/1185 | `AnimNodePose2` (AimPose пассажира) | `"AimY" "AimX"` |
| `vehicles.agr` | 2014 | `AnimNodeWeaponIK` "VehicleWeaponIK" | `"AimIKX" "AimY"` |
| `weapons.agr` | везде | выбор fire/ADS/chambering поз | `AimX` (напр. `AimX >= -35 && AimX <= 35`) |

**Гейт главного IK (`ArmIK`)** — из item-behavior конфига: `SetIKStance(...)`
(`dayzplayercfgbase.c:143-193`). Для `FIREARMS`: `SetIKStance(RAISEDERECT, true, true, true)`
(бит0 = true) vs `SetIKStance(ERECT, false, true, true)` (бит0 = false). Т.е. главный
`InventoryIK` (читает `AimIKX`/`AimX`) включается **только** для огнестрела в raised-стойке.

---

## 3. Серверная применимость `GetBaseAimingAngleUD/LR()`

- Натив доступен и на сервере: `HumanCommandWeapons` получается через
  `GetCommandModifier_Weapons()` (в `DayZPlayerImplement`), сам вызов не под `#ifndef SERVER`.
- **НО** для AI-пешки значение **не авторитетно**: движковый aim-модел на сервере AI не
  тикает (нет входного контроллера). Expansion **явно избегает** `GetBaseAimingAngle*` для AI:
  в `ExpansionHumanCommand.c:130-148` ветка `INSTANCETYPE_AI_SERVER/AI_REMOTE/AI_SINGLEPLAYER`
  пишет кастомные `SetLookDirX/Y(m_LookLR/UD)`, а `GetBaseAimingAngleLR/UD()` читается только в
  `default:` (человек). AI-код (`eAIBase.c`) вообще не зовёт `GetBaseAimingAngle*`, а
  `override AimingModel() → false` (`eAIBase.c:9993`) отключает скриптовые оффсеты.
- Значит «Camera Aim Sync Fix» (`GetBaseAimingAngleUD()` как источник истины) для **клиентского
  игрока** работает (у него aim-модел ведёт и камеру, и ствол, и пулю). Для **серверного AI** он
  не применим: читать мы сможем, но получим 0/stale, а не наш скриптовый прицел.

**Что у нас есть на сервере для чтения фактического угла ствола движка** (помимо
`GetBarrelDirection()` через мемори-поинты):
- `GetBaseAimingAngleUD/LR()` — базовый aim движка (не авторитетен для AI, см. выше) — это
  единственный прямой нативный источник «куда движок целит» без расчёта по костям.
- `GetBarrelDirection()` (наш) — фактическая ось ствола по мемори-поинтам, т.е. **результат**
  движковой анимации/IK. Это и есть «куда реально смотрит ствол».

---

## 4. Синхронизация: можно ли ЗАСТАВИТЬ ствол следовать скриптовому прицелу

**Нативного сеттера базового угла прицела НЕТ.** Проверено по всему `DayZ-Script-Diff`:
- Есть только `OverrideAimChangeX/Y` (входной путь, радианы, клиентский контроллер) и
  `SetAimLimits`/`SetLookLimits`/`SetVerticalMinimumAimLimit` (`dayzplayer.c:283/280/286`,
  `:1304/1298/1310`) — это **лимиты/клампы**, а не задание угла.
- `SetAimLimits` выставляется ванилью `-85,85,-180,180` (`dayzplayercfgbase.c:31`,
  `actionmanagerbase.c:140-152`) — диапазон, не значение.

Поэтому «заставить движковый aim-модел следить за нашим скриптом» нативно нельзя. Паттерны:

1. **(a) Полный вывод прицела из-под движка через кастомный граф** (эталон Expansion):
   кастомный `player_main.agr`, где **все** `AimPose`/`AnimNodeWeaponIK`/условия поз переписаны
   на кастомные `eAI_AimX/eAI_AimY`. Expansion пишет их так (`eAIBase.c:9584-9588`):
   ```c
   float aimX = RelAngle(AbsAngle(rel[0]) + (-15.0/distClamped));  // плечевая поправка L/d
   float aimY = rel[1];
   AnimSetFloat(m_ExpansionST.m_VAR_AimX, aimX);
   AnimSetFloat(m_ExpansionST.m_VAR_AimY, aimY);
   ```
   и `override AimingModel() → false` (`eAIBase.c:9993`), и НЕ входит в raised-стойку
   (`Raised`/`ADS` — кастомные/натив `SetADS`). `SetAimAnglesRel` в `eAICommandMove.c:163`
   **закомментирован** — не используется.
2. **(b) Читать фактический угол движка и вести по нему пулю** — для нас НЕверно: движковый
   ствол у AI не наведён на цель (см. готчу в `combat.md`: выстрел по `GetBarrelDirection()`
   закапывал пулю в землю). Это «синхронизировать камеру под ствол», а не «ствол под прицел».
3. **(c) Замкнутый контур (принцип «человеческой модели» из AGENTS.md)**: мерять ошибку
   `Δ = GetBarrelDirection() vs GetWeaponAimDirection()` и вносить в запись `dmAI_AimX/Y`
   коррекцию (не дожидаясь полного рерайта графа). Слабее (a), но дешевле — как временная мера.

---

## 5. Связь с нашими граф-переменными `dmAI_AimX/dmAI_AimY` — почему «лишь частично»

Наш `player_main.agr` (botorama `src/core/Animations/player_main.agr:1630`) включает **смесь**:

```
"dm_core/Animations/Locomotion.agf"           // custom (AimPose + 2 WeaponIK → dmAI_*)
"{9C03...}DZ/.../Combat.agf"                  // vanilla  (WeaponIK/AimX остались)
"{4DD0...}DZ/.../Weapons.agf"                 // vanilla  (AimX-условия остались)
"{F870...}DZ/.../Gestures.agf"                // vanilla  (UnconsciousIK AimIKX/AimY остались)
"{1D19...}DZ/.../Vehicles.agf"                // vanilla  (AimPose/VehicleWeaponIK остались)
"dm_core/Animations/Actions.agf"              // custom  (7 WeaponIK → dmAI_*)
"{3F5D...}DZ/.../Master.agf"                  // vanilla  (ГЛАВНЫЙ InventoryIK AimIKX/AimX остался)
"dm_core/Animations/Tests.agf"                // custom
```

Мы переписали на `dmAI_AimX/dmAI_AimY/dmAI_AimIKX` только **Locomotion.agf** и **Actions.agf**
(проверено: `grep WeaponDir` даёт только `dmAI_AimIKX`/`dmAI_AimY`). Но ванильные
**Master.agf** (главный `InventoryIK`), **Combat.agf** (`DamageFullBodyIK`/`MeleeWeaponIK`),
**Gestures.agf** (`UnconsciousIK`), **Vehicles.agf** (`VehicleWeaponIK` + AimPose) и условия в
**Weapons.agf** по-прежнему читают движковые `AimX`/`AimY`/`AimIKX` (которые у AI = 0/stale).
Отсюда «ствол ведётся движковым aim-моделом и следует `dmAI_*` лишь частично» — незавершённый
рерайт графа. Это ровно тот случай, который Expansion решает **полным** кастомным графом.

`AnimSetFloat(m_VarAimX/Y, ...)` в нашем `ApplyWeaponAim()` (`dmAISurvivorBase.c:1090-1095`)
корректен по механизму (пишет кастомные `dmAI_*`), но эти переменные запитаны только в
Locomotion/Actions, а не во все узлы, которые реально ориентируют ствол.

---

## Открытые вопросы / гипотезы для эмпирической проверки

1. **[нужно проверить] Возвращает ли `GetBaseAimingAngleUD/LR()` 0 (или stale) на серверном
   AI-боте?** Конкретный прогон: в `dmAISurvivorBase` залогировать
   `GetCommandModifier_Weapons().GetBaseAimingAngleUD()` и `.GetBaseAimingAngleLR()` в
   `ApplyWeaponAim` под `DM_BOT_DEBUG_PAWN`, сравнить с `m_AimRelAngleUD/LR`. Если движковый
   базовый aim ≠ 0 и **не** следует скрипту — подтверждается «ствол ведёт движковый aim-модел».

2. **[нужно проверить] Дрейфует ли `GetBaseAimingAngleUD()` вверх после того, как цель прошла
   мимо бота** (симптом «дуло осталось задранным»)? Захватить серию значений при прицеле вверх
   → цель ушла → `m_AimRelAngleUD` вернулся в 0, а `GetBaseAimingAngleUD()` остался. Если да —
   движковый aim накапливает питч и не сбрасывается (корень «питч-дрейфа»).

3. **[нужно подтвердить] `GetBaseAimingAngleUD() == m_fCurrentAimY`, `GetBaseAimingAngleLR() ==
   m_fCurrentAimX`** (базовый aim, до оффсетов). Косвенно подтверждено док-комментарием
   «without sway/offsets» + использованием в `weapon_base.c`/`crosshairselector.c`; прямое
   равенство проверить логом `m_fCurrentAimX/Y` из `AimingModel()` (но у нас override → false,
   и он не зовётся).

4. **[нужно проверить] Что реально ориентирует ствол у НАШЕГО бота** (вне raised-стойки, где
   `ArmIK` bit0 = false → главный `InventoryIK` выключен): расходится ли `GetBarrelDirection()`
   с `GetWeaponAimDirection()`, и совпадает ли расхождение с оставшимися ванильными
   `AimX/AimIKX`-узлами (Combat/Gestures/Vehicles) или с `Weapons.agr`-условиями. Проб: `botdump`
   с `GetBarrelDirection()` и `GetWeaponAimDirection()` в одни координаты.

5. **[нужно проверить] Есть ли какой-то нативный вход для задания базового aim на сервере AI**
   (напр. `OverrideAimChangeX/Y` на `AI_SERVER`): делает ли он что-то или no-op. Если работает —
   появляется путь «толкать aim-модел» без полного рерайта графа. (В `DayZ-Script-Diff` натива
   `SetBaseAimingAngle*` нет — это подтверждено чтением, остался только вопрос про входной
   override.)

---

## Существенные развилки / принятые решения (для `docs/decisions.md`)

- **Развилка:** вести пулю по фактическому углу ствола движка (`GetBarrelDirection()`) vs по
  скриптовому прицелу (`GetWeaponAimDirection()`). **Принято (уже зафиксировано в combat.md):**
  по скриптовому прицелу — движковый ствол у AI на цель не наведён (v3.74/75 пуля уходила в
  землю). Значит направление синхронизации — **ствол под скрипт**, а не наоборот.
- **Развилка:** нативно «заставить ствол следить за скриптом» vs полный рерайт графа (эталон
  Expansion). **Вывод:** нативного сеттера нет; рерайт нужен **полный** (в т.ч. `Master.agf`
  `InventoryIK`, `Combat/Gestures/Vehicles` WeaponIK, `Weapons.agr`-условия) — наш текущий рерайт
  только Locomotion+Actions неполон и объясняет «частичное следование». Альтернатива — замкнутый
  контур (мерять `Δ = GetBarrelDirection() − GetWeaponAimDirection()` и править `dmAI_AimX/Y`).

## Источники (файл:строка)

- `3_game/human.c`: `GetBaseAimingAngleUD` L1095, `GetBaseAimingAngleLR` L1098,
  `GetAimingHandsOffsetUD/LR` L1044/1047, `SetADS` L1029, `OverrideAimChangeX/Y` L240/243,
  `GetAimChange/GetAimDelta` L31/34, `AnimSetFloat` (`dayzplayer.c:1231`), `BindVariableFloat` L294.
- `3_game/dayzplayer.c`: `SDayZPlayerAimingModel` L1100 (`m_fCurrentAimX/Y` L1104-1105),
  `GetLookLimits/SetLookLimits/SetAimLimits/GetAimLimits/SetVerticalMinimumAimLimit`
  L1298-1310, `AimingModel` L1191.
- `4_world/entities/dayzplayerimplement.c`: `AimingModel` L1707 (`IsRaised` L1726),
  `m_AimingModel` L93/177/239.
- `4_world/entities/dayzplayerimplementaiming.c`: `ProcessAimFilters` L162 (hands offset L236-237).
- `4_world/entities/manbase/dayzplayer/dayzplayercamera_base.c`: `UpdateUDAngleUnlocked` L130
  (`GetBaseAimingAngleUD` L132), `UpdateUDAngle` L230 (`GetBaseAimingAngleUD` L234).
- `4_world/entities/manbase/dayzplayer/dayzplayercamera1stperson.c`: `CONST_UD/LR` L6-10.
- `4_world/entities/firearms/weapon_base.c`: `ApproximateWeaponLiftTransform` L1570-1645,
  `LiftWeaponCheckEx` L1708-1713 (yawPitchRoll = LR+orient, UD).
- `4_world/entities/dayzplayerimplementthrowing.c`: L55-58 (LR/UD → orientation).
- `5_mission/gui/crosshairselector.c`: L250-253 (aimAngles из LR/UD).
- `4_world/entities/manbase/dayzplayer/dayzplayercfgbase.c`: `SetIKStance` L143-193
  (`SetFirearms` L188: RAISEDERECT bit0=true, ERECT bit0=false).
- Ванильный граф `DZ/anims/workspaces/player/player_main/`: `master.agr:23`, `combat.agr:3/241/709`,
  `gestures.agr:417`, `vehicles.agr:367/905/1185/2014`, `weapons.agr` (AimX-условия).
- Expansion: `Core/.../Commands/ExpansionHumanCommand.c:130-148` (AI vs human ветка),
  `Core/.../Commands/ExpansionHumanST.c:139-140` (Bind `eAI_AimX/Y`), `:480-497` (SetAimX/Y),
  `AI/.../Entities/AI/eAIBase.c:9584-9588` (aimX=AbsAngle+(-15/dist), aimY), `:9993` (AimingModel→false),
  `AI/.../Classes/Commands/eAICommandMove.c:163` (SetAimAnglesRel — закомментирован).
- botorama: `src/core/Animations/player_main.agr:84/94/104` (dmAI_AimY/X/IKX), `:1630`
  (GraphFilesResourceNames — смесь vanilla/custom), `src/core/4_World/Entities/Bot/dmAISurvivorBase.c`
  (`SetAim` L804, `GetWeaponAimDirection` L835, `GetBarrelDirection` L880, `AimingModel` L705,
  `ApplyWeaponAim` L1076-1105).
