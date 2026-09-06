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
  счёт `SendDeathJuncture`/корректного завершения) и при необходимости перехватить
  `GetHive()`/`CharacterKill` отдельно (или переопределить `GetHive()` → `null`, чтобы ваниль
  сама скипала БД-шаг). Для скриптового «убить» использовать рут-хелф (`SetHealth(0)`), а не
  `SetHealth("","Health",0)`.
- **Нокаут**: при `StartCommand_Unconscious(0)` ставить `m_IsUnconscious = true` и звать
  `OnUnconsciousStart()`; при `WakeUp` — `m_IsUnconscious = false` + `OnUnconsciousStop()`.
  Либо, как Expansion, дать боту action-manager, чтобы ванильный блок отработал сам.

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
- `DayZ-Expansion-Scripts/.../eAIBase.c:735-736` (создание action-manager), `:1355-1384`
  (`EEKilled` с `super`), `:10158-10195` (`OnUnconsciousStart/Stop`), `:7077/7199-7202`
  (`CommandHandler` + реплика INSTANCETYPE_SERVER-гейтов).
- Мод: `core/4_World/Entities/Bot/dmAISurvivorBase.c:864` (`UpdateUnconsciousBridge`), `:934`
  (`EEKilled` без `super`), `:953` (`CanAct`); `core/4_World/Entities/Bot/dmAISurvivor.c:198/359`
  (`OnDeath`, релиз мозга); `test/4_World/dmBotTest.c:350` (`SetHealth("","Health",0.0)`).
