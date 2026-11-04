# body — журналирование причины смерти ИИ-бота от постепенной потери здоровья

Домен: тело/состояние. Ваниль — `DayZ Projects/scripts/` (Build 1.28+, файлы ниже), бот —
`dmAISurvivorBase : PlayerBase` (`INSTANCETYPE_AI_SERVER`). Смежное: `body.md` (death-flow,
труп, нокаут), `medical.md` (детект/лечение модификаторов), `combat.md` (внешний урон →
`EEKilled`).

## Цель

Бот вручную тикает `GetModifiersManager().OnScheduledTick(dt)` +
`GetBleedingManagerServer().OnTick(dt)` (`dmAISurvivorBase.TickBodySystems`, `:1339-1370`),
поэтому модификаторы реально снижают здоровье. Внешняя смерть логируется ванильным
`PluginAdminLog.PlayerKilled` (через `EEKilled → super.EEKilled`). Нужно понять, как
залогировать причину **естественной** смерти (голод/жажда/холод/болезнь), которая по
предварительным данным НЕ диспатчит `EEKilled`.

---

## 1. Полный enum `eModifiers` (подтверждено)

`emodifiers.c:1`. Значения последовательные, `MDF_TEMPERATURE = 1` (закомментировано
«must start at 1»), `COUNT` — маркер конца (не модификатор).

| # | Имя | Класс модификатора | Смертелен? |
|---|-----|--------------------|-----------|
| 1 | MDF_TEMPERATURE | HeatComfortMdfr | **да** (гипо/гипертермия, рут-хелф) |
| 2 | MDF_BLOOD_REGEN | BloodRegenMdfr | нет (реген крови) |
| 3 | MDF_BURNING | BurningMdfr | **да** (HeatComfort>199, Health-стат) |
| 4 | MDF_HEALTH_REGEN | HealthRegenMdfr | нет |
| 5 | MDF_TREMOR | TremorMdfr | нет |
| 6 | MDF_HUNGER | HungerMdfr | **да** (Energy≤300, Health-стат) |
| 7 | MDF_FEVER | FeverMdfr | косвенно (сливает Water) |
| 8 | MDF_COMMON_COLD | CommonColdMdfr | нет (симптомы) |
| 9 | MDF_THIRST | ThirstMdfr | **да** (Water≤300, Health-стат) |
| 10 | MDF_BLEEDING | BleedingCheckMdfr | **да** (кровь≤2500, рут-хелф) |
| 11 | MDF_BLINDED | BlindedMdfr | нет |
| 12 | MDF_HEART_ATTACK | HeartAttackMdfr | **да** (Shock≤0, Health-стат) |
| 13 | MDF_STUFFED | StuffedStomachMdfr | нет |
| 14 | MDF_BONE_REGEN | BoneRegenMdfr | нет |
| 15 | MDF_HEALTH | HealthMdfr | нет (только notifier-трекинг) |
| 16 | MDF_POISONING | PoisoningMdfr | косвенно (рвота → Water/Energy) |
| 17 | MDF_HEMOLYTIC_REACTION | HemolyticReactionMdfr | косвенно (Blood) |
| 18 | MDF_STOMACH | StomachMdfr | нет |
| 19 | MDF_STAMINACALC | (нет в Init) | — |
| 20 | MDF_IMMUNE_SYSTEM | ImmuneSystemMdfr | нет |
| 21 | MDF_INFLUENZA | InfluenzaMdfr | нет (кашель; → Pneumonia) |
| 22 | MDF_SHOCK | ShockMdfr | нет |
| 23 | MDF_BROKEN_LEGS | BrokenLegsMdfr | нет |
| 24 | MDF_BROKEN_ARMS | (нет в Init) | — |
| 25 | MDF_CHOLERA | CholeraMdfr | косвенно (сливает Water) |
| 26 | MDF_TESTING | TestDiseaseMdfr | (DEVELOPER) |
| 27 | MDF_SALMONELLA | SalmonellaMdfr | косвенно (рвота → Water/Energy) |
| 28 | MDF_BRAIN | BrainDiseaseMdfr | нет |
| 29 | MDF_VOMITSTUFFED | VomitStuffedMdfr | нет |
| 30 | MDF_SALINE | SalineMdfr | нет |
| 31 | MDF_SHOCK_DAMAGE | ShockDamageMdfr | нет |
| 32 | MDF_UNCONSCIOUSNESS | UnconsciousnessMdfr | нет |
| 33 | MDF_WETNESS | WetMdfr | нет (косвенно — охлаждение) |
| 34 | MDF_IMMUNITYBOOST | ImmunityBoost | нет |
| 35 | MDF_ANTIBIOTICS | AntibioticsMdfr | нет |
| 36 | MDF_TOXICITY | ToxicityMdfr | нет |
| 37 | MDF_BREATH_VAPOUR | BreathVapourMdfr | нет |
| 38 | MDF_WOUND_INFECTION | WoundInfectStage1Mdfr | косвенно |
| 39 | MDF_WOUND_INFECTION1 | WoundInfectStage2Mdfr | **да** (рут-хелф, стадия 2) |
| 40 | MDF_WOUND_INFECTION2 | WoundInfectStage2Mdfr | **да** (рут-хелф, стадия 2) |
| 41 | MDF_DISINFECTION | DisinfectionMdfr | нет |
| 42 | MDF_CHARCOAL | CharcoalMdfr | нет |
| 43 | MDF_MORPHINE | MorphineMdfr | нет |
| 44 | MDF_PAINKILLERS | PainKillersMdfr | нет |
| 45 | MDF_EPINEPHRINE | EpinephrineMdfr | нет |
| 46 | MDF_HEATBUFFER | HeatBufferMdfr | нет (защита от холода) |
| 47 | MDF_FATIGUE | FatigueMdfr | нет |
| 48 | MDF_CONTAMINATION1 | ContaminationStage1Mdfr | косвенно |
| 49 | MDF_CONTAMINATION2 | ContaminationStage2Mdfr | косвенно |
| 50 | MDF_CONTAMINATION3 | ContaminationStage3Mdfr | **да** (рут-хелф, стадия 3) |
| 51 | MDF_AREAEXPOSURE | AreaExposureMdfr | нет |
| 52 | MDF_MASK | MaskMdfr | нет |
| 53 | MDF_FLIES | FliesMdfr | нет |
| 54 | MDF_DROWNING | DrowningMdfr | **да** (рут-хелф) |
| 55 | MDF_HEAVYMETAL1 | HeavyMetalPhase1Mdfr | косвенно |
| 56 | MDF_HEAVYMETAL2 | HeavyMetalPhase2Mdfr | косвенно |
| 57 | MDF_HEAVYMETAL3 | HeavyMetalPhase3Mdfr | **да** (рут-хелф, фаза 3) |
| 58 | MDF_PNEUMONIA | PneumoniaMdfr | **да** (рут-хелф) |
| 59 | MDF_CHELATION | ChelationMdfr | нет |
| — | COUNT (60) | — | маркер |

Ключевое: **`MDF_HYPOTHERMIA` / `MDF_HYPERTHERMIA` / `MDF_FREEZING` не существуют**.
Гипо/гипертермия = один модификатор **`MDF_TEMPERATURE`** (`HeatComfortMdfr`). Смежные по
температуре: `MDF_HEATBUFFER` (пассивная защита от холода, `heatbuffer.c`), `MDF_BURNING`
(экстремальный жар HeatComfort>199). Полный список зарегистрированных в `Init()` классов —
`modifiersmanager.c:88-145` (там же видно, что `MDF_STAMINACALC`/`MDF_BROKEN_ARMS`
зарегистрированы в enum, но НЕ имеют класса — мёртвые ID).

## 2. Механизм смерти от модификатора (подтверждено по коду)

Два канала снижения здоровья в `object.c`:

| Канал | Вызов | Что это | Диспатчит EEKilled? |
|-------|-------|---------|--------------------|
| **Рут-хелф** | `SetHealth("","",x)` / `AddHealth("","",x)` / `AddHealth(x)` (одноарг = `AddHealth("","",x)`, `object.c:1106`) | «разрушен ли объект» (`IsDamageDestroyed`) | да (как внешний урон) |
| **Health-стат** | `SetHealth("GlobalHealth","Health",x)` / `AddHealth("GlobalHealth","Health",x)` (и alias `""`/`"Health"`) | числовой стат здоровья | **нет** (`[нужно подтвердить]`, см. ниже) |

`IsAlive()` = `!IsDamageDestroyed()` (`object.c:520-522`) реагирует на оба канала
(Health-стат 0 тоже даёт `IsAlive()=false` — нативная связка рут-хелф ↔ Health-стат,
из скриптов не видна).

**Какие модификаторы каким каналом убивают** (точные вызовы):

Рут-хелф (`AddHealth("","",…)`/`SetHealth("","",…)`):
- `MDF_BLEEDING` — `bleeding.c:37` `SetHealth("","",-1000)` (мгновенно, в `OnActivate`).
- `MDF_TEMPERATURE` (холод/жар) — `heatcomfortmdfr.c:75` `AddHealth("","",-health_loss)`.
- `MDF_DROWNING` — `drowning.c:35` `AddHealth("","",…)` (когда stamina ≤ 0).
- `MDF_PNEUMONIA` — `pneumonia.c:62` `AddHealth(-healthLoss)` (одноарг = рут).
- `MDF_WOUND_INFECTION2` (стадия 2) — `woundinfection2.c:88` `AddHealth("","",-damage)`.
- `MDF_CONTAMINATION3` (стадия 3) — `contamination3.c:54` `AddHealth("","",…)`.
- `MDF_HEAVYMETAL3` (фаза 3) — `heavymetal.c:222` `AddHealth(-healthLoss)` (рут).

Health-стат (`AddHealth("GlobalHealth","Health",…)`/`SetHealth("GlobalHealth","Health",…)`):
- `MDF_HUNGER` — `hunger.c:45` `AddHealth("GlobalHealth","Health",-LOW_ENERGY_DAMAGE_PER_SEC*dt)` (Energy≤300 и не переваривает).
- `MDF_THIRST` — `thirst.c:45` `AddHealth("GlobalHealth","Health",-LOW_WATER_DAMAGE_PER_SEC*dt)` (Water≤300 и не переваривает).
- `MDF_HEART_ATTACK` — `heartattack.c:36` `AddHealth("GlobalHealth","Health",…)` (Shock≤0).
- `MDF_BURNING` — `burning.c:47` `SetHealth("GlobalHealth","Health",…)` (HeatComfort>199).

**Косвенные** (сами НЕ бьют по здоровью — сливают Energy/Water, дальше срабатывает
HungerMdfr/ThirstMdfr):
- `MDF_CHOLERA` — `cholera.c:OnTick` `GetStatWater().Add(-waterLoss)` (WATER_LOSS_CHOLERA=0.5·норм. агента) + рвота сливает Water/Energy.
- `MDF_SALMONELLA` — `salmonella.c:OnTick` рвота → Water/Energy.
- `MDF_POISONING` — `poisoning.c:OnTick` рвота → Water/Energy.
- `MDF_FEVER` — `fever.c:OnTick` `GetStatWater().Add(-WATER_LOSS_FEVER)` (0.2/с).
- `MDF_INFLUENZA`/`MDF_COMMON_COLD` — только симптомы (кашель/чихание); рост агента
  INFLUENZA → прогрессия в `MDF_PNEUMONIA` (порог агента 1150, `pneumonia.c:ActivateCondition`).

Константы (`playerconstants.c`): `SL_ENERGY_MAX=SL_WATER_MAX=5000`,
`LOW_ENERGY_THRESHOLD=LOW_WATER_THRESHOLD=300`, `LOW_ENERGY_DAMAGE_PER_SEC=LOW_WATER_DAMAGE_PER_SEC=0.05`,
`BLOOD_THRESHOLD_FATAL=2500`, `THRESHOLD_HEAT_COMFORT_*_CRITICAL=±0.45`,
`HEALTH_LOSS_HC_*_LOW/HIGH=0.035/0.30`, `WATER_LOSS_FEVER=0.2`, `BURNING_TRESHOLD=199`.

### Диспатч EEKilled при Health-стате 0 — НЕ подтверждается из скриптов

Точка диспатча `EEKilled` — нативная, в скриптах вызывателя нет (только `override` +
`super.EEKilled`). Эмпирика мода (`body.md` §2): `SetHealth("","Health",0)` →
`IsAlive()=false`, но `EEKilled` НЕ приходит, труп не создаётся, «стоит мёртвый». Рабочая
гипотеза мода: **рут-хелф 0 → EEKilled диспатчится; Health-стат 0 → не диспатчится**.

Отсюда следствие для этой задачи (и **важная поправка к premise**):
- **Голод/жажда/сердечный приступ/ожог** (Health-стат) → `EEKilled` НЕ диспатчится → причина
  в ADM-журнал не попадёт через ванильный `PlayerKilled` вообще.
- **Холод/жара (MDF_TEMPERATURE), утопление, пневмония, кровотечение, рана-стадия-2,
  заражение-стадия-3, тяжёлые металлы-3** (рут-хелф) → `EEKilled` СКОРЕЕ ВСЕГО диспатчится
  (как внешний урон) с `killer == this`/`null` → ванильная ветка «естественной смерти» в
  `PluginAdminLog.PlayerKilled` отработает.
- **Болезни** (холера/сальмонелла/отравление/грипп/простуда) сами здоровье не трогают — убивают
  через жажду/голод → итоговый канал = Health-стат → `EEKilled` не диспатчится.

`[нужно подтвердить]` на тест-сервере (probe: `spawnobj` бот + форс модификатора): (1) диспатчит
ли `EEKilled` при доведении рут-хелф через `MDF_TEMPERATURE` (гипотермия) — т.е. приходит ли
`EEKilled` с `killer == this`? (2) при доведении Health-стата через `MDF_HUNGER`/`MDF_THIRST` —
`EEKilled` действительно не приходит? Разделить два канала на одном боте.

## 3. Как определить конкретную причину в момент смерти

### API `ModifiersManager` (`modifiersmanager.c`)

| Сигнатура | Строка | Назначение |
|-----------|--------|-----------|
| `bool IsModifierActive(eModifiers id)` | :199 | активен ли модификатор |
| `void OnScheduledTick(float dt)` | :204 | тик всех модификаторов (гейт `m_AllowModifierTick`) |
| `void DeactivateAllModifiers()` | :213 | — |
| `void ActivateModifier(int id, bool triggerEvent=…)` | :219 | — |
| `void DeactivateModifier(int id, bool triggerEvent=true)` | :224 | — |
| `ModifierBase GetModifier(int id)` | :306 | объект модификатора |
| `PlayerBase GetPlayer()` | :311 | — |
| `void SetModifierLock(int id, bool)` / `bool GetModifierLock(int)` | :316/:321 | — |
| `void DbgGetModifiers(array<ref ModifierDebugObj>)` | :326 | **полный список активных модификаторов** (id+имя+active+locked) |

`ModifierBase` (`modifierbase.c`): `IsActive()` :130, `GetModifierID()` :160, `GetName()` :165,
`GetAttachedTime()` :150, `GetPlayer()` :76, `ActivateCondition/DeactivateCondition` :176/:181.

**Нет метода `GetActiveModifiers()`** — перечисление активных только через `DbgGetModifiers`
(заполняет переданный массив `ModifierDebugObj` из 1..COUNT, у активных `IsActive()==true`)
или через N вызовов `IsModifierActive(MDF_X)` для конкретных ID.

### Голод/жажда — это И статы, И модификаторы

Поправка к `body.md`: голод/жажда НЕ «только статы». Есть статы `Energy`/`Water`
(`PlayerStat<float> GetStatEnergy()` `playerbase.c:7861`, `GetStatWater()` `:7843`,
`GetStatHeatComfort()` `:7870`) **и** модификаторы `MDF_HUNGER`/`MDF_THIRST`
(`HungerMdfr`/`ThirstMdfr`), которые сливают стат и при пороге 300 бьют по Health-стату.
Т.е. `IsModifierActive(MDF_HUNGER)`/`(MDF_THIRST)` — почти всегда `true` (эти модификаторы с
`DisableDeactivateCheck()`, всегда активны), поэтому «активен ли модификатор» НЕ говорит «это
причина смерти» — нужно смотреть на **стат**.

### Рекомендация (human model): логировать состояние, а не «виновника»

В ванили **нет** API «какой модификатор довёл здоровье до нуля» и нет «last-modifier-damage»
трекинга. Причинно-следственная цепочка многослойна и неразрешима однозначно (холера сливает
Water → `ThirstMdfr` бьёт по Health — «виновник» = ThirstMdfr, «причина» = холера). Строить
формулу «модификатор → стат → смерть» — хрупко (анти-паттерн по AGENTS.md «человеческая модель»).

Практичный ответ (эталон — ванильный `PluginAdminLog.PlayerKilled`): в момент смерти
**записать наблюдаемое состояние**, а не вычислить причину:
- `GetStatWater().Get()` / `GetStatEnergy().Get()` (0..5000) — голод/жажда/болезнь-обезвоживание;
- `GetStatHeatComfort().Get()` (±1) — гипо(<−0.45)/гипер(>0.45)/ожог(>199);
- `GetHealth("","Blood")` — кровь (≤2500 = истёк);
- `GetDrowningWaterLevelCheck()` (`playerbase.c:6813`) — утопление;
- `IsModifierActive(MDF_CHOLERA/SALMONELLA/POISONING/INFLUENZA/PNEUMONIA/COMMON_COLD)` — активная
  болезнь (как «контекст», не как «виновник»);
- `HasDisease()` (`playerbase.c:1089`, = `m_DiseaseCount`) — есть ли хоть одна болезнь;
- `GetSingleAgentCount(eAgents.X)` (`playerbase.c:7772`) — титр агента (пороги активации
  болезни: CHOLERA=250, SALMONELLA=60, INFLUENZA=600/1150, FOOD_POISON=1; `eagents.c`).

## 4. Где перехватить смерть

Детект смерти у мода: `dmAISurvivor.OnUpdate` → `if (!m_Pawn.IsAlive()) OnDeath()`
(`dmAISurvivor.c:515-517`). Порядок событий на кадре смерти:
1. `TickBodySystems` (`dmAISurvivorBase.c:1339`) зовёт `OnScheduledTick(dt)` → модификатор
   доводит здоровье до 0. `TickBodySystems` гейтится `IsAlive()` на месте вызова
   (`:1186-1187`), т.е. тикает только пока жив — после этого кадра больше не тикает.
2. В следующем `OnUpdate` — `IsAlive()==false` → `OnDeath()`.

Что валидно в точке `OnDeath()`/первом кадре `!IsAlive()`:
- **`IsAlive()`** — false (`object.c:520`).
- **`GetHealth("","Health")`** / **`GetHealth("GlobalHealth","Health")`** — 0 (оба алиаса
  используются в ванили: InjuryHandler/DebugMonitor через `""`, heartattack/burning через
  `"GlobalHealth"`; `[нужно подтвердить]` идентичность значений).
- **`GetModifiersManager()`** — валиден, **модификатор-убийца ещё активен** (`m_IsActive==true`):
  ванильный `PlayerBase.OnScheduledTick` гейтится `!IsAlive()` (`playerbase.c:2807-2809`), т.е. модификаторы
  НЕ деактивируются на смерть; `DeactivateCondition` у болезней проверяет агента (не здоровье),
  у Hunger/Thirst/HeatComfort/Burning/HeartAttack — `DisableDeactivateCheck()` → никогда не
  деактивируются. `EEKilled` тоже НЕ чистит модификаторы (`playerbase.c:1183-1221` — только
  BleedingManager, DB, SymptomManager, труп). Т.е. в `OnDeath()` всё ещё можно читать
  `IsModifierActive(MDF_X)` + статы.
- **`GetStatWater()/GetStatEnergy()/GetStatHeatComfort()`** — валидны (protected-поля, но
  getters public; `playerbase.c:7843/7861/7870`).

⚠️ Тонкость: детект в `OnUpdate` — **следующий кадр** после урона. Если между `OnScheduledTick`
и `OnUpdate` что-то ещё тронет статы/модификаторы — картина слегка устареет. Для точной
атрибуции надёжнее логировать в том же месте, где тикается модификатор (см. рекомендации).

## 5. Эталон / альтернатива (подтверждено)

Ваниль уже умеет две вещи:
1. **`BleedingCheckMdfr.OnActivate`** (`bleeding.c:23-37`): до убийства
   (`SetHealth("","",-1000)`) зовёт **`m_AdminLog.BleedingOut(player)`** → ADM-строка
   «… bled out» (`pluginadminlog.c`, метод `BleedingOut`). Это прямой эталон «лог причины
   в модификаторе-убийце ДО доведения здоровья до нуля».
2. **`PluginAdminLog.PlayerKilled(player, source)`** (`pluginadminlog.c`): ветка
   `if (player == source)` («deaths not caused by another object (starvation, dehydration)»)
   читает `GetStatWater()/GetStatEnergy()/GetBleedingManagerServer().GetBleedingSourcesCount()`
   и `GetDrowningWaterLevelCheck()` → пишет `«… died. Stats> Water: X Energy: Y Bleed sources: N»`
   или `«… drowned.»`. Есть также `PlayerKilledByDrowningUncon`, `PlayerKilledByRespawn`,
   `PlayerKilledByDisconnect`, `Suicide`, `BleedingOut`.

**Главный гэп ванили**: ветка «естественной смерти» в `PlayerKilled` достижима ТОЛЬКО если
движок диспатчил `EEKilled` с `killer == player` (self). Для Health-стат-смертей (голод/жажда)
этот диспатч не подтверждён → ветка не отработает. Для рут-хелф-смертей (холод/утопление/
кровотечение) — скорее отработает.

⚠️ **Засада мода**: `dmAISurvivorBase.EEKilled` (`:1531-1565`) при `killer == this || killer == null`
подменяет killer на `m_LastDamageSource` (последний ВНЕШНИЙ источник урона). Для естественной
смерти `m_LastDamageSource == null` → подмена даёт `null` → ванильный `PlayerKilled(this, null)`
упирается в guard `if (!player || !source)` → «DEBUG: PlayerKilled() player/source does not exist»
вместо честного «died». Т.е. текущая подмена **ломает** ванильную ветку естественной смерти:
нужно различать «внешний килл с self/null-killer» (подменять на m_LastDamageSource) и «настоящая
естественная смерть» (не подменять — дать killer==this, чтобы ветка `player==source` сработала).

Expansion **не добавляет** причину смерти от болезни: его `PlayerBase.EEKilled`
(`DayZExpansion_Core/.../PlayerBase.c:845`) только монитор-позиция + `super`; `DayZPlayerImplement.EEKilled`
(`:233`) — лут-лок + `super`. Готового механизма «причина смерти от болезни» в Expansion нет.

## Рекомендация для мода

1. **Не полагаться на `EEKilled`** для голода/жажды/болезни (Health-стат-канал) — он не
   диспатчится. Логировать в `OnDeath()` (или в `TickBodySystems` при первом `!IsAlive()`):
   снять статы (`Water/Energy/HeatComfort/Blood`) + активные болезнь-модификаторы и записать
   через `m_AdminLog.DirectAdminLogPrint(...)` (public `m_AdminLog`, `playerbase.c:319`) или
   `PluginAdminLog.Cast(GetPlugin(PluginAdminLog)).DirectAdminLogPrint(...)`.
2. **Формат — «состояние, а не виновник»** (human model): `«… died. Water=.. Energy=.. HeatComfort=..
   Blood=.. diseases=cholera,salmonella»`. Дополнить флагом утопления `GetDrowningWaterLevelCheck()`.
3. **Починить подмену killer** в `EEKilled`: не заменять `killer==this` на `m_LastDamageSource`,
   если `m_LastDamageSource == null` (естественная смерть) — иначе ванильная ветка
   `player==source` никогда не сработает.

## Открытые вопросы / `[нужно подтвердить]`

- **Диспатч `EEKilled` по каналам** (см. §2) — нативно, из скриптов не видно; сформулировать
  probe для `dayz-tester`: (а) форс гипотермии (HeatComfort < −0.45) → приходит ли `EEKilled`
  с `killer==this`; (б) форс голода (Energy ≤ 300) до Health 0 → приходит ли `EEKilled` вообще.
  Ручки моста: `spawnobj` + форс-модификатор/стат + `botdump`.
- **`GetHealth("","Health")` ≡ `GetHealth("GlobalHealth","Health")`** — два алиаса в ванили,
  идентичность значения не подтверждена. Рекомендация: читать `GetHealth("GlobalHealth","Health")`
  (тот же канал, что бьют модификаторы).
- **Валидность модификатора после смерти** — по коду `m_IsActive` остаётся true (нет деактивации
  на смерть), но не проверено эмпирически, что `IsModifierActive()` возвращает true в `OnDeath()`
  у AI_SERVER (а не сбрасывается нативом).

## Источники (файл:строка, ваниль = `DayZ Projects/scripts`)

- `4_world/classes/playermodifiers/emodifiers.c:1` (enum `eModifiers`).
- `4_world/classes/playermodifiers/modifiersmanager.c:90-146` (Init-список), `:199` (IsModifierActive),
  `:204` (OnScheduledTick), `:219/:224` (Activate/Deactivate), `:306` (GetModifier), `:326` (DbgGetModifiers).
- `4_world/classes/playermodifiers/modifierbase.c:91` (Tick), `:130` (IsActive), `:150` (GetAttachedTime),
  `:160` (GetModifierID), `:193/:217` (Activate/Deactivate), `:176/:181` (Activate/DeactivateCondition).
- `…/modifiers/conditions/bleeding.c:23-37` (BleedingOut + SetHealth рут), `…/conditions/burning.c:47`,
  `…/conditions/heartattack.c:36`, `…/conditions/heatbuffer.c` (MDF_HEATBUFFER).
- `…/modifiers/hunger.c:45`, `…/modifiers/thirst.c:45`, `…/modifiers/heatcomfortmdfr.c:75`,
  `…/modifiers/drowning.c:35`, `…/modifiers/health.c` (MDF_HEALTH — notifier-only).
- `…/modifiers/diseases/cholera.c` (ActivateCondition :агент 250, WATER_LOSS_CHOLERA),
  `salmonella.c` (агент 60), `poisoning.c`, `fever.c` (WATER_LOSS_FEVER), `influenza.c`,
  `commoncold.c`, `pneumonia.c:62` (AddHealth рут, агент 1150).
- `3_game/playerconstants.c:40-143` (SL_ENERGY/WATER_MAX=5000, LOW_*=300, *_DAMAGE_PER_SEC=0.05,
  BLOOD_THRESHOLD_FATAL=2500, THRESHOLD_HEAT_COMFORT_*_CRITICAL=±0.45, HEALTH_LOSS_HC_*=0.035/0.30,
  WATER_LOSS_FEVER=0.2).
- `3_game/enums/eagents.c:1` (enum `eAgents`, битмаска 1/2/4/…).
- `3_game/entities/object.c:520-522` (IsAlive), `:990/:1011/:1023/:1058/:1073/:1106`
  (GetHealth/SetHealth/AddHealth и одноарг-алиасы), `:977` (IsDamageDestroyed native).
- `4_world/entities/manbase/playerbase.c:1183-1221` (EEKilled), `:5868` (EEHealthLevelChanged),
  `:2807-2816` (OnScheduledTick, гейт `!IsPlayerSelected() || !IsAlive()`), `:7843/:7861/:7870`
  (GetStatWater/Energy/HeatComfort), `:6813` (GetDrowningWaterLevelCheck), `:7772` (GetSingleAgentCount),
  `:1089` (HasDisease), `:319` (m_AdminLog public), `:88` (m_ModifiersManager public).
- `4_world/plugins/pluginbase/pluginadminlog.c`: `PlayerKilled` (ветка `player==source`),
  `BleedingOut`, `PlayerKilledByDrowningUncon`, `DirectAdminLogPrint`.
- Мод: `src/core/4_World/Entities/Bot/dmAISurvivorBase.c:1339-1370` (TickBodySystems),
  `:1531-1565` (EEKilled с подменой killer), `:1186-1187` (гейт IsAlive); `dmAISurvivor.c:515-517`
  (детект смерти → OnDeath).
- Expansion (нет механизма причины): `DayZExpansion/Core/.../PlayerBase.c:845` (EEKilled →
  super), `.../DayZPlayerImplement.c:233` (EEKilled → super).

---

## Существенные развилки / решения (для `docs/decisions.md`)

1. **Поправка к premise «холод не диспатчит EEKilled»**: холод/жар (`MDF_TEMPERATURE`) бьют
   РУТ-хелф, а не Health-стат — в отличие от голода/жажды (Health-стат). Это меняет, для каких
   естественных смертей ванильный `PlayerKilled` сработает, а для каких нужен свой лог.
2. **«Виновник» vs «состояние»**: отказаться от вычисления «какой модификатор убил» (многослойная
   цепочка болезнь→стат→Health) в пользу записи наблюдаемого состояния (Water/Energy/HeatComfort/
   Blood/болезни) — эталон ванильного `PlayerKilled`, соответствует «человеческой модели».
3. **Подмена killer в `EEKilled` ломает естественную ветку**: текущий код заменяет
   `killer==this/null` на `m_LastDamageSource`; при естественной смерти `m_LastDamageSource==null`
   → ветка `player==source` в `PluginAdminLog` недостижима. Нужно различать эти два случая.
