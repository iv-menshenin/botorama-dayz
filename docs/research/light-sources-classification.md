# Research: надёжная классификация источников света (class hierarchy / направленность / слоты / NV-оптика)

Статус: **исследование**. Дополняет `light-sources.md` — там единый детект «включён»
(`GetCompEM().IsWorking()`) и калиброванные Expansion-значения интенсивности; здесь — **как
однозначно отличить источник по классу/направленности/слоту** и **серверно детектить
NV-оптику**. Мотивация — хрупкость текущей классификации в `modded_PlayerBase`: roadflare
(яркий) не давал illumination; `UniversalLight` на шлеме vs на оружии — разная интенсивность
и направленность; имя слота батареи `"Battery"` и слота NVG — допущения.

---

## 1. Иерархия классов и `IsInherited`-предикаты

Важная находка: **`ItemOptics`/`Weapon`/`NVGoggles` НЕ наследуют `ItemBase`** — они идут
веткой `InventoryItemSuper` (engine-класс, брат `InventoryItem : EntityAI`), тогда как
`ItemBase : InventoryItem`. Поэтому каст оптики/NVG — через `ItemOptics.Cast()` /
`EntityAI.Cast()`, **НЕ** `ItemBase.Cast()` (вернёт `null`). Источники-«предметы» (фонарь,
факел, налобник, химсвет, roadflare) — `ItemBase`, их `ItemBase.Cast()` ловит.

| Источник | Класс | extends | файл:строка | Предикат |
|---|---|---|---|---|
| ручной фонарь | `Flashlight` | `ItemBase` | itembase/flashlight.c:1 | `IsInherited(Flashlight)` |
| химсвет | `Chemlight_ColorBase` (+`White/Red/Green/Blue/Yellow`) | `ItemBase` | itembase/chemlight.c:1, :202–206 | `IsInherited(Chemlight_ColorBase)` |
| факел (база) | `FlammableBase` | `ItemBase` | itembase/torch.c:1 | `IsInherited(FlammableBase)` (ловит все факелы) |
| факел | `Torch` | `FlammableBase` | itembase/torch.c:920 | — (покрыт `FlammableBase`) |
| длинный факел | `LongTorch` | `Torch` | itembase/longtorch.c:1 | — |
| видео-факел | `Torch_Video` | `LongTorch` | itembase/torch_video.c:1 | — |
| roadflare | `Roadflare` | `ItemBase` | itembase/roadflare.c:24 | `IsInherited(Roadflare)` |
| налобник | `Headtorch_ColorBase` (+`Black/Grey`) | `Clothing` | clothing/headtorch_colorbase.c:1 | `IsInherited(Headtorch_ColorBase)` |
| насадка (база) | `Switchable_Base` | `ItemBase` | itembase/switchable_base.c:1 | — |
| винтовочный/шлемный фонарь | `UniversalLight` | `Switchable_Base` | itembase/switchable_base/universallight.c:1 | `IsInherited(UniversalLight)` |
| пистолетный фонарь | `TLRLight` | `Switchable_Base` | itembase/switchable_base/tlrlight.c:1 | `IsInherited(TLRLight)` |
| NVG | `NVGoggles` | `PoweredOptic_Base` | itembase/nvgoggles.c:1 | `IsInherited(NVGoggles)` или `IsNVG()` (nvgoggles.c:172) |
| NV-оптика (база) | `ItemOptics` | `InventoryItemSuper` | core/inherited/itemoptics.c:1 | `ItemOptics.Cast(it) && GetCurrentNVType()!=NVTypes.NONE` |
| NV-оптика (конкретно) | `KazuarOptic` / `StarlightOptic` | `ItemOptics` | inventory_base/optics.c:54 / :117 | (см. §5) |
| powered optic (база) | `PoweredOptic_Base` | `ItemOptics` | itembase/poweredoptic_base.c:1 | — |

---

## 2. Направленность: определяется РОДИТЕЛЕМ (слотом), а НЕ классом

Ключевой вывод: **`UniversalLight` — один и тот же класс и на оружии, и на шлеме.**
Направленность/интенсивность задаёт **родитель** (`GetHierarchyParent()`), а не `IsInherited`.

- **Vanilla (как свет реально фокусируется)** — `UniversallightLight.UpdateLightMode(slotName)`
  (scriptedlightbase/spotlightbase/universallightlight.c:26–37): узкий луч (angle/2, radius×1.8)
  только если `slotName == "weaponFlashlight"`; иначе — широкий (default). Имя слота берётся из
  `GetInventory().GetCurrentAttachmentSlotInfo(slot_id, slot_name)` →
  `InventorySlots.GetSlotName(il.GetSlot())` (scriptedlightbase.c:83–99, хук `UpdateMode()`).
- **Expansion (канонический предикат для Emit-значений)** — `modded UniversalLight`
  (`DayZExpansion_AI/Entities/ItemBase/switchable_base/universallight.c`):
  `if (GetHierarchyParent() && GetHierarchyParent().IsWeapon()) return 0.045; else return 0.0255;`
  с комментарием «on weapon has significantly more illumination power than in hands/on helmet».
- **TLRLight (пистолетный)** — `PistollightLight.UpdateLightMode` тоже проверяет только
  `"weaponFlashlight"` (spotlightbase/pistollightlight.c:27), а его слот `"pistolFlashlight"` —
  **не совпадает** → пистолетный фонарь **всегда в широком режиме** (vanilla не даёт ему
  narrow-beam). Expansion значение 0.0185 — фиксированное.

**Ответ на вопрос задачи** — направленность = **родитель (оружие vs не-оружие)**, что
коррелирует со слотом (`weaponFlashlight`/`pistolFlashlight` vs `helmetFlashlight`), а НЕ класс.
Рекомендуемый предикат (проще и без строки-слота):

```
bool dirLight = light.IsInherited(UniversalLight) && light.GetHierarchyParent()
                && light.GetHierarchyParent().IsWeapon();
```

`Weapon.IsWeapon()` → `true` (core/inherited/weapon.c:6, `override bool IsWeapon() { return true; }`).
`GetHierarchyParent()` — native `EntityAI` (entityai.c:880).

Таблица направленности (дополняет `light-sources.md` §7):

| Источник | Направленность | Гейт `IsRaised()`? | Основа |
|---|---|---|---|
| `UniversalLight` на оружии | **направленный** (куда целишься) | да | родитель `IsWeapon()` |
| `UniversalLight` на шлеме/в руках | постоянный (широкий) | нет | родитель НЕ weapon |
| `TLRLight` | направленный (но всегда широкий луч) | да | класс |
| `Flashlight` (ручной) | направленный вперёд модели | **нет** (светит не по прицелу) | класс |
| `Headtorch_ColorBase` | направленный, следует за головой | нет | класс |
| `FlammableBase` / `Roadflare` / `Chemlight` | всенаправленный | нет | класс |

---

## 3. Точные имена слотов и канонический API

Имена слотов — в `config.cpp` (класс `Slot_*`, поле `name=`) + строки в action-ах:

| Слот | Класс (config.cpp) | `name=` | файл:строка | потребитель |
|---|---|---|---|---|
| винтовочный фонарь | `Slot_weaponFlashlight` | `"weaponFlashlight"` | config.cpp:418–421 | actionturnonweaponflashlight.c:27 |
| пистолетный фонарь | `Slot_pistolFlashlight` | `"pistolFlashlight"` | config.cpp:201–205 | actionturnonweaponflashlight.c:31 |
| шлемный фонарь | `Slot_helmetFlashlight` | `"helmetFlashlight"` | config.cpp:2456–2460 | actionturnonhelmetflashlight.c:35 |
| NVG | `Slot_NVG` | `"NVG"` | config.cpp:2450–2454 | `FindAttachmentBySlotName("NVG")` |
| батарея D (лампы/прожектора) | `Slot_BatteryD` | `"BatteryD"` | config.cpp:449–453 | (НЕ для 9V) |

### Батарея 9V — «слота `"Battery"` НЕТ» (допущение из задачи опровергнуто)

`Battery9V : ItemBase` (itembase/battery9v.c:1) создаётся **по классу**, не по имени слота:
`GetInventory().CreateAttachment("Battery9V")` (headtorch_colorbase.c:46) или
`CreateInInventory("Battery9V")`. Слота с `name="Battery"`/`"Battery9V"` в ванильных скриптах нет
(grep по config.cpp: только `BatteryD`/`CarBattery`/`TruckBattery`/`LargeBattery`). Батарея — это
**энергоисточник** EM, а не слот-адресуемый предмет: `GetCompEM().GetEnergySource()`
(componentenergymanager.c:1337) / `HasEnoughStoredEnergy()` (:944). Её слот объявлен в CfgVehicles
(игровые данные, вне scripts) и в скриптах недоступен по имени.

**Вывод**: «есть батарея» = `GetCompEM().GetEnergySource() != null` (или `HasEnoughStoredEnergy()`);
имя слота батареи не нужно и в скриптах не подтверждается.

### Канонический API имени слота (не хардкодить строку)

- `InventorySlots.GetSlotIdFromString(string)` → int (3_game/systems/inventory/inventoryslots.c:42, native)
- `InventorySlots.GetSlotName(int)` → string (там же :48, native)
- `GameInventory.GetCurrentAttachmentSlotInfo(out slot_id, out slot_name)` (3_game/systems/inventory/inventory.c:456)
- `EntityAI.GetInventory()` / `GetInventory().GetAttachmentSlotsCount()` / `GetAttachmentSlotId(i)` /
  `FindAttachment(slot_id)` (используется в universallight.c `CanPutAsAttachment`).

Т.е. при необходимости определить «на каком слоте висит фонарь» — `GetCurrentAttachmentSlotInfo()`
→ `InventorySlots.GetSlotName(slot_id)` → сравнить с `"weaponFlashlight"`. Но для нашей задачи
родительский предикат (§2) проще и надёжнее.

---

## 4. Детект «включён» per-класс (расхождения с `GetCompEM().IsWorking()`)

Базовый детект у всех «светящих» — `GetCompEM().IsWorking()` (см. light-sources.md §1–2).
Расхождения/нюансы:

| Класс | Детект | файл:строка | Примечание |
|---|---|---|---|
| `FlammableBase` | `IsIgnited()` == `GetCompEM().IsWorking()` | torch.c:141 | обёртка; горящий факел не в карго/насадку (`CanBeTakenAsCargo`/`CanPutAsAttachment` = `!IsWorking()`, torch.c:130–135) |
| `Roadflare` | `GetCompEM().IsWorking()` | roadflare.c | + `m_BurningState` (enum, :5; net-var :59) — только если различать фазы |
| `Chemlight_ColorBase` | `GetCompEM().IsWorking()` | chemlight.c | одноразовый: `OnWorkStop` → `SetHealth(0)`; нет SwitchOff-эксшена |
| `Flashlight`/`UniversalLight`/`TLRLight`/`Headtorch_ColorBase` | `GetCompEM().IsWorking()` | — | — |
| `NVGoggles` | `m_IsLowered` && `IsWorking()` | nvgoggles.c | `RotateGoggles(state)` ставит `m_IsLowered = !state` и `SwitchOn/SwitchOff` (:134–170); `IsWorking()` тут = `CanWork()` (наследован от ItemOptics) |
| NV-оптика | `GetCurrentNVType() != NVTypes.NONE` + `IsWorking()` | optics.c:59/117 | см. §5 |

---

## 5. NV-оптика: серверный детект

**`ItemOptics.GetCurrentNVType()` — скриптовый (не native) и сервер-валиден.**

- Base `ItemOptics.GetCurrentNVType()` возвращает `NVTypes.NONE` (itemoptics.c:497, закомментирован
  TODO-пример). `KazuarOptic` (optics.c:54) / `StarlightOptic` (optics.c:117) override'ят:
  `if (IsWorking()) return NVTypes.NV_OPTICS_*; else return NVTypes.NV_OPTICS_OFF;`.
- `IsWorking()` у `ItemOptics` = `GetCompEM() && GetCompEM().CanWork()` (itemoptics.c:239) — НЕ
  EM-`IsWorking()`, и это **сервер-валидный** предикат (`CanWork` не гейтится клиентом).
- `m_CurrentOpticMode` — `RegisterNetSyncVariableInt(..., 0, 63)` (itemoptics.c:33), ставится
  сервером через `SetCurrentOpticMode()` → `ActionToggleNVMode.OnExecuteServer` → `SwitchMode`
  (actiontogglenvmode.c:44–58, серверная ветка `OnExecuteServer`). Т.е. сервер читает своё
  авторитетное значение — **`GetCurrentNVType()` корректен на выделенном сервере**.

Enum `NVTypes` (dayzplayercamera_base.c:54–66): `NONE=0, NV_GOGGLES, NV_GOGGLES_2D, NV_GOGGLES_OFF,
NV_OPTICS_ON, NV_OPTICS_OFF, NV_OPTICS_KAZUAR_DAY, NV_OPTICS_KAZUAR_NIGHT, NV_OPTICS_STARLIGHT_DAY,
NV_OPTICS_STARLIGHT_NIGHT`.

**⚠ Готча: `PlayerBase.IsNVGWorking()/IsNVGLowered()/GetNVType()` — КЛИЕНТ-only.**
Они читают `m_ActiveNVTypes` (playerbase.c:315), который наполняется `AddActiveNV`/`RemoveActiveNV`,
а те на мультиплеер-сервере делают `Error("illegal server-side call!")` и `return`
(playerbase.c:9115–9142). Т.е. на выделенном сервере `m_ActiveNVTypes` пуст, и эти helper'ы
**нельзя** использовать в нашем серверном кэше.

**Как серверно найти NV-оптику / NVG:**

- **NV-оптика** (насадка на оружие): `Weapon_Base.Cast(GetItemInHands()).GetAttachedOptics()`
  (native, core/inherited/weapon.c:403) → `ItemOptics`. `GetItemInHands()` → `ItemBase`
  (playerbase.c:6437). Kazuar/Starlight — всегда насадки на оружие, так что `GetAttachedOptics()`
  покрывает кейс. (`IsInOptics()` для «активно смотрит в прицел» — отдельный вопрос, см. открытые.)
- **NVG**: канонически через кэш экипировки —
  `GetCachedEquipment().GetEntitiesByCategory(ECachedEquipmentItemCategory.NVG)` (native,
  cachedequipmentstoragebase.c:70) — именно так делает ванильный `ActionToggleNVG` (actiontogglenvg.c:42–46)
  и рендер (dayzplayerimplement.c:3921). Кэш `LIGHT`/`OPTICS` **не наполняется** ванилью (§6), но
  `NVG` — наполняется (только `NVGoggles` override'ит `GetCachedEquipmentCategory()` → NVG,
  nvgoggles.c:11). Альтернатива — `FindAttachmentBySlotName("Eyewear").FindAttachmentBySlotName("NVG")`
  / `"Headgear"` (playerbase.c:9174–9180, `DEBUGRotateNVG`).

---

## 6. Кэш экипировки `ECachedEquipmentItemCategory` — только NVG реально наполняется

Enum `ECachedEquipmentItemCategory` (3_game/enums/ecachedequipmentitemcategory.c):
`NONE=0, LIGHT=1, NVG=2, OPTICS=4, FIREARM_SHORT=8, FIREARM_LONG=16`.

Классификатор `EntityAI.GetCachedEquipmentCategory()` — дефолт `NONE` (entityai.c:336); в ванили
**override'ит только `NVGoggles`** (→ `NVG`, nvgoggles.c:11). `LIGHT`/`OPTICS`/`FIREARM_*` никем
не возвращаются → `GetEntitiesByCategory(LIGHT)` / `(OPTICS)` вернёт **пусто**. Значит:
- для NVG `GetCachedEquipment().GetEntitiesByCategory(NVG)` — рабочий серверный путь;
- для источников света и NV-оптики кэш **нельзя** использовать — перебираем вручную
  (слоты §3 / `GetItemInHands()` + `GetAttachedOptics()`).

---

## 7. Сводная таблица «класс → слот → направленность → детект»

| Класс | Где ищем | Слот | Направленность | Детект «включён» (сервер) |
|---|---|---|---|---|
| `Flashlight` | `GetItemInHands()` | руки | вперёд (не по прицелу) | `GetCompEM().IsWorking()` |
| `Chemlight_ColorBase` | руки/карго | — | всенаправл. | `GetCompEM().IsWorking()` |
| `FlammableBase`(+`Torch`/`LongTorch`/`Torch_Video`) | `GetItemInHands()` | руки | всенаправл. | `IsIgnited()` = `IsWorking()` |
| `Roadflare` | руки/земля | — | всенаправл. | `GetCompEM().IsWorking()` |
| `Headtorch_ColorBase` | `GetItemOnSlot("Eyewear")` | `Eyewear` | за головой | `GetCompEM().IsWorking()` |
| `UniversalLight` на оружии | `FindAttachmentBySlotName("weaponFlashlight")` / `GetHierarchyParent().IsWeapon()` | `weaponFlashlight` | направл. (гейт `IsRaised`) | `GetCompEM().IsWorking()` |
| `UniversalLight` на шлеме | `FindAttachmentBySlotName("helmetFlashlight")` | `helmetFlashlight` | постоянный | `GetCompEM().IsWorking()` |
| `TLRLight` | `FindAttachmentBySlotName("pistolFlashlight")` | `pistolFlashlight` | направл. (гейт `IsRaised`, луч широкий) | `GetCompEM().IsWorking()` |
| `NVGoggles` | кэш `GetEntitiesByCategory(NVG)` / `"NVG"` | `NVG` | — (зрение) | `m_IsLowered` && `IsWorking()` |
| NV-оптика (`KazuarOptic`/`StarlightOptic`) | `GetItemInHands()` → `Weapon.GetAttachedOptics()` | слот оптики на оружии | — (зрение) | `GetCurrentNVType() != NVTypes.NONE` |

---

## 8. Открытые вопросы / [нужно подтвердить]

1. **[нужно подтвердить]** `ItemOptics.GetCurrentNVType()` на выделенном сервере возвращает
   non-`NONE` для активной NV-оптики (`KazuarOptic`/`StarlightOptic`) и `NONE` для обычной.
   Probe: дать боту оружие с `KazuarOptic` → `botdump` `GetAttachedOptics().GetCurrentNVType()`.
2. **[нужно подтвердить]** `GetCachedEquipment().GetEntitiesByCategory(NVG)` наполняется на
   выделенном сервере (ванильный `ActionToggleNVG.Start` серверный, но факт наполнения кэша на
   DS — проверить). Probe: надеть NVG → query NVG → `botdump` count>0.
3. **[нужно подтвердить]** `NVGoggles.m_IsLowered` валиден/актуален на сервере после
   `RotateGoggles` (поле не `RegisterNetSyncVariable`, чисто серверное; `RotateGoggles` зовётся
   серверно из `ActionToggleNVG`, но подтвердить, что значение совпадает с фактическим опусканием).
4. **[нужно подтвердить]** `m_IsRaised` (гейт направленных источников) валиден на сервере —
   уже помечено в light-sources.md §8.2; здесь подтверждаем привязку именно к насадкам на оружие.
5. **[нужно проверить]** `GetHierarchyParent().IsWeapon()` vs slot-name `"weaponFlashlight"` —
   оба отличают UniversalLight-на-оружии; подтвердить, что parent-based (Expansion) не ломается
   на краевых случаях (напр. UniversalLight на `weaponHandguardM4`/`weaponHandguardAK` — см.
   `InitAttachmentsSlotsToCheck`, universallight.c:123–130 — фонарь может цепляться и на цевьё,
   а не на `weaponFlashlight`; тогда слот-строка НЕ "weaponFlashlight", а родитель всё равно
   оружие → parent-based надёжнее).
6. **[нужно подтвердить]** NV-оптика «активна» = просто `GetCurrentNVType()!=NONE` или нужен
   ещё `IsInOptics()` (игрок реально смотрит в прицел)? Expansion использует только
   `GetCurrentNVType()!=NONE` (ItemBase.c:392 `eAI_GetNightVisibility`) — без `IsInOptics()`.
   Решить семантику для `Illumination` (как и NVG: даём зрение, если включено, независимо от
   того, смотрит ли в прицел).

---

## 9. Источники

Ваниль (`DayZ Projects/scripts`):
- `4_world/entities/itembase/flashlight.c:1`; `chemlight.c:1,:202–206`; `torch.c:1,:130–141,:920`;
  `longtorch.c:1`; `torch_video.c:1`; `roadflare.c:24`; `switchable_base.c:1`;
  `switchable_base/universallight.c:1,:123–130`; `switchable_base/tlrlight.c:1`; `nvgoggles.c:1,:11,:172`;
  `poweredoptic_base.c:1`; `clothing/headtorch_colorbase.c:1,:46`
- `4_world/entities/core/inherited/itemoptics.c:1,:33,:239,:497`; `inventory_base/optics.c:54,:117`
- `4_world/entities/core/inherited/weapon.c:4,:6,:403`
- `4_world/entities/scriptedlightbase.c:83–99`; `spotlightbase/universallightlight.c:26–37`;
  `spotlightbase/pistollightlight.c:27`; `spotlightbase/headtorchlight.c:1`
- `4_world/classes/useractionscomponent/actions/singleuse/actionturnonweaponflashlight.c:27–31`,
  `actionturnonhelmetflashlight.c:35`, `actionturnonheadtorch.c`, `actiontogglenvmode.c:44–58`,
  `actiontogglenvg.c:42–46`
- `4_world/entities/manbase/playerbase.c:315,:6437,:9083,:9115–9142,:9174–9180`
- `4_world/entities/manbase/dayzplayer/dayzplayercamera_base.c:54–66`
- `4_world/entities/cachedequipmentstorage.c`; `3_game/entities/cachedequipmentstoragebase.c:70,:100`
- `3_game/enums/ecachedequipmentitemcategory.c`; `3_game/entities/entityai.c:336,:880`
- `3_game/systems/inventory/inventoryslots.c:42,:48`; `3_game/systems/inventory/inventory.c:456`
- `3_game/tools/component/componentenergymanager.c:944,:1337`
- `config.cpp:201–205,:418–421,:449–453,:2450–2460`

Expansion (`DayZ-Expansion-Scripts/DayZExpansion/AI/Scripts/4_World/DayZExpansion_AI/Entities/`):
- `ItemBase/switchable_base/universallight.c` (parent.IsWeapon() → 0.045/0.0255)
- `ItemBase/nvgoggles.c` (`m_IsLowered && IsWorking()` → 0.35)
- `ItemBase.c:392` (`eAI_GetNightVisibility` через `GetCurrentNVType() != NVTypes.NONE`,
  zoom `GetZeroingDistanceZoomMax()*0.001`)

---

## 10. Существенные развилки / принятые решения (для docs/decisions.md)

1. **Направленность = родитель (`GetHierarchyParent().IsWeapon()`), а не класс и не строка слота.**
   Почему: `UniversalLight` один класс на оружии и шлеме; vanilla фокусирует луч по слоту
   `"weaponFlashlight"`, но фонарь может висеть и на цевье (`weaponHandguardM4/AK`, universallight.c
   `CanPutAsAttachment`) — тогда слот-строка не "weaponFlashlight", а родитель всё равно оружие.
   Expansion канонизирует parent-based (0.045 vs 0.0255). Родительский предикат покрывает и
   цевьё, и `weaponFlashlight`.
2. **Слот батареи 9V в скриптах НЕ существует** (только `BatteryD`/`CarBattery`/`TruckBattery`/
   `LargeBattery`); «есть батарея» = `GetCompEM().GetEnergySource()`. Почему: батарея — энергоисточник
   EM, создаётся по классу `CreateAttachment("Battery9V")`, слот объявлен в CfgVehicles вне scripts.
   Не хардкодить `"Battery"`.
3. **NV-детект на сервере — через `GetCurrentNVType()`/`m_IsLowered`, а НЕ `PlayerBase.IsNVGWorking()`.**
   Почему: `m_ActiveNVTypes` (и helper'ы `IsNVGWorking/IsNVGLowered/GetNVType`) — клиент-only
   (`AddActiveNV` кидает `illegal server-side call` на DS). Серверно — `ItemOptics.GetCurrentNVType()`
   (авторитетный `m_CurrentOpticMode` + `CanWork()`) и `NVGoggles.m_IsLowered`.
4. **Кэш экипировки `GetEntitiesByCategory` пригоден только для NVG, не для LIGHT/OPTICS.** Почему:
   `ECachedEquipmentItemCategory.LIGHT/OPTICS` в ванили никем не возвращаются (только `NVGoggles`
   override'ит `GetCachedEquipmentCategory`). Источники света и NV-оптику перебираем вручную по слотам.
