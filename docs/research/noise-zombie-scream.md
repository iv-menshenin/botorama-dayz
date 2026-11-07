# Research: почему крик зомби (SCREAM) не эмитит шум в dmNoiseSystem

Статус: **исследование, гипотезы для эмпирики**. Баг: хук
`modded ZombieBase.OnSoundVoiceEvent` (`core/4_World/modded/modded_ZombieBase.c:22-28`)
зовёт `dmNoiseSystem.AddNoise(this, GetPosition(), DM_NOISE_SCREAM_STRENGTH, dmNoiseType.SCREAM)`
под `#ifdef SERVER`, но в observer-проходе (живой зомби кричал рядом со слушателем-ботом)
лог `[Noise] AddNoise type=…` для `SCREAM` так и не появился — пришли только `STEP` (шаги игрока).

Вывод по коду: **хук корректен по сигнатуре и по месту, но почти наверняка ловит не тот
«крик».** У зомби ДВА независимых механизма вокализации, и тот, что слышал наблюдатель, —
клиентский state-звук, который НЕ проходит через `OnSoundVoiceEvent` (и НЕ генерит ни
нативный, ни наш шум на сервере).

---

## 1. Проверенные сигнатуры (ваниль)

### Иерархия (все в скриптах, движковых предков нет)

```
ZombieBase extends DayZInfected            zombiebase.c:1
DayZInfected extends DayZCreatureAI        dayzinfected.c:103
DayZCreatureAI extends DayZCreature        dayzanimal.c:191
DayZCreature extends EntityAI              dayzanimal.c:24
ZombieMaleBase / ZombieFemaleBase extends ZombieBase   zombiemalebase.c:1 / zombiefemalebase.c:1
```

### Регистрация anim-событий (кто зовёт `OnSoundVoiceEvent`)

`DayZCreatureAI()` ctor → `RegisterAnimEvents()` (`dayzanimal.c:215-217`).
`RegisterAnimEvents` (`dayzanimal.c:296-317`) регистрирует **на движке**:

| Событие | Функция | Гейт | Строка |
|---|---|---|---|
| `"Sound"` | `OnSoundEvent` | **без гейта** (и сервер) | `dayzanimal.c:298` |
| `"SoundVoice"` | `OnSoundVoiceEvent` | **без гейта** (и сервер) | `dayzanimal.c:303` |
| `"Step"` | `OnStepEvent` | `if (!g_Game.IsDedicatedServer())` — ТОЛЬКО клиент | `dayzanimal.c:308-311` |
| `"Damage"` | `OnDamageEvent` | без гейта | `dayzanimal.c:316` |

→ **`"SoundVoice"` регистрируется на выделенном сервере** (в отличие от `"Step"`), т.е.
движок ДОЛЖЕН диспатчить `OnSoundVoiceEvent` сервер-сайд, когда в анимации зомби срабатывает
`SoundVoice`-событие. Это косвенно подтверждается тем, что ваниль в
`ProcessSoundVoiceEvent` имеет серверную ветку (ниже).

### `ZombieBase.OnSoundVoiceEvent` — верхний override ванили

`zombiebase.c:550-575`:
```
override void OnSoundVoiceEvent(int event_id, string event_user_string)
{
    //super.OnSoundVoiceEvent(event_id, event_user_string);   // :552 — ЗАКОММЕНТИРОВАН
    AnimSoundVoiceEvent voice_event = GetCreatureAIType().GetSoundVoiceEvent(event_id);  // :553
    if (voice_event != null)                                   // :554
    {
        ... m_InfectedSoundEventHandler.Stop();                // :557-561
        ProcessSoundVoiceEvent(voice_event, m_LastSoundVoiceAW); // :571
        HandleSoundEvents();                                   // :573
    }
}
```
- `super` закомментирован → ванильный `ZombieBase.OnSoundVoiceEvent` — **верхний** override
  (перекрывает `DayZCreatureAI.OnSoundVoiceEvent`, `dayzanimal.c:269`, но не зовёт его).
- Наш `modded class ZombieBase` override + `super.OnSoundVoiceEvent(...)` → вызывается
  именно этот ванильный код. **Сигнатура совпадает 1-в-1** (`int event_id, string event_user_string`).

### `ZombieBase.ProcessSoundVoiceEvent` — нативный шум на сервере

`zombiebase.c:577-596`:
```
if (!g_Game.IsDedicatedServer()) { ... aw = PlaySound(...); }   // :579-589 — звук (клиент)
if (g_Game.IsServer())                                          // :591
    if (sound_event.m_NoiseParams != NULL)
        g_Game.GetNoiseSystem().AddNoise(this, sound_event.m_NoiseParams, ...);  // :593-594
```
→ **ваниль сама генерит нативный шум на сервере** при `SoundVoice`-событии. Это прямое
доказательство намерения разработчиков: `OnSoundVoiceEvent`/`ProcessSoundVoiceEvent` — это
**сервер-сайд** путь (иначе ветка `:591-594` — мёртвый код).

### `AnimSoundVoiceEvent` — на сервере живёт только если есть поле `noise`

`dayzanimevents.c:218-260`:
- клиент (`!IsDedicatedServer()`, `:230-238`): читает `soundSet` → `m_IsValid=true`.
- сервер (`IsServer()`, `:242-250`): читает `noise` → `m_NoiseParams.Load(noiseName)`,
  `m_IsValid=true`. **Если у анимации НЕТ поля `noise` → `m_NoiseParams==NULL`, `m_IsValid=false`.**

`LoadAnimSoundVoiceEvents` (`dayzcreatureaitype.c:34-50`) вставляет событие в `map` **только
если `soundEvent.IsValid()`**. Значит на сервере `GetSoundVoiceEvent(event_id)`
(`dayzcreatureaitype.c:105`) вернёт **null** для `SoundVoice`-событий без `noise`.

> **Следствие для нашего хука: неважно.** Наш override НЕ проверяет `voice_event == null` —
> он эмитит шум безусловно, как только `OnSoundVoiceEvent` вызван. Поэтому «`GetSoundVoiceEvent`
> вернул null» **не объясняет** баг — это объяснило бы только отсутствие *нативного* шума.

### Клиентские state-звуки (второй механизм вокализации)

`zombiebase.c`:
- `m_InfectedSoundEventHandler = new InfectedSoundEventHandler(this);` создаётся **только
  `if (!g_Game.IsDedicatedServer())`** (`:63-66`).
- `HandleSoundEvents()` помечен «Sound (client only)» (`:487`), свитчует по `m_MindState` и
  запускает state-звук (`CalmIdle/CalmMove/DisturbedIdle/ChaseMove/AlertedIdle/AlertedMove`,
  `infectedsoundeventhandler.c:5-20`).
- State-звук играется через `InfectedSoundEventBase.Play` →
  `m_Infected.ProcessVoiceFX(soundset_name)` (`infectedsoundeventbase.c:57-58`);
  `ProcessVoiceFX` обёрнут `if (!g_Game.IsDedicatedServer())` (`zombiebase.c:531`).

→ **Эти звуки — чисто клиентские, `SoundSet`-петли, НЕ anim-события и НЕ шум.** Они не
проходят `OnSoundVoiceEvent`, не зовут ни нативный `AddNoise`, ни наш хук.

### Переход mindstate — серверная точка «зомби заорал/заметил»

`HandleMindStateChange(int pCurrentCommandID, DayZInfectedInputController pInputController, float pDt)`
— `zombiebase.c:450-478`, вызывается из серверного `ModCommandHandlerInside` (`:260`):
```
m_MindState = pInputController.GetMindState();     // :453
if (m_LastMindState != m_MindState) { ... SetSynchDirty(); }  // :455-475
```
`m_MindState` — зарегистрированная net-sync-переменная (`:54 RegisterNetSyncVariableInt("m_MindState", -1, 4)`).
Клиент на `OnVariablesSynchronized` (`:80-86`) зовёт `HandleSoundEvents()` → проигрывает state-звук.

→ **Один из механизмов «агро-крика»:** сервер меняет mindstate (CALM→DISTURBED→ALERTED→CHASE) →
`SetSynchDirty()` → клиент играет `Alerted*`/`Chase*`-SoundSet. Это **клиентский** звук без шума.
**Открыто:** анимации ALERT/CHASE могут *также* содержать дискретное `SoundVoice`-событие (роер),
которое давало бы и нативный, и наш шум — это и есть развилка H1 vs H2 (см. §3/§4).
`HandleMindStateChange` — готовый серверный override-хук (эталон: Expansion
`zmbm_gamedev_base.c:4` `override bool HandleMindStateChange(...)`).

---

## 2. Наш мод (факт)

- `core/4_World/modded/modded_ZombieBase.c:22-28`:
  ```
  override void OnSoundVoiceEvent(int event_id, string event_user_string)
  {
      super.OnSoundVoiceEvent(event_id, event_user_string);
      #ifdef SERVER
      dmNoiseSystem.AddNoise(this, GetPosition(), DM_NOISE_SCREAM_STRENGTH, dmNoiseType.SCREAM);
      #endif
  }
  ```
  Эмиссия **безусловная** (не зависит от `voice_event`), `DM_NOISE_SCREAM_STRENGTH = 30.0`
  (`cons/3_Game/constants.c:61`).
- `dmNoiseSystem.AddNoise` (`core/3_Game/Perception/dmNoiseSystem.c:41-51`): лог
  `[Noise] AddNoise type=… src=… strength=…` гейтится `#ifdef DM_PERCEPTION_DEBUG` (`:45-48`).
- Приёмник `dmHearing.OnNoise` (`dmHearing.c:15-66`): **не фильтрует по типу**, отсекает только
  дистанцию (`d.LengthSq() > strength²`) и собственный шум (`root == m_Bot.GetPawn()`). Т.е.
  если бы `SCREAM` был эмитнут в радиусе 30 м — он дошёл бы до `HearNoise`. Отсутствие именно
  **эмиссионного** лога означает: **хук не вызвался**.
- Эталонный рабочий хук `STEP` — `core/4_World/modded/modded_DayZPlayerImplement.c:11-34`
  (`override void OnStepEvent(...)` под `#ifdef SERVER`). Он лежит **в том же модуле**
  (`core/4_World/modded/`), с тем же `#ifdef SERVER`, и работает → модуль компилируется,
  `SERVER`-гейт активен, override-механизм для 4_World-класса исправен.

---

## 3. Гипотезы

### H1 (главная) — наблюдаемый «крик» это клиентский state-звук, а не `SoundVoice`-событие

**(а)** «Крик/рык/стон», который слышал наблюдатель, — mindstate-звук
(`AlertedIdle`/`AlertedMove`/`ChaseMove`/`DisturbedIdle` SoundSet), играемый через
`ProcessVoiceFX` **на клиенте**. Он не проходит `OnSoundVoiceEvent` → нет ни нативного, ни
нашего шума. `OnSoundVoiceEvent` (и наш хук) срабатывает только на **дискретные** роеры,
вшитые в анимации атаки/смерти/удара (`SoundVoice`-события), а не на переход в ALERTED.

**(б) как проверить:**
1. В `modded_ZombieBase.OnSoundVoiceEvent` добавить **безусловный** лог в самом начале
   (до `super`), напр. `dmBotLog.Debug("", "[Zmb] OnSoundVoiceEvent event_id=" + event_id)`
   под `#ifdef DM_PERCEPTION_DEBUG` (или временный `Print`), и отдельный лог в
   `HandleMindStateChange` (`[Zmb] mindstate → ALERTED/CHASE`).
2. Спавн **живого** зомби с ИИ (`init_ai=true` — см. H3), рядом бот-слушатель; включить
   `DM_PERCEPTION_DEBUG` (`--test`).
3. Сценарий А: зомби заметил бота (агро) → НЕ атакует, просто кричит/рычит (переход в
   ALERTED/CHASE). Сценарий Б: зомби доходит и **атакует** бота (анимации удара) / бот убивает
   зомби (анимация смерти).
4. Сопоставить лог: в сценарии А — есть `[Zmb] mindstate → ALERTED/CHASE`, но **нет**
   `[Zmb] OnSoundVoiceEvent` и нет `[Noise] AddNoise type=2`; в сценарии Б — появляется
   `[Zmb] OnSoundVoiceEvent` + `[Noise] AddNoise type=2`.

**(в) ожидаемый результат (если H1 верна):** агро-крик не даёт `OnSoundVoiceEvent` (это
клиентский state-звук), а атака/смерть — дают (и наш `SCREAM` приходит). Вывод: хук корректен,
но «крик» надо ловить в `HandleMindStateChange` (H5), а не в `OnSoundVoiceEvent`.

### H2 — `SoundVoice`-события вообще НЕ диспатчатся на выделенном сервере

**(а)** Несмотря на регистрацию (`dayzanimal.c:303` без гейта) и серверную ветку в
`ProcessSoundVoiceEvent`, движок шлёт `SoundVoice`-события **только клиентам** (анимационные
события голоса сервером не эмитятся; ветка `:591-594` — legacy/мёртвый код).

**(б) как проверить:** тот же безусловный лог в `OnSoundVoiceEvent` + сценарий Б (атака/смерть,
где `SoundVoice` точно есть в анимациях). Если даже при атаке/смерти на сервере **ни разу** не
появляется `[Zmb] OnSoundVoiceEvent` — H2 подтверждается.

**(в) ожидаемый результат (если H2 верна):** `OnSoundVoiceEvent` не вызывается на сервере
вовсе → наш хук мёртв при любом сценарии → нужна альтернатива (H5). Отличие от H1 — в H1 хук
«живой», но ловит не тот звук.

### H3 — тестовый зомби был `init_ai=false` («безмозглый»)

**(а)** Зомби, которого наблюдал тест, был спавнут через `spawnobj`
(`dmE2EBridge.c:2715-2732`: `GetGame().CreateObject(className, pos, false)` — 3 аргумента →
`init_ai=false`, без ИИ) либо иным способом без ИИ. Такой зомби не имеет AI → не переходит
mindstate, не агрится, не играет анимаций атаки → **никаких `SoundVoice`-событий вообще**.
Он лишь стоит/idle и издаёт клиентский ambient-звук. (По `codeguide.md`: `IsInherited(ZombieBase)`
для `init_ai=false` зомби возвращает `false`; `ZombieBase.Cast(e)` — не-null.)

**(б) как проверить:** перед сценарием проверить ИИ-ность зомби (лог `IsInherited(ZombieBase)`
или `GetAIAgent() != null`). Спавнить зомби **с ИИ**: `CreateObject(cls, pos, false, true)`
(4-й аргумент `init_ai=true`, см. `docs/decisions.md` «melee: спавн зомби»). Сейчас `spawnobj`
так не умеет — нужен расширенный probe/`spawnzombie` (`[нужно подтвердить]` ручка).

**(в) ожидаемый результат (если H3 верна):** с реальным ИИ-зомби (агро→атака) хук
`OnSoundVoiceEvent` срабатывает при атаке/смерти → `SCREAM` появляется. Баг был артефактом
тестового зомби, а не кода.

### H4 — override-резолюция/класс (маловероятно)

**(а)** `modded class ZombieBase` не перекрывает фактический класс зомби / override не применяется.

**(б) как проверить:** уже косвенно опровергнуто — конструктор `modded_ZombieBase.c:3-6`
(`RegisterZombie`) работает (зомби детектятся через `dmEntityRegistry`), а рабочий `STEP` лежит
в том же модуле `core/4_World/modded/` и подтверждает компиляцию/гейт. Дополнительно можно
воткнуть лог в конструктор `ZombieBase`.

**(в) ожидаемый результат (если H4 верна):** зомби не регистрировались бы / STEP тоже не работал
— не наблюдается. Исключаем.

### H5 — альтернативная точка захвата «зомби заорал»: `HandleMindStateChange`

**(а)** Если `OnSoundVoiceEvent` не ловит агро-крик (H1) или не вызывается вовсе (H2) — правильная
серверная точка «зомби заметил/заорал» — `ZombieBase.HandleMindStateChange` (`zombiebase.c:450-478`),
вызываемая из серверного `ModCommandHandlerInside` (`:260`) на каждом изменении `m_MindState`.
Там же доступна цель через `pInputController.GetTargetEntity()` (эталон Expansion
`zmbm_gamedev_base.c:4`). Переход в `MINDSTATE_ALERTED`/`MINDSTATE_CHASE` = «агро-крик» → эмитим
наш `SCREAM` (в `modded ZombieBase` override `HandleMindStateChange`, зовём `super` и дописываем
свой `dmNoiseSystem.AddNoise`).

**(б) как проверить:** добавить override `HandleMindStateChange` (под `#ifdef SERVER`), лог + эмит
`SCREAM` на переходе `m_LastMindState < ALERTED` → `m_MindState >= ALERTED` (или `CHASE`/`FIGHT`);
прогнать сценарий А (агро). Наблюдать `[Noise] AddNoise type=2` без атаки.

**(в) ожидаемый результат (если H5 верна):** агро-крик (без атаки) даёт `SCREAM`-шум; бот-слушатель
слышит «зомби заорал» и может реагировать. Семантически это ближе к реальному «крику при обнаружении»,
чем роер в анимации удара.

---

## 4. Открытые вопросы / `[нужно подтвердить]`

1. **[нужно подтвердить]** Какие именно анимации зомби содержат `SoundVoice`-события с полем
   `noise` (атака? смерть? агро-переход?). Конфиг анимаций (`CfgVehicles … AnimEvents SoundVoice`)
   лежит в игровых данных (не в Script-Diff) — прочитать напрямую не удалось. Эмпирика H1/H2
   даст ответ: появление/отсутствие `[Zmb] OnSoundVoiceEvent` на атаку/смерть/агро.
2. **[нужно подтвердить]** Диспатчатся ли `SoundVoice`-события на выделенном сервере вообще
   (H1 vs H2). Разница определяет, чинить ли хук (оставить `OnSoundVoiceEvent` и дополнить
   `HandleMindStateChange`) или полностью переносить захват крика на `HandleMindStateChange`.
3. **[нужно проверить]** Есть ли probe-ручка спавна зомби **с ИИ** (`init_ai=true`). Сейчас
   `spawnobj` спавнит без ИИ (`CreateObject(cls,pos,false)`), что не воспроизводит агро/атаку →
   нужен `spawnzombie`/расширение `spawnobj` (4-й аргумент `init_ai=true`). Без этого H1/H2/H3
   не проверить детерминированно (`blocked on handle`).
4. **[нужно проверить]** `HandleMindStateChange` действительно вызывается на сервере у ИИ-зомби
   (а не только у gamedev-класса `ZmbM_Gamedev_Base`). Оно в базовом `ZombieBase`
   (`zombiebase.c:450`) и зовётся из `ModCommandHandlerInside` (`:260`) — серверный AI-цикл, но
   подтвердить логом на реальном зомби.
5. **[нужно проверить]** Частота/спам `SCREAM` при переключении mindstate туда-сюда: нужен ли
   троттлинг (как `dm_LastVoiceNoiseTime` для VOIP) — иначе бот может получить N шумов за агро-цикл.
6. **[нужно подтвердить]** `GetTargetEntity()` на `DayZInfectedInputController` — возвращает ли
   фактическую цель (игрока/бота) в момент агро, чтобы при желании атрибутировать «кто заорал
   и на кого» (для `dmTarget`-памяти угрозы).

---

## 5. Существенные развилки / принятые решения (для `docs/decisions.md`)

1. **Захват «крика зомби» — через `OnSoundVoiceEvent` (anim-событие) или через
   `HandleMindStateChange` (mindstate-переход)?**
   Почему: `OnSoundVoiceEvent` ловит только `SoundVoice`-anim-события (атака/смерть/роер) и
   генерит нативный шум, но **агро-крик** (переход CALM→ALERTED/CHASE) в ванили проигрывается
   клиентским state-звуком через `m_MindState`-синк и в `OnSoundVoiceEvent` не попадает.
   Решение: семантически «зомби заорал/заметил» = `HandleMindStateChange`; `OnSoundVoiceEvent`
   оставить для дискретных роеров, если подтвердится, что они диспатчатся на сервере.
   Действует пока: не подтверждено эмпирикой (H1 vs H2) — см. §4.

2. **`spawnobj` не подходит для теста агро-крика (спавнит без ИИ).**
   Почему: `CreateObject(cls, pos, false)` = `init_ai=false` → зомби без AI не агрится и не
   играет ни `SoundVoice`, ни mindstate-переходов. Нужен probe `spawnzombie`/`init_ai=true`.
   Действует пока: не добавлена ручка спавна зомби с ИИ.

## Источники

- ваниль: `4_world/entities/creatures/infected/zombiebase.c` (1, 17, 51-66, 80-86, 184, 260,
  355, 450-478, 487-523, 526-546, 550-596, 874); `3_game/entities/dayzinfected.c:103`;
  `3_game/entities/dayzanimal.c` (24, 191, 215-217, 260, 269, 296-317, 322, 342);
  `3_game/dayzanimevents.c:218-260`; `3_game/entities/dayzcreatureaitype.c:34-50,105`;
  `4_world/classes/soundevents/infectedsoundevents/infectedsoundeventbase.c:57-58`;
  `4_world/classes/soundevents/infectedsoundevents/infectedsoundeventhandler.c:5-20`;
  `4_world/classes/soundevents/infectedsoundevents/events/mindstates.c`.
- Expansion: `DayZExpansion/AI/Scripts/4_World/DayZExpansion_AI/Entities/zmbm_gamedev_base.c:4`.
- mod: `core/4_World/modded/modded_ZombieBase.c:22-28`; `core/3_Game/Perception/dmNoiseSystem.c:41-51`;
  `core/4_World/Entities/Bot/Perception/dmHearing.c:15-66`;
  `core/4_World/modded/modded_DayZPlayerImplement.c:11-34`; `cons/3_Game/constants.c:61`;
  `test/4_World/dmE2EBridge.c:2715-2732`.
