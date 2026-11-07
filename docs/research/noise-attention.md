# Research: реакция бота на шум (модель внимания: аттракшен/концентрация)

Цель: подтвердить по коду точные сигнатуры API для фичи «новая свежая цель получает
аттракшен по типу шума → затухает → бот сравнивает с концентрацией (по состоянию FSM)
→ личный интент взгляда/остановки на 5–15 с». Писать только факты с файл:строка;
неподтверждённое помечено `[нужно проверить]`/`[нужно подтвердить]`.

## 1. `dmTarget` — поля и жизненный цикл

Файл: `src/core/4_World/Entities/Bot/dmTarget.c` (класс `dmTarget`, строки 17–170).

Полный список полей (строки 19–36):
```
dmTargetType m_Type = DESTROY;         // 19  (enum dmTargetType: ACQUIRE/DESTROY, 11–15)
EntityAI m_Entity;                     // 20
string m_ClassEntity;                  // 21
vector m_LastPosition;                 // 23
float m_LastPositionSpread = 0.0;      // 24
float m_LastDistance;                  // 25
bool m_HasLOS = false;                 // 26
float m_LastContact = 0.0;             // 27  (GetGame().GetTickTime())
float m_LastDamage = 0.0;              // 28
float m_NextLOSUpdate = 0.0;           // 32
float m_Threat = 0.0;                  // 34
float m_Attractiveness = 0.0;          // 35
bool m_Friendly = false;               // 36
```

**Флага «ever seen» НЕТ.** `m_HasLOS` — «видна в последнем скане» (это НЕ «хоть раз
видели»: он сбрасывается в `false` каждый раз, когда FOV-конус/дистанция/вероятность не
прошли). `m_LastContact` — время последнего контакта (любого: зрение ИЛИ слух), поэтому
по нему нельзя отличить «слышал, но никогда не видел» от «видел». Для атома А1 нужен
новый флаг, например `bool m_EverSeen` (выставлять в `true` в пути LOS-успеха).

Где `m_HasLOS` выставляется `true` (единственное место в perception-конвейере):
`src/core/4_World/Entities/Bot/Perception/dmVision.c:204` — внутри `UpdateLOS()`:
```
t.m_HasLOS = HasLOS(pawn, e, botPos, targetPos);   // 204
if (t.m_HasLOS) { t.m_LastPosition = targetPos; t.m_LastPositionSpread = 0.0; t.m_LastContact = now; }
```
Легаси-путь `dmAISurvivor.RememberTarget()` (`dmAISurvivor.c:2189`) тоже пишет
`t.m_HasLOS = true` (строка 2203), но **нигде не вызывается** (grep: только определение)
— мёртвый код, не считать источником.

`IsVisible()` у `dmTarget` (`dmTarget.c:48–51`) = просто `return m_HasLOS;` — «виден в
последнем LOS-скане» (с учётом троттлинга `m_NextLOSUpdate` может отставать на 0.2–0.3 с).

**Все вызовы `DiscoverTarget(entity, threat, attractiveness, friendly)`**
(`dmAISurvivor.c:1836`, определение):
- `dmVision.ScanZombies` `dmVision.c:83` → `(z, DM_TARGET_THREAT_ZOMBIE, DM_TARGET_ATTRACT_ZOMBIE, false)`
- `dmVision.ScanAnimals` `dmVision.c:101` → `(a, DM_TARGET_THREAT_ANIMAL, DM_TARGET_ATTRACT_ANIMAL, false)`
- `dmVision.ScanPlayers` `dmVision.c:124` → `(p, DM_TARGET_THREAT_PLAYER, DM_TARGET_ATTRACT_PLAYER, friendly)`
- `dmAISurvivor.HearNoise` `dmAISurvivor.c:1993` → `(entity, DM_NOISE_THREAT, attractiveness, false)`

Поведение `DiscoverTarget` при существующей цели (`dmAISurvivor.c:1840–1843`): только
монотонно повышает `m_Threat`/`m_Attractiveness` (max), НЕ обновляет позицию/контакт.
Новая цель создаётся «чистой» (`m_HasLOS=false`, `m_LastPosition=vector.Zero`,
`m_LastContact=now`, `m_NextLOSUpdate=0.0`) — строки 1845–1855.

## 2. `dmNoiseType` — enum и генераторы шума

Enum: `src/core/3_Game/Perception/dmNoiseSystem.c:9–16`:
```
enum dmNoiseType { OTHER=0, STEP, SCREAM, BULLETIMPACT, SHOT }
```
**Типа EXPLOSION НЕТ.** Сейчас 5 значений; шум «взрыв» не эмитится.

Сигнатура `dmNoiseSystem.AddNoise` (`dmNoiseSystem.c:23`):
```
static void AddNoise(EntityAI source, vector position, float strength, int type = dmNoiseType.OTHER)
```
— публикует через `SI_OnNoiseAdded.Invoke(source, position, strength, type)` (строка 30).

Подписчик `dmHearing.OnNoise` (`src/core/4_World/Entities/Bot/Perception/dmHearing.c:15`):
```
void OnNoise(EntityAI source, vector position, float strength, int type)
```
Логика: фильтр дистанции `d.LengthSq() > strength*strength → return` (29); `!source → return`
(39, позиционный шум без цели, напр. попадание пули); `root = source.GetHierarchyRootPlayer()`
(47); свой шум игнор `root == bot.GetPawn() → return` (52); **SHOT → `m_Bot.OnGunshot(root)`**
(56–57); attractiveness = `DM_NOISE_ATTRACTIVENESS_NOISE` (0.6), для SHOT/BULLETIMPACT =
`DM_NOISE_ATTRACTIVENESS_SHOT` (0.9) (59–61); финал `m_Bot.HearNoise(root, position, attractiveness)`
(66). **`type` НЕ передаётся дальше в `HearNoise`** — только `attractiveness` (потеря типа
на границе слуха; для А2 тип шума придётся либо прокинуть, либо закодировать в attractiveness).

Генераторы шума (все, файл:строка):
- `modded_DayZPlayerImplement.c:11` — `OnStepEvent` → `AddNoise(this, GetPosition(), DM_NOISE_STEP_STRENGTH, STEP)` (шаги игрока/бота).
- `modded_WeaponBase.c:82` — `dmBot_Fire` (выстрел ИИ-бота) → `AddNoise(pawn, ownerPos, dmBotGunshotNoiseStrength(), SHOT)`.
- `modded_WeaponBase.c:114` — `OnFire` (выстрел ванильного стрелка) → `AddNoise(owner, ownerPos, ..., SHOT)`; скипает ИИ-владельца (111–112) чтобы не дублировать `dmBot_Fire`.
- `modded_ZombieBase.c:20` — `OnSoundVoiceEvent` → `AddNoise(this, GetPosition(), DM_NOISE_SCREAM_STRENGTH, SCREAM)`.
- `modded_DayZGame.c:13` — `FirearmEffects` (попадание пули в поверхность) → `AddNoise(null, pos, DM_NOISE_BULLETIMPACT_STRENGTH, BULLETIMPACT)`.

Константы силы (`src/cons/3_Game/constants.c`):
- `DM_NOISE_THREAT = 0.4` (53)
- `DM_NOISE_GUNSHOT_STRENGTH = 3000.0` (56), `DM_NOISE_STEP_STRENGTH = 10.0` (57),
  `DM_NOISE_BULLETIMPACT_STRENGTH = 15.0` (58), `DM_NOISE_SCREAM_STRENGTH = 30.0` (59)
- `DM_NOISE_GUNSHOT_SILENCED_PISTOL = 75.0` (62), `_RIFLE = 100.0` (63), `_HOMEMADE = 150.0` (64)
- `DM_NOISE_ATTRACTIVENESS_NOISE = 0.6` (67), `DM_NOISE_ATTRACTIVENESS_SHOT = 0.9` (68)

**Крик зомби** (`modded_ZombieBase.c:16–22`): хук `override OnSoundVoiceEvent(event_id, event_user_string)` —
вызывает `super` и на сервере эмитит `SCREAM` **безусловно** (при любом voice-событии зомби,
независимо от того, атакует ли он игрока или бота). Никакого условия `m_ActualTarget` здесь
нет. `[нужно подтвердить]` — эмитится ли `OnSoundVoiceEvent` при атаке зомби на ИГРОКА
(ванильный `OnSoundVoiceEvent` вызывается и для агро-рыка, и для idle-стонов?); если да, бот
услышит «крик зомби, атакующего игрока» — это поведение для feature «реакция на шум» скорее
желаемо, но требует явного подтверждения на сервере (probe: спавн зомби + игрок → botdump).

## 3. `m_Attractiveness` — кто читает

> **УДАЛЕНО (атом A5 рефакторинга целей).** `m_Attractiveness` и связанные константы
> (`DM_TARGET_ATTRACT_*` / `DM_NOISE_ATTRACTIVENESS_*` / `DM_HUNT_MIN_ATTRACTIVENESS`)
> вырезаны целиком; `DiscoverTarget`/`HearNoise` больше не принимают `attractiveness`,
> Hunting входит только по `threat >= DM_ATTACK_THREAT_THRESHOLD`. Записи ниже —
> исторический снимок до удаления.

Grep по всему `src` — **единственный потребитель-логика**: `dmAISurvivor.GetHuntTarget()`
(`dmAISurvivor.c:2169`):
```
bool attractive = t.m_Attractiveness > DM_HUNT_MIN_ATTRACTIVENESS;  // 0.5
```
Пишут: `DiscoverTarget` (1842/1849), `HearNoise` (1974–1975), `RememberTarget` (2200, мёртвый).
Деб-вывод: `dmBotCommand.c:664`. В луте/Hunting-состоянии напрямую `m_Attractiveness` больше
нигде не читается (Hunting полагается на `GetHuntTarget`, который уже отфильтровал).

**Вывод:** безопасно добавить отдельное поле `float m_Attraction` (отдельная шкала «свежести
внимания», затухает −0.05/10с), НЕ переиспользуя `m_Attractiveness` (та — «интересность типа
цели», ставится один раз по шуму и не затухает; затухание к ней привело бы к поломке
`GetHuntTarget`/входа в Hunting). Либо добавить поле-время `m_LastNoise`/`m_EverSeen` и
считать аттракшен «на лету».

## 4. Интенты взгляда/остановки

Базовый класс `dmBotIntent` (`src/core/4_World/Entities/Bot/Intent/dmBotIntent.c`):
- Поля конфигурации (40–51): `m_Priority` (IDLE/DESIRABLE/CRITICAL), `m_Concurrency`
  (PARALLEL/EXCLUSIVE), `m_Manage` (канал), `m_Deadline` (-1 = без), плюс runtime
  `m_Age/m_Finished/m_Failed/m_Active/m_LastRun`.
- Жизненный цикл: `OnStart` (58), `OnUpdate` (65), `OnSkip` (70), `OnCancel` (75),
  `Finish()` (84, ставит `m_Finished=true`), `Fail()` (96, ставит И `m_Failed`, И
  `m_Finished`), `TickAge` (105), `IsFinished` (110), `IsFailed` (115),
  `IsExpired` (120: `m_Age > m_Deadline`, или `> DM_INTENT_MAX_AGE=300` без дедлайна),
  `IsBusy` (134, по умолчанию false).
- `dmBotLookTurn` enum (21–26): `NONE` (только голова), `AUTO` (через плечо), `FULL`
  (полный разворот корпуса).
- `dmBotIntentsChannel` enum (28–38): NONE/LOOK/MOVE/STANCE/EMOTION/ATTACK/VOICE/DRIVE.

**`dmBotIntent_HoldLook`** (`dmBotIntent_HoldLook.c`) — «стой и смотри в точку/сущность»:
- Поля (4–7): `vector m_Point`, `EntityAI m_Entity`, `ref dmTarget m_Target`, `dmBotLookTurn m_Turn = AUTO`.
- Канал `m_Manage = LOOK` (11). `OnUpdate` (19–36): **каждый тик** пере-ассертит взгляд:
  если `m_Target` → `bot.LookAtPoint(m_Target.GetAimPosition(), m_Turn)`; иначе `m_Entity` →
  `LookAtPoint(pos+EYE_HEIGHT, m_Turn)`; иначе `m_Point`. **Нет встроенного таймаута/длительности**
  — живёт до `Finish()`/дедлайна/пула. Длительность задаётся полем `m_Deadline` (через базовый
  `IsExpired`), или явным `Finish()` из внешнего кода (как в `EvadeAim`, см. ниже).
- Пример парного запуска: `dmAISurvivor.EvadeAim` (1632–1666) создаёт `m_EvadeAimLook =
  new dmBotIntent_HoldLook(); m_Entity=aggressor; m_Turn=FULL; m_Priority=CRITICAL;` и добавляет
  в personality-пул. Снятие пары — `UpdateEvade` (1669–1683): при `IsFinished/IsExpired` у
  MOVE-интента зовёт `m_EvadeAimLook.Finish()`.

**`dmBotIntent_Turn`** (`dmBotIntent_Turn.c`) — «развернуться корпусом на относительный угол»:
- Поля (5–7): `m_Angle` (относительный, ±град), `m_ReachAngle = 2.0`, `m_TargetYaw` (абсолютный, в OnStart).
- `OnStart` (19–25): `m_TargetYaw = orientation[0] + m_Angle; bot.LookAtYaw(m_TargetYaw, FULL);`.
- `OnUpdate` (27–35): каждый тик `LookAtYaw(m_TargetYaw, FULL)`; при `|GetYawTo| <= m_ReachAngle → Finish()`.
- Канал LOOK (11). Самозавершается по достижению угла.

**`dmBotIntent_Glance`** (`dmBotIntent_Glance.c`) — «глянуть головой в относительную сторону»:
- Поля (5–6): `m_Angle` (относительный), `m_Turn = NONE` (по умолчанию только голова).
- `OnUpdate` (18–23): `bot.LookAtDirection(m_Angle, 0.0, m_Turn);` — короткоживущий (дедлайн).

**`dmBotIntent_LookAround`** (`dmBotIntent_LookAround.c`) — «стоять и периодически оглядываться»:
- Поля (12–22): `m_AllowBodyTurn`, `m_Turn`, `m_Interval=10`, `m_Timer`, `m_CurYaw`, `m_HoldTimer`,
  `m_HoldDuration`, `m_BodyTarget`, `m_Phase` (0=forward,1=head hold,2=body turn).
- Комментарий (3–7): **обязан пере-ассертить look каждый тик** (арбитраж сбрасывает LOOK в
  forward каждый тик через `LookForward`), иначе голова «щёлкает» обратно.
- Пример использования: `dmBotState_Idle.CreateScan` (`dmBotState_Idle.c:133–140`) —
  `m_AllowBodyTurn=true; m_Turn=NONE; m_Priority=DESIRABLE;`.

**Остановка движения.** Нет отдельного «stop»-интента. Движение — накопитель:
- `IntentMove(angle, speed)` (`dmAISurvivor.c:1089`) накапливает вклад (векторное среднее +
  max скорости), НЕ зовёт `SetMove` напрямую.
- `ResetIntentMove()` (1100) обнуляет накопитель в начале тика (зовётся `OnUpdate:659`).
- `HandleOnMove()` (1108): **нет вкладов → авто-стоп** `m_Pawn.SetMove(0.0, 0.0)` (1113–1118).
- Поэтому «остановиться» = **не слать движение в тик** (не добавлять MoveTo/IntentMove) — авто-стоп
  сам погасит. Это и есть штатный механизм: FSM-состояния Patrol/Exploration/Idle двигаются через
  `dmBotIntent_MoveTo` (см. ниже), а их отсутствие/`Finish()` останавливает бота без отдельного «стоп».

Интенты, ведущие движение в мирных состояниях:
- `dmBotState_Patrol.StartMoveToCurrent` (`dmBotState_Patrol.c:97–103`) — `new dmBotIntent_MoveTo(); m_Goal=...; AddFSMIntent`.
- `dmBotState_Exploration.StartMoveTo` (`dmBotState_Exploration.c:653–660`) и `EnsureCorpse` (577–588) — MoveTo DESIRABLE.
- `dmBotState_Hunting.EnsureMove` (`dmBotState_Hunting.c:156–207`) — MoveTo DESIRABLE (точка разброса/здание).
- `dmBotState_Idle` — движения не шлёт (только LOOK-скан).

Корректное «погасить» движение, не ломая авто-стоп: **не вносить MOVE-вкладов в тик** (для
личного интента взгляда достаточно канала LOOK и отсутствия MOVE). Если же нужно прервать
активный MoveTo — у него есть `Finish()`/`Fail()` (см. `dmBotState_Exploration.OnExit:713–718`),
а сам пул подчистит finished/expired в `dmBotIntentPool.Tick` (31–45). Остановка НЕ требует
гасить `HandleOnMove` — он идемпотентен (нет вкладов → SetMove(0,0)).

## 5. Тик мозга `dmAISurvivor.OnUpdate` и FSM

`OnUpdate(float pDt)` (`dmAISurvivor.c:520–688`), точный порядок:
1. `m_Pawn == null` → return (526); смерть → `OnDeath()` (529–533); автоперсист (536–546).
2. Тик кулдауна мили (549–550); `IsUnconscious` → return (556–557); детект моря (561–566).
3. Таймер держания в руках + FreeHands (570–591); детект restrained/untie (593–627);
   пробуждение/вернуть оружие (638–648).
4. `m_Vision.Update(this, pDt)` (650–652) — **зрение**: discovery-scan (1 Гц) + per-target
   LOS (каждый тик) + aggro. **Слух — НЕ здесь** (подписка через `dmNoiseSystem.SI_OnNoiseAdded`
   в конструкторе `dmHearing`, `dmHearing.c:11`; вызов `HearNoise` приходит из `OnNoise`
   синхронно в момент `AddNoise`, т.е. в произвольный момент, не в этот тик).
5. `m_Needs.Update` (654), `m_Arsenal.Tick` (656).
6. `ResetIntentMove()` (659) — сброс накопителя движения.
7. `m_FSM.Update(pDt)` (661–662) — тик FSM (может сделать переход → `ClearFSMIntents`).
8. Пересоздание persistent-интентов TidyInventory (667–671) и EatDrink (675–679).
9. `UpdateEvade()` (681); `UpdateIntents(pDt)` (682) — арбитраж; `HandleOnMove()` (683) —
   применение движения; `GetInventoryFrames().Tick()` (684); `GetAiming().OnUpdate` (685–686);
   `UpdateLook(pDt)` (687).

**Где вставить «мгновенное решение» (свежая цель vs концентрация):** после FSM-тика и
`ResetIntentMove`, но до `UpdateIntents`/`HandleOnMove` — оптимально сразу после `m_FSM.Update`
(между 662 и 667) или в начале секции интентов (перед 681). Тогда личный интент взгляда,
добавленный здесь через `AddPersonalityIntent`, попадёт в арбитраж того же тика (682) и
отработает в `UpdateLook` (687) в тот же кадр; остановка реализуется тем, что в этот тик не
вносится MOVE-вклад (авто-стоп в 683). Слух сам по себе приходит асинхронно (`OnNoise` →
`HearNoise`), поэтому «решение» следует делать по факту `m_LastContact`/свежести цели в
`OnUpdate`, а не в момент `HearNoise` (так решение не потеряется, если шум пришёл между тиками).

**Как мозг узнаёт текущее состояние FSM:** `dmAISurvivor.GetFSM()` (`dmAISurvivor.c:747`) →
`dmBotFSM.GetCurrentState()` (`dmBotFSM.c:60`) → `dmBotState.GetName()` (`dmBotState.c:42`).
Готового `GetCurrentStateName()` на мозге НЕТ (тесты собирают строку вручную: `dmE2EBridge.c:2002–2004`,
`dmTestSuite.c:163–165`). Поля/метода «ест/пьёт» на мозге тоже нет — см. ниже.

Полный список классов состояний (директория `src/core/4_World/Entities/Bot/States/`):
`Idle`, `Patrol`, `Stealth`, `Exploration`, `Fighting`, `Shooting`, `Flanking`, `Kamping`,
`Rushing`, `MedicalCare`, `Hunting`, `Travel`, `Follow`, `GetWater`, `Surrender`.

Регистрация состояний в пресетах (имена = строка в `AddState`):
- **Survivor** (`dmBotPreset_Survivor.c:20–29`): Idle, Fighting, Shooting, Flanking, Kamping,
  Rushing, Exploration, Stealth, MedicalCare, GetWater. **Hunting НЕ зарегистрирован.**
- Nomad (`dmBotPreset_Nomad.c:20–29`): + Travel (без Hunting, без Patrol, без Follow).
- Hunter (`dmBotPreset_Hunter.c:21–26`): Idle, Hunting, Stealth, Fighting, Surrender, MedicalCare.
- Escort (`dmBotPreset_Escort.c`): + Follow (без Hunting).
- Combat (`dmBotPreset_Combat.c`): только боевые + Idle + MedicalCare.
- Patrol подключается только в тестах (`dmTestCommand.c:395/444`, `dmCommandContext.c:43`).

**«Ест/пьёт» vs «лечится» (для концентрации):**
- «Лечится» = состояние FSM `MedicalCare` (PREEMPTIVE, `dmBotState_MedicalCare.c:30–33`), читается
  по имени состояния. Это чистый дискриминатор концентрации.
- «Ест/пьёт» — **НЕ состояние FSM**, а персональный NONE-интент `dmBotIntent_EatDrink`
  (`dmBotIntent_EatDrink.c`), persistent (пересоздаётся в `OnUpdate:675–679`). Занят когда
  `IsBusy()` → `m_Phase != 0` (`dmBotIntent_EatDrink.c:28–31`), т.е. в фазах open/eat/stop.
  Ссылка `m_EatDrinkIntent` приватная, геттера НЕТ. Для концентрации нужно либо добавить
  `dmAISurvivor.IsEatingOrDrinking()` (обёртка `m_EatDrinkIntent && m_EatDrinkIntent.IsBusy()`),
  либо `[нужно проверить]` использовать `pawn.GetCommandModifier_Action() != null` (eat/drink
  анимация — `AddCommandModifier_Action` в `dmBotIntent_EatDrink.c:268`), но это ловит и другие
  action-модификаторы.

## 6. Удаление/забывание целей

`dmAISurvivor.ForgetStaleTargets(float timeout)` (`dmAISurvivor.c:2209–2222`):
```
float now = GetGame().GetTickTime();
for (i = Count()-1; i >= 0; i--) {
    if (t.m_LastContact == 0) t.m_LastContact = now;   // 2217 (дефолт-контакт)
    if (!t.m_Entity || now - t.m_LastContact > timeout || !t.m_Entity.IsAlive())
        m_Targets.RemoveItem(t);                         // 2219-2220
}
```
Вызывается **только** из `dmVision.UpdateLOS` в конце (`dmVision.c:213`) с константой
`DM_TARGET_FORGET_TIME = 300.0` (`src/cons/4_World/constants.c:646`). «Протухание» = 300 с без
`m_LastContact` (контакт обновляется и слухом `HearNoise:1971`, и зрением `UpdateLOS:209`,
и уроном/прицелом). Мёртвая сущность удаляется безусловно (2219). Других точек удаления нет
(кроме `RemoveTarget`/`ClearTargets` для ручной очистки).

**Важно для А7:** цель, которую забрала Hunting, при 300-с «протухании» удалится, если бот её
не слышит/не видит. Если нужно держать охотничью цель дольше (или исключить из удаления
невиденной цели) — добавить в `ForgetStaleTargets` исключение по флагу (напр. `m_ClaimedByHunting`)
либо обновлять `m_LastContact` в Hunting.

## 7. Hunting — как цель вызывает переход

`dmBotState_Hunting` (`dmBotState_Hunting.c`):
- `CanEnter()` (23–26) = `GetOwner().GetHuntTarget() != null`.
- `GetHuntTarget()` (`dmAISurvivor.c:2152–2174`): первая невидимая (`!IsVisible`), не-friendly,
  живая цель с `IsValidTarget()` (есть сущность И позиция != Zero) И (`m_Threat >= 0.5` ИЛИ
  `m_Attractiveness > DM_HUNT_MIN_ATTRACTIVENESS=0.5`).
- Переход в Hunting: в пресетах Hunter/тестах `idle.AddTransition(hunting, ...)` через
  `CanEnter()` (Hunter: `dmBotPreset_Hunter.c:29`); **в Survivor/Nomad/Escort Hunting не входит
  в FSM вообще** — там «услышал шум» (threat 0.4, ниже порога атаки) сегодня НЕ приводит ни к
  какому поведению (подтверждает исходную постановку фичи).
- «Цель для преследования» внутри Hunting: `ResolveTarget` (121–146) держит `m_Target` =
  `GetHuntTarget()`, `m_SearchCenter = m_Target.LastPosition()`; EXIT при `IsVisible()`
  (83–89) или появлении hostile (73–80).
- Отметки «цель забрана Hunting» НЕТ — Hunting просто держит свою копию `m_Target` в поле
  состояния; внешних маркеров на `dmTarget` нет.

---

## Точки внесения изменений (атомы)

### А1 — флаг ever-seen
- Добавить поле в `dmTarget` (`dmTarget.c`, рядом с 26): `bool m_EverSeen = false;`.
- Выставить `true` в пути LOS-успеха `dmVision.UpdateLOS` (`dmVision.c:204–210`, в ветке
  `if (t.m_HasLOS)`), плюс при необходимости в `DiscoverTarget`/`HearNoise` остаётся `false`.
- Сигнатура `HasLOS` возвращает bool и уже обёрнута в `t.m_HasLOS = ...` (204) — идеальная точка.
- `[нужно подтвердить]` вопрос эмпирически: «происходит ли первый `m_HasLOS=true` только после
  реального райкаст-успеха (а не по шуму)?» — probe `botdump` цели после `HearNoise` должен
  показать `m_EverSeen=false`, после первого визуального контакта `true`.

### А2 — аттракшен по типу/дистанции в HearNoise
- Пробросить `type` из `dmHearing.OnNoise` (`dmHearing.c:15`) в `HearNoise`: сейчас сигнатура
  `HearNoise(entity, position, attractiveness)` (`dmAISurvivor.c:1960`) теряет `type`; либо
  добавить 4-й параметр `int type`, либо закодировать в attractiveness (уже частично: SHOT/
  BULLETIMPACT=0.9 vs прочее=0.6, `dmHearing.c:59–61`).
- В `HearNoise` (`dmAISurvivor.c:1960–1997`) выставлять новое поле `m_Attraction`/`m_LastNoise`
  по типу (STEP/SCREAM/BULLETIMPACT/SHOT) и дистанции (дистанция доступна: `d.LengthSq()` уже
  вычисляется в `OnNoise:29`, но в `HearNoise` позиция передаётся — дистанцию можно пересчитать
  из `GetPosition()` и `position`, строки 1977–1978).
- Используемые константы: `DM_NOISE_THREAT=0.4` (`constants.c:53`), `DM_NOISE_ATTRACTIVENESS_NOISE/SHOT`
  (67–68).

### А3 — затухание аттракшена (−0.05/10с)
- Место: в `OnUpdate` (после `m_FSM.Update`, `dmAISurvivor.c:662`), пробежать `m_Targets` и
  убывать новое поле по `pDt` (аналог существующего `RecalcTargetThreat`, `dmAISurvivor.c:1796–1831`,
  который уже вызывается из `dmVision.Update:37–38` с условиями по `m_HasLOS`/`m_LastContact`).
- Либо расширить `RecalcTargetThreat` (он уже вызывается для «свежих» целей) вторым полем.
- `[нужно подтвердить]`: корректный период затухания — уточнить, что `pDt` в `OnUpdate` ≈
  0.033 с (`DM_BOT_TICK_INTERVAL`, `constants.c:147`), а `RecalcTargetThreat` получает тот же `pDt`.

### А4 — GetConcentration
- Добавить `float GetConcentration()` (или `int ConcentrationLevel()`) на `dmAISurvivor`
  (рядом с `GetFSM`, `dmAISurvivor.c:747`). Внутри: прочитать `GetFSM().GetCurrentState().GetName()`
  (`dmBotFSM.c:60` + `dmBotState.c:42`) и сверить с множеством «занятых» состояний
  (`MedicalCare`, `Fighting`, `Shooting`, `Flanking`, `Kamping`, `Rushing` → высокая концентрация;
  `Idle`, `Patrol`, `Exploration`, `Travel`, `Stealth` → низкая). «Ест/пьёт» — через новый геттер
  `IsEatingOrDrinking()` (обёртка `m_EatDrinkIntent.IsBusy()`, поле объявлено `dmAISurvivor.c:121`,
  интент `dmBotIntent_EatDrink.c:28–31`).
- `[нужно проверить]`: список состояний и их градация концентрации — нужен эмпирический прогон
  (probe: переключение FSM-состояний через `/fsm` и чтение концентрации через botdump).

### А5 — интенты + решение в тике
- Новые личные интенты (или переиспользование существующих): «повернуться» — `dmBotIntent_Turn`
  (`m_Angle` отн., `m_Turn=FULL` через `OnStart`); «остановиться и смотреть» — `dmBotIntent_HoldLook`
  (`m_Target`/`m_Point` + `m_Turn`), длительность 5–15 с от аттракшена — через `m_Deadline`
  (базовый `IsExpired`, `dmBotIntent.c:120–125`) или явный `Finish()` по таймеру из `OnUpdate`.
- Точка вставки решения: `OnUpdate` между `m_FSM.Update` (662) и `UpdateIntents` (682) — тогда
  `AddPersonalityIntent(...)` (1610) участвует в арбитраже того же тика; LOOK-канал интент
  пере-ассертит каждый тик (обязательно, см. комментарий `dmBotIntent_LookAround.c:3–7`).
- Остановка: НЕ слать MOVE-вклад в тик (нет активного MoveTo) — авто-стоп `HandleOnMove`
  (`dmAISurvivor.c:1108–1123`); при необходимости прервать активный MoveTo через `Finish()`
  (пример `dmBotState_Exploration.OnExit:713–718`).
- Арбитражные константы/канал: LOOK (`dmBotIntent.c:28–38`), приоритеты IDLE/DESIRABLE/CRITICAL
  (8–13); personality-пул арбитражится первым (`dmAISurvivor.c:2270`).
- `[нужно подтвердить]`: «свежая цель» — признак свежести (сравнить `now - t.m_LastContact`
  с окном, напр. `DM_IMMINENT_CONTACT_WINDOW=30` (`constants.c:733`), или новое поле `m_LastNoise`).

---

## Открытые вопросы / гипотезы для эмпирической проверки

1. `[нужно подтвердить]` Эмитит ли `ZombieBase.OnSoundVoiceEvent` (`modded_ZombieBase.c:16`)
   крик при атаке зомби на ИГРОКА (не бота), или только при агро на бота? Влияет на то,
   слышит ли бот «чужой бой» (feature-желаемое, но не подтверждённое). Probe: спавн зомби
   (`CreateObject(..., init_ai=true)`) + игрок рядом → `botdump` бота рядом.
2. `[нужно проверить]` Нужен ли отдельный тип шума EXPLOSION (сейчас enum `dmNoiseType`
   без него, `dmNoiseSystem.c:9–16`); генератора взрывов в `src` нет — если фича требует
   реакцию на взрыв, надо добавить enum-значение + генератор (например в `modded_Explosion*`).
3. `[нужно проверить]` Длительность интента 5–15 с от аттракшена: где хранить таймер — поле
   `m_Deadline` (базовое) vs отдельный `m_Attraction`-таймаут на `dmTarget`. Эмпирический прогон:
   подать шум → замерить время взгляда (botdump/лог) и убедиться в границах 5–15 с.
4. `[нужно подтвердить]` Градация концентрации по состояниям FSM (А4): подтвердить списком
   состояний на реальном боте (probe `/fsm` + `botdump`).
5. `[нужно проверить]` Порядок: приходит ли `HearNoise` синхронно внутри `AddNoise` (не в
   `OnUpdate`) — да (`dmNoiseSystem.c:30` → `SI_OnNoiseAdded.Invoke`), значит «мгновенное
   решение» надёжнее делать в следующем `OnUpdate` по свежести цели, а не в самом `HearNoise`.
