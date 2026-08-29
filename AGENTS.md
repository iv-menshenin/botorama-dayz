# botorama — конвенции разработки

## Документация

- `docs/codeguide.md` — синтаксис/движок Enfusion (готчи, чеклист).
- `docs/plans/ai-development-plan.md` — дорожная карта задач ИИ (порядок + прогресс).
- `docs/plans/fsm-implementation-plan.md` — план/история FSM (фазы, уроки).
- `docs/research/*.md` — research-заметки по API (ведёт субагент `dayz-research`).
- `docs/techdebt.md` — техдолг.

## Классификация знаний (куда писать ошибку/факт)

- Ошибка **синтаксиса/движка Enfusion** (тернарник, области видимости, `ref`,
  `JsonSerializer`, `ToLower`) → `docs/codeguide.md`.
- Знание **механики бота DayZ** (спавн/синк/движение/навигация/инвентарь/бой) →
  скилл `dayz-ai-bot` (`.opencode/skills/dayz-ai-bot/SKILL.md`).
- Новая **сигнатура/поведение API** → `docs/research/<домен>.md`.

## Процесс

- Одна атомарная задача = один коммит; на каждое изменение bump `DM_BOTORAMA_VERSION`
  (`cons/3_Game/constants.c`).
- Реализацию делает субагент `dayz-dev`; ревью оркестратора — по чеклисту `codeguide.md`.
- API-исследование — субагент `dayz-research` (пишет только `docs/research/`).

## Субагенты

Определения (source of truth) — `.opencode/agent/dayz-dev.md`, `.opencode/agent/dayz-research.md`
(дублируются в глобальный `~/.config/opencode/agent/` для загрузки — держать в синхроне).
Скилл `dayz-ai-bot` — `.opencode/skills/dayz-ai-bot/`.
