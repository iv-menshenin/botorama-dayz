# Переход «бой → лечение» (вытеснение combat-состояний)

Цель: бот, получивший ранение в бою, может **перейти в MedicalCare прямо из
Fighting/Shooting**, не дожидаясь выхода в мирное состояние. При этом (а) не
ломать текущую логику боя и лечения, (б) не прерывать атомарные действия —
мили-обмен и перевязку, (в) не бросать бой, когда это убьёт бота.

## Текущая модель вытеснения

`dmBotFSM.Update`:
- **кооперативный EXIT** — состояние вернуло `EXIT` → взвешенный переход;
- **вытеснение** — раз в `DM_FSM_PREEMPT_INTERVAL` из **только INTERRUPTIBLE**
  в PREEMPTIVE-назначения с открытым guard'ом.

Ключевое ограничение: **PREEMPTIVE-состояния (Fighting/Shooting/Follow) никем не
вытесняются** — они покидают себя только кооперативным `EXIT`. Сейчас
MedicalCare = INTERRUPTIBLE, guard `MedicalCare()` = `!IsInCombat() && ...` —
поэтому лечение из боя недостижимо.

## Зафиксированные решения

1. **A1 — обобщить вытеснение**: триггер с `GetKind() == INTERRUPTIBLE` на
   `GetKind() != NORMAL` (PREEMPTIVE вытесняют PREEMPTIVE). Изменение само по себе
   ИНЕРТНО (у Fighting/Shooting пока нет рёбер в PREEMPTIVE) — изолированный коммит.
2. **Хард-блокер — `CanExit()`** (уже есть, используется Idle/Follow). Гейтит и
   кооперативный `EXIT`, и вытеснение. Блокирует только в АКТИВНОЙ фазе при ЖИВОЙ
   угрозе; враги мертвы → сброс флагов опасности → выход мгновенный.
3. **Guard перехода — `HasImminentThreat()`** (параметр, отдельно от блокера):
   «известный» враг ближе danger-дистанции. «Известный» = видимый (`m_HasLOS`)
   **или услышанный** (недавний `m_LastContact`, позиция `m_LastPosition`).
4. **Огнестрел — НЕ блокер**, только guard. Мили (CLOSE_COMBAT) — блокер.
5. **Перевязка не прерывается**; таблетки/шина — прерываемы.

## Параметры (константы)

| Параметр | Значение | Смысл |
|---|---|---|
| `DM_MELEE_ENGAGED_WINDOW` | 2.5 с | окно «недавно били мили» |
| `DM_ZOMBIE_DANGER_DIST` | 50 м | зомби опасен в радиусе 50 м |
| пороги D | 10/50/100/350 м | danger-дистанция для игрока/стрелка |

Danger-дистанция (для НЕ-зомби):
```
D = min(GetHealth01(), InvertLerp(0.5, 1.0, GetHealth("", "Blood")))
D <= 0.2  -> 10 м
D <= 0.5  -> 50 м
D <= 0.75 -> 100 м
иначе     -> 350 м
```
`InvertLerp(0.5,1,blood) = (blood - 0.5) / 0.5` (у Blood порог смерти 0.5).

## Механизм блокировки (`CanExit`)

**`dmBotState_Fighting.CanExit()`**:
```c
if (!bot.GetHostileTarget()) { bot.ClearMeleeDanger(); return true; }
if (bot.IsMeleeEngaged()) return false;   // бьёт ИЛИ бьют (<2.5 с CLOSE_COMBAT)
return super.CanExit();
```

**`dmBotState_MedicalCare.CanExit()`**:
```c
if (m_Intent && !m_Intent.IsFinished()
    && m_Intent.m_Kind == ACTION_BANDAGE && m_Intent.m_Phase >= 1)
    return false;   // animate/wait/apply перевязки
return super.CanExit();
```
(таблетки `ACTION_PILL`, шина `ACTION_SPLINT`, укрытие-MoveTo — НЕ блокируют.)

Сигналы активной мили (мозг `dmAISurvivor`):
- `m_LastCloseCombatDamageTime` — в `EEHitBy` при `damageType == DamageType.CLOSE_COMBAT`
  (новый `RegisterCloseCombatHit()`);
- `bool IsMeleeEngaged()` = `IsMeleeStriking() || (now - m_LastCloseCombatDamageTime < DM_MELEE_ENGAGED_WINDOW)`;
- `void ClearMeleeDanger()` — сброс `m_LastCloseCombatDamageTime` (только мили-флаги,
  `IsBulletDanger` НЕ трогаем — у него своя логика таймаута/прибытия/лечения).
- `IsMeleeStriking()` — сейчас `private` в `dmAISurvivorBase` → сделать доступным.

## Параметр перехода (`HasImminentThreat`)

Итерируем ВСЕ не-friendly живые цели с недавним `m_LastContact` (не только
`GetHostileTargetEx(true)` — из-за type-aware порога ближайшая может быть
«неопасной», а дальняя «опасной»). Позиция — `m_Entity.GetPosition()` (живая),
фолбэк `m_LastPosition` (услышанная). Угроза, если `dist < dangerDist(type)`:

```
dangerDist(target) = ZombieBase ? DM_ZOMBIE_DANGER_DIST : D(здоровье)
```

## Условие и рёбра

- `dmBotCondition_MedicalCare` = `AreNecessaryMedicationsAvailable() && !HasImminentThreat()`
  (убрать `!IsInCombat()`).
- Новый `dmBotCondition_ImminentThreat` + фабрика `dmBotConditions.HasImminentThreat()`.
- `MedicalCare.GetKind() = PREEMPTIVE`.
- Новые рёбра `fight → medical`, `shoot → medical` (`.Require(MedicalCare())`, вес 2.0)
  во всех пресетах.
- Сузить `medical → fight/shoot` с `HasHostile()` на `HasImminentThreat()`
  (анти-осцилляция: guard'ы взаимно-исключающие).
- Добить недостающие мирные рёбра `→ medical` (`travel → medical` в Nomad и т.п.).

## Атомарные шаги

1. **FSM** — `GetKind() != NORMAL` (инертно, изолированный коммит).
2. **Мозг** — механика мили-флагов + экспоуз `IsMeleeStriking`.
3. **`CanExit()`** — Fighting + MedicalCare (блокеры).
4. **`HasImminentThreat()` + `GetDangerDistance()`**.
5. **Условие** — `ImminentThreat` + переписать `MedicalCare`.
6. **MedicalCare → PREEMPTIVE + рёбра**.
7. **Константы**.
8. **Тесты** (E2E, отдельно тестировщик).

## Тест-план (E2E)

- (а) бой с далёким врагом + ранение → бот переходит в MedicalCare и лечится;
- (б) ближний мили-бой (бьёт/бьют) → НЕ переходит (остаётся в Fighting);
- (в) перевязка не прерывается при появлении врага mid-бинт;
- (г) враги мертвы → флаги сброшены, выход мгновенный.

Ру́чки моста уже есть: `spawn Preset:"medical"`/`combat`, `bullet`, `botdump`
(`bulletDanger`/`brownZones`/`bleeding`/`fsm`). При необходимости — достроить
(напр. `wait Cond` на `fsm=MedicalCare`, `hostile` для живой угрозы).

## Риски и митигации

- **Осцилляция fight↔medical** — взаимно-исключающие guard'ы
  (`MedicalCare() = !HasImminentThreat` vs `HasImminentThreat`).
- **Осцилляция на границе D** — движение монотонно (heal→away, fight→toward);
  страховка `m_CooldownGameTime`.
- **Follow вытесняется** — враждебный путь уже кооперативный (срабатывает раньше);
  `follow → medical` превентивно — желаемое поведение.
- **Прерывание mid-перевязки** — `CanExit()`-блокер; `OnExit` возвращает предмет.
