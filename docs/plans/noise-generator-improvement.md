# Эпик: усовершенствование генератора шумов (noise generator)

Статус: **планирование** (спека согласована, research готов — `docs/research/noise-generator-sources.md`). Продолжает эпик
`target-refactor.md` (noise-attention) и раздел «Будущая работа (слух/звук)» из
`ai-development-plan.md`. Ветка: `feature/noise-attention`.

Базовый конвейер уже работает: `dmNoiseSystem.AddNoise(source,pos,strength,type)` →
`SI_OnNoiseAdded` → `dmHearing.OnNoise` (фильтр дистанции → резолв корня → `HearNoise`) →
`dmTargetSound`-иерархия. Этот эпик **добавляет источники шума и погодный фактор**, не меняя
скелет конвейера.

## Цели (согласовано с заказчиком)

1. Шум падения пули → **ссылка на стрелка** (сейчас `source=null`).
2. Шум **падения предмета** и **взрыва гранаты**: при броске привязываем кидателя к предмету;
   при приземлении/взрыве шум несёт инфу о кидателе.
3. Шум **экшэнов игрока** (двери, перезарядка, бинтование + выбранный заказчиком набор).
4. **Погодный фактор** на дистанцию передачи: rain/snow/fog/wind/sandstorm (скалярно).
   «Температура» из исходной постановки = туман (`GetFog`); ветер — скаляр; буря — read-only
   `GetSandstorm().GetIntensity()` (решения зафиксированы в `decisions.md`).

## Открытые вопросы (research, до кода)

- **R1** Механика броска: как ваниль бросает предмет/гранату (`HumanCommandThrow`? `ThrowItem`?
  action?), как поймать момент броска и привязать кидателя к брошенной сущности (поле? карта
  `EntityAI→EntityAI`?). Точки взрыва — `grenade_base.c InitiateExplosion` / `OnExplosionEffects`.
- **R2** Хуки экшэнов игрока: `actionopendoors/closedoors` (`OnEndServer`), reload (FSM-состояния
  `WeaponChambering`), bandage (`ActionBandage`/мед-конвейер). Полный список-кандидат — см. ниже.
- **R3** Подтвердить, что `source` в `DayZGame.FirearmEffects` резолвится в стрелка через
  `GetHierarchyRootPlayer()` (а не null/пулю-как-не-игрок).

## Декомпозиция (атомы)

- **А0 (фундамент)** — расширить `dmNoiseType`: `EXPLOSION`, `DROP`, `ACTION` (имена согласовать);
  создать `dmTargetExplosion`/`dmTargetDrop`; `CreateSound`-маппинг новых типов; `NoiseAttraction`
  для новых типов. Файлы: `3_Game/Perception/dmNoiseSystem.c`, `Target/*.c`, `dmAISurvivor.c`.
- **А1 (цель 1)** — падение пули → стрелок: `FirearmEffects` резолвит shooter и шлёт его как
  `source`; `dmTargetDroppedBullet.m_Entity = стрелок`. Позиция обновляется штатным потоком
  `HearNoise` (шум → позиция шума). §7 target-refactor «середина/не перетирать» — **отменена**
  (см. decisions.md `noise: lastKnownPosition`).
- **А2 (цель 2)** — падение предмета + взрыв: хук броска (bind thrower) + `ItemBase.EOnContact`
  (шум приземления с кидателем) + взрыв (`EXPLOSION` с кидателем). Deps: R1, А0.
- **А3 (цель 3)** — шум экшэнов: двери open/close + reload + bandage + выбранный заказчиком набор.
  Deps: R2, А0.
- **А4 (цель 4)** — погодный множитель дистанции в `dmHearing.OnNoise`:
  `strength *= weatherFactor(rain, fog, wind, sandstorm)`. Deps: R4 (fog API), решения выше.
- **А5 (E2E)** — финальный регресс + харнесс слуха (бот-«слушатель» без движения/поворота +
  источник шума), из `ai-development-plan.md` «Будущая работа».

Порядок: R1–R3 → А0 → А1 → А2 → А3 → А4 → А5. А1/А4 независимы от А2/А3, могут идти
параллельно после А0.

## Список-кандидат экшэнов (цель 3) — заказчик выберет звук и дистанцию

| Действие | Ванильный AI-шум | Точка нашего хука |
|---|---|---|
| Дверь открыть/закрыть | ✅ `actionopendoors/closedoors.c` | `OnEndServer` |
| Калитка (fence) | ✅ `actionopenfence/closefence.c` | `OnEndServer` |
| Перезарядка оружия | ❌ | `WeaponChambering`-состояния FSM |
| Бинтование/лечение | ❌ | `ActionBandage`/`Medical` |
| Падение предмета | ❌ | `ItemBase.EOnContact` |
| Взрыв | ✅ `ExplosionEffectsEx` | цель 2 |
| Двигатель машины | ✅ `carscript.c` | `modded_CarScript` |
| Костёр/roadflare/дым-граната | ✅ | соответствующие ent |
| Будильник/таймер | ✅ | их ent |
| Ловушка (trap spawn) | ✅ `trapspawnbase.c` | |
| Голос/VON | ✅ | `OnSoundEvent` |
| Строительство/крафт | ❌ | `ConstructionAction` |
| Рубка дерева/копание | ❌ | `ActionDigWorms`/`ActionChop` |

## Рабочие правила (наследуем из target-refactor §10)

- Перед каждым атомом — обсуждение деталей с заказчиком через оркестратора.
- Ключевые решения — только с согласия заказчика.
- На ключевых точках — показ «Придире» (`dayz-reviewer`).
- Коммит — только после E2E (`PASS`); не-функциональные изменения — дымовой тест.
- Каждый атом = один коммит, bump `DM_BOTORAMA_VERSION`.
