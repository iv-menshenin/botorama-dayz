# Research: прижатие бота открытой дверью/воротами (door-press)

Цель: понять механизм «бот открывает дверь/ворота → створка прижимает его к стене/в
угол → nopath-ловушка и гибель» и предложить фикс для `dayz-dev`.

E2E-подтверждено: бот застревает в nopath-ловушке на воротах `Land_Gate_Small_Blue`
(в навмеше помечены DOOR). Цепочка: `TryOpenDoorOnPath` → `dmBotIntent_OpenDoor` →
`OpenDoor` → створка прижимает бота. Точки: `<3732.85, 290.84, 10954.49>` и
`<7631.25, 254.21, 8716.36>`.

API-список двери здания уже покрыт в `docs/research/camping-doors.md` §1 — здесь только
добавки по семантике `IsDoorOpen`/`IsDoorOpened` и механика прижатия.

---

## 1. Тезис (корень)

`dmBotIntent_OpenDoor` отходит назад НЕ на заявленные `DM_DOOR_STEP_BACK_DIST = 0.5 м`,
а максимум ~0.2 м, потому что отход ограничен `DM_DOOR_STEP_BACK_TIMEOUT = 0.1 с` при
скорости `DM_DOOR_OPEN_STEPBACK_SPEED = 2.0 м/с`. Для гейт-ворот (длинная створка,
большая дуга качания) 0.2 м недостаточно → створка заметает бота. Плюс в углу отход
назад (180°) блокируется стеной позади. Expansion решает это не «отходом», а **halt
(остановкой) перед открытием + ожиданием времени анимации двери** и (в закомментированном
коде) **отключением коллизии с дверью** на время качания.

## 2. Наш `dmBotIntent_OpenDoor` (что есть и где ломается)

Файл `src/core/4_World/Entities/Bot/Intent/dmBotIntent_OpenDoor.c` (EXCLUSIVE, канал MOVE,
priority CRITICAL):

- Фаза 0 (отход назад): `IntentMove(180.0, DM_DOOR_OPEN_STEPBACK_SPEED)` до
  `dist >= DM_DOOR_STEP_BACK_DIST` ИЛИ `m_PhaseTimer <= 0`, где `m_PhaseTimer` стартует с
  `DM_DOOR_STEP_BACK_TIMEOUT` (`dmBotIntent_OpenDoor.c:51,68,88`).
- Фаза 1 (открыть + ждать): `m_Building.OpenDoor(m_DoorIdx)` + анимация руки
  `CMD_ACTIONMOD_OPENDOORFW`, затем полл `IsDoorOpened(m_DoorIdx) || m_PhaseTimer <= 0`
  (`dmBotIntent_OpenDoor.c:70,94`).

Константы (`src/cons/4_World/constants.c`):

```
DM_DOOR_OPEN_DIST          = 2.0   // constants.c:265 — райкаст вперёд на уровне глаз
DM_DOOR_CHECK_INTERVAL     = 0.5   // constants.c:273 — троттл проверки двери в MoveTo
DM_DOOR_STEP_BACK_DIST     = 0.5   // constants.c:277 — «отойти на 0.5 м»
DM_DOOR_STEP_BACK_TIMEOUT  = 0.1   // constants.c:280 — макс. время отхода
DM_DOOR_OPEN_TIMEOUT       = 3.0   // constants.c:283 — макс. ожидание полного открытия
DM_DOOR_OPEN_STEPBACK_SPEED= 2.0   // constants.c:286 — скорость отхода
DM_DOOR_FLAG_TIMEOUT       = 2.0   // constants.c:226 — свежесть door-candidate
```

**Где ломается**: в фазе 0 условие `m_PhaseTimer <= 0.0` срабатывает через 0.1 с. Бот
движется назад со скоростью 2.0 м/с, но с учётом разгона реальное смещение за 0.1 с ≈
0.2 м (а не 0.5 м). То есть `DM_DOOR_STEP_BACK_DIST = 0.5` **никогда не достигается** —
таймаут побеждает раньше. Бот открывает ворота, стоя в ~0.2–0.3 м от них. Длинная створка
гейт-ворот (2–3 м) при распахивании проходит через позицию бота и толкает его к стене/в
угол.

## 3. Ваниль: door API (добавки к camping-doors.md)

`class Building : EntityAI` (`3_game/entities/building.c`):

- **Семантика «open» vs «opened»** (важно для полла):
  - `IsDoorOpen(index)`  — «запрошено полностью открыть» (wanted phase > 0.5), НЕ фактическое положение (`building.c:23`);
  - `IsDoorOpening(index)` — wanted phase == 1.0 (`building.c:26`);
  - `IsDoorOpened(index)` — **фактически** phase == 1.0 (`building.c:35`);
  - `IsDoorClosed(index)` — фактически phase == 0.0 (`building.c:41`).
  - Наш полл `IsDoorOpened` корректен (ждём реального полного открытия). А вот
    `TryOpenDoorOnPath` гейтится `IsDoorOpen` (wanted) — тоже ок для детекта «уже открывается».
- `CanDoorBeOpened(doorIdx, checkIfLocked=false)` (`building.c:130`): с `false` возвращает
  true только если дверь ЗАПЕРТА (awkward-реализация, подтверждено комментарием Expansion
  `eAIBase.c:11632-11633`); с `true` — «закрыта И не заперта». Наш код использует
  `CanDoorBeOpened(doorIdx, true)` — корректно.
- **НЕТ** нативного API для стороны петель/направления качания створки (подтверждено в
  `camping-doors.md` §1). Единственная геометрия двери — `GetDoorSoundPos(index)` (центр).

Прочие нативные примитивы, нужные для фикса:

- `Human.CheckFreeSpace(vector localDir, float distance, bool useHeading, vector posOffset = vector.Zero, float xzScale = 1.0)` — `3_game/human.c:1354`. `localDir` в локальном пространстве тела.
  Константы: `vector.Forward = "0 0 1"`, `vector.Aside = "1 0 0"` (`1_core/proto/enconvert.c:121-122`), `vector.Backward = "0 0 -1"`.
- `GetVelocity(notnull IEntity ent)` — глобальный натив, линейная скорость сущности (`1_core/proto/enphysics.c:104`). Плюс нативный `PhysicsGetVelocity(out vector)` на `Human` (используем в `dmAiming.c:296`, `dmBotIntent_FollowTo.c:93`).
- `HumanCommandMove.GetCurrentMovementSpeed()` — натив (`3_game/human.c:448`). 0..3 (idle/walk/run/sprint).
- `g_Game.ConfigGetFloat("CfgVehicles <type> Doors <name> animPeriod")` — время анимации
  открытия двери из конфига (паттерн взят у Expansion `eAI_GetDoorAnimationTime`, см. §4).

## 4. Expansion: как открывает двери и обходит прижатие

`eAIBase.HandleBuildingDoors(eAICommandMove hcm, float pDt)` (`AI/.../Entities/AI/eAIBase.c:11497-11742`).

Ключевые места:

1. **Halt перед открытием** (главный механизм против прижатия):
   ```c
   //! Decrease chance of AI getting stuck between wall and opened door by temporarily stopping before opening
   if (!isDoorOpen && !m_eAI_Halt)                      // eAIBase.c:11689
   {
       eAI_SetHalt(true);                                // eAIBase.c:11691
       g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(eAI_SetHalt, timeTresh * 0.65, false, false); // :11692
   }
   ```
   `timeTresh = building.eAI_GetDoorAnimationTime(doorIndex) * 1000` (`:11674`). То есть
   ИИ замирает (~65% времени анимации двери), пока створка распахивается, и только потом
   возобновляет движение — не «отходит», а **стоит и ждёт**, чтобы не влететь в створку
   своим движением.

2. **Время анимации двери из конфига** (`AI/.../Entities/Buildings/BuildingBase.c:206-235`):
   ```c
   float eAI_GetDoorAnimationTime(int doorIndex) {
       string path = CFG_VEHICLESPATH + " " + GetType() + " Doors";      // "CfgVehicles <type> Doors"
       int count = g_Game.ConfigGetChildrenCount(path);
       ... g_Game.ConfigGetChildName(path, doorIndex, name);
       time = g_Game.ConfigGetFloat(path + " " + name + " animPeriod");  // :222
       time = Math.Max(time, 1.0);                                       // :228
   }
   ```
   Это даёт **адаптивную паузу**: у гейт-ворот `animPeriod` больше, чем у комнатной двери.

3. **Отключение коллизии с дверью — ЗАКОММЕНТИРОВАНО** (`eAIBase.c:11695-11710`):
   ```c
   //! Prevent AI getting pushed by opening/closing door by temporarily disabling collision
   /*
   PhxInteractionLayers layer = dBodyGetInteractionLayer(this);
   if (layer != PhxInteractionLayers.AI && !m_eAI_ResetInteractionLayer) {
       m_eAI_ResetInteractionLayer = true;
       dBodySetInteractionLayer(this, PhxInteractionLayers.AI);   // дверь перестаёт толкать ИИ
       g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(eAI_dBodySetInteractionLayer, inactiveDuration, false, layer);
   }
   */
   ```
   Идея «золотого стандарта»: на время качания перевести ИИ в слой `AI`, с которым дверь
   не коллизит → створка физически не может прижать. Закомментировано (вероятно, давало
   сайд-эффекты), но это прямое указание на механику: **прижатие — это физическая коллизия
   анимированной створки с капсулой персонажа**, а не «бот сам зашёл за дверь».

4. **Открытие/закрытие** (`eAIBase.c:11712-11723`):
   ```c
   if (isDoorOpen) { StartActionObject(eAIActionCloseDoorsDummy, building); building.CloseDoor(doorIndex); }
   else { StartActionObject(eAIActionOpenDoorsDummy, building);
          if (canInteract || building.IsDoorLocked(doorIndex)) building.OpenDoor(doorIndex); }
   ```
   Наш `OpenDoor(doorIdx)` идентичен (без dummy-action).

5. **Детект «заблокирован дверью»** (для восстановления, если всё же прижало):
   - `m_eAI_BlockedTime` накапливается в `eAICommandMove` (`eAICommandMove.c:771`), сброс
     при прогрессе (`:775`);
   - «стоим и не движемся >5 с при commanded speed ≥0.5» → читает
     `m_eAI_CommandMove.GetBlockingObject()` и либо телепортируется на вейпоинт, либо
     сбрасывает pathfinding (`eAIBase.c:7652-7698`);
   - при уже-открытой двери проверяет `CheckFreeSpace(Forward/Aside/-Aside, …)` — если
     есть свободное место вперёд/вбок и блокировка короткая, игнорирует дверь
     (`eAIBase.c:11602-11618`).
   - `hcm.IsBlockedByBuildingWithDoor()` / `m_IsBlockedByBuildingWithDoor` — признак «упёрся
     именно в здание с дверью» (`eAICommandMove.c:782-794`).

## 5. Таблица: наш vs Expansion vs ваниль

| Аспект | botorama (наш) | Expansion (референс) | ваниль (натив) |
|---|---|---|---|
| Перед открытием | отход назад `IntentMove(180°)`, 0.5 м / 0.1 с таймаут (реально ~0.2 м) | `eAI_SetHalt(true)` — остановка, не отход | — (игрок сам отходит) |
| Длительность ожидания | `IsDoorOpened` или `DM_DOOR_OPEN_TIMEOUT=3с` (фикс.) | `animPeriod * 0.65` из конфига (адаптивно) | — |
| Время анимации двери | не читается | `eAI_GetDoorAnimationTime` → `CfgVehicles <type> Doors <name> animPeriod` | `animPeriod` в конфиге |
| Защита от толкания створкой | нет | (закомментировано) `dBodySetInteractionLayer(PhxInteractionLayers.AI)` | физика створки толкает капсулу |
| Детект «прижало» | `m_NoProgressTime` (нет прогресса к цели) → cascade OpenDoor/Vault/Recover/Detour | `m_eAI_BlockedTime` + `GetBlockingObject()` + `CheckFreeSpace()` | `GetVelocity` / `GetCurrentMovementSpeed` |
| Escape из угла | Recover 180° → Detour ±90° (без проверки свободного места) | `CheckFreeSpace(Aside/-Aside)` выбирает свободную сторону | `CheckFreeSpace` натив |

## 6. Механизм прижатия: гипотезы (что подтверждено кодом, что нет)

- **(a) STEP_BACK слишком мал** — **подтверждено кодом**: таймаут 0.1 с обрезает отход до
  ~0.2 м, `DM_DOOR_STEP_BACK_DIST=0.5` не достигается (см. §2). Для гейт-ворот этого мало.
- **(b) отход назад блокируется (угол/стена позади)** — **правдоподобно**: `IntentMove(180°)`
  движет строго назад; если позади стена/сарай — бот стоит на месте 0.1 с, затем открывает.
  `[нужно проверить]` на двух точках: есть ли позади бота в момент открытия свободное
  место ≥1 м (это решает, поможет ли вообще любой отход назад).
- **(c) ворота качаются иначе, чем House-двери** — **подтверждено частично**: у гейт-ворот
  больше `animPeriod` и шире створка; `OpenDoor` на гейт-воротах — тот же натив, но дуга
  качания больше. `[нужно подтвердить]` эмпирически: реальный `animPeriod` и радиус дуги
  створки `Land_Gate_Small_Blue` vs обычной `Land_House_*` двери.
- **Прижатие — физическая коллизия створки с капсулой бота** — **подтверждено** косвенно
  (Expansion-комментарий «prevent AI getting pushed … disabling collision», `eAIBase.c:11695`);
  прямого нативного «дверь толкает» в скрипт-дифе нет.

## 7. Предложение фикса (для `dayz-dev`)

Слоями, по принципу «человеческой модели» (меряем ошибку → поправляем), от дешёвого к
надёжному:

1. **Починить отход назад** (минимально):
   - `DM_DOOR_STEP_BACK_TIMEOUT` поднять так, чтобы бот успевал пройти
     `DM_DOOR_STEP_BACK_DIST`: `timeout = DM_DOOR_STEP_BACK_DIST / DM_DOOR_OPEN_STEPBACK_SPEED + margin`
     (например 0.5/2.0 + 0.3 ≈ 0.55 с вместо 0.1).
   - `DM_DOOR_STEP_BACK_DIST` увеличить (0.5 → 1.5–2.0 м) — створка гейта шире обычной двери.

2. **Halt перед открытием** (эталон Expansion): после отхода — стоп (не командовать
   движение), `OpenDoor`, ждать `IsDoorOpened` (уже есть) — но **не держать MOVE-ввод в
   сторону двери** в фазе 1. Сейчас фаза 1 вообще не шлёт `IntentMove` (ок); важно, чтобы
   после `Finish` MoveTo не въезжал в ещё распахивающуюся створку — полл `IsDoorOpened`
   это уже гарантирует.

3. **Адаптивная пауза по `animPeriod`** (эталон Expansion `eAI_GetDoorAnimationTime`):
   читать `g_Game.ConfigGetFloat("CfgVehicles " + m_Building.GetType() + " Doors <child> animPeriod")`
   для `m_DoorIdx` и использовать как таймаут фазы 1 вместо фикс. `DM_DOOR_OPEN_TIMEOUT=3с`.
   `[нужно проверить]`: что `ConfigGetFloat` возвращает для гейт-ворот (если 0 — фолбэк 1.0,
   как у Expansion).

4. **Проверять, что отход реально произошёл, и фолбэчить на бок** (кейс «угол»):
   в фазе 0 измерять фактическое смещение (дельта `GetPosition()` за тик или
   `GetVelocity(pawn)`). Если за время отхода бот сместился < порога (заблокирован) —
   перейти на ±90° страйф в свободную сторону:
   ```c
   if (pawn.CheckFreeSpace(vector.Aside,  0.8, false)) dir = 90.0;
   else if (pawn.CheckFreeSpace(-vector.Aside, 0.8, false)) dir = -90.0;
   else /* оба зажаты */ открыть и положиться на halt+IsDoorOpened;
   ```
   `[нужно проверить]` конвенцию `CheckFreeSpace(localDir, dist, useHeading=false)` в
   локальном пространстве тела (для `useHeading=true` dir в world-space по heading).

5. **Детект «прижало» после открытия и escape** (страховка, если всё же прижало):
   после `OpenDoor` (и до `Finish`) коротким окном следить за «вынужденной» скоростью:
   бот НЕ шлёт `IntentMove` (фаза 1), но `GetVelocity(pawn)` заметно ненулевая несколько
   тиков → его толкает створка. Тогда — командовать `IntentMove` в сторону
   `CheckFreeSpace(Aside/-Aside)` (или назад, если свободно) на ~0.5 с, прежде чем `Finish`.
   Либо проще: **не** `Finish` по `IsDoorOpened`, а `Finish` только после того, как
   `CheckFreeSpace(vector.Backward, 0.5, false)` стало true И `IsDoorOpened`.

Минимальный безопасный первый атом (для `dayz-dev`): п.1 (таймаут + дистанция) + п.2
(halt/без ввода в фазу 1 уже есть) — без новой конфиг-логики. П.3–5 — следующими атомами
по результатам E2E на двух точках.

## 8. Открытые вопросы / [нужно проверить]

- `[нужно проверить]` реальные `animPeriod` и радиус дуги створки `Land_Gate_Small_Blue`
  против `Land_House_*` двери (спавн на `<3732.85, 290.84, 10954.49>` и
  `<7631.25, 254.21, 8716.36>`, probe-ручка `botdump`/`raycast`/`scanbox`).
- `[нужно проверить]` есть ли позади бота (по `GetDirection()`) свободное место ≥1 м в
  момент открытия на обеих точках — это отделяет гипотезу (а) от (б).
- `[нужно проверить]` возвращает ли `ConfigGetFloat(... animPeriod)` ненулевое значение
  для гейт-ворот; какой дефолт давать при 0.
- `[нужно подтвердить]` конвенция `CheckFreeSpace` (`useHeading` false → localDir в
  локальном пространстве тела), прежде чем использовать для выбора стороны страйфа.
- `[нужно проверить]` толкает ли створка бота с заметным `GetVelocity` (или это мгновенный
  телепорт капсулы без скорости) — влияет на надёжность детекта п.5.
- `Land_Gate_Small_Blue` не найден в скрипт-дифе (ни `.c`, ни `.hpp`) — это статик из
  game-данных (pbo), базовый класс уточнить по `botdump` на точке. Наш `TryOpenDoorOnPath`
  делает `Building.Cast(hits[0].obj)` и `GetDoorIndex` — значит это `Building` (нативная
  дверь), а НЕ basebuilding `Fence` (`4_world/.../basebuildingbase/fence.c`, у которого
  ворота — анимация `Wall_Gate_Rotate`, а не нативный `OpenDoor`).

## 9. Существенные развилки и принятые решения (для decisions.md)

- **Развилка**: фикс через «отход назад» (наш путь) vs «halt + ожидание анимации»
  (Expansion). *Выбор*: первично чинить отход (таймаут/дистанция) + добавить halt, т.к.
  «отойти и не влететь в створку» проще и безопаснее, чем отключение коллизии.
- **Развилка**: фикс. `DM_DOOR_OPEN_TIMEOUT=3с` vs адаптивная пауза по `animPeriod`.
  *Выбор*: ввести `animPeriod`-паузу (эталон Expansion), т.к. гейт-ворота качаются дольше
  комнатной двери и 3с может быть впритык, но — вторым атомом после подтверждения
  `ConfigGetFloat` на гейтах.
- **Развилка**: детект «прижало» через `GetVelocity` (физика) vs через
  `CheckFreeSpace` (геометрия). *Выбор*: гибрид — `CheckFreeSpace(Backward)` как условие
  «можно отойти» + `GetVelocity` как признак «меня толкают»; точная конвенция `[нужно
  подтвердить]`.
