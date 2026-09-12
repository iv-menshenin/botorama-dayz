# Research: ИИ-лечение (medical) для botorama

Цель: выяснить, как серверный ИИ (`dmAISurvivorBase : PlayerBase`, `INSTANCETYPE_AI_SERVER`,
без клиента) может (1) детектить состояния здоровья, (2) определять лечебные предметы,
(3) применять лечение с анимацией и расходом предмета.

Источники:
- Ваниль: `/home/devalio/dayz/Work/DayZ-Script-Diff/scripts/`.
- botorama: `core/4_World/Entities/Bot/`, `loadout/4_World/`, `docs/research/{emotes,animation}.md`.

Ключевой контекст (уже в скилле): ванильный `PlayerBase.OnScheduledTick` гейтится
`!IsPlayerSelected()` (`playerbase.c:2687`), поэтому у ИИ модификаторы/кровотечение не тикают
сами — botorama тикает их вручную в `dmAISurvivorBase.TickBodySystems` (`dmAISurvivorBase.c:1007`):
`GetModifiersManager().OnScheduledTick(dt)` + `GetBleedingManagerServer().OnTick(dt)`.

---

## A. Детект состояний здоровья (СЕРВЕР)

Все проверки ниже — серверные, работают у ИИ-пешки.

### A1. Кровотечение (рана/порез)

- **Правильно: `PlayerBase.IsBleeding()`** — `return (m_BleedingBits != 0)`
  (`playerbase.c:897-900`). Читает битовую маску прямо, без модификатор-тика.
  Соседнее: `int GetBleedingBits()` (`playerbase.c:919`).
- `BleedingSourcesManagerServer GetBleedingManagerServer()` (`playerbase.c:2653`) — менеджер
  источников; список/биты в `bleedingsourcesmanagerserver.c`.
- **⚠️ `MDF_BLEEDING` НЕ использовать для детекта раны.** `MDF_BLEEDING` = `BleedingCheckMdfr`
  (`modifiers/conditions/bleeding.c:2`), который активируется при `blood < BLOOD_THRESHOLD_FATAL`
  и убивает (`SetHealth("","",-1000)`, `bleeding.c:23-37`). Т.е. это «истёк кровью насмерть»,
  а не «есть кровоточащая рана». Проверка раны — только `IsBleeding()` / `GetBleedingBits()`.

### A2. Перелом

- **`MDF_FRACTURE` не существует.** В enum `eModifiers` его нет (`emodifiers.c`).
- Правильный ID — `MDF_BROKEN_LEGS` (= `BrokenLegsMdfr`, `modifiers/conditions/brokenlegs.c:1`).
  Его `OnActivate` ставит `SetBrokenLegs(-eBrokenLegs.BROKEN_LEGS)` (`brokenlegs.c:26`).
- Два равнозначных способа:
  - `GetBrokenLegs()` → `eBrokenLegs` (`playerbase.c:3563`); enum:
    `NO_BROKEN_LEGS=0, BROKEN_LEGS=1, BROKEN_LEGS_SPLINT=2` (`3_game/enums/ebrokenlegs.c`).
  - `GetModifiersManager().IsModifierActive(eModifiers.MDF_BROKEN_LEGS)`.
- «Шина надета»: `bool IsWearingSplint()` (`playerbase.c:3781`) — проверяет `Splint_Applied` на
  слоте `"Splint_Right"`.
- `MDF_BROKEN_ARMS` (26) есть в enum, но модификатора нет (нет в `ModifiersManager.Init`,
  `modifiersmanager.c:95-145`) — не используется.

### A3. Пищевое отравление

- **`MDF_POISONING`** = `PoisoningMdfr` (`modifiers/conditions/poisoning.c:1`), `m_ID = MDF_POISONING`
  (`poisoning.c:20`); активируется при `FOOD_POISON`-агенте ≥ 1 (`poisoning.c:30-33`).
  Проверка: `IsModifierActive(eModifiers.MDF_POISONING)`.
- Отдельно — **`MDF_SALMONELLA`** = `SalmonellaMdfr` (`modifiers/diseases/salmonella.c:1`),
  активируется при `SALMONELLA`-агенте ≥ 60 (`salmonella.c:37`). Это второй «поносный» модификатор.
- Рвотные симптомы оба дают через `QueueUpPrimarySymptom(SYMPTOM_VOMIT)`.

### A4. Простуда / грипп

- `IsModifierActive(eModifiers.MDF_COMMON_COLD)` — `CommonColdMdfr` (`modifiers/diseases/commoncold.c`).
- `IsModifierActive(eModifiers.MDF_INFLUENZA)` — `InfluenzaMdfr` (`modifiers/diseases/influenza.c`).

### A5. Действующие препараты (чтобы не глотать повторно)

Проверка «препарат уже действует» — `IsModifierActive(...)`. «Осталось времени» — метода
`GetRemainingTime()` нет; есть `ModifierBase.GetAttachedTime()` (`modifierbase.c:150`), а лайфтайм —
приватная константа конкретного модификатора (вычислять `lifetime - GetAttachedTime()` самому).

| Препарат | Модификатор | Модификатор-класс | Лайфтайм |
|----------|-------------|-------------------|----------|
| Болеутоляющее | `MDF_PAINKILLERS` | `PainKillersMdfr` (`modifiers/painkillersmdfr.c:1`) | 240 c (`painkillersmdfr.c:3`) |
| Тетрациклин/антибиотик | `MDF_ANTIBIOTICS` | `AntibioticsMdfr` (`modifiers/antibiotics.c:1`) | 300 c (`antibiotics.c:3`) |
| Уголь | `MDF_CHARCOAL` | `CharcoalMdfr` (`modifiers/charcoalmdfr.c:1`) | 300 c (`charcoalmdfr.c:4`) |
| Мультивитамины | `MDF_IMMUNITYBOOST` | `ImmunityBoost` (`modifiers/immunityboost.c:1`) | `VITAMINS_LIFETIME_SECS` (`immunityboost.c:15`) |

**Про уголь отдельно** (важно): уголь **НЕ** «снимает `MDF_POISONING`». Его эффект — в
`CharcoalMdfr.OnTick`: `m_AgentPool.AddAgent(eAgents.SALMONELLA, -m_Killrate*dt)`
(`charcoalmdfr.c:81-87`) — убивает сальмонеллу. Значит «уголь уже действует» = `IsModifierActive(MDF_CHARCOAL)`.
Строка `DeactivateModifier(MDF_POISONING)` в `ActionEatCharcoalTablets.ApplyModifiers`
(`actioneatcharcoaltablets.c:16`) — **мёртвый код**: `ApplyModifiers` вызывается только из
инъекций (`actioninjectself.c:27`, `actioninjecttarget.c:25`), а не из eat-действий.

**API `ModifiersManager`** (`modifiersmanager.c`):
- `bool IsModifierActive(eModifiers)` (`:199`).
- `void ActivateModifier(int id, bool triggerEvent = TRIGGER_EVENT_ON_ACTIVATION)` (`:219`).
- `void DeactivateModifier(int id, bool triggerEvent = true)` (`:224`).
- `ModifierBase GetModifier(int id)` (`:306`) → `IsActive()` (`modifierbase.c:130`),
  `GetAttachedTime()` (`:150`).
- `void OnScheduledTick(float dt)` (`:204`) — гейтится `m_AllowModifierTick` (botorama включает его).

---

## B. Классы предметов

### B1. Перевязочные

- **`BandageDressing extends ItemBase`** (`bandagedressing.c:1`); `GetBandagingEffectivity() = 2.0`
  (`:49`); `AddAction(ActionBandageTarget)` + `AddAction(ActionBandageSelf)` (`:42-43`).
- **`Rag extends ItemBase`** (`rag.c:1`) — **НЕ** наследует `BandageDressing`, но является
  перевязочным: `GetBandagingEffectivity() = 0.5` (`rag.c:117`) + те же `AddAction(ActionBandageTarget/Self)`
  (`rag.c:96-97`). Общий маркер «перевязочного» — **override `GetBandagingEffectivity()`**
  (дефолт `ItemBase` = 1.0, `itembase.c:4497`) + наличие bandage-действий. Флага/связи с
  `BandageDressing` нет.
- **Бандана/шемаг**: `Bandana_ColorBase : Clothing` (`gear/consumables/consumables.c:1`,
  `GetBandagingEffectivity() = 0.5` на `:22`) и `Shemag_ColorBase : Clothing` (`consumables.c:65`,
  `:86`). **Класса `BandanaDressing` в ванили НЕТ**; легаси `BandanaHybrid_ColorBase` целиком
  закомментирован (`clothing/bandana_hybrid.c`).
- Общий `ItemBase` маркер: `void OnApply(PlayerBase player)` (`itembase.c:4495`) — но его
  override есть только у шприцев/морфина/эпинефрина, у бинтов НЕТ (бинты идут через
  `ActionBandageBase.ApplyBandage`).

### B2–B6. Медицина

| Предмет | Скрипт-класс | Файл:строка |
|---------|--------------|-------------|
| Шина | `Splint : Inventory_Base` | `gear/medical/splint.c:1` |
| Шина (надетая) | `Splint_Applied : Clothing` (слот `"Splint_Right"`) | `gear/medical/splint.c:12` |
| Болеутоляющее | `PainkillerTablets extends Edible_Base` | `edible_base/painkillertablets.c:1` |
| Уголь | `CharcoalTablets extends Edible_Base` | `edible_base/charcoaltablets.c:1` |
| Тетрациклин | `TetracyclineAntibiotics : Edible_Base` | `edible_base/tetracyclineantibiotics.c:1` |
| Мультивитамины | `VitaminBottle : Edible_Base` | `edible_base/vitaminbottle.c:1` |

- `Edible_Base : ItemBase` (`edible_base.c:1`).
- **`MultivitaminPills` не существует.** Мультивитамины — это `VitaminBottle` (бутылка с
  пилюлями, quantity = число пилюль); действие на ней — `ActionEatPillFromBottle`
  (`actioneatpillfrombottle.c:1`, `CMD_ACTIONMOD_EAT_PILL`). Таблетки в блистере — `ActionEatTabletFromWrapper`
  (`actioneattabletfromwrapper.c:1`, `CMD_ACTIONMOD_EAT_TABLET`).

---

## C. Как выполнить действие (серверный ИИ)

### C1. Что делают ванильные действия

- **Перевязка** — `ActionBandageSelf`/`ActionBandageTarget : ActionBandageBase`
  (`actionbandagebase.c:1`). Ядро `ApplyBandage(item, player)` (`actionbandagebase.c:3-15`):
  ```c
  player.GetBleedingManagerServer().RemoveMostSignificantBleedingSourceEx(item);
  PluginTransmissionAgents.TransmitAgents(item, player, AGT_ITEM_TO_FLESH);
  if (item.HasQuantity()) item.AddQuantity(-1, true); else item.Delete();
  ```
  (`RemoveMostSignificantBleedingSourceEx` — `bleedingsourcesmanagerserver.c:91`, ставит item и
  удаляет самый сильный источник; внутри `RemoveBleedingSource` — шанс инфицирования `WOUND_AGENT`
  из `GetInfectionChance`, `bleedingsourcesmanagerserver.c:43-63`.)
- **Шина** — `ActionSplintSelf` (`actionsplintself.c:9`). `OnFinishProgressServer` (`:32-48`):
  `TransferModifiers(player)` (no-op, легаси `itembase.c:96` — только декларация без тела) →
  `player.ApplySplint()` (`playerbase.c:2112`, +33% HP ног) → при `GetBrokenLegs()==BROKEN_LEGS`:
  `SetBrokenLegs(eBrokenLegs.BROKEN_LEGS_SPLINT)` (`playerbase.c:3569`) + создать `Splint_Applied`
  в инвентаре + `Delete()` шину.
- **Таблетки** — `ActionEatPainkillerTablets`/`Charcoal`/`Tetracycline`/`VitaminBottle : ActionConsume`
  (`actionconsume.c:9`) — их `ApplyModifiers` **мёртв** (закомментирован или не вызывается).
  Реальный эффект — в **`OnConsume(float amount, PlayerBase consumer)` на самом предмете**:
  - `PainkillerTablets` (`painkillertablets.c:11-17`): если active → `DeactivateModifier(MDF_PAINKILLERS)`, затем `ActivateModifier(MDF_PAINKILLERS)` (сброс таймера).
  - `CharcoalTablets` (`charcoaltablets.c:11-16`): если НЕ active → `ActivateModifier(MDF_CHARCOAL)`.
  - `TetracyclineAntibiotics` (`tetracyclineantibiotics.c:3-9`): reset + `ActivateModifier(MDF_ANTIBIOTICS)`.
  - `VitaminBottle` (`vitaminbottle.c:11-19`): reset + `ActivateModifier(MDF_IMMUNITYBOOST)`.
  - Цепочка вызова: `Edible_Base.Consume(amount, consumer)` (`edible_base.c:95-101`) =
    `AddQuantity(-amount)` + `OnConsume(...)`; дёргается из `CAContinuousQuantityEdible`
    (`cacontinuousquantityedible.c:52-58`, continuous) или `ActionConsumeSingle.OnExecuteServer`
    (`actionconsumesingle.c:40-53`, single-use, `player.Consume(consumeData)` → `playerbase.c:7175`).

### C2. Действия у серверного ИИ НЕ работают (client-driven)

- `GetActionManager()` → `ActionManagerBase` (`playerbase.c:1699`, `actionmanagerbase.c:30`).
- Старт действия — только с клиента: `ActionManagerClient.PerformAction`/`PerformActionStart`
  (`actionmanagerclient.c:756/762`); сервер получает через `ActionManagerServer.OnInputUserDataProcess`
  (`actionmanagerserver.c:36`, `INPUT_UDT_STANDARD_ACTION_START`).
- **Публичного API «запустить действие серверно у ИИ» нет** — `ActionManagerServer` питается
  сетевым инпутом клиента, которого у `INSTANCETYPE_AI_SERVER` нет. → полный путь через
  `useractionscomponent` для ИИ не годится; нужен прямой путь (C3) + анимация граф-командой (D).

### C3. Прямой путь (работает на сервере)

- **Кровотечение**: `GetBleedingManagerServer().RemoveMostSignificantBleedingSourceEx(item)`
  (`bleedingsourcesmanagerserver.c:91`; item нужен для шанса инфицирования) — или без item:
  `RemoveMostSignificantBleedingSource()` (`:84`) / `RemoveAnyBleedingSource()` (`:76`).
  Опционально `PluginTransmissionAgents.TransmitAgents(item, player, AGT_ITEM_TO_FLESH)`
  (`plugintransmissionagents.c:377`).
- **Перелом**: `ApplySplint()` (`playerbase.c:2112`) + `SetBrokenLegs(eBrokenLegs.BROKEN_LEGS_SPLINT)`
  (`playerbase.c:3569`) + спавн `Splint_Applied` в инвентарь (`CreateInInventory("Splint_Applied")`)
  + удалить `Splint`. (Снятие шины при излечении — `MiscGameplayFunctions.RemoveSplint`,
  `miscgameplayfunctions.c:1630`, зовёт `BrokenLegsMdfr.OnDeactivate` при HP ног ≥ 100, `brokenlegs.c:38-47`.)
- **Таблетки/пилюли** — самый чистый способ: `item.Consume(1.0, pawn)` (для `Edible_Base`),
  т.е. `Edible_Base.Consume` сам делает `AddQuantity(-amount)` + `OnConsume(amount, consumer)`
  (`edible_base.c:95-101`) — и расход, и эффект. Либо вручную:
  `GetModifiersManager().ActivateModifier(MDF_X)` (с reset-логикой из `OnConsume`) + расход предмета отдельно.
- **Отравление**: снять сразу `DeactivateModifier(MDF_POISONING)` (как делал мёртвый код угля),
  но «по-настоящему» лечится углём (`MDF_CHARCOAL` убивает сальмонеллу, см. A5).

### C4. Расход предмета / спавн в руки

- `bool AddQuantity(float value, bool destroy_config = true, bool destroy_forced = false)`
  (`itembase.c:3362`) — уменьшить количество; удалится при достижении минимума.
- `Delete()` — удалить предмет (эталон `actionbandagebase.c:14`).
- `bool HasQuantity()` (`itembase.c:3451`), `float GetQuantity()` (`itembase.c:3456`).
- `Edible_Base.Consume(amount, consumer)` (`edible_base.c:95`) — разом и расход, и `OnConsume`.
- **Спавн в руки серверного ИИ** — `pawn.GetHumanInventory().CreateInHands(cls)`; подтверждённый
  эталон в botorama: `dmLoadoutApplier.c:289` (`if (slotKey == "hands") item = pawn.GetHumanInventory().CreateInHands(cls);`).
  Слоты — `CreateAttachmentEx(cls, slotId)` (`dmLoadoutApplier.c:297`), карго — `CreateInInventory` (`:381`).

---

## D. Анимации (граф-команды для ИИ)

Механизм (тот же, что уже используется в botorama и подтверждён для серверного ИИ):

- Нативы `Human` (`3_game/human.c`): `StartCommand_Action(int id, typename cbClass, int stanceMask)`
  (`:1527`, full-body), `AddCommandModifier_Action(int id, typename cbClass)` (`:1557`, additive),
  `DeleteCommandModifier_Action(cb)` (`:1560`), `GetCommand_Action()` (`:1530`),
  `GetCommandModifier_Action()` (`:1563`).
- Callback — `HumanCommandActionCallback` (`human.c:309`, private ctor → нужен пустой конструктор
  подкласса). Эталоны: `dmBotActionAnimCB` (`dmBotIntent_OpenDoor.c:12`) и `EmoteCB` (`emotemanager.c:1`).
  botorama уже делает: `pawn.AddCommandModifier_Action(DayZPlayerConstants.CMD_ACTIONMOD_OPENDOORFW, dmBotActionAnimCB)`
  (`dmBotIntent_OpenDoor.c:78`).

Командные ID (`DayZPlayerConstants`, `3_game/dayzplayer.c`):

| Действие | ID | Строка | Тип |
|----------|----|--------|-----|
| Перевязка (self) | `CMD_ACTIONFB_BANDAGE = 58` | `:824` | full-body (cro) |
| Перевязка (target) | `CMD_ACTIONFB_BANDAGETARGET = 63` | `:828` | full-body (erc/cro) |
| Шина (крафт-анимация) | `CMD_ACTIONFB_CRAFTING = 59` | `:825` | full-body (cro) |
| Еда (modifier) | `CMD_ACTIONMOD_EAT = 1` | `:742` | additive (erc/cro) |
| Еда (prone full-body) | `CMD_ACTIONFB_EAT = 1` | `:810` | full-body (pne) |
| Пилюля | `CMD_ACTIONMOD_EAT_PILL = 527` | `:796` | additive (erc/cro) |
| Таблетка | `CMD_ACTIONMOD_EAT_TABLET = 528` | `:797` | additive (erc/cro) |
| Пилюля (prone) | `CMD_ACTIONFB_EAT_PILL = 527` | `:912` | full-body (pne) |
| Таблетка (prone) | `CMD_ACTIONFB_EAT_TABLET = 528` | `:913` | full-body (pne) |

Конкретные варианты для ИИ:
- **Перевязка**: `StartCommand_Action(CMD_ACTIONFB_BANDAGE, dmBotActionAnimCB, STANCEMASK_CROUCH)`.
- **Шина**: `StartCommand_Action(CMD_ACTIONFB_CRAFTING, dmBotActionAnimCB, STANCEMASK_CROUCH)`.
- **Таблетка/пилюля**: `AddCommandModifier_Action(CMD_ACTIONMOD_EAT_TABLET | CMD_ACTIONMOD_EAT_PILL, dmBotActionAnimCB)`
  (additive, верхняя часть тела — не блокирует MOVE/LOOK, как `CMD_ACTIONMOD_OPENDOORFW`).
- Маски стойки: `DayZPlayerConstants.STANCEMASK_CROUCH / STANCEMASK_ERECT / STANCEMASK_ALL`.

Завершение анимации детектится как в `dmBotIntent_Emote`/`dmBotIntent_OpenDoor`:
`GetCommand_Action() == null && GetCommandModifier_Action() == null` (НЕ `IsEmotePlaying()` —
он «залипает» у серверного ИИ, `docs/research/emotes.md`). One-shot additive-анимация
(`CMD_ACTIONMOD_*`) самозавершается; full-body `StartCommand_Action` — та же семантика.

---

## РЕКОМЕНДАЦИЯ (серверный ИИ)

1. **Не использовать `useractionscomponent`/`ActionManager`** — он client-driven
   (`ActionManagerServer` ест только сетевой инпут, `actionmanagerserver.c:36`). Для
   `INSTANCETYPE_AI_SERVER` публичного способа «выполнить действие» нет.
2. **Прямые модификаторы/менеджеры (C3)** — единственный рабочий путь эффекта:
   - перевязка → `GetBleedingManagerServer().RemoveMostSignificantBleedingSourceEx(item)`;
   - шина → `ApplySplint()` + `SetBrokenLegs(BROKEN_LEGS_SPLINT)` + спавн `Splint_Applied` + `Delete()` шины;
   - таблетки → `item.Consume(1.0, pawn)` (расход + `OnConsume` → `ActivateModifier`), либо
     `GetModifiersManager().ActivateModifier(MDF_*)` с reset-логикой.
3. **Анимацию проигрывать граф-командой (D)**: `StartCommand_Action` (full-body: бинт/шина) или
   `AddCommandModifier_Action` (additive: таблетка/пилюля), callback — `dmBotActionAnimCB`
   (паттерн `dmBotIntent_OpenDoor`). Завершение — по `GetCommand_Action()==null &&
   GetCommandModifier_Action()==null`.
4. **Детект «уже действует»** — только `IsModifierActive(MDF_PAINKILLERS/ANTIBIOTICS/CHARCOAL/IMMUNITYBOOST)`;
   «есть рана» — `IsBleeding()`; «перелом» — `GetBrokenLegs()` / `IsModifierActive(MDF_BROKEN_LEGS)`.
   **Не путать `MDF_BLEEDING` (смертельная кровопотеря) с раной.**

### Открытые вопросы

- Не подтверждено, что full-body `StartCommand_Action(CMD_ACTIONFB_BANDAGE, ...)` корректно
  завершается у серверного ИИ (проверено только additive `CMD_ACTIONMOD_OPENDOORFW` и emotes).
  При первом использовании заложить таймаут-фолбэк на случай «не завершился».
- Нужен ли `PluginTransmissionAgents.TransmitAgents` для бинтов у ИИ (шанс заражения раны) —
  решить по дизайну: можно пропустить, если заражение ИИ не моделируется.
- Точные времена анимаций (`UATimeSpent.BANDAGE/APPLY_SPLINT`, `UAQuantityConsumed.DEFAULT`) —
  в `4_world/classes/useractionscomponent/` (`ua*.c`), не выписаны; для тайминга ИИ достаточно
  поллинг завершения команды, а не фикс-таймер.
