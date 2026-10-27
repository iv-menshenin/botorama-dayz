# Research: мили-бой — блок/удар на уровне пешки (без мозга/FSM)

Статус: сигнатуры «удар»/«блок» подтверждены чтением кода; эмпирические вопросы —
в конце (`[нужно проверить]`). Дополняет `docs/research/combat.md` (там — общий путь
мили/огнестрела, здесь — только примитивы для переписывания мили по алгоритму Expansion
и e2e-ручки, шлющие сигнал прямо в `dmAISurvivorBase`).

Ведёт субагент `dayz-research`.

## Цель

Подтвердить два примитива «удар» и «блок» на уровне пешки `dmAISurvivorBase` (без
`dmAISurvivor`/FSM/интентов) и выписать точные сигнатуры, чтобы `dayz-dev` сделал
e2e-ручки `strike <entity>` / `block on|off` / `evade <angle>`, зовущие пешку напрямую.

Сокращения путей (везде ниже `файл:строка`):

- V: `/home/devalio/dayz/Work/DayZ Projects/scripts/`
- E: `/home/devalio/dayz/Work/DayZ-Expansion-Scripts/DayZExpansion/AI/Scripts/4_World/DayZExpansion_AI/`
- M: `/home/devalio/dayz/Work/botorama/src/core/4_World/Entities/Bot/`

---

## 1. Механизм блока (главное)

### Ваниль — блок только из raised-стойки + угол ввода назад

`HandleFightLogic` (`V/4_world/entities/manbase/dayzplayer/dayzplayermeleefightlogic_lightheavy.c:137-279`).
Ветка блока/эвейда — `else if (!isFireWeapon && pCurrentCommandID == COMMANDID_MOVE)` (`:202`):

- Блок (`:254-276`):
  ```c
  if (!isFireWeapon && (pMovementState.m_iStanceIdx == STANCEIDX_RAISEDERECT
      || pMovementState.m_iStanceIdx == STANCEIDX_RAISEDPRONE))
  {
      float angle2;
      if (hcm.GetCurrentInputAngle(angle2) && (angle2 < -130.0 || angle2 > 130))
      { hcm.SetMeleeBlock(true); SetBlock(true); }
      else
      { hcm.SetMeleeBlock(false); SetBlock(false); }
  }
  else
  { hcm.SetMeleeBlock(false); SetBlock(false); }
  ```
  Т.е. ваниль требует **нативную raised-стойку** (`STANCEIDX_RAISEDERECT/RAISEDPRONE`) И
  **угол ввода назад** (`GetCurrentInputAngle` ∈ (130, 180] ∪ [-180, -130) — клавиша «назад»).
- Эвейд (`:205-238`): `STANCEIDX_RAISEDERECT` + `pInputs.IsMeleeLREvade()` → `hcm.StartMeleeEvadeA(angle)`
  (`:229`), `angle = -90` (влево) / `+90` (вправо), `SetCooldown(EVADE_COOLDOWN, EVADE)` (`:228`).

### Expansion — блок из ERECT + кастомный `IsRaised()` + угол фактического движения

`eAIMeleeFightLogic_LightHeavy.HandleFightLogic` (`E/Classes/Melee/eaimeleefightlogic_lightheavy.c:52-249`).
Блок/эвейд — секция `if (!isFireWeapon && hcm)` (`:75-190`):

- Эвейд (`:80-160`): `stance == STANCEIDX_ERECT && m_AI.IsRaised()` (`:87`); 50% шанс при
  `target.GetAttackCooldown() > EVADE_COOLDOWN` → `roll = RandomIntInclusive(-1,1)`; при roll≠0 и
  стамине → `m_IsEvading = true; SetCooldown(EVADE_COOLDOWN, EVADE); hcm.StartMeleeEvadeA(angle)`
  (`:150-152`), `angle = ±90`.
- Блок (`:162-186`):
  ```c
  if ((stance == STANCEIDX_ERECT || stance == STANCEIDX_PRONE) && m_AI.IsRaised())
  {
      float angle2 = m_AI.m_eAI_CommandMove.GetCurrentMovementAngle();
      if (angle2 < -130.0 || angle2 > 130 || block)   // block = «враг атакует» (см. ниже)
      { hcm.SetMeleeBlock(true); SetBlock(true); }
      else
      { hcm.SetMeleeBlock(false); SetBlock(false); }
  }
  else
  { hcm.SetMeleeBlock(false); SetBlock(false); }
  ```
  `block`-флаг («враг начал атаку → ставим блок до конца атаки/таймаута») — `:102-127`:
  `else if (!target.IsPlayer() && target.GetAttackCooldown() > 0)` → `m_eAI_BlockEndTime =
  GetTime() + RandomInt(1500,3000); block = true`. Удержание — `:93-101` (`m_IsInBlock &&
  target.IsFighting() && GetTime() < m_eAI_BlockEndTime` → `block = true`).

**Разница ваниль ↔ Expansion**: ваниль читает **нативную стойку** `STANCEIDX_RAISEDERECT` и
**инпут-угол** `GetCurrentInputAngle`; Expansion читает **кастомный `IsRaised()`** (флаг
`m_WeaponRaised`) и **угол фактического движения** `m_eAI_CommandMove.GetCurrentMovementAngle()`.

`GetCurrentMovementAngle()` у Expansion — это `eAICommandMove.GetCurrentMovementAngle()` =
`return m_MovementDirection` (`E/Classes/Commands/eAICommandMove.c:280-283`), т.е. **желаемое
направление движения** (0 = вперёд, ±180 = назад), а не нативный фильтрованный угол. Тот же
конвенции придерживается наш `SetMove(angle, speed)` (`M/dmAISurvivor.c:903-906`:
`angle -180..180, 0 = forward, ±90 = strafe, ±180 = backward`).

**Кто зовёт `SetMeleeBlock`**: fight-logic (`HandleFightLogic`) из `CommandHandler` пешки каждый
кадр (`V/4_world/entities/dayzplayerimplement.c:2642-2649`). Входы для блока: **raised-флаг**
(`IsRaised()`) + **угол движения назад** (`|angle2| > 130`) ИЛИ **триггер «враг атакует»**
(`target.GetAttackCooldown() > 0`).

---

## 2. Гашение урона при блоке — проверяется СКРИПТОВЫЙ флаг, не натив

Ключевой факт: **в ванилле нативный `HumanCommandMove.SetMeleeBlock(...)` нигде не ЧИТАЕТСЯ как
гейт урона** (только пишется). Редукция урона читает **скриптовый флаг `IsInBlock()`** =
`m_IsInBlock` (`SetBlock(bool)` `:58-61`). `SetMeleeBlock` гонит только визуальную позу блока.

Два пути гашения:

### (а) Игрок бьёт игрока — `EvaluateHit_Player`

`EvaluateHit` (`:577-602`) → для цели-`DayZPlayer` → `EvaluateHit_Player` (`:604-635`):

```c
float hitAngle = RAD2DEG * AbsFloat(AngleFromPosition(targetPos, GetHeadingVector(targetPlayer), agressorPos));
if (targetPlayer.GetMeleeFightLogic() && targetPlayer.GetMeleeFightLogic().IsInBlock()
    && hitAngle <= GameConstants.PVP_MAX_BLOCKABLE_ANGLE)
{
    if (weaponMode > 0) forcedWeaponMode = --weaponMode; // Heavy -> Light
    else                forcedDummy = true;              // Light -> dummy (без урона)
}
EvaluateHit_Common(weapon, target, forcedDummy, forcedWeaponMode);
```

### (б) Зомби бьёт игрока — `ZombieBase.FightLogic`

`V/4_world/entities/creatures/infected/zombiebase.c:640-660`:

```c
playerInBlockStance = playerTarget.GetMeleeFightLogic() && playerTarget.GetMeleeFightLogic().IsInBlock();
...
if (playerInBlockStance && (RAD2DEG * AbsFloat(AngleFromPosition(targetPos, GetHeadingVector(playerTarget), zombiePos))) <= GameConstants.AI_MAX_BLOCKABLE_ANGLE)
{
    if (m_ActualAttackType.m_IsHeavy == 1)
        DamageSystem.CloseCombatDamageName(this, target, target.GetHitComponentForAI(), "MeleeZombie", hitPosWS);  // тяжёлый -> лёгкий
    else
        DamageSystem.CloseCombatDamageName(this, target, target.GetHitComponentForAI(), "Dummy_Light", hitPosWS); // лёгкий -> без урона (анимация)
}
else
    DamageSystem.CloseCombatDamageName(this, target, target.GetHitComponentForAI(), m_ActualAttackType.m_AmmoType, hitPosWS); // полный урон
```

Константы угла: `AI_MAX_BLOCKABLE_ANGLE = 60` (`V/3_game/constants.c:1069`),
`PVP_MAX_BLOCKABLE_ANGLE = 60` (`:1089`). Т.е. цель должна быть **развёрнута к атакующему** (±60°).

Блок-звук при попадании в блок: `PlayerBase.EEHitByRemote` (`V/.../manbase/playerbase.c:1349-1357`)
и `OnPlayerRecievedHit` (`:1393-1399`) играют `BlockingAttack_SoundSet` при `IsInBlock()`.

### Вывод по механике блока (для e2e-ручки `block`)

Для гашения входящего мили-урона у server-side ИИ-пешки достаточно:
1. `pawn.GetMeleeFightLogic().SetBlock(true)` — **обязателен** (скриптовый флаг, читается
   зомби/игроком в `FightLogic`/`EvaluateHit_Player`).
2. Развернуть пешку к атакующему (±60°) — `IsInBlock()` один не гасит урон без facing-угла.
3. `pawn.GetCommand_Move().SetMeleeBlock(true)` — **опционален**, только визуальная поза.

`[нужно проверить]` — ключевой вопрос задачи: достаточно ли прямого
`GetMeleeFightLogic().SetBlock(true)` с сервера у `INSTANCETYPE_AI_SERVER`-пешки, чтобы зомби
(`ZombieBase.FightLogic`) реально понизил/обнулил урон, БЕЗ raised-стойки и блок-анимации в
графе. Эмпирика: заспавнить зомби (`init_ai=true`), бот `SetBlock(true)` + развернут к зомби,
ждать удар → замерить `GetHealth("","Health")` бота: (ожидается) лёгкий удар = 0 урона
(`Dummy_Light`), тяжёлый = пониженный (`MeleeZombie`). Контроль: без блока — полный урон. Ручка —
`botdump` здоровья + счётчик урона через `EEHitBy` лог.

---

## 3. Сигнатуры нативных методов (для прямого вызова из пешки)

Источник нативов: `V/3_game/human.c` (команды/движение) и
`V/4_world/entities/manbase/dayzplayer/dayzplayermeleefightlogic_lightheavy.c` (fight-logic).

### `HumanCommandMove` (`human.c:432-496`), все `proto native`, public

| Метод | Строка | Назначение |
|---|---|---|
| `float GetCurrentMovementAngle()` | `:440` | фактический угол движения (-180..180), 0=вперёд |
| `bool GetCurrentInputAngle(out float)` | `:445` | `proto bool` (НЕ native), false в idle; клавиатурный угол |
| `void StartMeleeEvade()` | `:466` | эвейд без угла |
| `void StartMeleeEvadeA(float pDirAngle)` | `:469` | эвейд с углом (±90) |
| `bool IsMeleeEvade()` | `:472` | в эвейде сейчас? |
| `void SetMeleeBlock(bool pBlock)` | `:475` | визуальный блок (поза) |
| `void ForceStance(int pStanceIdx)` | `:479` | форс стойки (или -1 = снять лок) |

### `HumanCommandMelee2` (`human.c:535-567`), `proto native`, public

`IsInComboRange()` `:550`, `WasHit()` `:553` (true ровно раз после Hit-события), `Cancel()` `:556`,
`ContinueCombo(bool, float, EntityAI, vector)` `:547`, `GetComboCount()` `:561`,
`GetCurrentHitType()` `:563`, `IsFinisher()` `:565`. Константы `HIT_TYPE_*` `:541-544`
(LIGHT=0/HEAVY=1/FINISHER=2/FINISHER_NECK=3).

### Старт удара / доступ к командам (`human.c`)

- `HumanCommandMove GetCommand_Move()` `:1447`; `HumanCommandMelee2 GetCommand_Melee2()` `:1459`.
- `HumanCommandMelee2 StartCommand_Melee2(EntityAI pTarget, int pHitType, float pComboValue, vector hitPos = vector.Zero)` `:1457`.

### Fight-logic `DayZPlayerMeleeFightLogic_LightHeavy` — видимость членов

| Член | Строка | Видимость | Доступно из `dmBotMeleeFightLogic_LightHeavy` |
|---|---|---|---|
| `bool IsInBlock()` | `:53` | public | да |
| `void SetBlock(bool)` | `:58` | public | да (прямой вызов) |
| `bool IsEvading()` | `:63` | public | да |
| `bool CanFight()` | `:73` | public | да |
| `bool HandleFightLogic(...)` | `:137` | public | override |
| `void SetCooldown(float, EFightLogicCooldownCategory)` | `:790` | protected | да (super/прямо) |
| `m_IsInBlock` / `m_IsEvading` | `:25-26` | protected | да (поле) |
| `EvaluateHit` / `EvaluateHit_Player` / `EvaluateHit_Infected` / `EvaluateHit_Common` | `:577-703` | protected | да (override/super) |
| `GetAttackTypeFromInputs` | `:90` | protected | да (override) |
| `GetTargetData` / `GetAttackTypeByDistanceToTarget` | `:744/120` | protected | да |

Итого: из нашего подкласса доступны **все** нужные примитивы блока/эвейда/удара:
`SetBlock`, `IsInBlock`, `IsEvading`, `SetCooldown`, `m_IsInBlock`, `m_IsEvading`,
`EvaluateHit*`, плюс нативы `hcm.SetMeleeBlock`, `hcm.StartMeleeEvadeA`,
`GetCommand_Melee2().IsInComboRange()`.

### `DayZPlayerImplement` — raise-методы (ваниль vs Expansion vs наш)

- Ваниль НЕ имеет метода `RaiseWeapon` (это имя — только у Expansion `eAIBase.c:9728` и у нас
  `M/dmAISurvivorBase.c:426`). Ванильные: `CompleteWeaponRaise()` `V/.../dayzplayerimplement.c:911`,
  `ResetWeaponRaiseProgress()` `:920`, `IsWeaponRaiseCompleted()` → `m_WeaponRaiseCompleted` `:926`,
  `IsRaised()` → `m_IsRaised` `:3861` (обновляется из `m_MovementState.IsRaised()` в CommandHandler
  `:2299-2302`), `IsFireWeaponRaised()` → `m_MovementState.IsRaised()` `:309`.
- Expansion `override IsRaised()` → `m_WeaponRaised` (`E/Entities/AI/eAIBase.c:9746`);
  `override IsWeaponRaiseCompleted()` → `m_WeaponRaisedTimer > 0.5` (`:9751`).
- Наш `override IsRaised()` → `m_WeaponRaised` (`M/dmAISurvivorBase.c:546-548`),
  `override IsWeaponRaiseCompleted()` → `m_WeaponRaisedTimer > 0.5` (`:552-554`) — совпадает с
  Expansion-паттерном.

---

## 4. Raised-маппинг (подтверждено)

Expansion-мили проверяет **кастомный флаг `m_WeaponRaised`**, а НЕ нативную стойку
`STANCEIDX_RAISEDERECT`:

- `RaiseWeapon(bool up=true)` ставит `m_WeaponRaised = up` (`eAIBase.c:9728-9733`); анимация —
  `AnimSetBool(m_VAR_Raised, m_WeaponRaised)` (`:9536`, `:9742` в `eAI_ResetRaised`).
- `eAI_GetStance()` нормализует стойку `-= STANCEIDX_RAISED` (`:5840-5848`) — raised не считается
  отдельной стойкой, а флагом поверх `ERECT/CROUCH/PRONE`.
- В fight-logic блок/эвейд/удар гейтятся `stance == STANCEIDX_ERECT && m_AI.IsRaised()`
  (`eaimeleefightlogic_lightheavy.c:87/162/212`), а НЕ `STANCEIDX_RAISEDERECT`.

У нас эквивалент уже есть: кастомная граф-переменная `dmAI_Raised`
(`M/dmAISurvivorBase.c:389` `BindVariableBool("dmAI_Raised")`), пишется в `ApplyWeaponRaise`
(`:630-631` `AnimSetBool(m_VarRaised, m_WeaponRaised)`), флаг `m_WeaponRaised` ставит
`RaiseWeapon` (`:426-434`), `IsRaised()` → `m_WeaponRaised` (`:546`).

**Вывод**: для «удара/блока по подходу Expansion» достаточно поднять **наш** `dmAI_Raised`
(`RaiseWeapon(true)` → `ApplyWeaponRaise`), НЕ переводя бота на нативную raised-стойку
(`ForceStance(RAISEDERECT)` НЕ нужен). Гейт в fight-logic делать по
`stance == STANCEIDX_ERECT && IsRaised()` (наш `IsRaised()` = `m_WeaponRaised`). См. также
`docs/research/combat.md` §4 (вариант (а) vs (б)) — здесь подтверждается именно вариант (а).

`[нужно подтвердить]`: вызовы `SetMeleeBlock`/`StartMeleeEvadeA` и `StartCommand_Melee2` идут из
**голой ERECT** (не raised) в Expansion — т.е. нативная блок-поза/эвейд-анимация стартуют из
ERECT-команды MOVE без raised-стойки. Эмпирика: бот в ERECT + `dmAI_Raised=true` → `block on` →
наблюдатель видит поднятие рук/блок-позу (или позы нет — тогда визуал блокировать отдельной
анимацией). Влияет только на визуал, не на гашение урона (§2).

---

## 5. Путь удара на уровне пешки (подтверждено — без мозга/FSM)

Цепочка удара целиком в пешке, мозг только ставит one-shot флаг:

1. `RequestMeleeAttack(target)` (`M/dmAISurvivorBase.c:1808-1819`): `m_MeleeAttackRequest = true;
   m_MeleeTarget = target; m_MeleeStrikeYaw = GetOrientation()[0]`.
2. `dmAISurvivorBase.CommandHandler` (`:1074`) → `super.CommandHandler(...)` (`:1108`) → ванильный
   `DayZPlayerImplement.CommandHandler` (`V/.../dayzplayerimplement.c:2642-2649`) зовёт
   `m_MeleeFightLogic.HandleFightLogic(...)` — наш override
   (`M/Melee/dmBotMeleeFightLogic_LightHeavy.c:38-86`).
3. `dmBotMeleeFightLogic_LightHeavy` читает `m_Bot.HasMeleeAttackRequest()` (`:49`), где
   `m_Bot = dmAISurvivorBase.Cast(player)` (Init `:13-17`); цель — `m_MeleeCombat.GetTargetEntity()`
   (`:66`, выбран `dmBotMeleeCombat.TargetSelection` из `pawn.GetMeleeAttackTarget()`), удар —
   `m_Player.StartCommand_Melee2(null, heavy, 1.0, hitPos)` (`:73`), затем `ConsumeMeleeAttackRequest()`
   (`:84`).

Ни `dmAISurvivor`, ни FSM/интенты в самом ударе не участвуют. Поэтому e2e-ручка `strike <entity>`
должна звать **`pawn.RequestMeleeAttack(entity)`** напрямую (без `dmAISurvivor`); флаг съестся
в следующем `CommandHandler` и удар стартует. Геттеры для диагностики уже есть:
`HasMeleeAttackRequest()` `:1821`, `GetMeleeAttackTarget()` `:1826`, `IsMeleeStriking()` `:1833`
(`(GetCommand_Move() == null) && !unconscious/!climb/!ladder/!fall/!swim/!vehicle`).

---

## 6. Анти-спин (текущее состояние)

Текущая защита от «кручения на замахе» (см. `docs/decisions.md` «melee: заморозка тела на
замахе»):

- `RequestMeleeAttack` снапает `m_MeleeStrikeYaw = GetOrientation()[0]` (`M/dmAISurvivorBase.c:1812`).
- `HeadingModel` override: ветка `COMMANDID_MELEE2` → `pModel.m_fHeadingAngle =
  m_MeleeStrikeYaw * DEG2RAD`, `SetOrientation(m_MeleeStrikeYaw)` (`:1524-1537`).
- `CommandHandler`: `IsMeleeStriking()` → `SetOrientation(m_MeleeStrikeYaw)` (`:1165-1166`), иначе
  `ApplyBodyTurn(pDt)`.
- Эталон Expansion: `eAIBase.HeadingModel` для `MELEE/MELEE2/FALL` — `SetOrientation(m_fHeadingAngle
  * RAD2DEG)` (`eAIBase.c:10094-10102`); плюс `eAIMeleeCombat.Update` зовёт
  `m_AI.SetOrientation(m_AI.GetOrientation())` (`E/Classes/Melee/eAIMeleeCombat.c:217`).

`[нужно проверить]`: работает ли этот анти-спин при **raised-ударе** (Expansion-алгоритм:
`dmAI_Raised=true` + возможно блок/эвейд). Raised меняет лимиты скорости/ротацию
(`V/.../dayzplayercfgbase.c:114/121`), что может снова включить нативный доворот/root-motion спин.
Эмпирика: бот `strike` в raised → оракул `TickMeleeSpinOracle` (лог `Melee spin: turn=...`)
должен показать ~0° за замах; наблюдатель — без полного оборота корпуса. Ручка `botdump` +
`DM_MELEE_SPIN_THRESHOLD`-лог.

---

## 7. Почему мили-урон не применяется на AI_SERVER-пешке (удар)

Статус: цепочка «удар → урон» подтверждена чтением; корневая причина — **не подтверждена
эмпирически** (2 конкурирующие гипотезы, обе `[нужно проверить]`). Дополняет §5 (там — старт
MELEE2), здесь — почему урон после старта не идёт.

### 7.1 Точный триггер `EvaluateHit`: нативный anim-событие `WasHit()`

Урон применяется НЕ в момент `StartCommand_Melee2`, а позже — по флагу **anim-события**:

`V/.../dayzplayermeleefightlogic_lightheavy.c:282-334` (`HandleHitEvent`, зовётся из
`HandleFightLogic` каждый кадр — ваниль `:148`, наш override
`M/Melee/dmBotMeleeFightLogic_LightHeavy.c:47`):

1. Гейт команды: `pCurrentCommandID == COMMANDID_MELEE2` (`:285`).
2. `HumanCommandMelee2 hmc2a = m_Player.GetCommand_Melee2()` (`:287`).
3. **`if (hmc2a.WasHit())`** (`:291`) — единственный триггер урона.
4. При true: ре-таргет `m_MeleeCombat.Update(itemInHands, m_HitType, true)` (`:299`, если
   `GetFinisherType() == -1`) → **`EvaluateHit(itemInHands)`** (`:303`) → `DamageHands`
   (`:318`) → `m_MeleeCombat.ResetTarget()` (`:320`) → `EnableControls()` (`:322`).

`WasHit()` — `proto native bool` (`V/3_game/human.c:553`), коммент «is true only once after hit
event». Это **нативный anim-event**: флаг ставит движок, когда анимация команды MELEE2 доходит до
кадра «Hit»; скрипт его НЕ эмитит (аналог `WasHit()` у зомби-команды атаки —
`V/3_game/entities/dayzinfected.c:50`, которую серверная `ZombieBase.FightLogic` читает на сервере).

`EvaluateHit` (`:577-602`) → `EvaluateHit_Infected` (`:637-642`) → `EvaluateHit_Common`
(`:660-703`): урон `m_Player.ProcessMeleeHit(weapon, weaponMode, target, hitZoneIdx, hitPosWS)`
(`:687`) или `DamageSystem.CloseCombatDamage(...)` (`:682/699`). Оба — серверные нативы
(`V/3_game/dayzplayer.c:1273`, `V/3_game/damagesystem.c:22`).

### 7.2 Почему на AI_SERVER не срабатывает

Всю цепочку урона гейтит нативный `WasHit()`. В ванильном мультиплеере выбор цели и сам
`EvaluateHit` происходят **на клиенте**:

- `TargetSelection()` — только клиент (`#ifndef SERVER`,
  `V/.../dayzplayerimplementmeleecombat.c:224-245`).
- Клиент шлёт цель серверу через `ScriptInputUserData` (`INPUT_UDT_MELEE_TARGET`, `:237-243`);
  сервер читает в `OnInputUserDataProcess` (`V/.../dayzplayerimplement.c:2996-3033`).

У `INSTANCETYPE_AI_SERVER` клиента нет — сервер должен сам эмитить anim-event. **Гипотеза
тестировщика «на AI_SERVER Hit-событие не эмитится» сомнительна**: Expansion AI — тоже
`INSTANCETYPE_AI_SERVER` (спавн `g_Game.CreateObject(GetRandomAI(), pos)`,
`DayZExpansion/AI/Scripts/5_Mission/DayZExpansion_AI/eaicommandmanagerimpl.c:370-372`), его
мили-урон держится на той же цепочке `HandleHitEvent → WasHit() → EvaluateHit` (см. §7.3) и
работает. Т.е. anim-event на сервере для AI_SERVER эмитится, а ломается что-то наше.

Две конкурирующие причины:

- **(H1) `null`-цель в `StartCommand_Melee2`.** Мы передаём `null` вместо цели
  (`M/Melee/dmBotMeleeFightLogic_LightHeavy.c:73`), чтобы глушить нативный доворот (см.
  `docs/decisions.md` «блокировка спина удара»). Expansion/ваниль передают реальный `target`
  (`eaimeleefightlogic_lightheavy.c:276`; ваниль `:363/408/413/422/471/496`). Нативный MELEE2 на
  сервере при `null`-цели, возможно, не делает hit-test и не эмитит «Hit». Оговорка: ваниль сама
  нуллит цель для NONALIGNABLE (`GetTargetData`, `:744-760` — стены/машины), но это клиентский
  путь; на сервере AI не проверялось.
- **(H2) Кастомный граф `botorama/Animations/player_main.agr` не эмитит «Hit»-нотификацию** для
  анимаций MELEE2. Менее вероятно: «Hit»-нотификация обычно лежит в данных клипа (.rtm), а не в
  графе, и анимация у нас играется (стамина снимается, спина нет).

### 7.3 Как Expansion применяет мили-урон для ИИ

Expansion **не обходит** `WasHit()` — нормальный мили-урон у него ровно через ту же ванильную
цепочку:

- `eAIMeleeFightLogic_LightHeavy.HandleFightLogic` (`E/Classes/Melee/eaimeleefightlogic_lightheavy.c:52-249`)
  зовёт унаследованный `HandleHitEvent` (`:76`) — тот же `WasHit() → EvaluateHit`.
- `eAIMeleeCombat.Update(..., wasHitEvent=true)` (`E/Classes/Melee/eAIMeleeCombat.c:200-218`)
  при hit-событии лишь ставит `m_WasHit = true` (`:208`) и выходит; ре-таргет-проверка
  переносится в `TargetSelection` (`:41` — `m_WasHit && GetFinisherType()==-1 && ...`).
- `EvaluateHit` override (`eaimeleefightlogic_lightheavy.c:363-375`): null-guard цели →
  `super.EvaluateHit`, иначе `eAI_SkipMelee("missed melee target")`.
- `eAI_UpdateAttackCooldown()` зовётся ПОСЛЕ `startMeleeSuccess` (`:245-246`) — это
  **кулдаун/тайминг для блок-логики**, а НЕ нанесение урона. Урон идёт только из `EvaluateHit`.
- Единственный прямой урон мимо `EvaluateHit` — «Shoryuken» (комбо-финишер):
  `targetEntity.ProcessDirectDamage(DT_CLOSE_COMBAT, m_AI, "Torso", "MeleeShoryuken", "0 0 0", mult)`
  (`:343`).

Вывод: чтобы у Expansion мили наносил урон, у него ДОЛЖЕН срабатывать `WasHit()` на сервере.
Значит для нас эталон-фикс — сделать так, чтобы `WasHit()` срабатывал (H1: передать реальную
цель), либо явно звать `EvaluateHit`, обходя `WasHit()`.

### 7.4 Варианты фикса (для `dayz-dev`)

**(а) Передавать реальную цель в `StartCommand_Melee2` (эталон Expansion).**
В `HandleFightLogic` заменить `StartCommand_Melee2(null, ...)` (`M/.../dmBotMeleeFightLogic_LightHeavy.c:73`)
на `m_Player.StartCommand_Melee2(target, m_HitType == EMeleeHitType.HEAVY, 1.0, m_MeleeCombat.GetHitPos())`.
Анти-спин уже держится снэпом `m_MeleeStrikeYaw` в `HeadingModel`/`CommandHandler`
(`docs/decisions.md` v3.293), поэтому `null`-хак может быть уже не нужен. После этого `WasHit()`
должен заэмититься на сервере → унаследованный `HandleHitEvent` сам применит урон.
`[нужно проверить]` — H1: эмитится ли `WasHit()` на сервере при `target != null` (залогировать
`GetCommand_Melee2().WasHit()` каждый кадр MELEE2 до/после фикса; ожидается false→true).

**(б) Явно звать `EvaluateHit`, обходя `WasHit()` (детерминированно, без anim-event).**
В `HandleFightLogic` после успешного `StartCommand_Melee2` поставить флаг и применить урон один
раз: либо `GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ApplyHit, ~200, false, target)`
(тайминг кадра «Hit» лёгкого/тяжёлого), либо на первом кадре, где `pCurrentCommandID ==
COMMANDID_MELEE2`. В колбэке звать `EvaluateHit(itemInHands)` (наш override → `super.EvaluateHit`
→ `EvaluateHit_Common` → `ProcessMeleeHit` с уже разрезолвленным `hitZone=7`). Флаг сбрасывать на
новом `StartCommand_Melee2`. Плюс: сохраняется полный ванильный путь (блок-чек + кровь/звук).
`[нужно проверить]` — фактический момент кадра «Hit» (подобрать `CallLater`-задержку по визуалу)
и что `EvaluateHit` из `CALL_CATEGORY_SYSTEM`-колбэка читает валидный `m_MeleeCombat` (цель/
`hitZoneIdx` ещё не сброшены `ResetTarget`).

**(в) Прямой серверный урон мимо `EvaluateHit` (эталон зомби/Shoryuken).**
`DamageSystem.CloseCombatDamage(m_Player, target, hitZoneIdx, ammo, hitPosWS)` (статик-натив,
`V/3_game/damagesystem.c:22`) или `target.ProcessDirectDamage(DamageType.CLOSE_COMBAT, m_Player,
target.GetDefaultHitComponent(), ammo, modelPos, coef)` (`V/3_game/entities/object.c:1134`) — оба
сервер-безопасны (зомби так бьют игрока `V/.../zombiebase.c:652-664`). `ammo` — из
`GetMeleeCombatData().GetAmmoTypeName(weaponMode)` (`V/3_game/gameplay.c:166`; для голых рук
`m_Player.GetMeleeCombatData()`, как уже в `ApplyZombieExtraDamage`
`M/.../dmBotMeleeFightLogic_LightHeavy.c:118`). Минус: теряется ванильный блок-чек
(`EvaluateHit_Player`/зомбиный `IsInBlock()`) и кровь/звук — их надо воспроизводить отдельно.
Фолбэк, если (а)/(б) не дадут урона. `[нужно проверить]` — достаточен ли `CloseCombatDamage` для
полного конвейера (кровь/`EEHitBy`/смерть зомби), или нужен `ProcessDirectDamage` с флагами.

---

## Открытые вопросы / гипотезы для эмпирической проверки

1. `[нужно проверить]` — **блок без raised**: гасит ли `GetMeleeFightLogic().SetBlock(true)`
   (скриптовый флаг) входящий зомби-удар на AI_SERVER-пешке без raised-стойки и блок-анимации
   (§2). Прогнать: зомби (`init_ai=true`) + бот `block on` + разворот к зомби → дельта здоровья.
2. `[нужно подтвердить]` — **нативная блок-поза из ERECT**: стартует ли `hcm.SetMeleeBlock(true)`
   / `StartMeleeEvadeA(±90)` из голой ERECT (не raised) на нашем кастомном графе
   (`player_main.agr`), или визуал требует raised-стойки/граф-переменной (§4). Влияет только на
   визуал.
3. `[нужно проверить]` — **анти-спин при raised-ударе** (§6): `m_MeleeStrikeYaw`-снап держится
   ли при `dmAI_Raised=true` (raised меняет ротацию/скорость).
4. `[нужно подтвердить]` — **эвейд-кулдаун**: `SetCooldown(EVADE_COOLDOWN=0.5, EVADE)` сбрасывает
   `m_IsEvading` через Timer; при серверном вызове из пешки напрямую — таймер
   `CALL_CATEGORY_SYSTEM` тикает ли на AI_SERVER (не только на клиенте).
5. `[нужно проверить]` — **H1 (§7.2): эмитится ли `WasHit()` на сервере при `target != null`.**
   Залогировать `GetCommand_Melee2().WasHit()` каждый кадр MELEE2 (или в `HandleHitEvent`) на
   AI_SERVER-боте: с текущим `StartCommand_Melee2(null, …)` ожидается всегда `false`; после
   смены на `target` (фикс (а)) — ожидается `true` ровно один раз за удар. Это ключевой
   диагностический прогон перед выбором фикса (а)/(б)/(в).
6. `[нужно проверить]` — **H2 (§7.2): кастомный граф `player_main.agr` эмитит ли «Hit»-нотификацию.**
   Прогнать тот же удар, но на ванильном графе (временно) или убедиться, что `WasHit()` остаётся
   `false` и при `target != null` — тогда причина в графе/клипах, а не в `null`-цели.

## Развилки / решения (для журнала `docs/decisions.md`)

- **Блок = скриптовый флаг `SetBlock(true)`, а не натив `SetMeleeBlock`**: гашение урона читает
  `IsInBlock()` (`m_IsInBlock`); натив `SetMeleeBlock` — только визуальная поза. Поэтому e2e-ручка
  `block` обязана звать `GetMeleeFightLogic().SetBlock(true)` (+ разворот к атакующему ±60°), а
  `SetMeleeBlock(true)` — опционально для визуала. (Подтверждено чтением; эмпирика — вопрос 1.)
- **Вход «движение назад» для блока**: Expansion берёт угол из собственного
  `eAICommandMove.GetCurrentMovementAngle()` (= желаемое направление `m_MovementDirection`), НЕ
  нативный `GetCurrentInputAngle` (у ИИ нет клавиатуры). У нас аналог — `m_DesiredMoveAngle`
  (`SetMove(angle, speed)`, тот же конвеншен 0=вперёд/±180=назад). Выбор для fight-logic: читать
  `m_Bot`-ное desired-направление напрямую (проще) или нативный `GetCommand_Move().GetCurrentMovementAngle()`
  (эквивалент после `OverrideMovementAngle`).
- **Мили-урон гейтится нативным anim-событием `WasHit()`, а не `StartCommand_Melee2`** (§7.1):
  `EvaluateHit` (и весь ванильный конвейер урона) зовётся только из `HandleHitEvent` при
  `GetCommand_Melee2().WasHit() == true`. На AI_SERVER этот флаг у нас не эмитится → урона нет,
  хотя старт удара/стамина/таргетинг работают. Кандидат-причина — `null`-цель в
  `StartCommand_Melee2` (H1); запасной механизм — звать `EvaluateHit` напрямую мимо `WasHit()`
  (вариант (б)). Решение о фиксе отложено до эмпирики (вопросы 5/6).
