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
  **Готча позиции удара**: `SetHitPos` брать через
  `target.ModelToWorld(target.GetDefaultHitPosition())`, НЕ через
  `GetBoneIndexByName("Spine3")` — зомби (`ZombieBase`) это `DayZCreature`, а не `Human`,
  и кости `"Spine3"` у него нет → фолбэк на `GetPosition()` (ноги/земля) → эффект удара
  ложится на поверхность (листья/искры/снег) вместо крови на теле. Ваниль так и делает
  (`EvaluateHit_Common` L678: `targetEntity.ModelToWorld(targetEntity.GetDefaultHitPosition())`).

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

### Визуальный прицел: наклон ствола + IK рук + ADS

**Задача**: ствол визуально указывает на цель, руки не трясутся, есть поза «прижал щеку»
(ironsights/optics). Всё подтверждено чтением графов (`botorama/Animations/*.agr` — копия
ванили `DZ/anims/workspaces/player/player_main/`) и ванильных скриптов.

#### Кто гонит наклон ствола и IK (AimX/AimY/AimIKX vs engine)

**`AimX`/`AimY`/`AimIKX` — engine-driven, скриптом НЕ пишутся.** Цепочка:

1. Мышь → `HumanInputController.OverrideAimChangeX/Y(...)` (`3_game/human.c:240/243`) → базовый
   угол прицела `m_fCurrentAimX/Y` (`SDayZPlayerAimingModel`, `3_game/dayzplayer.c:1104-1105`).
2. Движок вызывает `DayZPlayerImplement.AimingModel()` (`dayzplayerimplement.c:1707`) только при
   `m_MovementState.IsRaised()` (`:1726`); скрипт-класс `DayZPlayerImplementAiming.ProcessAimFilters`
   (`dayzplayerimplementaiming.c:162`) вычисляет **оффсеты** (дыхание/шум/отдача/kuru) и пишет их в
   `m_fAimXHandsOffset/Y` (`:236-237`), `m_fAimXCamOffset/Y` (`:248-249`), `m_fAimXMouseShift/Y`
   (`:271-272`). Абсолютные `AimX/AimY` скрипт НЕ пишет — их пишет движок как `base aim + offset`.
3. Движок проецирует результат в граф: `AimX`/`AimY`/`AimIKX`. `GetBaseAimingAngleUD/LR()` —
   нативы (`human.c:1095/1098`) = base aim «без sway/offsets».

Что эти переменные двигают в графе:

- **`AimX`+`AimY` → `AnimNodePose2`** (2D blend space, 13×3 поз): `AimPose` (`Locomotion.agr:8778`),
  `AimObstPose` `:8770`, `AimInjPose` `:8748`, `AimInjWalkPose` `:8756`, `AimRunPose` `:8792`,
  `AimRunPoseObst` `:8800`, `AimWalkPose` `:8808` — все читают `"AimY" "AimX"` (вертикаль первой,
  горизонталь второй), позы из сетов `Locomotion.Aim/AimObst/AimRun/AimWalk/...`. **Это и есть
  «наклон ствола»** — поворот корпуса+рук+ствола в нужную сторону.
- **`AimIKX`+`AimY` → `AnimNodeWeaponIK`** (hand IK): `NormalWeaponIK` (`Locomotion.agr:2566`),
  `AnimNodeWeaponIK` в `Actions.agr:1627/1995/2035/4575/5155/6816` (и `master.agr:23` — там
  `"AimIKX" "AimX"`). Включается битами `ArmIK` (`isbitset(ArmIK, 0/1/2)`). **Переменной `AimIKY`
  НЕТ** — вертикальный IK берёт `AimY` (горизонтальный — `AimIKX`); в `master.agr` вертикаль
  берёт `AimX`. Это «прижим рук к цевью/рукояти».
- **`ArmIK`** (int 0..7, `player_main.agr:115`) — маска включения IK, ставится движком из
  item-behavior конфига: `SetIKStance(STANCEIDX_RAISEDERECT, true, true, true)` для FIREARMS
  (`dayzplayercfgbase.c:191`), т.е. в raised-стойке hand-IK **полностью включён**.

Вывод: `AnimSetFloat("AimX"/"AimY")` (наш текущий код `dmAISurvivorBase.c:288-290`) — **no-op**,
движок каждый кадр перезаписывает эти переменные из своей aim-модели.

#### Как Expansion вшивает кастомные aim-переменные в граф

- Бинд: `m_VAR_AimX = hai.BindVariableFloat("eAI_AimX")`, `m_VAR_AimY = BindVariableFloat("eAI_AimY")`,
  `m_VAR_Raised = BindVariableBool("eAI_Raised")`, `m_VAR_ADS = BindVariableBool("ADS")` (ванильный!)
  (`Core/Scripts/.../Classes/Commands/ExpansionHumanST.c:139-145`, под `#ifdef EXPANSIONMODAI`).
- Запись: `AnimSetFloat(m_VAR_AimX/AimY, aimX/aimY)` в `eAI_HandleWeapons` (`eAIBase.c:9587-9588`);
  raise — `AnimSetBool(m_VAR_Raised, m_WeaponRaised)` (`:9536`); ADS — натив `hcw.SetADS(ads)`
  (`:9608`), `AnimSetBool(m_VAR_ADS, ads)` **закомментирован** (`:9606`).
- `override AimingModel(...) → false` (`eAIBase.c:9993`).
- Их граф в репо НЕ лежит (только `graphName="DayZExpansion\Animations\AI\player_main.agr"`,
  `Animations/AI/config.cpp:55`). Паттерн (восстановлен по биндам/вызовам, посимвольно **не
  подтверждено**): их кастомный `player_main.agr` — копия ванильного, где `AnimNodePose2`-ноды и
  `AnimNodeWeaponIK`-ноды переписаны читать `eAI_AimX/eAI_AimY` вместо `AimX/AimY/AimIKX`, а
  raise-машина — на `eAI_Raised` вместо `Raised`. Т.е. прицел целиком выведен из-под движка и
  гонится скриптом через кастомные переменные.

#### Причина тряски рук и как её убрать

**Тряска — от `AnimNodeWeaponIK` (hand IK).** В raised-стойке `ArmIK` включён (см. выше),
IK-нода активна и читает engine-driven `AimIKX`/`AimY`. У ИИ они не задаются скриптом (no-op) и
не приходят от мыши → движок гоняет их сам (нативная проекция/реконсиляция прицела при
«raised»-команде) → IK-таргет дёргается → руки визуально трясутся.

- `override AimingModel(...) → false` отключает ТОЛЬКО скриптовые оффсеты (`m_fAimXHandsOffset`,
  recoil, kuru — `ProcessAimFilters`). Нативную проекцию IK это НЕ трогает → поэтому «AimingModel
  = false НЕ помогло» (ожидаемо).
- Второй источник: в raised-стойке тикает ванильная ADS-машина `HandleWeapons`
  (`dayzplayerimplement.c:1894-1999`, см. ниже), которая сама зовёт `ExitSights()` → `SetADS(false)`
  и `ResetADS`.

**Как убрать (Expansion-подход)**: НЕ входить в raised-стойку (не форсить `STANCEIDX_RAISED`),
тогда `ArmIK` не включён и IK-нода неактивна, а raise/aim гнать целиком кастомными переменными
(вариант (а) из раздела raise). Если raised-стойка нужна — переписать `AnimNodeWeaponIK` читать
кастомные `dmAI_AimIKX`/`dmAI_AimY` и писать туда **сглаженные** значения (а не скачущие углы
цели), чтобы руки не дёргались. (Нативный механизм дёрганья `AimIKX/AimY` — гипотеза, на живом
сервере проверить через `DM_BOT_DEBUG_FSM`/`PAWN`-лог значений.)

#### ADS / ironsights / optics поза (SetADS)

- `HumanCommandWeapons.SetADS(bool)` — натив (`3_game/human.c:1029`) = «sets head tilt to optics»,
  пишет engine-var `ADS` → граф выбирает ADS-позы `Locomotion.*.AimingDownSight`:
  `IdleRasADSPose` (`Locomotion.agr:8011`), `ErcADSPose` `:8858`, `CroADSPose` `:8816`,
  `PneADSPose` `:8974`; переходы — по условию `ADS` (`IdleRasADST :8017`, `ErcADST :436`,
  `CroADST`/`PneADST` и т.д.). Это и есть «щека прижата к прикладу».
- `IsInIronsights()`/`IsInOptics()` (`dayzplayerimplement.c:289/294`) — **камерные** флаги
  (`m_CameraIronsight`/`m_CameraOptics`), НЕ поза. Позу даёт только `SetADS`.
- Ванильная ADS-машина (`dayzplayerimplement.c:1894-1999`): `m_bADS = hic.WeaponADS()` (инпут);
  `hcw.SetADS(true)` зовётся **только** в ветке `switchToADS` при `m_bADS == true` (`:1983-1986`);
  иначе `exitSights = true` → `ExitSights()` → `hcw.SetADS(false)` (`:421`).
- **Корень «бот стреляет не прицеливаясь»**: наш `ApplyWeaponAim()` зовёт `hcw.SetADS(m_WeaponRaised)`
  ДО `super.CommandHandler()` (`dmAISurvivorBase.c:336` → `:294`). Затем `super.CommandHandler()`
  → `HandleWeapons` (работает, т.к. `m_MovementState.IsRaised()`) → `hic.WeaponADS() == false` →
  `exitSights` → `ExitSights()` → `SetADS(false)` **перетирает наш true каждый кадр**.

#### Минимальный рецепт для botorama (точные переходы/переменные)

1. **`player_main.agr` `$Vars`** — добавить (ванильные `AimX/AimY/AimIKX` ОСТАВИТЬ — натив
   обращается к ним по хешу имени):
   ```
   #Var dmAI_AimX float 0.0 -180.0 180.0 ""
   #Var dmAI_AimY float 0.0 -85.0 85.0 ""
   #Var dmAI_AimIKX float 0.0 -180.0 180.0 ""
   ```
2. **`Locomotion.agr` — переписать `AnimNodePose2`** (заменить `"AimY" "AimX"` на
   `"dmAI_AimY" "dmAI_AimX"`) в: `AimPose :8778`, `AimObstPose :8770`, `AimInjPose :8748`,
   `AimInjWalkPose :8756`, `AimRunPose :8792`, `AimRunPoseObst :8800`, `AimWalkPose :8808`.
3. **`Locomotion.agr` — `AnimNodeWeaponIK` `NormalWeaponIK :2566`** (и, при желании, IK-ноды в
   `Actions.agr`/`master.agr`): заменить `"AimIKX" "AimY"` на `"dmAI_AimIKX" "dmAI_AimY"` и писать
   туда стабильные значения. **Проще/надёжнее** (Expansion-путь): не входить в raised-стойку
   (`ArmIK` выключен → IK неактивен → тряски нет), а raise-позу вести кастомной `dmAI_Raised`
   (вариант (а) раздела raise).
4. **Скрипт `dmAISurvivorBase`**:
   - `BindLookVars()` (`:131-132`): биндить `"dmAI_AimX"/"dmAI_AimY"` вместо `"AimX"/"AimY"`.
   - `ApplyWeaponAim()` (`:283-295`): `AnimSetFloat(m_VarAimX, m_AimRelAngleLR)` /
     `AnimSetFloat(m_VarAimY, m_AimRelAngleUD)` — оставить (теперь пишут кастомные vars);
     **`hcw.SetADS(m_WeaponRaised)` перенести ПОСЛЕ `super.CommandHandler()`** (рядом с
     `ApplyBodyTurn`/`ApplyMovement`), иначе ваниль `ExitSights()` перетрёт.
   - Сгладить `m_AimRelAngleLR/UD` (например `Math.SmoothCD`) перед записью в граф — ствол не
     дёргается.
5. **ADS-поза** = `hcw.SetADS(m_WeaponRaised)` после `super` → граф сам переключит на
   `AimingDownSight`. Режимы: «от бедра» = raised без `SetADS`; «с мушки» = `SetADS(true)` без
   оптики; «в оптику» = `SetADS(true)` + `SwitchOptics(optic, true)` (TODO, позже).

Открытые вопросы: точный текст `.agr` Expansion недоступен (PBO) — паттерн восстановлен по
биндам/вызовам, посимвольно «не подтверждено». Нативный механизм тряски (переписывание
`AimIKX/AimY` движком в raised-стойке) — гипотеза, проверять на живом сервере; если тряска
останется после рецепта — выключить `ArmIK` (не входить в raised-стойку) или писать 0 в
`dmAI_AimIKX/dmAI_AimY`.

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

### Взятие в руки (мили) — занятые руки

- Флаг `isMeleeWeapon` (`InventoryItem.m_IsMeleeWeapon`) НЕнадёжен для классификации мили
  (ложный `BomberJacket_Brown`). Классификация — `EntityIsMelee` (в `dmAISurvivor`): НЕ
  `IsWeapon()` (огнестрел) + `inventorySlot`/`itemInfo` содержит Knife/Melee/Shoulder/Axe.
- Взятие в ЗАНЯТЫЕ руки сырым `LocalTakeToDst(src, hands)` → `LocationSyncMoveEntity`
  некорректен (вытесняет/теряет предмет, дальше в руки попадает одежда). Правильный путь —
  `dmInventoryFrame`-цепочка: фрейм 1 освобождает руки (stash: сломанное → PLACEONGROUND,
  иначе TAKEINTOCARGO в `Back`, fallback PLACEONGROUND), фрейм 2 — `PUTINTOHANDS` мили.
  Оркестрация — в `dmBotState_Fighting.EnsureMeleeEquipped` (re-runnable каждый тик).
- После `LocalTakeToDst` в руки — `GetItemAccessor().HideItemInHands(true); HideItemInHands(false);`
  (эталон Expansion `eAI_TakeItemToLocation`, фикс аним-состояния рук).
- `IsRuined()` ≡ `IsDamageDestroyed()` (`object.c:1211`).

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

## AI weapon fire — direction + modes

Цель: заменить muzzle-based выстрел (`TryFireWeapon` → вперёд от дула) на натив
`Weapon_Base.Fire(mi, pos, dir, speed)` с **явным** направлением из модели прицеливания
`dmAiming`, при этом сохранив ванильный weapon-FSM (речь/анимация/reload/джам). Эталон —
Expansion (`eAI_Fire` / modded `weaponfire.c`). Все пути — ваниль `DayZ-Script-Diff/scripts/`,
конфиги — `DayZ Projects/DZ/`.

### 1. Натив `Fire(mi, pos, dir, speed)` — семантика `speed`

- Сигнатура: `proto native bool Fire(int muzzleIndex, vector pos, vector dir, vector speed)`
  (`4_world/entities/core/inherited/weapon.c:58`). Никакой док-комментария нет.
- Ванильный выстрел (muzzle-based): `proto native bool TryFireWeapon(EntityAI weapon, int muzzleIndex)`
  (`3_game/systems/inventory/weaponinventory.c:8`) — глобальный натив; сам берёт transform дула
  (`GetCameraPoint(mi, out pos, out dir)`, `weapon.c:413` — модель-пространство «глаза»/дула),
  читает `initSpeed` из `CfgAmmo` патрона в патроннике, применяет `dispersion` и спавнит пулю.
  Именно его зовут все ванильные `WeaponFire*`-состояния (`TryFireWeapon(m_weapon, mi)`).
- **Семантика `speed` (не подтверждено исходниками натива — движок, но поведение подтверждено
  Expansion в проде):** `pos` = мировая точка спавна пули, `dir` = норм. направление выстрела,
  `speed` = **направление** вектора скорости; величину скорости движок берёт сам из
  `CfgAmmo <ammo> initSpeed` (× `initSpeedMultiplier` музла). Доказательство: Expansion в
  `eAI_Fire` передаёт `Fire(muzzleIndex, pos, dir, dir)` — т.е. **в `speed` кладёт тот же
  единичный `dir`** (`DayZExpansion_AI/.../Entities/Weapons/Firearms/Weapon_Base.c:157`), и
  выстрелы летят/убивают с корректной скоростью/уроном; свою модель урона Expansion считает
  от `g_Game.ConfigGetFloat("CfgAmmo " + ammoType + " initSpeed")` (`Weapon_Base.c:546`), а не
  от аргумента `speed`.
- **Рекомендация для botorama**: повторять эталон — `Fire(mi, pos, dir, dir)` (направление в
  обоих аргументах). Мировое направление бот держит в `m_AimWorldDirection`; перед выстрелом
  добавить сверху оружейный `dispersion` (конус), затем `pos = neck-кость (или eye) + dir*0.2`
  (сдвиг вперёд, как Expansion `Weapon_Base.c:149`), `dir` = итоговое направление. ВАЖНО: не
  пытаться самому умножать `dir` на `initSpeed` — величину скорости движок возьмёт из патрона.

### Готча: натив `GetChamberedCartridgeMagazineTypeName` падает на некорректном индексе ствола

- **Симптом**: нативный краш в `GetAmmoInitSpeed` → `weapon.GetChamberedCartridgeMagazineTypeName(mi)`
  (строковый натив, читает аммо-тип патронника) — стек `ComputeShot → CompensateBulletDrop →
  ComputeBulletTravelTime → GetAmmoInitSpeed → dmBot_Fire → WeaponFireMultiMuzzle.OnEntry`. В логе
  перед крашем `dmBot_Fire: mi=0` и `mi=1` отработали (оба `ammo=Ammo_308Win`), затем краш на `mi=2`.
- **Реальный корень (см. ниже)**: выход индекса ствола за границы в мультимузл-цикле, а НЕ «десинк
  патронника после нокаута». Ранняя гипотеза про «полуживой патронник» была ложной.
- **Ложный путь (откачено)**: аборт weapon-FSM при нокауте/смерти (`AbortWeaponEvent()` в
  `UpdateUnconsciousBridge` и `EEKilled`) — он ничего не чинил (краш продолжился), зато ловил ванильную
  ошибку: обрыв `WeaponChambering_MultiMuzzle` посреди цикла кидает `Error("[wpnfsm] ... DropBullet,
  error - cannot drop Bullet - lost")` (патрон уже съеден в патронник, дропнуть нечего). Урок: при
  `!CanAct()` (нокаут/смерть) `TryFireWeapon` не вызывается вовсе (ранний `return` в `CommandHandler`),
  поэтому аборт оружия для защиты от краша не нужен; ваниль при нокауте оружие тоже не абортит.
- **Вспомогательный фикс (оставлен)**: сброс `m_FireRequest=false` в `ResetActuation` (чтобы после
  нокаута не стреляло по «висячему» запросу с замороженным направлением).

### Вторая причина того же краша: мультимузл-цикл `WeaponFireMultiMuzzle` выходит за число стволов

- **Факт**: B95 — это `DoubleBarrel_Base` (`B95_base : DoubleBarrel_Base`, конфиг `muzzles[]={"this","SecondMuzzle"}`
  = 2 ствола, `modes[]={"Single","Double"}`). В режиме `Double` FSM-состояние `WeaponFireMultiMuzzle`
  стреляет циклом `for (i=0; i<GetCurrentModeBurstSize(mi); i++)`, передавая `i` как ИНДЕКС СТВОЛА.
- **Симптом**: в логе `dmBot_Fire: mi=0` (ok) и `dmBot_Fire: mi=1` (ok, оба `ammo=Ammo_308Win`), затем
  краш на `mi=2` — `GetChamberedCartridgeMagazineTypeName(2)` падает (ствола 2 нет). Тот же стек
  `ComputeShot → CompensateBulletDrop → GetAmmoInitSpeed`, но корень ДРУГОЙ: не десинк нокаута, а
  выход индекса за число стволов (`GetCurrentModeBurstSize` вернул ≥3 при 2 стволах).
- **Почему ваниль не падает**: ванильный `TryFireWeapon(m_weapon, i)` — натив с проверкой границ;
  наш `dmBot_Fire(i)` читает `GetChamberedCartridgeMagazineTypeName(i)` напрямую без проверки.
- **Фикс (сделано)**: (1) ограничить цикл `WeaponFireMultiMuzzle` по `GetMuzzleCount()`
  (`for (i=0; i<b && i<muzzleCount; i++)`), (2) гард диапазона в `GetAmmoInitSpeed`
  (`if (mi < 0 || mi >= weapon.GetMuzzleCount()) return 0.0;`). Добавлен диагностический лог
  `b`/`muzzleCount`/`mi`/`mode`, чтобы зафиксировать реальное значение `GetCurrentModeBurstSize`.

### 2. Иерархия `WeaponFire*` и какие нужны для AKM / M4A1 / B95

Файл `4_world/entities/firearms/fsm/states/weaponfire.c` (+ `weaponfirelast.c`,
`weaponfireandchambernext.c`, `weaponfireandchambernextfrominnermag.c`). Иерархия:

```
WeaponStateBase                                  (weaponstatebase.c:10)
├─ WeaponStartAction                             (weaponstartaction.c:4)  — стартует hcw.StartAction
│  ├─ WeaponDryFire                              (weaponfire.c:2)
│  ├─ WeaponFire                                 (weaponfire.c:44)        — ядро: TryFireWeapon
│  │  ├─ WeaponFireWithEject                     (weaponfire.c:112)
│  │  ├─ WeaponFireAndChamber                    (weaponfire.c:329)
│  │  └─ WeaponFireAndChamberFromInnerMagazine   (weaponfire.c:350)
│  ├─ WeaponFireMultiMuzzle                      (weaponfire.c:135)
│  │  └─ WeaponFireMagnum                        (weaponfire.c:212)
│  └─ WeaponFireToJam                            (weaponfire.c:279)
├─ WeaponFireLast                                (weaponfirelast.c:2)     — nested FSM: WeaponFireWithEject
├─ WeaponFireAndChamberNext                      (weaponfireandchambernext.c:1) — nested FSM: WeaponFireAndChamber
└─ WeaponFireAndChamberNextFromInnerMag          (weaponfireandchambernextfrominnermag.c:1) — nested FSM: WeaponFireAndChamber
```

- `WeaponFireLast` / `WeaponFireAndChamberNext` / `WeaponFireAndChamberNextFromInnerMag` — НЕ
  наследники `WeaponStartAction`; это «составные» состояния с вложенным FSM, стартующим
  `WeaponFireWithEject` (`weaponfirelast.c:16`) или `WeaponFireAndChamber`
  (`weaponfireandchambernext.c:15`, `weaponfireandchambernextfrominnermag.c:15`).
- `WeaponFireMultiMuzzle.OnEntry` (`weaponfire.c:141-181`): `mi = GetCurrentMuzzle()`,
  `b = GetCurrentModeBurstSize(mi)`; если `b > 1` → цикл `for i < b: TryFireWeapon(m_weapon, i)`
  (огонь со ВСЕХ стволов подряд), иначе один выстрел `mi`; затем листает ствол
  `SetCurrentMuzzle(mi + 1)` (или в 0, если был последний) — `weaponfire.c:175-178`.
  Для B95 «двойной ствол» `Double`-режим имеет `burst=2` → оба ствола; `Single` → `burst=1` →
  один ствол, с чередованием.

**Какие fire-состояния реально стреляют у наших стволов** (по `InitStateMachine` баз):

- **AKM** (`AKM_Base : RifleBoltFree_Base`, `automaticrifle/akm.c:1`; FSM в
  `rifleboltfree_base.c:106`): `Trigger_C10 = WeaponFireLast` (:145, последний патрон без магазина),
  `Trigger_C11 = WeaponFireAndChamberNext` (:146, обычный с магазином),
  `Trigger_C11L = WeaponFireLast` (:147, последний патрон), `Trigger_C10J/C11J = WeaponFireToJam`
  (:154-155), dry = `WeaponDryFire`.
- **M4A1** (`M4A1_Base : RifleBoltLock_Base`, `automaticrifle/m4a1.c:1`; FSM в
  `rifleboltlock_base.c:134`): те же — `WeaponFireLast` (:183), `WeaponFireAndChamberNext` (:184),
  `WeaponFireLast` (:185), `WeaponFireToJam` (:193-194).
- **B95** (`B95_base : DoubleBarrel_Base`, `rifle/b95.c:1`; FSM в `doublebarrel_base.c:130`):
  `Trigger_L_E/E_L/F_L/L_L = WeaponFireMultiMuzzle` (:167-170), dry = `WeaponDryFire` (:172-175).
  Мультиствол: `enum MuzzleIndex { First=0, Second=1 }` (`doublebarrel_base.c:19-23`).

**Какие классы надо moddить** (ровно те, что трогает Expansion в
`0_DayZExpansion_AI_Preload/4_world/entities/weapons/firearms/fsm/states/weaponfire.c`):
`WeaponFire`, `WeaponFireWithEject`, `WeaponFireMultiMuzzle`, `WeaponFireToJam`.
Почему этого достаточно: `WeaponFireAndChamber` / `WeaponFireAndChamberFromInnerMagazine`
наследуют `WeaponFire` и зовут `super.OnEntry(e)` → попадают в modded `WeaponFire.OnEntry`;
`WeaponFireLast`/`WeaponFireAndChamberNext` — составные, их вложенные состояния (`WeaponFireWithEject`
/ `WeaponFireAndChamber`) уже покрыты modded-классами.

### 3. Паттерн `eAI_Vanilla_OnEntry`

- **Ванильный `WeaponStateBase.OnEntry`** (`weaponstatebase.c:103-112`) делает ТОЛЬКО запуск
  вложенного FSM: `if (HasFSM() && !m_fsm.IsRunning()) m_fsm.Start(e);` (иначе лог).
- **Ванильный `WeaponStartAction.OnEntry`** (`weaponstartaction.c:15-46`): `super.OnEntry(e)`
  (запуск под-FSM) + старт анимации: `e.m_player.GetCommandModifier_Weapons().StartAction(m_action,
  m_actionType)` (с `HumanCommandAdditives.CancelModifier()` перед этим).
- Expansion переименовывает это в helper, НЕ трогая ванильный `OnEntry`:
  - `modded class WeaponStateBase` (`DayZExpansion_AI/.../FSM/States/weaponstatebase.c:3-12`)
    добавляет `void eAI_Vanilla_OnEntry(WeaponEventBase e)` = тело ванильного `OnEntry`
    (`if (HasFSM() && !m_fsm.IsRunning()) m_fsm.Start(e);`). Там же — override лог-хелперов
    `wpnDebugPrint`/`wpnPrint`/`Error` (:25-43).
  - `modded class WeaponStartAction` (`.../FSM/States/weaponstartaction.c:3-34`) override
    `eAI_Vanilla_OnEntry(e)`: `super.eAI_Vanilla_OnEntry(e)` (= `WeaponStateBase.eAI_Vanilla_OnEntry`
    = запуск под-FSM) + блок `hcw.StartAction(m_action, m_actionType)`.
- **modded `WeaponFire.OnEntry`** (`0_DayZExpansion_AI_Preload/.../weaponfire.c:3-24`) — порядок:
  ```
  override void OnEntry(WeaponEventBase e)
  {
      if (e) {
          eAIBase p;
          if (Class.CastTo(p, e.m_player)) {          // ветка ИИ
              m_dtAccumulator = 0;
              int mi = m_weapon.GetCurrentMuzzle();
              if (m_weapon.eAI_Fire(mi, p)) {         // ЯВНЫЙ выстрел по направлению ИИ
                  p.GetAimingModel().SetRecoil(m_weapon);   // отдача
                  m_weapon.OnFire(mi);                // EEFired + звук/эффекты
              }
              super.eAI_Vanilla_OnEntry(e);           // запуск под-FSM + hcw.StartAction
              return;
          }
      }
      super.OnEntry(e);                               // ванильный fallback для не-ИИ
  }
  ```
  Тот же порядок в modded `WeaponFireWithEject` (:27-53, + `EjectCasing`/`EffectBulletHide` между
  recoil и `OnFire`), `WeaponFireMultiMuzzle` (:55-100, `b = min(GetCurrentModeBurstSize(mi),
  GetMuzzleCount())`, цикл по стволам с проверкой `!IsChamberEmpty(i) && !IsChamberFiredOut(i)`,
  затем `SetCurrentMuzzle`-ротация), `WeaponFireToJam` (:102-128, + `SetJammed(true)` +
  `ResetBurstCount()`).
- **Зачем `eAI_Vanilla_OnEntry`, а не `super.OnEntry`**: в ванильном `WeaponFireWithEject.OnEntry`
  вызов `super.OnEntry(e)` (= `WeaponFire.OnEntry`) — «безобидный повторный» выстрел (после
  `EjectCasing` патронник пуст → `TryFireWeapon` вернёт false). Для ИИ это не работает: modded
  `WeaponFire.OnEntry` зовёт `eAI_Fire` (hitscan+`Fire`-натив), который НЕ зависит от
  «пустоты» патронника так же — повторный `super.OnEntry(e)` дал бы **двойной выстрел**.
  Поэтому modded-состояния зовут `super.eAI_Vanilla_OnEntry(e)` — «чистое» ванильное базовое
  поведение (под-FSM + анимация), минуя modded fire-логику.

### 4. Конфиг: `modes[]` / `burst` / `dispersion` / `reloadTime` / патрон

**API чтения конфига** (определены в `3_game/global/game.c` и `3_game/entities/object.c`):
- Глобально (по path-строке, классы через пробел): `GetGame().ConfigGetFloat(path)` (:522),
  `ConfigGetInt(path)` (:537), `ConfigGetText(path, out string)` (:447),
  `ConfigGetTextArray(path, out TStringArray)` (:556), `ConfigGetTextArrayRaw` (:569),
  `ConfigGetFloatArray` (:576), `ConfigGetType` (:544).
- Относительно конфиг-класса объекта (методы `Object`): `ConfigGetString(entry)` (:871),
  `ConfigGetInt(entry)` (:879), `ConfigGetFloat(entry)` (:885), `ConfigGetTextArray(entry, out)` (:894).
  Именно так Expansion читает `weapon.ConfigGetTextArray("modes", modes)`
  (`Core/.../Firearms/Weapon_Base.c:44`).
- Константы: `CFG_WEAPONSPATH = "CfgWeapons"` (`3_game/constants.c:221`),
  `CFG_MAGAZINESPATH = "CfgMagazines"` (:222), `CFG_AMMO = "CfgAmmo"` (:223).

**Пути**:
- Режимы: `weapon.ConfigGetTextArray("modes", modes)`; строка режима → класс
  `CfgWeapons <type> <mode>`, ключи `burst` / `dispersion` / `reloadTime` / `autoFire` /
  `recoil` / `soundSetShot`. Подтверждено Expansion: `g_Game.ConfigGetFloat(CFG_WEAPONSPATH + " "
  + type + " " + mode + " reloadTime")` (`Core/.../Firearms/Weapon_Base.c:50`).
- `dispersion` — то же место (`CfgWeapons <type> <mode> dispersion`); это оружейный разброс
  (радиан), который наш `dmAiming` должен добавлять СВЕРХУ своего личного `m_AimDirection`.
- Патрон: `CfgAmmo <ammo> initSpeed|airFriction|typicalSpeed` (`Weapon_Base.c:516/546/552`).
  `initSpeedMultiplier` — ключ музла (читается `ConfigGetFloat("initSpeedMultiplier")` от объекта,
  `Weapon_Base.c:547`; напр. B95 `SecondMuzzle initSpeedMultiplier=1.05`).

**Фактические значения (из `DayZ Projects/DZ/`)**:
- **AKM** (`weapons/firearms/akm/config.cpp`): `modes[] = {"SemiAuto","FullAuto"}` (:74-78);
  `SemiAuto`: `reloadTime=0.12` (:110), `dispersion=0.0020000001` (:113);
  `FullAuto`: `reloadTime=0.097999997` (:158), `dispersion=0.0020000001` (:161).
- **M4A1** (`weapons/firearms/m4/config.cpp`): `modes[] = {"SemiAuto","FullAuto"}` (:93-96);
  `SemiAuto`: `reloadTime=0.12` (:124), `dispersion=0.0020000001` (:127);
  `FullAuto`: `reloadTime=0.064999998` (:172), `dispersion=0.0020000001` (:175).
- **B95** (`weapons/firearms/b95/config.cpp`): `modes[] = {"Single","Double"}` (:78-82);
  `Single`: `reloadTime=0.1` (:93), `dispersion=0.00075000001` (:94);
  `Double`: `reloadTime=0.1` (:107), `dispersion=0.0015` (:108). `SecondMuzzle` дублирует
  `modes[]` (:130-134) и имеет `initSpeedMultiplier=1.05` (:126).
- **Типовые строки режимов** (матчатся по `typename.StringToEnum(ExpansionFireMode, mode)`,
  enum в `Core/.../Enums/ExpansionFireMode.c:1-9`: `INVALID=-1, SemiAuto, Burst, FullAuto, Single,
  Double`): `"SemiAuto"` (самозаряд/одно нажатие), `"Burst"` (очередь, `burst=N`), `"FullAuto"`
  (`autoFire=1`), `"Single"` (ручная перезарядка/однозаряд), `"Double"` (двустволка, `burst=2`).
- **`burst`/`autoFire`**: базовые классы `Mode_Single`/`Mode_SemiAuto`/`Mode_Burst`/`Mode_FullAuto`/
  `Mode_Double` в DZ-конфигах только forward-declared (`class Mode_Single;` и т.п.) — их
  определения (`burst`, `autoFire`) engine-side, в скриптах не читаются. Значения `burst` в
  конфигах появляются только у burst-режимов: `burst=3` (`m16a2/config.cpp:220`,
  `aug:114/498`, `famas:197`, `mp5:154`). Семантика: `GetCurrentModeBurstSize(mi)` (`weapon.c:46`)
  возвращает `burst` (≥1); `GetCurrentModeAutoFire(mi)` (`weapon.c:47`) возвращает `autoFire`
  (FullAuto). Для `Double` `burst=2` — поэтому `WeaponFireMultiMuzzle` палит оба ствола.

**Формулы полёта/падения (эталон Expansion `Weapon_Base.c:514-616`), компактно**:
- `speedCoef = e^(airFriction * distance)` = `Math.Pow(Math.EULER, airFriction * distance)`
  (`Weapon_Base.c:519`; `airFriction` из `CfgAmmo <ammo> airFriction`, `distance` = |origin − hit|).
- `initSpeed' = initSpeed * initSpeedMultiplier` (:546-550).
- `speed = initSpeed' * speedCoef` (:558).
- Время полёта `travelTime` (`eAI_CalculateProjectileTravelTime`, :580-606): интегрирование с шагом
  `simulationStep=0.05`, на каждом шаге `speed = e^(airFriction * distanceTraveled) * initSpeed`,
  `distanceTraveled += speed * dt`; остановка при `distanceTraveled >= distance` или `time >= 6.0`
  (макс. время полёта пули в DayZ — 6 с); последний шаг линейно интерполируется.
- Падение `drop = 0.5 * 9.81 * travelTime^2` (`eAI_CalculateProjectileDrop`, :611-616).
- Компенсация в `eAI_Fire` (:140-149): если `drop > 0.1` → `projectedPos = pos + dir*distance;
  projectedPos[1] += drop * 0.8; dir = normalize(projectedPos - pos);` затем `pos += dir*0.2`.
- Коэф. урона (`eAI_CalculateProjectileDamageCoefAtPosition`, :544-575): если
  `typicalSpeed != initSpeed'`, то `dmgCoef = (speed > typicalSpeed ? 1.0 : speed/typicalSpeed)`,
  иначе `dmgCoef = speedCoef` (`typicalSpeed` из `CfgAmmo <ammo> typicalSpeed`, :552).

### Открытые вопросы / не подтверждено

- Точное поведение `Fire(...)` на стороне натива (берёт ли `speed`-величину из `initSpeed` или
  игнорирует magnitude) — исходников движка нет; вывод по косвенным признакам (см. п.1). Пометка
  «не подтверждено из кода» честная; эталон Expansion в проде — рабочее доказательство.
- `m_TravelTime` в `eAIShot` (используется `eAI_CalculateProjectileDrop(shot.m_TravelTime)`,
  `Weapon_Base.c:141`) — вычисляется в конструкторе `eAIShot` (не входил в разбор); предполагается
  `eAI_CalculateProjectileTravelTime(airFriction, distance, initSpeed)`. Стоит проверить при
  переносе формулы времени полёта в `dmAiming`.
- Точные значения `burst`/`autoFire` базовых mode-классов (`Mode_*`) — engine-side, в скриптах/конфигах
  DZ не определены (только forward-декларации); при необходимости задавать `burst` своим стволам
  явно в `config.cpp` (как `burst=3` у M16A2/AUG).
- Передача оружейного `dispersion` как конуса: Expansion НЕ добавляет `dispersion` к `dir` в
  `eAI_Fire` (разброс у них в aiming-профиле `eAIAimingProfile.Update`, `Classes/Weapons/eAIAimingProfile.c:16`).
  Для botorama решение, КУДА добавлять `dispersion` (в `dmAiming` или при `Fire`), — наша конвенция,
  не ванильный контракт.

## Патронник после `Fire()`: не становится «стреляным» (дефект перезарядки B95)

Цель: выяснить, почему после `Fire()` (`dmBot_Fire`) `IsChamberFiredOut(mi)` остаётся `false`,
из-за чего `RandomizeFSMState()` ресинкает `DoubleBarrel_Base` (B95) в `LoadedLoaded` вместо
`FireoutFireout`, и событие `WeaponEventLoad1Bullet` (LOAD1_BULLET) отклоняется.

### 1. `Fire` vs `TryFireWeapon` — что делает каждый с патронником

- `proto native bool Fire(int muzzleIndex, vector pos, vector dir, vector speed)`
  (`4_world/entities/core/inherited/weapon.c:58`) — **низкоуровневый натив**: только спавнит пулю
  из `pos` в направлении `dir` со скоростью `speed`. **НЕ трогает состояние патронника**
  (не помечает `IsChamberFiredOut`). Доказательство: в ванили он НИГДЕ не используется — во всех
  fire-состояниях он закомментирован `//m_weapon.Fire();` (`fsm/states/weaponfire.c:64/119/292`),
  реальный выстрел идёт через `TryFireWeapon`. Эмпирически подтверждено логом botorama:
  `dmBot_Fire: firedOut=false` до `Fire()`, и после `Fire()` патронник остаётся «полным»
  (`RandomizeFSMState` ресинкает в `LoadedLoaded`).
- `proto native bool TryFireWeapon(EntityAI weapon, int muzzleIndex)`
  (`3_game/systems/inventory/weaponinventory.c:8`) — **полный выстрел**: сам берёт transform дула
  (`GetCameraPoint(mi, out pos, out dir)`, `weapon.c:413`), читает `initSpeed` патрона, спавнит пулю
  И **переводит патронник «полный → стреляный»** (`IsChamberFiredOut` становится `true`). Именно его
  зовут ВСЕ ванильные fire-состояния: `WeaponFire.OnEntry` (`weaponfire.c:66`),
  `WeaponFireWithEject.OnEntry` (:121), `WeaponFireMultiMuzzle.OnEntry` (:155/:166),
  `WeaponFireToJam.OnEntry` (:294). Что он обновляет патронник — следует из FSM-переходов с гвардами:
  напр. `Trigger_L_L → _fin_/rto/abt → F_F, GuardAnd(WeaponGuardChamberFiredOut(First),
  WeaponGuardChamberFiredOut(Second))` (`doublebarrel_base.c:260-262`) — переход в `FireoutFireout`
  возможен только если `IsChamberFiredOut` обоих стволов стал `true` сразу после `TryFireWeapon`.

### 2. Что помечает ствол «стреляным» — прямого натива НЕТ

Полный список chamber-нативов в `weapon.c` (все проверены, сеттера «fired out» среди них нет):

- `IsChamberEmpty/FiredOut/Jammed/Full/Ejectable(int mi)` (:74/:79/:84/:96/:90) — только запросы.
- `EjectCasing(int mi)` (:64) — **выбрасывает стреляную гильзу** (fired-out → empty). НЕ ставит
  «стреляный»: в ванили его зовут ТОЛЬКО после `TryFireWeapon`, причём под гвардом
  `IsChamberFiredOut(mi)` (`WeaponFireWithEject` :126; `WeaponEjectCasing.OnEntry`
  `weaponejectcasingandchamberfromattmag.c:13-16`).
- `CreateRound(int mi)` (:68, бывший `EjectRound`) — материализует патрон для извлечения
  (`WeaponEjectAllMuzzles.OnEntry` `weaponcharging.c:158/101/106` перед `ejectBulletAndStoreInMagazine`),
  не про «fired out».
- `PushCartridgeToChamber(mi, dmg, type)` (:179) — заталкивает ЖИВОЙ патрон → `IsChamberFull` (L).
- `PopCartridgeFromChamber(mi, out dmg, out type)` (:174) — извлекает патрон → `IsChamberEmpty` (E).
- `EjectCartridge(mi, out dmg, out type)` (скрипт, `weapon_base.c:2044-2057`) — обёртка:
  `IsChamberEjectable(mi) ? PopCartridgeFromChamber : PopCartridgeFromInternalMagazine`; «= GetCartridgeInfo
  + PopCartridge» (комментарий :2037). Тоже даёт EMPTY, а не FIRED-OUT.
- `EffectBulletShow/Hide(mi)` (:187/:193) — только визуальная «пуля в патроннике», не логическое состояние.
- `SetJammed(bool)` (скрипт, `weapon_base.c:409`) — флаг `m_isJammed`, не fired-out.
- `OnFire(int muzzle_index)` (`weapon_base.c:1028-1054`) — **только `m_BurstCount++`** (плюс у
  botorama override — шум выстрела). Патронник НЕ трогает.

**Вывод**: единственный script-visible путь «полный → стреляный» — это натив `TryFireWeapon`
(внутри движка). Отдельного натива/метода «пометить патронник стреляным» в ванили **нет**
(`grep SetChamberFiredOut/SetChamberState` по всем скриптам — пусто).

### 3. Рекомендация фикса для AI-бота

Поскольку `TryFireWeapon` стреляет от дула (`GetCameraPoint`), которое ИИ не гонит, а `Fire()`
(явное направление) патронник не обновляет — «правильного» способа получить именно `FireoutFireout`
через `Fire()` не существует. Практический фикс: после `Fire()` вручную «съесть» патрон, чтобы
патронник перешёл в корректное «потраченное» состояние, и FSM ресинкался в состояние, принимающее
`LOAD1_BULLET`:

```c
// reg/4_World/modded_WeaponBase.c, в dmBot_Fire после `bool fired = Fire(...)`:
if (fired)
{
    float dmg; string ammoType;
    if (EjectCartridge(muzzleIndex, dmg, ammoType))   // live round -> EMPTY (аналог «выстрелили»)
        EffectBulletHide(muzzleIndex);                // спрятать модель пули в патроннике
    pawn.ApplyRecoil(this);
    // ... шум
}
```

- `EjectCartridge(mi, out dmg, out type)` = `IsChamberEjectable ? PopCartridgeFromChamber :
  PopCartridgeFromInternalMagazine` (`weapon_base.c:2044`). Для B95 (патрон в патроннике) сработает
  `PopCartridgeFromChamber` → `IsChamberEmpty=true`.
- После этого `RandomizeFSMState()` (`weapon_base.c:675` → `GetMuzzleStates` :691 → `RandomizeFSMStateEx`
  `weaponfsm.c:719`) ресинкает B95 в `EmptyEmpty` (E_E), а НЕ `FireoutFireout` (F_F). Разница чисто
  косметическая: в E_E есть переход `E_E → __L__ → Chamber_E` (`doublebarrel_base.c:210`), т.е.
  `WeaponEventLoad1Bullet` ПРИНИМАЕТСЯ и перезарядка работает; пропадает лишь шаг «выброс стреляной
  гильзы» при перезарядке (гильзы нет — патрон сразу извлечён). Для ИИ это допустимо.
- **Если нужен именно F_F (полный визуальный цикл eject-casing)**: это достижимо ТОЛЬКО через
  `TryFireWeapon`, который стреляет в неправильном направлении (ИИ не гонит `GetCameraPoint`), —
  либо надо принимать десинк патронника и перезаряжать ИИ-вручную через `WeaponManager.EjectBullet()`
  (эталон Expansion: `eAIBase.ReloadWeaponAI` проверяет `IsChamberFiredOut` → `EjectBullet()`,
  `eAIBase.c:8869/8969`), минуя FSM-событие `LOAD1_BULLET`. Т.е. выбор: (а) пустой патронник +
  штатный FSM-релоад (рекомендуется, минимальный дифф) или (б) десинк + ручной релоад через
  `EjectBullet` (как Expansion).

### Открытые вопросы / не подтверждено

- Что именно внутри движка делает `Fire()` с патронником (спавн пули — точно; точный момент/условие,
  когда движок мог бы «допозже» пометить fired-out) — исходников движка нет. Поведение «не помечает»
  подтверждено косвенно: ваниль `Fire()` не использует, а `RandomizeFSMState` после `Fire()` ресинкает
  B95 в `LoadedLoaded` (лог botorama). Стоит проверить в логе с включённым `LogManager.IsWeaponLogEnable()`
  строку `[wpnfsm] RandomizeFSMState - randomized current state=...` для окончательного подтверждения.
- У Expansion серверный `eAI_Fire` тоже использует `Fire(...)` (`Weapon_Base.c:157`), т.е. тот же
  «не-помечает-fired-out» паттерн; как они реально закрывают перезарядку двустволки (Blaze/BK-43)
  на сервере — релоад через `EjectBullet` + `LoadMultiBullet` (`eAIBase.c:8955-8976`), но источник
  `IsChamberFiredOut=true` на сервере не разобран до конца (вероятно, клиентский `TryFireWeapon` в
  `eAI_FireOnClient` + сетевая синхронизация `Synchronize`). Пометить как «не подтверждено из кода».

---

# Боевое движение / фланг (Flank)

Статус: реализовано (`dmBotIntent_Flank` в `dmBotState_Shooting`), фаза 1 плана
`docs/plans/combat-movement.md`.

## Сводка решения (чем отличается от Expansion)

Expansion-эталон (`eaistate_flank` / `OverrideTargetPosition` в навигации, `eaistate_cover`)
флангует **одним случайным углом без предпроверки** — бот телепортирует/перекладывает цель пути
на случайную точку по кругу от цели и идёт, не зная, будет ли оттуда видно. Наш вариант точнее:

- **Свип углом**, а не случайный угол: от `DM_FLANK_START_ANGLE` (15°) шагом `DM_FLANK_ANGLE_STEP`
  (15°) до `DM_FLANK_MAX_ANGLE` (180°), обе стороны (±). Один кандидат за тик — дёшево.
- **LOS-предпроверка кандидата**: прежде чем задать точку движения, рейкаст из кандидата
  (`RaycastRVParams(ObjIntersectView, NEARESTCONTACT)`, точка луча = последняя точка пути, высота =
  `SurfaceY + neck-кость бота`) до головы цели — как в `dmVision.HasLOS`. Виден → идём, иначе skip.
- **Гард высоты через `SurfaceY`**: `|pathPoint.y - g_Game.SurfaceY(x,z)| <= DM_FLANK_MAX_SURFACE_DELTA`
  (1.5 м), иначе кандидат отвергается (навмеш по высоте врёт, точка может висеть в воздухе/под землёй).
- **Ранняя остановка по LOS**: каждый тик `bot.FindTarget(m_TargetEntity).m_HasLOS` — появилась
  видимость (цель выглянула/бот дошёл) → `Finish()`, возврат к `Aim` (стрельбе).
- **Мин. дистанция** `DM_FLANK_MIN_DIST` (5 м): в упор не кружить (перенято у Expansion).
- **Stall-таймаут** `DM_FLANK_STALL_TIMEOUT` (8 с): если `MoveTo` застрял/не может, вся попытка
  обрывается `Fail()`; свип до `DM_FLANK_MAX_ANGLE` без результата тоже `Fail()` (стейт пересоздаст
  фланг на следующем тике — ретрай).

## Детали реализации (готчи)

- `dmBotIntent_Flank : dmBotIntent_MoveTo` — `IsContinuous()=true` (нет мгновенного `Fail()` при
  отсутствии пути на старте; застрявший MoveTo пере-прокладывает путь, а не падает), `KeepLookAtGoal()=true`,
  `GetMoveSpeed()=2` (бег). Свип/`RePath` — в `OnUpdate` (до `super`), когда `!m_HasPath`.
- Направление кандидата — от зафиксированного `m_BaseDir` (yaw направления «цель → бот» на старте),
  поворот через `Vector(yaw, 0, 0).AnglesToVector()` (без ручного sin/cos и без деления вектора).
- Точка движения = **последняя точка пути** (`path[path.Count()-1]`), а не сырой кандидат —
  pathfinder сам снэпнул/поправил высоту; точку луча поднимаем на `SurfaceY + neckHeight`, НЕ на
  Y навмеш-точки.
- Активация фланга — флагом `m_Active` в `dmBotState_Shooting.OnUpdate` (как `m_HitTo`/`m_Look`),
  без пересоздания интентов: `!LOS && dist > DM_FLANK_MIN_DIST && threat >= DM_ATTACK_THREAT_THRESHOLD`.
  `Finish()` фланга — в `OnExit` и `ResolveTarget` (смена цели).

## Открытый вопрос

- `m_HasLOS` (восприятие) гейтится FOV-конусом по направлению головы; во время фланга голова
  смотрит на кандидата (`KeepLookAtGoal`), поэтому для больших углов свипа цель может быть вне
  конуса и ранняя остановка по `m_HasLOS` сработает с задержкой (когда `Aim`/тело довернётся к цели).
  Геометрическая видимость кандидата проверяется прямым райкастом (без FOV) — это не ломает выбор
  точки, но «момент Finish» может отставать на время доворота. Если понадобится — проверять LOS
  фланга прямым райкастом «бот → цель» (без FOV-гейта), как `dmVision.HasLOS`, а не `m_HasLOS`.

# Получение урона ботом (зомби → ИИ-бот)

## Цель

Выяснить, почему `dmAISurvivorBase : PlayerBase` (`INSTANCETYPE_AI_SERVER`) не получает урон от
зомби и нет порезов; где правильная точка перехвата; почему текущий хак в `EEHitBy`/`EEOnDamageCalculated`
(`core/4_World/Entities/Bot/dmAISurvivorBase.c:896` и `:909`) не работает.

## Путь зомби → урон (подтверждено)

1. `ZombieBase.FightLogic()` (ветка `COMMANDID_ATTACK`) → `attackCommand.WasHit()` →
   `DamageSystem.CloseCombatDamageName(this, m_ActualTarget, m_ActualTarget.GetHitComponentForAI(),
   ammo, hitPosWS)` — `zombiebase.c:652/658/664` (light/block/heavy ветки).
2. `DamageSystem.CloseCombatDamageName` — `proto native` (`damagesystem.c:23`). Натив сам резолвит
   damage-zone/component, считает `TotalDamageResult` из cfgAmmo и на цели вызывает цепочку:
   - `EEOnDamageCalculated(...)` — `bool`; `return false` = НЕ применять урон, `return true` = применить
     (`object.c:1136`, дефолт `true`).
   - применение Health/Shock/Blood — нативно.
   - `EEHitBy(...)` — пост-событие (`entityai.c:1111`).
   - `EEHealthLevelChanged(...)` — при смене уровня здоровья (`entityai.c:1021`).
   - `EEKilled(...)` — при смерти (`entityai.c:1072`).
3. `PlayerBase.EEHitBy` (`playerbase.c:1086`) — скриптовые допы: **порезы** через
   `GetBleedingManagerServer().ProcessHit(dmg, source, component, dmgZone, ammo, modelPos)`
   (`playerbase.c:1120-1124`, читает `damageResult.GetDamage(dmgZone, "Blood")`); сломанные ноги
   (`1153-1162`); `m_ShockHandler.CheckValue(true)`.
4. `PlayerBase.EEKilled` (`playerbase.c:1045`) — corpse-процессинг (`InsertCorpse`), `CharacterKill`
   через `GetHive()` (у ИИ-бота GetHive() null — поэтому мод и переопределяет `EEKilled`).

Замечания к нативу: `ProcessDirectDamage` — `proto native void` (`object.c:1130`); параметр
`damageCoef` — **множитель** базового урона ammo, а `componentName` на деле — имя **damage-zone**
(«NOT a component name» — комментарий `object.c:1124`). Флаги — `ProcessDirectDamageFlags`
(`object.c:1-7`: `ALL_TRANSFER / NO_ATTACHMENT_TRANSFER / NO_GLOBAL_TRANSFER / NO_TRANSFER`).

## Что гейтится для AI_SERVER (подтверждено)

- `EEHitBy` / `EEOnDamageCalculated` / `EEKilled` / `EEHealthLevelChanged` — **НЕ гейтятся**
  инстанс-типом и **НЕ** `IsPlayerSelected()`. Это нативные события конвейера урона, зовутся на
  сервере для любой `EntityAI`-цели, включая `AI_SERVER`. Доказательства: базовые объявления
  (`entityai.c:1111`, `object.c:1136`); Expansion `eAIBase` (тоже AI_SERVER PlayerBase) получает
  урон через те же `super`-методы (`eAIBase.c:1307-1343`).
- `IsPlayerSelected()` (= `m_PlayerSelected`, `dayzplayerimplement.c:3803`) гейтит только тело-тик
  `OnScheduledTick` (`playerbase.c:2689` → `m_ModifiersManager.OnScheduledTick`,
  `GetBleedingManagerServer().OnTick`). Для AI_SERVER это `false` → ваниль НЕ тикает модификаторы и
  порезы. botorama уже тикает их вручную в `CommandHandler` (см. SKILL, «Системы тела»).
- `HandleDamageHit` (`dayzplayerimplement.c:1367`) — это **анимация реакции** на удар
  (`COMMANDID_DAMAGE`/`AddCommandModifier_Damage`), а НЕ применение HP. Отдельного метода
  `HandleDamage` в ванили нет.
- Цель зомби: `m_TargetableObjects` включает `PlayerBase` (`zombiebase.c:71`) → ИИ-бот — валидная
  цель зомби (атака до него доходит).

## Почему текущий хак не работает (`dmAISurvivorBase.c:896-920`)

Текущий override:
- `EEHitBy` → `super` + `RegisterDamageThreat` (это ок: `EEHitBy` у AI_SERVER зовётся).
- `EEOnDamageCalculated` → для `ZombieBase`-источника: `m_ProcessindDMG = true;`
  `ProcessDirectDamage(damageType, source, dmgZone, ammo, modelPos,
  damageResult.GetDamage(dmgZone,"Health") * 0.5);` `AddHealth("","Shock",-shock);`
  `m_ProcessindDMG = false; return false;`

Проблемы:

1. **Re-entrant `ProcessDirectDamage` внутри `EEOnDamageCalculated` = анти-паттерн
   «inconsistent damage».** Expansion явно пишет `//! Need to use Call() to avoid inconsistent
   damage` и применяет модифицированный урон **отложенно** через
   `g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Call(ProcessDamage, ...)` (`eAIDamageHandler.c:493-494`),
   а в колбэке возвращает `false` только после того, как отложил применение. Синхронный вложенный
   `ProcessDirectDamage` внутри нативного колбэка урона может не примениться (натив не реентерабелен) →
   оригинальный урон отменён (`return false`) И вложенный не применился → **ноль урона** — ровно
   симптом «урон не проходит».
2. **`damageResult.GetDamage(dmgZone,"Health") * 0.5` передаётся как `damageCoef` (множитель),
   а это абсолютное значение HP (~20-40).** Если бы вложенный `ProcessDirectDamage` сработал, это дало
   бы ×10-20 урона (мгновенная смерть), а не 0.5x. Логика коэффициента сломана: нужно передавать
   чистый множитель (`0.5`), а не абсолютный урон.
3. `AddHealth("","Shock",-damageShock)` берёт `damageResult.GetDamage("","Shock")` по глобальной
   зоне `""`; для зомби-хита зона может быть `"Torso"/"Head"`, а не `""` → shock может быть 0
   (мелочь на фоне п.1-2).

Разбор гипотез из задачи:
- «`EEOnDamageCalculated` возвращает `false` в не-зомби ветке» — **не подтверждено**: не-зомби
  ветка возвращает `true`.
- «`ProcessDirectDamage` зовётся не в том месте» — **подтверждено** как корневая причина
  (синхронный re-entrant вызов внутри нативного колбэка + неверный коэффициент).
- «`EEHitBy` не зовётся у AI_SERVER» — **опровергнуто**: `EEHitBy`/`EEKilled` зовутся (не гейтятся
  инстанс-типом). Порезы «не появляются» потому, что `EEHitBy` вообще не доходит до `ProcessHit` —
  урон отменён раньше, на уровне `EEOnDamageCalculated`.

## Правильная точка перехвата (рекомендация)

**Вариант A (минимальный фикс).** Убрать override `EEOnDamageCalculated` целиком (или
`return super.EEOnDamageCalculated(...)` = `true`) → ваниль применит полный зомби-урон;
`EEHitBy` уже перехватывает `RegisterDamageThreat`; порезы пойдут из `super.EEHitBy`
(`ProcessHit`), при условии что `BleedingManagerServer` тикает (уже тикается вручную в `CommandHandler`).

**Вариант B (редукция урона, как Expansion).** В `EEOnDamageCalculated` возвращать `false` ТОЛЬКО
когда реально модифицируем урон, а модифицированный урон применять ОТЛОЖЕННО:
- сохранить `damageType/source/dmgZone/ammo/modelPos` и чистый множитель (`0.5`);
- `g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Call(ApplyDeferredDamage, damageType, source, dmgZone,
  ammo, modelPos, coef);`
- `return false;`
- `ApplyDeferredDamage`: `m_ProcessindDMG = true; ProcessDirectDamage(damageType, source, dmgZone,
  ammo, modelPos, coef); m_ProcessindDMG = false;`
- в `EEOnDamageCalculated` при `m_ProcessindDMG` → `return true` (применить отложенный).

Это ровно паттерн `eAIDamageHandler` (`OnDamageCalculated` + `ProcessDamage`,
`eAIDamageHandler.c:493-497, 531-556`), включая детект «damage was not processed» (флаг
`m_ProcessDamage`, который после `ProcessDirectDamage` должен быть сброшен реентрантным колбэком).

## Открытые вопросы / не подтверждено

- Точное нативное поведение вложенного синхронного `ProcessDirectDamage` (реентерабельность) —
  не подтверждено напрямую (натив). Косвенно — Expansion-паттерн «avoid inconsistent damage».
- Какой `dmgZone` фактически приходит на зомби-хит по PlayerBase (`""` vs `"Torso"/"Head"`) —
  влияет на `GetDamage("","Shock")` и `GetDamage(dmgZone,"Health")` в хаке; не проверено логом.

## Источники (файл:строка)

- `3_game/damagesystem.c:22-23` — `CloseCombatDamage` / `CloseCombatDamageName` (proto native).
- `3_game/entities/object.c:1-7` — `ProcessDirectDamageFlags`; `:1130` — `ProcessDirectDamage`;
  `:1136` — `EEOnDamageCalculated` (дефолт `return true`).
- `3_game/entities/entityai.c:1021/1072/1111` — `EEHealthLevelChanged` / `EEKilled` / `EEHitBy`.
- `4_world/entities/creatures/infected/zombiebase.c:607-674` (атака), `:652/658/664`
  (`CloseCombatDamageName`), `:71` (`m_TargetableObjects` включает `PlayerBase`).
- `4_world/entities/manbase/playerbase.c:1045-1084` (`EEKilled`), `:1086-1209` (`EEHitBy`),
  `:1120-1124` (порезы `ProcessHit`), `:2689` (`OnScheduledTick` гейт `!IsPlayerSelected()`).
- `4_world/entities/dayzplayerimplement.c:1367` (`HandleDamageHit` — анимация), `:3803`
  (`IsPlayerSelected()` = `m_PlayerSelected`).
- `DayZ-Expansion-Scripts/.../eAIBase.c:1307-1343` — `EEOnDamageCalculated`/`EEHitBy` = `super`.
- `DayZ-Expansion-Scripts/.../eAIDamageHandler.c:493-497, 531-556` — отложенный `ProcessDamage`
  (`Call()` + `ProcessDirectDamage`, «avoid inconsistent damage»).

# Полёт пули и затухание (air friction / отложенный урон)

## Цель

Выяснить, почему выстрел ИИ-бота через `Weapon_Base.Fire(mi, pos, dir, speed)` выглядит
«мгновенным» (урон / эффект попадания в землю / шум возникают сразу, без времени полёта), и что
нужно, чтобы (1) момент урона/прилёта зависел от скорости пули с учётом air friction,
(2) остаточный урон зависел от дистанции.

## Вывод (кратко)

1. **`Fire()` и `TryFireWeapon` спавнят НАСТОЯЩИЙ баллистический снаряд**, который симулирует
   ДВИЖОК на сервере (не скрипт). Урон + эффекты прилёта (`DayZGame.FirearmEffects` → NoiseHit)
   применяются движком **в момент попадания** (с учётом времени полёта), а НЕ при вызове
   `Fire()`. Т.е. «мгновенного» применения урона в движке нет.
2. **НО** у натива `Weapon::Fire` есть ванильный баг **T186177** (подтверждён комментарием
   Expansion): урон выстрела применяется **с опозданием** (в момент СЛЕДУЮЩЕГО выстрела), а
   первый выстрел по новой сущности может попасть в **предыдущую** цель. Поэтому доверять
   таймингу/сущности движка нельзя — Expansion полностью пересчитывает время полёта и остаточный
   урон в скрипте и применяет урон сам через `ProcessDirectDamage`.
3. «Мгновенность» в botorama, скорее всего, НЕ означает «движок применяет урон сразу» (движок
   летит с air friction). Наблюдение объясняется либо короткой дистанцией боя (десятки мс полёта),
   либо тем, что botorama вообще не считает время полёта/затухание (нет travelTime с air friction,
   нет dmgCoef, нет отложенного урона) — и при T186177 урон/эффект приезжают по непредсказуемому
   таймингу. Точная первопричина «мгновенности» — **не подтверждена** (см. «Открытые вопросы»).

## 1. Натив `Fire(mi, pos, dir, speed)` vs `TryFireWeapon` — что спавнит движок

- `proto native bool Fire(int muzzleIndex, vector pos, vector dir, vector speed)`
  (`4_world/entities/core/inherited/weapon.c:58`).
- `proto native bool TryFireWeapon(EntityAI weapon, int muzzleIndex)`
  (`3_game/systems/inventory/weaponinventory.c:8`).
- Оба — нативы. Ванильный weapon-FSM (`WeaponFire.OnEntry` и наследники, `weaponfire.c:66/121/155/
  166/294`) зовёт `TryFireWeapon`, которая сама берёт transform дула (`GetCameraPoint`,
  `weapon.c:413`) и спавнит пулю. `Fire()` — низкоуровневый аналог с явными `pos/dir`.
- **Доказательство, что движок симулирует полёт с air friction** (а не мгновенный hitscan):
  - `DayZGame.FirearmEffects(..., inSpeed, ...)` (`3_game/dayzgame.c:3540`) получает `inSpeed` =
    скорость пули **в момент удара** (уже уменьшенную air friction) и считает шум попадания как
    `inSpeed.Length() / ConfigGetFloat("cfgAmmo " + ammoType + " initSpeed")` (`dayzgame.c:3585`).
    Значит движок знает скорость пули на каждом участке траектории.
  - Expansion реимплементирует время полёта с комментарием «In DayZ, max projectile travel time
    is 6 seconds» (`eAI_CalculateProjectileTravelTime`, AI `Weapon_Base.c:590`) и
    `speedCoef = e^(airFriction·distance)` (`Weapon_Base.c:519`) — т.е. берёт те же
    `CfgAmmo`-параметры, которыми пользуется движок.
  - Комментарий к багу T186177 (`eAIDamageHandler.c:185-189`): «firing over a longer distance
    (several hundred meters) to ensure the projectile **is in flight for a certain amount of
    time**» — прямое подтверждение, что снаряд летит.
- **Семантика `speed`** — направление, не величина: Expansion передаёт `Fire(mi, pos, dir, dir)`
  (`AI Weapon_Base.c:157`); величину скорости движок берёт из `CfgAmmo <ammo> initSpeed`
  (× `initSpeedMultiplier` музла). (Уже отражено в секции «AI weapon fire — direction + modes» §1.)

## 2. Как движок/Expansion считают затухание скорости и остаточный урон

Конфиг `CfgAmmo <bullet>` (ваниль `DayZ Projects/DZ/weapons/projectiles/config.cpp`; напр.
`Bullet_556x45`: `initSpeed=850; typicalSpeed=1000; airFriction=-0.00125`). `airFriction`
**отрицателен** (затухание). Параметры: `initSpeed`, `typicalSpeed`, `airFriction`,
`initSpeedMultiplier` (музл, из конфига **оружия** — читается экземплярным `ConfigGetFloat`, не
`g_Game.ConfigGetFloat`).

Точные формулы Expansion (`AI Weapon_Base.c`):

- **speedCoef** (коэффициент скорости на дистанции `d`), `:506-520`:
  ```
  airFriction = g_Game.ConfigGetFloat("CfgAmmo " + ammoType + " airFriction");   // :516
  distance = vector.Distance(origin, hitPosition);                                // :517
  speedCoef = Math.Pow(Math.EULER, airFriction * distance);                       // :519  = e^(airFriction·d)
  ```
- **initSpeed (эффективная)**, `:546-550`:
  ```
  initSpeed = ConfigGetFloat("CfgAmmo " + ammoType + " initSpeed");              // :546
  initSpeedMultiplier = ConfigGetFloat("initSpeedMultiplier");                    // :547 (музл)
  if (initSpeedMultiplier) initSpeed *= initSpeedMultiplier;
  ```
- **dmgCoef**, `:552-574`:
  ```
  typicalSpeed = ConfigGetFloat("CfgAmmo " + ammoType + " typicalSpeed") * damageOverride; // :552-554
  speed = initSpeed * speedCoef;                                                  // :558
  if (typicalSpeed != initSpeed)
      dmgCoef = (speed > typicalSpeed) ? 1.0 : speed / typicalSpeed;              // clamp 1.0
  else
      dmgCoef = speedCoef;                                                        // вырожденный случай
  ```
- **travelTime** (пошаговое интегрирование, `:580-606`):
  ```
  // simulationStep = 0.05, макс 6.0 c
  while (distanceTraveled < distance && timeTraveled < 6.0) {
      speed = Math.Pow(Math.EULER, airFriction * distanceTraveled) * initSpeed;   // :595
      distanceTraveled += speed * simulationStep;                                 // :599
      timeTraveled += simulationStep;                                             // :594
  }
  travelTime = ExpansionMath.LinearConversion(distanceTraveledPrev, distanceTraveled, distance,
                                              timeTraveledPrev, timeTraveled);    // :603
  ```
- **drop** (компенсация дропа, `:611-616`): `drop = 0.5 * 9.81 * travelTime^2`.

## 3. Как Expansion откладывает урон/эффект до «прилёта»

(Источник — `eAIDamageHandler.c`, номера строк оттуда, если не сказано иначе.)

- **Выстрел запоминается**: `Weapon_Base.eAI_Fire` (`AI Weapon_Base.c:60-164`) делает свой hitscan
  (`Hitscan`, `:37-58`), создаёт `eAIShot(this, mi, pos, dir, hitObject, hitPosition, component)`
  и кладёт в `ai.m_eAI_FiredShots` (`:124-125`).
- **`eAIShot`** (`eAIDamageHandler.c:1-63`) хранит: `m_Time` (время выстрела, `g_Game.GetTime()`),
  `m_Weapon`, `m_Origin`, `m_Direction`, `m_HitObject`/`m_HitObjectRoot` (`GetHierarchyRoot()`),
  `m_HitPosition`, `m_Component`, `m_Ammo` (`weapon.GetCartridgeInfo(...)`, `:43`), `m_Distance`,
  `m_SpeedCoef`, `m_DamageCoef` (`eAI_CalculateProjectileDamageCoefAtPosition`, `:48`),
  `m_TravelTime` (`eAI_CalculateProjectileTravelTime`, `:49`).
- **Перехват события урона**: на ЦЕЛИ (любой EntityAI — `DayZPlayerImplement`/`ZombieBase`/
  `AnimalBase`/`ItemBase`/`CarScript`) override `EEOnDamageCalculated(...)` (ванильный хук
  `object.c:1136`) → `m_eAI_DamageHandler.OnDamageCalculated(...)` (см. `DayZPlayerImplement.c:407-416`).
- **`OnDamageCalculated`** (`eAIDamageHandler.c:148`): для `DT_FIRE_ARM` сопоставляет входящее
  событие с записанным `eAIShot` (по `source == shot.m_Weapon && ammo == shot.m_Ammo` и
  `rootEntity == shot.m_HitObjectRoot`, `:205-231`). Если точного совпадения нет — ищет
  «кандидатов» (выстрел в эту сущность, ещё не обработан) и:
  ```
  elapsed = (time - candidate.m_Time) * 0.001;                   // :251
  travelTimeRemaining = candidate.m_TravelTime - elapsed;        // :252
  if (travelTimeRemaining > 0.05)
      GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CheckCandidate,
          travelTimeRemaining * 1000, false, candidate, ai, modelPos, dir, travelTimeRemaining, dmgZone); // :255
  else if (CheckCandidate(...)) break;                           // :256 — уже «прилетело», сразу
  ```
  и возвращает `false` (запрещает движку применить «сырой» урон, `:277`).
- **`CheckCandidate`** (`:559-664`): по таймеру проверяет, что цель всё ещё там (коллижен-бокс +
  `Math3D.IntersectRayBox`, `:574-583`), уточняет `dmgZone` (`GetDamageZoneNameByComponentIndex` +
  редиректы Head/Brain/Torso, `:596-618`), затем применяет **остаточный** урон сам:
  ```
  match.m_HitObjectRoot.ProcessDirectDamage(DT_FIRE_ARM, match.m_Weapon, dmgZone,
      match.m_Ammo, match.m_HitPosition, match.m_DamageCoef);    // :639
  ```
- **`ProcessDamage`** (`:531-556`) — обёртка, зовущая `ProcessDirectDamage` через
  `GetCallQueue(CALL_CATEGORY_SYSTEM).Call(...)` («avoid inconsistent damage», `:493-494`) с
  `m_ProcessDamage`-флагом для детекта «damage was not processed».
- **Итого**: движок лишь даёт триггер `EEOnDamageCalculated` (ненадёжный по таймингу/сущности из-за
  T186177). Expansion сопоставляет его со своим hitscan-рекордом, при необходимости **откладывает**
  `ProcessDirectDamage` на `travelTimeRemaining` и применяет с `m_DamageCoef` (остаточный урон по
  дистанции).

## 4. Что уже есть в botorama и что переиспользовать/переписать

Текущее состояние (прочитано):
- `reg/4_World/modded_WeaponBase.c:34-75` `dmBot_Fire` → `Fire(mi, pos, direction, velocity)`
  (`velocity` = единичное `direction`, `:64`); после выстрела — `ApplyRecoil` + SHOT-шум
  (`dmNoiseSystem.AddNoise`, `:69`). `ComputeShot` (`dmAISurvivorBase.c:583-591`) =
  `GetShotOrigin` + `GetAimWorldDirection` + дисперсия + `CompensateBulletDrop` +
  `ComputeShotVelocity`.
- `dmAISurvivorBase.c:608-629` `CompensateBulletDrop` — райкаст → `ComputeBulletTravelTime` →
  `drop = 0.5·g·t²` → наклон ствола (корректный шаблон, но время полёта без air friction).
- `dmAISurvivorBase.c:633-639` `ComputeBulletTravelTime` = `distance / initSpeed` (**без air
  friction** — главное, что переписать).
- `dmAISurvivorBase.c:643-654` `GetAmmoInitSpeed` — читает `CfgMagazines <mag> ammo` →
  `CfgAmmo <bullet> initSpeed` (**не учитывает `initSpeedMultiplier`** музла).
- `ComputeShotVelocity` (`:576-579`) возвращает `direction` (единичный) — ок, оставить.
- `core/3_Game/modded/modded_DayZGame.c:8-18` — override `FirearmEffects` добавляет
  BULLETIMPACT-шум (`dmNoiseSystem.AddNoise(null, pos, 15.0, BULLETIMPACT)`). Происходит в момент,
  когда ДВИЖОК вызывает `FirearmEffects` (= момент прилёта), т.е. уже «честно» отложено движком.
- `dmAISurvivorBase.c:898-917` `EEHitBy` — перехват ПОСЛЕ применения урона (для угрозы), не влияет
  на тайминг.

Переиспользовать (не трогать): `GetShotOrigin`, `GetAimWorldDirection`,
`ApplyPersonalDispersion`/`ApplyWeaponDispersion`, `ComputeShotVelocity` (возвращает `direction`),
`ApplyRecoil`, обёртку `dmBot_Fire` → `Fire(mi,pos,dir,dir)`, `modded_DayZGame.FirearmEffects`
(шум прилёта уже в нужной точке).

Переписать/добавить:
1. **`ComputeBulletTravelTime`** → заменить на пошаговое интегрирование Expansion
   (`eAI_CalculateProjectileTravelTime`, `AI Weapon_Base.c:580-606`) с `airFriction` и `initSpeed`.
2. **`GetAmmoInitSpeed`** → добавить `initSpeedMultiplier` (`ConfigGetFloat("initSpeedMultiplier")`,
   музл) и читать `airFriction`/`typicalSpeed` (нужны для dmgCoef).
3. **Новые** `ComputeSpeedCoef`/`ComputeDamageCoef` (формулы §2: `speedCoef = e^(airFriction·d)`,
   `dmgCoef = speed/typicalSpeed` с clamp 1.0).
4. **`CompensateBulletDrop`** → использовать новый `ComputeBulletTravelTime` (с air friction),
   иначе компенсация дропа на дальних дистанциях будет чуть неточной (вторично).
5. **Запись выстрела + отложенный урон**: аналог `eAIShot` (запомнить hitscan-цель,
   `m_TravelTime`, `m_DamageCoef` на момент выстрела) + перехват `EEOnDamageCalculated` на цели →
   при `DT_FIRE_ARM` отложить/редиректить `ProcessDirectDamage(DT_FIRE_ARM, weapon, dmgZone,
   ammo, hitPos, m_DamageCoef)` через `CallLater` на `travelTimeRemaining` (эталон
   `eAIDamageHandler`). Это даст и (1) время прилёта по air friction, и (2) остаточный урон по
   дистанции.
   - Минимальный вариант без полного eAIShot-механизма: если доверять движку (не T186177), то
     движок УЖЕ применяет урон/шум в момент прилёта — тогда достаточно исправить ТОЛЬКО
     `ComputeBulletTravelTime`+`ComputeDamageCoef` и передать остаточный урон. **Но** это не
     решает «мгновенность», если её первопричина — T186177/десинк; надёжный путь — eAIShot.

## Открытые вопросы / не подтверждено

- **Первопричина «мгновенности» не подтверждена** на скриптовом уровне: нативы `Fire`/
  `TryFireWeapon` — движок. Судя по косвенным признакам движок летит с air friction и применяет
  урон в момент прилёта (не мгновенно). Нужен натурный тест: выстрел на ~500 м и замер дельты
  между `dmBot_Fire` и `EEHitBy`/`FirearmEffects` цели (через `GetTickTime` в логах).
- Точное поведение `Fire(mi,pos,dir,speed)` (явные pos/dir) vs `TryFireWeapon` (camera point) на
  сервере в части тайминга прилёта — не подтверждено; Expansion использует `Fire(...)` и всё равно
  городит собственный тайминг (косвенное указание, что нативу верить нельзя).
- Реентерабельность синхронного `ProcessDirectDamage` из `EEOnDamageCalculated` — не подтверждена
  (натив); Expansion обходит через `Call()`/`CallLater`.

## Источники (файл:строка)

- `4_world/entities/core/inherited/weapon.c:58` — `Fire` (proto native); `:413` — `GetCameraPoint`.
- `3_game/systems/inventory/weaponinventory.c:8` — `TryFireWeapon` (proto native).
- `4_world/entities/firearms/fsm/states/weaponfire.c:66/121/155/166/294` — ваниль: `TryFireWeapon`.
- `3_game/dayzgame.c:3540-3591` — `FirearmEffects` (`inSpeed` → шум `inSpeed.Length()/initSpeed`).
- `3_game/entities/object.c:1130` — `ProcessDirectDamage`; `:1136` — `EEOnDamageCalculated`.
- `3_game/entities/entityai.c:1111` — `EEHitBy`.
- `DayZ Projects/DZ/weapons/projectiles/config.cpp:3922-3973` — `CfgAmmo Bullet_556x45`
  (`initSpeed=850, typicalSpeed=1000, airFriction=-0.00125`).
- `DayZ-Expansion-Scripts/.../AI/.../Entities/Weapons/Firearms/Weapon_Base.c:506-606` —
  `eAI_CalculateProjectileSpeedCoefAtPosition` / `DamageCoefAtPosition` / `TravelTime` / `Drop`;
  `:60-164` — `eAI_Fire` (hitscan + `Fire(mi,pos,dir,dir)`).
- `DayZ-Expansion-Scripts/.../AI/Classes/eAIDamageHandler.c:1-63` (`eAIShot`), `:148-509`
  (`OnDamageCalculated`), `:255` (`CallLater(CheckCandidate)`), `:559-664` (`CheckCandidate` →
  `ProcessDirectDamage` `:639`), `:531-556` (`ProcessDamage`).
- `DayZ-Expansion-Scripts/.../AI/.../Entities/DayZPlayerImplement.c:407-416` — `EEOnDamageCalculated`
  → `eAIDamageHandler.OnDamageCalculated`.
- botorama: `reg/4_World/modded_WeaponBase.c:34-75` (`dmBot_Fire`),
  `core/4_World/Entities/Bot/dmAISurvivorBase.c:576-654` (`ComputeShotVelocity`/`ComputeShot`/
  `CompensateBulletDrop`/`ComputeBulletTravelTime`/`GetAmmoInitSpeed`),
  `core/3_Game/modded/modded_DayZGame.c:8-18` (`FirearmEffects` → BULLETIMPACT-шум),
  `cons/4_World/constants.c:432-438` (`DM_AI_SHOT_MAX_DISTANCE`, `DM_AI_GRAVITY`,
  `DM_AI_DEFAULT_INIT_SPEED`).
