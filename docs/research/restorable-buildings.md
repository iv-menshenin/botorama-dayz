# Research: «восстанавливаемые здания» DayZ 1.30 — достройка двери + кодовый замок (DigitalCodeLock)

Цель: понять core-механику 1.30 «восстанавливаемое здание → достроить дверь → повесить
кодовый замок → закрыть кодом» и ответить, (а) core это или DLC, (б) как это делается
руками, (в) поведёт ли Exploration бота в такое здание.

> **ИСПРАВЛЕНИЕ ПРОШЛОЙ ЗАМЕТКИ `codelock-fence-gate.md`.** Тот прогон заключил
> «восстанавливаемые здания = Badlands DLC, в Script-Diff их нет» и увёл задачу в
> `Fence`+`CombinationLock`. Это **НЕВЕРНО по причине неверного дампа**: исследование
> велось по `/home/devalio/dayz/Work/DayZ Projects/scripts` = **DayZ 1.21**
> (`scripts.txt version=121737`). Актуальный дамп 1.30 — это
> **`/home/devalio/dayz/Work/DayZ-Script-Diff-130/scripts`** (`version=127250`, тот же
> источник, что в `1.30-breaking-changes.md`). В 1.21 base-building = только
> `Fence`/`Watchtower`/`ShelterSite`/`TerritoryFlag` (нет строительства на зданиях).
> В 1.30 base-building **полностью переписан** и появилась достройка дверей на зданиях.
> Рецепт `Fence`+`CombinationLock` из прошлой заметки **остаётся валидным как фолбэк**
> (забор в поле), но это НЕ то, что нужно задаче — нужен **Building**.

---

## 0. Главный вывод (короткий)

- **«Восстанавливаемые здания» — это CORE-фича 1.30, не Badlands DLC.** Всё в ванильном
  дампе 1.30. Механика: `ConstructionBase`/`ConstructionBasic` (новый базовый слой),
  `Rebuilding : ConstructionBase` (компонент стройки на **зданиях**), типы частей
  `ConstructionPartRebuild*` (в т.ч. `...ReversibleDoorTypeData` = достраиваемая дверь),
  и **новый цифровой кодовый замок `DigitalCodeLock`** (батарейный, PIN 4–6 цифр).
- **`BuildingBase : Building` (а значит `House`/`Building`/`ContainerLockedBase`) в 1.30
  ВСЕ имеют `Construction`-компонент** (`Rebuilding`) — `CreateConstructionComponent()`
  возвращает `new Rebuilding(this)`. Достроенная дверь — это **нативная дверь** здания
  (`GetDoorCount()` её считает), связанная с construction-частью через config
  (`doorConstructionPart` + `doorConstructionPhysicsSource`).
- **Кодовый замок на здании — `DigitalCodeLock`** (keypad), а НЕ `CombinationLock`
  (дисковый 3/4 цифры, остался для ворот `Fence`). Замок вешается в слот
  `Att_CodeLock<N>_<material>` (напр. `Att_CodeLock1_Wood`) и при запирании зовёт
  **нативный `Building.LockDoor(doorIndex)`** → `IsDoorLocked(doorIdx) == true`.
- **Exploration поведёт бота в такое здание**: `Land_House_*` rebuildable — это `House`
  (`BuildingSuper`), регистрируются в `dmLiveBuildingRegistry`; их двери — нативные.
  Запертая дверь блокирует `OpenDoor` (rattle) → бот не попадёт внутрь.

---

## 1. (а) Core vs DLC

| Вопрос | Ответ | Доказательство |
|---|---|---|
| Механика стройки на зданиях core? | **Да, core 1.30** | `4_world/classes/constructionbase.c`, `4_world/classes/rebuilding/rebuilding.c`, `4_world/classes/basebuilding/typeconstructiondata/*` — в ванильном дампе |
| Здания умеют достройку? | **Да** | `4_world/entities/game/super/building.c:23` `BuildingBase.CreateConstructionComponent() → new Rebuilding(this)` |
| Кодовый замок для двери здания core? | **Да** | `4_world/entities/itembase/codelock.c:1` `class DigitalCodeLock : ItemBase` |
| Rebuildable-дома в дампе? | **Да** | `4_world/entities/building/rebuildable/{house_1m1..3s2, irrigationtunnel_entrance}.c` |
| На тест-сервере (nasdara)? | **Да, грузятся** | лог `DayZ_x64_2026-10-31_18-33-10.rpt`: `Load entity type 'Land_House_1M1/1M2/.../2M1/..._Dam2'` |

> **Про Badlands**: предыдущий вывод «Badlands DLC» ошибочен. `1.30-breaking-changes.md`
> уже фиксировал, что target 1.30 = `DayZ-Script-Diff-130` (127250); прошлый прогон
> codelock-заметки этого не учёл и смотрел в 1.21.

## 2. Архитектура (классы, файл:строка из 1.30-дампа)

### 2a. Новый базовый слой стройки

```
ConstructionBasic                                 3_game/systems/construction_basic.c
  └─ ConstructionBase : ConstructionBasic         4_world/classes/constructionbase.c:30
       ├─ Construction : ConstructionBase         4_world/classes/basebuilding/construction.c  (BaseBuildingBase = Fence/Watchtower/...)
       └─ Rebuilding : ConstructionBase           4_world/classes/rebuilding/rebuilding.c:1    (ЗДАНИЯ)
```

- `EntityAI.CreateConstructionComponent()` → `null` по умолчанию (`3_game/entities/entityai.c:3474`).
- `EntityAI.CanUseConstruction()` = `GetConstructionBasic() != null` (`entityai.c:3485`).
- `Object.CanUseConstruction()` = `false` (`3_game/entities/object.c:698`).
- `BuildingBase` переопределяет `CreateConstructionComponent()` → `new Rebuilding(this)`
  (`4_world/entities/game/super/building.c:23`), и `GetConstructionBasic()` → `m_Construction`
  (`building.c:48`).

### 2b. Типы construction-частей (новый каталог `typeconstructiondata/`)

| Класс | Файл | Роль |
|---|---|---|
| `ConstructionPartTypeData` | `typeconstructiondata/constructionparttyped.c` | базовый тип части (config-driven: `name`, `id`, `is_gate`, `Materials`, `required_parts`, `build_action_type`) |
| `ConstructionPartRebuildTypeData` | `constructionpartrebuildtyped.c` | маркер «достраиваемая часть» (на руинах здания) |
| `ConstructionPartRebuildReversibleBaseTypeData` | `constructionpartrebuildreversiblebasetyped.c` | реверсИруемая часть (дверь/баррикада) |
| **`ConstructionPartRebuildReversibleDoorTypeData`** | `constructionpartrebuildreversibledoortyped.c` | **достраиваемая дверь**: config `CustomPartTypeProperties { door_board_default, door_board_reversed }` — имена дверных полотен (default/перевёрнутая сторона) |
| `ConstructionPartRebuildReversibleBarricadeTypeData` | `constructionpartrebuildreversiblebarricadetyped.c` | баррикада (доски на окна/двери) |
| `ConstructionPartRebuildRubbleTypeData` | `constructionpartrebuildrubbletyped.c` | завалы/руины (`rubble_*`) |
| `ConstructionPartRebuildInvertedTypeData` | `constructionpartrebuildinvertedtyped.c` | инвертированная часть |
| `ConstructionPart` (инстанс) | `basebuilding/constructionpart.c` | состояние одной части (`IsBuilt`, `IsGate`, sync-биты) |

> `ConstructionPartRebuild` — инстансный тип для `Rebuilding` (`rebuilding.c:579`
> `GetDefaultPartInstanceTypename() → ConstructionPartRebuild`).

### 2c. Дверь здания ↔ construction-часть (связь через config)

`3_game/entities/additionaldoorsinfo.c` — `AdditionalDoorInfo` (на дверь) читает из
`cfgVehicles <тип> Doors <door>:`:
- `doorConstructionPart` → имя construction-части, связанной с этой дверью (пусто = обычная дверь).
- `doorConstructionPhysicsSource` → имя AnimationSource, управляющего физикой двери
  (**обязателен для rebuildable-дверей**).
- `relatedInventorySlots` → слоты замков (`Att_CodeLock*`/`Att_CombinationLock*`).
- `lockCompatibilityBitMask` → совместимость с ключом (дефолт `1 << LOCKPICK`).

`AdditionalDoorInfosHolder` хранит их на тип (`building.c:20` статик `m_AdditionalDoorInfosMap`).
`Building.GetDoorInfo(doorIdx)` / `GetDoorInfoHolder()` — доступ (`building.c:312/317`).

Физика достроенной двери переключается AnimationSource-ом:
`Rebuilding.OnConstructionDoorOpenStart/CloseStart` (`rebuilding.c:439`) →
`building.SetAnimationPhase(doorConstructionPhysicsSource, 1.0 / 0.0)`.

### 2d. Цифровой кодовый замок `DigitalCodeLock`

`4_world/entities/itembase/codelock.c:1` `class DigitalCodeLock : ItemBase`:

| Сигнатура | строка | смысл |
|---|---|---|
| `bool IsLocked()` | :373 | `m_CodeLockComponent.IsLocked()` |
| `void SetLockState(bool state)` | :363 | задать состояние замка |
| `string GetLockPIN()` | :353 | текущий PIN |
| `bool NeedBattery()` | :333 | нужна батарейка (`Battery9V`) |
| `void UnlockOnServer(EntityAI parent)` | :277 | отпереть + выронить замок из слота |
| `override bool CanDetachAttachment(parent)` | :164 | `false` если заперт |
| `override void OnWasAttached(parent, slot_id)` | :111 | при слот `Att_CodeLock*` → `SetDoorInfoFromSlot` |
| `SetDoorInfoFromSlot(BuildingBase, slotName)` | :172 | матчит `relatedInventorySlots` → `doorIndex`; читает `Reversed` у реверс-двери |
| `override bool IsExternalLockType()` | :542 | `true` (гейт `CanReceiveAttachment`) |
| `override void SetActions()` | :470 | `AddAction(ActionAttachToConstruction)` |

`DigitalCodeLock_Bunker : DigitalCodeLock` (:567) — бункерный вариант (без батарейки, без
смены кода) — вне скоупа.

Состояние замка — в `CodeLockComponent` (`4_world/classes/codelockcomponent.c`):

| Сигнатура | строка | смысл |
|---|---|---|
| `MIN_LOCK_PIN_LENGTH = 4`, `MAX = 6` | :23 | PIN 4–6 цифр |
| `bool SetLockCode(string pin, bool sync = true)` | :121 | задать PIN, `m_HasPIN=true`, `SetLockState(true,false)` |
| `void SetLockState(bool state, bool sync = true)` | :157 | при `state=true` → `HandleHierarchyRootLock(true)` |
| `HandleHierarchyRootLock(bool state)` | :212 | **`building.LockDoor(m_DoorIndex)` / `UnlockDoor(m_DoorIndex)`** (натив!) |
| `void CheckCodeRequest(string pin, PlayerIdentity)` | :350 | ввод кода: заперт+верно → unlock; нет PIN → set PIN+lock; неверно → brute-force |
| `void SetDoorIndex(int index)` | :627 | привязка замка к двери здания |

**Смысл**: `DigitalCodeLock.IsLocked()` → заперт → `Building.IsDoorLocked(doorIdx) == true`
(через нативный `LockDoor`), поэтому `OpenDoor` на такой двери **не открывает** (rattle,
подтверждено Expansion `eAIBase.c:11720` комментарием).

> **Готча для dev**: `BuildingBase.IsLocked(int doorIndex)` (`building.c:283`) — **скриптовый**
> метод с инвертированной логикой (возвращает `false` и при найденном запертом замке — т.е.
> всегда `false`). НЕ использовать его как предикат «дверь заперта». Корректный предикат —
> **нативный `Building.IsDoorLocked(doorIdx)`** (`building.c:55`), а гейт открытия —
> `BuildingBase.CanDoorBeOpened(params)` (`building.c:307`), который возвращает `false` при
> запертом замке.

### 2e. `CombinationLock` (дисковый) — остался для ворот

`4_world/entities/itembase/combinationlock.c:25` `class CombinationLock extends ItemBase`
— дисковый замок (3/4 цифры): `SetCombination` (:233), `LockServer(parent, ignore)` (:384),
`IsLockedOnGate()` (:847), `IsLockAttached()` (:534). Используется на **`Fence`** (воротах)
через слот `Att_CombinationLock` (константа `BuildingBase.ATTACHMENT_SLOT_COMBINATION_LOCK`,
`building.c:10`). Это фолбэк прошлой заметки — актуален только для отдельно стоящих ворот,
НЕ для достроенной двери здания.

## 3. (б) Ручной рецепт «достроить дверь + замок кодом» (тестировщик)

Всё через штатный UI игрока (никакого скрипта). Порядок:

1. **Найти повреждённое rebuildable-здание** `Land_House_*_Dam2` (на nasdara грузятся,
   см. §1). У него отсутствуют/разрушены часть стен/крыши/дверь.
2. **Принести материалы + инструмент.** Дверь — это wood-часть (`door<N>_wood`), значит
   нужны **деревянные доски (`WoodenPlank`) + гвозди (`Nail`) + молоток** (маска
   `EConstructionTools.TOOL_NAIL`). Материалы грузятся в слоты `Materials` самого здания
   (вкладка стройки в vicinity), количество/тип — из config (`Materials` блока части).
   `[нужно подтвердить]` точные имена материалов/количества — они в game-data (config), а
   не в Script-Diff (см. §5).
3. **Достроить дверь**: навестись на проём, действие **`ActionBuildPart`** (continuous)
   (`4_world/.../actions/continuous/actionbuildpart.c`). Для реверс-двери
   (`door_board_default`/`door_board_reversed`) можно выбрать ориентацию полотна.
4. **Повесить `DigitalCodeLock`**: нужен замок **`DigitalCodeLock`** + **`Battery9V`**
   внутри. Навестись на дверь → **`ActionAttachToConstruction`** (`actionattachtoconstruction.c`)
   → замок уходит в слот `Att_CodeLock<N>_<material>` (напр. `Att_CodeLock1_Wood`).
   `OnWasAttached` (:111) сам привяжет `doorIndex` через `relatedInventorySlots`.
5. **Задать PIN и запереть**: открыть UI замка (`DigitalCodeLockUI`), ввести 4–6 цифр.
   `CheckCodeRequest` → PIN пуст → `SetLockCode(pin)` → `SetLockState(true)` →
   `HandleHierarchyRootLock(true)` → **`LockDoor(doorIndex)`**. Дверь ЗАПЕРТА.
6. **Проверить**: `IsDoorLocked(doorIdx) == true`, `OpenDoor` дребезжит (не открывает).
   Открыть: ввести тот же PIN → `SetLockState(false)` → `UnlockDoor`.

> Порядок важен: сначала **достроить дверь**, потом замок (`CanReceiveAttachment`,
> `building.c:190` — проверяет `IsDoorOpen(doorIndex)` и конфликтующие слоты замков).

## 4. (в) Поведёт ли Exploration бота в такое здание?

**Да, поведёт** — по трем причинам:

1. **Регистрация**: `dmLiveBuildingRegistry` регистрирует `Building`/`House` (через
   `modded class House`, `src/reg/4_World/modded_House.c`). Rebuildable `Land_House_*` —
   это `BuildingWithFireplaceNasdara : BuildingWithFireplace : BuildingSuper(House)`
   (`buildingwithfireplace.c:1,19`), т.е. `House`. → попадают в реестр.
2. **Двери считаются**: `GetNearestBuildingWithDoors` фильтрует
   `BuildingBase.Cast(b) && b.GetDoorCount() > 0` (`dmLiveBuildingRegistry.c:190-192`).
   Достроенная дверь — **нативная** дверь здания (`GetDoorCount()` её видит); её физика
   включается `doorConstructionPhysicsSource` при сборке.
3. **Interior-точки**: `dmExplorer` берёт точки интерьера из `dmBuildingInteriorMap`
   (`GetBuildingsForLocation` → точки внутри). Если бочка с винтовкой стоит за достроенной
   дверью, бот получит точку внутри и пойдёт к ней.

**Блокировка**: `dmBotIntent_OpenDoor` вызывает нативный `m_Building.OpenDoor(m_DoorIdx)`
(`dmBotIntent_OpenDoor.c:84`). На запертой (`LockDoor`) двери `OpenDoor` не открывает →
`IsDoorOpened` остаётся `false` → интент выходит по таймауту, дверь остаётся закрытой.

> **Развилка, которую НЕ закрыть статикой** (см. §5): навмеш-поведение. Достроенная дверь —
> нативная door-часть модели с `doorConstructionPhysicsSource`; при сборке она должна
> включиться в pathgraph (нативно, как обычная дверь). Но перестроится ли навмеш при
> **программной/ручной достройке** так же, как при штатном open/close — надо проверить на
> сервере. Если бот идёт СКВОЗЬ закрытую дверь (не видит её как препятствие) — тест
> «запертая дверь блокирует» не сработает так, как задумано.

## 4b. Опционально: force-build мост для dev (НЕ для ручного теста)

Заказчик строит дверь+замок **вручную**, но прошлая заметка предлагала `buildfence`/`codelock`
op'ы поверх `OnPartBuiltServer(null, ...)`. В 1.30 это **сломано/устарело**:

- `BaseBuildingBase.OnPartBuiltServer(notnull Man player, ...)` (`basebuildingbase.c:549`,
  `fence.c:331`) — теперь **`notnull Man`**, `null`-плеер невалиден; это event-хук, а не
  force-build.
- Force-build 1.30 = **`GetConstructionBasic().BuildPartServerEx(null, part_name, -1)`**
  (`constructionbase.c:179`; `Rebuilding` override `rebuilding.c:32`). Именно так делает
  ванильный dev-спавнер `RebuildingSpawner` (`rebuildingspawner.c` `SpawnAndRebuildAllParts`
  и `RebuildAllWoodenPartsFiltered`). `BuildPartServerEx` → `TakeMaterialsServer` (списывает
  материалы, но **не блокирует** постройку при их отсутствии) → `ProcessOnPartBuiltServer`
  (`constructionbase.c:379`) ставит built-состояние.
- Для здания это `Rebuilding` (а не `Construction`): `building.GetConstructionBasic()` возвращает
  `Rebuilding`. Двери строятся по имени части `door<N>_wood`, замок вешается в
  `Att_CodeLock<N>_Wood` (`rebuildingspawner.c` `RebuildAllWoodenPartsFiltered` строит
  `door`-части и ставит `DigitalCodeLock` в `Att_CodeLock%1_Wood`, где %1 = номер двери).

> Если понадобится мост «достроить здание серверно» — это отдельный op по образцу
> `RebuildingSpawner`, но для теста барьера он НЕ нужен (ручной рецепт §3).

## 5. Гипотезы для эмпирической проверки (тест-сервер 1.30 / nasdara)

- `[нужно подтвердить]` **Доступность фичи на нашем сервере.** Rebuildable `Land_House_*`
  и `_Dam2` грузятся в логе, но нужно подтвердить, что на nasdara реально расставлены
  `_Dam2`-здания с достраиваемой дверью (а не только типы загружены). Probe: прогулка
  наблюдателя по карте / `scanbox` на координатах точки из CE-расстановки nasdara.
- `[нужно проверить]` **Ручная достройка двери игроком.** Сработает ли цепочка
  `ActionBuildPart` в проёме `_Dam2`-здания с досками+гвоздями+молотком; станет ли дверь
  нативной (`GetDoorCount()>0`, `IsDoorClosed`). Probe: наблюдатель строит дверь, потом
  `botdump`/клиентский `scanobj` на здание.
- `[нужно проверить]` **Точные имена материалов/частей/слотов.** `door<N>_wood`,
  `Att_CodeLock<N>_Wood`, материалы доски/гвозди — имена из game-data (config), в
  Script-Diff их нет. Probe: наблюдатель в UI стройки читает имена частей и требуемые
  материалы (или `botdump` construction-частей после `spawnobj` `Land_House_*_Dam2`).
- `[нужно проверить]` **Блокирует ли запертая дверь бота.** После достройки+`DigitalCodeLock`
  (PIN задан): (1) `IsDoorLocked(doorIdx)==true`; (2) `OpenDoor` не открывает; (3) бот в
  Exploration (бочка с винтовкой за дверью) не попадает внутрь и не завладевает винтовкой.
  Probe: полный сценарий §3 + `botdump` инвентаря/позиции бота; `assert` «винтовка всё ещё
  в бочке».
- `[нужно проверить]` **Навмеш достроенной двери.** Видит ли `AIWorld.FindPath` бота закрытую
  достроенную дверь как непроходимую (а не идёт сквозь неё). Probe: `moveto` бота в точку за
  дверью → `snapshot` позиции; сравнить «дверь заперта» vs «дверь открыта/недостроена».
- `[нужно подтвердить]` **`DigitalCodeLock` требует батарейку.** `NeedBattery()==true`,
  `Battery9V` должна быть внутри; без энергии замок не работает (`OnWork` → разрядилась →
  `SetLockState(false)`). Probe: повесить замок без батарейки → `HasEnergy()==false`.

## 6. Существенные развилки и решения (для `docs/decisions.md`)

1. **Источник API 1.30 — `DayZ-Script-Diff-130`, не `DayZ Projects`.** Решение: вся
   base-building-1.30 исследована по `DayZ-Script-Diff-130/scripts` (127250); `DayZ Projects`
   = 1.21 — использовать только для исторических сигнатур. Почему: прошлая заметка ошибочно
   заключила «DLC/нет в дампе» именно из-за 1.21-дампа. Действует пока: материализован
   дамп 1.30; `1.30-breaking-changes.md` уже ссылается на него.

2. **Кодовый замок на здании — `DigitalCodeLock` (PIN+батарея), НЕ `CombinationLock`.**
   Решение: тестовая среда — `DigitalCodeLock` в слот `Att_CodeLock*`; `CombinationLock`
   остаётся только для `Fence`. Почему: `BuildingBase` запирает дверь через
   `DigitalCodeLock`→`HandleHierarchyRootLock`→`LockDoor`; `CombinationLock` — дисковый замок
   ворот, к `Building`-дверям не применим (у него нет `SetDoorIndex`/`LockDoor`-пути).
   Действует пока: не появится «кодовый замок старого образца» на дверях зданий (в 1.30 его нет).

3. **Дверь здания после достройки остаётся нативной (`GetDoorCount`) — не отдельный класс.**
   Решение: бот оперирует обычным `Building.OpenDoor`/`IsDoorLocked`; «достроена» проявляется
   только `doorConstructionPhysicsSource` (AnimationSource физики). Никакого отдельного
   «дверь-с-замком» класса у здания нет — замок это `ItemBase` в слоте. Действует пока:
   нативный door-API не изменится.

4. **Тестовый барьер — ЗДАНИЕ (rebuildable `Land_House_*_Dam2`), а не `Fence` в поле.**
   Решение: сценарий ставит достроенную дверь в проёме здания (Exploration ведёт бота внутрь),
   а не забор. `Fence`+`CombinationLock` — фолбэк, если rebuildable-здание окажется недоступным
   на сервере. Действует пока: `[нужно подтвердить]` наличие `_Dam2`-здания на nasdara.
