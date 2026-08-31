# Research: бой (мили / огнестрел)

Статус: мили-заглушка (`dmBotMeleeFightLogic_LightHeavy`). Задачи T10/T11/T12 плана.
Ведёт субагент `dayz-research`.

## Цель

Реальный бой: мили против заражённых, огнестрел против игроков (прицел/стрельба/
перезарядка), состояние `Fighting`.

## Известные стартовые точки

- **Мили-заглушка** (`dmAISurvivorBase`): `m_MeleeFightLogic = new dmBotMeleeFightLogic_LightHeavy(this)`
  (ванильный `DayZPlayerMeleeFightLogic_LightHeavy.HandleFightLogic` null-дерефит `hcm =
  GetCommand_Move()`; у AI-бота `CanFight()` всегда true → VM Exception вне MOVE-команды).
  Сейчас заглушка возвращает `false` (не дерётся).
- `Weapon_Base`, `WeaponManager` (`GetWeaponManager()`), прицел/выстрел — ванильный
  `HumanInputController.OverrideAimChangeX/Y` НЕ двигает голову ИИ (клиентский путь мыши).
- `HasNoAmmo()` в мозге — заглушка `false`; нужна инспекция магазина (`Magazine.GetAmmoCount()`,
  `Weapon_Base.GetMagazine(int)`).

---

# Мили

Целевая модель — **«магия без райкастов»**: бот попадает по зомби/животным в 100%.
Нужно лишь (а) бот видит цель и развёрнут корпусом к ней (±15°), (б) дистанция ≤
досягаемости оружия, (в) триггер (враг близко + кулдаун). Урон наносится нативом по
прямому выбору цели, а НЕ по `RaycastRV`.

Все пути ниже подтверждены чтением ванили (`/home/devalio/dayz/Work/DayZ Projects/scripts/`)
и Expansion (`/home/devalio/dayz/Work/DayZ-Expansion-Scripts/DayZExpansion/`).

## 1. `DayZPlayerImplementMeleeCombat` — задание цели

Файл: `4_world/entities/dayzplayerimplementmeleecombat.c`.

Класс `DayZPlayerImplementMeleeCombat` (L19). Инстанцируется в `DayZPlayerImplement`
(`4_world/entities/dayzplayerimplement.c:178` — `m_MeleeCombat = new DayZPlayerImplementMeleeCombat(this)`),
геттер `GetMeleeCombat()` (L244).

### Публичные сеттеры цели (можно звать из подкласса)

| Метод | Строка | Что делает |
|---|---|---|
| `SetTargetObject(Object)` | L156 | пишет `m_TargetObject` |
| `SetHitPos(vector)` | L172 | пишет `m_HitPositionWS` |
| `SetHitZoneIdx(int)` | L146 | пишет `m_HitZoneIdx` (component index) |
| `SetFinisherType(int)` | L182 | пишет `m_FinisherType` |
| `GetTargetEntity()` | L151 | `EntityAI.Cast(m_TargetObject)` |
| `GetHitPos()` / `GetHitZoneIdx()` / `GetFinisherType()` / `GetWeaponMode()` | L167/162/177/187 | геттеры |
| `Reset(weapon, hitMask, wasHitEvent)` / `ResetTarget()` | L192/208 | сброс/реинит |

`SetTarget(Object obj, vector hitPos, int hitZone)` — **protected** (L519); единственное
отличие от публичных сеттеров: он ещё вычисляет `m_HitZoneName =
obj.GetDamageZoneNameByComponentIndex(hitZone)` (L526).

### `TargetSelection()` / `Update()` — override

- `TargetSelection()` — **protected** (L334), переопределяем. Ваниль делает райкаст:
  1-й проход `HitZoneSelectionRaycastHelper` → `HitZoneSelectionRaycast` (L535/544,
  `DayZPhysics.RaycastRV`, отдаёт component index `hitZone`), 2-й проход конус
  `MeleeTargeting` (дорогой box-collide, L393), 3-й проход non-alignable.
- `Update(weapon, hitMask, wasHitEvent)` — public (L220), **не помечен** `final`; Expansion
  переопределяет как `override void Update(...)` (`eAIMeleeCombat.c:200`) → виртуальный.
- **Ключевая готча сервера**: в ванили `Update()` вызывает `TargetSelection()` ТОЛЬКО под
  `#ifndef SERVER` (L224–245). На сервере `Update()` = просто `Reset()`. Поэтому у
  серверного ИИ `TargetSelection` не зовётся ванилью — нужно override `Update()` (как
  `eAIMeleeCombat.c:200-218`: всегда `Reset` → `TargetSelection()` → `SetFinisherType`).
- **Готовый «магический» инъектор цели уже есть в ванили для игроков**: клиент сериализует
  цель в `ScriptInputUserData` (`INPUT_UDT_MELEE_TARGET`, L237-243), сервер читает в
  `DayZPlayerImplement.OnInputUserDataProcess` (`dayzplayerimplement.c:2996-3033`) и зовёт
  ровно `SetTargetObject/SetHitPos/SetHitZoneIdx/SetFinisherType`. Это и есть паттерн
  «задать цель напрямую без райкаста» — серверная цель для игрока вводится из сети, а не
  вычисляется сервером. Для бота мы делаем то же самое локально.

### Как `TargetSelection`/`Update` связаны с выбором цели

Порядок в ванили (клиент): `Update()` → `Reset()` → `TargetSelection()` → `SetFinisherType()`
→ отправка цели в пакет. В `HandleFightLogic` перед ударом зовётся
`m_MeleeCombat.Update(itemInHands, m_HitType)` (цель выбирается ДО `StartCommand_Melee2`),
а при событии Hit — `m_MeleeCombat.Update(itemInHands, m_HitType, true)` (ре-таргет, см. §2).

## 2. `StartCommand_Melee2(...)` и куда наносится урон

Сигнатура (натив), `3_game/human.c:1457`:

```c
proto native HumanCommandMelee2 StartCommand_Melee2(EntityAI pTarget, int pHitType, float pComboValue, vector hitPos = vector.Zero);
```

- `pTarget` — целевой EntityAI (может быть `null`).
- `pHitType` — **`HumanCommandMelee2.HIT_TYPE_*`** (`human.c:541-544`): `LIGHT=0`,
  `HEAVY=1`, `FINISHER=2` (liver stab), `FINISHER_NECK=3`. **НЕ путать с `EMeleeHitType`**
  (`dayzplayerimplementmeleecombat.c:1-17`, LIGHT=0/HEAVY=1/SPRINT=2/KICK=3/WPN_*=7..10).
  Fight-logic передаёт `m_HitType == EMeleeHitType.HEAVY` (bool→0/1, совпадает) и
  `DetermineFinisherAnimation()` (маппит FINISHER_* → HIT_TYPE_* 2/3, L899-915).
- `pComboValue` — это `attackByDistance` из `GetAttackTypeByDistanceToTarget`
  (`dayzplayermeleefightlogic_lightheavy.c:120-135`): `1.0` = удар на месте (цель ближе
  `CLOSE_TARGET_DISTANCE = 1.5` м, L13), `0.0` = удар с шагом вперёд (цель дальше).
- `hitPos` — мировая точка попадания (для ориентации анимации; урон пересчитывает позицию).

`HumanCommandMelee2` (нативы, `human.c:536-566`): `ContinueCombo(bool pHeavyHit, float pComboValue, EntityAI target = null, vector hitPos = vector.Zero)` (L547), `IsInComboRange()` (L550), `WasHit()` (L553, true ровно один раз после anim-события Hit), `Cancel()` (L556), `GetComboCount()` (L561), `GetCurrentHitType()` (L563), `IsFinisher()` (L565).

**Куда наносится урон**: на anim-событие Hit `WasHit()` становится true → `HandleHitEvent`
(`dayzplayermeleefightlogic_lightheavy.c:282-334`): если finisher == -1 — ре-таргет
`m_MeleeCombat.Update(itemInHands, m_HitType, true)` (L299, враг мог уйти), затем
`EvaluateHit(itemInHands)` (L303). Урон берётся из **текущего** `m_MeleeCombat.GetTargetEntity()`
+ `GetHitZoneIdx()` + `GetWeaponMode()` (см. `EvaluateHit_Common`, L660-703), а НЕ из
`pTarget`, переданного в `StartCommand_Melee2`. Т.е. цель можно пере-выставить перед Hit —
но при «магии» цель одна и та же.

`EvaluateHit_Common` применяет урон либо `m_Player.ProcessMeleeHit(weapon, weaponMode, target, hitZoneIdx, hitPosWS)` (L687), либо `DamageSystem.CloseCombatDamage(...)` (L682) для сломанного оружия/финишера.

## 3. Точка вызова fight-logic и путь Expansion

**Ваниль**: `4_world/entities/dayzplayerimplement.c:2642-2649`, внутри
`override void CommandHandler(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)`
(L2271), после throwing, до `ModCommandHandlerAfter`:

```c
if (m_MeleeFightLogic.CanFight())
{
    if (m_MeleeFightLogic.HandleFightLogic(pCurrentCommandID, hic, entityInHands, m_MovementState, m_ContinueFirearmMelee))
    {
        m_ProcessFirearmMeleeHit = isWeapon && (hic.IsMeleeWeaponAttack() || m_ProcessFirearmMeleeHit) || m_ContinueFirearmMelee;
        return;
    }
}
```

`hic` = `HumanInputController`. `m_MeleeFightLogic` создаётся в `dayzplayerimplement.c:179`
(`new DayZPlayerMeleeFightLogic_LightHeavy(this)`), геттер `GetMeleeFightLogic()` (L249).
`CanFight()` (`dayzplayermeleefightlogic_lightheavy.c:73-82`) у ИИ всегда `true`
(нет `ActionManager`, нет эмоута) — отсюда VM Exception у не-null-safe ванили.

**Expansion путь (FSM-тик → флаг → CommandHandler → HandleFightLogic → StartCommand_Melee2)**:

1. FSM `eAIState_Fighting_Melee.OnUpdate` (`Classes/FSM/states/fighting/eaistate_fighting_melee.c:112`)
   зовёт `unit.Notify_Melee()`.
2. `Notify_Melee(bool melee = true)` (`Entities/AI/eAIBase.c:4406-4418`) ставит
   `m_eAI_MeleeFightLogic.m_eAI_Melee = melee` (L4417) и поднимает `m_eAI_IsPreparingMelee`.
3. `eAIMeleeFightLogic_LightHeavy.HandleFightLogic` (override, `Classes/Melee/eaimeleefightlogic_lightheavy.c:52-249`)
   гейтится `m_AI.m_eAI_IsFightingFSM` (L56) и `m_eAI_Melee` (L193); выбирает вариант
   (`HandleInitialMeleeErc` / `HandleInitialFirearmMelee` / `HandleProneKick` /
   `HandleSprintAttack` / `HandleComboHit`), каждый из которых кончается
   `m_AI.StartCommand_Melee2(target, ..., attackByDistance, m_MeleeCombat.GetHitPos())`.
4. `m_eAI_IsFightingFSM` ставится `eAI_SetIsFightingFSM(bool)` (`eAIBase.c:8703`) при
   входе/выходе из fighting-FSM. После успешного старта — `eAI_SkipMelee(...)` (`eAIBase.c:2120`,
   флаг `state.m_SkipMelee` на eAITarget) как «кулдаун», `eAI_UpdateAttackCooldown()` и
   `m_eAI_MeleeTime` (`eAIBase.c:4420-4425`, в `OnCommandMelee2Start`).

Итого для botorama: наш подкласс fight-logic полностью контролирует `HandleFightLogic` —
ни `pInputs` (кнопки), ни райкаст не нужны; нужен только флаг «хочу ударить» из FSM/мозга.

## 4. Raised-стойка и совместимость с нашей `ApplyStance`

- Стойки (`3_game/dayzplayer.c:619-629`): `STANCEIDX_ERECT/CROUCH/PRONE/RAISEDERECT/RAISEDCROUCH/RAISEDPRONE/RAISED`
  (RAISED — маска-смещение: `ERECT + RAISED = RAISEDERECT`).
- `HumanCommandMove.ForceStance(int pStanceIdx)` — натив (`3_game/human.c:479`), принимает
  и `STANCEIDX_RAISEDERECT`, и `-1` (снять лок стойки).
- Ванильный `HandleFightLogic` (lightheavy, L164-189) **требует** `pMovementState.m_iStanceIdx
  == STANCEIDX_RAISEDERECT` для `HandleInitialMeleeErc` (L175), `RAISEDPRONE` для кика (L180),
  `ERECT + IsSprintFull()` для спринт-атаки (L185). Без raised-стойки ваниль НЕ начнёт
  light/heavy punch с предметом/голыми руками.
- `HandleInitialMeleeErc` сам по себе (L386-449) **не проверяет стойку** — проверка в
  `HandleFightLogic`. Метод лишь: `m_HitType = GetAttackTypeFromInputs()` → `Update` →
  `GetTargetData` → `StartCommand_Melee2`.

**Expansion-подход (эталон для нас)**: они НЕ заводят отдельный stance-index raised.
`eAI_GetStance()` (`eAIBase.c:5840-5848`) нормализует стойку (`-= STANCEIDX_RAISED`), а
«поднятое оружие» держат отдельным флагом `m_WeaponRaised` + таймером: `RaiseWeapon(bool up)`
(`eAIBase.c:9728-9733`), `IsRaised()` → `m_WeaponRaised` (`L9746-9749`),
`IsWeaponRaiseCompleted()` → `m_WeaponRaisedTimer > 0.5` (`L9751-9754`). Проверка в
fight-logic: `stance == STANCEIDX_ERECT && m_AI.IsRaised()` (`eaimeleefightlogic_lightheavy.c:212`),
а визуальное «поднятие» гонится аним-граф-переменной `m_ExpansionST.m_VAR_Raised`
(`AnimSetBool`, `eAIBase.c:9742`) — т.е. **не** `ForceStance(RAISEDERECT)`.

**Вывод для botorama** — два пути:
- **(A) Без raised вовсе (рекомендуется для «магии»)**: мы переопределяем
  `HandleFightLogic` целиком и напрямую зовём `StartCommand_Melee2` из ERECT/CROUCH, минуя
  ванильную проверку стойки. Требуется проверить, что анимация punch играется из
  non-raised MOVE-команды (команда MELEE2 — отдельная, скорее всего да).
- **(B) Vanilla-совместимо**: перед ударом `ForceStance(STANCEIDX_RAISEDERECT)`, как ваниль.
  Наша `ApplyStance` (`dmAISurvivorBase.c:441-477`) сейчас нормализует `m_iStanceIdx -= RAISED`
  (L449-450) и гоняет только erect/crouch/prone — придётся учить её не ломать raised и не
  сбрасывать его каждый тик. Риск: raised-поза для голых рук требует ванильного
  `HumanCommandWeapons`-модификатора прицела; для bare-hand ваниль обходится
  `ForceStance(RAISEDERECT)` (см. §5). Не проверено на нашем кастомном графе.

## 5. Голые руки (light/heavy punch без предмета)

Подтверждено: путь без `itemInHands` полностью рабочий.

- `HandleInitialMeleeErc` берёт `itemInHands` только чтобы передать в `Update` → `SelectWeaponMode`.
  `SelectWeaponMode(null)` → ветка «bare hand» (`dayzplayerimplementmeleecombat.c:301-310`):
  `HEAVY → 1`, `SPRINT → 2`, иначе `0` (light). `GetWeaponRange(null, mode)` →
  `m_DZPlayer.GetMeleeCombatData().GetModeRange(mode)` (L313-319) — у голых рук валидная
  досягаемость из `DayZPlayer.GetMeleeCombatData()` (`3_game/dayzplayer.c:1270`; `MeleeCombatData`
  и `GetModeRange(int)` в `3_game/gameplay.c:160-168`).
- `m_HitType`: `GetAttackTypeFromInputs` (`dayzplayermeleefightlogic_lightheavy.c:90-98`) →
  `HEAVY` если `IsMeleeFastAttackModifier() && CanConsumeStamina(MELEE_HEAVY)`, иначе `LIGHT`.
  Для ИИ override (эталон `eAIMeleeFightLogic_LightHeavy.c:32-40`): `HEAVY` если
  `CanConsumeStamina(EStaminaConsumers.MELEE_HEAVY)`, иначе `LIGHT` (вход `pInputs` игнорим).
- Урон: `EvaluateHit_Common` с `weapon == null` → `WeaponDestroyedCheck(null)` = false →
  `ProcessMeleeHit(null, weaponMode, target, hitZoneIdx, hitPosWS)` (L687). `ProcessMeleeHit`
  натив (`dayzplayer.c:1273`): `(InventoryItem pMeleeWeapon, int pMeleeModeIndex, Object pTarget, int pComponentIndex, vector pHitWorldPos)`. Есть также `ProcessMeleeHitName(...)`
  (`dayzplayer.c:1275`) с `string pComponentName` — **имя** компонента вместо индекса.

**Предмет в руках НЕ обязателен** — удар по зомби голыми руками валиден. `isMeleeWeapon`
флаг из конфига (`inventoryitem.c:38`), `IsMeleeWeapon()` (`inventoryitem.c:81`), но
`HandleInitialMeleeErc` не требует предмета.

## 6. Кулдаун / длительность удара

- Команда `COMMANDID_MELEE2` живёт до конца анимации → `OnCommandMelee2Finish`
  (`4_world/entities/manbase/playerbase.c:4108-4114`) → `RunFightBlendTimer()` (L5406) →
  через `PlayerConstants.MELEE2_MOVEMENT_BLEND_DELAY = 0.35` с (`3_game/playerconstants.c:4`)
  → `EndFighting()` → `m_IsFighting = false` (L5416-5419).
- `OnCommandMelee2Start` ставит `m_IsFighting = true` (L4098-4106). `IsFighting()` → `m_IsFighting`
  (L5382-5385).
- Комбо: пока `COMMANDID_MELEE2` активен и `GetCommand_Melee2().IsInComboRange()` →
  `HandleComboHit` → `ContinueCombo(...)`. Иначе новый удар стартует из MOVE.

**Как понять, что можно бить снова**: `pCurrentCommandID != COMMANDID_MELEE2` (вернулись в
MOVE) или `!IsFighting()`. `WasHit()` — одноразовый флаг события Hit (для ре-таргета/урона),
не для кулдауна. Ваниль жёстко не блокирует повторный удар (вне меле-команды кнопка просто
стартует заново); для бота достаточно СВОЕГО кулдауна в FSM (напр. `m_eAI_MeleeTime` +
пауза, как `eAI_SkipMelee`/`eAI_UpdateAttackCooldown`). Достаточно своего кулдауна; ваниль
дополнительно ограничивает только комбо-цепочку и стамину (`CanConsumeStamina`).

## Схема для botorama (что писать / override / звать)

Два подкласса (как у Expansion), оба — в `core/4_World/Entities/Bot/`:

**A. `dmBotMeleeCombat : DayZPlayerImplementMeleeCombat`** — «магия» без райкаста.
- `override void Update(InventoryItem weapon, EMeleeHitType hitMask, bool wasHitEvent = false)`:
  вызвать `super.Update(...)` (на сервере = `Reset`), затем `TargetSelection()` и
  `SetFinisherType(-1)` (эталон `eAIMeleeCombat.c:200-218`, но без `m_AI.SetOrientation`).
- `override protected void TargetSelection()`: **без райкаста** — взять цель из мозга
  (`dmAISurvivor`), `InternalResetTarget()`, затем `SetTargetObject(target)` +
  `SetHitPos(позиция)` + `SetHitZoneIdx(<валидный component index или -1>)` +
  `SetFinisherType(-1)`. Если index неизвестен — использовать `-1` и переопределить
  `EvaluateHit`/`EvaluateHit_Common` (см. B), т.к. `GetTargetData` обнуляет цель при
  `hitZoneIdx < 0` (lightheavy L750-753), а урон идёт по `hitZoneIdx >= 0`.

**B. `dmBotMeleeFightLogic_LightHeavy : DayZPlayerMeleeFightLogic_LightHeavy`** (заменить
текущую заглушку, `dmAISurvivorBase.c:547-554`).
- `override bool HandleFightLogic(pCurrentCommandID, pInputs, pEntityInHands, pMovementState, out pContinueAttack)`:
  1) null-check `hcm = GetCommand_Move()` и `hcm` (ванильная null-дереф — причина заглушки);
  2) если нет флага «атаковать» (из мозга) и нет цели → `return false`;
  3) `HandleHitEvent(pCurrentCommandID, pInputs, itemInHands, pMovementState, pContinueAttack)` (урон на Hit);
  4) если `pCurrentCommandID == COMMANDID_MOVE` → установить `m_HitType` (HEAVY если
     `CanConsumeStamina(MELEE_HEAVY)`, иначе LIGHT), `m_MeleeCombat.Update(itemInHands, m_HitType)`,
     затем `m_Player.StartCommand_Melee2(target, m_HitType == HEAVY, 1.0, m_MeleeCombat.GetHitPos())`,
     `DepleteStamina(...)`, вернуть `true`. (Эталон — `eaimeleefightlogic_lightheavy.c:193-249`
     + ванильный `HandleInitialMeleeErc` L386-449, но БЕЗ проверки raised-стойки.)
- `override protected EMeleeHitType GetAttackTypeFromInputs(HumanInputController pInputs)` →
  `HEAVY` при стамине, иначе `LIGHT` (эталон `eAIMeleeFightLogic_LightHeavy.c:32-40`).
- **Если «магия» без component index**: `override protected void EvaluateHit(InventoryItem weapon)` —
  применить урон напрямую через `ProcessMeleeHitName(weapon, GetWeaponMode(), target, <имя компонента>, hitPosWS)` либо `DamageSystem.CloseCombatDamageName(...)`, минуя требование индекса.
  Имя компонента — `target.GetDefaultHitPositionComponent()` (или `GetDefaultHitComponent()`,
  `entityai.c:3792`; для `DayZInfectedType` — `dayzinfectedtype.c:141/136`).

**Условия триггера (в мозге/FSM)**: враг близко (`dist <= GetRange()`, см. `eAI_GetRange()`
`eAIMeleeCombat.c:332`; ванильный `GetRange()` = `m_WeaponRange + 0.65`, L321), цель видна
(LOS) и корпус развёрнут к ней (±15°), кулдаун прошёл (`!IsFighting()` и/или таймер мозга).
`GetRange()` — protected в `DayZPlayerImplementMeleeCombat`; для мозга добавить публичную
обёртку (`eAI_GetRange()` эталон).

### Открытые вопросы / риски (не проверено)

1. **Анимация punch из non-raised MOVE-команды** (путь A): играет ли `StartCommand_Melee2`
   из обычного ERECT на кастомном графе (`player_main.agr`) без raised-модификатора.
   Ваниль требует `RAISEDERECT` только на уровне `HandleFightLogic`; сама команда MELEE2 —
   отдельная. Риск: анимация не стартует или сломается бленд — нужен тест.
2. **Component index для урона**: если не делать райкаст и не переопределять `EvaluateHit`,
   нужно валидное `hitZoneIdx >= 0`. Натива «имя зоны → index» НЕ нашлось
   (есть только `GetDamageZoneNameByComponentIndex(index)`, `object.c:1160`). Обход —
   `ProcessMeleeHitName`/`CloseCombatDamageName` по имени (проверен факт наличия нативов, не
   проверен фактический урон по зомби).
3. **Финишеры не нужны** для «магии»: `SetFinisherType(-1)` + `GetFinisherType() == -1`
   отключает ветку finisher (`EvaluateFinisherAttack` не сработает). Не проверено поведение
   при `target.CanBeBackstabbed()` у зомби без нашего контроля.
4. **Стамина**: `DepleteStamina(MELEE_HEAVY/LIGHT)` — для голых рук/зомби Expansion отменяет
   потребление (`eAIBase.c:9703-9714`). Решить, деплеить ли стамину боту (иначе «магия» бесконечна).
5. **Поворот корпуса к цели** перед ударом: наша `ApplyBodyTurn`/`SetOrientation` — убедиться,
   что ±15° доворот завершён до `StartCommand_Melee2`, иначе анимация бьёт мимо (визуально).
6. **`CanFight()` у ИИ** возвращает `true` (нет `ActionManager`) — ванильный
   `HandleFightLogic` гейтится этим; наша override-версия должна сама гейтить по
   состоянию тела (`CanAct()`/unconscious), чтобы не звать `StartCommand_Melee2` в мёртвом/нокдауне.

## Огнестрел

Статус: исследование API завершено (прицел/выстрел/перезарядка/патроны/урон). Реализация —
состояние `Shooting` + примитивы в пешке (TODO, см. «Минимальный путь»).

Все сигнатуры подтверждены чтением ванили (`/home/devalio/dayz/Work/DayZ Projects/scripts/`)
и Expansion (`/home/devalio/dayz/Work/DayZ-Expansion-Scripts/DayZExpansion/`). Пути — `файл:строка`.

### Анимация поднятия оружия (raise)

#### Чем реально запускается (Raised graph var vs команда/стостента)

**`Raised` (graph var) — engine-driven, скриптом НЕ ставится.** Он является проекцией
**raised-стойки** (`m_iStanceIdx >= STANCEIDX_RAISEDERECT`) на аним-граф. Цепочка:

- `HumanMovementState.IsRaised()` = `m_iStanceIdx >= DayZPlayerConstants.STANCEIDX_RAISEDERECT`
  (`3_game/human.c:1163-1166`). Т.е. «поднятое оружие» в ванили — это СТОЙКА (raised stance), а не
  отдельный флаг.
- `IsFireWeaponRaised()` = `m_MovementState.IsRaised()` (та же стойка)
  (`4_world/entities/dayzplayerimplement.c:309-316`).
- Стойки: `STANCEIDX_ERECT/CROUCH/PRONE/RAISEDERECT/RAISEDCROUCH/RAISEDPRONE/RAISED`
  (`3_game/dayzplayer.c:619-629`), причём `RAISEDERECT = ERECT + RAISED` (сдвиг на 3).
- В графе `Raised` — это отдельный bool (`botorama/Animations/player_main.agr:92
  #Var Raised bool 0`), который ДВИЖОК пишет из фактической стойки: `Stance` (int 0/1/2) +
  `Raised` (bool = стойка в диапазоне RAISED*). Все «raised»-позы графа (`*Ras`: `ErcRas`/`CroRas`/
  `PneRas`) выбираются по `Raised`; raise/lower-бленд — по `HasVariableChanged(Raised, ...)`
  (`botorama/Animations/Locomotion.agr:3639-3648`, бленд-время из `RaiseTime`/`LowerTime` = 0.4).
- Скриптовый `AnimSetBool(индекс"Raised", ...)` — **no-op**: движок каждый кадр перезаписывает
  `Raised` из фактической стойки move-команды. Подтверждение: ванильный класс, который биндит и
  пишет `Raised`/`ADS`/`AimX`/`AimY`, — это `HumanST` — НЕ входит в «DayZ-Script-Diff» (он в
  движковом скрипт-модуле), а `proto native AnimSetBool` (`3_game/dayzplayer.c:1233`) в ванили
  НЕ вызывается нигде в дифе для `Raised`.

**Ванильный триггер raise** — смена стойки на raised: `HumanCommandMove.ForceStance(int)`
(`3_game/human.c:479`). Пример из ванили: `hcm.ForceStance(DayZPlayerConstants.STANCEIDX_RAISEDERECT)`
(`4_world/entities/manbase/dayzplayer/dayzplayermeleefightlogic_lightheavy.c:248, 376`) —
ровно так персонаж «поднимает ствол к плечу». При входе в raised-стойку движок ставит
`Raised=true` → проигрывается raise-бленд. Т.е. НЕ `HumanCommandWeapons`-команда и НЕ стостента
«RAISEDERECT» в графе, а именно `ForceStance(RAISEDERECT)` move-команды.

Вывод: наша `ApplyWeaponRaise()` (`dmAISurvivorBase.c:194-205`) делает `AnimSetBool(m_VarRaised,
m_WeaponRaised)` по индексу `BindVariableBool("Raised")` (`:130`) — это ничего не меняет, т.к.
`Raised` engine-driven, а фактическая стойка бота никогда не raised (наша `ApplyStance` намеренно
обнуляет raised-сдвиг: `if (current >= STANCEIDX_RAISED) current -= STANCEIDX_RAISED;`,
`dmAISurvivorBase.c:632-633`).

#### Как Expansion вшивает eAI_Raised в граф

Expansion НЕ использует ванильный `Raised` (по той же причине) и НЕ использует
`ForceStance(RAISEDERECT)` для raise. Вместо этого — отдельная кастомная переменная + кастомный граф:

- Бинд: `m_VAR_Raised = hai.BindVariableBool("eAI_Raised")`
  (`Core/Scripts/.../Classes/Commands/ExpansionHumanST.c:142`; под `#ifdef EXPANSIONMODAI`).
  Рядом — кастомные `eAI_AimX`/`eAI_AimY` (`:139-140`) и `eAI_Look*`.
- Запись: `AnimSetBool(m_ExpansionST.m_VAR_Raised, m_WeaponRaised)` в `eAI_HandleWeapons`
  (`AI/Scripts/.../Entities/AI/eAIBase.c:9536`), при опускании — `hcw.SetADS(false)` (`:9541`) и
  `AnimSetBool(..., false)` (`:9742` в `eAI_ResetRaised`). Дублирующий set при re-raise после
  `StartCommand_MoveAI` (`:4375`).
- Флаг+таймер: `m_WeaponRaised`/`m_WeaponRaisedPrev`/`m_WeaponRaisedTimer` (`:256-259`),
  `RaiseWeapon(bool)` (`:9728`), `override IsRaised()` → `m_WeaponRaised` (`:9746`),
  `override IsWeaponRaiseCompleted()` → `m_WeaponRaisedTimer > 0.5` (`:9751`), `CanRaiseWeapon()`
  (`:9716`).
- **Сам граф Expansion НЕ в репо** (только ссылка `graphName="DayZExpansion\Animations\AI\player_main.agr"`
  в `Animations/AI/config.cpp:55`). Т.е. точные переходы/бленды `eAI_Raised` в их `.agr` прочитать
  нельзя — но паттерн однозначен: кастомный bool, добавленный в их кастомный граф, и поднятие
  сделан его движущей переменной вместо ванильного `Raised`.
- ADS у них тоже engine-driven: `AnimSetBool(m_VAR_ADS, ads)` ЗАКОММЕНТИРОВАН (`eAIBase.c:9606`),
  вместо него — натив `HumanCommandWeapons.SetADS(bool)` (`eAIBase.c:9608`; натив
  `3_game/human.c:1029`). Аналогично aim-углы — кастомные `eAI_AimX/Y` (`eAIBase.c:9587-9588`),
  а НЕ ванильные `AimX/AimY`.

#### Минимальный рецепт для botorama (вариант а/б, точные переходы)

**Вариант (б) — ванильный путь через raised-стойку (РЕКОМЕНДУЕТСЯ, минимум кода).**
Граф botorama уже содержит полную raised-машину (`*Ras`-позы, `Raised`-бленды с
`HasVariableChanged(Raised, ...)` в `botorama/Animations/Locomotion.agr`) — нужно лишь дать
движку реально войти в raised-стойку, и `Raised`/raise-бленд сработают сами:

1. В `ApplyStance()` (`dmAISurvivorBase.c:624-660`) при `m_WeaponRaised == true` форсить
   `STANCEIDX_RAISEDERECT/CROUCH/PRONE` (базовая стойка + `STANCEIDX_RAISED`), а не 0/1/2:
   ```c
   int desired = m_DesiredStance;
   if (m_WeaponRaised && desired < DayZPlayerConstants.STANCEIDX_RAISED)
       desired += DayZPlayerConstants.STANCEIDX_RAISED;      // ERECT→RAISEDERECT и т.д.
   ```
   и убрать/учредить нормализацию `if (current >= STANCEIDX_RAISED) current -= STANCEIDX_RAISED;`
   (`:632-633`) так, чтобы raised-стойка не «обнулялась» обратно. Шаг erect↔prone через crouch
   (`:648-652`) применять к БАЗОВОЙ части, сохраняя raised-сдвиг.
2. `ApplyWeaponRaise()` (`:194-205`) — убрать `AnimSetBool(m_VarRaised, ...)` (no-op); оставить
   только таймер `m_WeaponRaisedTimer` для `IsWeaponRaiseCompleted()`. `BindVariableBool("Raised")`
   (`:130`) больше не нужен (или оставить безвредным).
3. `IsRaised()`/`IsWeaponRaiseCompleted()` (`:171-180`) — оставить как есть (флаг+таймер), они
   гейтят `TryFireWeapon`, а не анимацию.

Итог: `RaiseWeapon(true)` → `ApplyStance` форсит `ForceStance(RAISEDERECT)` → движок пишет
`Raised=true` → raise-бленд (~0.4 c из `RaiseTime`) проигрывается → `IsWeaponRaiseCompleted()`
становится `true` через 0.5 c. Опускание симметрично (`ForceStance(базовая)`).

**Вариант (а) — кастомная `dmAI_Raised` (Expansion-стиль, дороже).** Добавить
`#Var dmAI_Raised bool 0 ""` в `player_main.agr` `$Vars`, биндить `BindVariableBool("dmAI_Raised")`
и писать `AnimSetBool`. НО граф должен РЕАГИРОВАТЬ на неё: заменить `Raised` на `dmAI_Raised`
(или добавить `|| dmAI_Raised`) во всех raise/lower-блендах `Locomotion.agr`
(`ErcIdleRasG`/`CroIdleRasG`/`PneIdleRasG`, `ErcToCroT`/`CroToErcT`/`ErcToPneT`/`CroToPneT`,
`HasVariableChanged(Raised, ...)`-переходы `:3639-3648` и т.д.). Минус: ванильные `Weapons.agr`/
`Combat.agr` (включены в `player_main.agr` `$Files`) по-прежнему смотрят на `Raised`
(`(Stance == 2 && AimX >= -50 && AimX <= 50 && Raised) || (!Raised && Stance == 2)` в
`DZ/.../Weapons.agr:4` и damage-позы в `Combat.agr`) — их raise-варианты не подхватятся, если
не патчить и ванильные условия. Поэтому (б) полнее и проще.

#### Риски

- **Конфликт с `ApplyStance`/движением**: raised-стойка меняет лимиты скорости и ротацию
  (`dayzplayercfgbase.c:114/121` — RAISEDERECT разрешает IDLE/WALK/RUN/SPRINT и ROTATION_ENABLE);
  наша `ApplyMovement`/`ApplyBodyTurn`/`HeadingModel` должны учесть raised-стойку, иначе бленд
  «raised idle» будет бороться с `ONE_FRAME`-override движения. Expansion избежал raised-стойки
  именно ради контроля над движением — если у нас она сломает движение, откатиться на вариант (а).
- **Ванильный `HandleWeaponFire`/`HandleWeapons`** (`dayzplayerimplement.c:1148-1195`) активируется
  по `m_MovementState.IsRaised()`. С raised-стойкой этот код начнёт тикать; у бота нет
  `pInputs.IsAttackButton()`, поэтому автовыстрела быть не должно, но это стоит проверить и, при
  необходимости, не вызывать его у ИИ (или держать `IsRaised()`-override флагом).
- **`SetADS`**: уже корректно через натив `hcw.SetADS(bool)` (`dmAISurvivorBase.c:287`,
  `ApplyWeaponAim`); ADS-позы (`ErcADSPose`/`CroADSPose`/`PneADSPose`) завязаны на engine-driven
  `ADS` — продолжат работать.
- **`AimX`/`AimY` (аналогичная готча, отдельная задача)**: наши `AnimSetFloat("AimX"/"AimY")`
  (`dmAISurvivorBase.c:281-283`) скорее всего ТОЖЕ no-op (ванильные `AimX/AimY` engine-driven,
  Expansion потому и вводит `eAI_AimX/Y`). Направление ствола для `Fire()`-натива берётся из
  `GetWeaponAimDirection()` (`:268-273`) и от аним-переменных не зависит, но ВИЗУАЛЬНЫЙ наклон
  ствола к цели может не проигрываться — нужен кастомный `dmAI_AimX/Y` (отдельная задача).
- **HandIK/ArmIK**: `ArmIK`/`WeaponIK` (`AnimNodeWeaponIK` в `Actions.agr`, `ArmIK` var
  `player_main.agr:115`) включаются движком для raised-стойки (`SetIKStance(RAISEDERECT, ...)` в
  `dayzplayercfgbase.c:146-191`) — при входе в raised-стойку IK подхватится сам; при «ручном»
  варианте (а) IK-позу придётся включать вручную, иначе руки «сломаны».

### Цель/прицел (raise/aim) — API + Expansion-эталон

**Ваниль (клиентский путь, для ИИ НЕ работает)**:
- Мышь → `HumanInputController.OverrideAimChangeX/Y(HumanInputControllerOverrideType, float)`
  (`3_game/human.c:240/243`) → угол прицела `HumanCommandWeapons`. Это клиентский путь —
  голову/прицел ИИ на сервере НЕ двигает (подтверждено ранее).
- Raised-стойка: `DayZPlayerConstants.STANCEIDX_RAISEDERECT/CROUCH/PRONE/RAISED`
  (`3_game/dayzplayer.c:619-629`), `HumanCommandMove.ForceStance(int)` (`3_game/human.c:479`).
  `IsRaised()` = `m_MovementState.IsRaised()` (`dayzplayerimplement.c:313`);
  `IsWeaponRaiseCompleted()` = `m_WeaponRaiseCompleted` (`dayzplayerimplement.c:926-929`,
  ставится `CompleteWeaponRaise()` L911).
- `HumanCommandWeapons` (`3_game/human.c:999-1119`): `SetADS(bool)` L1029,
  `StartAction(WeaponActions, int)` L1017 (анимация перезарядки/механизма),
  `GetRunningAction/GetRunningActionType/IsActionFinished` L1008/1011/1005,
  `GetBaseAimingAngleUD/LR` L1095/1098, `GetAimingHandsOffsetUD/LR` L1044/1047,
  `IsInWeaponReloadBulletSwitchState` L1026.
- `Weapon_Base.GetCameraPoint(int muzzleIndex, out vector pos, out vector dir)` (`weapon.c:413`) —
  точка/направление «глаза» для выстрела (то, что использует `TryFireWeapon`-натив).

**Expansion-эталон (серверный ИИ, то, что нужно воспроизвести)** — отдельный флаг + таймер,
НЕ raised-стойка:
- `eAIBase`: `m_WeaponRaised`/`m_WeaponRaisedPrev`/`m_WeaponRaisedTimer` (`eAIBase.c:256-259`).
  `RaiseWeapon(bool up=true)` L9728 (ставит флаг; при опускании `m_eAI_QueuedShots = 0`).
  `override IsRaised()` → `m_WeaponRaised` L9746. `override IsWeaponRaiseCompleted()` →
  `m_WeaponRaisedTimer > 0.5` L9751. `CanRaiseWeapon()` L9716 (гейт: не climb/fall/swim/ladder/
  side-step-vehicle).
- Поднятие анимацией: `AnimSetBool(m_ExpansionST.m_VAR_Raised, m_WeaponRaised)` в
  `eAI_HandleWeapons` L9536; при опускании `hcw.SetADS(false)` L9541.
- Прицел: `m_eAI_AimRelAngles` (интерполированные rel-углы) через `eAI_InterpolateYawPitch`
  L9557; в аним-граф пишутся `AnimSetFloat(m_VAR_AimX/AimY, …)` L9587-9588; ADS —
  `hcw.SetADS(ads)` L9608 (по дистанции/оптике).
- Источник направления прицела: `eAI_OnWeaponAimUpdate()` L7045 → `GetAimingProfile().Update()`
  → `GetAimDirection()` (кость `neck` → aim-позиция + рандом точности) → `m_eAI_AimRelAngleLR/UD`
  L7052-7053. `GetWeaponAimDirection()` L9967 = `Vector(m_eAI_AimRelAngleLR, m_eAI_AimRelAngleUD, 0).AnglesToVector()`
  (направление ствола в мире). `GetAimDirection()` L9956, `GetAimPosition()` L9983.
- `override AimingModel(...)` возвращает `false` L9993-9996 — ванильная aiming-модель у ИИ
  отключена; прицел целиком гонится аним-переменными + `SetADS`.
- `eAIAimingProfile.Update()` (`Classes/Weapons/eAIAimingProfile.c:16`): точность по дистанции/
  оптике/взрывным, для зомби/животных — 100% попадание.

**Вывод для botorama**: куда указывает ствол = НЕ голова (`SetLookYaw/Pitch`), а отдельный
направление прицела, которое для `Fire()`-натива передаётся явно (`dir`). Серверный ИИ держит
его как `m_eAI_AimRelAngleLR/UD` (из кости neck → точка цели) и визуально — через кастомные
аним-переменные `AimX/AimY` + `Raised` + `HumanCommandWeapons.SetADS`. `ForceStance(RAISEDERECT)`
для этого НЕ нужен.

### Выстрел (fire) — API + эталон

**Ваниль (сигнатуры)**:
- `proto native bool Fire(int muzzleIndex, vector pos, vector dir, vector speed)` (`weapon.c:58`) —
  НИЗКОУРОВНЕВЫЙ натив: пустить пулю из `pos` в направлении `dir` со скоростью `speed`.
- `proto native bool TryFireWeapon(EntityAI weapon, int muzzleIndex)` (`3_game/systems/inventory/weaponinventory.c:8`) —
  ГЛОБАЛЬНЫЙ натив, которым ванильный weapon-FSM реально стреляет (сам берёт `GetCameraPoint`,
  спавнит пулю, наносит урон).
- `proto native bool CanFire(int muzzleIndex)` (`weapon.c:57`); скриптовый `CanFire()` без аргумента
  (`weapon_base.c:1268`): `!IsChamberEmpty && !IsChamberFiredOut && !IsJammed && !m_LiftWeapon && !IsDamageDestroyed`.
- `WeaponManager.Fire(Weapon_Base wpn)` (`weaponmanager.c:485-502`): проверяет `IsChamberFiredOut/
  IsJammed/IsChamberEmpty` → `JamCheck(0)` → `wpn.ProcessWeaponEvent(new WeaponEventTrigger(player))`
  (или `WeaponEventTriggerToJam`). **Это «правильный» спуск**: событие → weapon-FSM → `TryFireWeapon`.
- `WeaponManager.CanFire(Weapon_Base)` (`weaponmanager.c:79-88`): in-hands + `!IsLiftWeapon` +
  `IsRaised` + `!IsDamageDestroyed` + `!IsProcessing` + `IsWeaponRaiseCompleted` + `!IsFighting` +
  `!wpn.IsCoolDown()`.
- Ванильный flow (клиент): `HandleWeaponFire` (`dayzplayerimplement.c:1148-1195`) →
  `GetWeaponManager().CanFire(weapon)` → `Fire(weapon)`. `HandleWeapons` вызывается из
  `CommandHandler` (`dayzplayerimplement.c:2324`), `HandleWeapons` L1009.
- Темп/гейт: weapon-FSM fire-состояния (`fsm/states/weaponfire.c`: `WeaponFire` L44,
  `WeaponFireWithEject` L112, `WeaponFireMultiMuzzle` L135, `WeaponFireToJam` L279,
  `WeaponFireAndChamberNext` L329) вызывают `TryFireWeapon(m_weapon, mi)` на `OnEntry`, а повторный
  выстрел гейтится `GetReloadTime(muzzleIndex)` (`weapon.c:247`) → `WeaponEventReloadTimeout`.

**Expansion-эталон**:
- `eAIBase.TryFireWeapon()` L1112: гейты `m_eAI_MinTimeTillNextFire` (L1118), `eAI_CanFire(weapon)`
  (L1125), `eAI_HasLOS()` (L1128); очередь выстрелов `m_eAI_QueuedShots` для burst/fullauto
  (L1131-1159); сам выстрел — **`GetWeaponManager().Fire(weapon)` L1165**; каденция —
  `GetReloadTime()*1000` или 200–300 мс (L1171-1173).
- `eAI_CanFire(weapon)` L1263: `!IsClimbing/IsFalling/IsFighting/IsSwimming`, `!IsProcessing`,
  `IsRaised()`, `weapon.CanFire()`, `!GetWeaponManager().IsRunning()`, `weapon.CanFire(mi)`.
- Expansion **подменяет** ванильные fire-состояния (`0_DayZExpansion_AI_Preload/.../weaponfire.c`):
  вместо `TryFireWeapon` зовут `m_weapon.eAI_Fire(mi, p)`; `WeaponFireAndChamberNext.OnUpdate`
  использует `GetCurrentModeAutoFire` вместо `hic.IsAttackButton()`.
- `Weapon_Base.eAI_Fire(int muzzleIndex, eAIBase ai)` (`AI/.../Entities/Weapons/Firearms/Weapon_Base.c:60`,
  SERVER-ветка): серверный hitscan `Hitscan(...)` (L32/37, `DayZPhysics.RaycastRV(..., ObjIntersectFire, 0.01)`),
  компенсация падения пули, затем **`return Fire(muzzleIndex, pos, dir, dir)` L157** (pos = кость
  neck + dir*0.2, dir = `ai.GetWeaponAimDirection()`). Клиентский fallback — `eAI_FireOnClient`
  (`TryFireWeapon`) + `eAI_FireWeaponOnClient` (RPC, L1176).
- `eAI_SelectFireMode` L1245, `eAI_SetFireModeAuto` (`Weapon_Base.c:621`).

### Перезарядка (reload) — API + эталон

**Ваниль (сигнатуры)**:
- `WeaponManager.AttachMagazine(mag)` / `SwapMagazine(mag)` / `DetachMagazine(il)` /
  `LoadBullet(mag)` / `Unjam()` / `EjectBullet()` (`weaponmanager.c:398/408/403/423/438/443`).
  Все ведут в `StartAction(action, mag, il, control_action)` L772.
- **Критично**: `StartAction` без `control_action` на сервере+мультиплеере возвращает `false`
  (`weaponmanager.c:789-790`) — ванильный путь рассчитан на клиентский ввод. С `control_action != null`
  сразу `StartPendingAction()` (L783-786) → `PostWeaponEvent(...)` (L821+).
- `WeaponManager.IsRunning()` = `m_InProgress` (L874); `Update(deltaT)` (L889) тикает
  `StartPendingAction` (по `m_readyToStart`) и завершение по `m_WeaponInHand.IsIdle()` (L957).
- `DayZPlayerInventory.PostWeaponEvent(e)` — однослотовый `m_DeferredWeaponEvent` (`dayzplayerinventory.c:303`);
  `HandleWeaponEvents(dt, out exitIronSights)` L345 обрабатывает его через `weapon.ProcessWeaponEvent`
  (L416-432). Обработка — только внутри `CommandHandler`/`HandleWeapons`.
- Reload-FSM состояния: `weaponattachmagazine.c`, `weaponreplacingmagandchambernext.c`,
  `weapondetachingmag.c` и т.д.; анимация — `HumanCommandWeapons.StartAction(WeaponActions, int)`
  (`fsm/states/weaponstartaction.c:18-28`).

**Expansion-эталон**:
- `eAIWeaponManager : WeaponManager` (`AI/.../Classes/Weapons/eAIWeaponManager.c:15`): `override
  StartAction` L17 (на сервере НЕ возвращает false — ставит `m_readyToStart=true`), `override
  StartPendingAction` L47 (постит `WeaponEventAttachMagazine/SwapMagazine/DetachMagazine/…`),
  `PostWeaponEvent` L112 (обёртка с проверкой `m_DeferredWeaponEvent`).
- `eAIBase.ReloadWeaponAI(EntityAI weapon, EntityAI magazine)` L8825 — полная последовательность:
  1) `CanUnjam` → return (не перезаряжаем заклинившее); 2) `IsChamberFiredOut && internal mag > 0
  && CanEjectBullet` → `EjectBullet()`; 3) `CanAttachMagazine` → `AttachMagazine(mag)`; 4) `CanSwapMagazine`
  → `SwapMagazine(mag)`; 5) `CanDetachMagazine` → `DetachMagazine(il)` (с drop-локацией).
- FSM-состояния перезарядки: `eAIState_Weapon_Reloading` (guard `eaistate_weapon_reloading.c:9`),
  `_Start`, `_Removing`, `_Reloading` (`eaistate_weapon_reloading_reloading.c:15` → `unit.ReloadWeaponAI`),
  `_Fail`. Завершение: `GetWeaponManager().IsRunning() || GetActionManager().GetRunningAction()` = false.
- `eAI_HasAmmoForFirearm(gun, out mag, checkMagsInInventory=true)` L6619 — ищет непустой магазин/
  пачку под оружие в инвентаре.

### HasNoAmmo (инспекция магазина) — точный путь

Заглушку `HasNoAmmo() → false` заменить на:

```c
Weapon_Base wpn = Weapon_Base.Cast(GetHumanInventory().GetEntityInHands());
if (!wpn) return true;
int mi = wpn.GetCurrentMuzzle();
bool chamberLive = !wpn.IsChamberEmpty(mi) && !wpn.IsChamberFiredOut(mi);
Magazine mag = wpn.GetMagazine(mi);                 // null у внутреннего магазина
bool magAmmo  = mag ? (mag.GetAmmoCount() > 0)
                    : (wpn.GetInternalMagazineCartridgeCount(mi) > 0);
return !chamberLive && !magAmmo;
```

Сигнатуры:
- `Magazine.GetAmmoCount()` — натив (`4_world/entities/itembase/magazine/magazine.c:68`);
  `GetAmmoMax()` — скрипт (L158, config `count`); `ServerSetAmmoCount(int)` L70;
  `ServerAcquireCartridge(out dmg, out type)` L80; `IsAmmoPile()` (пачка/луз).
- `Weapon_Base.GetMagazine(int muzzleIndex)` — натив (`weapon.c:240`).
- `Weapon_Base.IsChamberEmpty/IsChamberFiredOut/IsChamberFull/IsChamberJammed(int)` (`weapon.c:74/79/96/84`).
- `Weapon_Base.HasInternalMagazine(int)` (`weapon.c:106`), `GetInternalMagazineCartridgeCount(int)`
  (`weapon.c:111`), `GetTotalCartridgeCount(int)` (`weapon.c:127`).
- В ванили **НЕТ** `GetAmmoTotal`/`IsMagazineEmpty`/`HasAmmo` — это сторонние/Expansion-хелперы.
  Expansion: `Expansion_HasAmmo(out Magazine)` (`Core/.../Weapon_Base.c:412`),
  `Expansion_GetMagazineAmmoCount(int, out Magazine)` L434, `Expansion_IsChambered(int)` L404.

### Попадание/урон — сигнатуры

- Пуля — настоящий физический снаряд, симулируется на **сервере** движком (не скриптом). `Fire()`/
  `TryFireWeapon()`-нативы спавнят её; попадание и урон резолвятся внутри движка, скрипт получает
  только хуки:
  - `DayZGame.FirearmEffects(source, directHit, componentIndex, surface, pos, surfNormal, exitPos,
    inSpeed, outSpeed, isWater, deflected, ammoType)` (`3_game/dayzgame.c:3614`) — эффекты/шум, НЕ урон.
  - Урон по цели — стандартный конвейер: `EEOnDamageCalculated` / `EEHitBy` (у бота уже перехватывается
    для `RegisterDamageThreat`). То есть «бот попал/убил» видно через здоровье цели и `EEHitBy`.
- Для ИИ-hitscan/прицеливания: `DayZPhysics.RaycastRV(beg, end, out pos, out dir, out comp, results,
  ignoreObj, false, false, ObjIntersectFire, 0.01)` (эталон `Hitscan`, AI `Weapon_Base.c:46`).
- Взрывные боеприпасы: `DamageSystem.ExplosionDamage(...)` (`dayzgame.c:3651`); `Weapon_Base.ShootsExplosiveAmmo()`.

**Самопроверяемый тест** (сервер): дать боту ствол с магазином → `RaiseWeapon` → дождаться
`IsWeaponRaiseCompleted` → `Fire(weapon)` → проверить `GetAmmoCount()` упал и/или цель получила
`EEHitBy` (или health цели снизился). Пустой магазин → `HasNoAmmo` → `ReloadWeaponAI`.

### Минимальный путь для botorama (рекомендация: примитивы в пешке, стейт Shooting)

Повторяем УПРОЩЁННУЮ Expansion-схему (без client-FSM-desync и точности — сперва «магия», 100% попадание):

1. **Пешка (`dmAISurvivorBase`)** — примитивы:
   - `RaiseWeapon(bool)`: флаг `m_WeaponRaised` + таймер `m_WeaponRaisedTimer` + `AnimSetBool(dmAI_VAR_Raised, ...)`
     (добавить переменную в кастомный граф `player_main.agr`); override `IsRaised()`/`IsWeaponRaiseCompleted()`
     (таймер > 0.5с), `CanRaiseWeapon()` (гейт по команде/состоянию).
   - Прицел: направление ствола = `vector.Direction(GetBonePositionWS("neck"), aimPos)` (без рандома точности
     на первом этапе); пишем в `dmAI_AimX/AimY` + `hcw.SetADS(bool)`; для `Fire`-натива отдаём это `dir`.
   - `TryFireWeapon()`: гейт `IsRaised && IsWeaponRaiseCompleted && weapon.CanFire() && !GetWeaponManager().IsRunning()
     && LOS && cooldown` → `GetWeaponManager().Fire(weapon)` (из тика `CommandHandler`).
   - `ReloadWeaponAI(weapon)`: подкласс/override `WeaponManager.StartAction` (серверный путь
     `PostWeaponEvent`) + `AttachMagazine/SwapMagazine/DetachMagazine`; тикать `GetWeaponManager().Update(dt)`.
2. **Мозг/FSM** — состояние `dmBotState_Shooting` (PREEMPTIVE): вход при `GetHostileTarget()` +
   огнестрел в руках + патроны; `OnUpdate`: RaiseWeapon → wait raise → прицел по цели → fire (по кулдауну)
   → при `HasNoAmmo()` перезарядка; `EXIT` при пустой цели/нет патронов/магазинов.
3. **Что обязательно проверить (открытые вопросы)**:
   - `GetWeaponManager().Fire()`/`ProcessWeaponEvent` требуют вызова **внутри CommandHandler**; FSM-тик у нас
     уже из `TickAll` → `CommandHandler` — ок, но выстрел не должен вызываться из `OnUpdate` мозга напрямую
     (только через пешку-примитив, исполняемый в CommandHandler).
   - Клиентский sync оружейного FSM: для простоты сначала стрелять только на сервере (звук/эффекты на клиенте
     появятся через синхронизацию позиций/событий позже — см. `ProcessWeaponEvent` override в Expansion,
     `Weapon_Base.c:632`, `INPUT_UDT_WEAPON_REMOTE_EVENT`).
   - Аним-переменные `Raised`/`AimX`/`AimY` нужно ДОБАВИТЬ в кастомный `player_main.agr` (по правилу
     «кепить ванильные + добавить свои»), иначе поднятие/прицел не проиграются.
   - `GetReloadTime()`-каденция у оружия, затвор (`IsChamberFiredOut` → `EjectBullet`), `IsCoolDown()` —
     учесть в кулдауне выстрела.

### Звук/эффекты выстрела на клиенте (server AI fire → client sound)

#### Где играется звук выстрела

- Звук выстрела (мушка/дуло) + вспышка (muzzle flash) — **клиентский эффект**, играется
  нативом `TryFireWeapon(m_weapon, mi)` в `WeaponFire.OnEntry` ванильного weapon-FSM
  (`4_world/entities/firearms/fsm/states/weaponfire.c:66`). На сервере тот же `TryFireWeapon`
  только спавнит снаряд; частицы — в `Weapon_Base.EEFired` (`weapon_base.c:341`) под
  `!g_Game.IsDedicatedServer()` (т.е. только на не-dedicated клиенте/локальном сервере).
- Звук НЕ играется скриптом напрямую — его тянет натив `TryFireWeapon`. Вторичный путь
  для аним-событий `"SoundWeapon"` — `DayZPlayerImplement.ProcessWeaponEvent(string,string,int)`
  (`dayzplayerimplement.c:3414`), но это не основной shot-звук (это звуки механизма/перезарядки).

#### Как ваниль/Expansion синхронизируют (INPUT_UDT / RPC)

- **Ванильный путь (человек)**: клиент (LOCAL) → weapon-FSM `WeaponFire.OnEntry` →
  `TryFireWeapon` (звук+вспышка локально). Ввод уходит на сервер; серверный weapon-FSM
  `WeaponFire.OnEntry` → `TryFireWeapon` (спавн пули). Другим клиентам (REMOTE) сервер шлёт
  событие: `Weapon_Base.ProcessWeaponEvent` → `SyncEventToRemote(e)` (`weapon_base.c:1240-1255`) —
  пишет `INPUT_UDT_WEAPON_REMOTE_EVENT` + `e.WriteToContext(ctx)` в `ScriptRemoteInputUserData`
  (`3_game/gameplay.c:143`) и зовёт `p.StoreInputForRemotes(ctx)` (натив
  `DayZPlayer.StoreInputForRemotes`, `dayzplayer.c:1286`).
- **Корень бага (гейт)**: `SyncEventToRemote` шлёт событие только если
  `p.GetInstanceType() == INSTANCETYPE_SERVER` (`weapon_base.c:1243`). ИИ-бот —
  `INSTANCETYPE_AI_SERVER` (`dayzplayer.c:1076`), НЕ `INSTANCETYPE_SERVER` → событие НЕ
  уходит → клиент не запускает свой weapon-FSM → звука/вспышки нет.
- **Приём на клиенте**: `DayZPlayerImplement.OnInputForRemote` (`dayzplayerimplement.c:937-961`)
  → `INPUT_UDT_WEAPON_REMOTE_EVENT` → `DayZPlayerInventory.OnEventForRemoteWeapon(ctx)`
  (`dayzplayerinventory.c:2473-2509`) → `CreateWeaponEventFromContext` (`fsm/events.c:281`)
  → `wpn.ProcessWeaponEvent(e)` → weapon-FSM `WeaponFire` → `TryFireWeapon` → звук/вспышка.
  (Приём идёт из сетевого колбэка, НЕ из `CommandHandler` — это штатный путь для REMOTE-игроков.)
- **Expansion — два механизма**:
  1. `Weapon_Base.ProcessWeaponEvent` override (`AI/.../Weapons/Firearms/Weapon_Base.c:632-660`):
     если `e.m_player` — `eAIBase`, вручную пишет `INPUT_UDT_WEAPON_REMOTE_EVENT` +
     `e.WriteToContext(ctx)` + `ai.StoreInputForRemotes(ctx)`, затем `m_fsm.ProcessEvent(e)`
     (т.е. копирует `SyncEventToRemote` для AI-владельца, минуя гейт `INSTANCETYPE_SERVER`).
  2. RPC `eAI_FireWeaponOnClient` (`eAIBase.c:1176-1220`; зарегистрирован `eAIBase.c:569` как
     `RPC_eAI_FireWeaponOnClient`) — «hacky workaround for client FSM desync»: шлёт на клиент
     `muzzleIndex`/`shotID`; клиент зовёт `weapon.eAI_FireOnClient(muzzleIndex, this, shotID)`
     (`AI Weapon_Base.c:166`), который напрямую `TryFireWeapon(this, muzzleIndex)`; при фейле
     (десинк патронника) — `EEFired` + `eAI_PlayShotSound` (`AI Weapon_Base.c:247`) вручную
     (строит SoundObject из конфига `soundSetShot` + `AttenuateSoundIfNecessary`). Гейтится
     настройкой `OverrideClientWeaponFiring`; вызывается из `eAI_Fire` как
     `ai.eAI_FireWeaponOnClient(muzzleIndex, m_eAI_ShotID)` (`AI Weapon_Base.c:153`).

#### Минимальный путь для botorama (что добавить, сигнатуры, порядок)

Рекомендация: **override `SyncEventToRemote` в `modded class Weapon_Base`** — минимально,
зеркалит ваниль, не трогает логику FSM и не рискует double-send на клиенте:

```c
modded class Weapon_Base
{
    override void SyncEventToRemote(WeaponEventBase e)
    {
        DayZPlayer p = DayZPlayer.Cast(GetHierarchyParent());
        if (p && p.GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_AI_SERVER)
        {
            ScriptRemoteInputUserData ctx = new ScriptRemoteInputUserData();
            ctx.Write(INPUT_UDT_WEAPON_REMOTE_EVENT);
            e.WriteToContext(ctx);
            p.StoreInputForRemotes(ctx);
        }
        else
        {
            super.SyncEventToRemote(e);
        }
    }
}
```

Порядок (без доп. кода): серверный `TryFireWeapon` (`dmAISurvivorBase.c:764`) →
`wm.Fire(weapon)` → `ProcessWeaponEvent(WeaponEventTrigger)` → `SyncEventToRemote` (наш
override шлёт для `AI_SERVER`) → клиент `OnInputForRemote` → `OnEventForRemoteWeapon` →
`ProcessWeaponEvent` (клиентский weapon-FSM) → `WeaponFire.OnEntry` → `TryFireWeapon` →
звук+вспышка. Этот путь покрывает ВСЕ weapon-события (trigger/reload/eject/mechanism) —
заодно синхронизируются и анимации перезарядки.

Почему НЕ override `ProcessWeaponEvent` (Expansion-путь): работает, но на клиенте
`e.m_player` = AI_REMOTE тоже проходит `Class.CastTo` → ветка дёргает `StoreInputForRemotes`
на REMOTE-инстансе (лишний/потенциально вредный вызов). `SyncEventToRemote`-вариант чище:
на клиенте `GetInstanceType()` = `AI_REMOTE` → уходит в `super` (ваниль = no-op).

RPC-fallback (`eAI_FireWeaponOnClient` + `eAI_FireOnClient` + ручной `eAI_PlayShotSound`) —
**НЕ нужен на первом этапе** (нужен только при десинке патронника клиентского FSM). Завести
как TODO/фолбэк, если появятся случаи «пуля есть, звука нет» или наоборот.

#### Риски

- **Десинк звук/пуля**: если клиентский weapon-FSM рассинхронизирован с серверным патронником
  (`IsChamberEmpty/FiredOut` разошлись — напр. после drop/pickup, `RemoteObjectTreeDelete/Create`,
  netbubble выхода/входа), `TryFireWeapon` на клиенте вернёт `false` и звук не проиграется при
  существующей пуле. Mitigation (как Expansion): RPC-fallback `EEFired` + ручной звук +
  `Synchronize()` / `eAI_RemoteRecreate` (`AI Weapon_Base.c:207-237, 490-494`).
- **Двойной звук**: если позже добавить И override `SyncEventToRemote`, И RPC — будет два звука.
  Держать ОДИН путь синхронизации.
- **Приглушение звуков ИИ**: в чистом пути (`TryFireWeapon`) натив сам применяет затухание.
  Ручной путь (`eAI_PlayShotSound`) требует явного `eAI_AttenuateSoundIfNecessary(soundObject)`
  (`DayZPlayerImplement.c:647` → натив `AttenuateSoundIfNecessary`). Громкость/дистанция —
  из конфига `soundSetShot` оружия (nested arrays — из скрипта целиком НЕ читаются, см.
  комментарий `eAI_PlayShotSound` `AI Weapon_Base.c:249-250`).
- **Требование контекста**: `ProcessWeaponEvent`/`StoreInputForRemotes` должны зваться внутри
  `CommandHandler`/`HandleWeapons`. У botorama `TryFireWeapon` вызывается после
  `super.CommandHandler()` в `dmAISurvivorBase.CommandHandler` (`dmAISurvivorBase.c:352`) — ок.
  `StoreInputForRemotes` из тика мозга (вне CommandHandler) НЕ звать.
- **Глобальный `modded class Weapon_Base`**: override затронет ВСЕ оружия (в т.ч. игроков), но
  ветка `AI_SERVER` срабатывает только для ботов; для игроков — `super` (поведение ванили
  неизменно). Проверить отсутствие конфликта с другими модами, тоже override'ящими
  `SyncEventToRemote`/`ProcessWeaponEvent`.

### Сигнатуры (шпаргалка)

| Что | Сигнатура | Файл:строка |
|---|---|---|
| Спуск (натив, низкий) | `bool Fire(int muzzleIndex, vector pos, vector dir, vector speed)` | `weapon.c:58` |
| Спуск (ванильный FSM) | `proto native bool TryFireWeapon(EntityAI weapon, int muzzleIndex)` | `weaponinventory.c:8` |
| Спуск (правильный путь) | `WeaponManager.Fire(Weapon_Base)` / `CanFire(Weapon_Base)` | `weaponmanager.c:485/79` |
| Могу стрелять | `bool CanFire(int)` / `bool CanFire()` | `weapon.c:57` / `weapon_base.c:1268` |
| Патронник | `IsChamberEmpty/FiredOut/Full/Jammed(int)` | `weapon.c:74/79/96/84` |
| Магазин | `Magazine GetMagazine(int)` / `GetReloadTime(int)` / `GetCurrentMuzzle()` | `weapon.c:240/247/35` |
| Внутр. магазин | `HasInternalMagazine(int)` / `GetInternalMagazineCartridgeCount(int)` | `weapon.c:106/111` |
| Патроны в магазине | `Magazine.GetAmmoCount()` / `GetAmmoMax()` / `ServerSetAmmoCount(int)` | `magazine.c:68/158/70` |
| Перезарядка | `WeaponManager.AttachMagazine/SwapMagazine/DetachMagazine` | `weaponmanager.c:398/408/403` |
| Анимация перезарядки | `HumanCommandWeapons.StartAction(WeaponActions,int)` | `human.c:1017` |
| Поднять/опустить | `RaiseWeapon(bool)` / `IsRaised()` / `IsWeaponRaiseCompleted()` / `CanRaiseWeapon()` | `eAIBase.c:9728/9746/9751/9716` |
| Прицел (натив ADS) | `HumanCommandWeapons.SetADS(bool)` / `GetBaseAimingAngleLR/UD` | `human.c:1029/1098/1095` |
| Прицел (направление) | `GetWeaponAimDirection()` / `GetAimDirection()` / `GetAimPosition()` | `eAIBase.c:9967/9956/9983` |
| Выстрел ИИ (эталон) | `eAIBase.TryFireWeapon()` / `eAI_CanFire()` | `eAIBase.c:1112/1263` |
| Выстрел ИИ (натив+hitscan) | `Weapon_Base.eAI_Fire(int, eAIBase)` / `Hitscan(...)` | AI `Weapon_Base.c:60/37` |
| Перезарядка ИИ (эталон) | `eAIBase.ReloadWeaponAI(EntityAI, EntityAI)` | `eAIBase.c:8825` |
| WeaponManager серверный | `eAIWeaponManager.StartAction/StartPendingAction/PostWeaponEvent` | `eAIWeaponManager.c:17/47/112` |
| Патроны под оружие | `eAI_HasAmmoForFirearm(gun, out mag)` | `eAIBase.c:6619` |
| Хит/эффекты | `DayZGame.FirearmEffects(...)` (эффекты/шум, не урон) | `dayzgame.c:3614` |
| Урон по цели | стандартный `EEHitBy`/`EEOnDamageCalculated` (уже перехватывается) | — |
| Синк события выстрела | `Weapon_Base.SyncEventToRemote(WeaponEventBase)` (гейт `INSTANCETYPE_SERVER`) | `weapon_base.c:1240` |
| Синк события (натив) | `DayZPlayer.StoreInputForRemotes(ParamsWriteContext)` | `dayzplayer.c:1286` |
| Константа события | `INPUT_UDT_WEAPON_REMOTE_EVENT = 10` | `_constants.c:12` |
| Приём на клиенте | `DayZPlayerImplement.OnInputForRemote` / `DayZPlayerInventory.OnEventForRemoteWeapon` | `dayzplayerimplement.c:937` / `dayzplayerinventory.c:2473` |
| Override для ИИ (Expansion) | `Weapon_Base.ProcessWeaponEvent` (AI-ветка `StoreInputForRemotes`) | AI `Weapon_Base.c:632` |
| RPC-фолбэк (Expansion) | `eAIBase.eAI_FireWeaponOnClient` / `Weapon_Base.eAI_FireOnClient` / `eAI_PlayShotSound` | `eAIBase.c:1176` / AI `Weapon_Base.c:166/247` |

## Источники

- `DayZ Projects/scripts/4_world/entities/dayzplayerimplementmeleecombat.c` (цель/райкаст/Update)
- `DayZ Projects/scripts/4_world/entities/manbase/dayzplayer/dayzplayermeleefightlogic_lightheavy.c` (fight-logic, Handle*)
- `DayZ Projects/scripts/4_world/entities/dayzplayerimplement.c` (CommandHandler L2271/2642, OnInputUserDataProcess L2996, инстанцияция L178-179)
- `DayZ Projects/scripts/3_game/human.c` (StartCommand_Melee2 L1457, HumanCommandMelee2 L536, ForceStance L479)
- `DayZ Projects/scripts/3_game/dayzplayer.c` (стойки L619, ProcessMeleeHit/Name L1273/1275, GetMeleeCombatData L1270)
- `DayZ Projects/scripts/3_game/gameplay.c` (MeleeCombatData L160)
- `DayZ Projects/scripts/3_game/damagesystem.c` (CloseCombatDamage/Name L22/23, DamageType L10)
- `DayZ Projects/scripts/3_game/entities/object.c` (GetDamageZoneNameByComponentIndex L1160, ProcessDirectDamage L1134)
- `DayZ Projects/scripts/4_world/entities/manbase/playerbase.c` (OnCommandMelee2Start/Finish L4098/4108, IsFighting L5382, EndFighting L5416)
- `DayZ Projects/scripts/4_world/entities/firearms/weapon_base.c`, `.../itembase/magazine/magazine.c`
- Expansion: `AI/.../Classes/Melee/eAIMeleeCombat.c`, `AI/.../Classes/Melee/eaimeleefightlogic_lightheavy.c`,
  `AI/.../Entities/AI/eAIBase.c` (Notify_Melee L4406, eAI_SkipMelee L2120, RaiseWeapon L9728, IsRaised L9746, eAI_GetStance L5840),
  `AI/.../Classes/FSM/states/fighting/eaistate_fighting_melee.c`

**Огнестрел (см. раздел «Огнестрел» выше)**:
- `DayZ Projects/scripts/4_world/entities/core/inherited/weapon.c` (Fire L58, CanFire L57, GetMagazine L240, GetReloadTime L247, GetCameraPoint L413, chamber-нативы L74-96)
- `DayZ Projects/scripts/3_game/systems/inventory/weaponinventory.c` (TryFireWeapon L8)
- `DayZ Projects/scripts/4_world/classes/weapons/weaponmanager.c` (Fire L485, CanFire L79, Attach/Swap/DetachMagazine L398/408/403, StartAction L772, IsRunning L874, Update L889)
- `DayZ Projects/scripts/4_world/systems/inventory/dayzplayerinventory.c` (PostWeaponEvent L303, HandleWeaponEvents L345)
- `DayZ Projects/scripts/4_world/entities/firearms/fsm/states/weaponfire.c`, `.../weaponstartaction.c`
- `DayZ Projects/scripts/4_world/entities/itembase/magazine/magazine.c` (GetAmmoCount L68, GetAmmoMax L158, ServerSetAmmoCount L70, ServerAcquireCartridge L80)
- `DayZ Projects/scripts/3_game/dayzgame.c` (FirearmEffects L3614)
- Expansion: `AI/.../Entities/AI/eAIBase.c` (TryFireWeapon L1112, eAI_CanFire L1263, ReloadWeaponAI L8825, eAI_HasAmmoForFirearm L6619, eAI_HandleWeapons L9518, eAI_OnWeaponAimUpdate L7045, GetWeaponAimDirection L9967, AimingModel L9993),
  `AI/.../Entities/Weapons/Firearms/Weapon_Base.c` (eAI_Fire L60, Hitscan L37), `AI/.../Classes/Weapons/eAIWeaponManager.c` (StartAction L17, StartPendingAction L47, PostWeaponEvent L112),
  `AI/.../Classes/Weapons/eAIAimingProfile.c`, `AI/.../Classes/FSM/states/fighting/eaistate_fighting_fireweapon.c`, `.../eaistate_weapon_reloading*.c`,
  `0_DayZExpansion_AI_Preload/.../weaponfire.c`, `Core/.../Entities/Firearms/Weapon_Base.c` (Expansion_IsChambered L397, Expansion_HasAmmo L412)

---

# Дефект: «удар по поверхности» + урон в предмет (искры)

## Симптомы

1. Анимация удара всегда выглядит как «удар по твёрдой поверхности» — из-под ножа
   вылетают искры (не «мясной» удар).
2. Бот ударил игрока: игрок урона НЕ получил, но его винтовка стала сломанной — урон
   ушёл в предмет, а не в цель.

## Причина (корневая)

Корневая причина одна: **`SetHitPos()` получает позицию кости `Spine3` (грудь), а не
«чистую» точку тела, которую использует ваниль.**

- `dmBotMeleeCombat.TargetSelection()` ставит `SetHitPos(hp)` где `hp = GetBonePositionWS("Spine3")`
  (`dmBotMeleeCombat.c:37-56`). Spine3 — уровень груди; у игрока именно там в руках
  перед корпусом висит винтовка.
- `dmBotMeleeFightLogic_LightHeavy.EvaluateHit()` зовёт `m_Player.ProcessMeleeHitName(weapon,
  weaponMode, target, compName, hitPos)` (`dmBotMeleeFightLogic_LightHeavy.c:80-98`),
  передавая `hitPos = m_MeleeCombat.GetHitPos()` = Spine3.
- Натив `ProcessMeleeHit`/`ProcessMeleeHitName` **делает внутренний hit-трейс от
  атакующего к `pHitWorldPos`** и применяет урон к ПЕРВОМУ объекту, который встретит
  (это не скриптовый райкаст — он внутри натива; в скриптах ванили его нет). Трейс к
  Spine3 упирается в винтовку перед грудью → урон уходит в винтовку, а не в игрока.
  Аналогично натив `HumanCommandMelee2` при anim-событии Hit своим хит-детектом тоже
  попадает в винтовку (металл) → играется «поверхностный» удар с искрами, а не «мясной».

Подтверждение, что ваниль именно поэтому не использует chest-позицию: `EvaluateHit_Common`
перед `ProcessMeleeHit` **перетирает** hitPos на `targetEntity.ModelToWorld(targetEntity.GetDefaultHitPosition())`
(`dayzplayermeleefightlogic_lightheavy.c:678`), а `GetDefaultHitPosition()` у игрока =
позиция `Pelvis` (таз), у зомби = `Spine1`, у животных = `Pelvis` — т.е. «чистая» точка
тела, не закрытая предметом в руках. В скриптах повторного райкаста по hitPos НЕТ — вся
магия резолва «кого ударить» в момент Hit-события живёт в нативе `ProcessMeleeHit*`.

Имя компонента при этом ВЕРНОЕ: `GetDefaultHitComponent()` у игрока возвращает
`"dmgZone_torso"` (`dayzplayer.c:468/497`), у зомби `"Torso"`, у животного `"Zone_Chest"` —
это имена **damage-zones** (не костей), которые `ProcessMeleeHitName`/`CloseCombatDamageName`
и ждут (см. доку `ProcessDirectDamage`: «componentName = which DamageZone was hit (NOT a
component name, actually!)», `object.c:1128`). Значит «зона» не является причиной бага —
причина именно `hitPos`.

`SetHitZoneIdx(-1)` само по себе НЕ ломает урон (мод переопределяет `EvaluateHit` и не
ходит через `EvaluateHit_Common`/`GetTargetData`), но `GetTargetData` в ванили обнулил бы
цель при `hitZoneIdx < 0` (`lightheavy.c:750-753`) — потому мод и вынужден переопределять
`EvaluateHit`.

## Правильный путь урона (точные сигнатуры)

Все нативы — в `3_game/`:

| Натив | Сигнатура | Резолв цели | Райкаст? |
|---|---|---|---|
| `DayZPlayer.ProcessMeleeHit` | `(InventoryItem pMeleeWeapon, int pMeleeModeIndex, Object pTarget, int pComponentIndex, vector pHitWorldPos)` (`dayzplayer.c:1273`) | по `pTarget` + hit-трейс к `pHitWorldPos` | **да, внутренний** (бьёт первый объект на пути) |
| `DayZPlayer.ProcessMeleeHitName` | `(InventoryItem pMeleeWeapon, int pMeleeModeIndex, Object pTarget, string pComponentName, vector pHitWorldPos)` (`dayzplayer.c:1275`) | по `pTarget` + hit-трейс к `pHitWorldPos` | **да, внутренний** |
| `DamageSystem.CloseCombatDamage` | `static (EntityAI source, Object targetObject, int targetComponentIndex, string ammoTypeName, vector worldPos, int directDamageFlags = ALL_TRANSFER)` (`damagesystem.c:22`) | прямо по `targetObject` + index | **нет** |
| `DamageSystem.CloseCombatDamageName` | `static (EntityAI source, Object targetObject, string targetComponentName, string ammoTypeName, vector worldPos, int directDamageFlags = ALL_TRANSFER)` (`damagesystem.c:23`) | прямо по `targetObject` + имя damage-zone | **нет** |

**Гарантированный урон КОНКРЕТНОЙ цели без райкаста и без component-index —
`DamageSystem.CloseCombatDamageName(...)`** — ровно тот путь, которым бьёт зомби
(`zombiebase.c:652/658/664`: `DamageSystem.CloseCombatDamageName(this, m_ActualTarget,
m_ActualTarget.GetHitComponentForAI(), ammo, hitPosWS)`, где `hitPosWS =
m_ActualTarget.ModelToWorld(m_ActualTarget.GetDefaultHitPosition())`). Зомби не делает
никакого райкаста для урона — только проверку дистанции.

Разница аммо:
- `ProcessMeleeHit*` сам выводит тип боеприпаса из `pMeleeWeapon`+`pMeleeModeIndex`.
- `CloseCombatDamage*` требует явное имя аммо: `weapon.GetMeleeCombatData().GetAmmoTypeName(weaponMode)`
  (`gameplay.c:166`, `inventoryitem.c:24`) или, для голых рук, `m_Player.GetMeleeCombatData().GetAmmoTypeName(weaponMode)`
  (`dayzplayer.c:1270`; bare-hand режимы 0/1/2 → `MeleeFist`/`MeleeFist_Heavy`/…).

## Что должно быть в GetDefaultHitComponent vs GetDefaultHitPositionComponent vs GetDamageZoneNameByComponentIndex

- `GetDefaultHitComponent()` → **имя damage-zone** (валидно для `ProcessMeleeHitName`/
  `CloseCombatDamageName`): игрок `"dmgZone_torso"`, зомби `"Torso"`, животное `"Zone_Chest"`
  (`dayzplayer.c:468/497`, `dayzinfectedtype.c:30/136`, `dayzanimal.c:961/981`,
  `playerbase.c:1431` → `DayZPlayerType`). **НЕ возвращает «Weapon»** — гипотеза из задачи
  не подтвердилась: зона корректная.
- `GetDefaultHitPositionComponent()` → **имя кости/селекции** для расчёта дефолтной точки
  удара: игрок `"Pelvis"`, зомби `"Spine1"`, животное `"Pelvis"` (`dayzplayer.c:470/502`,
  `dayzinfectedtype.c:32/141`, `dayzanimal.c:963/986`). Используется только для
  `SetDefaultHitPosition(...)`.
- `GetDefaultHitPosition()` → **model-space вектор** дефолтной точки удара:
  `m_DefaultHitPosition = SetDefaultHitPosition(GetDefaultHitPositionComponent())`
  (`playerbase.c:591`, `zombiebase.c:60`, `dayzanimal.c:690`); `SetDefaultHitPosition` =
  `GetSelectionPositionMS(selection)` (`playerbase.c:1447`, `dayzanimal.c:996`). Для вызова
  в мировых координатах — `target.ModelToWorld(target.GetDefaultHitPosition())`
  (`object.c:869`).
- `GetDamageZoneNameByComponentIndex(int)` (`object.c:1160`) и `GetDamageZonePos(string)`
  (`object.c:1155`) — служебные (маппинг index↔зона, центр зоны); для «магии» не нужны,
  т.к. имя зоны берём напрямую из `GetDefaultHitComponent()`.

Итог: `GetDefaultHitComponent()` возвращает валидную зону для `ProcessMeleeHitName` —
проблема не в ней, а в `hitPos = Spine3`.

## Как SetHitPos/SetHitZoneIdx/SetTargetObject взаимодействуют с анимацией и уроном

Минимальный набор, при котором анимация играется как «удар по персонажу» И урон приходит
в цель:

1. `SetTargetObject(target)` — кого бьём (`dayzplayerimplementmeleecombat.c:156`); отдаётся
   в `StartCommand_Melee2(pTarget, ...)` для ориентации анимации и хит-детекта.
2. `SetHitPos(<worldPos на/внутри тела, НЕ за предметом>)` — точка, куда бьём
   (`:172`). Использовать `target.ModelToWorld(target.GetDefaultHitPosition())`
   (таз/Spine1), а НЕ кость груди Spine3. Она идёт и в `StartCommand_Melee2`, и в урон.
3. `SetFinisherType(-1)` — отключить финишеры (`:182`).
4. `SetHitZoneIdx(...)` — для урона через `ProcessMeleeHit*` **не обязателен**, если
   переопределяем `EvaluateHit` и зовём `ProcessMeleeHitName`/`CloseCombatDamageName` по
   имени; но если пользоваться ванильным `EvaluateHit_Common`, нужен `>= 0`.

Порядок в ванили (клиент): `Update()` → `Reset()` → `TargetSelection()` (райкаст даёт
target+hitPos+hitZone) → `SetFinisherType()`; при Hit-событии — `m_MeleeCombat.Update(..., true)`
(ре-таргет) → `EvaluateHit` (`lightheavy.c:295-303`). На сервере ваниль `Update()` НЕ зовёт
`TargetSelection` (`#ifndef SERVER`, `dayzplayerimplementmeleecombat.c:224-245`) — поэтому
`dmBotMeleeCombat.Update()` override и существует. Сеттеры влияют на **скриптовый урон**
(`EvaluateHit` читает `GetTargetEntity()`/`GetHitZoneIdx()`/`GetHitPos()`/`GetWeaponMode()`);
**визуальный** исход (мясо/искры) решает натив `HumanCommandMelee2` своим хит-детектом,
которому нужна корректная точка удара (не за предметом в руках) — поэтому `SetHitPos`
влияет и на него через `hitPos` в `StartCommand_Melee2`.

## Что менять в dmBotMeleeCombat/dmBotMeleeFightLogic (рекомендация)

1. **`dmBotMeleeCombat.TargetSelection()`**: заменить `hp = GetBonePositionWS("Spine3")`
   на `hp = t.m_Entity.ModelToWorld(t.m_Entity.GetDefaultHitPosition())` (таз у игрока/
   животного, Spine1 у зомби). Это чинит и анимацию («мясной» удар), и урон (трейс не
   упирается в винтовку).
2. **`dmBotMeleeFightLogic_LightHeavy.EvaluateHit()`** (рекомендуемый, «гарантированный»
   путь): вместо `ProcessMeleeHitName(...)` звать
   `DamageSystem.CloseCombatDamageName(m_Player, target, target.GetDefaultHitComponent(),
   ammoName, hitPosWS)` (путь зомби, без внутреннего райкаста), где:
   - `ammoName = weapon ? weapon.GetMeleeCombatData().GetAmmoTypeName(weaponMode)
     : m_Player.GetMeleeCombatData().GetAmmoTypeName(weaponMode)`;
   - `hitPosWS = target.ModelToWorld(target.GetDefaultHitPosition())`.
   Множитель ×2 против зомби (`DM_MELEE_DAMAGE_MULT_ZOMBIE`) остаётся циклом.
   Альтернатива (минимальный фикс): оставить `ProcessMeleeHitName`, но с исправленным
   `hitPosWS` (п.1). `CloseCombatDamageName` надёжнее: не зависит от предметов в руках цели
   и от того, что натив встретит по пути к точке.
3. `SetHitZoneIdx(-1)` оставить как есть (урон идёт по имени зоны, индекс не нужен).

## Источники (файл:строка)

- `DayZ Projects/scripts/4_world/entities/manbase/dayzplayer/dayzplayermeleefightlogic_lightheavy.c`:
  `HandleHitEvent` L282-334 (WasHit→Update(...,true)→EvaluateHit), `EvaluateHit` L577-602,
  `EvaluateHit_Common` L660-703 (L678 `hitPosWS = ModelToWorld(GetDefaultHitPosition())`,
  L687 `ProcessMeleeHit`), `GetTargetData` L750-753 (обнуление цели при hitZoneIdx<0),
  finisher `CloseCombatDamage` L652.
- `DayZ Projects/scripts/4_world/entities/dayzplayerimplementmeleecombat.c`: сеттеры
  `SetHitZoneIdx` L146 / `SetTargetObject` L156 / `SetHitPos` L172 / `SetFinisherType` L182 /
  `GetTargetEntity` L151, `Update` L220-246 (`#ifndef SERVER` L224), `TargetSelection` L334-409,
  `SetTarget` L519-528, `InternalResetTarget` L510-517.
- `DayZ Projects/scripts/3_game/dayzplayer.c`: `ProcessMeleeHit` L1273 / `ProcessMeleeHitName` L1275,
  `GetMeleeCombatData` L1270, `GetDefaultHitComponent` L497 / `GetDefaultHitPositionComponent` L502,
  `m_DefaultHitComponent = "dmgZone_torso"` L468 / `m_DefaultHitPositionComponent = "Pelvis"` L470.
- `DayZ Projects/scripts/3_game/damagesystem.c`: `CloseCombatDamage` L22 / `CloseCombatDamageName` L23.
- `DayZ Projects/scripts/3_game/entities/object.c`: `ProcessDirectDamage` L1134 (док L1128 «NOT a
  component name»), `GetDamageZonePos` L1155, `GetDamageZoneNameByComponentIndex` L1160,
  `GetSelectionPositionMS` L860, `ModelToWorld` L869.
- `DayZ Projects/scripts/3_game/entities/entityai.c`: `GetDefaultHitComponent` L3784 /
  `GetDefaultHitPositionComponent` L3792 / `GetDefaultHitPosition` L3804.
- `DayZ Projects/scripts/3_game/entities/dayzinfectedtype.c`: `GetDefaultHitComponent` L136 /
  `GetDefaultHitPositionComponent` L141, дефолты `"Torso"` L30 / `"Spine1"` L32.
- `DayZ Projects/scripts/3_game/entities/dayzanimal.c`: `GetDefaultHitPosition` L991,
  `SetDefaultHitPosition` L996, дефолты `"Zone_Chest"` L961 / `"Pelvis"` L963.
- `DayZ Projects/scripts/3_game/gameplay.c`: `MeleeCombatData.GetAmmoTypeName` L166.
- `DayZ Projects/scripts/3_game/entities/inventoryitem.c`: `GetMeleeCombatData` L24.
- `DayZ Projects/scripts/4_world/entities/manbase/playerbase.c`: `GetDefaultHitComponent` L1431 /
  `GetDefaultHitPosition` L1436, `m_DefaultHitPosition` L591, `SetDefaultHitPosition` L1447.
- `DayZ Projects/scripts/4_world/entities/creatures/infected/zombiebase.c`: `CloseCombatDamageName`
  L652/658/664 (эталон «прямого» урона), `hitPosWS = ModelToWorld(GetDefaultHitPosition())` L663.
- Expansion `eAIMeleeCombat.c`: `Update` L200-218 (всегда Reset→TargetSelection→SetFinisherType),
  `TargetSelection` L24-97 (использует РЕАЛЬНЫЙ райкаст `HitZoneSelectionRaycast` L136),
  `HitZoneSelectionRaycast` L136-198.
- Expansion `eaimeleefightlogic_lightheavy.c`: `HandleFightLogic` L52-249, `EvaluateHit` L363-375
  (зовёт `super.EvaluateHit` — ванильный путь с валидным hitZoneIdx из райкаста; потому у них
  нет бага — у них НЕТ «магии без райкаста»).
- botorama: `core/4_World/Entities/Bot/Melee/dmBotMeleeCombat.c:37-56` (SetHitPos=Spine3),
  `core/4_World/Entities/Bot/Melee/dmBotMeleeFightLogic_LightHeavy.c:80-98` (ProcessMeleeHitName).
