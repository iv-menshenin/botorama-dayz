# Research: атрибуция «кто кинул гранату / кто активировал мину» — эталон Expansion + ваниль 1.30

Статус: **исследование** (R5 эпика «усовершенствование генератора шумов»). Дополняет
`noise-generator-traps.md` (R5) и `noise-generator-sources.md` (R1/R3). Цель — выяснить,
есть ли у Expansion или ванили 1.30 более «чистый» механизм атрибуции кидателя/жертвы
взрыва, чем запасной статик-мост botorama (`dmNoiseThrowerBridge`, паттерн `dmBallisticsBridge`).

Источники: Expansion `/home/devalio/dayz/Work/DayZ-Expansion-Scripts`, ваниль 1.2x
`/home/devalio/dayz/Work/DayZ Projects/scripts`, ваниль 1.30
`/home/devalio/dayz/Work/DayZ-Script-Diff-130/scripts`.

---

## TL;DR (главный вывод)

**Ни Expansion, ни ваниль 1.30 НЕ имеют механизма атрибуции кидателя/жертвы взрыва.**
Статик-мост botorama — единственный рабочий путь, и он совпадает с тем, чего Expansion
не хватает (Expansion сознательно эмитит взрыв как **позиционный** шум без источника).
Единственное «кто» в Explosion-пути у Expansion — `source.GetHierarchyRootPlayer()`,
которое используется чтобы **ИСКЛЮЧИТЬ** случай «взрывчатка прикреплена к игроку», а не
чтобы найти кидателя. Для брошенной гранаты `GetHierarchyRootPlayer()` уже `null`
(предмет на земле), поэтому «кто кинул» в момент взрыва нативно НЕ вытащить — нужен
биндинг на броске.

**Уточнение к предпосылке задачи:** в `DamageSystem.ExplosionDamage` второй аргумент
`null` — это **`directHitObject`** (объект прямого попадания), а НЕ слот кидателя.
Кидателя в сигнатуре `ExplosionDamage` нет вообще — он теряется раньше, в момент
отцепления предмета от рук (`LocationSyncMoveEntity` → `GetHierarchyRootPlayer()`→`null`),
а `Explode`/`ExplosionDamage`/`ExplosionEffectsEx` его никогда и не несли.

---

## Q1. Трекает ли Expansion «кто кинул гранату/взрывчатку»? — НЕТ

Grep по всему дереву Expansion `.c` на `Thrower|ThrownBy|m_Thrower|SetThrower|GetThrownBy`
— **пусто**. Ни поля, ни карты, ни метода «кидатель» нет.

Единственные «кто»-разрешения вблизи стрельбы/взрыва — через иерархию сущности:

- `ExpansionWorld.FixAIFirearmFX` (AI, 4_World) — `DayZExpansion/AI/Scripts/4_World/DayZExpansion_AI/Classes/ExpansionWorld.c:497`:
  ```
  if (source && Class.CastTo(entity, source) && Class.CastTo(ai, entity.GetHierarchyRoot()))
  ```
  → `GetHierarchyRoot()` = **стрелок** (для ПУЛИ, не гранаты).
- `eAI_OnNoiseEvent` (AI, 4_World) — `DayZExpansion/AI/Scripts/4_World/DayZExpansion_AI/Entities/AI/eAIBase.c:4124`:
  ```
  if (source) { root = source.GetHierarchyRoot(); if (root == this) return; ... }
  ```
  — здесь `source` = сущность-источник шума (оружие/пуля), root = владелец.

Для ГРАНАТЫ никакого аналога нет: шум взрыва не несёт ни `source`, ни `root`.

---

## Q2. Как Expansion атрибутирует взрыв источнику? — НИКАК (позиционный шум)

Точка потребления взрыва у Expansion — `override DayZGame.ExplosionEffectsEx`
(AI, 3_Game) — `DayZExpansion/AI/Scripts/3_Game/DayZExpansion_AI/DayZGame.c:66-74`:

```c
override void ExplosionEffectsEx(Object source, Object directHit, int componentIndex,
    float energyFactor, float explosionFactor, HitInfo hitInfo)
{
    super.ExplosionEffectsEx(...);
    if (IsServer())
        eAINoiseSystem.AddNoise(hitInfo.GetPosition(), 21.0,
            string.Format("cfgAmmo %1 NoiseExplosion", hitInfo.GetAmmoType()),
            hitInfo.GetSurfaceNoiseMultiplier(), eAINoiseType.EXPLOSION);
}
```

- `source` (граната/мина) **игнорируется** для атрибуции — берётся только
  `hitInfo.GetPosition()`/`GetAmmoType()`/`GetSurfaceNoiseMultiplier()` (все — engine-методы
  `HitInfo`/`Object`, без 4_World-кастов).
- Вызывается **позиционная** перегрузка `eAINoiseSystem.AddNoise(vector position, ...)`,
  которая шлёт `source = null` в `SI_OnNoiseAdded` (см. Q3).

`eAINoiseSystem` (3_Game) — `DayZExpansion/AI/Scripts/3_Game/DayZExpansion_AI/eAINoiseSystem.c:52`.
Три перегрузки `AddNoise` (`:70`, `:76`, `:82`); позиционная (`:82`) → `SI_OnNoiseAdded.Invoke(null, position, lifetime, params, mult)`.

**Вторая (независимая) точка** — собственный damage-слой Expansion, тоже без кидателя:
- `ItemBase.Explode` (Core, 4_World) — `DayZExpansion/Core/Scripts/4_World/DayZExpansion_Core/Entities/ItemBase.c:651`:
  ```
  override void Explode(int damageType, string ammoType = "") {
      ExpansionDamageSystem.OnBeforeExplode(this, damageType, ammoType);
      super.Explode(damageType, ammoType);
  }
  ```
- `ExpansionDamageSystem.OnBeforeExplode` (4_World) — `.../Classes/Systems/ExpansionDamageSystem.c:103`
  — кастует взрыв по своему radius/dropoff, но `source` = взрывчатка.
- `OnExplosionDamageCalculated` (`:330`) и `ItemBase.EEHitBy` (`:686`) используют
  `source.GetHierarchyRootPlayer()` **только для исключения** кейса «взрывчатка у игрока
  в руках/прикреплена» (тогда «невозможно получить позицию источника относительно цели»):
  ```
  //! Note that this only works as intended if damage source root is not a player
  if (!GetHierarchyParent() && source && !source.GetHierarchyRootPlayer())
  ```

**Вывод:** Expansion ни в шуме, ни в damage-слое НЕ знает кидателя. Взрыв — чистая позиция.

---

## Q3. Кросс-модульная граница Expansion: Game/World split + virtual dispatch (НЕ static-мост)

У Expansion та же модульная граница (`modded class` только в своём модуле), но решена
она **архитектурно**, а не static-картой по `EntityAI`:

1. `modded class DayZGame` (3_Game) держит поле `m_ExpansionGame` типа **`ExpansionGame`**
   (базовый класс 3_Game). `DayZExpansion/Core/Scripts/3_Game/DayZExpansion_Core/DayZGame.c:107`
   `SetExpansionGame(ExpansionGame game)`, `:119` `GetExpansionGame()`.
2. Иерархия моста через модули (только наследование, вниз по слоям):
   - `class ExpansionWorld: ExpansionGame` — `Core/Scripts/4_World/DayZExpansion_Core/ExpansionWorld.c:13`
   - `class DayZExpansion: ExpansionWorld` — `Core/Scripts/5_Mission/DayZExpansion_Core/DayZExpansion.c:16`
   - инстанс создаётся в 5_Mission: `GetDayZGame().SetExpansionGame(new DayZExpansion)` (`:117`).
3. 3_Game `DayZGame.ExplosionEffectsEx`/`FirearmEffects` вызывают виртуальный метод
   `ExpansionGame.FixAIFirearmFX(...)` (объявлен в 3_Game
   `AI/Scripts/3_Game/DayZExpansion_AI/ExpansionGame.c:58`), который реально диспатчится в
   **4_World** override `ExpansionWorld.FixAIFirearmFX` (`AI/.../Classes/ExpansionWorld.c:488`),
   где уже можно кастовать в 4_World-типы (`Weapon_Base`).

Правило направления зависимостей, которое из этого следует:

- **4_World → 3_Game свободно** (4_World зависит от 3_Game): `Weapon_Base.eAI_FirearmEffects`
  (4_World) зовёт статик `eAINoiseSystem.AddNoise` (3_Game) напрямую —
  `AI/.../Weapons/Firearms/Weapon_Base.c:416`; `eAIBase` (4_World) подписывается на
  `eAINoiseSystem.SI_OnNoiseAdded` (3_Game) — `eAIBase.c:563`.
- **3_Game → 4_World только через виртуальный мост** `GetExpansionGame()` (3_Game-база →
  4_World/5_Mission-подкласс). В `ExplosionEffectsEx` (3_Game) Expansion **намеренно НЕ
  кастует** `source` ни в какой 4_World-тип — только engine-методы + 3_Game-статики.

**Что это даёт botorama (развилка):** Expansion-паттерн (Game/World singleton + virtual
dispatch) — чище, чем static-карта по `EntityAI`, но:
- требует game-singleton `SetExpansionGame` и классовой цепочки 3_Game→4_World→5_Mission;
- он **решает только** «проложить 3_Game-хук в 4_World-логику», а **НЕ** «запомнить кидателя»
  (Expansion кидателя не запоминает вовсе).
- Для «запомнить сущность по ключу `EntityAI`» Expansion-аналога нет — это ровно то, что
  делает `dmBallisticsBridge`/`dmNoiseThrowerBridge`. Т.е. запасной план botorama — **не
  хуже** эталона; он закрывает то, что Expansion не закрывает.

---

## Q4. Ловушки/мины: трекает ли Expansion «кто наступил/активировал»? — НЕТ

- `modded class TrapBase` (Core, 4_World) — `DayZExpansion/Core/Scripts/4_World/DayZExpansion_Core/Entities/TrapBase.c:1`
  — только `override bool Expansion_IsDanger() { return IsActive(); }` (для threat-оценки ИИ). Жертвы нет.
- `modded class LandMineTrap` — `.../TrapBase/Trap_LandMine.c:1` — только `Expansion_IsExplosive() { return true; }`.
- `modded class TripwireTrap` — `.../TrapBase/Trap_Tripwire.c:1` — хранит `m_Expansion_AttachedExplosive`
  (прикреплённую взрывчатку, для `Expansion_GetAmmoTypes()`), **не жертву**.
- `modded class TrapBase` (AI, 4_World) — `DayZExpansion/AI/Scripts/4_World/DayZExpansion_AI/Entities/TrapBase.c:1`
  — override `EEHitBy` для **обезвреживания** мили-ударом (`Disarm()`), где `source.GetHierarchyRootPlayer()`
  **закомментирован** (`:17`) — это «кто обезвредил», не «кто наступил».

Ни `OnSteppedOn`, ни `SnapOnObject`, ни `TrapTrigger` в Expansion НЕ переопределяются.
Вывод: хук botorama `modded TrapBase.SnapOnObject(EntityAI victim)` (из `noise-generator-traps.md`)
**не имеет Expansion-эталона** — это наш собственный биндинг, и он единственный способ достать жертву.

---

## Q5. Ваниль 1.30: нативный механизм атрибуции кидателя/жертвы? — НЕТ (проверено по дереву)

Grep по 1.30 и 1.2x на `ThrownBy|SetThrownBy|GetThrownBy|m_Thrower|ThrowerPlayer` — **пусто в обеих**.

Точная цепочка (идентична 1.2x и 1.30):

1. `Object.Explode(int damageType, string ammoType = "")` — `3_game/entities/object.c:144` (обе версии):
   ```
   if (g_Game.IsServer()) {
       SynchExplosion();
       DamageSystem.ExplosionDamage(EntityAI.Cast(this), null, ammoType, GetPosition(), damageType);  // :155
   }
   ```
2. `DamageSystem.ExplosionDamage` — `3_game/damagesystem.c:25` (proto native static):
   ```
   static proto native void ExplosionDamage(EntityAI source, Object directHitObject,
       string ammoTypeName, vector worldPos, int damageType);
   ```
   → **`source` = взрывчатка (`this`), `directHitObject` = `null`** (прямого попадания нет).
   Слота кидателя в сигнатуре НЕТ. `directHitObject` — не кидатель.
3. `DayZGame.ExplosionEffectsEx` — `3_game/dayzgame.c:3470` (1.2x) / `:3600` (1.30), сигнатура та же;
   шум на сервере **позиционный**: `GetNoiseSystem().AddNoiseTarget(pos, 21, m_NoiseParams, multiplier)`.
   Отличие 1.2x→1.30: 1.2x множит на `GetWeather().GetNoiseReductionByWeather()` (`:3488`),
   1.30 — без него (нативное глушение, см. `1.30-breaking-changes.md` §R4).
4. `Object.ExplosionEffects` (legacy) зовёт `source.OnExplosionEffects(...)` **только в
   `#ifndef SERVER`** (`dayzgame.c:3495` — клиентская ветка). Т.е. на сервере per-entity
   `OnExplosionEffects` (напр. `ExplosivesBase.OnExplosionEffects`) **НЕ вызывается** —
   точка потребления для сервера одна: `DayZGame.ExplosionEffectsEx`.
5. `InventoryItem.ThrowPhysically(DayZPlayer player, vector force, bool collideWithCharacters = true)`
   — `3_game/entities/inventoryitem.c:26` (proto native). `player` = кидатель, но native,
   геттера нет, нигде не сохраняется (только физике броса).
6. `Grenade_Base` (`4_world/entities/itembase/grenade_base.c`) — поля кидателя НЕТ в обеих версиях
   (те же методы `Activate`/`InitiateExplosion`/`EEItemLocationChanged`).

**Ключевой факт «почему кидатель теряется»:** связь «предмет → кидатель» живёт только в
иерархии (`GetHierarchyRootPlayer()`), пока предмет в руках/прикреплён. `HandActionThrow.Action`
(`3_game/systems/inventory/hand_actions.c:62`) делает `LocationSyncMoveEntity` (`:73`), после чего
предмет на земле → `GetHierarchyRootPlayer() == null` (подтверждено в `decisions.md` loot: «предметы
на земле → `GetHierarchyRootPlayer()==null`»). Поэтому в момент `ExplosionEffectsEx` кидателя
нативно уже нет — нужен биндинг на броске (static-мост) или `EEItemLocationChanged`
(`oldLoc.GetParent()`, `grenade_base.c:273` — как описано в `noise-generator-sources.md` §1.3).

---

## Гипотезы для эмпирической проверки (dayz-tester)

1. **[нужно подтвердить]** В `modded DayZGame.ExplosionEffectsEx` на сервере `source` для
   подорванной мины = `LandMineTrap`, для брошенной гранаты = `Grenade_Base`, и
   `EntityAI.Cast(source).GetHierarchyRootPlayer()` в этот момент == `null` (граната/мина на
   земле). Probe: бросить гранату + подорвать мину → лог `source=<тип> rootPlayer=<null?>`.
   Это подтверждает необходимость моста (без него кидателя не достать), а не нативный путь.
2. **[нужно подтвердить]** `OnExplosionEffects` (per-entity) на **выделенном сервере** НЕ
   вызывается (ветка `#ifndef SERVER`). Probe: лог в `modded ExplosivesBase.OnExplosionEffects`
   при взрыве гранаты на headless — записей нет (в отличие от клиента).
3. **[нужно подтвердить]** Биндинг кидателя через `modded HandActionThrow.Action` виден на
   выделенном сервере (уже в `noise-generator-traps.md` §4.1/§6.1; повторять не нужно, если
   тот probe пройден) — здесь только фиксируем, что альтернативы «нативно» нет.
4. **[нужно проверить]** Для ИИ-бота-кидателя `e.m_Player` (в `HandActionThrow`) = `dmAISurvivorBase`
   и его `GetHierarchyRootPlayer()` == он сам → self-skip в `dmHearing` работает. (Совпадает с
   `noise-generator-sources.md` R1, открытый вопрос №6 — продублировать не требуется.)

---

## Существенные развилки / принятые решения (для docs/decisions.md)

1. **Атрибуция взрыва — static-мост по `EntityAI`, а не Expansion-паттерн Game/World-virtual.**
   Почему: Expansion вообще не атрибутирует взрыв (позиционный шум), а его Game/World-virtual-мост
   решает «проложить 3_Game-хук в 4_World-логику», но не «запомнить сущность по ключу». Запоминать
   связь `кидатель/жертва → взрывчатка` в 3_Game-статике по ключу `EntityAI` (паттерн
   `dmBallisticsBridge`) — единственный путь, т.к. в момент взрыва иерархия уже разорвана
   (`GetHierarchyRootPlayer()==null`), а нативного поля нет ни в 1.2x, ни в 1.30.
   Действует пока: нет нативного `m_ThrownBy`/`SetThrownBy` (1.30 не добавил); если появится —
   заменить мост на поле.
2. **Точка потребления взрыва — только `DayZGame.ExplosionEffectsEx` (server), НЕ `OnExplosionEffects`.**
   Почему: `OnExplosionEffects` на сервере не вызывается (`#ifndef SERVER`, dayzgame.c:3495),
   а `ExplosionEffectsEx` — единственный серверный хук, где `source` = взрывчатка. (Уже
   зафиксировано в `noise-generator-sources.md`; подтверждено здесь по 1.30.)
3. **Предпосылка «второй аргумент `ExplosionDamage` = место потери кидателя» уточнена.**
   Второй аргумент — `directHitObject` (объект прямого попадания), не слот кидателя; кидателя в
   сигнатуре нет вообще. Кидатель теряется раньше (отцепление от рук → `GetHierarchyRootPlayer()==null`).
