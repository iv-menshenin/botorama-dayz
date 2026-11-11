# Research: замок под цветной ключ (дверь/контейнер) — ваниль

Цель: понять механизм «дверь/контейнер закрыта на ключ» (не просто `IsDoorLocked`) — как
детектить тип, как сопоставлять ключ, какое действие открывает, и что делать боту без
action-manager. Контекст: бот `dmAISurvivorBase` пытается проломиться в здание/контейнер,
закрытый цветным ключом (Sakhal-контейнеры).

База door-API уже покрыта в `camping-doors.md` §1 и `door-press.md` §3 — здесь только
добавки про **замок/ключ**.

---

## 0. Главный вывод (короткий)

- «Ключ» ≠ отдельный предикат. Это **битмаска совместимости**: `Building.GetLockCompatibilityType(doorIdx)`
  против `ToolBase.GetKeyCompatibilityType()`. Совпадение = `(doorMask & keyMask) != 0`.
- Натив `UnlockDoor(index)` **НЕ гейтится ключом** — это голый сеттер состояния. Проверка
  «есть ли ключ» живёт только в player-action (`ActionUnlockDoors.ActionCondition`).
  Для бота проверку ключа делаем **сами** (сравниваем маски), а затем вызываем
  `UnlockDoor` → `OpenDoor` напрямую. `SetLocked(false)` в скриптах НЕ существует.
- `land_warheadstorage_*` — **НЕ под ключ** (красная селёдка): это Sakhal-бункер с
  питанием/рычагами, двери гоняются `OpenDoor`/`CloseDoor` по power-state, а не `LockDoor`.
- Замок под ключ — только `Land_ContainerLocked_{Blue,Yellow,Orange,Red}_DE`.

---

## 1. Типы объектов и детект

### 1a. Классы «контейнер под ключ»

`4_world/entities/itembase/containerlocked.c`:

```c
class ContainerLockedBase : BuildingSuper            // containerlocked.c:1
{
    protected int m_LockedMask = 0;                  // containerlocked.c:3
    protected int m_OpeningMask = 0;

    void ContainerLockedBase()
    {
        int count = GetDoorCount();
        for (int i = 0; i < count; i++)
            LockDoor(i);                             // containerlocked.c:11 — все двери заперты при спавне
    }
    ...
    override void OnDoorLocked(DoorLockParams params)  // containerlocked.c:67
    {
        super.OnDoorLocked(params);
        m_LockedMask |= 1 << params.param1;          // containerlocked.c:71
        string selectionName = string.Format("side%1_lock", (params.param1 + 1));
        SetAnimationPhase(selectionName, 0);         // видимый замок
    }
    override void OnDoorUnlocked(DoorLockParams params) // containerlocked.c:78
    {
        super.OnDoorUnlocked(params);
        int doorIdx = params.param1;
        m_LockedMask &= ~(1 << doorIdx);             // containerlocked.c:84
        SetAnimationPhase(string.Format("side%1_lock", (doorIdx + 1)), 1);
        if (!g_Game.IsDedicatedServer())
            SEffectManager.PlaySoundEnviroment("Land_ContainerLocked_lock_SoundSet", GetDoorSoundPos(doorIdx));
    }
}

class Land_ContainerLocked_Blue_DE : ContainerLockedBase   { ... GetLockCompatibilityType → 1<<SHIP_CONTAINER_0 } // :108
class Land_ContainerLocked_Yellow_DE : ContainerLockedBase { ... SHIP_CONTAINER_1 }                                // :116
class Land_ContainerLocked_Orange_DE : ContainerLockedBase { ... SHIP_CONTAINER_2 }                                // :124
class Land_ContainerLocked_Red_DE : ContainerLockedBase    { ... SHIP_CONTAINER_3 }                                // :132
```

Иерархия (`4_world/entities/game/super/building.c`):
`ContainerLockedBase : BuildingSuper`, где `typedef House BuildingSuper` (:92),
`House : BuildingBase : Building` (:1, :85). **Значит `ContainerLockedBase` — это `Building`**
(кастится `Building.Cast(obj)`), и весь door-API `building.c` применим.

> Существенный нюанс: сам `ContainerLockedBase` **не** переопределяет
> `GetLockCompatibilityType` — переопределяют только 4 цветных подкласса. Если бы
> где-то спавнился голый `ContainerLockedBase`, его маска была бы `1 << LOCKPICK`
> (дефолт `Building`, см. §2). В игре спавнятся только цветные.

### 1b. Нативные предикаты замка

`3_game/entities/building.c`:

| Сигнатура | строка | смысл |
|---|---|---|
| `proto native bool IsDoorLocked(int index)` | :44 | дверь ЗАПЕРТА (факт) |
| `proto native void LockDoor(int index, bool force = false)` | :56 | запереть; `force=true` ещё и закрыть открытую; **сбрасывает door health** (см. §3) |
| `proto native void UnlockDoor(int index, bool animate = true)` | :59 | отпереть; `animate=false` — без ajar-анимации |
| `proto native void OpenDoor(int index)` | :50 | открыть (на запертой — НЕ открывает, «ручка дребезжит», см. §3) |
| `bool CanDoorBeOpened(int doorIndex, bool checkIfLocked = false)` | :130 | `true` → «закрыта И не заперта»; `false` → «закрыта И заперта» (awkward, см. door-press.md) |
| `bool CanDoorBeLocked(int doorIndex)` | :154 | `!IsDoorOpen && !IsDoorLocked` |
| `int GetLockCompatibilityType(int doorIdx)` | :167 | битмаска «какие ключи подходят» (см. §2) |

**Предикатов `GetDoorLock` / `IsLocked` / `HasKeyLock` в скриптах НЕТ.** Есть только
`IsDoorLocked`. `IsDoorLocked` ≠ `IsDoorOpen` (`:44` vs `:23`): `IsDoorOpen` = «запрошено
полностью открыть» (wanted phase > 0.5), а `IsDoorLocked` = факт состояния замка. Дверь
может быть открыта и заперта одновременно (теоретически), но контейнер запирается закрытым
в конструкторе.

### 1c. «Замок под ключ» vs «запираемое ломиком»

Детект строится через **маску совместимости**, а не через имя класса:

- Дефолт `Building.GetLockCompatibilityType(doorIdx)` возвращает
  `1 << EBuildingLockType.LOCKPICK` (building.c:167-170) — «любая дверь вскрывается ломиком».
- Контейнерные подклассы возвращают `1 << EBuildingLockType.SHIP_CONTAINER_N` (без бита LOCKPICK).

Поэтому:
- «дверь под ЦВЕТНОЙ ключ» = `IsDoorLocked(i)` И `(GetLockCompatibilityType(i) & (1<<LOCKPICK)) == 0`.
- «запираемая, но ломиком» = `IsDoorLocked(i)` И бит LOCKPICK установлен.

Это ровно логика Expansion `eAIBase.HandleBuildingDoors` (см. §3): `canHitToUnlock` путь
доступен только если `GetLockCompatibilityType & (1 << LOCKPICK)`.

### 1d. `land_warheadstorage_*` — НЕ под ключ (красная селёдка)

`4_world/entities/building/underground/land_warheadstorage_main.c:1` —
`class Land_WarheadStorage_Main : House` (Sakhal-бункер). Двери управляются **питанием и
рычагами**, не замком:

- `UpdateDoorStateServer()` (`:272-289`) — `OpenDoor`/`CloseDoor` по `(bit & m_LeverStatesBits) && m_HasPower`.
- `override bool CanDoorBeOpened(...)` (`:356-372`) — для storage-дверей (idx 8-11) возвращает `false`
  (открываются только питанием), bunker-двери 4-7 — по сотоянию соседних.
- `override bool CanDoorBeLocked(...)` (`:374-379`) — `false` для bunker/storage дверей
  (`IsStorageDoor`/`IsBunkerDoor`, `:380-391`).
- `LockDoor`/`UnlockDoor`/`GetLockCompatibilityType` тут **не используются вовсе** (grep пуст).

Итого: если задача «контейнер под ключ» — это НЕ warheadstorage. Это `Land_ContainerLocked_*_DE`.

---

## 2. Сопоставление ключа

### 2a. Атрибут сопоставления — битмаска `EBuildingLockType`

`3_game/enums/ebuildinglocktypes.c`:

```c
enum EBuildingLockType
{
    NONE = 0,              // :3 — «открыть только скриптом», не сдвигается
    LOCKPICK,              // :4
    SHIP_CONTAINER_0,      // :5
    SHIP_CONTAINER_1,      // :6
    SHIP_CONTAINER_2,      // :7
    SHIP_CONTAINER_3,      // :8
}
```

Обе стороны возвращают **битмаску** `1 << EBuildingLockType.X`:

- **Дверь**: `Building.GetLockCompatibilityType(int doorIdx)` (building.c:167). Дока
  («Which door is compatible with which key … Bitwise») прямо говорит комбинировать
  `(1 << LOCKPICK) | (1 << SHIP_CONTAINER_1)`.
- **Ключ**: `ToolBase.GetKeyCompatibilityType()` (toolbase.c:12).

**Совпадение**: `(doorMask & keyMask) != 0`.

Замечание про сдвиг: enum-значения идут 0..5, а бит = `1 << value` — т.е. `LOCKPICK` (=1)
даёт бит 1 (значение 2), `SHIP_CONTAINER_0` (=2) — бит 2 (значение 4), и т.д. Точные числа
не важны, если обе стороны используют один и тот же паттерн `1 << EBuildingLockType.X`.

### 2b. Классы ключей

`4_world/entities/itembase/shippingcontainerkeys_colorbase.c`:

```c
class ShippingContainerKeys_ColorBase : ToolBase      // :1
{
    void DestroyKeyServer() { SetHealth("","",0); StartItemSoundServer(...); } // :11
    override void SetActions() { super.SetActions(); AddAction(ActionUnlockShippingContainer); } // :18-22
};
class ShippingContainerKeys_Blue   : ShippingContainerKeys_ColorBase { GetKeyCompatibilityType → 1<<SHIP_CONTAINER_0 } // :25
class ShippingContainerKeys_Yellow : ... { SHIP_CONTAINER_1 }  // :33
class ShippingContainerKeys_Orange : ... { SHIP_CONTAINER_2 }  // :41
class ShippingContainerKeys_Red    : ... { SHIP_CONTAINER_3 }  // :49
```

Плюс ломик: `class Lockpick : ToolBase` (gear/tools/lockpick.c:1), `GetKeyCompatibilityType`
→ `1 << EBuildingLockType.LOCKPICK` (:8-11).

База `ToolBase.GetKeyCompatibilityType()` (toolbase.c:12-18) возвращает
`EBuildingLockType.NONE` (=0, «pure '0'») — т.е. обычный инструмент **не** подходит ни к
одному замку.

### 2c. Проверка «у бота есть подходящий ключ»

Ключ — обычный предмет `ToolBase : ItemBase`. Перечислить инвентарь (см.
`inventory-enumeration.md`):

```c
// inventory.c:127: proto native bool EnumerateInventory(InventoryTraversalType tt, out array<EntityAI> items)
// gameplay.c:5-10: enum InventoryTraversalType { PREORDER, INORDER, POSTORDER, LEVELORDER }
array<EntityAI> items = {};
pawn.GetInventory().EnumerateInventory(InventoryTraversalType.LEVELORDER, items);

bool HasMatchingKey(Building b, int doorIdx)
{
    int doorMask = b.GetLockCompatibilityType(doorIdx);
    ToolBase tool;
    foreach (EntityAI e : items)
        if (Class.CastTo(tool, e) && (tool.GetKeyCompatibilityType() & doorMask) != 0)
            return true;
    return false;
}
```

Дополнительно проверить руки: `EnumerateInventory` НЕ возвращает предмет в руках
(`inventory-enumeration.md` §«HANDS») — если бот может держать ключ в руке, глянуть
`pawn.GetHumanInventory().GetEntityInHands()`.

> Замечание: `ShippingContainerKeys_ColorBase` — `ToolBase`, а не отдельный базовый класс
> «KeyBase»; единый признак «это ключ» — `GetKeyCompatibilityType() != 0`.

---

## 3. Ванильное действие открытия ключом

Все действия — `ActionContinuousBase` (для player action-manager). Само открытие — нативные
вызовы, которые они делают **server-side**; их можно воспроизвести без action-manager.

### 3a. `ActionUnlockDoors` (общий: ломик / любой ключ)

`4_world/classes/useractionscomponent/actions/continuous/actionunlockdoors.c`:

```c
class ActionUnlockDoors: ActionContinuousBase            // :9
{
    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)  // :28
    {
        if (!IsBuilding(target)) return false;
        if (!IsInReach(player, target, UAMaxDistances.DEFAULT)) return false;
        Building building; ToolBase tool;
        if (Class.CastTo(building, target.GetObject()) && Class.CastTo(tool, item))
        {
            int doorIndex = building.GetDoorIndex(target.GetComponentIndex());   // :37
            if (doorIndex != -1 && tool.GetKeyCompatibilityType() & building.GetLockCompatibilityType(doorIndex))  // :38
                return building.IsDoorLocked(doorIndex);                         // :39
        }
        return false;
    }

    override void OnFinishProgressServer(ActionData action_data)   // :58
    {
        UnlockDoor(action_data.m_Target);                          // :60 — см. protected ниже
        MiscGameplayFunctions.DealAbsoluteDmg(action_data.m_MainItem, APPLIED_DMG); // :63 — ломик тратит 6 ед.
    }

    protected void UnlockDoor(ActionTarget target)                // :44
    {
        Building building;
        if (Class.CastTo(building, target.GetObject()))
        {
            int doorIndex = building.GetDoorIndex(target.GetComponentIndex());
            if (doorIndex != -1)
                building.UnlockDoor(doorIndex);                    // :53 — animate=true (ajar)
        }
    }
}
```

**Итог действия** (server): `building.UnlockDoor(doorIndex)` + порча ключа-инструмента.

### 3b. `ActionUnlockShippingContainer` (контейнер под цветной ключ)

`4_world/classes/useractionscomponent/actions/continuous/actionunlockcontainerdoor.c`
(файл называется `actionunlockcontainerdoor.c`, класс — `ActionUnlockShippingContainer`):

```c
class ActionUnlockShippingContainer: ActionUnlockDoors        // :1
{
    // ! ВАЖНО: «неправильный ключ» НЕ отсекается на уровне Can — попытка разрешена
    override bool ActionCondition(...)                         // :4
    {
        ContainerLockedBase shipCont;
        if (Class.CastTo(shipCont, target.GetObject()))
        {
            int doorIndex = TranslateLockSelectionIntoDoorIdx(target);  // :9
            if (doorIndex != -1)
                return shipCont.IsDoorLocked(doorIndex);       // :11 — ТОЛЬКО «заперта», без проверки ключа
        }
        return false;
    }

    override void OnFinishProgressServer(ActionData action_data) // :16
    {
        ShippingContainerKeys_ColorBase key = ShippingContainerKeys_ColorBase.Cast(action_data.m_MainItem);
        ContainerLockedBase shipCont = ContainerLockedBase.Cast(action_data.m_Target.GetObject());
        if (shipCont && key &&
            ((shipCont.GetLockCompatibilityType(shipCont.GetDoorIndex(action_data.m_Target.GetComponentIndex()))
              & key.GetKeyCompatibilityType()) == 0))          // :20 — НЕ тот ключ
        {
            key.DestroyKeyServer();                            // :22 — ключ ЛОМАЕТСЯ
        }
        else
        {
            UnlockDoor(action_data.m_Target);                  // :26 — тот ключ
            MiscGameplayFunctions.DealAbsoluteDmg(action_data.m_MainItem, APPLIED_DMG); // :27
        }
    }

    override protected void UnlockDoor(ActionTarget target)     // :31
    {
        Building building;
        if (Class.CastTo(building, target.GetObject()))
        {
            int doorIndex = TranslateLockSelectionIntoDoorIdx(target); // :36
            if (doorIndex != -1)
                building.UnlockDoor(doorIndex, false);         // :39 — animate=false (без ajar)
        }
    }

    // side1_lock → 0, side2_lock → 1 ...  (selection name на видимом замке)
    protected int TranslateLockSelectionIntoDoorIdx(ActionTarget target)  // :45
    {
        ContainerLockedBase shipCont;
        if (Class.CastTo(shipCont, target.GetObject()))
        {
            string selectionName = shipCont.GetActionComponentName(target.GetComponentIndex()); // :51
            if (selectionName.Contains("_lock"))
                return (selectionName.Substring(4, 1).ToInt() - 1);       // :53
        }
        return -1;
    }
}
```

**Ключевые факты для бота:**
1. Проверка «правильный ли ключ» — чистое сравнение масок (`doorMask & keyMask`). У бота
   нет `ActionTarget`/`GetComponentIndex`, поэтому «какая дверь» — через `GetDoorIndex` по
   райкасту (как уже делает `TryOpenDoorOnPath`, door-press.md) или через
   `GetLockCompatibilityType(doorIdx)` по перебору `GetDoorCount()`.
2. Открытие контейнера делает `UnlockDoor(doorIdx, false)` — **без ajar**, т.е. дверь
   остаётся закрытой и её надо **отдельно** открыть (`OpenDoor(doorIdx)`), как делает игрок
   вторым действием. Для бота: `UnlockDoor(doorIdx, false)` → `OpenDoor(doorIdx)`.
3. `ActionUnlockDoors` (ломик) использует `UnlockDoor(doorIdx)` с `animate=true` → дверь
   приоткрывается (ajar). Разница только в анимации, на конечное состояние не влияет.

### 3c. Как это делает Expansion AI (референс для locked-дверей)

`AI/.../Entities/AI/eAIBase.c` `HandleBuildingDoors`:

```c
// :11637 — дверь ЗАПЕРТА
else if (building.IsDoorLocked(doorIndex) && building.CanDoorBeOpened(doorIndex, false))
{
    bool canHitToUnlock = building.GetAllowDamage();            // :11639
    if (canHitToUnlock && (building.GetLockCompatibilityType(doorIndex) & (1 << EBuildingLockType.LOCKPICK)))  // :11646
    {
        // «выстрелить в дверь, чтобы сбить замок» — eAIDoorTargetInformation.AddAI(this)
    }
    else
    {
        // ! дверь под ключ НЕ открыть → пометить путь недостижимым
        if (m_PathFinding.m_IsBlocked)
        {
            m_PathFinding.m_IsUnreachable = true;               // :11654
            m_PathFinding.m_IsTargetUnreachable = true;
        }
        canInteract = false;                                    // :11663
    }
}
...
// :11719-11723 — собственно открытие
else
{
    StartActionObject(eAIActionOpenDoorsDummy, building);
    //! if the door cannot be opened because it is locked,
    //! the game will prevent opening and play the usual "doorknob rattle" sound
    if (canInteract || building.IsDoorLocked(doorIndex))
        building.OpenDoor(doorIndex);                           // :11723
}
```

Выводы:
- Expansion **НЕ открывает** двери под цветной ключ (у него нет инвентаря/ключей в этом
  коде) — он их трактует как «недостижимо» (`canInteract=false`).
- Дверь под **ломик** (бит LOCKPICK) Expansion «сбивает выстрелом» (если `GetAllowDamage()`),
  target `eAIDoorTargetInformation` (`Classes/Targets/eAIDoorTargetInformation.c`) — в
  `IsActive()` требует `IsDoorLocked` И `GetLockCompatibilityType & (1<<LOCKPICK)`.
- Комментарий подтверждает: `OpenDoor` на запертой двери **не** открывает (звук «дребезжания
  ручки»). Поэтому сначала обязательно `UnlockDoor`.

---

## 4. Итоговые правила для бота

### (а) «Нет подходящего ключа → НЕ давать задание / скип»

Для каждой двери здания/контейнера, которую бот собирается пересечь:

```c
bool DoorNeedsKey(Building b, int doorIdx)
{
    return b.IsDoorLocked(doorIdx)                              // building.c:44
        && (b.GetLockCompatibilityType(doorIdx) & (1 << EBuildingLockType.LOCKPICK)) == 0;  // building.c:167
}
```

- Если `DoorNeedsKey(...)` И `!HasMatchingKey(...)` (§2c) → **скип**: не назначать точку
  внутри этого контейнера/здания, либо пометить путь недостижимым (аналог Expansion
  `m_IsUnreachable=true`).
- Если дверь заперта, но с битом `LOCKPICK` (обычная дверь) — либо вскрыть ломиком (если у
  бота есть ломик: `GetKeyCompatibilityType() & (1<<LOCKPICK)`), либо тоже скип. Ломание
  выстрелом (`GetAllowDamage` + `eAIDoorTargetInformation`) — опционально, вне этого скоупа.

### (б) «Есть ключ → открыть ключом»

```c
// 1. отпереть (без ajar — как ActionUnlockShippingContainer)
b.UnlockDoor(doorIdx, false);     // building.c:59; containerlocked действие использует animate=false (:39)
// 2. только потом открыть (OpenDoor на запертой НЕ работает)
b.OpenDoor(doorIdx);              // building.c:50
// 3. (опционально) полл факта открытия
//    b.IsDoorOpened(doorIdx)     // building.c:35 — реальное полное открытие
```

Порядок критичен: `OpenDoor` на запертой двери только «дребезжит» (Expansion-комментарий,
§3c). `UnlockDoor(doorIdx, false)` — без ajar-анимации, дверь остаётся закрытой, поэтому
`OpenDoor` обязателен (в отличие от ломика, где `animate=true` приоткрывает).

Ключ **не обязателен для нативного вызова** — но наша логика должна сама гейтить:
проверять маску перед `UnlockDoor` (иначе бот «вскроет» контейнер без ключа — нарушение
механики). Один раз на «отпирание» можно портить ключ по образцу ванили
(`DealAbsoluteDmg(key, 6)`, actionunlockcontainerdoor.c:27) — [нужно проверить] хочет ли
заказчик портить/ломать ключ (в ванили ключ ломается только при НЕВЕРНОЙ попытке,
`DestroyKeyServer` на :22).

---

## Открытые вопросы / [нужно проверить]

- `[нужно проверить]` `UnlockDoor(doorIdx, false)` на `ContainerLockedBase` **на AI_SERVER**:
  вызывает ли `OnDoorUnlocked` (ставит `side<N>_lock` в фазу 1 и звук) и переходит ли дверь
  в состояние «отперта, но закрыта», требуя отдельного `OpenDoor`. Probe: `spawnobj`
  `Land_ContainerLocked_Red_DE` → `botdump` → `UnlockDoor(0,false)` → `IsDoorLocked(0)` →
  `OpenDoor(0)` → `IsDoorOpened(0)`.
- `[нужно подтвердить]` что `EnumerateInventory(LEVELORDER)` возвращает `ShippingContainerKeys_*`
  в карго/аттачах пешки, но НЕ в руках (руки — отдельно `GetHumanInventory().GetEntityInHands()`).
  Влияет на полноту `HasMatchingKey` (см. inventory-enumeration.md §«HANDS»).
- `[нужно подтвердить]` нет ли в game-data других «запертых под ключ» классов за пределами
  скрипт-дифа (config-level `Doors`/`InteractActions`). В скриптах видно только 4 цветных
  контейнера; `GetLockCompatibilityType` — виртуальная, любой мод может добавить свои типы
  (детект по маске, а не по имени класса, это уже покрывает).
- `[нужно проверить]` реальные `EBuildingLockType`-числа и что `1 << NONE` (=1) нигде не
  используется как «ключ» (база `ToolBase` возвращает `NONE` **без** сдвига — `0`, toolbase.c:16).

---

## Существенные развилки / решения (для `docs/decisions.md`)

1. **«Замок под ключ» — это маска совместимости, а не отдельный предикат.** Решение:
   детект через `IsDoorLocked(i) && (GetLockCompatibilityType(i) & (1<<LOCKPICK))==0` +
   сравнение с `ToolBase.GetKeyCompatibilityType()`, а не поиск нативного `HasKeyLock`
   (его нет). Почему: ваниль выражает совместимость битмаской `EBuildingLockType`, обе
   стороны (`Building`/`ToolBase`) возвращают `1 << enum`. Действует пока: API не сменится.

2. **Нативный `UnlockDoor` не гейтится ключом — проверку ключа бот делает сам.** Решение:
   перед `UnlockDoor` сравнивать маску двери с ключами в инвентаре; без ключа — скип (не
   вскрывать «бесплатно»). Почему: ключ проверяется только в `ActionUnlockDoors.ActionCondition`
   (player action-manager), которого у бота нет; натив — голый сеттер. Действует пока: бот
   без action-manager (решение `docs/decisions.md` «body: настоящий нокаут — без action-manager»).

3. **`land_warheadstorage_*` — вне домена «ключ».** Решение: не классифицировать бункер как
   key-lock; его двери — power/lever (`OpenDoor`/`CloseDoor`), а не `LockDoor`. Почему:
   grep по `LockDoor`/`UnlockDoor` в underground-каталоге пуст, `CanDoorBeLocked=false`.
   Действует пока: в скоуп «ключ» не входит Sakhal-бункер (отдельная задача, если понадобится
   «взломать питанием»).

4. **Порядок Unlock→Open обязателен.** Решение: `UnlockDoor(doorIdx,false)` → `OpenDoor(doorIdx)`.
   Почему: `OpenDoor` на запертой двери не открывает (Expansion-комментарий `eAIBase.c:11720`),
   а `UnlockDoor(...,false)` не приоткрывает дверь (в отличие от ломика `animate=true`).
   Действует пока: используется нативное `UnlockDoor`/`OpenDoor`.
