# body — смерть и нокаут ИИ-бота

Домен: тело/состояние. Ваниль — `DayZ-Script-Diff/scripts/` (Build 1.28.160420), Expansion —
`DayZ-Expansion-Scripts/`. Смежное уже разобрано в `combat.md` (конвейер урона, `EEKilled` из
damage-системы). Здесь — только death-flow (труп) и нокаут (бессознательное состояние) для
`dmAISurvivorBase : PlayerBase` (`INSTANCETYPE_AI_SERVER`).

## Цель

Понять, почему:
1. бот, убитый внешним уроном, «зацикливается» на создании/протухании трупа;
2. бот с `Health=0` от холода/голода/`/test bot death` НЕ становится трупом (стоит мёртвый);
3. нокаут «ненастоящий»: нельзя взять оружие, нет «привести в чувство» (CPR), поза не та.

## Иерархия и instance-типы (контекст)

- `DayZPlayer : Human` (`dayzplayer.c:1155`) → `DayZPlayerImplement : DayZPlayer`
  (`dayzplayerimplement.c:110`) → `ManBase : DayZPlayerImplement` (`manbase.c:1`) →
  `PlayerBase : ManBase` (`playerbase.c:1`).
- `enum DayZPlayerInstanceType` (`dayzplayer.c:1067`): `INSTANCETYPE_SERVER`,
  `INSTANCETYPE_CLIENT`, `INSTANCETYPE_REMOTE`, **`INSTANCETYPE_AI_SERVER`**,
  `INSTANCETYPE_AI_REMOTE`, `INSTANCETYPE_AI_SINGLEPLAYER`.
- `IsAlive()` = `!IsDamageDestroyed()` (`object.c:520-522`) — нативная «разрушен ли рут-хелф».
  Это то, по чему мозг бота детектит смерть (`dmAISurvivor.OnUpdate` → `if (!m_Pawn.IsAlive())
  OnDeath()`).

## 1. Ванильный death-flow (подтверждено)

### Серверная цепочка (событие `EEKilled`)

`EEKilled` — нативный конвейер урона/смерти, диспатчится движком при уничтожении сущности
(у нас нет скриптового вызывателя — только `override` + `super.EEKilled`). База:
`entityai.c:1072` (инвокер + аналитика + `ReplaceOnDeath`).

`PlayerBase.EEKilled` (`playerbase.c:1045`):
1. `m_AdminLog.PlayerKilled(...)` — `:1047-1050`.
2. `delete GetBleedingManagerServer()` — `:1052-1053`.
3. `if (GetHive()) GetHive().CharacterKill(this)` — `:1056-1059` (убить персонажа в БД; у ИИ
   без identity печатает «Can't kill player with id -1» — потому мод и вырезал).
4. `GetGame().EnableVoN(this, false)` — `:1061`.
5. `ClientData.RemovePlayerBase(this)` (только не-dedicated) — `:1062-1063`.
6. `GetSymptomManager().OnPlayerKilled()` — `:1064`.
7. `if (GetEconomyProfile() && !m_CorpseProcessing && m_CorpseState == 0 &&
   GetGame().GetMission().InsertCorpse(this)) m_CorpseProcessing = true;` — `:1068-1072`.
8. `SyncRespawnModeInfo(GetIdentity())` — `:1074-1079`.
9. `super.EEKilled(killer)` — `:1081` → `DayZPlayerImplement.EEKilled`
   (`dayzplayerimplement.c:762`) → `SendDeathJuncture(-1, 0)` → `super`.

### Регистрация трупа и протухание (сервер)

- `MissionServer.InsertCorpse(Man)` (`missionserver.c:729-733`) — создаёт `CorpseData` и кладёт
  в `m_DeadPlayersArray`.
- `MissionServer.UpdateCorpseStatesServer()` (`missionserver.c:735`) — вызывается из тика миссии
  (`missionserver.c:211`); каждые 30с (`:752`) зовёт `corpse_data.UpdateCorpseState()`.
- `CorpseData.UpdateCorpseState(bool force_check=false)` (`corpsedata.c:27`):
  - инициализация `m_iMaxLifetime = m_Player.GetLifetime()` (`:38`); если `GetLifetime() <= 0`
    («cleanup time not initialized yet!») — до `GET_LIFETIME_TRIES_MAX` (3) ретраев, потом
    `m_bUpdate = false` (труп перестаёт обрабатываться) — `:39-49`.
  - свежесть = `m_LifetimeAdjusted / m_iMaxLifetime`; пороги `CORPSE_THRESHOLD_MEDIUM` /
    `CORPSE_THRESHOLD_DECAYED` → `m_iCorpseState` FRESH→MEDIUM→DECAYED — `:86-99`.
  - при смене состояния: `m_Player.m_CorpseState = m_iCorpseState; m_Player.SetSynchDirty();`
    — `:98-104`.
- `m_CorpseState` — net-sync var (`playerbase.c:516`), диапазон
  `-CORPSE_STATE_DECAYED .. CORPSE_STATE_DECAYED` (отрицательный знак = «форсировано живой»).
- Визуал гниения — клиент: `UpdateCorpseState()`/`UpdateCorpseStateVisual()`
  (`playerbase.c:5692`/`8993`).

### Смертельная анимация (клиент)

Анимация смерти **клиентская** и отдельна от трупа:
1. Сервер `DayZPlayerImplement.EEKilled` → `SendDeathJuncture(-1, 0)`
   (`dayzplayerimplement.c:753-760`) → `DayZPlayerSyncJunctures.SendDeath`
   (`dayzplayersyncjunctures.c:65-72`, `SJ_DEATH = 12`).
2. Клиент `OnSyncJuncture(SJ_DEATH)` (`dayzplayerimplement.c:3068-3069`) →
   `ReadDeathParams` → `m_DeathAnimType`, `m_DeathHitDir`.
3. Клиент `DayZPlayerImplement.CommandHandler` (`dayzplayerimplement.c:2237`) → `HandleDeath`
   (`:623`) → при `m_DeathAnimType != -2 && MISSION_STATE_GAME` (`:646`): `m_ShouldBeUnconscious
   = false` (`:669`) → `StartCommand_Death(type, m_DeathHitDir, callback, keepInLocalSpace)`
   (`:707`).
4. `PlayerBase.OnCommandDeathStart` (`playerbase.c:3964`) — `m_AnimCommandStarting =
   CommandDeath`, `AbortWeaponEvent()`, refresh анимации оружия, `super`.

`StartCommand_Death` — `proto native` (`human.c:1501`). `HumanCommandDeath` — нативный
(`human.c:609`).

### Что делает мод (`dmAISurvivorBase.c:934 EEKilled`, БЕЗ `super`)

```c
override void EEKilled(Object killer) {
    if (GetBleedingManagerServer()) delete GetBleedingManagerServer();
    GetSymptomManager().OnPlayerKilled();
    if (GetEconomyProfile() && !m_CorpseProcessing && m_CorpseState == 0
        && g_Game.GetMission().InsertCorpse(this))
        m_CorpseProcessing = true;
}
```

Пропущено относительно ванили: `CharacterKill` (осознанно), `EnableVoN(false)`,
`ClientData.RemovePlayerBase`, **`super.EEKilled` → `SendDeathJuncture`** (смертельная анимация),
`SyncRespawnModeInfo`. Труп регистрируется (`InsertCorpse`), но анимация смерти не запускается.

## vanilla EEKilled для AI_SERVER (покроково: краш / спам / no-op)

Цель: понять, какие шаги ванильного `super.EEKilled`-флоу реально крашат/спамят для
`INSTANCETYPE_AI_SERVER`-бота (нет character id, нет identity, нет клиента/action-manager),
чтобы выбрать минимальный фикс (труп создаётся + бот реально «умирает», без ошибки).

### Полная цепочка (сервер) с разметкой

`PlayerBase.EEKilled` (`playerbase.c:1045-1084`):

| # | Строка | Вызов | Для AI_SERVER |
|---|---|---|---|
| 1 | :1049-1052 | `if (m_AdminLog) m_AdminLog.PlayerKilled(this, killer)` | **безопасно** (в). `m_AdminLog` не null на сервере (`playerbase.c:380`). `PluginAdminLog.PlayerKilled` (`pluginadminlog.c:116`) при `identity==null` берёт `GetCachedName()/GetCachedID()` (`:90-91`) — не крашит, просто лог-строка. |
| 2 | :1054-1055 | `delete GetBleedingManagerServer()` | **безопасно** (в). `m_BleedingManagerServer` создаётся в серверном блоке (`:373`). |
| 3 | :1058-1061 | `if (GetHive()) GetHive().CharacterKill(this)` | **СПАМ** (б). `GetHive()` — глобальный native (`hive.c:31`), всегда не-null на сервере → `CharacterKill` зовётся всегда; native печатает «Can't kill player with id -1» (у ИИ `GetIdentity()==null`). Единственный реальный источник ошибки. |
| 4 | :1064 | `GetGame().EnableVoN(this, false)` | **no-op** (в). У ИИ нет клиента/VoN. |
| 5 | :1065-1066 | `if (!IsDedicatedServer()) ClientData.RemovePlayerBase(this)` | **пропускается** (в) на дедике (типично для AI_SERVER). |
| 6 | :1067 | `GetSymptomManager().OnPlayerKilled()` | **безопасно** (в). `m_SymptomManager` безусловно создаётся (`:383`). |
| 7 | :1069-1073 | `InsertCorpse(this)` (guard по `GetEconomyProfile() && !m_CorpseProcessing && m_CorpseState==0`) | **нужно** — создаёт труп. ИИ спавнится `CreateObject` → CE profile есть (лог `hasCEProfile=1`). |
| 8 | :1075-1081 | `SyncRespawnModeInfo(GetIdentity())` | **не краш, лёгкий waste** (в). `GetIdentity()==null` (лог `hasIdentity=0`) → `ScriptRPC.Send(null, …, true, null)`; `recipient==null` = broadcast всем клиентам (`gameplay.c:117`). Лишний `RPC_SERVER_RESPAWN_MODE` на каждую смерть ИИ. |
| 9 | :1083 | `super.EEKilled(killer)` | → `DayZPlayerImplement.EEKilled` (`dayzplayerimplement.c:762`). |

`DayZPlayerImplement.EEKilled` (`dayzplayerimplement.c:762-767`):

- `SendDeathJuncture(-1, 0)` (`:764`) → `DayZPlayerSyncJunctures.SendDeath(this, -1, 0)`
  (`dayzplayersyncjunctures.c:65-72`) → `SendSyncJuncture(SJ_DEATH=12, ctx)` (native,
  `dayzplayer.c:1282`). **Нужно, не крашит**: шлёт `SJ_DEATH` на `AI_REMOTE`-клиентов →
  смертельная анимация + терминальное мёртвое состояние. Identity не требуется.
- `super.EEKilled` → `EntityAI.EEKilled` (`entityai.c:1072-1081`): `m_OnKilledInvoker.Invoke`
  (может быть null — безопасно), `GetAnalyticsServer().OnEntityKilled(killer, this)`
  (`analyticsmanagerserver.c:48` — null-killer обрабатывается), `ReplaceOnDeath()==false`
  для игроков → no-op.

`StartCommand_Death` / `HandleDeath` (`dayzplayerimplement.c:623-720`) /
`OnCommandDeathStart` (`playerbase.c:3964-3973`) — **КЛИЕНТСКИЕ** (идут из `CommandHandler`
под action-manager). Для AI_SERVER на сервере не выполняются; на клиенте `AI_REMOTE`
отрабатывают штатно через juncture. Краша/спама не дают.

### `GetHive()`: что это и как его «переопределить»

- `GetHive()` — **глобальный native** (`hive.c:31`), НЕ метод класса. В ванили нет ни одного
  метода `GetHive()` в иерархии `DayZPlayer*`/`PlayerBase`; все вызовы в `playerbase.c:1058/1060`
  и `missionserver.c` — bare-вызовы глобала. Поэтому **`if (GetHive())` у ИИ всегда true**, и
  `CharacterKill` не скипается.
- Bare-вызов функции внутри метода резолвится в пользу **метода класса** над глобалом. Значит
  затенять глобал можно ТОЛЬКО объявив метод `GetHive()` на **ancestor'е** `PlayerBase`
  (`DayZPlayerImplement`/`ManBase`/`PlayerBase`) через `modded class`. Метод на потомке
  (`dmAISurvivorBase`) глобал для `PlayerBase.EEKilled` **не затеняет** (вызов резолвится по
  иерархии вверх, потомок не виден).
- Подтверждённый референс — **Expansion**: `modded class DayZPlayerImplement { Hive GetHive()
  { if (IsAI()) return null; return Expansion_GlobalGetHive(); } }`
  (`DayZExpansion_AI/.../DayZPlayerImplement.c:418-429`), где `Expansion_GlobalGetHive()`
  (`.../Hive.c:1-7`) — wrapper, зовущий глобал (внутри метода bare `GetHive()` рекурсивно ушёл
  бы в сам метод). Комментарий Expansion: «Suppress "couldn't kill player" in server logs when
  AI gets killed».
- **Достаточно ли `GetHive()→null` для CharacterKill-спама?** Да: `if (GetHive())` в
  `playerbase.c:1058` станет false → `CharacterKill` штатно скипается. Это ровно фикс Expansion.

### Итоговая рекомендация (минимальный безопасный набор)

1. `modded class PlayerBase` (или `DayZPlayerImplement`, как Expansion):
   `Hive GetHive() { if (GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_AI_SERVER)
   return null; return <wrapper-к-глобалу>; }` — глушит шаг 3 (единственный спам).
2. В `dmAISurvivorBase.EEKilled` вернуть **`super.EEKilled(killer)`** вместо ручной репликации:
   полный ванильный флоу теперь безопасен (шаги 1/2/4/5/6/8 no-op, шаг 7 даёт труп, шаг 9 даёт
   `SendDeathJuncture` → анимация + мёртвое состояние). `CharacterKill` скипается за счёт п.1.
3. `SendDeathJuncture` переопределять no-op **НЕ нужно** — он желаем и не крашит.
4. (опц.) `SyncRespawnModeInfo(null)`-broadcast можно заглушить (override `GetIdentity()` или
   guard), но это не краш/спам — только лишний RPC.

Минимальный вариант БЕЗ `modded class` (если не хочется трогать `GetHive`): оставить ручной
`EEKilled`, но добавить `SendDeathJuncture(-1, 0);` (public, наследуется от
`DayZPlayerImplement`) — это убирает «стоячего мёртвого» (анимация) без CharacterKill; труп уже
регистрируется. Менее «ванильно», но короче.

## 2. Натуральная смерть vs внешний килл (EEKilled при Health=0)

Два разных API манипуляции здоровьем (`object.c`):

| Путь | Вызов | Зона | Пример |
|---|---|---|---|
| Рут-хелф | `SetHealth(float)` = `SetHealth("","",x)` (`object.c:1069`) | `""`/`""` | суицид `SetHealth(0)` (`emotemanager.c:826-832`), кровотечение `SetHealth("","",-1000)` (`bleeding.c:37`) |
| Хелф-стат | `SetHealth("","Health",x)` / `AddHealth("GlobalHealth","Health",x)` | `Health` | голод `AddHealth("GlobalHealth","Health",-X)` (`hunger.c:45`), жажда (`thirst.c:45`), `/test bot death` `SetHealth("","Health",0.0)` (`test/4_World/dmBotTest.c:350`) |

`IsDamageDestroyed()`/`IsAlive()` реагируют на обе формы — мёртвый «Health»-стат даёт
`IsAlive()=false` (мозг бота отпускается, `dmBotTest_Death` ждёт `IsSpawned()=false`).

Разница в поведении (по симптомам мода):
- **Внешний урон** (damage-система: `DamageSystem.CloseCombatDamageName` → нативное применение →
  рут-хелф 0) → движок диспатчит `EEKilled` → `InsertCorpse` → труп есть.
- **`SetHealth("","Health",0)` / `AddHealth("GlobalHealth","Health",-X)`** → `IsAlive()=false`,
  но `EEKilled` НЕ приходит → труп не создаётся, смертельная анимация не играет → «стоит мёртвый».

Точная точка диспатча `EEKilled` при хелф-стате 0 — **нативная, из скриптов не подтверждается**.
Косвенно: ванильный суицид/кровотечение убивают через **рут**-хелф (`SetHealth(0)` /
`SetHealth("","",-1000)`), а голод/жажда/холод — через **Health-стат** (`AddHealth(...,"Health",…)`).
Практический вывод для мода: если нужно «честно» убить бота скриптом — звать `SetHealth(0)`
(рут), а не `SetHealth("","Health",0)`; либо после доведения Health-стата до 0 вручную
дёргать `EEKilled`/`InsertCorpse`-логику.

## 3. Нокаут (бессознательное состояние)

### Ванильный путь (для клиента игрока)

1. Сервер `UnconsciousnessMdfr.OnActivate` (`unconsciousness.c:23-26`) → шок ≤
   `UNCONSCIOUS_THRESHOLD` → `DayZPlayerSyncJunctures.SendPlayerUnconsciousness(player, true)`
   (`dayzplayersyncjunctures.c:168-175`, `SJ_UNCONSCIOUSNESS = 11`).
2. Клиент `PlayerBase.OnSyncJuncture` (`playerbase.c:7883`) → case `SJ_UNCONSCIOUSNESS`
   (`:7920-7921`) → `ReadPlayerUnconsciousnessParams(..., m_ShouldBeUnconscious)`.
3. Клиент `PlayerBase.CommandHandler` → блок `if (mngr && hic)` (`playerbase.c:2974`), где
   `mngr = GetActionManager()` (`:2909`), `hic = GetInputController()`:
   - `m_ShouldBeUnconscious` → `StartCommand_Unconscious(0)` (`:3030`);
   - следующий кадр `pCurrentCommandID == COMMANDID_UNCONSCIOUS` → `m_IsUnconscious = true;`
     (`:3009`) + `OnUnconsciousStart()` (`:3010`);
   - пробуждение: `m_UnconsciousTime > 2` → `hcu.WakeUp(wakeUpStance)` (`:3051`),
     `m_IsUnconscious = false;` (`:3053`) + `OnUnconsciousStop(pCurrentCommandID)` (`:3054`).
4. `m_IsUnconscious` — **net-sync bool** (`playerbase.c:528`). `IsUnconscious()` =
   `m_MovementState.m_CommandTypeId == COMMANDID_UNCONSCIOUS || m_IsUnconscious`
   (`playerbase.c:3522`); `IsUnconsciousStateOnly()` = `m_IsUnconscious` (`:3527`).

`OnUnconsciousStart()` (`playerbase.c:3373`): клиент — скрыть HUD, `SetInventorySoftLock(true)`,
**выбросить предмет из рук** (`DropItem`, `:3386`); сервер (`INSTANCETYPE_SERVER`) —
`SetSynchDirty()`, `EnableVoN(false)`, admin log, `GetMeleeFightLogic().SetBlock(false)`,
`SetMasterAttenuation("UnconsciousAttenuation")`.

`OnUnconsciousStop()` (`playerbase.c:3424`): `SetSynchDirty()`, `m_UnconsciousTime=0`; сервер —
`EnableVoN(true)`; `SetMasterAttenuation("")`.

`StartCommand_Unconscious(float)` / `GetCommand_Unconscious()` — `proto native`
(`human.c:1507/1509`); `HumanCommandUnconscious.WakeUp(int stance=-1)` / `IsWakingUp()` /
`IsOnLand()` / `IsInWater()` — `human.c:624-628`.

### Что нужно для «настоящего» нокаута у других игроков

- `m_IsUnconscious = true` (net-var) — иначе `IsUnconscious()`/`IsUnconsciousStateOnly()` у
  других игроков `false` → CPR-экшен не появится (`actioncpr.c:50`:
  `other_player.IsUnconscious() && !holds_heavy_item`), «обыскать тело» не откроется, поза не
  засинкается.
- `OnUnconsciousStart()` — иначе оружие остаётся в руках (нет `DropItem`), нет `SetSynchDirty()`,
  нет `EnableVoN(false)`.
- `OnUnconsciousStop()` на пробуждение.

### Почему у мода не работает (`dmAISurvivorBase.c:864 UpdateUnconsciousBridge`)

Мод зовёт `StartCommand_Unconscious(0)` напрямую (т.к. `m_ActionManager` у AI_SERVER `null` →
ванильный `if (mngr && hic)`-блок не выполняется), но:
- НЕ ставит `m_IsUnconscious = true` → другие игроки не видят нокаут, нет CPR/лута.
- НЕ зовёт `OnUnconsciousStart()` → оружие в руках, нет `SetSynchDirty`, нет `EnableVoN(false)`.
- на пробуждение: `hcu.WakeUp(...)` без `m_IsUnconscious = false` и без `OnUnconsciousStop()`.

### Референс Expansion (`eAIBase.c`)

- **Создаёт action-manager**: `m_ActionManager = new eAIActionManager(this)`
  (`eAIBase.c:735-736`) — поэтому ванильный `if (mngr && hic)`-блок у AI выполняется, и
  `m_IsUnconscious`/`OnUnconsciousStart` зовутся штатно.
- `EEKilled` (`eAIBase.c:1355`): **сначала `super.EEKilled(killer)`** (`:1361`), затем AI-чистка
  и лут-дроп — ванильный труп-флоу сохраняется.
- `OnUnconsciousStart` (`eAIBase.c:10158`): `eAI_ResetRaised()`; `eAI_DropItemInHandsImpl(...)`
  (выбросить предмет из рук серверно); `super.OnUnconsciousStart()`; затем повтор серверной
  части (`SetSynchDirty`, `MarkCrewMemberUnconscious`, admin log, `SetBlock(false)`) с
  комментарием **«Needed because vanilla only checks for INSTANCETYPE_SERVER»**.
- `OnUnconsciousStop` (`eAIBase.c:10188`) — `super` + повтор admin-log (та же причина).
- `CommandHandler` (`eAIBase.c:7077`) — `super.CommandHandler` + реплика
  `GetPlayerSoundManagerServer().Update(); ShockRefill(); FreezeCheck()` (`:7199-7202`), тоже
  потому что ваниль гейтит их `INSTANCETYPE_SERVER` (`playerbase.c:3086-3090`).

## Выводы для фикса (рекомендации)

- **Смерть**: в `EEKilled` вызвать `super.EEKilled(killer)` (это уберёт «стоячего мёртвого» за
  счёт `SendDeathJuncture`/корректного завершения) и заглушить `CharacterKill` через
  `modded class`-метод `GetHive()`→null на **ancestor'е** `PlayerBase` (не на `dmAISurvivorBase` —
  см. «vanilla EEKilled для AI_SERVER»). Для скриптового «убить» использовать рут-хелф
  (`SetHealth(0)`), а не `SetHealth("","Health",0)`.
- **Нокаут**: при `StartCommand_Unconscious(0)` ставить `m_IsUnconscious = true` и звать
  `OnUnconsciousStart()`; при `WakeUp` — `m_IsUnconscious = false` + `OnUnconsciousStop()`.
  Либо, как Expansion, дать боту action-manager, чтобы ванильный блок отработал сам.

## 4. Нокаут и связывание (restrain/unrestrain)

Цель фикса: (а) настоящий нокаут, (б) бот «развязывается» после связывания, (в) программно
связать бота в автотестах. Ниже — проверенные сигнатуры и выводы по трём вопросам.

### 4.1 Связывание (`ActionRestrainTarget`)

- Класс `ActionRestrainTarget` (`4_world/classes/useractionscomponent/actions/continuous/
  actionrestraintarget.c`), команда `CMD_ACTIONFB_RESTRAINTARGET`, full-body.
- `ActionCondition` (`:41-55`): цель должна быть `PlayerBase`; ветка по instance-type игрока-актора:
  - `INSTANCETYPE_SERVER` → `other_player.CanBeRestrained()`;
  - иначе (клиент) → `!other_player.IsRestrained()`.
- `ActionConditionContinue` (`:65-89`): на сервере в MP `return false`, если
  `target.IsSurrendered() || !target.CanBeRestrained()`; на сервере при прогрессе `>0.75`
  ставит `SetRestrainPrelocked(true)`; `return false` если `IsPlayerDisconnecting(target)`.
- `OnStartServer` (`:91-107`): если цель `IsSurrendered()` → `EndSurrenderRequest(SurrenderDataRestrain)`
  (завершить сдачу), иначе если `IsEmotePlaying()` → отменить эмоцию; затем
  `target.SetRestrainStarted(true)`.
- `OnFinishProgressServer` (`:121-149`): `if (CanReceiveAction(target) && !target.IsRestrained())`
  → у актора в руках должен быть предмет (`item_in_hands_source`), иначе `Error`; `new_item_name =
  MiscGameplayFunctions.ObtainRestrainItemTargetClassname(item_in_hands_source)` (= `ConfigGetString(
  "OnRestrainChange")`); если у цели есть предмет в руках — `ChainedDropAndRestrainLambda`, иначе
  `RestrainTargetPlayerLambda` → `LocalReplaceItemInHandsWithNewElsewhere`.
- Финальная точка: `RestrainTargetPlayerLambda.OnSuccess` (`:221-227`) =
  `m_TargetPlayer.SetRestrained(true); m_TargetPlayer.OnItemInHandsChanged();`.

**Ключевой вывод: связывание НЕ гейтится `IsUnconscious()`.** Условие — `CanBeRestrained()`
(`playerbase.c:1999-2010`): `false` если `IsInVehicle() || IsRaised() || IsSwimming() ||
IsClimbing() || IsClimbingLadder() || IsRestrained() || !GetWeaponManager() ||
GetWeaponManager().IsRunning() || !GetActionManager() || GetActionManager().GetRunningAction()
!= null || IsMapOpen()`, или включён throwing. То есть фикс нокаута **сам по себе не открывает**
связывание; для MP-игрока дополнительно нужно, чтобы цель НЕ была `IsSurrendered()` в момент
continue (сдачу снимает сам `OnStartServer`).

**Засада для AI-бота:** `CanBeRestrained()` требует `GetActionManager() != null`
(`!GetActionManager()` → `false`). У `INSTANCETYPE_AI_SERVER`-бота `m_ActionManager == null`,
поэтому ванильный игрок **не сможет** связать бота через `ActionRestrainTarget` — то же самое
`if (mngr && hic)`-гейтование, что и у нокаута. Два пути: (а) дать боту action-manager (как
Expansion, `eAIBase.c:735-736`), либо (б) `modded class`-override `CanBeRestrained()` для AI
(вернуть `true` при выполнении остальных условий). Рекомендация — (а), т.к. заодно чинит нокаут
и позволяет ванильным CPR/лут/restrain-флоу отрабатывать штатно.

**Кто связал (restrainer):** на цели **нет** хука с идентичностью. Restrainer виден только в
action'е (`action_data.m_Player` = source_player). `OnRestrainStart()` (`playerbase.c:3683`) — это
клиентская UI-уборка (`CloseInventoryMenu`, снятие input-excludes), не событие «кто связал».
Expansion добавляет `m_Expansion_OnRestrainedStateChaged` ScriptInvoker (bool, не кто) в
`modded class PlayerBase` (`PlayerBase.c:61/1093-1106`). Вывод: чтобы бот знал «кто связал» —
нужен СВОЙ хук (например `modded class ActionRestrainTarget.OnFinishProgressServer` записать
restrainer в поле цели, либо собственный `SetRestrainedBy(PlayerBase)`).

**Программный restrain (автотест):** `void SetRestrained(bool)` (`playerbase.c:2034`) — public,
ставит `m_IsRestrained` + `SetSynchDirty()`. Вызов `bot.SetRestrained(true)` с сервера — легальный
серверный путь, ванильный action не нужен. `IsRestrained()` override (`:2040`) = `m_IsRestrained`.
Для «связанных рук» (визуал + предмет) ваниль кладёт `RestrainingToolLocked` в руки цели: его
`EEItemLocationChanged` (`handcuffslocked.c:12-49`) при `newLoc` = HANDS сам делает
`SetRestrained(true)` + `OnItemInHandsChanged()` (+ `OnRestrainStart()` если controlled). Expansion
AI-action делает ровно `ai.GetHumanInventory().CreateInHands(new_item_name); ai.SetRestrained(true);
ai.OnItemInHandsChanged();` (`ActionRestrainTarget.c:79-81`). Минимальный автотест:
`bot.SetRestrained(true)` (флаг), опционально положить locked-restraint в руки.

### 4.2 Развязывание (`ActionUnrestrainSelf`, не `ActionBreakFreeRestrain`)

- Ванильный класс «развязать себя» — **`ActionUnrestrainSelf`** (`4_world/classes/useractionscomponent/
  actions/continuous/actionunrestrainself.c`). `ActionBreakFreeRestrain` **не существует**.
- Анимация — «борьба» (struggle): `m_CommandUID = CMD_ACTIONMOD_RESTRAINEDSTRUGGLE` (=23,
  `dayzplayer.c:758`), `m_CommandUIDProne = CMD_ACTIONFB_RESTRAINEDSTRUGGLE` (=111, `dayzplayer.c:868`).
- `ActionCondition` (`:64-67`): `player.IsRestrained()`. `CanBeUsedInRestrain()` → true (`:105-108`).
- `OnFinishProgressServer` (`:85-103`): `player.SetRestrained(false)` + урон предмету +
  `MiscGameplayFunctions.TransformRestrainItem(...)` (вернуть исходный предмет).
- Развязать другого: `ActionUnrestrainTarget` (`actionunrestraintarget.c`) — гейт `target.IsRestrained()`
  + предмет-инструмент из `CanBeUnrestrainedBy`; финал `SetRestrained(false)` + `TransformRestrainItem`.
  Есть также `ActionUnrestrainTargetEmpty` (пустыми руками).
- `TransformRestrainItem(current_item, tool, source, target)` (`miscgameplayfunctions.c:795-824`) —
  превращает restrained-предмет обратно по конфигу `OnRestrainChange` (замена/уничтожение).

**Серверный запуск у AI-бота:** у бота нет action-manager → ванильный `ActionUnrestrainSelf`
запустить нельзя. Expansion решает так: action-manager создаётся (`eAIBase.c:735-736`), а FSM-состояние
`eAIState_Struggle` (`eaistate_struggle.c:7`) зовёт `unit.StartActionObject(ActionUnrestrainSelf, null)`
при `IsRestrained() && !IsUnconscious() && !m_eAI_IsInventoryVisible`. Т.е. ванильный путь «развязаться» —
**через action-систему**, а не прямой дёржкой состояния. Минимальный кастомный путь для бота без
action-manager: `bot.SetRestrained(false)` + удалить/трансформировать restrained-предмет
(`TransformRestrainItem`/`DeleteSafe` + `OnItemInHandsChanged()`), как делает Expansion
`ActionUnrestrainTarget.eAI_Unrestrain` (`ActionUnrestrainTarget.c:57-78`). Анимация борьбы — чисто
визуальная; при желании запускается `StartCommand_Action(CMD_ACTIONFB_RESTRAINEDSTRUGGLE, pCallbackClass,
pStanceMask)` (`human.c:1533`, `proto native`), но для серверного «развязался» не обязательна.

### 4.3 Доступность `m_IsUnconscious` и дроп оружия при нокауте

- **Объявление**: `protected bool m_IsUnconscious` — в `dayzplayerimplement.c:124` (НЕ в
  `playerbase.c`). `protected` = доступен подклассу `dmAISurvivorBase` напрямую, сеттер через
  `modded class PlayerBase` НЕ нужен. Регистрация net-sync: `RegisterNetSyncVariableBool(
  "m_IsUnconscious")` (`playerbase.c:575`) → после записи нужен `SetSynchDirty()` (его делает
  `OnUnconsciousStart()` в серверной ветке).
- Для сравнения: `m_IsRestrained` — bare `bool` (`playerbase.c:158`, public по умолчанию), net-sync
  `:576`; `m_IsRestrainStarted`/`m_IsRestrainPrelocked` — bare bool `:160/162`, net-sync `:581/582`.
- **Дефолтная видимость в Enfusion = public** (подтверждено: `m_EmoteManager` объявлен без модификатора
  `playerbase.c:94`, а к нему обращаются из другого класса `actionrestraintarget.c:103`). `protected`/
  `private` — явные ограничения. Поэтому «можно ли писать напрямую»: `m_IsUnconscious` — да (protected),
  `m_IsRestrained` — да (public), но для `m_IsRestrained` правильнее через `SetRestrained()` (делает
  `SetSynchDirty`).
- **Дроп оружия в `OnUnconsciousStart()`** (`playerbase.c:3502-3553`): `DropItem` находится в блоке
  `INSTANCETYPE_CLIENT` (`:3508-3522`) — клиентская часть; серверная ветка
  (`:3524`: `INSTANCETYPE_SERVER || (!IsMultiplayer && CLIENT)`) НЕ содержит `DropItem` (только
  `SetSynchDirty`, `MarkCrewMemberUnconscious`, `EnableVoN(false)`, admin log, `SetBlock(false)`).
  Для `INSTANCETYPE_AI_SERVER` **ни одна из двух веток не выполняется** → ванильный `OnUnconsciousStart`
  у бота не выбросит оружие и не сделает `SetSynchDirty`. Подтверждено: дроп оружия для бота надо
  делать СВОИМ кодом (наш override `DropItem`), как делает Expansion `eAI_DropItemInHandsImpl` в
  `eAIBase.OnUnconsciousStart` (`eAIBase.c:10158-10162`) с комментарием «Needed because vanilla only
  checks for INSTANCETYPE_SERVER».

## Открытые вопросы / не подтверждено

- Точное нативное условие диспатча `EEKilled` при обнулении `Health`-стата
  (`SetHealth("","Health",0)`/`AddHealth(...,"Health",…)) — не видно из скриптов; вывод сделан
  по симптомам мода (внешний килл → труп; хелф-стат 0 → нет трупа) и по тому, что ванильные
  «естественные» смерти (суицид/кровотечение) идут через рут-хелф.
- Точная причина «цикла создания трупа и протухания» при внешнем килле — не подтверждена.
  Кандидат: `CorpseData.UpdateCorpseState` не инициализирует `m_iMaxLifetime` (у ИИ
  `GetLifetime()` может быть `0` → 3 ретрая → `m_bUpdate=false`), либо `EEKilled` ре-диспатчится
  (нет `super` → сущность не переходит в терминальное мёртвое состояние). Диагностика — команда
  `/bot deadmans` (уже читает `m_DeadPlayersArray` + `CorpseData`).
- Нужен ли `GetInputController()` (`hic`) для `if (mngr && hic)` у AI_SERVER (у Expansion
  `hic` приходит из `GetInputController()` — `eAIBase.c:6779`), и не `null` ли он — не проверено.

## Источники (файл:строка, ваниль = DayZ-Script-Diff)

- `3_game/entities/object.c:520-522` (`IsAlive`= `!IsDamageDestroyed`), `:1007` (`SetHealth`),
  `:1019` (`AddHealth`), `:1069` (`SetHealth(float)` = `SetHealth("","",x)`).
- `3_game/entities/entityai.c:1072` (`EEKilled` база), `:1021` (`EEHealthLevelChanged`).
- `3_game/dayzplayer.c:1067` (enum instance-type), `:1168` (`GetInstanceType`).
- `3_game/human.c:1501/1507/1509` (`StartCommand_Death`/`StartCommand_Unconscious`/
  `GetCommand_Unconscious`), `:609/619` (`HumanCommandDeath`/`HumanCommandUnconscious`),
  `:624-628` (`WakeUp`/`IsWakingUp`/`IsOnLand`/`IsInWater`).
- `4_world/entities/manbase/playerbase.c:1045` (`EEKilled`), `:1068-1072` (InsertCorpse-guard),
  `:516` (net-var `m_CorpseState`), `:528` (net-var `m_IsUnconscious`), `:2974-3064`
  (unconscious-блок `if(mngr&&hic)`), `:3373/3424/3469` (`OnUnconsciousStart/Stop/Update`),
  `:3522/3527` (`IsUnconscious`/`IsUnconsciousStateOnly`), `:3964` (`OnCommandDeathStart`),
  `:5692/8993` (`UpdateCorpseState`/`UpdateCorpseStateVisual`), `:7883/7920-7921`
  (`OnSyncJuncture`/`SJ_UNCONSCIOUSNESS`).
- `4_world/entities/dayzplayerimplement.c:623` (`HandleDeath`), `:707`
  (`StartCommand_Death`), `:753-760` (`SendDeathJuncture`), `:762` (`EEKilled` → juncture),
  `:2237` (`CommandHandler`), `:148-150` (`m_IsUnconscious`/`m_ShouldBeUnconscious`),
  `:519` (`m_DeathAnimType=-2`).
- `4_world/entities/dayzplayersyncjunctures.c:17/18/65-72/168-175` (SJ_UNCONSCIOUSNESS/SJ_DEATH/
  SendDeath/SendPlayerUnconsciousness).
- `5_mission/mission/missionserver.c:729-733` (`InsertCorpse`), `:735-763`
  (`UpdateCorpseStatesServer`).
- `4_world/classes/corpsedata.c:27-104` (`UpdateCorpseState`, lifetime-init, `m_CorpseState`).
- `4_world/classes/playermodifiers/modifiers/unconsciousness.c:23-32`
  (`UnconsciousnessMdfr`), `conditions/bleeding.c:37` (`SetHealth("","",-1000)`),
  `hunger.c:45`/`thirst.c:45` (`AddHealth("GlobalHealth","Health",…)`).
- `4_world/classes/emotemanager.c:826-832` (`KillPlayer` → `SetHealth(0)`).
- `4_world/classes/useractionscomponent/actions/continuous/medical/actioncpr.c:50`
  (`IsUnconscious()`-гейт CPR).
- `3_game/hive/hive.c:31` (`proto native Hive GetHive();` — глобал, НЕ метод).
- `3_game/gameplay.c:117` (`ScriptRPC.Send(..., recipient=NULL)` → broadcast всем клиентам),
  `:338-391` (`PlayerIdentity`).
- `3_game/analytics/analyticsmanagerserver.c:48` (`OnEntityKilled` — null-killer безопасен).
- `4_world/plugins/pluginbase/pluginadminlog.c:116` (`PlayerKilled`), `:67-101` (`GetPlayerPrefix`,
  null-identity → `GetCachedName()/GetCachedID()`).
- `DayZ-Expansion-Scripts/.../eAIBase.c:735-736` (создание action-manager), `:1355-1384`
  (`EEKilled` с `super`), `:10158-10195` (`OnUnconsciousStart/Stop`), `:7077/7199-7202`
  (`CommandHandler` + реплика INSTANCETYPE_SERVER-гейтов).
- `DayZ-Expansion-Scripts/DayZExpansion/AI/Scripts/4_World/DayZExpansion_AI/Entities/
  DayZPlayerImplement.c:418-429` (`modded class DayZPlayerImplement` → `Hive GetHive()` с
  `IsAI()→null`), `.../3_Game/DayZExpansion_AI/Hive.c:1-7` (`Expansion_GlobalGetHive()` wrapper).
- Мод: `core/4_World/Entities/Bot/dmAISurvivorBase.c:864` (`UpdateUnconsciousBridge`), `:934`
  (`EEKilled` без `super`), `:953` (`CanAct`); `core/4_World/Entities/Bot/dmAISurvivor.c:198/359`
  (`OnDeath`, релиз мозга); `test/4_World/dmBotTest.c:350` (`SetHealth("","Health",0.0)`).
