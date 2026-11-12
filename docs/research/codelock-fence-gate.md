# Research: кодовый замок на воротах (Fence + CombinationLock) — ваниль

> **⚠️ ГЛАВНЫЙ ВЫВОД ЭТОЙ ЗАМЕТКИ ОШИБОЧЕН (исправлено в `restorable-buildings.md`).**
> Заключение «восстанавливаемые здания 1.30 = Badlands DLC, в Script-Diff их нет» сделано по
> **неверному дампу**: `/home/devalio/dayz/Work/DayZ Projects/scripts` = **DayZ 1.21**
> (`version=121737`), а не 1.30. Актуальный 1.30 — `/home/devalio/dayz/Work/DayZ-Script-Diff-130/scripts`
> (`version=127250`), где «восстанавливаемые здания» — **CORE-фича** (`Rebuilding`,
> `ConstructionPartRebuildReversibleDoorTypeData`, `DigitalCodeLock`, `Land_House_*_Dam2`).
> Для задачи «достроить дверь здания + кодовый замок» читай **`restorable-buildings.md`**.
>
> Ниже сохранено как **фолбэк** (забор в поле): механика `Fence`+`CombinationLock` в 1.30
> осталась валидной, но это **вторичный** вариант, не то, что нужно задаче (нужен Building).

---

## (исходный текст, переквалифицирован в фолбэк)

Цель: дать рецепт тестовой среды «запертая комната с бочкой+винтовкой внутри» для E2E
«бот НЕ может забрать вещь из-за барьера». План задачи предполагал «восстанавливаемое
здание (1.30)» → достроить дверь → повесить кодовый замок. Надёжный ванильный эквивалент
**без здания** — **`Fence` (ворота) + `CombinationLock`** (актуален как фолбэк, если
rebuildable-здание окажется недоступно на сервере).

---

## 0. Главный вывод (короткий)

- ~~«Восстанавливаемые здания» 1.30 — часть Badlands DLC~~ — **НЕВЕРНО** (см. `restorable-buildings.md`):
  это CORE 1.30, а «нет в дампе» было из-за 1.21-дампа. Оставлен только фолбэк ниже.
- **Кодовый замок ≠ дверь `Building`.** `Fence : ItemBase : BaseBuildingBase` — это НЕ
  `Building`. Весь door-API из `doors-locks.md` / `camping-doors.md` (`IsDoorLocked`,
  `OpenDoor`, `GetLockCompatibilityType`) тут неприменим. `dmBotIntent_OpenDoor` (типизирован
  `Building`) и `dmLiveBuildingRegistry` (регистрирует только `Building`/`House`) **не видят** Fence.
- Механика замка: `CombinationLock` — обычный `ItemBase`, вешается в слот
  `Att_CombinationLock` ворот; «заперто» = `GetInventory().SetSlotLock(slot, true)` на воротах +
  внутренний `m_IsLocked = (m_Combination != m_CombinationLocked)` у замка. «Открыть/закрыть
  ворота» — `OpenFence()`/`CloseFence()` (анимация поворота створки), а НЕ `Building.OpenDoor`.

---

## 1. Классы и иерархия

| Класс | Файл | Родитель | Роль |
|---|---|---|---|
| `BaseBuildingBase` | `4_world/entities/itembase/basebuildingbase.c:2` | `ItemBase` | базовый объект стройки; держит `Construction` |
| `Construction` | `4_world/classes/basebuilding/construction.c` | — (ref-член) | список `ConstructionPart` + build/dismantle/destroy |
| `ConstructionPart` | `4_world/classes/basebuilding/constructionpart.c` | — | одна «деталь» (стена/ворота/основание), `IsBuilt()`, `IsGate()`, `IsBase()` |
| `Fence` | `4_world/entities/itembase/basebuildingbase/fence.c:1` | `BaseBuildingBase` | стена с воротами (gate) — наш «запираемый дверной проём» |
| `Watchtower`/`ShelterSite`/`TerritoryFlag`/`StaticFlagPole` | basebuildingbase/* | `BaseBuildingBase` | прочие стройки (не нужны) |
| `CombinationLock` | `4_world/entities/itembase/combinationlock.c:12` | `ItemBase` | 3-значный кодовый замок |
| `CombinationLock4` | `4_world/entities/itembase/combinationlock4.c` | `CombinationLock` | 4-значный замок |
| `Barrel_ColorBase` | `4_world/entities/itembase/barrel_colorbase.c` | `DeployableContainer_Base` | бочка-контейнер (`Barrel_Green/Blue/Red/Yellow`) |

---

## 2. Механика строительства ворот (проёма)

«Дверь» = полностью достроенные **ворота** (`wall_gate` + его required-части). Детали читаются
из config (`cfgVehicles <тип> Construction ...`), имена частей в config (в скриптах только
хардкод `wall_base_down`/`wall_base_up`, `construction.c:163,428`, `fence.c:349,354,755-756,793-794`).

Ключевые сигнатуры:

```c
// construction.c
ConstructionPart GetConstructionPart(string part_name)          // construction.c:280
ConstructionPart GetGateConstructionPart()                       // construction.c:395 — часть с is_gate
void BuildPartServer(notnull Man player, string part_name, int action_id)  // construction.c:75 — С ПРОВЕРКОЙ материалов

// basebuildingbase.c
void OnPartBuiltServer(notnull Man player, string part_name, int action_id) // basebuildingbase.c:587 — БЕЗ проверки материалов (force-build)
void FullyBuild()                                                 // basebuildingbase.c:1270 — build всех частей, НО с исключениями OnDebugSpawnBuildExcludes()
void UpdateNavmesh()                                              // basebuildingbase.c:905 — SetAffectPathgraph + UpdatePathgraphRegionByObject

// fence.c
bool HasFullyConstructedGate()                                    // fence.c:61 (m_GateState == GATE_STATE_FULL)
int  CheckGateState()                                             // fence.c:72
void CloseFence()                                                 // fence.c:489 — закрыть створку (поворот на 0°)
void OpenFence()                                                  // fence.c:447 — открыть створку (поворот на 100°)
bool CanOpenFence()  // fence.c:427  HasHinges && !IsOpened && !IsLocked
bool CanCloseFence() // fence.c:437  HasHinges && IsOpened
```

> **Готча `FullyBuild()`:** он вызывает `OnDebugSpawnBuildExcludes()` (для Fence — `fence.c:868-895`),
> который в обычной (не `DIAG_DEVELOPER`) сборке **исключает `gate` и `_wood_`**. Т.е. голый
> `FullyBuild()` строит ТОЛЬКО металл-части и **не строит ворота**. Для «достроить дверь» нужен
> собственный цикл по всем частям **без** исключений (см. §4 рецепт).

> **Разница `BuildPartServer` vs `OnPartBuiltServer`:** `BuildPartServer` (construction.c:75)
> проходит `TakeMaterialsServer` (списывает доски/гвозди из слотов `Materials`), `OnPartBuiltServer`
> (basebuildingbase.c:587) — чистый force-build без материалов (им пользуется debug-spawn). Для
> E2E-моста «достроить дверь без участия игрока» используем `OnPartBuiltServer`.

---

## 3. Механика кодового замка

`CombinationLock` (`combinationlock.c`):

```c
void SetCombination(int combination)          // :181 — задать набранную комбинацию (0..999/9999)
int  GetCombination()                         // :189
int  GetLockDigits()                          // :195 (3 или 4)
void LockServer(EntityAI parent, bool ignore_combination = false)  // :272
void UnlockServer(EntityAI player, EntityAI parent)                // :299
bool IsLocked()                               // :365 = m_IsLocked
void CheckLockedStateServer()                 // :370 → m_IsLocked = (m_Combination != m_CombinationLocked)
bool IsLockedOnGate()                         // :375 — замок на воротах И заперт
bool IsLockAttached()                         // :389 — замок висит на Fence
void DialNextNumber()                         // :202 — +1 к текущему диску (как игрок крутит)
void ShuffleLock()                            // :342 — перемешать набранную комбинацию
override void OnItemLocationChanged(old, new) // :75 — авто-LockServer(new_owner) при прицепке к BaseBuildingBase
```

**Как «повесить + закрыть кодом» (смысл `LockServer`, :272-293):**
1. `SetCombinationLocked(m_Combination)` — запоминает текущую набранную комбинацию как КОД замка.
2. `parent.GetInventory().SetSlotLock(slot, true)` — запирает слот `Att_CombinationLock` на воротах
   (замок нельзя снять).
3. `ShuffleLock()` — перемешивает набранную комбинацию (визуальный диск уходит в случайное).
4. `SetTakeable(false)`, `CheckLockedStateServer()` (→ `m_IsLocked = true`, т.к. диск ≠ код).

**Повесить программно** — как делает `ActionAttachToConstruction` (singleuse/actionattachtoconstruction.c):
`BaseBuildingBase` + слот `Att_CombinationLock`; гейт `Fence.CanReceiveAttachment` (`fence.c:367-396`)
требует `HasFullyConstructedGate() && !IsOpened()`. При **перемещении** готового замка в слот
`OnItemLocationChanged` сам зовёт `LockServer` (авто-закрытие текущим кодом).

**Снять/открыть** (`UnlockServer`, :299): `SetSlotLock(slot, false)` + дроп замка на землю
(`ServerDropEntity`/`DropEntity`) + `SetTakeable(true)`. Открытие ворот делается **отдельно**
`OpenFence()` (замок снят → `CanOpenFence()`).

> Нюанс 1.30 (из сети, low.ms): у модов «base building» в 1.30 сыпалось
> `Undefined function 'CombinationLock.UnlockServer'` — у части модов сигнатура вызывалась
> по-старому. В нашем дампе 1.30 сигнатура `UnlockServer(EntityAI player, EntityAI parent)` жива
> (combinationlock.c:299), `UnlockOnServer` (Expansion) — отдельный метод, в ванили его нет.

---

## 4. Программный рецепт (серверный скрипт / E2E-мост)

Всё делается серверно, **без живого игрока** (см. также §6 по мосту):

```c
// 1. Ворота в проёме (вместо «достроить дверь»).
Fence fence = Fence.Cast(GetGame().CreateObject("Fence", pos, false));   // деплоится как kit-состояние
fence.SetOrientation(Vector(yaw, 0, 0));

// 2. Достроить ВСЕ части включая ворота (не FullyBuild() — тот исключает gate/_wood_).
array<ConstructionPart> parts = fence.GetConstruction().GetConstructionParts().GetValueArray();
foreach (ConstructionPart part : parts)
    fence.OnPartBuiltServer(null, part.GetPartName(), AT_BUILD_PART);   // AT_BUILD_PART = 193 (_constants.c:6)

// 3. Закрыть створку (поворот на 0°; навмеш перестраивается CallLater ~2с).
fence.CloseFence();   // fence.c:489

// 4. Замок в слот + код.
int slot = InventorySlots.GetSlotIdFromString("Att_CombinationLock");
CombinationLock lock = CombinationLock.Cast(fence.GetInventory().CreateAttachmentEx("CombinationLock", slot));
lock.SetCombination(4321);     // код ДО запирания
lock.LockServer(fence);        // combinationlock.c:272 — запирает слот + shuffle + m_IsLocked=true

// 5. (опционально) проверить.
fence.IsLocked();              // fence.c:115 = lock.IsLocked()  → true
lock.IsLockedOnGate();         // combinationlock.c:375 → true
```

**Альтернатива п.4** (если `CreateAttachmentEx` не сработает в рантайме — см. §7): создать замок на
земле (`CreateObjectEx("CombinationLock", pos, ECE_PLACE_ON_SURFACE)`), `SetCombination(4321)`, затем
`fence.GetInventory().TakeEntityAsAttachmentEx(lock, slot)` → `OnItemLocationChanged` авто-закроет кодом.
(Осторожно: для свежего `CreateObject`-предмета `TakeToDst`-перемещение может не найти
`InventoryLocation` — тот же класс готчи, что в скилле про выдачу предметов боту.)

**Бочка с винтовкой** (уже частично покрыто мостом `fillcargo`):
```c
Barrel_ColorBase barrel = Barrel_ColorBase.Cast(GetGame().CreateObject("Barrel_Green", pos, false));
barrel.Open();                                   // barrel_colorbase.c — CanReceiveItemIntoCargo требует IsOpen
Weapon_Base rifle = Weapon_Base.Cast(barrel.GetInventory().CreateInInventory("Mosin9130"));
barrel.Close();                                  // закрыть (lid)
```

> Порядок п.2 важен: `CanReceiveAttachment` для замка требует `HasFullyConstructedGate()`, поэтому
> сначала достроить ворота, потом замок. `CloseFence()` можно и до замка, и после — гейт замка только
> `!IsOpened()`, а закрытие створки при запертом замке всё равно разрешено (замок висит, створка
> закрыта).

---

## 5. Что уже есть в боте (переиспользование)

- **`dmBotIntent_OpenDoor`** (`src/core/4_World/Entities/Bot/Intent/dmBotIntent_OpenDoor.c`) — типизирован
  `Building`; работает только с `Building.OpenDoor(doorIdx)`. **Не работает для Fence** (не `Building`).
- **`dmLiveBuildingRegistry`** (`src/reg/4_World/dmLiveBuildingRegistry.c`) — регистрирует только
  `Building`/`House` (`Building.Cast`), Fence туда не попадает. `GetNearestBuildingWithDoors` фильтрует
  по `BuildingBase.GetDoorCount()`.
- **`docs/research/doors-locks.md`** — ключ/замок на `Building`/`ContainerLockedBase` (битмаска
  `EBuildingLockType`). Не покрывает кодовый замок на воротах (другой домен).
- **E2E-мост** (`src/test/4_World/dmE2EBridge.c`):
  - `spawnobj` (:2934) — спавнит произвольный CfgVehicles объект в `m_Objects` (можно `Fence`, `Barrel_Green`).
  - `fillcargo` (:2282) — заполняет карго наземного контейнера (`Barrel_Green`), с `variant=="close"`
    закрывает обратно. **Уже покрывает «бочка+винтовка, закрыта».**
  - `raycast`/`scanbox`/`botdump` — probe-ручки.
  - `clearobj` — удаляет probe-объекты.

**Пробел:** нет op'а «достроить ворота», «повесить+закодить замок», «закрыть створку». Это и есть
то, что нужно доделать девелоперу (§6).

---

## 6. E2E-мосты, нужные девелоперу

Один новый op `lockdoor` (или два-три атомарных) поверх существующего `dmE2EBridge`:

| Op | Поля | Что делает |
|---|---|---|
| `spawnobj` (есть) | `Who, ClassName="Fence", Pos, Yaw` | спавн ворот в `m_Objects` |
| **`buildfence`** (новый) | `Who` (имя в m_Objects), опц. `Variant="gate"` | цикл по `GetConstruction().GetConstructionParts()` → `OnPartBuiltServer(null, partName, AT_BUILD_PART)`; возвращает число частей |
| **`codelock`** (новый) | `Who` (ворота), `Code` (int), опц. `Variant="close"` | `CloseFence()` + `CreateAttachmentEx("CombinationLock", Att_CombinationLock)` + `SetCombination(Code)` + `LockServer(fence)`; dump: `fence.IsLocked()`/`lock.IsLockedOnGate()` |
| `fillcargo` (есть) | `Who="barrel", ClassName="Mosin9130", Variant="close"` | бочка+винтовка закрыта |
| `assert` (есть) | `Cond=...` | проверить `fence.IsLocked()==true`, бот НЕ завладел винтовкой |

**Полный сценарий (псевдо-E2E):**
```
spawnobj   Who=gate  ClassName=Fence        Pos=<проём, Y=0> Yaw=<в проём>
buildfence Who=gate  Variant=gate
codelock   Who=gate  Code=4321              Variant=close
spawnobj   Who=barrel ClassName=Barrel_Green Pos=<внутри комнаты, Y=0>
fillcargo  Who=barrel ClassName=Mosin9130   Variant=close
spawn      Who=bot  ClassName=dmAI_SurvivorM_Denis Pos=<снаружи, Y=0>
moveto     Who=bot  Pos=<к бочке>           # бот пытается дойти до винтовки
wait       …
assert     Cond=barrier                      # бот НЕ пересек ворота / не завладел винтовкой
clearobj / clearall
```

Что нужно от девелопера: `buildfence` + `codelock` op'ы в `dmE2EBridge.c` (нативные вызовы из §4,
без участия игрока) + `assert`-предикат «бот не владеет винтовкой / винтовка всё ещё в бочке»
(читается `barrel.GetInventory().GetCargo()`).

---

## 7. Гипотезы для эмпирической проверки (тест-сервер)

- `[нужно проверить]` **`CreateAttachmentEx("CombinationLock", slot)` на воротах AI_SERVER** — реально
  ли создаёт замок в слоте `Att_CombinationLock` и отрабатывает ли `CanReceiveAttachment`-гейт
  (`HasFullyConstructedGate() && !IsOpened()`). Фолбэк — create-on-ground + `TakeEntityAsAttachmentEx`.
  Probe: `spawnobj Fence` → `buildfence` → `codelock` → `botdump` слота.
- `[нужно проверить]` **Блокирует ли закрытые+запертые ворота навмеш бота.** `CloseFence()`/build
  зовут `UpdateNavmesh()` (`SetAffectPathgraph(true,false)` + `UpdatePathgraphRegionByObject`,
  basebuildingbase.c:905), но ворота — это `ItemBase` с proxy-physics, а не `Building`. Вопрос: видит ли
  `AIWorld.FindPath` (бот) закрытую створку как непроходимую, или бот «проходит сквозь» визуально
  закрытые ворота. Probe: `moveto` бота в точку за воротами → `snapshot`/позиция; сравнить с открытыми.
- `[нужно проверить]` **Видит ли бот винтовку в бочке за стеной** (perception LOS через створку/стену).
  Если нет — тест «бот не забрал» пройдёт тривиально (бот даже не попытается), и нужен отдельный
  сценарий «видит, но не может дойти». Probe: `botdump` perception / `scanobj` на клиенте.
- `[нужно подтвердить]` **Имена construction-частей ворот** (`wall_gate`, `wall_base_down/up`,
  `wall_wood_down/up`, `wall_metal_down/up`) — берутся из config `CfgVehicles Fence Construction`,
  которого нет в скрипт-дампе. Прогон `buildfence` (цикл по `GetConstructionParts()`, без хардкода имён)
  снимает этот вопрос.
- `[нужно подтвердить]` **`CreateObject("Fence", pos)`** даёт именно deployable-объект с работающим
  `GetConstruction()` (а не голый kit), и `OnPartBuiltServer(null, ...)` с `player=null` не падает на
  dedicated server (в `FullyBuild` так и делается, basebuildingbase.c:1297-1304).

---

## 8. Существенные развилки и решения (для `docs/decisions.md`)

1. ~~Тестовый барьер — `Fence`+`CombinationLock`, а не «восстанавливаемое здание» 1.30~~ — **НЕВЕРНО
   (исправлено).** Решение пересмотрено в `restorable-buildings.md`: барьер — это **здание**
   (`Land_House_*_Dam2`) + достроенная дверь + `DigitalCodeLock`, а `Fence`+`CombinationLock` — только фолбэк.
   Почему исходного (ошибочного) выбора: исследование велось по дампу 1.21, где фичи нет. Действует пока:
   rebuildable-здание доступно на сервере (тогда Fence не нужен).

2. **Строить ворота через `OnPartBuiltServer`, а не `BuildPartServer`/`FullyBuild`.**
   Решение: E2E-мост зовёт `OnPartBuiltServer` (force-build) циклом по всем частям, минуя
   `FullyBuild()` (исключает gate/_wood_) и `BuildPartServer` (требует материалов). Почему: тест не должен
   зависеть от расхода досок/гвоздей; `FullyBuild` с исключениями не строит ворота. Действует пока: не
   нужен тест «постройка с реальным расходом материалов».

3. **Кодовый замок — отдельный домен от `doors-locks.md`.** Решение: новый research-файл, а не расширение
   `doors-locks.md`. Почему: там «замок под ключ» = `Building.IsDoorLocked` + битмаска `EBuildingLockType`;
   тут — `BaseBuildingBase`+слот-лок (`SetSlotLock`) + состояние `CombinationLock`. Разные предикаты, разные
   классы, разный bot-детект. Действует пока: бот не пытается открывать Fence-ворота (иначе нужен новый
   интент `OpenGate`/`UnlockCodeLock`, аналог `dmBotIntent_OpenDoor`).
