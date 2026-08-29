# Loadout-ы ботов (botorama)

Loadout — это JSON-файл, который описывает, что бот носит и носит с собой.
Это **свой компактный формат** (не громоздкий Expansion-формат). Файлы лежат в

```
$profile:dmBotorama/loadouts/<имя>.json
```

Имя loadout = имя файла без `.json`. Применить к уже заспавненному
(привязанному) боту — командой в чате:

```
/bot loadout hunter      — применить loadout "hunter" к своему боту
/bot loadout             — список доступных loadout-ов
```

Команда только **добавляет** предметы: то, что на боте уже было, остаётся.
Автоматически при спавне loadout не применяется — только по команде.

## Формат

```json
{
  "Version": 1,
  "Presets": [ { "Name": "ranged", "Chance": 1.0 } ],
  "Slots": [
    { "SlotName": "Hands", "Items": [ { "ClassName": ["M14"] } ] }
  ],
  "Cargo": [
    { "ClassName": ["Apple"], "Chance": 0.5 }
  ]
}
```

### Корень

| Поле       | Тип        | Назначение |
|------------|------------|------------|
| `Version`  | int        | Версия схемы. Сейчас `1`. |
| `Presets`  | массив     | Пул пресетов (см. ниже). Можно опустить. |
| `Slots`    | массив     | Слоты экипировки бота (одежда, оружие). |
| `Cargo`    | массив     | Предметы в общий карго бота. |

### `Presets`

Пресет — это «вариант комплекта». Один пресет выбирается случайно при каждом
применении (вес — поле `Chance`). Предмет может через `Conform` указать, в какие
пресеты он входит; в выборе участвуют только подходящие предметы.

```json
"Presets": [
  { "Name": "sniper",  "Chance": 1.0 },
  { "Name": "assaulter", "Chance": 1.0 }
]
```

- `Name` — имя пресета (сравнение регистрозависимое).
- `Chance` — вес при выборе пресета (по умолчанию `1.0`).
- Если `Presets` пуст/отсутствует — пресет не выбирается, поле `Conform`
  игнорируется, все предметы участвуют.

### `Slots` (слоты экипировки)

Каждый слот — целевой слот + список кандидатов, из которых **выбирается один**
(взвешенно по `Chance`):

```json
{ "SlotName": "Legs", "Items": [ { "ClassName": ["CargoPants_Beige"] } ] }
```

- `SlotName` — имя слота. Особые случаи: `Hands` (оружие в руки); остальные —
  слоты экипировки (`Body`, `Legs`, `Vest`, `Back`, `Headgear`, `Mask`,
  `Eyewear`, `Gloves`, `Feet`, `Hips`, `Shoulder`, `Melee`, …).
- `Items` — кандидаты. Один победитель (или «пусто», см. ниже).

### `Cargo` (общий карго)

В отличие от слотов, каждый элемент карго кидает **независимый** шанс
`Chance` (каждый предмет появляется или нет сам по себе):

```json
"Cargo": [
  { "ClassName": ["AmmoBox_308Win_20Rnd"], "Chance": 0.8 }
]
```

### Предмет (общий для слотов и карго)

| Поле           | Тип    | Назначение |
|----------------|--------|------------|
| `ClassName`    | массив | Варианты имени класса; выбирается один случайно. |
| `Chance`       | float  | В слоте — вес; в карго — независимый шанс. По умолчанию `1.0`. |
| `Health`       | объект | Доля здоровья `{ "Min": 0.3, "Max": 0.9 }` (0..1), только GlobalHealth. |
| `Quantity`     | объект | Доля от макс. стака `{ "Min": 0.5, "Max": 1.0 }` (0..1). |
| `Conform`      | массив | Имена пресетов, в которые входит предмет. Пусто = всегда. |
| `Attachments`  | массив | Слоты-аттачи этого предмета (оптика, магазин…). Рекурсивно. |
| `Cargo`        | массив | Предметы внутрь этого предмета (рюкзак → еда). Рекурсивно. |

Семантика:

- **Слот, взвешенный выбор с «порогом пустоты»**: `r = RandomFloat01() *
  max(сумма_весов, 1.0)`. Выбирается предмет, у которого накопленный вес
  первым превысил `r`; если `r >= сумма_весов` — слот остаётся **пустым**.
  Поэтому при сумме весов < 1.0 слот с шансом `(1 − сумма)` пустует.
- **Карго**: каждый предмет независимо выпадает, если `RandomFloat01() < Chance`.
- `Chance` по умолчанию `1.0`.
- Предмет без `Conform` участвует всегда (если выбран пресет — тоже).
- `Health`/`Quantity` — диапазоны, значение берётся случайно из `[Min, Max]`.

## Примеры

### hunter.json — охотник

```json
{
  "Version": 1,
  "Slots": [
    {
      "SlotName": "Hands",
      "Items": [
        {
          "ClassName": ["Winchester70"],
          "Attachments": [
            { "SlotName": "WeaponOptics", "Items": [ { "ClassName": ["HuntingOptic"] } ] },
            { "SlotName": "WeaponMagazine", "Items": [ { "ClassName": ["Mag_Winchester70_5Rnd"], "Quantity": { "Min": 1.0, "Max": 1.0 } } ] }
          ]
        }
      ]
    },
    { "SlotName": "Body",  "Items": [ { "ClassName": ["HikingJacket_Green", "HikingJacket_Blue"] } ] },
    { "SlotName": "Legs",  "Items": [ { "ClassName": ["CargoPants_Beige"] } ] },
    { "SlotName": "Feet",  "Items": [ { "ClassName": ["WorkingBoots_Beige"] } ] },
    {
      "SlotName": "Back",
      "Items": [
        {
          "ClassName": ["HuntingBag"],
          "Cargo": [
            { "ClassName": ["AmmoBox_308Win_20Rnd"], "Chance": 0.8 },
            { "ClassName": ["TacticalBaconCan"], "Chance": 0.6 },
            { "ClassName": ["CanOpener"], "Chance": 0.4 }
          ]
        }
      ]
    }
  ],
  "Cargo": [
    { "ClassName": ["Ammo_308Win"], "Chance": 0.7, "Quantity": { "Min": 0.3, "Max": 1.0 } }
  ]
}
```

### soldier.json — пресеты (снайпер / штурмовик)

```json
{
  "Version": 1,
  "Presets": [
    { "Name": "sniper",    "Chance": 1.0 },
    { "Name": "assaulter", "Chance": 1.0 }
  ],
  "Slots": [
    {
      "SlotName": "Hands",
      "Items": [
        {
          "ClassName": ["Winchester70"],
          "Conform": ["sniper"],
          "Attachments": [
            { "SlotName": "WeaponOptics", "Items": [ { "ClassName": ["HuntingOptic"] } ] }
          ]
        },
        {
          "ClassName": ["M14"],
          "Conform": ["assaulter"],
          "Attachments": [
            { "SlotName": "WeaponOptics",   "Items": [ { "ClassName": ["PSO1Optic"] } ] },
            { "SlotName": "WeaponMagazine", "Items": [ { "ClassName": ["Mag_M14_20Rnd"], "Quantity": { "Min": 1.0, "Max": 1.0 } } ] }
          ]
        }
      ]
    },
    { "SlotName": "Body", "Items": [ { "ClassName": ["HikingJacket_Green"] } ] },
    { "SlotName": "Legs", "Items": [ { "ClassName": ["CargoPants_Beige"] } ] }
  ],
  "Cargo": [
    { "ClassName": ["Ammo_308Win"], "Chance": 0.7, "Quantity": { "Min": 0.3, "Max": 1.0 } }
  ]
}
```

## Примечания

- Имена классов (`ClassName`, `SlotName`) должны совпадать с реальными классами
  предметов DayZ. Если класс не найден — предмет просто не заспавнится
  (не ошибка, не краш), остальное применяется.
- `Health` — нормализованный (0..1), применяется только к GlobalHealth.
- `Quantity` — доля от максимума стака. Для нестакаемых предметов игнорируется.
- Примеры выше — иллюстративные; сверяйте имена с актуальным списком предметов
  вашей сборки DayZ.
