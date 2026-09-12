# voice — «реплики» ИИ-бота (голосовые звуки vs VoN)

Домен: голос. Ваниль — `DayZ-Script-Diff/scripts/` (Build 1.28.160420), данные — `DZ/`
(`characters/data/config.cpp`, `sounds/hpp/config.cpp`, `anims/`), Expansion — `DayZ-Expansion-Scripts/`.

## Цель

Заставить `dmAISurvivorBase : PlayerBase` (`INSTANCETYPE_AI_SERVER`, клиент `AI_REMOTE`)
«произносить реплики», слышимые окружающими игроками. Выяснить: можно ли через VoN, а если
нет — какой ванильный механизм голосовых звуков использовать и как его расширить на
кастомные слова.

## Вердикт (кратко)

- **Через обычный VoN — нельзя.** VoN = engine-driven аудиоканал (микрофон → голосовой кодек →
  пакеты). Скриптового способа впрыснуть аудио в VoN-канал нет. `EnableVoN` для ИИ — no-op.
- **Через ванильную систему «голосовых звуков» — можно.** Это НЕ VoN, а отдельная
  script-триггерируемая цепочка позиционных звуков: сервер `RequestSoundEventEx(id)` →
  net-var `m_SoundEvent` → клиент `OnVariablesSynchronized` → `CheckSoundEvent` →
  `PlaySoundEventEx` → `PlayerSoundEventBase.Play()` → `ProcessVoiceEvent(config_id)` →
  позиционный `PlaySound` на каждом клиенте. Именно так ваниль озвучивает кашель/смех/чих.
- **Минимальный путь для botorama** (см. «Рекомендуемый путь»): кастомная конфиг-запись
  `SoundVoice` (id + soundLookupTable + аудио) + кастомная net-var на боте + на клиенте в
  `OnVariablesSynchronized` прямой вызов `ProcessVoiceEvent("", "", <config_id>)`. Полностью
  в рамках уже принятого в моде паттерна (net-var + клиентский `OnVariablesSynchronized`).
- **Реализовано (итог, `feature/bot-voice-lines` → master):** пошли БОЛЕЕ простым путём, чем
  цепочка `SoundVoice`/`ProcessVoiceEvent`. Сервер `dmAISurvivorBase.SpeakLine(id)` → net-var
  `m_VoiceLineId`+`m_VoiceLineNonce` → клиент в `OnVariablesSynchronized` играет `Play3D` через
  `SoundParams("dmBotVoice_test_SoundSet")` (свой `CfgSoundSets`/`CfgSoundShaders` в `config.cpp`);
  рот — серверный `SetTalking` (как ZenExpansionAudioAI). Реплики-слова задаются обычным
  SoundSet, БЕЗ `CfgVoiceSoundTables`/`AnimEvents SoundVoice`. Подтверждено в игре (рот шевелится).

---

## 1. VoN — почему нельзя (подтверждено)

VoN = engine-driven: микрофон игрока кодируется движком, пакеты шлются нативно, приём
генерит движковые события. Скриптовая поверхность — только управление/уведомления, НЕ впрыск
аудио.

| Что | Файл:строка | Комментарий |
|---|---|---|
| `proto native void EnableVoN(Object player, bool enable)` | `3_game/global/game.c:1057` | для ИИ no-op (нет клиента/VoN) — см. `body.md` |
| `proto native void SetVoiceEffect(Object player, int effect, bool enable)` | `3_game/global/game.c:1065` | server-only; эффекты `VoiceEffectMumbling=1/Extortion=2/Obstruction=4` |
| `proto native void SetVoiceLevel(int level)` | `3_game/global/game.c:1071` | client-only |
| `proto native int GetVoiceLevel(Object player = null)` | `3_game/global/game.c:1076` | server+client |
| `proto native void EnableMicCapture(bool)` / `proto native bool IsMicCapturing()` | `3_game/global/game.c:1082/1087` | захват микрофона контролируемого игрока |
| `proto native bool DisableReceiveVoN(bool disable)` | `3_game/global/world.c:213` | глобальное отключение приёма |
| `proto native float IsPlayerSpeaking()` | `3_game/dayzplayer.c:1333` | амплитуда РЕАЛЬНОГО говорения (микрофон). У ИИ = 0 — это драйвер `SetTalking`/VoN-шума (см. §5) |

`VONManager` (`3_game/vonmanager.c`) — только клиентский UI (иконка/PTT/нотификации), аудио не
передаёт. События `VONStartSpeakingEventParams = Param2<string,string>` / `VONStartSpeakingEventTypeID`
(`3_game/gameplay.c`) генерирует движок при реальном приёме пакетов — скриптового триггера нет.
**Вывод: VoN-канал скриптом не задействовать.**

---

## 2. Система голосовых звуков (пригодная альтернатива)

Это два разных механизма, объединённых через anim-событие `SoundVoice`:
1. **`PlayerSoundEventHandler` + `EPlayerSoundEventID`** — сервер триггерит по id, клиент играет.
2. **`ReplaceSoundEventHandler`** — перехват anim-событий `SoundVoice` и подмена id (нужен anim-event).

Оба в итоге зовут **`ProcessVoiceEvent`**, который читает звук из конфига
`CfgVehicles SurvivorBase AnimEvents SoundVoice { id=...; soundLookupTable=...; noise=... }`.

### 2.1. Сервер→клиент репликация (`m_SoundEvent`) — КЛЮЧЕВОЕ

| Шаг | Файл:строка | Что происходит |
|---|---|---|
| поля | `playerbase.c:108-111` | `int m_SoundEvent; int m_SoundEventParam; bool m_SoundEventSent;` |
| net-var | `playerbase.c:555-556` | `RegisterNetSyncVariableInt("m_SoundEvent", 0, EPlayerSoundEventID.ENUM_COUNT - 1)`, `...("m_SoundEventParam", 0, ((EPlayerSoundEventParam.ENUM_COUNT-1)*2)-1)` |
| создание хендлеров | `playerbase.c:2428-2429` | `m_PlayerSoundEventHandler = new PlayerSoundEventHandler(this); m_ReplaceSoundEventHandler = new ReplaceSoundEventHandler(this);` — в блоке `if (!g_Game.IsDedicatedServer())` (клиент) |
| серверный вход | `playerbase.c:7423-7432` | `RequestSoundEventEx(id, from_server_and_client=false, param=0)` → если `from_server_and_client && CLIENT` → локально `PlaySoundEventEx`; иначе `SendSoundEventEx(id, param)` |
| серверная отправка | `playerbase.c:7455-7471` | `SendSoundEventEx`: `if (!g_Game.IsServer()) return; m_SoundEvent = id; m_SoundEventParam = param; SetSynchDirty();` (+ в singleplayer сразу `CheckSoundEvent()`) |
| клиентский вход | `playerbase.c:5886-5901` | `OnVariablesSynchronized()` → `CheckSoundEvent()` |
| клиентская обработка | `playerbase.c:7655-7678` | `CheckSoundEvent()`: `if (m_SoundEvent != 0 && !(param & STOP_PLAYBACK)) PlaySoundEventEx(m_SoundEvent, false, true, m_SoundEventParam); else if (param & STOP_PLAYBACK) StopSoundEvent(...);` затем обнуляет `m_SoundEvent/Param` |
| тик remote | `playerbase.c:2856-2857` | `m_PlayerSoundEventHandler.OnTick(delta_time)` — только `INSTANCETYPE_CLIENT`; для REMOTE/AI_REMOTE хендлер сам ставит `Timer` раз в 1с (`playersoundeventhandler.c:58-62`) |
| серверный «сброс» | `playerbase.c:2991-3003` | в `CommandHandler`: `CheckZeroSoundEvent(); CheckSendSoundEvent();` (`:3002-3003`) — флаг `m_SoundEventSent` для однократной отправки/обнуления |

`PlaySoundEventEx` (`playerbase.c:7505-7511`) → `m_PlayerSoundEventHandler.PlayRequestEx(id, is_from_server, param)`.
`RequestSoundEvent`/`RequestSoundEventEx`/`RequestSoundEventStop` — **public** (`dayzplayerimplement.c:982-984`);
`SendSoundEvent`/`SendSoundEventEx` — **protected** (`dayzplayerimplement.c:985-986`).

### 2.2. `PlayerSoundEventHandler` → `PlayerSoundEventBase` → `ProcessVoiceEvent`

- `enum EPlayerSoundEventID` (`playersoundeventhandler.c:2-42`): `HOLD_BREATH=1 ... FORCE_DRINK`, затем `ENUM_COUNT`.
  Комментарий `// defined in animEventsSoundVoice.hpp` (`:1`) — **самого `.hpp` в scripts-репо нет** (это engine-заголовок/данные); авторитет для скрипта — этот enum.
- Конструктор `PlayerSoundEventHandler` (`:54-99`) регистрирует все состояния через `RegisterState(new <X>SoundEvent())`.
- `RegisterState(state)` (`:101-106`): `m_AvailableStates[state.GetSoundEventID()] = state;` и
  `m_ConfigIDToScriptIDmapping.Insert(state.GetSoundVoiceAnimEventClassID(), index);` — маппинг
  **config-id → script-id** (используется из `OnSoundEvent`, см. §3).
- `PlayRequestEx(id, sent_from_server, param)` (`:148-196`): bounds-check `id > SOUND_EVENTS_MAX-1` (`:150-153`),
  `SKIP_CONTROLLED_PLAYER` (`:156-159`), приоритеты (`:166-188`), затем
  `m_CurrentState = PlayerSoundEventBase.Cast(requested_state.ClassName().ToType().Spawn());`
  `m_CurrentState.InitEx(m_Player, param); if (m_CurrentState.Play()) m_CurrentState.OnPlay(m_Player);` (`:189-194`).
- `PlayerSoundEventBase.Play()` (`playersoundeventbase.c:175-201`):
  `m_SoundSetCallback = m_Player.ProcessVoiceEvent("", "", m_SoundVoiceAnimEventClassID);` (`:182`) — **вот где config-id превращается в звук**; затем вешает `Event_OnSoundWaveEnded → OnEnd` (`:187-188`).
- `m_SoundVoiceAnimEventClassID` задаётся в конструкторе каждого события; пример
  `CoughSoundEvent` (`events/symptomevents.c:15-22`): `m_ID = SYMPTOM_COUGH; m_SoundVoiceAnimEventClassID = 8;`
  (8 ↔ config id=8 `cough_SoundVoice`, см. §4).
- `PlayerSoundEventBase` базовые: `enum EPlayerSoundEventType` (`:1-11`, битовые GENERAL/MELEE/STAMINA/…),
  `enum EPlayerSoundEventParam` (`:13-29`, `SKIP_CONTROLLED_PLAYER=1 / HIGHEST_PRIORITY=2 / STOP_PLAYBACK=4`),
  `CanPlay` (`:116-129`, гасит при hold-breath/плавании), `GetSoundVoiceAnimEventClassID` (`:70-73`).

### 2.3. `ReplaceSoundEventHandler` (перехват anim-событий)

- `enum ESoundEventType { SOUND_COMMON, SOUND_WEAPON, SOUND_ATTACHMENT, SOUND_VOICE }` (`replacesoundeventhandler.c:1-7`);
  `enum EReplaceSoundEventID` (`:9-17`).
- `ReplaceSoundEventHandler` (`:20-89`): `RegisterEvent` (`:40-51`) кладёт в **static**
  `m_MainReplaceMap[type][anim_id] = soundEvent`; `GetSoundEventID(anim_id, type)` (`:53-63`);
  `PlayReplaceSound(id, type, flags)` (`:79-89`).
- `ReplaceSoundEventBase.Play()` (`replacesoundeventbase.c:30-58`): выбирает `m_ReplacedSoundAnimID` и зовёт
  `m_Player.ProcessVoiceEvent("", m_UserString, m_ReplacedSoundAnimID)` для `SOUND_VOICE` (`:49-51`).
- Назначение — «перехватить anim-событие и подменить другим по контексту» (еда/питьё). **Требует, чтобы
  anim-событие реально сработало** — сам по себе из сервера произвольный звук не запускает.

---

## 3. Anim-событие `SoundVoice` (модель «каждый клиент сам играет»)

- `DayZPlayerTypeRegisterSounds` (`dayzplayercfgsounds.c:319-354`): `pType.RegisterSoundEvent("SoundVoice", -1)`
  (`:327`); `RegisterVoiceSoundLookupTable(voiceTable)` (`:332-333`) — **вне** `if(!IsDedicatedServer())`,
  т.е. таблица голосов регистрируется на ВСЕХ машинах (сервер+клиенты).
- `RegisterSoundEvent` — `proto native` с комментарием **«calls DayZPlayer.OnSoundEvent()»**
  (`3_game/dayzplayer.c:302-303`). Т.е. движок сам зовёт `OnSoundEvent` при попадании анимации на
  маркер `SoundVoice` (маркеры — в `.anm`-файлах).
- `OnSoundEvent(pEventType, pUserString, pUserInt)` (`dayzplayerimplement.c:3347-3406`), ветка
  `pEventType == "SoundVoice"` (`:3377-3401`):
  1. CLIENT/REMOTE: `m_ReplaceSoundEventHandler.GetSoundEventID(pUserInt, SOUND_VOICE)` → если есть, `PlaySoundEventType(SOUND_VOICE, ...)` (`:3379-3387`);
  2. иначе `m_PlayerSoundEventHandler.ConvertAnimIDtoEventID(pUserInt)` → если >0, `PlaySoundEvent(eventID)` (`:3391-3396`);
  3. иначе `ProcessVoiceEvent(...)` (`:3400`).
- `ProcessVoiceEvent` (`dayzplayerimplement.c:3577-3657`) — **`#ifdef SERVER → return null`** (`:3579-3581`),
  т.е. звук играется ТОЛЬКО на клиенте/remote:
  - `GetVoiceSoundLookupTable()` (`:3583`), категория от маски/шлема (`soundVoiceType`/`soundVoicePriority`, `:3588-3624`);
  - `table.GetSoundBuilder(pUserInt, category.Hash())` (`:3626`);
  - переменные `male`/`female` из `player.IsMale()`/`player.GetVoiceType()` (`:3632-3644`);
  - `AttenuateSoundIfNecessary(soundObject); wave = PlaySound(soundObject, soundBuilder);` (`:3650-3651`) — **позиционный звук**.
- `IsMale()` (`playerbase.c:1610-1617`) = `ConfigGetBool("woman") != 1`; `GetVoiceType()` (`playerbase.c:1619-1629`) =
  `ConfigGetInt("voiceType")` (0→1).

**Модель репликации (вопрос C) подтверждается:** анимация сервер-авторитетна (для ИИ симулируется
на сервере, состояние синхронизируется на клиентов), anim-маркеры `SoundVoice` срабатывают на КАЖДОЙ
машине, симулирующей анимацию (сервер + каждый REMOTE/AI_REMOTE). На сервере `ProcessVoiceEvent`
no-op (`#ifdef SERVER`), на каждом клиенте — играет позиционный звук. Поэтому реплику слышат все
рядом стоящие. Скриптового триггера anim-события из сервера НЕТ — событие идёт только из анимации.

**Важно (шум):** `ProcessVoiceEvent` НЕ добавляет шум на сервере (в отличие от зомби). Ванильный
`AddNoise` в `dayzplayerimplement.c` есть только в `ProcessSoundEvent` (событие `Sound`, `:3567-3571`),
а голосовые звуки игрока (`SoundVoice`) шум не генерят. Если нужно, чтобы зомби «слышали» реплики
бота — добавлять шум самим (наш `dmNoiseSystem`, как для крика зомби — см. `perception.md`).

---

## 4. Конфиг голосовых звуков (как задаётся звук)

Цепочка: `CfgVehicles SurvivorBase AnimEvents SoundVoice` (id → lookup table) → `CfgVoiceSoundTables`
(lookup table → category → soundSets[]) → `CfgSoundSets`/`CfgSoundShaders` (само аудио).

- `DayZPlayerTypeVoiceSoundLookupTableImpl` (`dayzplayercfgsounds.c:167-244`) читает
  `CfgVehicles SurvivorBase AnimEvents SoundVoice` (`:177`): `id` (`:186`), `soundLookupTable` (`:189`),
  `noise` (`:198-204`, кладёт `NoiseParams` в таблицу). `GetSoundBuilder(eventId, parameterHash)` (`:211`).
- Сами записи — `DZ/characters/data/config.cpp:7565-7796`:
  ```
  class SoundVoiceEvent { noise="NoiseStepStand"; };
  class SoundVoice {
      class cough_SoundVoice: SoundVoiceEvent { soundLookupTable="cough_SoundVoice_Char_LookupTable"; id=8; };
      class laugh_SoundVoice: SoundVoiceEvent { soundLookupTable="laugh_SoundVoice_Char_LookupTable"; id=10; };
      ... (id 1..33, 200..204, 888..906 и т.д.)
  };
  ```
- `SoundLookupTable.LoadTable(name)` (`3_game/dayzanimeventmaps.c:15-51`) читает
  `CfgSoundTables <category> <name>`; `PlayerVoiceLookupTable` (`:95-113`) =
  `InitTable("CfgVoiceSoundTables", "category")` → путь `CfgVoiceSoundTables Voice <tableName>`.
- Структура `CfgVoiceSoundTables` — `DZ/sounds/hpp/config.cpp:449700+`:
  ```
  class CfgVoiceSoundTables {
      class <tableName> {
          class None { category="none"; soundSets[]={"<SoundSet>"}; };
          class Gasmask { category="gasmask"; soundSets[]={"<Gasmask SoundSet>"}; };
          class Metalhelmet / Motohelmet / Gag { ... };
      };
  };
  ```
  `category` = значение `soundVoiceType` надетой маски/шлема (см. `ProcessVoiceEvent` §3).
- Сами SoundShader'ы голоса — `DZ/sounds/hpp/config.cpp:201096+` (`*_SoundVoice_Char_SoundShader : baseCharacter_SoundShader`).

**Для кастомной реплики нужно добавить в свой config.cpp:**
1. `class CfgSoundShaders` / `CfgSoundSets` — аудиофайл + SoundSet;
2. `class CfgVoiceSoundTables { class Voice { class dmBotLine1_Char_LookupTable { class None { category="none"; soundSets[]={"dmBotLine1_SoundSet"}; }; }; }; };`
3. `class CfgVehicles { class SurvivorBase { class AnimEvents { class SoundVoice { class dmBotLine1 { soundLookupTable="dmBotLine1_Char_LookupTable"; id=<НОВЫЙ id>; }; }; }; }; };`
   (необязательно `noise=...` — если хотим шум через зомби-сенсорику, хотя у игрока это всё равно не используется).

**Готча (выяснено при реализации):** при наследовании `baseCharacter_SoundShader`/
`baseCharacter_SoundSet` (аддон `DZ_Sounds_Effects`) нужна forward-декларация
`class baseCharacter_SoundShader;` / `class baseCharacter_SoundSet;` внутри своего блока,
иначе `CfgConvert` падает с `Undefined base class "baseCharacter_SoundShader"`.
`requiredAddons[]` это НЕ решает. Подробнее — `docs/codeguide.md`.

---

## 5. Talking-анимация (движение рта) — вопрос D

- `proto native void SetTalking(bool pValue)` — на `HumanCommandAdditives` (`3_game/human.c:1137`).
  Вызывается ТОЛЬКО из `DayZPlayerImplement.CommandHandler` (`dayzplayerimplement.c:2593/2627`) по
  `IsPlayerSpeaking()` (`:2588`, амплитуда VoN) — т.е. у ИИ (нет VoN) всегда `SetTalking(false)`.
  Там же на сервере VoN-шум раз в секунду (`AddNoise`, `:2596-2621`) — тоже только при реальном говорении.
- Граф-переменная `#Var Talking bool 0 ""` (`DZ/anims/workspaces/player/player_main/player_main.agr:110`),
  переход `FacialTalkingT` → анимация `FacialTalkingAnim` (`locomotion.agr:796`, переход `:804` с порогом
  `Talking 0.1`), сама анимация `FacialTalking` = `p_fcl_talking.anm` (`player_main.asi:26`);
  `VehicleTalkingT` (`vehicles.agr:2009`, порог `Talking 0.3`).
- `OnVoiceEvent`/`OnVoiceEventPlayback` (`playerbase.c:6220-6246`) — НЕ липсинг, а виджет дыхания
  противогаза (маска). Рот двигается только через `Talking`/`SetTalking`, НЕ от проигрывания звука.

**Вывод (D):** `SetTalking` — нативный, драйвит граф-переменную `Talking`. Для ИИ-бота рот надо
двигать вручную: звать `GetCommandModifier_Additives().SetTalking(true/false)` **на сервере** (в
`CommandHandler` бота), либо ставить граф-переменную `Talking` через `AnimSetBool` (наш кастомный граф —
паттерн `dmAI_Raised`/`dmAI_Look`, см. скилл). `Talking` НЕ входит в список engine-driven переменных
(`Raised/AimX/AimY/AimIKX`), которые движок перетирает каждый кадр, но точная перетирка `Talking`
нативом — не подтверждена (нужна проверка). Осторожно: `GetCommandModifier_Additives()` может быть
`null` без additive-команды (ваниль гейтит `if (ad)`).

---

## 6. Референсы в чужих модах (вопрос E)

- **Expansion** (`DayZ-Expansion-Scripts/`): grep по `SetTalking`/`SoundVoice`/`RequestSoundEvent`/
  `PlaySoundEvent`/`ProcessVoiceEvent`/`OnSoundEvent`/`voice` — **пусто**. ИИ Expansion голосом/репликами
  НЕ говорит; у них только `NoiseTalk`-семантика для слуха (см. `perception.md`, `eAINoiseSystem`).
- **gagasha_botikbase/config.cpp** — `voiceType=1` (мужские классы) и `voiceType=2` (женские) в
  CfgVehicles-классах ботов (строки 100/150/…/600/650/…). Это **тип голосового тембра персонажа**
  (`GetVoiceType()`, переменная `male`/`female` в `ProcessVoiceEvent`), а НЕ «реплики» — звуки всё равно
  берутся из `SoundVoice`-конфига, слов там нет. Реплик бот не произносит.
- Других модов с голосом ИИ в `/home/devalio/dayz/Work/` не найдено (проверены `MODS/`, `TerjeMods/`,
  `KUBC.*` — быстрый просмотр; полный аудит всех модов не проводился).

**Итог:** готового эталона «ИИ говорит реплики» в доступных референсах нет; делаем сами по ванильной
системе голосовых звуков.

---

## 7. Рекомендуемый путь для botorama (вопрос B/F)

Два варианта, оба используют `ProcessVoiceEvent` как финальный звук; различаются **триггером**.

### Вариант 1 (рекомендуемый, минимальный и надёжный) — своя net-var + прямой `ProcessVoiceEvent` на клиенте

1. Конфиг: добавить `SoundVoice`-запись с новым `id` + `CfgVoiceSoundTables` + SoundSet/Shader (§4).
2. На боте зарегистрировать net-var (как уже сделано для `m_LookYawDeg`, `dmAISurvivorBase.c:178-179`):
   `RegisterNetSyncVariableInt("dmAI_VoiceLine", 0, <max id>)`. Серверный мозг ставит значение + `SetSynchDirty()`.
3. На клиенте в `dmAISurvivorBase.OnVariablesSynchronized` (уже есть override, `dmAISurvivorBase.c:869`),
   под `#ifndef SERVER`: если `dmAI_VoiceLine != 0` → `ProcessVoiceEvent("", "", dmAI_VoiceLine);` и сбросить.
4. Рот: в `CommandHandler` бота (сервер) звать `SetTalking(true/false)` на время реплики (или `Talking`-var).

Почему надёжнее: `ProcessVoiceEvent` — **public** (`dayzplayerimplement.c:3577`), client-only, играет
позиционный звук на каждом клиенте; триггер — обычная net-var, уже штатный паттерн мода. Не трогаем
ванильный enum, статические реестры `PlayerSoundEventHandler` и диапазон net-var `m_SoundEvent`.

### Вариант 2 («ванильный») — расширить `EPlayerSoundEventID` + кастомный `PlayerSoundEventBase`

- `modded enum EPlayerSoundEventID { ... }` + кастомный `class dmBotLine1 : PlayerSoundEventBase { m_ID=...; m_SoundVoiceAnimEventClassID=<config id>; }` + регистрация в `modded class PlayerSoundEventHandler` (constructor → `RegisterState(new dmBotLine1())`), затем сервер `RequestSoundEventEx(dmBotLine1_ID)`.
- Ограничения (НЕ подтверждены / рискованны):
  1. **`modded enum` не подтверждён**: ни в ванили, ни в Expansion нет ни одного `modded enum` — поддержка
     расширения enum'ов движком не подтверждена (нужна пробная компиляция).
  2. Диапазон net-var `m_SoundEvent` = `0 .. ENUM_COUNT-1` (`playerbase.c:555`) — кастомный id обязан
     попасть внутрь (т.е. встать ДО `ENUM_COUNT`), иначе значение может клампиться синхронизацией.
  3. Реестр `m_AvailableStates`/`m_ConfigIDToScriptIDmapping` — **static** в `PlayerSoundEventHandler`
     (`:48-49`), т.е. глобальный: надо аккуратно регистрировать один раз.

**Вывод:** Вариант 1 проще и безопаснее (не зависит от `modded enum`, не трогает ванильные статические
реестры). Вариант 2 — только если понадобится бесплатно получить приоритеты/`STOP_PLAYBACK`/тики
`PlayerSoundEventBase` (позиционирование волны `OnTick` и т.п.).

### Таблица «что звать» (вариант 1)

| Роль | Файл:строка (ваниль) | Вызов |
|---|---|---|
| финальный звук (клиент) | `dayzplayerimplement.c:3577` `ProcessVoiceEvent(string,string,int)` | `ProcessVoiceEvent("", "", <config id>)` — public, client-only, позиционный `PlaySound` |
| триггер | паттерн мода `dmAISurvivorBase.c:178-179` (`RegisterNetSyncVariableInt`) + `:869` (`OnVariablesSynchronized`) | сервер: `m_dmAI_VoiceLine = id; SetSynchDirty();`; клиент: читать в `OnVariablesSynchronized` |
| движение рта (сервер) | `3_game/human.c:1137` `SetTalking(bool)` | `GetCommandModifier_Additives().SetTalking(true)` в `CommandHandler` (под `if (ad)`) |
| шум для зомби (опц.) | свой `dmNoiseSystem.AddNoise` (см. `perception.md`) | вызвать из той же точки, где ставим net-var |

---

## Открытые вопросы / не подтверждено

1. **`modded enum`** — расширение `EPlayerSoundEventID` не подтверждено (ни одного примера в ванили/
   Expansion); нужна пробная компиляция. Из-за этого Вариант 2 помечен рискованным.
2. **Перетирка граф-переменной `Talking` движком**: `Talking` не числится среди engine-driven
   (`Raised/AimX/AimY/AimIKX`), но точно ли `AnimSetBool("Talking", …)` удерживается (или натив
   `SetTalking` перезаписывает её каждый кадр) — не подтверждено. Надёжнее звать `SetTalking` на сервере.
3. **Клампинг net-var `m_SoundEvent`** при значении вне `0..ENUM_COUNT-1` — нативное поведение не
   подтверждено из скриптов; поэтому в Варианте 1 используем СВОЮ net-var с нужным диапазоном.
4. **`GetCommandModifier_Additives()` у ИИ-бота** — возвращает ли non-null вне additive-команды; ваниль
   всегда гейтит `if (ad)`. Проверить на боте; фолбэк — `AnimSetBool("Talking", …)`.
5. **Звук на самом «говорящем» клиенте vs соседях**: `ProcessVoiceEvent` играет на всех машинах,
   включая контролируемого игрока-наблюдателя; поведение «слышно ли самому боту» (у ИИ нет клиента)
   — звук на стороне бота не играется (нет AI-клиента), что и нужно.
6. **Точное число/список свободных `id`** для кастомных `SoundVoice`-записей (не пересечься с ванильными
   `1..33, 200..204, 888..906` и зомбиными) — сверять с `DZ/characters/data/config.cpp:7565-7796` при вводе.

## Источники (файл:строка, ваниль = `DayZ-Script-Diff/scripts/`)

- VoN: `3_game/global/game.c:1057/1065/1071/1076/1082/1087`; `3_game/global/world.c:213`;
  `3_game/dayzplayer.c:1333` (`IsPlayerSpeaking`); `3_game/vonmanager.c` (клиентский UI); `3_game/gameplay.c` (VoN-события).
- Репликация звука: `4_world/entities/manbase/playerbase.c:108-111, 555-556, 1610-1629, 2428-2429,
  2856-2857, 2991-3003, 5886-5901, 7423-7432, 7455-7471, 7473-7490, 7492-7498, 7505-7519, 7655-7678`;
  `4_world/entities/dayzplayerimplement.c:982-986, 2588-2628, 3347-3406, 3577-3657`.
- Хендлеры/события: `4_world/classes/soundevents/soundevents.c:1-92`;
  `4_world/classes/soundevents/playersoundevents/playersoundeventhandler.c:1-214`;
  `4_world/classes/soundevents/playersoundevents/playersoundeventbase.c:1-202`;
  `4_world/classes/soundevents/playersoundevents/events/symptomevents.c:15-22`;
  `4_world/classes/soundevents/replacesoundevents/replacesoundeventhandler.c:1-94`;
  `4_world/classes/soundevents/replacesoundevents/replacesoundeventbase.c:1-61`.
- Lookup-таблицы: `4_world/entities/manbase/dayzplayer/dayzplayercfgsounds.c:167-244, 319-354`;
  `3_game/dayzanimeventmaps.c:2-76, 95-113`; `3_game/dayzplayer.c:302-303` (RegisterSoundEvent).
- Зомби-путь: `4_world/entities/creatures/infected/zombiebase.c:550-596`.
- Talking: `3_game/human.c:1137`; `DZ/anims/workspaces/player/player_main/player_main.agr:110`;
  `.../locomotion.agr:796,804`; `.../player_main.asi:26`; `.../vehicles.agr:2009`.
- Конфиг данных: `DZ/characters/data/config.cpp:7565-7796` (SoundVoice), `:8644+` (voiceType=1/2);
  `DZ/sounds/hpp/config.cpp:201096+` (SoundVoice шейдеры), `:449700+` (CfgVoiceSoundTables).
- Референсы: `DayZ-Expansion-Scripts/DayZExpansion/AI/` (голоса ИИ нет — grep пуст);
  `gagasha_botikbase/config.cpp` (voiceType=1/2 — тембр, не реплики).
- Мод (интеграция): `botorama/core/4_World/Entities/Bot/dmAISurvivorBase.c:17` (класс), `:178-179`
  (net-var), `:869` (OnVariablesSynchronized), `:912` (CommandHandler).
