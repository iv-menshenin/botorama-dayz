# MedicalCare — ИИ-лечение ботов (план)

Цель: бот, не находясь в бою, самостоятельно лечит себя (перевязка, шина, таблетки)
с анимацией. Согласованный дизайн. Research по ванильному API — `docs/research/medical.md`
(читать при реализации). Готчи синтаксиса Enfusion — `docs/codeguide.md`.

## Зафиксированные решения

1. Триггер входа в состояние — гейт `!IsInCombat() && AreNecessaryMedicationsAvailable()`
   (флаг `IsInCombat` ставят/снимают FSM-состояния `Shooting`/`Fighting` — НЕ threat).
2. Бонусы (обезбол, витамины) НЕ триггерят вход — только выполняются внутри состояния,
   если предмет есть. Исключение: витамины — standalone-триггер при холоде.
3. Шина: если есть в инвентаре — берётся; нет — спавнится в руки с платой
   (1 бинт → 4 тряпки → бесплатно). Бандана в плату НЕ идёт.
4. Таблетки/витамины — additive-анимация (на бегу). Бинт/шина — full-body.
5. Болеутоляющее — только бонус (после шины; или после перевязки при HP<75%).

## Таблица соответствий

| # | Лекарство | Класс | Триггер | Эффект проверять (действует) | Роль |
|---|---|---|---|---|---|
| 1 | Бинт/тряпка/бандана | `BandageDressing`/`Rag`/`Bandana_ColorBase` | `IsBleeding()` | `IsBleeding()==false` | Primary |
| 2 | Шина | `Splint` | `GetBrokenLegs()==BROKEN_LEGS` | `GetBrokenLegs()==BROKEN_LEGS_SPLINT` | Primary |
| 3 | Болеутоляющее | `PainkillerTablets` | бонус (a) после шины; (b) после перевязки И `GetHealth01()<0.75` | `MDF_PAINKILLERS` | Bonus |
| 4 | Уголь | `CharcoalTablets` | `MDF_POISONING && !MDF_CHARCOAL` | `MDF_CHARCOAL` | Primary |
| 5 | Тетрациклин | `TetracyclineAntibiotics` | `(MDF_INFLUENZA\|\|MDF_COMMON_COLD) && !MDF_ANTIBIOTICS` | `MDF_ANTIBIOTICS` | Primary |
| 6 | Мультивитамины | `VitaminBottle` | (a) холод `HeatComfort<=-0.15 && !MDF_IMMUNITYBOOST`; (b) бонус после лечения | `MDF_IMMUNITYBOOST` | Primary(холод)+Bonus |

## Ванильный API (из research/medical.md)

### Детект (сервер, `dmAISurvivorBase : PlayerBase`)
- Рана: `PlayerBase.IsBleeding()` (⚠️ НЕ `MDF_BLEEDING` — это смертельная кровопотеря).
- Перелом: `GetBrokenLegs()` → `eBrokenLegs` (`NO_BROKEN_LEGS=0, BROKEN_LEGS=1, BROKEN_LEGS_SPLINT=2`);
  или `GetModifiersManager().IsModifierActive(eModifiers.MDF_BROKEN_LEGS)`. `MDF_FRACTURE` не существует.
- Отравление: `IsModifierActive(MDF_POISONING)`.
- Грипп/простуда: `IsModifierActive(MDF_INFLUENZA)` / `MDF_COMMON_COLD`.
- Действующие препараты: `IsModifierActive(MDF_PAINKILLERS / MDF_ANTIBIOTICS / MDF_CHARCOAL / MDF_IMMUNITYBOOST)`.
- Холод: `GetStatHeatComfort().Get() <= PlayerConstants.THRESHOLD_HEAT_COMFORT_MINUS_WARNING (-0.15)`.
- Менеджер: `GetModifiersManager()` (`ActivateModifier`/`DeactivateModifier`/`IsModifierActive`).

### Применение (прямой путь, ActionManager для ИИ НЕ работает)
- Перевязка: `GetBleedingManagerServer().RemoveMostSignificantBleedingSourceEx(item)` + расход предмета.
- Шина: `ApplySplint()` + `SetBrokenLegs(eBrokenLegs.BROKEN_LEGS_SPLINT)` + спавн `Splint_Applied` в инвентарь + `Delete()` шину.
- Таблетки: `Edible_Base.Consume(1.0, pawn)` (сам делает `AddQuantity(-1)` + `OnConsume` → `ActivateModifier`).

### Анимации (граф-команды + `dmBotActionAnimCB`, эталон `dmBotIntent_OpenDoor.c`)
- Бинт: `StartCommand_Action(DayZPlayerConstants.CMD_ACTIONFB_BANDAGE=58, dmBotActionAnimCB, STANCEMASK_CROUCH)` (full-body).
- Шина: `StartCommand_Action(CMD_ACTIONFB_CRAFTING=59, ...)` (full-body).
- Таблетка: `AddCommandModifier_Action(CMD_ACTIONMOD_EAT_TABLET=528, dmBotActionAnimCB)` (additive).
- Витамины: `AddCommandModifier_Action(CMD_ACTIONMOD_EAT_PILL=527, ...)` (additive).
- Завершение: поллинг `GetCommand_Action()==null && GetCommandModifier_Action()==null` (НЕ `IsEmotePlaying()`).

### Инвентарь / спавн
- Перечисление: `pawn.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items)`.
- В руки ИИ: `dmLoot.TakeToHands(pawn, item)`; спавн в руки: `pawn.GetHumanInventory().CreateInHands(cls)`.
- Расход: `item.AddQuantity(-n, true)` / `item.Delete()`.

## Классы/файлы

### 1. `dmAISurvivor` (мозг) — флаг-методы + хелперы
```c
bool IsMedicalAttentionRequired();          // здоровье + действующие препараты (без инвентаря)
bool AreNecessaryMedicationsAvailable();    // то же + наличие лекарств (кроме шины)
```
Логика `AreNecessaryMedicationsAvailable()` (true если любое):
1. `IsBleeding() && hasBandage`.
2. `GetBrokenLegs()==BROKEN_LEGS` (шина — всегда доступна: инвентарь или спавн).
3. `MDF_POISONING && !MDF_CHARCOAL && hasCharcoal`.
4. `(MDF_INFLUENZA||MDF_COMMON_COLD) && !MDF_ANTIBIOTICS && hasTetra`.
5. `HeatComfort<=-0.15 && !MDF_IMMUNITYBOOST && hasVitamins`.

`IsMedicalAttentionRequired()` — то же без `has*`-проверок инвентаря.

Хелперы в `dmLoot`: `IsBandage(item)` (BandageDressing/Rag/Bandana_ColorBase),
`FindItemByClass(pawn, typename)` (по `EnumerateInventory`), `CountItem`/`ConsumeItemQuantity`
(для платы шины). Раздельно: «бинт» = `BandageDressing`, «тряпка» = `Rag`.

### 2. Условие `dmBotCondition_MedicalCare` + фабрика
`Evaluate(bot) = !bot.IsInCombat() && bot.AreNecessaryMedicationsAvailable()`.
Фабрика: `dmBotConditions.MedicalCare()`.

### 3. Состояние `dmBotState_MedicalCare` (новый файл)
- `GetKind() = INTERRUPTIBLE` (бой вытесняет).
- `OnEntry`: построить очередь шагов (снапшот, дедупликация бонусов):
  1. `IsBleeding && hasBandage` → БИНТ; затем `GetHealth01()<0.75 && hasPainkiller && !MDF_PAINKILLERS` → ОБЕЗБОЛ (bonus b).
  2. `GetBrokenLegs()==BROKEN_LEGS` → ШИНА; затем `hasPainkiller && !MDF_PAINKILLERS` → ОБЕЗБОЛ (bonus a, дедуп).
  3. `MDF_POISONING && !MDF_CHARCOAL && hasCharcoal` → УГОЛЬ.
  4. `(грипп) && !MDF_ANTIBIOTICS && hasTetra` → ТЕТРАЦИКЛИН.
  5. `hasVitamins && !MDF_IMMUNITYBOOST` → ВИТАМИНЫ (бонус; при холоде это единственный шаг).
- `OnUpdate`: если нет активного интента — взять следующий шаг (пере-проверить его условие,
  иначе пропустить) → создать `dmBotIntent_MedicalAction`; дождаться `IsFinished()`;
  пустая очередь → `EXIT`. Хелдержит `ref`, пересоздаёт при автодедлайне.

### 4. Интент `dmBotIntent_MedicalAction` (новый файл, generic)
Поля: `int m_Kind` (BANDAGE/SPLINT/PILL), `ItemBase m_Item`, `int m_AnimCmd` (для PILL), фазы:
1. **prepare** — бинт/таблетка → `dmLoot.TakeToHands`; шина → инвентарная или `CreateInHands` (+плата).
2. **animate** — граф-команда по kind (full-body для бинта/шины, additive для таблетки).
3. **wait** — поллинг завершения команды + таймаут `DM_MEDICAL_ANIM_TIMEOUT` (фолбэк, full-body не подтверждён).
4. **apply** — эффект (таблица выше) + расход.
5. `Finish()`.

### 5. Пресеты (все 4)
`dmBotPreset_Survivor/Escort/Hunter/Combat` — добавить состояние `MedicalCare` + рёбра
`idle/explore/patrol/follow → medical` с `.Require(dmBotConditions.MedicalCare())` (вес 2.0, приоритетный).

### 6. Константы `cons/4_World/constants.c`
```c
static const float DM_MEDICAL_PAINKILLER_HEALTH_THRESHOLD = 0.75;  // бонус-обезбол после перевязки
static const int   DM_MEDICAL_SPLINT_RAG_COST = 4;                 // тряпок за спавн шины
static const float DM_MEDICAL_COLD_HC = -0.15;                     // порог «холодно» для витаминов
static const float DM_MEDICAL_ANIM_TIMEOUT = 4.0;                  // фолбэк ожидания анимации
```

### 7. Bump `DM_BOTORAMA_VERSION`.

## Чек-лист реализации (codeguide)
- Без тернарников, `-=`/`+=` на элементе вектора, переносов строк в выражении.
- `ref` не на параметрах методов; `out`/`inout` только в сигнатуре.
- Интент НЕ создаёт другие интенты (обезбол/витамины — это шаги очереди СОСТОЯНИЯ, не интента).
- Логи — `#ifdef DM_BOT_DEBUG_MEDICAL` на месте вызова (новый домен добавить в `defines.c` + при
  необходимости в `config.cpp`).
- Анимация full-body — с таймаут-фолбэком (не полагаться на завершение у ИИ).

## Открытые риски
- Full-body `StartCommand_Action(CMD_ACTIONFB_BANDAGE/CRAFTING)` у серверного ИИ не подтверждён —
  таймаут-фолбэк обязателен.
- `TransmitAgents` (шанс заражения раны бинтом) — пропущено по дизайну (заражение ИИ не моделируется).

---

# Тесты MedicalCare (автотесты)

Каркас: `dmTestSuite_TestCase` (`Setup`/`GetSummary`/`GetDuration`/`OnCheck`) + `dmTestSuite_TestRunner`
(спавн → 5с тишины `DM_TEST_QUIET_SECONDS` → `Setup` → поллинг). Команды в `dmTestCommand.Handle` +
имена `DM_CHAT_TEST_*` (`test/3_Game/constants.c`).

Чистый тест-пресет `dmBotTestPreset_Medical` (Idle + MedicalCare, ребро `idle→medical` `.Require(MedicalCare())`,
`medical→idle`).

Хелпер `dmTestSuite_TestCase.GiveMedicalPants()` — штаны `CargoPants_Beige` + в карго все медикаменты:
`BandageDressing`, `Rag`, `Splint`, `PainkillerTablets`, `CharcoalTablets`, `TetracyclineAntibiotics`, `VitaminBottle`.

| Команда | Эффект | PASS |
|---|---|---|
| `bandaging` | `GetBleedingManagerServer().AttemptAddBleedingSourceBySelection("Pelvis")` | `!IsBleeding()` |
| `splinting` | `SetBrokenLegs(-BROKEN_LEGS)` + `ActivateModifier(MDF_BROKEN_LEGS)` | `GetBrokenLegs()==BROKEN_LEGS_SPLINT` |
| `painkiller` | перелом (+обезбол в инвентаре) | `IsModifierActive(MDF_PAINKILLERS)` (бонус a: после шины) |
| `painkillerbandage` | кровотечение + `SetHealth("","Health",30.0)` (+обезбол) | `IsModifierActive(MDF_PAINKILLERS)` (бонус b: после перевязки при HP<75%) |
| `charcoal` | `ActivateModifier(MDF_POISONING)` | `IsModifierActive(MDF_CHARCOAL)` |
| `tetracycline` | `ActivateModifier(MDF_INFLUENZA)` | `IsModifierActive(MDF_ANTIBIOTICS)` |
| `vitamins` | `GetStatHeatComfort().Set(-0.5)` (null-check) | `IsModifierActive(MDF_IMMUNITYBOOST)` |

Плату за спавн шины НЕ проверяем (в тестах шина даётся в инвентарь). `GetDuration()` ~30с.
Доп. мини-фикс: null-check `GetStatHeatComfort()` в `IsMedicalAttentionRequired`/`AreNecessaryMedicationsAvailable`.
