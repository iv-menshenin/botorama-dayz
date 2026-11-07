# Research: ловушки (мины/растяжки) и бросок — источники генератора шумов

Статус: **исследование** (R5 эпика «усовершенствование генератора шумов»). Дополняет
`noise-generator-sources.md` (R1 бросок, R2 экшэны, R3 стрелок, R4 погода). Факты с
`файл:строка` (ваниль 1.2x `/home/devalio/dayz/Work/DayZ Projects/scripts`); 1.30
сверен по `/home/devalio/dayz/Work/DayZ-Script-Diff-130/scripts` (реструктуризация
каталогов, семантика ключевых методов не изменилась). Неподтверждённое — маркером.

Цель: где и каким `modded class` + override ловить (а) «кто наступил/задел» мину/растяжку
(`m_TriggeredPlayer`), (б) подтвердить бросок через `HandActionThrow` + базу для
`m_ThrowerPlayer`, (в) приземление брошенного не-взрывного предмета.

---

## 1. Иерархия классов (подтверждено)

| Класс | Файл:строка | Наследует |
|---|---|---|
| `InventoryItem` | `3_game/entities/inventoryitem.c:1` | `EntityAI` |
| `ItemBase` | `4_world/entities/itembase.c:20` | `InventoryItem` |
| `ExplosivesBase` | `4_world/entities/explosivesbase.c:19` | `ItemBase` |
| `Grenade_Base` | `4_world/entities/itembase/grenade_base.c:30` | `ExplosivesBase` |
| `TrapBase` | `4_world/entities/itembase/trapbase.c:6` | `ItemBase` |
| `LandMineTrap` | `4_world/entities/itembase/trapbase/trap_landmine.c:6` | `TrapBase` |
| `TripwireTrap` | `4_world/entities/itembase/trapbase/trap_tripwire.c:9` | `TrapBase` |
| `TripwireTrapDeployed` | `trap_tripwire.c:459` | `TripwireTrap` |
| `TrapSpawnBase` | `4_world/entities/itembase/trapspawnbase.c:1` | `ItemBase` (НЕ TrapBase) |
| `TrapTrigger` | `4_world/entities/scriptedentities/triggers/traptrigger.c:2` | `Trigger` |
| `TripWireTrigger` | `traptrigger.c:68` | `TrapTrigger` |

**Важная развилка:** есть ДВА разных семейства «ловушек»:
- **`TrapBase`** — боевые ловушки (мина, растяжка, медвежий капкан-мина `trap_bear.c`).
  Имеют `m_TrapTrigger` + `OnSteppedOn(victim)`, взрываются/срабатывают от задевания.
- **`TrapSpawnBase`** — охотничьи ловушки-«силки» (капкан/сети для ловли животных,
  `m_Bait`, `EvaluateCatch`, `SpawnCatch`). НЕ имеют триггера/`OnSteppedOn`; это
  периодический roll на поимку добычи. Для «кто наступил» НЕ подходит (там нет жертвы —
  только добыча). Шум у них — `PlayCatchNoise` (`trapspawnbase.c:639` → `AddNoiseTarget`, `:649`).

Плюс отдельное семейство взрывчатки (130): `ClaymoreMine : ExplosivesBase`
(`4_world/entities/explosivesbase/claymoremine.c:1`, 130), `ImprovisedExplosive`,
`PlasticExplosive` — это **дистанционно подрываемые** (`RemotelyActivatedItemBehaviour`,
`RemotelyActivatedItemBehaviour`/`Pair`), НЕ триггерные; у них нет `OnSteppedOn`.
«Кто подорвал» у них — оператор детонатора (другой механизм, вне скоупа R5).

---

## 2. Хук «кто наступил/задел» — `OnSteppedOn(EntityAI victim)` (подтверждено)

Полная цепочка срабатывания (серверная ветка):

1. **`TrapTrigger.OnEnterServerEvent(TriggerInsider insider)`** — `traptrigger.c:31`:
   ```
   if (m_ParentObj && m_ParentObj.IsActive())
       m_ParentObj.SnapOnObject(EntityAI.Cast(insider.GetObject()));   // :39
   ```
   `insider.GetObject()` = объект, вошедший в триггер. `CanAddObjectAsInsider`
   (`traptrigger.c:26`) пропускает только `object.IsInherited(EntityAI)` → insider
   **всегда** `EntityAI` (человек, зомби, машина, предмет).
2. **`TripWireTrigger.OnEnterServerEvent`** — `traptrigger.c:70` — фильтрует: вызывает
   `super` (→ `SnapOnObject`) только если insider — `PlayerBase` и бежит/спринтует в
   полный рост (`STANCEIDX_ERECT/RAISEDERECT/RAISEDCROUCH` + `MOVEMENTIDX_RUN/SPRINT`),
   либо crouch-sprint (`:86`); НЕ-`PlayerBase` insider пропускается как есть (`:93`).
   Т.е. растяжку задевает только движущийся стоя человек (или не-игрок).
3. **`TrapBase.SnapOnObject(EntityAI victim)`** — `trapbase.c:216` (сервер). Делает
   `RefreshState()`, урон/снап, `Synch(victim)` (`:262`, RPC `RPC_TRAP_VICTIM`), и в
   конце **`OnSteppedOn(victim)`** (`:265`).
4. **`TrapBase.OnSteppedOn(EntityAI victim)`** — `trapbase.c:273`, default: `SetInactive(false)`.
   Переопределяется:
   - **`LandMineTrap.OnSteppedOn(EntityAI victim)`** — `trap_landmine.c:101`: ломает ноги/
     кровотечение/одежду, затем `Explode(DamageType.EXPLOSION)` (`:157`), `DeleteThis()` (`:160`),
     `super.OnSteppedOn(victim)` (`:163`). `victim` — тот, кто наступил (человек → `PlayerBase`,
     машина → `CarScript` (спец.ветка `OnUpdate`→`OnServerSteppedOn`, `:89/:177`), предмет → `ItemBase`).
   - **`TripwireTrap.OnSteppedOn(EntityAI victim)`** — `trap_tripwire.c:72`: `ProcessDirectDamage(CLOSE_COMBAT, this, ..., "TripWireHit", ...)` (`:87`),
     `SetState(TRIGGERED)` (`:88`), `SetInactive(false)` (`:89`). `victim` = задевший.

**Вывод по Q2:** да, `victim` (кто наступил/задел) доступен как `EntityAI` (не null) в
`OnSteppedOn(EntityAI victim)` — это серверный хук в момент срабатывания ловушки.
**`EEKilled(Object killer)` НЕ подходит** — у `ExplosivesBase.EEKilled` (`explosivesbase.c:100`)
и `LandMineTrap.EEKilled` (`trap_landmine.c:204`) `killer` = кто УНИЧТОЖИЛ взрывчатку
(пуля/нуль), а не жертва срабатывания. Для триггерных ловушек жертва только в `OnSteppedOn`.

---

## 3. `modded class` + override для `m_TriggeredPlayer` (рекомендация)

Два уровня, где ловить «armed → triggered» переход:

- **Мина** (`LandMineTrap`): отдельного «armed→triggered»-состояния НЕТ. Вооружение =
  `StartActivate(player)` (`trap_landmine.c:44`) → таймер `m_InitWaitTime` → `SetActive()`
  (`trapbase.c:404`, создаёт триггер `CreateTrigger()` `:465`). **Срабатывание = сам
  `OnSteppedOn`** (взрыв тут же, внутри метода).
- **Растяжка** (`TripwireTrap`): явный `m_State` `FOLDED(3)/DEPLOYED(2)/TRIGGERED(1)`
  (`trap_tripwire.c:12-16`). Переход **DEPLOYED→TRIGGERED = `SetState(TRIGGERED)` в
  `OnSteppedOn` (`:88`)**; также `EEHealthLevelChanged` (ruined → TRIGGERED, `:136-146`).
  Сама растяжка НЕ взрывается — `SetInactive(false)` отцепляет прикреплённую гранату и
  зовёт `attachment.OnActivatedByItem(this)` (`trap_tripwire.c:150-169`) → фитиль гранаты.

**Рекомендуемый override — `SnapOnObject` на общем предке `TrapBase`:**

```
modded class TrapBase
{
    override void SnapOnObject(EntityAI victim)
    {
        if (g_Game.IsServer() && victim)
            m_TriggeredPlayer = victim;          // EntityAI: PlayerBase/CarScript/ItemBase
        super.SnapOnObject(victim);
    }
}
```

Почему `SnapOnObject`, а не `OnSteppedOn`:
- `TrapTrigger.OnEnterServerEvent` зовёт именно `m_ParentObj.SnapOnObject(...)`
  (`traptrigger.c:39`), и `SnapOnObject` **не переопределён** ни `LandMineTrap`, ни
  `TripwireTrap` → наш `modded TrapBase.SnapOnObject` гарантированно достигается для ВСЕХ
  веток (человек / машина / предмет / растяжка).
- `OnSteppedOn` **не гарантирует** этого: `LandMineTrap.OnSteppedOn` в ветке `CarScript`
  делает `return` **без** `super.OnSteppedOn(victim)` (`trap_landmine.c:112-119`) — машина
  через `modded TrapBase.OnSteppedOn` не пройдёт (взрыв машины идёт через
  `OnServerSteppedOn`, `:177`, минуя `OnSteppedOn`).
- `SnapOnObject` вызывается строго перед `OnSteppedOn` (`trapbase.c:265`), так что
  `m_TriggeredPlayer` записан до взрыва/`Explode` (в т.ч. до deferred `OnServerSteppedOn`).
- Побочный путь `SnapOnObject` из `OnItemLocationChanged` («взял активную ловушку в руки»,
  `trapbase.c:507/511`) для мины/растяжки не срабатывает — их `OnItemLocationChanged`
  переопределён без `SnapOnObject` (`trap_landmine.c:199`, `trap_tripwire.c:99`).

`m_TriggeredPlayer` добавить полем на `TrapBase` (тип `EntityAI`, т.к. наступить/наехать
может и машина). Если нужен строго человек — фильтровать `PlayerBase.Cast(victim)`.

**Альтернатива** (точечно, если нужен только человек): `modded class LandMineTrap` и
`modded class TripwireTrap` override `OnSteppedOn` — но тогда надо дублировать в двух
классах и отдельно закрывать машинную ветку.

---

## 4. Бросок: `HandActionThrow` и база для `m_ThrowerPlayer` (подтверждено + готча)

### 4.1 `modded class HandActionThrow` — точка бинда «кто кинул»

- **`class HandActionThrow extends HandActionBase`** — `3_game/systems/inventory/hand_actions.c:62`,
  `override void Action(HandEventBase e)` (`:63`):
  ```
  HandEventThrow throwEvent = HandEventThrow.Cast(e);      // :70
  GameInventory.LocationSyncMoveEntity(e.GetSrc(), e.GetDst()); // :73  ← предмет отцепился
  DayZPlayer player = DayZPlayer.Cast(e.m_Player);          // :75
  if (player.GetInstanceType() != DayZPlayerInstanceType.INSTANCETYPE_REMOTE) {
      InventoryItem item = InventoryItem.Cast(e.GetSrcEntity());  // :78
      if (item) item.ThrowPhysically(player, throwEvent.GetForce()); // :80
  }
  player.OnItemInHandsChanged();                            // :85
  ```
- Инстанс создаётся `new HandActionThrow` в FSM-таблице `HumanInventoryWithFSM.Init`
  (`3_game/systems/inventory/humaninventorywithfsm.c:61`, transition `m_Equipped —__Tw_→ m_Empty`),
  поэтому `modded class HandActionThrow` переопределит его. `HumanInventoryWithFSM` →
  `DayZPlayerInventory` (`4_world/systems/inventory/dayzplayerinventory.c:147`) существует и
  на выделенном сервере, и на клиенте.
- **Готча (важно для Q4):** на выделенном сервере подключённый игрок имеет
  `GetInstanceType() == INSTANCETYPE_REMOTE`, поэтому `ThrowPhysically` **НЕ вызывается**
  (физический бросок делает клиент predictive, сервер реплицирует `HandEventThrow`). НО
  `LocationSyncMoveEntity` + `OnItemInHandsChanged` **выполняются и на сервере**, т.е.
  `HandActionThrow.Action` **реально бежит на выделенном сервере**, и там доступны
  `e.m_Player` (кидатель) и `e.GetSrcEntity()` (предмет). `[нужно подтвердить]` — что
  override загружается/вызывается на выделенном сервере (probe: бросок → лог
  `Thrower[item]=player`).
- `e.GetSrcEntity()` → `EntityAI` (`3_game/systems/inventory/hand_events.c:83`) = брошенный
  предмет; `e.m_Player` → `DayZPlayer` (`hand_events.c:42`). Каст в `ItemBase` корректен
  (все бросаемые предметы — `ItemBase`).

### 4.2 База для `m_ThrowerPlayer`: **`ItemBase`** (рекомендация)

- **`ItemBase` (`4_world/entities/itembase.c:20`) чище, чем `InventoryItem`
  (`3_game/entities/inventoryitem.c:1`)**:
  1. Все релевантные источники — `Grenade_Base → ExplosivesBase → ItemBase` (граната),
     `LandMineTrap/TripwireTrap → TrapBase → ItemBase` (ловушка), камень/банка — `ItemBase`.
  2. `ItemBase` — слой `4_world` (модифицируемый), рядом лежат `grenade_base.c`,
     `explosivesbase.c`, `trapbase.c`. `InventoryItem` — слой `3_game` (engine-ближе), база
     для **всех** предметов (патроны, магазины, еда, одежда) — добавлять туда поле шире и
     рискованнее.
  3. В `ExplosionEffectsEx` (`3_game/dayzgame.c:3470`) `source` = граната; чтение
     `ItemBase.Cast(source).m_ThrowerPlayer` прямое, без доп. каста.
- Тип поля: `EntityAI` (или `PlayerBase` — кидатель всегда `DayZPlayer`/`dmAISurvivorBase`);
  образец статик-карты `EntityAI → EntityAI` уже есть в `dmBallisticsBridge`
  (`src/core/3_Game/dmBallisticsBridge.c:18-22`). `[нужно проверить]` — на бросок ИИ-бота
  `e.m_Player` = `dmAISurvivorBase` (наследник `PlayerBase`) → self-skip в `dmHearing` по
  `root == m_Bot.GetPawn()` должен работать.

**Развилка (m_ThrowerPlayer vs m_TriggeredPlayer — два разных поля):**
- `m_ThrowerPlayer` (на `ItemBase`, бинд в `HandActionThrow`) — **кто кинул** гранату/предмет.
- `m_TriggeredPlayer` (на `TrapBase`, бинд в `SnapOnObject`) — **кто наступил** на мину/растяжку.
- В `ExplosionEffectsEx` `source` разный: граната → читать `ItemBase.m_ThrowerPlayer`;
  мина (`LandMineTrap`) → читать `TrapBase.m_TriggeredPlayer`. Для мины `m_ThrowerPlayer`
  не заполняется (мину НЕ бросают, а ставят — `OnPlacementComplete`, `trap_landmine.c:285`;
  `HandActionThrow` не участвует). Держать оба поля на `ItemBase` (общий предок) — тоже
  вариант, но семантически грязнее; чище развести по классам (ItemBase / TrapBase).

---

## 5. Брошенный НЕ-взрывной предмет: приземление — `EOnContact` (подтверждено)

- **`EOnContact(IEntity other, Contact extra)`** — engine-событие `EntityEvent.CONTACT`,
  объявлено `1_core/proto/enentity.c:213`; per-entity override:
  **`ItemBase.EOnContact(IEntity other, Contact extra)`** — `4_world/entities/itembase.c:1194`
  (играет impact-звук через `ProcessImpactSoundEx`, `itembase.c:1199`;
  `ProcessImpactSoundEx` — `3_game/entities/inventoryitem.c:198`).
- **`other` = то, во что ударился предмет** (земля/поверхность/объект), **НЕ кидатель**.
  Кидателя в `EOnContact` нет вообще — только `this` (сам предмет) и `other`.
- `Contact` (`1_core/physics/contact.c:9`) несёт `Position` (мировая точка контакта, `:29`),
  `RelativeVelocityBefore` (`:35`), `Impulse` (`:21`), `Normal` (`:27`) — по
  `RelativeVelocityBefore`/`Impulse` можно отличить «жёсткое приземление/бросок» от лёгкого
  касания, если нужен порог силы шума.
- **Вывод по Q5:** приземление = `modded class ItemBase` override `EOnContact` (или точечно —
  `modded class` конкретного предмета); кидатель **недоступен** в этом хуке — его надо брать
  из статик-карты `Thrower[item]`, заполненной в `HandActionThrow.Action` (§4.1), резолвить
  `this` (сам предмет) → `Thrower[this]`. `[нужно подтвердить]` — `EOnContact` на сервере
  вызывается и для брошенных предметов с физикой (probe: бросок камня → лог `EOnContact
  item=<this> other=<тип>`, наличие записи `Thrower[item]`).

---

## Таблица «что портировать» (R5)

| Источник | Ванильная точка | `modded class` + override | Тип шума | Примечание |
|---|---|---|---|---|
| Мина (наступили) | `TrapBase.SnapOnObject` (`trapbase.c:216`), `LandMineTrap.OnSteppedOn` (`trap_landmine.c:101`) | `modded class TrapBase` override `SnapOnObject` → `m_TriggeredPlayer=victim` | взрыв = `EXPLOSION` (через `ExplosionEffectsEx`, source=мина) | victim = PlayerBase/CarScript/ItemBase |
| Растяжка (задели) | `TripwireTrap.OnSteppedOn` (`trap_tripwire.c:72`) → `SetState(TRIGGERED)` | тот же `modded class TrapBase` `SnapOnObject` | триггер-звук `SOUND` (у растяжки) + взрыв прикрепл. гранаты = `EXPLOSION` (source=граната) | взрыв гранаты → `m_ThrowerPlayer`? нет — гранату «активировала» растяжка, не бросили |
| Граната (кинули) | `HandActionThrow.Action` (`hand_actions.c:63`) | `modded class HandActionThrow` → `m_ThrowerPlayer = e.m_Player` на `ItemBase` | `EXPLOSION` (через `ExplosionEffectsEx`, source=граната) | уже описано в R1 §1.4 |
| Брошенный не-взрывной предмет (приземление) | `ItemBase.EOnContact` (`itembase.c:1194`) | `modded class ItemBase` override `EOnContact` → резолв `Thrower[this]` | `SOUND`/`OTHER` (приземление) | кидатель только из статик-карты |

**Сопутствующее (из R1 §таблица):** добавить в `enum dmNoiseType` типы `SOUND`, `EXPLOSION`.

---

## Открытые вопросы / [нужно проверить]

1. **[нужно подтвердить]** `modded class HandActionThrow` (слой `3_game/systems/inventory`)
   загружается и `Action` вызывается на **выделенном сервере** (probe: игрок бросает →
   лог `Thrower[item]=player`). На сервере `GetInstanceType()==INSTANCETYPE_REMOTE`, поэтому
   `ThrowPhysically` пропускается, но `LocationSyncMoveEntity`/`OnItemInHandsChanged` идут —
   удостовериться, что наш код в override тоже бежит.
2. **[нужно подтвердить]** `m_TriggeredPlayer` (через `modded class TrapBase.SnapOnObject`)
   заполняется для всех веток: (а) человек наступил на мину (`victim = PlayerBase`),
   (б) машина наехала (`victim = CarScript`, путь `LandMineTrap.OnUpdate`→`OnServerSteppedOn`,
   `trap_landmine.c:89/177` — но `SnapOnObject(CarScript)` вызван раньше, до return в
   `OnSteppedOn`), (в) растяжку задели бегом (`TripWireTrigger` фильтр, `traptrigger.c:70`).
   probe: наступить/наехать → лог `m_TriggeredPlayer=<тип victim>` для каждого случая.
3. **[нужно проверить]** В `ExplosionEffectsEx` для мины `source` = `LandMineTrap` (каст в
   `TrapBase`/`ItemBase` для чтения `m_TriggeredPlayer`), для гранаты — `Grenade_Base`
   (каст в `ItemBase` для `m_ThrowerPlayer`). probe: подорвать мину → лог типа `source`.
4. **[нужно проверить]** Растяжка: взрыв даёт прикреплённая граната (source=граната), а
   `m_TriggeredPlayer` лежит на растяжке (не на гранате). Для шума «взрыв растяжки» нужно
   пробросить жертву с растяжки на гранату (в момент `OnActivatedByItem`/`SetInactive`,
   `trap_tripwire.c:150-169`) — отдельная точка связки, не покрыта `TrapBase.OnSteppedOn`.
5. **[нужно подтвердить]** `EOnContact` вызывается на сервере для брошенного предмета с
   физикой; `other` = поверхность/объект, `Contact.Position` = точка приземления.
6. **[нужно подтвердить]** Для ИИ-бота-кидателя `e.m_Player` = `dmAISurvivorBase`, self-skip
   в `dmHearing` по `root == m_Bot.GetPawn()` корректно глушит собственный шум броска.

---

## Существенные развилки / принятые решения (для docs/decisions.md)

1. **Два семейства ловушек: `TrapBase` (боевые, с жертвой) vs `TrapSpawnBase` (охотничьи
   силки, без жертвы).** Почему: «кто наступил» есть только у `TrapBase`-ветки
   (`OnSteppedOn(victim)`); у `TrapSpawnBase` жертвы нет (roll на поимку добычи). Решение:
   шум срабатывания боевой ловушки — через `modded class TrapBase`, силки не трогаем.
2. **Хук срабатывания = `TrapBase.SnapOnObject(EntityAI victim)`, а не `OnSteppedOn` и не
   `EEKilled(killer)`.** Почему: `EEKilled.killer` = уничтожитель (не жертва); `OnSteppedOn`
   недостижим для машинной ветки (ранний `return` без `super` у `LandMineTrap`), а
   `SnapOnObject` — единственная общая серверная точка, которую зовёт триггер и которую
   мина/растяжка НЕ переопределяют → гарантированно ловит все ветки (человек/машина/предмет).
3. **Разнести `m_ThrowerPlayer` (ItemBase, бросок) и `m_TriggeredPlayer` (TrapBase, ловушка).**
   Почему: мину не бросают (ставят `OnPlacementComplete`), гранату не «задевают» — поля
   ортогональны и живут в разных точках связки; объединять на одном `ItemBase` — грязнее.
4. **База для `m_ThrowerPlayer` = `ItemBase`, не `InventoryItem`.** Почему: `ItemBase` —
   модифицируемый слой 4_world, покрывает все релевантные источники (граната/ловушка/камень);
   `InventoryItem` — engine-слой 3_game, база всех предметов (избыточно широко).
5. **`m_TriggeredPlayer` тип `EntityAI`, не `PlayerBase`.** Почему: наступить/наехать может и
   машина (`CarScript`), и предмет; `EntityAI` покрывает всех, при необходимости фильтровать
   `PlayerBase.Cast` на месте потребления.
