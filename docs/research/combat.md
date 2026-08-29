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

## Огнестрел (TODO — открытые вопросы остаются)

1. Прицел/поворот ствола для ИИ, `OverrideAimChange*` или анимация прицела, выстрел (`Fire`?),
   перезарядка (`WeaponManager`/действие reload). Как бот выбирает цель и ведёт прицел.
2. Перезарядка/извлечение магазина — ванильные команды/действия, доступные без `ActionManager`.
3. Расчёт урона и хит (для самопроверяемых тестов боя).

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
