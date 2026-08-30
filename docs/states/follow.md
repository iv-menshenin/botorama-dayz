# Состояние Follow (сопровождение)

Сопровождение цели: бот держится сбоку от цели (игрок/бот/предмет), догоняет при
отставании, защищается при атаке. Это «готовая» документация состояния — актуальна
на момент версии 2.59. Краткая справка — в скилле `dayz-ai-bot`, раздел FSM.

## Пороги дистанции

`GetThresholdDistance(target)`: `PlayerBase` → `DM_FOLLOW_THRESHOLD_PLAYER` (5 м),
прочее → `DM_FOLLOW_THRESHOLD_OTHER` (1 м).

## Модель — два интента

Состояние само не двигает бота; оно выбирает один из двух интентов каждый тик:

- **Цель видна ИЛИ близко (`dist ≤ порога`)** → `dmBotIntent_FollowTo` (непрерывный,
  держит строй).
- **Цель не видна И далеко (`dist > порога`)** → `dmBotIntent_MoveTo` к последней
  известной позиции (`m_LastPosition`, спринт через `m_ReachDeadline`) — догнать.

## Интент `dmBotIntent_FollowTo`

PARALLEL + CRITICAL (непрерываем по каналу движения, но параллелен взгляду
`LookAround` — канал взгляда FollowTo не трогает, только `SetMoveYaw` + `SetMove`).

**Якорь (точка сопровождения):**

- цель стоит (`|velocity| < 0.5`):
  - игрок/бот → плечо: `targetPos + перпендикуляр(GetDirection) × ±DM_FOLLOW_SIDE_DISTANCE`;
  - предмет → 1 м не доходя: `targetPos + normalize(botPos - targetPos) × DM_FOLLOW_SIDE_DISTANCE`.
- цель движется:
  ```
  anchor = targetPos + Velocity × DM_FOLLOW_VEL_MULTIPLIER (1.5)
                     + перпендикуляр(Velocity) × (случайный сдвиг 1.5–3 м × знак)
  ```

  Знак (±) и сдвиг (1.5–3 м) рандомизируются **один раз** на входе в состояние и не
  перебрасываются (иначе бот «плавает» вокруг цели).

**Скорость:** длина вектора до якоря (`distToAnchor`): `> DM_FOLLOW_SPRINT_GAP (7.5)` →
sprint, `> DM_FOLLOW_JOG_GAP (5)` → jog, иначе walk. Плюс правило «не отставать»: скорость
не ниже скорости цели (`max(dist-скорость, скорость цели)`).

**Pathfinding:** пересчёт маршрута не чаще 1 Гц (`DM_FOLLOW_PATH_INTERVAL`), цель маршрута —
**якорь** (точка сопровождения). Без фолбэков: нет пути → бот идёт прямо на якорь.

## «Магия» — восстановление при потере видимости

- Цель не видна ≥ `DM_FOLLOW_LOST_SIGHT_TIME` (60 с) → обновить только `m_LastPosition`
  (реальная позиция), чтобы бот догонял актуальную точку.
- Если цель забыта (нет в `m_Targets`, т.е. 5 мин без контакта) → выход.

## Вход / выход

- **`CanEnter()`** — блокировка «цель неизвестна»: цель должна быть в `m_Targets`
  (`FindTarget`) и иметь `m_LastPosition != 0`.
- **Выход**: угроза в 4 м (`GetDefendTarget()`) → в бой; цель-игрок мертва; цель забыта;
  цель стоит + бот в месте (`dist ≤ DM_FOLLOW_REACH + DM_FOLLOW_SIDE_DISTANCE`) в течение
  `DM_FOLLOW_EXIT_TIME` (3 с).

## Пресет (веса-приоритеты)

Вес `> 1.0` — приоритетный переход (детерминированный, вытесняет переходы с весом
`≤ 1.0` из розыгрыша).

```c
idle.AddTransition(follow, 0.5).Require(FollowFar()).BlockWhen(ThreatInRange());
idle.AddTransition(fight, 2.0).Require(ThreatInRange());
fight.AddTransition(idle, 1.0);
follow.AddTransition(fight, 2.0).Require(DefendInRange());
follow.AddTransition(idle, 1.0).BlockWhen(DefendInRange());
```

## Константы

`DM_FOLLOW_THRESHOLD_PLAYER/OTHER`, `DM_FOLLOW_SIDE_DISTANCE`, `DM_FOLLOW_REACH`,
`DM_FOLLOW_EXIT_DISTANCE/TIME`, `DM_FOLLOW_VEL_MULTIPLIER`,
`DM_FOLLOW_SIDE_DISTANCE_MIN/MAX`, `DM_FOLLOW_SPRINT_GAP/JOG_GAP`,
`DM_FOLLOW_PATH_INTERVAL`, `DM_FOLLOW_PATH_EXTRAPOLATE_TIME`, `DM_FOLLOW_VEL_SMOOTH`,
`DM_FOLLOW_LOST_SIGHT_TIME` — все в `cons/4_World/constants.c`.
