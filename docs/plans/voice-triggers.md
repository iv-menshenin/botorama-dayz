# Оживление ботов — голосовые реплики по событиям

> Статус: **реализовано полностью** (v3.483–v3.490, ветка `feature/multiple-voices`):
> ядро + все 10 категорий (со всеми под-случаями). Содержимое категорий и правила выбора —
> `docs/voices/voices.md` (единственный источник правды по репликам/триггерам/кулдаунам).

## Реализовано

- **Ядро**: `dmBotVoice.GetCooldown(cat)`, per-category `m_LastSpokeAt`, `Say(cat, from, to)` /
  `SayNoCooldown(cat, from, to)` (+ `TryAmbientChat` для фонового трёпа). `SayCategory`/`ResetCooldown` удалены.
- **Триггеры (все 10 категорий)**:
  - HEARD_SHOT — `dmHearing.OnNoise` (SHOT/EXPLOSION, чужой).
  - GOT_SHOT — `EEHitBy` FIRE_ARM → `[2..3]`; BULLETIMPACT-шум → `[1..2]`; кровотек (фронт `IsBleeding`) → `[4..5]`.
  - COMBAT — `OnEntry` Fighting/Shooting (производные наследуют через `super.OnEntry`).
  - IDLE — `TryAmbientChat` (персистентный таймер 15м+RandInt(0,15)).
  - PATROL — `TryAmbientChat`, только муж. модель (`IsMale`).
  - ESCORT — `Follow.OnEntry`.
  - WAKE — `UpdateUnconsciousBridge` при пробуждении.
  - AIMED_AT — `dmBotIntent_EvadeAim.OnStart`.
  - GREETING — `TargetIdentified` (первое опознание игрока/бота, threat<0.5, диапазон 7/5/1).
  - PASSENGER_CRASH — override `AddHealth`: урон от ДТП по величине (<10/20/30/≥30) +
    «очнулся после ДТП» (`m_CrashKnocked` → `SayNoCooldown([3..5])`).

## Ключевые факты (ресёрч)

- Урон от ДТП к экипажу приходит через `CarScript.DamageCrew` → `AddHealth("", "Health", -hp)` /
  `AddHealth("", "Shock", -shock)` — **минуя `EEHitBy`** (поэтому детект — override `AddHealth`).
- BULLETIMPACT-шум имеет source-стрелка (генерится в `modded DayZGame.FirearmEffects`), не позиционный.

## Цель

Боты сейчас «немые»: воспроизведение (`SpeakLine` → net-var → `Play3D`), интент
`dmBotIntent_Voice` (VOICE-канал, EXCLUSIVE, personality) и `dmBotVoiceCenter` готовы,
но **ни один вызов `SayCategory` не существует** (`grep` — ноль). Задача — привязать
10 категорий реплик к событиям/состояниям и заменить единый кулдаун на покатегорийный.

## Текущее состояние

- 4 голоса × 52 реплики + `CfgSoundSets` — готово (`feature/multiple-voices`).
- `dmBotVoiceCenter.SayCategory(cat)` — один глобальный `m_LastSpeechTime` (600 с).
- `dmBotVoice` — `s_Keys` (таблица ключей), `GetCategoryName/Count`, `GetSoundSetName(voice, lineId)`.

## Дизайн

### Кулдаун — покатегорийный

- Константа `DM_VOICE_COOLDOWN_DEFAULT = 900.0` (15 мин) вместо `DM_VOICE_COOLDOWN = 600`.
- `dmBotVoice.GetCooldown(cat)` — явные кулдауны: GREETING 60с, WAKE/AIMED_AT/HEARD_SHOT 300с;
  остальные → дефолт 900с.
- `dmBotVoiceCenter` хранит `map<dmVoiceCategory, float> m_LastSpokeAt` (категория → момент
  последней реплики) вместо одного `m_LastSpeechTime`.

### Методы (категория — первый аргумент)

```c
// Случайная реплика из диапазона [indexFrom..indexTo], С кулдауном категории.
// indexTo = -1 → до конца (GetCategoryCount). Вернёт lineId или -1 (кулдаун/пусто).
int Say(dmVoiceCategory cat, int indexFrom = 1, int indexTo = -1);

// То же, но БЕЗ кулдауна (редкие события). Всё равно обновляет m_LastSpokeAt[cat].
int SayNoCooldown(dmVoiceCategory cat, int indexFrom = 1, int indexTo = -1);
```

Оба — обёртки над `SayImpl(cat, indexFrom, indexTo, respectCooldown)`:
нормализация диапазона → проверка кулдауна → рандом-индекс → `lineId = cat*100 + index`
→ `m_LastSpokeAt[cat] = now` → `dmBotIntent_Voice` → `AddPersonalityIntent`.

Категория отвечает сразу за **кулдаун** (через `GetCooldown`) и **базу lineId**
(`cat * 100 + index`). Без обратной совместимости: `SayCategory`/`ResetCooldown`
удаляются, тест `dmBotVoiceTest.c` переписывается.

## Маппинг категорий → триггеры

| Категория | Триггер | Кулдаун | Диапазон/выбор |
|---|---|---|---|
| HEARD_SHOT | `dmHearing.OnNoise` тип SHOT/EXPLOSION | 300с | `[1..5]` |
| GOT_SHOT | урон от пули → `[2..3]`; кровотек → `[4..5]` | дефолт | по триггеру |
| COMBAT | `OnEntry` Fighting/Shooting | дефолт | `[1..5]` |
| IDLE | `dmBotState_Idle`, таймер 15м + RandInt(0,15) | дефолт | `[1..4]` |
| PATROL | `dmBotState_Patrol`, таймер 15м + RandInt(0,15), муж. модель | дефолт | `[1..5]` |
| ESCORT | `dmBotState_Follow` | дефолт | `[1..3]` |
| WAKE | выход из Unconscious | 300с | `[1..5]` |
| AIMED_AT | `OnStart` у `dmBotIntent_EvadeAim` | 300с | `[1..5]` |
| GREETING | видит не-враждебного игрока (`threat < 0.5`) | 60с | диапазон по threat (voices.md) |
| PASSENGER_CRASH | урон от ДТП (не пуля/болезнь); очнулся после ДТП → **без кулдауна** | дефолт / без | по урону (voices.md) |

> IDLE/PATROL: базовый кулдаун 15 мин, а случайную «оттяжку» 0–15 мин (итог до 30 мин тишины)
> планирует САМ триггер (состояние), не механизм кулдауна.

## Атомы (порядок)

1. **Ядро**: `DM_VOICE_COOLDOWN_DEFAULT`, `GetCooldown`, per-category `m_LastSpokeAt`,
   `Say`/`SayNoCooldown`/`SayImpl`, делегаты `dmAISurvivor`, переписать тест, bump.
2. **Реактивные**: HEARD_SHOT / GOT_SHOT / COMBAT.
3. **Фоновые**: IDLE / PATROL / ESCORT.
4. **Контекстные**: WAKE / AIMED_AT / GREETING / PASSENGER_CRASH.

Каждый атом — код (`dayz-dev`) + сборка + наблюдательный тест (`dayz-tester`/человек).
