# Дизайн прицеливания (dmAiming + интент Aim)

Согласованный дизайн выноса логики прицела/стрельбы из `dmBotState_Shooting` в
отдельный класс точности (`dmAiming`) и интент (`dmBotIntent_Aim`). Состояния FSM
держим тонкими.

## Исследование: полёт пули / выстрел / урон

1. **Пул — физический снаряд** (симулирует движок, не скрипт). `Fire(mi, pos, dir, speed)`
   спавнит пулю; движок сам ведёт баллистику (гравитация/падение). Мы стреляем в явном
   направлении `Fire(mi, pos, dir, dir)` (см. `dmBot_Fire`).
2. **Попадание и damageZone решает физика**: пуля летит по траектории, бьётся о хитбокс
   персонажа → движок определяет компонент/damageZone (голова/грудь/нога) и шлёт цели
   `EEHitBy(..., component, dmgZone, ...)`. Скрипт damageZone НЕ выбирает — он задаёт
   направление; фактическое попадание/промах — из разброса точности + движения цели.
3. Значит «точка прицеливания» (голова/грудь) — это **куда целится** бот; реальный хит —
   из разброса + физики.

## Модель точности (по `eAIAimingProfile` из dayz-devaliada)

- База: `direction = vector.Direction(neck, GetAimPosition())` — идеальная траектория.
- Для зомби/животных — **100% попадание** (без разброса).
- Для игроков — `hitProbability` из факторов:
  - здоровье бота (`GetHealth01` → штраф);
  - время слежения (`GetAccuracyByTrackingTime` — накопитель);
  - оптика: нулевой пристрел `GetZeroingDistanceZoomMin/Max` (бонус при дистанции в нуле);
  - взрывной боеприпас (`ShootsExplosiveAmmo`) — бонус;
  - **угловая скорость цели** (`vCross/dist`), `strafeFactor` — штраф;
  - видимость (`eAI_GetVisibility`) — заглушка 100%;
  - дистанция (ближе → точнее; >500м → резко хуже).
- `deviationLR/UD` (в ширинах силуэта) → угловой сдвиг
  `(deviation/dist)*RAD2DEG*(1+targetSpeedMult)` → `AimDirection`.

## Компоненты

### 1. `dmAiming` (новый класс, атрибут пешки `dmAISurvivorBase.m_Aiming`)
- `SetTarget(EntityAI)` — дать цель при «собираемся прицелиться» (сброс EMA-скорости/трекинга
  при смене цели); `Enable()`/`Disable()`/`IsEnabled()` — вкл/выкл тика.
- `OnUpdate(pDt)`: тик зовётся мозгом (после тиков интентов) при `IsEnabled()`. Точка
  прицеливания → база `neck→aimPoint` → разброс → `m_AimDirection`; затем кладёт всё в пешку:
  `SetAim(m_AimDirection, targetPos, dist, m_TargetVelocity)`.
- `vector GetAimDirection()` — геттер (аналог `m_AimDirection` у Expansion).
- **Точка прицеливания**: цель **стоит** (velocity ≈ 0) **И** «реальная оптика»
  (`optic.GetZoomMax() > 0`) → **голова** (`GetBonePositionWS("Head")`); иначе/движется →
  **грудь** (`Spine3`). Источник позиции — LOS-позиция жертвы.

### 2. `dmBotIntent_Aim` (новый интент, CRITICAL + PARALLEL, атрибут — пешка)
- выбор цели (`GetHostileTarget`, как у Fighting) → `m_Aiming.SetTarget(entity)` + `Enable()`;
- `LookAtPoint` по `m_Aiming.GetAimDirection()`;
- `RaiseWeapon(true)`;
- если `IsRaised()` (все задержки прошли) **и** `m_HasLOS` → `RequestFire` вдоль `AimDirection`.

### 3. Readiness (пешка)
- `IsRaised()` = готов стрелять (стойка + тайминги прошли).
- `IsRaising()` = тайминги идут.
- Задержки: подъём 0.5с + «смотреть в прицел» 0.3с + «найти цель в оптике» 0.5с (только
  оптика). HIP — только подъём.

### 4. Режим прицела (мозг/Shooting)
- `dist > 100м` → ADS (с задержками).
- `dist <= 100м` → HIP в течение `DM_AIM_HIP_GRACE = 10с` → ADS **без** таймингов
  (винтовка уже поднята, повторные задержки пропускаем).
- Точность: HIP → грудь/ниже; ADS+оптика+стоит → голова.

### 5. `dmBotState_Shooting` — тонкий координатор
- резолв цели → создать `dmBotIntent_Aim`; управлять перезарядкой/EXIT. Логика aim/fire
  уходит в интент.

## TODO / заглушки

- **Отдача на выстрел** (recoil) — TODO.
- `eAI_GetVisibility` — заглушка 100% (при LOS).
- `GetAccuracyByTrackingTime` — свой накопитель времени слежения.
- «Реальная оптика» = `optic.GetZoomMax() > 0`.

## Порядок реализации
1. `dmAiming` (класс точности + точка прицеливания).
2. Readiness (`IsRaised`/`IsRaising`) + режим HIP/ADS.
3. `dmBotIntent_Aim` + рефактор `dmBotState_Shooting`.
