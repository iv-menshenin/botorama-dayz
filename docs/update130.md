# DayZ 1.29 → 1.30 — руководство по адаптации модов

Цель документа — дать агенту (dayz-dev / dayz-research / человеку) исчерпывающий
чеклист того, что ломается при переходе сервера с DayZ 1.29 на 1.30, как это
выглядит в логе и как чинить. Абстрактно, без привязки к конкретным модам; код —
только как иллюстрация «было → стало».

Референсы ванильного API 1.30: `DayZ Projects/scripts` (SDK). Для сравнения «что
изменилось» полезен `DayZ-Script-Diff` (git-репозиторий по билдам, в т.ч. 1.29).

---

## TL;DR — быстрый чеклист

1. **`config.cpp` → `CfgMods.defs.files[]`**: только прямой слэш `/` (не `\`), и
   регистр префикса должен совпадать с PBO-префиксом.
2. Пересобери мод, стартуй сервер, смотри `script.log` на `SCRIPT (E)`,
   `Can't compile`, `obsolete`, `Undefined function`.
3. **`FindFile`** не работает на этапе `OnInit` (ядро сервера ещё не готово) —
   вызывай его отложенно (`CallLater` / более поздняя фаза).
4. Замени obsolete-методы (список в §4).
5. Проверь `modded class`, размазанные по нескольким файлам: в 1.30 важен порядок
   компиляции (`Undefined function` при обращении «вперёд»).
6. Транспорт: система флюидов (`CarFluid`) и свет (`LightToggle`) переписаны (§5–6).

---

## 1. Критично: слэш в `CfgMods.defs.files[]`

**Симптом.** Мод «зарегистрирован» (в логе в списке `defines:` есть его CfgMods-имя),
но скрипты мода не грузятся: счётчики `Module: World/Mission; loaded Nx files`
не включают файлы мода, в логе нет ни ошибок, ни классов мода.

**Причина.** До 1.29 в `files[]` принимался обратный слэш, в 1.30 — только прямой.

**Было (работало в 1.29):**
```cpp
class CfgMods {
    class MyMod {
        class defs {
            class worldScriptModule {
                value = "";
                files[] = {"MyMod\4_World"};   // обратный слэш
            };
        };
    };
};
```

**Стало (1.30):**
```cpp
                files[] = {"mymod/4_World"};   // прямой слэш
```

**Внимание на регистр.** `files[]`-префикс должен совпадать с PBO-префиксом
(как собран PBO: `-prefix=...`). Если PBO собран с `-prefix="mymod"`, а в `files[]`
написано `MyMod/4_World` (заглавная) — скрипты не найдутся.

Проверка: `strings <mod>.pbo | grep -i "files"` покажет, что реально лежит в PBO.

---

## 2. `FindFile` — работает, но не на этапе `OnInit` (тайминг)

**Симптом.** Мод грузится, но листинг файлов (`FindFile`) возвращает пусто
(например «нет конфигов»), хотя файлы на диске есть.

**Что происходит.** `FindFile` **штатно резолвит** виртуальные префиксы
`$profile:` / `$saves:` / `$mission:`. Проблема в **тайминге**: на этапе `OnInit`
(и ранних фазах инициализации) ядро сервера ещё не готово, поэтому `FindFile`
возвращает пусто. После старта мира листинг работает.

**Что делать:** вызывать `FindFile` отложенно — через
`GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(..., delayMs, false)` или из
более поздней фазы (`OnMissionStart`, `OnUpdate` и т.п.).

**Было (пусто — вызов в OnInit):**
```c
modded class MissionServer {
    override void OnInit() {
        super.OnInit();
        RestoreAllShelters();   // ← FindFile внутри, ядро ещё не готово → пусто
    }
    void RestoreAllShelters() {
        string fileName;
        FileAttr fileAttr;
        FindFileHandle h = FindFile("$profile:/Devaliada/Respawns/*", fileName, fileAttr, 0);
        if (h) { /* … */ }
    }
}
```

**Стало (отложенный вызов):**
```c
modded class MissionServer {
    override void OnInit() {
        super.OnInit();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RestoreAllShelters, 15000, false);
    }
    void RestoreAllShelters() {
        string fileName;
        FileAttr fileAttr;
        FindFileHandle h = FindFile("$profile:/Devaliada/Respawns/*", fileName, fileAttr, 0);
        if (h) {
            bool ok = true;
            while (ok) {
                if (!(fileAttr & FileAttr.DIRECTORY) && fileName.LastIndexOf(".bin") > 0)
                    /* обработать файл */;
                ok = FindNextFile(h, fileName, fileAttr);
            }
        }
    }
}
```

`FileExist` / `OpenFile` / `FileSerializer` этой проблемы не имеют — они работают с
префиксами и на ранних фазах (для чтения/записи одиночных файлов оставляем
`$profile:`/`$mission:`).

---

## 3. Кросс-файловые ссылки в `modded class` (порядок компиляции)

**Симптом.** `SCRIPT (E): Undefined function 'X.Method'` — метод класса X используется
в одном `.c`-файле, а определён в другом `.c`-файле того же `modded class X`.

**Причина.** В 1.30 компилятор учитывает порядок файлов: метод, объявленный в файле,
который компилируется позже, «не виден» при обращении из более раннего файла.
(В 1.29 это работало.)

**Пример.** `ItemBase.c` (корневой, компилится раньше) зовёт `IsVipItem()`,
объявленный в `Vip/ItemBase.c` (подкаталог, компилится позже) → `Undefined function`.

**Что делать:** перенести определение метода в файл, который компилируется раньше
(обычно корневой `X.c`), либо объединить файлы одного класса в один.

**Было:**
```c
// 4_World/ItemBase.c (корневой)
modded class ItemBase {
    override bool CanPutIntoHands(EntityAI parent) {
        if (IsVipItem())        // ← определён в Vip/ItemBase.c (позже) → Undefined function
            return false;
        return super.CanPutIntoHands(parent);
    }
};
// 4_World/Vip/ItemBase.c (позже)
modded class ItemBase {
    bool IsVipItem() { return m_VipOwnerID != ""; }
};
```

**Стало:** перенести `IsVipItem()` (и поле `m_VipOwnerID`) в корневой `ItemBase.c`.

---

## 4. Obsolete / удалённые методы

Метод помечен `[Obsolete("1.30: …")]` — это **warning** (мод грузится, но варнинг в логе).
Полностью удалённый метод (нет даже obsolete-стаба) при `override` даёт
**error** `no function with this name in the base class` (или `Multiple declaration`,
если пытаешься переопределить `proto native`).

### 4.1 Инвентарь / EntityAI (перенос предмета в attachment/cargo)

Старый набор `*TakeEntityAsAttachment` → новый `*TakeEntityToTargetAttachment`
(у нового появился параметр **target**).

| Было | Стало |
|---|---|
| `LocalTakeEntityAsAttachment(item)` | `LocalTakeEntityToTargetAttachment(target, item)` |
| `LocalTakeEntityAsAttachmentEx(item, slot)` | `LocalTakeEntityToTargetAttachmentEx(target, item, slot)` |
| `ServerTakeEntityAsAttachment(item)` | `ServerTakeEntityToTargetAttachment(target, item)` |
| `ServerTakeEntityAsAttachmentEx(item, slot)` | `ServerTakeEntityToTargetAttachmentEx(target, item, slot)` |
| `PredictiveTakeEntityAsAttachment(item)` | `PredictiveTakeEntityToTargetAttachment(target, item)` |

**Было:**
```c
bot.LocalTakeEntityAsAttachment(item);
```
**Стало:**
```c
bot.LocalTakeEntityToTargetAttachment(bot, item);
```

### 4.2 Прочее

| Было | Стало / примечание |
|---|---|
| `GetHeatIsolationInit()` | `GetHeatIsolation()` (тот же возврат `ItemBaseType.m_HeatIsolation`) |
| `BuildPartServer(Man, string, int)` | `BuildPartServerEx(Man, string, int)` (сигнатура та же) |
| `PlaceOnGround()` (UndergroundStash) | удалён, «No replacement» → инлайнить: `pos[1] = g_Game.SurfaceRoadY(pos[0], pos[2]) + 0.22; SetPosition(pos);` |
| `UpdatePathgraphDoorByAnimationSourceName()` | `UpdatePathgraphEntityState()` (ванила) |
| `IsScriptedLightsOn()` | `LightIsOn()` (obsolete-стаб есть, но лучше новый) |
| `IsVitalCarBattery()` / `IsVitalTruckBattery()` | `Transport.NeedElectricitySourceDevice()` |
| `ForceUpdateLightsStart()` / `ForceUpdateLightsEnd()` | obsolete, «No replacement» |
| `BrakesRearLight()` / `ReverseRearLight()` и пр. | `VehicleLightsComponent` |

---

## 5. Транспорт: система флюидов (`CarFluid`)

В 1.30 флюиды автомобиля переведены на перечисление `CarFluid`:

```c
enum CarFluid {
    FUEL, OIL, BRAKE, COOLANT,
    USER1, USER2, USER3, USER4   // резерв для модов
};
```

Нативные методы (в `Car`/`Transport`):
```c
float GetFluidCapacity(CarFluid fluid);   // ёмкость бака
float GetFluidFraction(CarFluid fluid);   // текущий уровень (0..1)
void  Fill(CarFluid fluid, float amount);  // долить
void  Leak(CarFluid fluid, float amount);  // отлить
void  LeakAll(CarFluid fluid);             // слить всё
void  OnFluidChanged(CarFluid fluid, float newValue, float oldValue); // событие
```

Ёмкости по умолчанию (колесный транспорт): `oilCapacity=4`, `coolantCapacity=6`,
`brakeFluidCapacity=1` (задаются в `cfgVehicles`).

### 5.1 CE-спавн заливает только топливо

Ванильный `EEOnCECreate()` (создание через Central Economy) заливает **только
FUEL (0–35%)**, а OIL / BRAKE / COOLANT остаются пустыми → двигатель деградирует.

```c
override void EEOnCECreate() {
    float maxVolume = GetFluidCapacity(CarFluid.FUEL);
    float amount = Math.RandomFloat(0.0, maxVolume * 0.35);
    Fill(CarFluid.FUEL, amount);   // ← только топливо
}
```

Дебаг-спавн (`FillUpCarFluids`, зовётся только из `OnDebugSpawn`) заливает
FUEL/COOLANT/OIL (по 200), но **не BRAKE**.

### 5.2 Хуки создания/загрузки

- `EEOnCECreate()` — «создана заново CE/дебагом» (свежий спавн).
- `AfterStoreLoad()` — «загружена из DB/Storage (после загрузки детей)»
  (персистентные машины). В конструкторе/`EEInit` флюиды ещё не готовы —
  заполнять рано.

### 5.3 `Fill` — «асинхронный» в смысле чтения

`GetFluidFraction()` сразу после `Fill()` может вернуть устаревшее значение;
факт изменения виден по `OnFluidChanged(old, new)`. Для доливки персистентных
машин — отложить через `CallLater`:
```c
override void AfterStoreLoad() {
    super.AfterStoreLoad();
    if (g_Game.IsServer())
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(EnsureFluids, 5000, false);
}
```

---

## 6. Транспорт: свет (удалён `ToggleHeadlights`)

`CarScript.ToggleHeadlights()` **полностью удалён** в 1.30. Тумблер теперь —
`Transport.LightToggle()` (native) + `LightOn()` / `LightOff()` / `LightIsOn()`.

```c
// Transport (1.30)
proto native bool LightIsOn();
proto native void LightOn();
proto native void LightOff();
proto native void LightToggle();   // включает/выключает
bool OnBeforeLightOn() { return true; }   // хук «можно ли включить»
void OnUpdateLight() { UpdateLights(); }  // хук «обнови свет»
```

**Важно:** `LightToggle()` — `proto native`, переопределить его нельзя: попытка
`override void LightToggle()` даёт **`Multiple declaration of function 'LightToggle'`**.

Если мод делал `override void ToggleHeadlights()` — этот метод нужно убрать
(фича «удержания света» в 1.30 переехала в `m_HeadlightsState`/`UpdateHeadlightState`).

`UpdateLights(int new_gear = -1)` остался (не obsolete).

---

## 7. Экшены: `ActionFillBottleBase` → `ActionObtainLiquidBase`

Класс действия заливки жидкости переработан: старый `ActionFillBottleBase` помечен
obsolete, реальный экшен — `ActionObtainLiquidBase` (и его колбэк
`ActionObtainLiquidBaseCB`).

```c
[Obsolete("1.30: replaced by class ActionObtainLiquidBase")]
class ActionFillBottleBase: ActionContinuousBase { … }

class ActionObtainLiquidBase: ActionContinuousBase { … }  // новый
class ActionFillBottleSnow: ActionObtainLiquidBase { … }   // теперь от нового базового
```

Сигнатуры `ActionCondition(PlayerBase, ActionTarget, ItemBase)` и
`ActionConditionContinue(ActionData)` у нового класса совпадают со старыми,
поэтому обычно достаточно переименовать `modded class`.

Попутно: для флюидов транспорта появились отдельные экшены
`ActionFillFuel` / `ActionFillCoolant` / `ActionFillOil` / `ActionFillBrakes`.

---

## 8. Энумы файловой системы: `FileAttr` / `FindFileFlags`

`FindFile` сменил 4-й параметр с `int` на перечисление `FindFileFlags`:

```c
enum FindFileFlags {
    DIRECTORIES,  // «в каталогах файловой системы»
    ARCHIVES,     // «в архивах (.pak)»
    ALL           // «и там, и там»
};
```

`FileAttr` (out-параметр) — **битовая маска** (`DIRECTORY/HIDDEN/READONLY/INVALID`),
хотя декомпилятор может показать их как последовательные `0,1,2,3` — это артефакт
декомпиляции, реальные значения — биты. Проверять каталог надо побитово:
```c
if (fileAttr & FileAttr.DIRECTORY) { /* это каталог */ }
```

---

## 9. Контейнеры: доступ к грузу через `AreChildrenAccessible()`

В 1.30 доступ к грузу контейнера контролируется `GameInventory.AreChildrenAccessible()`
(→ `EntityAI.AreChildrenAccessible()`). Если мод хранил «открытость» контейнера в
своём поле (например `m_Openable.IsOpened()`), а не через этот механизм, то после
открытия дверцы груз останется недоступным (инвентарь не показывается), хотя
визуально контейнер открыт.

Общий признак такой проблемы у стороннего мода: варнинги вида
```
FIX-ME: Overriding function 'X' but not marked as 'override'
Missing ';' at the end of line
Broken expression (missing ';'?)
```
— код писался под старый DayZ и не обновлялся.

---

## 10. Готчи компиляции (косметика, но мешает читать лог)

### 10.1 «No need to use 'Cast' for up-casting»
Апкаст через `Cast` не нужен — прямой апкаст и так неявный.

**Было:**
```c
EntityAI ent = EntityAI.Cast(item);   // item уже ItemBase (наследник EntityAI)
```
**Стало:**
```c
EntityAI ent = item;
```
Или вообще без промежуточной переменной: `item.GetSomething()`.

### 10.2 Конкатенация строк в `Print`
Нельзя склеивать со строкой `enum` и `bool`-результат сравнения массивов — будет
`Incompatible parameter`.

**Было (не компилируется):**
```c
Print("x=" + pattern + " handle=" + (findFileHandle != null) + " attr=" + fileAttr);
```
**Стало:**
```c
if (findFileHandle)
    Print("x=" + pattern + " firstFile='" + fileName + "'");
else
    Print("x=" + pattern + " HANDLE_NULL");
```
`float`/`string`/`int` в `Print` конкатенируются нормально.

---

## 11. Миграция storage (не ошибка кода)

При первом старте после апдейта 1.29 → 1.30 в логе возможны:
```
[EntityAI::OnStoreLoad] Scripted variables corrupted upon "ItemName"
```
Это персистентные предметы с несогласованной раскладкой скрипт-переменных —
не фатально, предметы пересоздаются. К адаптации кода отношения не имеет, но
стоит отличать от настоящих ошибок.

---

## 12. Порядок работы агента над адаптацией мода

1. Проверить `config.cpp` (`files[]`, слэш + регистр) — чаще всего первый блокер.
2. Пересобрать, стартануть сервер, собрать **все** `SCRIPT (E)` и `obsolete` из лога.
3. Заменить obsolete-методы (§4) и переименованные классы (§7).
4. Пройтись по `modded class`, размазанным по файлам — починить порядок (§3).
5. Для транспорта — флюиды и свет (§5–6).
6. Для листинга файлов — убедиться, что `FindFile` вызывается не на этапе `OnInit`, а отложенно (§2).
7. Убрать косметику (§10), чтобы лог был чистым для следующей итерации.
8. Прогнать E2E и зафиксировать результат.
