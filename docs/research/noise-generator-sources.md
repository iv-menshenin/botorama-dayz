# Research: источники генератора шумов (noise generator sources)

Статус: **исследование**. Цель — точные сигнатуры/точки хука для расширения
`dmNoiseSystem.AddNoise(source, pos, strength, type)` четырьмя классами источников:
бросок/граната (R1), экшэны игрока (R2), стрелок из `FirearmEffects` (R3), погодный
множитель дистанции (R4). Факты с `файл:строка`; неподтверждённое помечено
`[нужно проверить]`/`[нужно подтвердить]`.

Контекст (не дублируем): текущие генераторы и приёмник — `docs/research/noise-attention.md`
(§2) и `docs/research/perception.md` (раздел «Слух»). Ванильный sink `NoiseSystem.AddNoise*`
script-колбэков не даёт (подтверждено), поэтому каждый источник дублируем своим
`dmNoiseSystem`-вызовом в modded-хуке; ванильные вызовы остаются для зомби/животных.

---

## R1 — Бросок предмета / гранаты (привязка кидателя)

### 1.1 Поток броска (полная цепочка)

1. **`DayZPlayerImplementThrowing.HandleThrowing(HumanInputController pHic, HumanCommandWeapons pHcw, EntityAI pEntityInHands, float pDt)`**
   — `4_world/entities/dayzplayerimplementthrowing.c:11`.
   - Клиентская логика броска (в `DayZPlayerImplement` — см. `dayzplayerimplement.c`,
     где инстанс `DayZPlayerImplementThrowing` создаётся).
   - Событие «предмет покинул руки»: `pHcw.WasItemLeaveHandsEvent()` (`:53`, native,
     `3_game/human.c:1109` — `proto native bool WasItemLeaveHandsEvent();`).
   - Сам бросок: `m_Player.GetHumanInventory().ThrowEntity(pEntityInHands, aimOrientation.AnglesToVector(), force)` (`:68`).
   - Запуск анимации броска: `pHcw.ThrowItem(throwType)` (`:112`, native `3_game/human.c:1107`).
   - `c_fThrowingForceMin = 20.0`, `c_fThrowingForceMax = 90.0` (`:157-159`).

2. **`HumanInventory.ThrowEntity(EntityAI item, vector dir, float force)`** — `3_game/systems/inventory/humaninventory.c:164`.
   - **Готча:** `if (g_Game.IsServer() && g_Game.IsMultiplayer()) return false;` (`:166`) — на
     выделенном сервере метод НЕ выполняет бросок сам; бросок делается клиентом (predictive)
     и реплицируется через `HandEventThrow`.
   - Создаёт `HandEventThrow` (`:184`), `throwEvent.SetForce(dir * force)`, `HandEvent(invMode, throwEvent)`.
   - Ветка `HANDS` → `HandEventThrow`; иначе → `DropEntity` (`:188-190`).

3. **`class HandEventThrow extends HandEventRemove`** — `3_game/systems/inventory/hand_events.c:505`.
   - Поля: `DayZPlayer m_Player` (в базе `HandEventBase`, `:42`), `vector m_Force`.
   - `vector GetForce()` (`:552`), `void SetForce(vector force)` (`:551`).
   - `EntityAI GetSrcEntity()` (`:83`), `InventoryLocation GetSrc()` (`:80`), `GetDst()` (`:90`).
   - `m_Dst` = ground-локация, ставятся в конструкторе `SetGround(entity, mat)` (`:507-523`).

4. **`class HandActionThrow extends HandActionBase`** — `3_game/systems/inventory/hand_actions.c:62`.
   - `override void Action(HandEventBase e)` (`:63`):
     ```
     HandEventThrow throwEvent = HandEventThrow.Cast(e);          // :70
     GameInventory.LocationSyncMoveEntity(e.GetSrc(), e.GetDst()); // :78  ← предмет "отцепился"
     DayZPlayer player = DayZPlayer.Cast(e.m_Player);             // :80
     if (player.GetInstanceType() != INSTANCETYPE_REMOTE) {
         InventoryItem item = InventoryItem.Cast(e.GetSrcEntity());
         if (item) item.ThrowPhysically(player, throwEvent.GetForce());  // :81 ← стал брошенной сущностью
     }
     player.OnItemInHandsChanged();                                // :86
     ```
   - **Это единственная авторитетная точка «предмет стал брошенным».** Здесь есть и
     кидатель (`e.m_Player`), и предмет (`e.GetSrcEntity()`), и сила (`throwEvent.GetForce()`).
   - Инстанциируется напрямую `new HandActionThrow` в FSM-таблице
     (`3_game/systems/inventory/humaninventorywithfsm.c:61`), поэтому `modded class HandActionThrow`
     переопределит его. `[нужно подтвердить]` — `HandActionThrow` (3_game/systems/inventory)
     модифицируем из `core/3_Game` (не engine-класс, но расположен в inventory-подсистеме; проверить, что `modded class` загружается).

5. **`InventoryItem.ThrowPhysically(DayZPlayer player, vector force, bool collideWithCharacters = true)`**
   — `3_game/entities/inventoryitem.c:26` — `proto native`, `player` = кидатель.
   - **Нельзя override** (native). Хукать только вызывающий код (`HandActionThrow.Action`).

### 1.2 Готовая «кто кинул» — НЕТ

Grep по ванили: `m_ThrownBy`/`SetThrownBy`/`GetThrownBy`/`m_Thrower`/`SetThrower` — **пусто**.
`Grenade_Base` (`4_world/entities/itembase/grenade_base.c:30`) и `ExplosivesBase`
(`4_world/entities/explosivesbase.c:19`) НЕ хранят кидателя. Связь `кидатель → граната`
надо строить самим (статик-карта `EntityAI → EntityAI`, как `dmBallisticsBridge`
`src/core/3_Game/dmBallisticsBridge.c:18-22`).

### 1.3 Граната: детект «покинул руки» и взрыв

- **`Grenade_Base.EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)`**
  — `grenade_base.c:273`:
  ```
  if (newLoc.GetType() != InventoryLocationType.HANDS && !IsPinned())
      Activate();      // :280 — запуск фитиля
  ```
  Т.е. граната активируется когда покидает руки. **В этой точке кидатель = `oldLoc.GetParent()`**
  (`EntityAI.EEItemLocationChanged`, `3_game/entities/entityai.c:962-963`: `oldOwner = oldLoc.GetParent()`).
- **`EntityAI.EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)`**
  — `entityai.c:962`; `InventoryLocation.GetParent()` — `3_game/systems/inventory/inventorylocation.c:52`
  (`proto native EntityAI GetParent()`). Для броска `oldLoc.GetType()==HANDS` → `oldLoc.GetParent()` = кидатель.
  `[нужно подтвердить]` — `EEItemLocationChanged` на сервере вызывается из
  `LocationSyncMoveEntity` (стр. 78 выше), т.е. oldLoc.GetParent() ещё валиден.
- **Взрыв (цепочка):**
  - `Grenade_Base.InitiateExplosion()` (`grenade_base.c:151`) → `Explode(DamageType.EXPLOSION, m_AmmoTypes[i])`.
  - `ExplosivesBase.InitiateExplosion()` (`explosivesbase.c:198`) → `Explode(...)` (`:203`) → `OnExplode()` (`:209`).
  - `ExplosivesBase.EEKilled(Object killer)` (`explosivesbase.c:100`) → `InitiateExplosion()` (`:105`).
    `killer` — НЕ кидатель (для фитиля `OnActivateFinished` → `SetHealth("","",0.0)` (`grenade_base.c:201`),
    `killer` скорее null/граната), поэтому кидателя через `EEKilled` не достать.
  - `Object.Explode(int damageType, string ammoType = "")` (`3_game/entities/object.c:144`) →
    `DamageSystem.ExplosionDamage(EntityAI.Cast(this), null, ammoType, GetPosition(), damageType)` (`:156`)
    — **второй аргумент `null`**, кидатель не пробрасывается.
  - `DayZGame.ExplosionEffectsEx(Object source, Object directHit, int componentIndex, float energyFactor, float explosionFactor, HitInfo hitInfo)`
    — `3_game/dayzgame.c:3470`. `source` = граната. Шум: `GetNoiseSystem().AddNoiseTarget(pos, 21, m_NoiseParams, multiplier * GetWeather().GetNoiseReductionByWeather())` (`:3488`).
  - `OnExplosionEffects(...)` — `Object.OnExplosionEffects` (`object.c:189`, пустой),
    `EntityAI.OnExplosionEffects` (`entityai.c:943`), `ExplosivesBase.OnExplosionEffects` (`explosivesbase.c:64`),
    `smokegrenadebase.c:240`/`flashgrenade.c:5` (переопределяют). Per-entity, moddable.

### 1.4 Вывод по R1 (точки хука)

- **Бинд кидателя → предмет** (рекомендуется): `modded class HandActionThrow` (override `Action`) —
  ловит И бросок, И кидателя, И силу; в статик-карту `Thrower[item] = player`. Гейт `#ifdef SERVER`.
  Фолбэк/альтернатива (без различения throw vs drop): `modded class ItemBase`/`modded class Grenade_Base`
  override `EEItemLocationChanged` → при `oldLoc.GetType()==HANDS` читать `oldLoc.GetParent()`.
- **Для гранаты** — `modded class Grenade_Base.EEItemLocationChanged` (уже есть, `grenade_base.c:273`)
  — поймать `oldLoc.GetParent()` как кидателя.
- **Потребление (взрыв)** — `modded class DayZGame.ExplosionEffectsEx` (`core/3_Game/modded/modded_DayZGame.c`,
  сейчас там только `FirearmEffects`) → `source` = граната, резолв кидателя из карты, шлём
  `dmNoiseSystem.AddNoise(thrower ?? source, pos, DM_NOISE_EXPLOSION_STRENGTH, EXPLOSION)`.
  **Типа `EXPLOSION` в `dmNoiseType` сейчас НЕТ** (см. `noise-attention.md` §2) — добавить.

---

## R2 — Хуки экшэнов игрока

### 2.1 Общий жизненный цикл action (`actionbase.c`, `actioncontinuousbase.c`)

- `class ActionData` (`4_world/classes/useractionscomponent/actionbase.c:28`):
  `ref ActionBase m_Action`, `ItemBase m_MainItem`, `ActionBaseCB m_Callback`,
  `ref CABase m_ActionComponent`, `int m_State`, `ref ActionTarget m_Target`,
  `PlayerBase m_Player` (`:43`), `int m_PossibleStanceMask`, `ref array<ref InventoryLocation> m_ReservedInventoryLocations`, … .
- `class ActionBase : ActionBase_Basic` (`actionbase.c:58`). Серверные/общие хуки:
  - `void OnUpdateServer(ActionData action_data)` (`:1267`)
  - `void OnStart(ActionData)` (`:1271`), `OnStartClient` (`:1277`), `OnStartServer` (`:1280`)
  - `void OnEnd(ActionData)` (`:1285`), `OnEndClient` (`:1290`), `OnEndServer` (`:1293`)
- `class ActionContinuousBase : AnimatedActionBase` (`actions/actioncontinuousbase.c:120`):
  - `void OnFinishProgress(ActionData action_data)` (`:243`) → сервер: `OnFinishProgressServer(action_data)`; клиент: `OnFinishProgressClient`.
  - `protected void OnFinishProgressServer(ActionData action_data)` (`:283`) — **главный хук «непрерывный экшн завершён»**.
  - `protected void OnFinishProgressClient(ActionData)` (`:287`).
  - `OnStartAnimationLoop`/`OnEndAnimationLoop` (`:178`/`:198`) — старт/конец основной анимационной петли.

### 2.2 Двери — подтверждено

- **`ActionOpenDoors.OnEndServer(ActionData action_data)`** — `actions/interact/actionopendoors.c:66`.
  - `m_NoisePar.LoadFromPath("CfgVehicles SurvivorBase NoiseActionDefault")` (`:71-72`).
  - `noise.AddNoisePos(action_data.m_Player, action_data.m_Target.GetObject().GetPosition(), m_NoisePar, NoiseAIEvaluate.GetNoiseReduction(g_Game.GetWeather()))` (`:81`).
  - Позиция двери = `action_data.m_Target.GetObject().GetPosition()` (это `Building`, не сама дверь);
    индекс двери = `action_data.m_Target.GetComponentIndex()` (см. `OnStartServer:58`).
  - `ActionLockedDoors : ActionOpenDoors` (`:89`) — `CreatesNoise() == false` (`:96`) — бесшумная запертая дверь.
- **`ActionCloseDoors.OnEndServer(ActionData action_data)`** — `actionclosedoors.c:63`; шум `:70`.
- Заборы: **`ActionOpenFence.OnEndServer`** — `actionopenfence.c:56` (шум `:60`), **`ActionCloseFence.OnEndServer`** — `actionclosefence.c:56` (шум `:60`).
- **Хук для нас:** `modded class ActionOpenDoors`/`ActionCloseDoors`/`ActionOpenFence`/`ActionCloseFence`
  override `OnEndServer` → `dmNoiseSystem.AddNoise(action_data.m_Player, action_data.m_Target.GetObject().GetPosition(), DM_NOISE_DOOR_STRENGTH, SOUND)`.
  (Тип `SOUND` в `dmNoiseType` сейчас нет — есть `OTHER`; либо добавить `SOUND`.)
  `[нужно подтвердить]` — `OnEndServer` вызывается на сервере и для AI-бота (у бота action-manager'а нет, но
  если человек открывает дверь — да); для «бот слышит человека» источник всегда человек.

### 2.3 Перезарядка (weapon FSM) — подтверждено

Классы-состояния оружия: `4_world/entities/firearms/fsm/states/*.c`, базовый
`WeaponStateBase` (`states/weaponstatebase.c`): поле `Weapon_Base m_weapon` (`:12`),
хуки `void OnEntry(WeaponEventBase e)` (`:102`), `OnExit` (`:142`), `OnAbort` (`:127`), `OnUpdate(float dt)` (`:117`).
`class WeaponEventBase` (`fsm/events.c:25`): `int m_eventID`, `DayZPlayer m_player`, `Magazine m_magazine`.
`enum WeaponEventID` (`events.c:4`): `LOAD1_BULLET`, `CONTINUOUS_LOADBULLET_START/END`, `UNJAM`,
`ATTACH_MAGAZINE`, `DETACH_MAGAZINE`, `SWAP_MAGAZINE`, `RELOAD_TIMEOUT`, … .

Состояния перезарядки (все `OnEntry(WeaponEventBase e)`; `e.m_player` = игрок, `m_weapon` = оружие):

| Состояние | Файл:строка | Триггер |
|---|---|---|
| `WeaponChambering` (зарядить 1 патрон) | `weaponchambering.c:543`, `OnEntry:592` | `WeaponEventLoad1Bullet` |
| `ChamberMultiBullet` (зарядить N) | `weaponchambering.c:799`, `OnEntry:861` | `WeaponEventLoad1Bullet`/`ContinuousLoadBulletStart` |
| `WeaponChambering_Base` | `weaponchambering.c:21` | (базовый, `DropBullet:119` уже modded — `reg/4_World/modded_WeaponChambering.c`) |
| `WeaponAttachMagazine` (вставить магазин) | `weaponattachmagazine.c:114`, `OnEntry:163` | `WeaponEventAttachMagazine`/`SwapMagazine` |
| `WeaponReplacingMagAndChamberNext` (смена магазина) | `weaponreplacingmagandchambernext.c:338`, `OnEntry:406` | `WeaponEventSwapMagazine` |
| `WeaponChamberFromAttMag` (дослать из магазина) | `weaponchamberfromattmag.c:3`, `OnEntry:8` | |
| `WeaponRechamber` (передёрнуть затвор) | `weaponrechamber.c:3`, `OnEntry:35` | |

Запуск FSM-событий: `WeaponManager.StartPendingAction()` (`4_world/classes/weapons/weaponmanager.c:810`)
постит `WeaponEventAttachMagazine/SwapMagazine/DetachMagazine/Load1Bullet/ContinuousLoadBulletEnd`
(`:821/:826/:832/:838/:843/:848`) через `m_player.GetDayZPlayerInventory().PostWeaponEvent(...)`.
(Заполнение патронами магазина — отдельный экшн `ActionLoadMagazine` `actions/continuous/actionloadmagazine.c:9`,
`OnExecuteServer:47` — это не перезарядка оружия, а зарядка магазина патронами.)

- **Хук для нас:** `modded class WeaponChambering` / `ChamberMultiBullet` / `WeaponAttachMagazine` /
  `WeaponReplacingMagAndChamberNext` / `WeaponRechamber` override `OnEntry` →
  `dmNoiseSystem.AddNoise(e.m_player, e.m_player.GetPosition(), DM_NOISE_RELOAD_STRENGTH, SOUND)`.
  `[нужно проверить]` — какой именно набор состояний покрывает все типы оружия (болтовки/PП/магнум) без
  дублей на один цикл перезарядки (внутренние под-состояния `WeaponChambering_*` срабатывают на каждый патрон).
  Рекомендация: эмитить шум только на «внешних» состояниях (`WeaponChambering`/`ChamberMultiBullet`/
  `WeaponAttachMagazine`/`WeaponReplacingMagAndChamberNext`), не на `WeaponChambering_*` под-состояниях.

### 2.4 Бинтование — подтверждено

- `class ActionBandageBase : ActionContinuousBase` — `actions/continuous/medical/actionbandagebase.c:1`.
  - `void ApplyBandage(ItemBase item, PlayerBase player)` (`:3`) — снимает кровотечение
    (`RemoveMostSignificantBleedingSourceEx`), тратит расходник (`AddQuantity(-1)`/`Delete()`).
- `class ActionBandageSelf : ActionBandageBase` (`actionbandageself.c:15`) —
  `override void OnFinishProgressServer(ActionData action_data)` (`:35`) → `ApplyBandage(action_data.m_MainItem, target)`.
- `class ActionBandageTarget : ActionBandageBase` (`actionbandagetarget.c:15`) —
  `override void OnFinishProgressServer(ActionData action_data)` (`:50`) → `ApplyBandage(action_data.m_MainItem, target)`.
- **Хук для нас:** `modded class ActionBandageSelf`/`ActionBandageTarget` override `OnFinishProgressServer`
  → `dmNoiseSystem.AddNoise(action_data.m_Player, action_data.m_Player.GetPosition(), DM_NOISE_BANDAGE_STRENGTH, SOUND)`.
  (Аналог для других мед-экшнов — `ActionSewSelf`/`ActionSewTarget`/`ActionSplintSelf/Target` и т.п.,
  та же точка `OnFinishProgressServer`.)

### 2.5 Полный список нативных AI-шумов в ванили (`AddNoise*`)

(Чтобы при желании продублировать каждый в `dmNoiseSystem`. Источник → наш hook.)

| Источник | Файл:строка | Вызов | Наш hook (modded class) |
|---|---|---|---|
| Взрыв (граната/боеприпас) | `3_game/dayzgame.c:3488` | `AddNoiseTarget(pos,21,...)` | `DayZGame.ExplosionEffectsEx` |
| Попадание пули | `3_game/dayzgame.c:3663` | `AddNoiseTarget(pos,10,...)` | `DayZGame.FirearmEffects` (уже есть) |
| Ближний бой (удар) | `3_game/dayzgame.c:3701` | `AddNoisePos(EntityAI.Cast(source), pos,...)` | `DayZGame.CloseCombatEffects` |
| Звук животного | `3_game/entities/dayzanimal.c:338,359` | `AddNoise(this,...)` | `AnimalBase` (уже есть `modded_AnimalBase.c`) |
| Крик/рык зомби | `4_world/entities/creatures/infected/zombiebase.c:594` | `AddNoise(this,...)` | `ZombieBase.OnSoundVoiceEvent` (уже есть) |
| Шаги игрока | `4_world/entities/dayzplayerimplement.c:3207` | `AddNoise(this,...)` | `DayZPlayerImplement.OnStepEvent` (уже есть) |
| Anim-sound события | `dayzplayerimplement.c:3570` | `AddNoise(this, soundEvent.m_NoiseParams,...)` | `DayZPlayerImplement.OnSoundEvent` |
| Голос (VOIP) | `dayzplayerimplement.c:2595-2624` | `AddNoise(vonpar,...)` раз в 1 с (`m_NextVoNNoiseTime`) | `DayZPlayerImplement.CommandHandler` (см. §2.6) |
| Дымовая граната | `4_world/entities/grenade_base/smokegrenadebase.c:186,202` | `AddNoisePos(this,...)` | `SmokeGrenadeBase` |
| Костёр | `4_world/entities/itembase/fireplacebase.c:1921` | `AddNoisePos(this,...)` | `FireplaceBase` |
| Будильник | `itembase/gear/tools/alarmclock.c:140` | `AddNoiseTarget(GetPosition(), UPDATE_TICK_RATE,...)` | `AlarmClock` |
| Кухонный таймер | `itembase/gear/tools/kitchentimer.c:127` | `AddNoiseTarget(...)` | `KitchenTimer` |
| Road flare | `4_world/entities/itembase/roadflare.c:210,257` | `AddNoisePos(this,...)` | `Roadflare` |
| Ловушка (срабатывание) | `4_world/entities/itembase/trapspawnbase.c:649` | `AddNoiseTarget(m_PreyPos,5,...)` | `TrapSpawnBase` |
| Двигатель машины | `4_world/entities/vehicles/carscript.c:3091` | `AddNoiseTarget(GetPosition(),5,...)` | `CarScript` (уже есть `modded_CarScript.c`) |
| Helper | `4_world/static/miscgameplayfunctions.c:1757-1760` | `GenerateAINoiseAtPosition(pos, lifeTime, noiseParams)` → `AddNoiseTarget` | (готовый helper) |

### 2.6 Голос (VOIP) — деталь

`DayZPlayerImplement.CommandHandler(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)`
(`dayzplayerimplement.c:2271`) на сервере, внутри (`:2588-2624`):
```
float amplitude = IsPlayerSpeaking();
if (amplitude > 0.1 || force) {
    ... if (g_Game.IsServer()) {
        int now = g_Game.GetTime();
        if (now >= m_NextVoNNoiseTime) { m_NextVoNNoiseTime = now + 1000;
            int voiceLevel = g_Game.GetVoiceLevel(this);
            switch (voiceLevel) {
                case VoiceLevelWhisper: vonpar = pt.GetNoiseParamsWhisper(); break;
                case VoiceLevelTalk:    vonpar = pt.GetNoiseParamsTalk();    break;
                case VoiceLevelShout:   vonpar = pt.GetNoiseParamsShout();   break;
            }
            AddNoise(vonpar, NoiseAIEvaluate.GetNoiseReduction(g_Game.GetWeather()));
        }
    }
}
```
- `g_Game.GetVoiceLevel(Man)` — `DayZGame` (см. `dayzgame.c`, native); `IsPlayerSpeaking()` — `DayZPlayer`/`Man`.
- `GetNoiseParamsWhisper/Talk/Shout()` — `DayZPlayerType` (config).
- Хук для нас: в `modded DayZPlayerImplement.CommandHandler` (или проще — `OnSoundEvent` с
  `pEventType=="SoundVoice"`, `dayzplayerimplement.c:3377`, уже есть ветка для anim-sound).

---

## R3 — `source` в `DayZGame.FirearmEffects`

### Сигнатура (подтверждено)

`3_game/dayzgame.c:3614`:
```
void FirearmEffects(Object source, Object directHit, int componentIndex, string surface, vector pos,
    vector surfNormal, vector exitPos, vector inSpeed, vector outSpeed, bool isWater, bool deflected, string ammoType)
```
(Идентична нашему override `core/3_Game/modded/modded_DayZGame.c:8`.)

### Что такое `source` (подтверждено)

- **`source` = сущность-снаряд (пуля), НЕ стрелок.** В серверной ветке (`dayzgame.c:3641-3648`):
  `if (source && source.ShootsExplosiveAmmo() && ...)` → `DamageSystem.ExplosionDamage(EntityAI.Cast(source), null, "Explosion_40mm_Ammo", pos, ...)`.
  Т.е. `source` кастуется в `EntityAI` и является снарядом (bullet/граната-снаряд M79). `ShootsExplosiveAmmo()`
  переопределён у `M79` (`4_world/entities/firearms/launcher/m79.c:7`) и `Ammo_40mm_Explosive`
  (`itembase/magazine/ammunitionpiles.c:187`).
- **Клиент:** `source == null` (Expansion явно комментирует «source is NULL on client»,
  `DayZExpansion/AI/.../Classes/ExpansionWorld.c:494`). На сервере `source` — валидный снаряд.
- **Expansion-эталон резолва стрелка** (`ExpansionWorld.c:497`):
  ```
  if (source && Class.CastTo(entity, source) && Class.CastTo(ai, entity.GetHierarchyRoot()))
  ```
  → `entity.GetHierarchyRoot()` = стрелок (корень иерархии снаряда).

### Резолв стрелка из `source` (подтверждено)

- `EntityAI.GetHierarchyRoot()` → `EntityAI` (`3_game/entities/entityai.c:874`, `proto native`).
- `EntityAI.GetHierarchyRootPlayer()` → `Man` (`entityai.c:877`, `proto native`; null если корень не Man).
- Наш мод **уже делает это** в `dmBallisticsBridge.OnImpact` (`src/core/3_Game/dmBallisticsBridge.c:65-69`):
  `EntityAI shooter = sourceEnt.GetHierarchyRootPlayer();` — вызывается из
  `modded_DayZGame.FirearmEffects:14` (`dmBallisticsBridge.OnImpact(EntityAI.Cast(source), directHit, pos, inSpeed.Length())`).
  Т.е. эмпирически `source` ненулевой на сервере и `GetHierarchyRootPlayer()` возвращает стрелка (баллистик-обучение работает).

### Вывод по R3

**Подтверждено:** в `modded_DayZGame.FirearmEffects` можно (и нужно) резолвить стрелка из `source`
и слать его в `dmNoiseSystem` вместо `null`:
```
EntityAI shooter;
if (source) shooter = EntityAI.Cast(source).GetHierarchyRootPlayer();   // или GetHierarchyRoot()
dmNoiseSystem.AddNoise(shooter, pos, DM_NOISE_BULLETIMPACT_STRENGTH, dmNoiseType.BULLETIMPACT);
```
Сейчас `modded_DayZGame.c:13` шлёт `null` → `dmHearing.OnNoise` отсекает позиционный шум
(`dmHearing.c:39`). С `shooter` попадание пули станет «сущностным» шумом (бот узнает, ЧЬЯ пуля
упала рядом), сработает self-skip и `OnGunshot`-ветка при необходимости.
`[нужно проверить]` — для пули ИИ-бота `GetHierarchyRootPlayer()` вернёт `dmAISurvivorBase` (корень —
ИИ-пешка), тогда `dmHearing` отсечёт собственный шум (`root == m_Bot.GetPawn()`) — ок. Для стрелка в
транспорте `GetHierarchyRoot()` вернёт транспорт (не игрока) — тогда лучше `GetHierarchyRootPlayer()`.

---

## R4 — Погода: множитель дистанции

### Сигнатуры (`3_game/weather.c`)

- `Weather.GetOvercast()` → `Overcast` (`:183`), `GetFog()` → `Fog` (`:186`), `GetRain()` → `Rain` (`:189`),
  `GetSnowfall()` → `Snowfall` (`:192`), `GetWindDirection()` → `WindDirection` (`:205`),
  `GetWindMagnitude()` → `WindMagnitude` (`:210`).
- `Weather.GetWind()` → `vector` (направление + длина = скорость) (`:220`).
- `Weather.GetWindSpeed()` → `float` м/с, эквивалент `GetWindMagnitude().GetActual()` (`:243`).
- `WeatherPhenomenon.GetActual()` → `float` в `<0,1>` **кроме Wind** («does not apply for Wind, which
  always returns the magnitude values», `:35-38`); `GetForecast()` (`:41`) — «куда ползёт».
- **Fog/overcast/rain/snowfall — нормированы `GetActual()` ∈ [0,1].** `[нужно подтвердить]` точный диапазон
  `Fog.GetActual()` (0..1 по док-комменту `GetActual`; конкретный clamp — native).

### `GetNoiseReductionByWeather()` (подтверждено, обе версии)

- **База (`DayZ Projects`, 1.2x):** `3_game/weather.c:404`:
  ```
  float GetNoiseReductionByWeather() {
      float rainReduction = GetRain().GetActual() * GameConstants.RAIN_NOISE_REDUCTION_WEIGHT;
      float snowfallReduction = GetSnowfall().GetActual() * GameConstants.SNOWFALL_NOISE_REDUCTION_WEIGHT;
      if (rainReduction == 0 && snowfallReduction == 0) return 1;
      return 1 - max(rainReduction, snowfallReduction);   // берётся СИЛЬНЕЙШЕЕ, не перемножается
  }
  ```
- **Константы (`3_game/constants.c`):** `RAIN_NOISE_REDUCTION_WEIGHT = 0.5` (`:1082`),
  `SNOWFALL_NOISE_REDUCTION_WEIGHT = 0.25` (`:1083`). Комментарий (`:1076-1081`): «noise = (NoiseMultiplier - NoiseReduction) * NOISE_LEVEL_MAX»;
  rain=1 → шум режется наполовину, snow=1 → на четверть; одновременные явления НЕ стакаются.
- **1.30 (`DayZ-Script-Diff-130`):** `GetNoiseReductionByWeather()` → `[Obsolete]` (`weather.c:455`, только rain+snow),
  новый `GetNoiseReductionByWeatherEx(notnull Object object)` (`weather.c:408`) — rain+snow+**sandstorm**, берёт max reduction;
  `GetSandstormNoiseReduction(object)` (`:432`): `GetSandstorm().IsActive()` → `GetIntensityForPlayer(player)` (если Man)
  или `GetIntensity(pos)` × `SANDSTORM_NOISE_REDUCTION_WEIGHT = 0.5` (`constants.c:1145`).
  **Важно:** комментарий `[Obsolete("1.30: AI noise dampening is now done natively via AIParams config parameters")]`
  (`weather.c:407`) — в 1.30 нативный `NoiseSystem` режет шум САМ; наш кастомный `dmNoiseSystem` не использует
  нативный sink, поэтому погодный множитель применяем сами.

### Как это используется ванилью/Expansion

- Ваниль: каждый `GetNoiseSystem().AddNoise*(..., strengthMultiplier)` передаёт
  `NoiseAIEvaluate.GetNoiseReduction(g_Game.GetWeather())` (sensesaievaluate.c) или
  `g_Game.GetWeather().GetNoiseReductionByWeather()` (dayzgame.c:3488/3663/3701) как **external multiplier**.
  Т.е. `strength_final = base_strength * GetNoiseReductionByWeather()`.
- Expansion: `eAI_OnNoiseEvent` фильтр `distSq > strength²` берёт `strength = params.m_Strength * strengthMultiplier`
  (`eAIBase.c:4117`), где multiplier уже содержит погоду (в точках генерации).

### Сведение к единому `weatherFactor` (для нашего фильтра `strength *= factor`)

```
float weatherFactor = g_Game.GetWeather().GetNoiseReductionByWeather();   // 1.0 ясно, 0.5 дождь, 0.75 снег
// (1.30, опционально для sandstorm на Nasdara):
// float weatherFactor = g_Game.GetWeather().GetNoiseReductionByWeatherEx(source ?? /*pos*/);
dmNoiseSystem.AddNoise(source, pos, strength * weatherFactor, type);
```
Итог: `weatherFactor ∈ (0,1]`, умножается на `strength` в точке генерации (как ваниль передаёт
external multiplier). Тогда `dmHearing.OnNoise` фильтр `d.LengthSq() > (strength*weatherFactor)²`
автоматически сужает радиус слышимости в дождь/снег/песчаную бурю.
`[нужно подтвердить]` — `GetNoiseReductionByWeatherEx(Object)` требует `notnull Object`; для позиционных
шумов (без source) передавать `null` нельзя — использовать базовый `GetNoiseReductionByWeather()` или
обёртку с позицией. Sandstorm на Chernarus/Livonia естественно не возникает (см. `sandstorm.md` §5).

---

## Таблица «что портировать»

| Ваниль / Expansion | botorama (точка хука) | Тип шума | Примечание |
|---|---|---|---|
| `HandActionThrow.Action` (hand_actions.c:62) | `modded class HandActionThrow` (`core/3_Game/modded/`) → бинд `Thrower[item]=player` в статик-карте | — | бросок предмета (R1) |
| `Grenade_Base.EEItemLocationChanged` (grenade_base.c:273) | `modded class Grenade_Base` override → `oldLoc.GetParent()` = кидатель | — | граната (R1), бинд кидателя |
| `DayZGame.ExplosionEffectsEx` (dayzgame.c:3470) | `modded_DayZGame.ExplosionEffectsEx` → `AddNoise(thrower??source, pos, EXPLOSION)` | **`EXPLOSION` (новый)** | потребление карты кидателя |
| `ActionOpenDoors/CloseDoors/OpenFence/CloseFence.OnEndServer` | `modded class ActionOpenDoors/...` override `OnEndServer` | `SOUND` (новый) | двери/заборы (R2a) |
| `WeaponChambering/ChamberMultiBullet/WeaponAttachMagazine/WeaponReplacingMagAndChamberNext/WeaponRechamber.OnEntry` | `modded class ...` override `OnEntry` → `e.m_player` | `SOUND` | перезарядка (R2b) |
| `ActionBandageSelf/Target.OnFinishProgressServer` | `modded class ActionBandageSelf/Target` override | `SOUND` | бинтование (R2c) |
| `DayZGame.FirearmEffects` (dayzgame.c:3614) | `modded_DayZGame.FirearmEffects` → `AddNoise(EntityAI.Cast(source).GetHierarchyRootPlayer(), pos, BULLETIMPACT)` | `BULLETIMPACT` (есть) | сменить `null` → стрелок (R3) |
| `Weather.GetNoiseReductionByWeather()` (weather.c:404) | в каждой точке генерации `strength *= GetWeather().GetNoiseReductionByWeather()` | — | погодный множитель (R4) |
| `DayZGame.CloseCombatEffects` (dayzgame.c:3668) | `modded_DayZGame.CloseCombatEffects` → `AddNoise(EntityAI.Cast(source), pos, ...)` | `SOUND` | ближний бой (опц.) |
| Голос/anim-sound/костёр/будильник/roadflare/ловушка/машина (см. §2.5) | по одному `modded class` на сущность | `SOUND`/`OTHER` | опционально, по мере надобности |

**Сопутствующее:** расширить `enum dmNoiseType` (`src/core/3_Game/Perception/dmNoiseSystem.c:9`) типами
`SOUND`, `EXPLOSION` (сейчас `OTHER/STEP/SCREAM/BULLETIMPACT/SHOT`). См. `noise-attention.md` §2.

---

## Открытые вопросы / [нужно проверить] (для dayz-tester)

1. **[нужно подтвердить]** `modded class HandActionThrow` (3_game/systems/inventory) загружается и
   переопределяет `Action` на выделенном сервере. Probe: бросить предмет игроком → `botdump`/лог
   должен показать `Thrower[item]=player`. (Альтернатива без этого — `EEItemLocationChanged`.)
2. **[нужно подтвердить]** `EEItemLocationChanged` на сервере вызывается из `LocationSyncMoveEntity` и
   `oldLoc.GetParent()` ещё валиден (не null) в момент хука.
3. **[нужно подтвердить]** `source != null` в `FirearmEffects` для ванильного игрока И для ИИ-бота
   (уже косвенно подтверждено баллистик-обучением, но явного лога нет). Probe: выстрел → лог
   `FirearmEffects source=<тип снаряда> shooter=<тип>`.
4. **[нужно проверить]** `GetHierarchyRootPlayer()` на снаряде ИИ-бота возвращает `dmAISurvivorBase`
   (не null, не транспорт). И self-skip в `dmHearing` (`root == m_Bot.GetPawn()`) корректно глушит собственный шум.
5. **[нужно проверить]** Какие состояния weapon-FSM покрывают все типы оружия для «один шум на цикл
   перезарядки» без дублей (болтовки/ПП/магнум). Probe: перезарядка разных классов оружия, считать
   число `dmNoiseSystem.AddNoise` за цикл.
6. **[нужно подтвердить]** `Fog.GetActual()` и `Rain/Snowfall.GetActual()` — точный диапазон `<0,1>`;
   `GetNoiseReductionByWeatherEx(Object)` на 1.30 требует `notnull` (что передавать для позиционных шумов).
7. **[нужно проверить]** `OnEndServer` дверей вызывается и когда дверь открывает ИИ-бот (у бота нет
   action-manager'а — бот сам не дергает `ActionOpenDoors`; только для «человек открывает дверь»).
8. **[нужно подтвердить]** Наш фильтр `strength *= weatherFactor` совпадает с нативным поведением 1.30
   (где native уже режет шум) — не приведёт ли двойное ослабление к «не слышит в дождь» (ванильные
   зомби режутся нативно, наш `dmNoiseSystem` — независимо; двойного ослабления нет, но проверить).

---

## Существенные развилки / принятые решения (для docs/decisions.md)

1. **Бинд «кидатель → предмет» через статик-карту, а не через поле на предмете.**
   Почему: в `Grenade_Base`/`ExplosivesBase`/`ItemBase` НЕТ поля «кто кинул» (grep пуст), а движок не
   пробрасывает кидателя в `Explode`/`ExplosionDamage` (второй аргумент `null`, object.c:156). Решение:
   карта `EntityAI thrower[item]` в статике (образец `dmBallisticsBridge`), бинд на броске, потребление
   в `ExplosionEffectsEx`/`FirearmEffects`.
2. **`source` в `FirearmEffects` трактовать как снаряд и резолвить стрелка через `GetHierarchyRootPlayer()`.**
   Почему: ваниль использует `source` как снаряд (`ShootsExplosiveAmmo()`/`EntityAI.Cast`), а не стрелка;
   Expansion резолвит стрелка через `GetHierarchyRoot()` (ExpansionWorld.c:497). Решение: заменить `null`
   на `EntityAI.Cast(source).GetHierarchyRootPlayer()` в `modded_DayZGame.FirearmEffects`.
3. **Погодный множитель = `GetNoiseReductionByWeather()` (умножение strength), а не аддитивная поправка.**
   Почему: ваниль передаёт его как external-strength-multiplier (`strength_final = base * factor`); sandstorm
   в 1.30 — через `GetNoiseReductionByWeatherEx` (max-reduction, не стакается). Решение: единый
   `strength *= weatherFactor` в точке генерации, фильтр `dmHearing` не меняем.
