# Research: эмоции/жесты (emotes) для ИИ-бота

Источник: ванильный DayZ (DayZ-Script-Diff) + Expansion AI (`eAIBase.Expansion_PlayEmote`).

## API

- `PlayerBase.GetEmoteManager()` → `EmoteManager` (`4_world/classes/emotemanager.c`):
  - `bool CanPlayEmote(int id)` — гейт (жив, не лезет/не дерётся/не плывёт, не restrained,
    не в prone/FB-emote, не с тяжёлым предметом, stance-чек конкретной эмоции).
  - `bool PlayEmote(int id)` — стартует команду action: `DetermineEmoteData` →
    `CreateEmoteCallback` → full-body `StartCommand_Action(callbackID, EmoteCB, mask)`
    или additive `AddCommandModifier_Action(callbackID, EmoteCB)`.
  - `bool IsEmotePlaying()` = `m_bEmoteIsPlaying || m_IsSurrendered || m_bEmoteIsRequestPending`.
- ID — `EmoteConstants` (`3_game/constants.c:358`): `ID_EMOTE_SALUTE=44`, `POINT=40`,
  `DANCE=12`, `THUMB=9`, `NOD=58`, `SHRUG=60`, `SURRENDER=61`, etc.
- Команда action: `Human.StartCommand_Action(int, typename, int)` → `HumanCommandActionCallback`,
  `Human.GetCommand_Action()`, `Human.AddCommandModifier_Action(int, typename)`,
  `Human.GetCommandModifier_Action()` (нативы `3_game/human.c:1527/1530/1557/1563`, публичные).

## Готча (обожглись): IsEmotePlaying() залипает у серверного ИИ

`PlayerBase` вызывает `m_EmoteManager.Update(pDt)` только при `IsPlayerSelected()`
(`4_world/entities/manbase/playerbase.c:2930`), а у серверного ИИ `m_PlayerSelected == false`.
А сброс флага `m_bEmoteIsPlaying` происходит только в `EmoteManager.OnEmoteEnd()`
(вызывается из `Update`). Итог: у ИИ `m_bEmoteIsPlaying` ставится в `true` и никогда
не сбрасывается → `IsEmotePlaying()` «залипает».

**Поэтому конец эмоции детектим по состоянию команды action:**
`GetCommand_Action() == null && GetCommandModifier_Action() == null` (full-body и additive
соответственно). Это рабочий сигнал — нативы публичны, доступны на пешке.

## Маппинг на botorama

- Примитив пешки `dmAISurvivorBase.PlayEmote(int id)`: `GetEmoteManager()` →
  `CanPlayEmote(id)` → `PlayEmote(id)`. Возвращает false, если эмоция не может стартовать.
- Интент `dmBotIntent_Emote`: PARALLEL + канал `EMOTION`. `OnStart` → `PlayEmote` (иначе `Fail`),
  `OnUpdate` → `Finish()` при `!GetCommand_Action() && !GetCommandModifier_Action()`.
  Full-body эмоции замораживают тело на уровне движка (команда action владеет корпусом),
  поэтому PARALLEL не конфликтует с MOVE/LOOK.
- Цикличные эмоции (танец `DANCE=12`) не завершаются сами — вызывающий ставит `m_Deadline`
  (иначе автодедлайн `DM_INTENT_MAX_AGE`).

## Эталон (Expansion)

`eAIBase.Expansion_SetEmote(id, autoCancel, autoCancelDelay)` + `Expansion_PlayEmote()`
(`eAIBase.c:10833/10840`): на сервере `m_EmoteManager.PlayEmote(id)` после гейта
`!IsEmotePlaying() && CanPlayEmote(id)`; для статичных/зацикленных поз — отложенный
`m_EmoteManager.ServerRequestEmoteCancel(delay)` через `CALL_CATEGORY_SYSTEM`.
