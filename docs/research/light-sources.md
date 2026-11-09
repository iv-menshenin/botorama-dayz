# Research: источники света игрока/бота (light sources)

Статус: **исследование**. Цель — точные сигнатуры детекта «источник света **включён**» на
сервере для двух новых методов `modded_PlayerBase`: `EmitLightIntensivity()` (насколько далеко
игрока видно — по интенсивности ЕГО источника) и `IlluminationIntensivity()` (бинарно 0/1,
даёт ли хозяину зрение). План — `/home/devalio/.opencode/plan/vision-rework.md`. Факты с
`файл:строка`; неподтверждённое — `[нужно подтвердить]`/`[нужно проверить]`.

Контекст (не дублируем): зрение ботов — `docs/research/perception.md`; слух — `perception.md`
(«Слух») и `noise-*.md`. Здесь — только свет.

---

## 1. Ключевой вывод: единый детект «включён» = `GetCompEM().IsWorking()`

У **всех** источников света ванили (кроме NV-оптики) состояние «горит/светит» держит
**энергоменеджер** (`ComponentEnergyManager`), а не отдельные поля. Канонический детект:

```
EntityAI item = ...;
bool on = item.HasEnergyManager() && item.GetCompEM() && item.GetCompEM().IsWorking();
```

`IsWorking()` (`m_IsWorking`) — это «реально работает прямо сейчас» (включён И есть энергия,
не разрушен, не мокрый). **Сервер-авторитетно** (см. §3) — именно его и нужно читать в
серверном кэше `modded_PlayerBase`, без клиентского round-trip.

Важно различать три состояния EM (это меняет семантику):

| Метод | строка | Семантика |
|---|---|---|
| `IsWorking()` | componentenergymanager.c:938 | **излучает сейчас** (switch on + энергия + не мокрый + не ruined). Это наш детект. |
| `IsSwitchedOn()` | :897 | позиция тумблера (может быть ON, но батарея села → не работает) |
| `CanWork(test_energy=-1)` | :781 | «мог бы работать» (энергия/мокрота/ruined), не глядя на тумблер |

Для факела/химсвета/roadflare `IsWorking()==false` также ловит «выгорел» (энергия исчерпана →
`OnWorkStop`).

---

## 2. Энергоменеджер — точные сигнатуры

Файл `3_game/tools/component/componentenergymanager.c`:

- `class ComponentEnergyManager : Component` (:18).
- Доступ: `EntityAI.GetCompEM()` → `ComponentEnergyManager` (`3_game/entities/entityai.c:3410`,
  возвращает `NULL`, если у сущности нет компонента `COMP_TYPE_ENERGY_MANAGER`); есть кэш-поле
  `m_EM` (:3413). `EntityAI.HasEnergyManager()` → bool (:3419).
- `void SwitchOn()` (:384) / `void SwitchOff()` (:415) — серверная ветка `if (g_Game.IsServer() || !g_Game.IsMultiplayer())`.
- `bool CanWork(float test_energy = -1)` (:781); `bool CheckWetness()` (:867);
  `bool IsSwitchedOn()` (:897); `bool IsWorking()` (:938); `bool HasEnoughStoredEnergy()` (:944).
- `float GetEnergy0To1()` (:1224); `GetUpdateInterval()` (:1235); `GetWetnessExposure()` (:1247);
  `GetEnergyUsage()` (:1253); `GetEnergy()` (:1259); `GetEnergyMax()` (:1297);
  `GetEnergySource()` (:1337) (батарея/источник питания).

**Почему `IsWorking()` сервер-валиден** (важно для нашего серверного кэша):

- `DeviceUpdate()` (:1811) — тикает по таймеру (`m_UpdateTimer.Run(GetUpdateInterval(), ...)`,
  :1518) и вызывается сразу из `SwitchOn()` (:395) / `SwitchOff()` (:429).
- Внутри: потребление энергии `ConsumeEnergy(consume_energy)` гейтится
  `if (g_Game.IsServer() || !g_Game.IsMultiplayer())` (:1857); затем `SetPowered(has_consumed_enough)`
  (:1860) → `SetPowered(bool state)` ставит `m_IsWorking = state` (:1743).
- Т.е. `m_IsWorking` вычисляется на сервере (авторитетно) и обновляется **мгновенно** при
  включении/выключении (`SwitchOn`/`SwitchOff` зовут `DeviceUpdate` синхронно); только плавное
  выгорание энергии происходит по таймеру `updateInterval`.

`[нужно подтвердить]` эмпирически: `GetCompEM().IsWorking()` на выделенном сервере = true сразу
после `SwitchOn()` игроком (фонарь/налобник/факел) и = false после `SwitchOff()`/выгорания.
Probe: включить источник игроком → лог/botdump `IsWorking=true`; выключить/дождаться разряда →
`false`.

---

## 3. Классы источников света (ваниль, `4_world/entities/itembase/`)

| Класс | extends | Файл:строка | Слот | Детект «включён» |
|---|---|---|---|---|
| `Flashlight` (ручной) | `ItemBase` | flashlight.c:1 | руки (HANDS, `ActionTurnOnWhileInHands`) | `GetCompEM().IsWorking()` |
| `Headtorch_ColorBase` (налобник) | `Clothing` | clothing/headtorch_colorbase.c:1 | `Eyewear` (headstrap) | `GetCompEM().IsWorking()` |
| `FlammableBase` (факел, `Torch`) | `ItemBase` | torch.c:1 | руки (или земля, когда горит) | `IsIgnited()` = `GetCompEM().IsWorking()` |
| `Chemlight_ColorBase` (химсвет) | `ItemBase` | chemlight.c:1 | руки/карго | `GetCompEM().IsWorking()` (одноразовый) |
| `Roadflare` | `ItemBase` | roadflare.c:24 | руки/земля | `GetCompEM().IsWorking()` (+ `m_BurningState`) |
| `Switchable_Base` | `ItemBase` | switchable_base.c:1 | — (базовый) | `GetCompEM().IsWorking()` |
| `TLRLight` (пистолетный фонарь) | `Switchable_Base` | switchable_base/tlrlight.c:1 | `pistolFlashlight` | `GetCompEM().IsWorking()` |
| `UniversalLight` (насадка) | `Switchable_Base` | switchable_base/universallight.c:1 | `weaponFlashlight` (винтовка), `helmetFlashlight` (Mich2001Helmet) | `GetCompEM().IsWorking()` |
| `NVGoggles` | `PoweredOptic_Base` | nvgoggles.c:1 | `NVG` | НЕ эмиттер; см. §5 |
| NV-оптика (`KazuarOptic`,`StarlightOptic`) | `ItemOptics` | inventory_base/optics.c:3 | слот оптики на оружии | НЕ эмиттер; см. §5 |

### Детали по классам

- **`Flashlight`** (ручной): `OnWorkStart/OnWork/OnWorkStop` (flashlight.c) — `m_Light` создаётся
  **только на клиенте** (`if (!g_Game.IsServer() || !g_Game.IsMultiplayer())`), но EM-состояние —
  на сервере. **Не** override'ит `IsLightSource()` (см. §4). Свет направленный
  (`FlashlightLight : SpotLightBase`, scriptedlightbase/spotlightbase/flashlightlight.c:1).
- **`Headtorch_ColorBase`**: override'ит `IsLightSource()→true` и `GetLightSourceItem()→this`.
  `CheckParent()` (headtorch_colorbase.c:37) — серверный таймер: если надет на зомби, сам
  создаёт `Battery9V` и `GetCompEM().SwitchOn()` (значит, `SwitchOn()` на сервере штатно).
- **`FlammableBase`** (факел): `bool IsIgnited() { return GetCompEM().IsWorking(); }` (torch.c).
  `OnIgnitedThis()` → `GetCompEM().SwitchOn()`; `OnSwitchOn/Off` включает/выключает
  `UniversalTemperatureSource`. **Горящий факел нельзя в карго**:
  `CanBeTakenAsCargo()`/`CanPutAsAttachment()` = `!GetCompEM().IsWorking()` (torch.c).
  `OnItemInHandsPlayerSwimStart` → `SwitchOff()`.
- **`Chemlight_ColorBase`**: одноразовый — `OnWorkStop()` при исчерпании энергии делает
  `SetHealth(0)` (chemlight.c). Нет `SwitchOff`-эксшна (`SetActions` добавляет только
  `ActionTurnOnWhileInHands`). Эффективность затухания — `m_Efficiency0To10` (net-sync),
  `GetEfficiency0To1()`.
- **`Roadflare`**: `enum RoadflareBurningState { NOT_BURNING, INITIAL_BURN, MAIN_BURN,
  FINAL_BURN, SMOKE_ONLY }` (roadflare.c:5); `m_BurningState` — `RegisterNetSyncVariableInt`
  (:59). Энергия = «время горения»; `OnWork` переключает состояния по
  `GetEnergyMax() - GetEnergy()`. `CanIgniteItem()` = `GetCompEM().IsWorking()` (:276).
  Для детекта «светится» достаточно `IsWorking()`; `m_BurningState` нужен, только если
  различать фазы (final-burn тусклее).

### Слоты (где искать источник на игроке)

- `weaponFlashlight` (Rifle_Base), `pistolFlashlight` (Pistol_Base) —
  `actionturnonweaponflashlight.c:27–31` (`item.FindAttachmentBySlotName(...)`).
- `helmetFlashlight` (Mich2001Helmet) — `actionturnonhelmetflashlight.c:35`,
  `helmetbase/mich2001helmet.c` (шлем создаёт `UniversalLight` для этой дырки, `OnDebugSpawn`).
  Слот-конфиг `Slot_helmetFlashlight` — config.cpp:2456.
- `Eyewear` (налобник) — `headtorch_colorbase.c` (`owner_PB.GetItemOnSlot("Eyewear")`).
- `NVG` — `Slot_NVG` (config.cpp:2451), Expansion `FindAttachmentBySlotName("NVG")`.
- Ручные (`Flashlight`/`Torch`/`Chemlight`/`Roadflare`) — `GetItemInHands()` (и, для
  включённого-на-земле факела/фонаря, отдельный edge-case, см. §6).

---

## 4. Ванильный контракт «источник света» (`IsLightSource()` и `GetLight()`)

`ItemBase` (itembase.c):

- `bool IsLightSource() { return false; }` (:951) — дефолт; override'ят → `true`:
  `Headtorch_ColorBase`, `TLRLight`, `UniversalLight` (НЕ `Flashlight`, НЕ `FlammableBase`,
  НЕ `Chemlight`, НЕ `Roadflare`).
- `ScriptedLightBase GetLight()` (:401) — возвращает клиентский light-объект (`m_Light`);
  override'ят `TLRLight`/`UniversalLight` (на `Switchable_Base`).
- `AddLightSourceItem(ItemBase)` (:4750) / `RemoveLightSourceItem()` (:4755) /
  `GetLightSourceItem()` (:4760) + поле `m_LightSourceItem` — `Switchable_Base.OnWasAttached`
  вызывает `parent.AddLightSourceItem(this)` (switchable_base.c:7), т.е. **оружие регистрирует
  надетый фонарь как свой light-source**. Это готовая ванильная «у оружия есть надетый свет».

`IsLightSource()` — это семантически «носимый/надеваемый переключаемый свет» (для quickbar:
`5_mission/gui/radialquickbarmenu.c:33,332,348`). Для нашего детекта «включён» он НЕ нужен —
только для поиска «есть ли у игрока источник» (в паре с `HasEnergyManager()`).

---

## 5. NVG и NV-оптика — это «зрение», НЕ «эмиссия»

- `NVGoggles : PoweredOptic_Base` (nvgoggles.c:1); `bool IsNVG() { return true; }` (nvgoggles.c:172).
  `m_IsLowered` (опущены на глаза); `RotateGoggles(bool state)` переключает
  `GetCompEM().SwitchOn/SwitchOff()`.
- `PoweredOptic_Base : ItemOptics` (poweredoptic_base.c:1); `ItemOptics.IsWorking()` =
  `GetCompEM() && GetCompEM().CanWork()` (core/inherited/itemoptics.c:239) — **внимание: у оптики
  `IsWorking()` использует `CanWork()`, а не EM-`IsWorking()`** (другая семантика).
- NV-оптика: `GetCurrentNVType()` (`NVTypes.NV_OPTICS_*`) override'ят `KazuarOptic`/`StarlightOptic`
  (inventory_base/optics.c:59,132). Enum `NVTypes` — dayzplayercamera_base.c:54
  (`NONE, NV_GOGGLES, NV_GOGGLES_2D, NV_GOGGLES_OFF, NV_OPTICS_ON, ...`).
- Серверные helper'ы на `PlayerBase`: `bool IsNVGWorking()` = `m_ActiveNVTypes.Count() > 0`
  (playerbase.c:9083), `bool IsNVGLowered()` (playerbase.c:9088), `int GetNVType()` (playerbase.c:9093).

Для наших методов: NVG/NV-оптика дают `Illumination` (зрение хозяину), но НЕ `Emit`
(не светят наружу). Отразить отдельной веткой.

---

## 6. Эталон Expansion — значения и разница подхода

### 6.1 Своя «ночная видимость» (`eAI_GetNightVisibility`)

База (`DayZExpansion/AI/Scripts/4_World/DayZExpansion_AI/Entities/ItemBase.c:392`):

```
float eAI_GetNightVisibility(bool checkWorking = true)
```

Значения по классам (все — `if (!checkWorking || GetCompEM().IsWorking())`):

| Источник | `eAI_GetNightVisibility` | файл (Expansion) |
|---|---|---|
| `Flashlight` (ручной) | 0.0255 | ItemBase/flashlight.c:3 |
| `Headtorch_ColorBase` | 0.0255 | ItemBase/Clothing/headtorch_colorbase.c:3 |
| `FlammableBase` (факел) | **0.03** | ItemBase/flammablebase.c:19 |
| `Roadflare` | **0.03** | ItemBase/roadflare.c:3 |
| `UniversalLight` на оружии | **0.045** (иначе 0.0255) | ItemBase/switchable_base/universallight.c:3 |
| `UniversalLight` не на оружии | 0.0255 | там же |
| `TLRLight` (пистолетный) | 0.0185 | ItemBase/switchable_base/tlrlight.c:3 |
| `Chemlight` | **0.0075** | ItemBase/chemlight.c:3 |
| `NVGoggles` (опущены + работают) | **0.35** | ItemBase/nvgoggles.c:23 |
| NV-оптика | `min(zoom×0.001, 1.0)` | ItemBase.c:392 (база, через `GetCurrentNVType()`) |

Замечания (расхождение с премисой задачи):

1. **Roadflare — яркий (0.03), как факел**, а не «низкий/≈0». Низкий только химсвет (0.0075).
2. **Самая яркая эмиссия — `UniversalLight` на оружии (0.045)**: сфокусированный прожектор
   (радиус ×1.8, угол/2 при слоте `weaponFlashlight` — `universallightlight.c:26–37`
   `UpdateLightMode("weaponFlashlight")`). Факел/roadflare — 0.03 (открытый огонь, всенаправленный).
3. Рекомендация: для `EmitLightIntensivity()` взять именно эти калиброванные Expansion-значения
   (не «факел > фонарь»), т.к. они уже отражают реальную геометрию (радиус/яркость света).

### 6.2 Регистрация активных светильников (`eAI_AddActiveVisibilityEnhancer`)

- Хук на EM-события в `ItemBase` (ItemBase.c:287–300): `OnSwitchOn` → `eAI_OnSwitchOn()` →
  `ai.eAI_AddActiveVisibilityEnhancer(this)` (если `eAI_GetNightVisibility() > 0`); `OnSwitchOff`
  → `eAI_RemoveActiveVisibilityEnhancer(this)`. У `FlammableBase` ваниль НЕ зовёт `super`, поэтому
  там отдельный override (flammablebase.c:3–17).
- `eAI_UpdateNightVisibility()` (eAIBase.c:9275): `m_eAI_NightVisibility = max(eAI_GetNightVisibility)`
  по активным энхансерам; `m_eAI_IsLit = true`, если есть НЕ-оптический энхансер (т.е. любой свет).
- Ночью ИИ сам включает источники: `Expansion_TryTurningOnAnyLightsOrNVGEx()` (eAIBase.c:9190–9222)
  перебирает items через `Expansion_GetAnyLightOrNVG()` (ItemBase.c:345, список типов
  `{Flashlight, Headtorch_ColorBase, TLRLight, FlammableBase, UniversalLight}` + `NVG`),
  зовёт `Expansion_TryTurningOn()` (`GetCompEM().CanSwitchOn() && CanWork()` → `SwitchOn()`).

### 6.3 «Подсвеченный» игрок (`m_eAI_IsLit`) — КЛИЕНТСКАЯ регистрация + RPC

Это второй механизм (не «свой свет», а «стою в ЧУЖОМ свете»):

- `ScriptedLightBase.s_eAI_LightNodes` — клиентский реестр всех light-узлов
  (scriptedlightbase.c:9, весь файл под `#ifndef SERVER`); каждый light при создании
  `s_eAI_LightNodes.Add(this)` (:11), при удалении `Remove` (:31).
- `PlayerBase.eAI_ProcessLights()` (PlayerBase.c:98) — **клиентский** `CommandHandler` каждый кадр:
  если ночь и игрок не «подсвечен», прогоняет `s_eAI_LightNodes.Each(m_eAI_AddLightIfPlayerInLight, 30)`;
  `eAI_AddLightIfPlayerInLight` (PlayerBase.c:32) проверяет `eAI_IsVisible(true)` +
  `eAI_IsLightOnPlayer(this)` (свет на самом игроке) **или** `eAI_IsPlayerInLight(this)` (игрок в
  радиусе + raycast `ObjIntersectGeom` на unobstructed, scriptedlightbase.c:89–106).
- Если `m_eAI_IsLit` изменился → RPC `RPC_eAI_SetLit` (PlayerBase.c:126–133) на сервер;
  сервер хранит `m_eAI_IsLit` (OnStoreLoad :364, геттер `eAI_IsLit()` :374) и использует в
  `eAI_GetVisibility`/`eAI_GetVisibilityDistance`/`eAI_GetVisibilityDistanceLimit`
  (eAIBase.c:9459–9495): подсвеченного игрока видно дальше (до 250 м при ночи).

### 6.4 Разница подхода (важно для нас)

- **Expansion «подсвечен чужим светом» = клиентская регистрация + RPC на сервер**, потому что
  геометрия/радиус/видимость света известны только клиенту (light-сущности клиентские).
- **Наш `EmitLightIntensivity` (СВОЙ источник) можно делать ЧИСТО на сервере** через
  `GetCompEM().IsWorking()` (§2): EM сервер-авторитетен, `SwitchOn/Off` реплицируются, `IsWorking`
  вычисляется сервером. Это дешевле и проще (кэш 3 с в `modded_PlayerBase`, без RPC).
- Оставшийся серверный пробел — только «стоит ли игрок в ЧУЖОМ свете» (костёр/прожектор/другой
  игрок). Его в рамках этой фичи НЕ делаем (отдельная задача, если понадобится — по образцу
  Expansion-клиентского light-nodes, либо серверная аппроксимация по `FireplaceBase`/`Roadflare`
  с радиусом).

---

## 7. Таблица классификации (для `EmitLightIntensivity` / `IlluminationIntensivity`)

| Источник | Направленность | `Emit` (Expansion) | `Illumination` (даёт зрение хозяину) | Детект «включён» (сервер) |
|---|---|---|---|---|
| `UniversalLight` на оружии | **направленный** (зависит от `IsRaised()`) | 0.045 | да | `GetCompEM().IsWorking()` |
| `TLRLight` (пистолет) | **направленный** (зависит от `IsRaised()`) | 0.0185 | да | `GetCompEM().IsWorking()` |
| `Flashlight` (ручной) | направленный (вперёд, но НЕ гейтится `IsRaised()`) | 0.0255 | да | `GetCompEM().IsWorking()` |
| `Headtorch_ColorBase` | направленный (следует за головой) | 0.0255 | да | `GetCompEM().IsWorking()` |
| `FlammableBase` (факел) | всенаправленный | 0.03 | да | `IsIgnited()` = `IsWorking()` |
| `Roadflare` | всенаправленный | 0.03 | да | `GetCompEM().IsWorking()` |
| `Chemlight` | всенаправленный | 0.0075 | нет (слишком слабый) | `GetCompEM().IsWorking()` |
| `NVGoggles` | — (не светит) | **0 (не эмиттер)** | **да (0.35)** | `IsWorking()` + `m_IsLowered` |
| NV-оптика | — (не светит) | **0 (не эмиттер)** | да (`zoom×0.001`) | `GetCurrentNVType() != NVTypes.NONE` + `IsWorking()` |

Направленность:
- **Гейт `IsRaised()`** — только для насадок на оружие (`UniversalLight`/`TLRLight`):
  `DayZPlayerImplement.IsRaised()` → `m_IsRaised` (dayzplayerimplement.c:3861);
  натив `Human.IsWeaponRaised()` (human.c:83). Expansion `IsRaised()` **НЕ** проверяет
  (упрощение: свет на оружии считает «видимым» всегда, когда `IsWorking()`).
  `[нужно подтвердить]` `m_IsRaised` валиден на сервере для игрока/бота и корректно = true
  только при поднятом оружии.
- **Ручной фонарь** — направленный (`FlashlightLight : SpotLightBase`), но НЕ гейтится
  `IsRaised()`: светит «вперёд от модели» пока в руках; при дропе остаётся включённым и лежит
  (см. §6 открытых вопросов).

---

## 8. Открытые вопросы / [нужно подтвердить]

1. **[нужно подтвердить]** `GetCompEM().IsWorking()` на выделенном сервере = true сразу после
   `SwitchOn()` игроком и = false после `SwitchOff()`/выгорания для всех 8 классов. Probe: включить
   источник игроком → `botdump`/лог `IsWorking=true`; выключить/разрядить → `false`.
2. **[нужно проверить]** Гейтить ли направленные источники (`UniversalLight`/`TLRLight`) по
   `IsRaised()`: Expansion НЕ гейтит. Решение влияет на `Emit` (свет при опущенном оружии почти
   не виден противнику). Probe: `m_IsRaised` на сервере для бота/игрока при поднятом/опущенном оружии.
3. **[нужно подтвердить]** Ручной фонарь/факел: детект «в руках» — `GetItemInHands()`. Горящий
   факел в карго невозможен (`CanBeTakenAsCargo()` = `!IsWorking()`), а **включённый фонарь при
   дропе остаётся включённым** (ваниль не гасит в `OnInventoryExit`, только поворачивает) — т.е.
   включённый фонарь может лежать на земле и светить ≠ «виден игрок». Уточнить семантику `Emit`:
   «источник у игрока (в руках/надет)» vs «любой включённый источник, включая лежащий на земле».
4. **[нужно подтвердить]** `ItemOptics.IsWorking()` = `CanWork()` (не EM-`IsWorking()`) — для
   NV-оптики детект «работает» отличается от остальных; решить, достаточно ли `CanWork()` для
   `Illumination` NV-оптики или нужен `GetCurrentNVType() != NVTypes.NONE`.
5. **[нужно проверить]** `IsRaised()` не применим к налобнику (он «постоянный», следует за
   головой) — подтвердить, что для налобника Emit = const, не зависит от позы/прицела.
6. **[нужно подтвердить]** Химсвет в `Illumination`: Expansion трактует его как light (0.0075),
   но задача просит «нет (слишком слабый)». Решение: порог для бинарного `Illumination` —
   либо `> 0` (тогда химсвет = да), либо порог ≥ 0.02 (тогда химсвет = нет). Вынести порог в константу.

---

## Источники

Ваниль (`DayZ Projects/scripts`):
- `3_game/tools/component/componentenergymanager.c` (:18, :384, :415, :781, :867, :897, :938,
  :944, :1224, :1235, :1259, :1337, :1741–1743, :1811–1900)
- `3_game/entities/entityai.c` (:3410 `GetCompEM`, :3419 `HasEnergyManager`)
- `4_world/entities/itembase.c` (:401 `GetLight`, :932 `IsNVG`, :951 `IsLightSource`,
  :4750/4755/4760 `Add/Remove/GetLightSourceItem`)
- `4_world/entities/core/inherited/itemoptics.c` (:1, :239 `IsWorking`)
- `4_world/entities/itembase/flashlight.c`; `clothing/headtorch_colorbase.c`; `torch.c`;
  `chemlight.c`; `roadflare.c`; `switchable_base.c`; `switchable_base/tlrlight.c`;
  `switchable_base/universallight.c`; `nvgoggles.c`; `poweredoptic_base.c`;
  `inventory_base/optics.c` (:59, :132)
- `4_world/entities/itembase/clothing/helmetbase/mich2001helmet.c`
- `4_world/entities/manbase/playerbase.c` (:9083 `IsNVGWorking`, :9088 `IsNVGLowered`, :9093 `GetNVType`)
- `4_world/entities/manbase/dayzplayer/dayzplayercamera_base.c` (:54 `enum NVTypes`)
- `4_world/entities/dayzplayerimplement.c` (:3861 `IsRaised`); `3_game/human.c` (:83 `IsWeaponRaised`)
- `4_world/entities/core/inherited/weapon.c` (:6 `IsWeapon`)
- `4_world/entities/scriptedlightbase/spotlightbase/universallightlight.c` (:1–37 `UpdateLightMode`)
- `4_world/classes/useractionscomponent/actions/singleuse/actionturnonweaponflashlight.c` (:27–31, :45–51),
  `actionturnonhelmetflashlight.c` (:35), `actionturnonheadtorch.c` (:31)
- `config.cpp` (:2451 `Slot_NVG`, :2456 `Slot_helmetFlashlight`)
- `5_mission/gui/radialquickbarmenu.c` (:33, :332, :348) — потребитель `IsLightSource()`

Expansion (`DayZ-Expansion-Scripts/DayZExpansion/AI/Scripts/4_World/DayZExpansion_AI/Entities/`):
- `ItemBase.c` (:287–300 `eAI_OnSwitchOn/Off`, :345 `Expansion_GetAnyLightOrNVG`, :392 `eAI_GetNightVisibility`)
- `ItemBase/flashlight.c`, `ItemBase/Clothing/headtorch_colorbase.c`, `ItemBase/flammablebase.c`,
  `ItemBase/chemlight.c`, `ItemBase/roadflare.c`, `ItemBase/switchable_base/universallight.c`,
  `ItemBase/switchable_base/tlrlight.c`, `ItemBase/nvgoggles.c` (значения §6.1)
- `AI/eAIBase.c` (:362 `m_eAI_NightVisibility`, :9190–9246 `Expansion_TryTurningOnAnyLightsOrNVGEx`,
  :9249/9258 `eAI_Add/RemoveActiveVisibilityEnhancer`, :9275 `eAI_UpdateNightVisibility`,
  :9459–9495 использование `m_eAI_IsLit`)
- `PlayerBase.c` (:14 `m_eAI_IsLit`, :32 `eAI_AddLightIfPlayerInLight`, :98 `eAI_ProcessLights`,
  :126–133 RPC `RPC_eAI_SetLit`, :364/374 серверное хранение/геттер)
- `scriptedlightbase.c` (:9 `s_eAI_LightNodes`, :89–106 `eAI_IsPlayerInLight`/`eAI_IsPositionInLight`)

---

## Существенные развилки / принятые решения (для docs/decisions.md)

1. **Детект «включён» — единый `GetCompEM().IsWorking()` на сервере, а не per-класс флаги.**
   Почему: у всех источников «горит» держит энергоменеджер, сервер-авторитетный (`DeviceUpdate`
   гейтится `IsServer()`, `SwitchOn/Off` зовут его синхронно); `IsIgnited()`/`m_BurningState` —
   лишь обёртки над тем же EM. Один механизм на класс задач.
2. **`Emit`/`Illumination` — чисто серверный кэш в `modded_PlayerBase`, без клиентской регистрации
   света (как у Expansion).** Почему: Expansion делает «подсвечен чужим светом» клиентски, потому
   что геометрия света — клиентская; но «СВОЙ источник» полностью сервер-авторитетен через EM,
   клиентский round-trip не нужен. «Чужой свет» — сознательно вне скоупа этой фичи.
3. **NVG/NV-оптика — только `Illumination`, не `Emit`** (зрение, не эмиссия). Эмиссию считать
   только для 7 «светящих» классов; значения интенсивности брать калиброванными Expansion
   (`eAI_GetNightVisibility`), а не премисой «факел > фонарь» (roadflare = 0.03 как факел,
   `UniversalLight` на оружии = 0.045 — самый яркий).
