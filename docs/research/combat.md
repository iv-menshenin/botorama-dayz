# Research: бой (мили / огнестрел)

Статус: мили-заглушка (`dmBotMeleeFightLogic_LightHeavy`). Задачи T10/T11/T12 плана.
Ведёт субагент `dayz-research`.

## Цель

Реальный бой: мили против заражённых, огнестрел против игроков (прицел/стрельба/
перезарядка), состояние `Fighting`.

## Известные стартовые точки

- **Мили-заглушка** (`dmAISurvivorBase`): `m_MeleeFightLogic = new dmBotMeleeFightLogic_LightHeavy(this)`
  (ванильный `DayZPlayerMeleeFightLogic_LightHeavy.HandleFightLogic` null-дерефит `hcm =
  GetCommand_Move()`; у AI-бота `CanFight()` всегда true → VM Exception вне MOVE-команды).
  Сейчас заглушка возвращает `false` (не дерётся).
- `Weapon_Base`, `WeaponManager` (`GetWeaponManager()`), прицел/выстрел — ванильный
  `HumanInputController.OverrideAimChangeX/Y` НЕ двигает голову ИИ (клиентский путь мыши).
- `HasNoAmmo()` в мозге — заглушка `false`; нужна инспекция магазина (`Magazine.GetAmmoCount()`,
  `Weapon_Base.GetMagazine(int)`).

## Открытые вопросы (research)

1. **Мили**: как дать ИИ бить — правильный подкласс `MeleeFightLogic` (как Expansion
   `eAIMeleeFightLogic_LightHeavy`) или прямой вызов анимации/команды удара? Что
   триггерит урон (`DamageSystem`)?
2. **Огнестрел**: прицел/поворот ствола для ИИ, `OverrideAimChange*` или анимация прицела,
   выстрел (`Fire`?), перезарядка (`WeaponManager`/действие reload). Как бот выбирает
   цель и ведёт прицел.
3. Перезарядка/извлечение магазина — ванильные команды/действия, доступные без `ActionManager`.
4. Расчёт урона и хит (для самопроверяемых тестов боя).

## Источники

- `DayZ Projects/scripts/4_world/entities/manbase/playerbase.c` (weapon manager, fire)
- `DayZ Projects/scripts/4_world/entities/firearms/weapon_base.c` (SpawnAmmo, muzzle)
- `DayZ Projects/scripts/4_world/entities/itembase/magazine/magazine.c`
- Expansion: `eAIMeleeFightLogic_LightHeavy.c`, их combat-команды
