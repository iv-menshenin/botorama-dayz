# Система боя (огнестрел)

Архитектура и ключевые решения стрельбы бота. Исследование API — `docs/research/combat.md`,
дизайн механизма прицела (`dmAiming`) — `docs/plans/aiming-design.md`.

## Слои (строгое разделение)

- **Логика** — `dmAISurvivorBase` (пешка): все параметры выстрела (точка / направление /
  скорость / отдача) и выбор режима огня. Методы переиспользуемы, с явными параметрами.
- **Низкий уровень** — `modded class Weapon_Base` (`reg/4_World/`): только нативный
  `Fire(mi, pos, dir, speed)` и `SetCurrentMode`. Никакой игровой логики.
- **Хук** — `modded class WeaponFire*` (`reg/4_World/`): детект ИИ-стрелка → `dmBot_Fire`
  + `ApplyRecoil`; ванильный muzzle-fire (`TryFireWeapon`) не вызывается.

Вся логика в пешке — будущие модификаторы (скиллы / баффы / состояние) вешаются в одном
месте, а не растаскиваются по слоям.

## Направление пули

- **`dmAiming` — единый механизм прицела** (как `dmLoot` для лута). Интент `Aim` (или тест
  `trajectory`) даёт цель (`SetTarget(EntityAI)`) и включает тик (`Enable`); мозг тикает
  `dmAiming.OnUpdate(pDt)` сразу после тиков интентов. `OnUpdate` считает точку прицела
  (кость), направление с ЛИЧНЫМ разбросом (`m_AimDirection`), дистанцию (дуло → точка) и
  псевдосреднюю скорость цели (EMA) — и кладёт всё в пешку через `SetAim(direction, targetPos,
  distance, targetVelocity)`.
- Пешка хранит world-направление **напрямую** (`m_AimWorldDirection`, сетится там же, где
  берётся `m_AimDirection`), а не реконструирует из bodyYaw + относительного угла (корпус
  успевает довернуться между прицелом и выстрелом).
- `ComputeShot(...)` → точка вылета (neck) + направление (aim + drop-компенсация +
  оружейный dispersion) + скорость.
- `dmBot_Fire` → `ComputeShot` + нативный `Fire(mi, pos, dir, velocity)`. Величину скорости
  движок берёт из `CfgAmmo initSpeed`; в `speed` передаётся только направление.

## Разброс — двухуровневый

1. **Личный** (`dmAiming`) — качество стрелка (бота).
2. **Оружейный** (`dispersion` из конфига режима) — качество оружия, добавляется **поверх**
   в `ComputeShot` (`ApplyWeaponDispersion`). Нативный `Fire` оружейный разброс не добавляет.

## Режим огня

- Режимы читаются из конфига оружия (`modes[]` + burst/dispersion/reloadTime) и кэшируются
  (`dmWeaponFireInfo` / `dmFireMode`).
- **Выбор зависит от дистанции** до цели: `GetPreferredFireModeByDistance(distance,
  availableModes)` возвращает предпочтительный режим.
- **Применяется в `SetFireMode`** (→ `SetCurrentMode`), то есть ставится **на оружие**, а не
  закрепляется за ботом. При выстреле используется режим, выставленный на оружии
  (`GetCurrentMode` / `GetCurrentModeBurstSize`), без обращения к пешке.
- Установка **проактивна**: в Idle — при входе; в Shooting — если бот долго не стрелял
  (цепочка `GetPreferredFireMode` → `SetFireMode`).
- Серия (Burst/Auto) — интент `Aim` очередит выстрелы (интервал `reloadTime`); Double —
  оба ствола за один вызов (`WeaponFireMultiMuzzle`).

## Отдача

- Сила считается в пешке (`ComputeRecoilModifier`), применяется к `dmAiming`
  (`ApplyRecoil` → `AddRecoil` + визуальный кик). Переиспользуемо — вызывается из любого места.

## Падение пули

- `CompensateBulletDrop`: дистанция из `m_AimDistance` (выставленной `dmAiming` через `SetAim`,
  **без рейкаста** — прицел у бота не идеален и мог бы попасть в дерево/землю за целью) →
  время полёта (из `initSpeed`) → drop → доворот направления вверх.
- Для одиночных выстрелов перед доворотом `RecordShot` сохраняет состояние выстрела
  (origin/dir/distance/targetPos/targetVelocity) для самообучающегося фидбека промаха
  (`dmBallisticsBridge.OnImpact`).

## Перезарядка

- `ReloadWeaponAI`: unjam → eject → chamber-load (break-action) → attach/swap магазина.
- Автоперезарядка при пустом стволе: `SelectFirearmForRange` зовёт `ReloadWeaponAI`, если
  заряженного огнестрела нет.

## Cooldown повторного входа (FSM)

- `dmBotState.m_CooldownGameTime` + `CanEnter()`: состояние, из которого бот вышел по
  «не могу стрелять» (нет ствола / нет патронов и перезарядиться нечем), ставит cooldown;
  FSM не пускает обратно, пока cooldown не истёк. Убирает осцилляцию `Shooting ↔ Idle`.
