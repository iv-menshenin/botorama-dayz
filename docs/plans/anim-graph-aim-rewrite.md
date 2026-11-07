# Рерайт анимационного графа: синхронизация ствола с AIM (InventoryIK)

> **СТАТУС: отложено.** Временная мера — параллакс-поправка в `ApplyWeaponAim` (см. ниже).
> Вернуться, когда/если замкнутый контур (путь B) упрётся в стену, либо когда понадобится
> чинить вертикаль (питч-дрейф) по корню.

## Проблема

Визуальный ствол бота не следует за скриптовым AIM:

- **горизонталь** — сдвиг вбок (параллакс), зависит от дистанции;
- **вертикаль (питч)** — дрейф вверх, накапливается после ухода цели («дуло остаётся
  задранным вверх» после того, как цель пробежала мимо).

При этом пуля летит по скриптовому AIM (`m_AimWorldDirection`) и попадает — рассинхрон
**чисто визуальный**, на баллистику не влияет.

## Корень

Наш `player_main.agr:1630` берёт ванильный `Master.agf` (InventoryIK) по GUID:

```
"{3F5D37C345D55BBA}DZ/anims/workspaces/player/player_main/Master.agf"
```

`Master.agf` (InventoryIK — поза оружия/рук) читает **движковые** `AimX`/`AimIKX`/`AimY`,
которые у серверного AI равны нулю (нет входного контроллера, `m_fCurrentAimX/Y` не тикает).
Наши `dmAI_AimX/Y` толкаются в граф (`ApplyWeaponAim`), но ванильный IK читает не их, а
движковый aim → «частичное следование» и дрейф.

Подтверждение API: `HumanCommandWeapons.GetBaseAimingAngleUD()/LR()`
(ваниль `3_game/human.c:1095/1098`) читают именно движковый aim — у серверного AI это
0/stale. Нативного сеттера «заставить ствол следовать за скриптом» нет.
Подробности — `docs/research/aim-sync.md`.

## Временная мера (текущая)

`ApplyWeaponAim` (`dmAISurvivorBase.c` ~1087): параллакс-поправка только по горизонтали:

```c
float corrDeg = K / m_AimDistance;            // K — калибруется (град·м)
AnimSetFloat(m_VarAimX,    m_AimSmoothedLR - corrDeg);
AnimSetFloat(m_VarAimIKX,  m_AimSmoothedLR - corrDeg);
```

- `K` подбирается вручную (на близкой дистанции ~6°), значение в коде **не фиксировано** —
  заказчик калибрует. `m_AimDistance` — дистанция «дуло → точка прицела» (ставится в `SetAim`).
- Чисто визуально: пишется только в `AnimSetFloat` графа, `m_AimRelAngleLR`/пулю не трогает.
- **Не решает вертикаль** (питч-дрейф) — она остаётся на совести движка.

## План рерайта

1. Взять ванильный `Master.agf` из
   `DayZ Projects/DZ/anims/workspaces/player/player_main/Master.agf`.
2. Найти узлы IK, читающие движковый `AimX`/`AimIKX`/`AimY` (+ возможно `LookX/Y`).
3. Пере-привязать на наши переменные: `AimX → dmAI_AimX`, `AimIKX → dmAI_AimIKX`,
   `AimY → dmAI_AimY`.
4. Скопировать изменённый файл в `dm_core/Animations/Master.agf`, в `player_main.agr:1630`
   заменить GUID-ссылку на `"dm_core/Animations/Master.agf"`.
5. Пересобрать → редеплой → визуально проверить (ствол идёт за AIM по обеим осям,
   питч-дрейф уходит).

## Риск / трудозатраты

- Низкоуровневая правка критичной IK-цепи; формат `.agf` — сериализованный `AnimSrcNode*`,
  правится текстом, но структура хрупкая.
- Высокий риск сломать позу оружия/рук (ствол/руки отвалятся или задёргаются). Нужна
  итеративная проверка. День-два минимум, больше — если `Master.agf` тянет подграфы
  (их тоже копировать).

## Альтернатива (путь B — замкнутый контур)

Скриптовый, в духе «человеческой модели» из `AGENTS.md`:

```c
Δ = GetBarrelDirection() − GetWeaponAimDirection();   // мерим расхождение
m_AimSmoothedLR += gain·Δ_yaw;    m_AimSmoothedUD += gain·Δ_pitch;   // гасим
```

Дешевле и безопаснее; автоматически лечит обе оси (и горизонталь, и питч-дрейф).
**Сначала стоит проверить именно его** — если ванильный IK не перебивает `dmAI_AimY`,
рерайт `Master.agf` вообще не нужен. Проверяется одним прогоном (лог `DM_BOT_DEBUG_PAWN`
плюс сравнение `GetBarrelDirection()` vs `GetWeaponAimDirection()`).

## Ссылки

- `docs/research/aim-sync.md` — нативный aim-API; вывод «у серверного AI обратная задача
  против клиентского игрока».
- `docs/research/animation.md` — структура графа (`.agr/.agf`), `Master.agf` = InventoryIK,
  GUID-резолв подграфов.
- `dmAISurvivorBase.c`: `BindLookVars` (~468), `SetAim` (~804), `GetMuzzlePosition` (~861),
  `GetBarrelDirection` (~880), `ApplyWeaponAim` (~1076).
- `src/core/Animations/player_main.agr:1630` — список подграфов (ванильные `Combat/Weapons/
  Gestures/Vehicles/Master.agf` по GUID + свои `Locomotion/Actions/Tests.agf`).
