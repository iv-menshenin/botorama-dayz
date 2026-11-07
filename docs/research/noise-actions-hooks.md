# Research: хуки для шумов «рубка дерева / копание» и «голос (VON)»

Статус: **исследование**. Атом A3 генератора шумов. Цель — точные точки перехвата двух
типов шума игрока: (1) рубка дерева / копание, (2) голос (VON/VOIP). Двери и перезарядка
уже покрыты в `noise-generator-sources.md` (R2a/R2b) — здесь не дублируются.

Смежное (не дублируем): `noise-generator-sources.md` §2.5 (таблица нативных `AddNoise*`),
§2.6 (VOIP). Приёмник/типы — `dmNoiseSystem.c` (`enum dmNoiseType`: `OTHER/STEP/SCREAM/
BULLETIMPACT/SHOT/EXPLOSION/DROP/SOUND` — **`SOUND` и `EXPLOSION` уже добавлены**, вопреки
строке в старом R2). Сигнатура: `dmNoiseSystem.AddNoise(EntityAI source, vector position,
float strength, int type = OTHER)` (`src/core/3_Game/Perception/dmNoiseSystem.c:26`).

---

## Вопрос 1 — рубка дерева / копание

### 1.1 Точные классы и файлы (ваниль 1.2x)

| Действие | Класс | Файл | CommandUID |
|---|---|---|---|
| **Рубка дерева** | **`ActionMineTree`** (НЕ `ActionChopWood` — такого нет) | `4_world/classes/useractionscomponent/actions/continuous/actionminetree.c:66` | `CMD_ACTIONFB_HACKTREE` (`:71`; `3_game/dayzplayer.c:847` = 86) |
| Добыча камня | `ActionMineRock` / `ActionMineRock1H` | `.../continuous/actionminerock.c:11` / `:60` | `CMD_ACTIONFB_MINEROCK` (`:16`; `dayzplayer.c:866` = 109) / `CMD_ACTIONFB_ASSEMBLE` |
| **Копание червей** | **`ActionDigWorms`** | `.../continuous/actiondigworms.c:17` | `CMD_ACTIONFB_DIGGIN_WORMS` (`:22`; `dayzplayer.c:886` = 128) |
| Копание грядки | `ActionDigGardenPlot` | `.../continuous/actiondiggardenplot.c:13` | `CMD_ACTIONFB_DIG`/`DIGMANIPULATE` |
| Выкопать/закопать тайник | `ActionDigOutStash` / `ActionDigInStash` | `.../continuous/actiondigoutstash.c:12` / `actiondiginstash.c` | `CMD_ACTIONFB_DIGUPCACHE` |

- **НЕТ** класса `ActionChop` / `ActionDigFishingWorm` (grep пуст). «Рубка дерева» = `ActionMineTree`.
- Условие рубки: `targetObject.IsTree() && targetObject.IsCuttable()` (`actionminetree.c:97`).
- Грядка/тайник — **однократные** экшны (`CAContinuousTime`, `HasMultipleExecution=false`),
  их `OnFinishProgressServer` срабатывает один раз на завершение — для «шума на каждый копок»
  не годятся; черви (`ActionDigWorms`) — многократный.

### 1.2 Иерархия наследования

```
ActionMineTree  ─┐
ActionMineRock  ─┼→ ActionMineBase → ActionContinuousBase → AnimatedActionBase → ActionBase
ActionDigWorms  ──────────────────→ ActionContinuousBase → ...
```
- `ActionMineBase : ActionContinuousBase` (`actionminetree.c:17`).
- `ActionContinuousBase : AnimatedActionBase` (`actions/actioncontinuousbase.c:120`).
- `ActionDigWorms : ActionContinuousBase` (`actiondigworms.c:17`).

### 1.3 Два серверных хука «на цикл»

Цепочка A (per material-drop / per-dig, через action-компонент):
```
CAContinuousBase.OnCompletePogress(action_data)                      // actioncomponents/cacontinuousbase.c:8
  → ActionContinuousBase.OnFinishProgress(action_data)               // actioncontinuousbase.c:243
      → (server) OnFinishProgressServer(action_data)                 // actioncontinuousbase.c:283
```
- `OnFinishProgressServer(ActionData action_data)` — **protected, пустой** (`:283`), переопределяется.
- Вызывается НЕ только на завершение всего action'а, а **на каждый цикл компонента**
  (`OnCompletePogress` дёргается из `Execute` компонента при каждом накоплении времени цикла).

Цепочка B (per animation event, через анимацию):
```
AnimatedActionBase.OnAnimationEvent(action_data)                     // animatedactionbase.c:184
  if (m_DelayedAnimationEventID == UA_ANIM_EVENT)                    // :186
  if (HasMultipleExecution(action_data) || !m_WasExecuted)           // :188
    → OnExecuteServer(action_data)                                   // :197 (server)
```
- `OnExecuteServer(ActionData action_data)` — **protected** (`animatedactionbase.c`, объявлен в
  `AnimatedActionBase`); срабатывает на каждый `UA_ANIM_EVENT` только если `HasMultipleExecution()==true`.

### 1.4 Гранулярность по каждому action'у

| Action | ActionComponent (`CreateActionComponent`) | `HasMultipleExecution` | `OnFinishProgressServer` | `OnExecuteServer` |
|---|---|---|---|---|
| `ActionMineTree` (рубка) | `CAContinuousMineWood(4)` (`actionminetree.c:13`; `TIME_BETWEEN_MATERIAL_DROPS_DEFAULT=4`, `actionminetree.c:9`) | **`true`** (`actionminetree.c:116`) | **на каждый дроп материала (~4 с)** | **на каждый замах (`UA_ANIM_EVENT`)** |
| `ActionMineRock` (камень) | `CAContinuousMineRock(8)` (`actionminerock.c:7`; `TIME_BETWEEN_MATERIAL_DROPS=8`) | `false` (не override) | на каждый дроп (~8 с) | **только 1 раз** |
| `ActionDigWorms` (черви) | `CAContinuousRepeat(UATimeSpent.DIG_WORMS=10)` (`actiondigworms.c:11`; `actionconstants.c:79`) | `false` (не override) | **на каждый копок (~10 с; ×1.2 нож, ×cold-модиф.)** | 1 раз |

Детали компонентов (все вызывают `OnCompletePogress` → `OnFinishProgress(Server)`):
- `CAContinuousMineWood.Execute` (`cacontinuousminewood.c`) — на каждый цикл `m_TimeElpased >=
  m_AdjustedTimeBetweenMaterialDrops` делает дроп и `OnCompletePogress` (`:115` — последний дроп,
  `:120` — промежуточный), затем `m_TimeElpased -= cycle` (`:119`). Т.е. **per-drop, включая последний**.
- `CAContinuousMineRock : CAContinuousMineWood` (`cacontinuousminerock.c:1`) — то же, таймер 8 с.
- `CAContinuousRepeat.Execute` (`cacontinuousrepeat.c:35-47`) — на каждый цикл `OnCompletePogress`
  (`:46`), возвращает `UA_PROCESSING` (зациклен до отпускания). Т.е. **per-dig, бесконечно**.

### 1.5 Вывод по Вопросу 1 (точка хука)

- **Рубка дерева — многократный action** (`HasMultipleExecution()==true`), несколько замахов.
  `OnFinishProgressServer` = **per material drop (~4 с), НЕ per замах**. Per-замах = `OnExecuteServer`.
- **Копание червей** — `OnFinishProgressServer` = **per копок (~10 с)**.
- **Добыча камня** — `OnFinishProgressServer` = per дроп (~8 с); `OnExecuteServer` срабатывает
  **один раз** (не per-цикл), т.к. `HasMultipleExecution` не переопределён.

**Рекомендация (единый хук):** `modded class ActionMineTree` / `ActionDigWorms` / `ActionMineRock`
override `OnFinishProgressServer(ActionData action_data)` → `super.OnFinishProgressServer(...)` +
`dmNoiseSystem.AddNoise(action_data.m_Player, action_data.m_Player.GetPosition(), DM_NOISE_*_STRENGTH, SOUND)`.

Почему `OnFinishProgressServer`, а не `OnExecuteServer`:
1. Консистентно по всем трём (`OnExecuteServer` для камня/червей срабатывает один раз — не per-цикл).
2. Совпадает с «смысловым» событием (фактически произведённый материал/червь = реальный шум).
3. `action_data.m_Player` (источник) и позиция доступны напрямую (`action_data.m_Target.GetObject()
   .GetPosition()` — позиция дерева/камня, если нужен источник на объекте, а не на игроке).

Если нужен именно **per-замах** для рубки (шум каждые ~1 с, а не каждые 4 с) — override
`OnExecuteServer` **только** в `ActionMineTree` (там `HasMultipleExecution()==true`, т.е. реально
per-замах). Для камня/червей этот хук не подходит.

`[нужно подтвердить]` — фактический период `UA_ANIM_EVENT` (замах) у `CMD_ACTIONFB_HACKTREE`
([end,end2]) против таймера дропа 4 с: сколько раз `OnExecuteServer` срабатывает между двумя
`OnFinishProgressServer` (это даёт «шум на каждый замах» vs «шум на дроп»).

---

## Вопрос 2 — голос (VON/VOIP)

### 2.1 Два кандидата

**Кандидат A: `CommandHandler`** (единственный рабочий серверный хук VON).

`DayZPlayerImplement.CommandHandler(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)`
— `4_world/entities/dayzplayerimplement.c:2271`, объявлен как `override` (база — `DayZPlayer`,
`3_game/dayzplayer.c:1203`, виртуальный per-tick «deterministic simulation»). Внутри VOIP-ветка `:2588-2624`:
```
float amplitude = IsPlayerSpeaking();                       // :2588
if (amplitude > 0.1 || force) {                            // :2590
    if (g_Game.IsServer()) {                               // :2596  ← серверная ветка
        int now = g_Game.GetTime();
        if (now >= m_NextVoNNoiseTime) {                   // :2599  ← троттлинг 1 с
            m_NextVoNNoiseTime = now + 1000;               // :2601
            int voiceLevel = g_Game.GetVoiceLevel(this);   // :2603
            DayZPlayerType pt = GetDayZPlayerType();       // :2604
            NoiseParams vonpar;
            switch (voiceLevel) {
                case VoiceLevelWhisper: vonpar = pt.GetNoiseParamsWhisper(); break; // :2611
                case VoiceLevelTalk:    vonpar = pt.GetNoiseParamsTalk();    break; // :2614
                case VoiceLevelShout:   vonpar = pt.GetNoiseParamsShout();   break; // :2617
            }
            AddNoise(vonpar, NoiseAIEvaluate.GetNoiseReduction(g_Game.GetWeather())); // :2622
        }
    }
}
```
Поле троттлинга `protected int m_NextVoNNoiseTime` (`:147`, инициализация `:195`).

**Кандидат B: `OnSoundEvent("SoundVoice")` — НЕ подходит (клиент-only, не VON).**

`DayZPlayerImplement.OnSoundEvent(string pEventType, string pUserString, int pUserInt)`
(`:3347`); ветка `pEventType == "SoundVoice"` (`:3377`) → `ProcessVoiceEvent(...)` (`:3400`).
`ProcessVoiceEvent` (`:3577`) начинается с `#ifdef SERVER return null; #endif` (`:3579-3581`).
Т.е. **на сервере «SoundVoice» ничего не делает**; это клиентские anim-звуки «голоса»
(грызёт/ест/кряхтит — см. комментарий про «eating X forcefeeding» `:3387-3388`), а НЕ микрофон VON.
Старый R2 (§2.6) предлагал его как «проще» — **это ошибка**, VON через `OnSoundEvent` не ловится.

### 2.2 Сигнатуры (подтверждено)

| Символ | Сигнатура | Файл:строка |
|---|---|---|
| `IsPlayerSpeaking()` | `proto native float IsPlayerSpeaking();` | `3_game/dayzplayer.c:1333` |
| `GetVoiceLevel(...)` | `proto native int GetVoiceLevel(Object player = null);` («on both client and server») | `3_game/global/game.c:1076` |
| `VoiceLevel` | `enum VoiceLevel { Whisper=0, Talk=1, Shout=2 }` | `3_game/constants.c:17-21` |
| `GetDayZPlayerType()` | `proto native DayZPlayerType GetDayZPlayerType();` | `3_game/dayzplayer.c:1169` |
| `GetNoiseParamsWhisper/Talk/Shout()` | `NoiseParams GetNoiseParamsX()` (поля конфига) | `3_game/dayzplayer.c:391/396/401` |
| `AddNoise` (helper) | `void AddNoise(NoiseParams noisePar, float noiseMultiplier = 1.0)` → `GetNoiseSystem().AddNoise(this, ...)` | `dayzplayerimplement.c:3204-3208` |
| `NoiseParams` | `Load(string)` / `LoadFromPath(string)` | `3_game/noise.c:13` |

### 2.3 Какой хук проще/надёжнее — вывод

**Только `CommandHandler`** работает на сервере и несёт VON-семантику. `OnSoundEvent("SoundVoice")`
— клиент-only anim-звук, для VON не подходит. `CommandHandler`:
- override-абельный (в ванили уже `override`, база `DayZPlayer`);
- на сервере `IsPlayerSpeaking()` возвращает амплитуду VON (ваниль сама так делает, `:2588`);
- `GetVoiceLevel(this)` работает на сервере (doc-comment `game.c:1074`);
- **«раз в N сек» троттлинг уже есть** в ванили: `m_NextVoNNoiseTime` (1 с). `OnSoundEvent` троттлинга не имеет.

**Рекомендуемый хук:**
```
modded class DayZPlayerImplement {
    override void CommandHandler(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished) {
        super.CommandHandler(pDt, pCurrentCommandID, pCurrentCommandFinished);
        #ifdef SERVER
        // зеркалим ванильный VON-шум в dmNoiseSystem (нативный AddNoise идёт боту/зомби мимо)
        if (IsPlayerSpeaking() > 0.1) {
            int now = g_Game.GetTime();
            if (now >= m_dmVoiceNoiseNextTime) {          // свой троттлинг 1 с (не переиспользуем m_NextVoNNoiseTime)
                m_dmVoiceNoiseNextTime = now + 1000;
                float strength;
                switch (g_Game.GetVoiceLevel(this)) {
                    case VoiceLevelWhisper: strength = DM_NOISE_VOICE_WHISPER; break;
                    case VoiceLevelTalk:    strength = DM_NOISE_VOICE_TALK;    break;
                    case VoiceLevelShout:   strength = DM_NOISE_VOICE_SHOUT;   break;
                }
                dmNoiseSystem.AddNoise(this, GetPosition(), strength, dmNoiseType.SOUND);
            }
        }
        #endif
    }
    float m_dmVoiceNoiseNextTime;   // новое поле (instance) для своего троттлинга
}
```
- Не переиспользуем `m_NextVoNNoiseTime`, т.к. после `super` его значение уже «следующее»,
  а «сработал ли ванильный шум в этом кадре» не читается — проще свой таймер.
- `IsPlayerSpeaking() > 0.1` естественно отсекает ИИ-ботов (они не говорят в VON) — self-noise не возникает.
- `dmNoiseType.SOUND` уже есть; при желании выделить `VOICE`/`SHOUT` — отдельным типом.

`[нужно подтвердить]` — `IsPlayerSpeaking()` на выделенном сервере возвращает не-ноль для
говорящего REMOTE-игрока (ваниль это подразумевает, но явного подтверждения нет). Probe: человек
говорит в VON → лог `dmNoiseSystem.AddNoise` с `strength=VOICE_*`.

`[нужно подтвердить]` — `g_Game.GetVoiceLevel(this)` на сервере корректно различает whisper/talk/shout
(репликация уровня голоса от клиента).

---

## Вопрос 3 — существующий modded-класс `DayZPlayerImplement`

Файл `src/core/4_World/modded/modded_DayZPlayerImplement.c` (прочитан целиком, 41 строка). Сейчас в нём:
- `modded class DayZPlayerImplement` (`:5`);
- `override void OnStepEvent(string, string, int)` (`:7`) — шаги (уже шлёт `dmNoiseSystem.AddNoise(... STEP)`);
- `Hive GetHive()` (`:35`) — заглушка для AI-ботов (не `override`, shadow глобального `GetHive()`).

**НЕ переопределены:** `CommandHandler`, `OnSoundEvent`, `OnEndServer` и др. — **конфликта нет**,
новый `override void CommandHandler` можно добавить в этот же файл (тот же `modded class`).

Внимание к связке: `dmAISurvivorBase` (бот) **уже** override'ит `CommandHandler`
(`src/core/4_World/Entities/Bot/dmAISurvivorBase.c:1237`) и зовёт `super.CommandHandler(...)`
(`:1273`). Наш `modded class DayZPlayerImplement.CommandHandler` встанет В ЦЕПОЧКУ:
`dmAISurvivorBase.CommandHandler` → `super` → (наш) `DayZPlayerImplement.CommandHandler` → `super` → ваниль.
Для ботов `IsPlayerSpeaking()==0`, поэтому лишнего шума не будет; порядок override не конфликтует.

---

## Открытые вопросы / [нужно проверить] (для dayz-tester)

1. **[нужно подтвердить]** `modded class ActionMineTree`/`ActionDigWorms`/`ActionMineRock`
   (лежат в `4_world/classes/useractionscomponent/`) переопределяет `OnFinishProgressServer`
   на выделенном сервере (как и `HandActionThrow` в старом R1 — проверить загрузку modded-ов
   из `core/4_World/modded/`). Probe: человек рубит дерево/копает червей → лог
   `dmNoiseSystem.AddNoise type=SOUND src=SurvivorBase pos=...`.
2. **[нужно проверить]** Гранулярность рубки: сколько `OnFinishProgressServer` (дропов ~4 с) и
   `OnExecuteServer` (замахов) за одну полную рубку дерева — чтобы выбрать «шум на замах» vs
   «шум на дроп». Probe: рубка до конца → счётчик вызовов обоих хуков в логе.
3. **[нужно подтвердить]** `IsPlayerSpeaking()` > 0.1 на выделенном сервере для говорящего человека
   (и что `GetVoiceLevel` различает whisper/talk/shout). Probe: человек в VON → лог
   `dmNoiseSystem.AddNoise` с `strength=VOICE_*`.
4. **[нужно проверить]** Что `OnSoundEvent("SoundVoice")` действительно НЕ срабатывает на сервере
   (подтвердить «клиент-only» через лог в modded `OnSoundEvent` при речи в VON) — чтобы закрыть
   красную тропу из старого R2 §2.6.
5. **[нужно проверить]** Боты не порождают эти шумы сами (нет action-manager'а → `ActionMineTree`
   не запускается ботом; `IsPlayerSpeaking()==0` для бота) — self-noise отсекается без доп. гейтов.

---

## Существенные развилки / принятые решения (для docs/decisions.md)

1. **«Рубка дерева» = `ActionMineTree`, а не `ActionChopWood`.** Класса `ActionChopWood` в 1.2x
   нет; рубку делает `ActionMineTree` (`CMD_ACTIONFB_HACKTREE`). Решение: хук ставится на
   `ActionMineTree`/`ActionMineRock`/`ActionDigWorms` (семья `ActionMineBase`/`ActionContinuousBase`).
2. **Единый per-цикл хук = `OnFinishProgressServer`, а не `OnExecuteServer`.** Почему:
   `OnFinishProgressServer` срабатывает на каждый смысловой цикл (дроп/копок) консистентно для всех
   трёх; `OnExecuteServer` per-замах только у `ActionMineTree` (HasMultipleExecution=true), у камня/
   червей — один раз. Per-замах рубки (если понадобится) — отдельный override `OnExecuteServer`
   только в `ActionMineTree`.
3. **VON ловится в `CommandHandler`, а не в `OnSoundEvent("SoundVoice")`.** Почему: `ProcessVoiceEvent`
   на сервере `return null` (`#ifdef SERVER`), «SoundVoice» — клиентские anim-звуки (грызёт/ест), не
   микрофон. `CommandHandler` — серверный per-tick хук с готовым 1-с троттлингом и доступом к
   `IsPlayerSpeaking()`/`GetVoiceLevel()`. Это исправляет красную тропу старого R2 §2.6.
