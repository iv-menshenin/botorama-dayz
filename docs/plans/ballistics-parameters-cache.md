# Кэш баллистических параметров оружия и пули

## Цель

Вынести чтение характеристик оружия и пули из конфигов в кэширующий слой, чтобы
баллистика (дроп, ветер) работала для **любого калибра** без повторных чтений
конфига на каждый выстрел. Это фундамент под исправление модели дропа и сноса
ветром (отдельные задачи).

## Иерархия конфигов (проверено)

- Патрон-пачка (`Ammo_762x54`) → `CfgMagazines`, поле `ammo="Bullet_762x54"`, `weight=9` (инвентарный вес пачки — НЕ вес пули).
- Пуля (`Bullet_762x54`) → `CfgAmmo`, поля `initSpeed`, `airFriction`, `weight` (кг), `caliber`.
- Оружие (`Mosin9130_Base`) → `CfgWeapons`, поля `initSpeedMultiplier` (помузловый), `ObstructionDistance`, `NoiseShoot.strength`.

Цепочка: `патрон → CfgMagazines <патрон> ammo → имя пули → CfgAmmo <пуля> атрибуты`.

## Дизайн

### 1. `dmProjectileInfo` — структура (новый класс)
```c
class dmProjectileInfo
{
	float m_InitSpeed;    // CfgAmmo <bullet> initSpeed
	float m_AirFriction;  // CfgAmmo <bullet> airFriction
	float m_Weight;       // CfgAmmo <bullet> weight (кг)
	float m_Caliber;      // CfgAmmo <bullet> caliber
}
```

### 2. `dmProjectileCache` — глобальный кэш (новый статический класс)
`static ref map<string, ref dmProjectileInfo> s_Cache` по ключу — имя патрона
(`Ammo_762x54`). Метод `static dmProjectileInfo Get(string ammoType)`:
- пусто/найдено в кэше → вернуть;
- иначе `CfgMagazines <ammoType> ammo` → имя пули, затем `CfgAmmo <пуля> {initSpeed,airFriction,weight,caliber}`, сохранить в кэш.

Паттерн — как `dmWeaponFireInfo.s_Cache`. Файл `core/4_World/Entities/Bot/Weapons/dmProjectileCache.c`.

### 3. `modded class Weapon_Base` — атрибуты оружия (per-instance)
Поля `m_dmInitSpeedMultiplier`, `m_dmObstructionDistance`, флаг `m_dmAttributesReady`.
- `dmReadWeaponAttributes()` — лениво читает `ConfigGetFloat("initSpeedMultiplier")`
  (по текущему музлу) и `ConfigGetFloat("ObstructionDistance")`, ставит флаг.
- Геттеры `dmGetInitSpeedMultiplier()` / `dmGetObstructionDistance()` — зовут
  `dmReadWeaponAttributes()` при неготовом флаге.
- Хуки `EEItemAttached` / `EEItemDetached` / `OnAttachmentRuined` — сбрасывают
  флаг (задел под отдачу от приклада/цевья; сейчас `initSpeedMultiplier`/
  `ObstructionDistance` от аттачей не зависят, но сброс — безопасная перестраховка).

### 4. Перевязка `dmAISurvivorBase`
- Новый хелпер `GetChamberedProjectileInfo(weapon, mi)` → `dmProjectileCache.Get(weapon.GetChamberedCartridgeMagazineTypeName(mi))`.
- `GetAmmoInitSpeed`/`GetAmmoAirFriction` → перевести на кэш (тонкие обёртки).
- Новый `GetAmmoWeight(weapon, mi)`.
- Новый `GetEffectiveInitSpeed(weapon, mi)` = `initSpeed × weapon.dmGetInitSpeedMultiplier()`.
- `ComputeBulletTravelTime` использует `GetEffectiveInitSpeed` (фикс игнора `initSpeedMultiplier`).
- Убрать ставший ненужным `GetChamberedBulletType`.

## Файлы

- `core/4_World/Entities/Bot/Weapons/dmProjectileCache.c` — новый (`dmProjectileInfo`, `dmProjectileCache`).
- `reg/4_World/modded_WeaponBase.c` — атрибуты + геттеры + хуки.
- `core/4_World/Entities/Bot/dmAISurvivorBase.c` — перевязка геттеров.

## Вне скоупа (следующие задачи)

- Модель дропа (пошаговая дистанционная интеграция) — использует `GetEffectiveInitSpeed` + `GetAmmoAirFriction`.
- Модель сноса ветром — использует `GetAmmoWeight` + `GetAmmoAirFriction`.
- `NoiseShoot.strength` → в TODO (звук).
