# Как работает loadout (dmLoadoutApplier)

Пользовательский формат и примеры — в `loadouts.md` (корень мода). Здесь —
внутренняя механика применения.

## Модуль

```
botorama/
├── loadouts.md                  # формат + примеры (для пользователя)
└── loadout/4_World/
    ├── dmLoadoutConfig.c        # структуры данных (схема JSON)
    ├── dmLoadoutApplier.c       # загрузка + применение
    └── README.md                # этот файл
```

`loadout/4_World` подключён к `worldScriptModule` в `config.cpp` (загружается
после `cons/` и `core/`, поэтому видит `dmJsonFile`/`dmJsonConfigBase` и
`dmBotLog`). Команда `/bot loadout` живёт в `test/5_Mission/dmBotCommand.c`.

## Порядок применения

1. **Пресет** — один раз случайно выбирается пресет (взвешенно по `Chance`).
2. **Слоты** — для каждого слота выбирается один предмет-победитель, спавнится
   и ставится в слот.
3. **Карго** — каждый элемент карго независимо выпадает по `Chance` и кладётся
   в общий карго бота.

Применение **аддитивное**: существующий инвентарь бота не трогается.

## Прямое создание в слоте

Предметы создаются **сразу на месте** — как ванильная выдача экипировки
(`cfgplayerspawnhandler.c`), а не «спавн на полу + перенос»:

- слот экипировки: `pawn.GetInventory().CreateAttachmentEx(cls, slotId)`;
- руки: `pawn.GetHumanInventory().CreateInHands(cls)`;
- карго / вложенный предмет: `parent.GetInventory().CreateInInventory(cls)`
  (сам находит свободный слот-аттач или карго);
- магазин на оружии: `wep.SpawnAmmo(cls)` (внутренний/внешний магазин).

`TakeEntityTo*` здесь не работает: это операции **перемещения**, а у только что
созданного предмета ещё нет валидной `InventoryLocation`, поэтому он «не
надевается» и остаётся лежать.

### Фолбэк для вложенного контейнера (ванильный баг)

Движок не может создать предмет внутри контейнера, который сам лежит в карго
другого контейнера (вложенный контейнер) — `CreateEntityInCargoEx` возвращает
`null`. Это **не основной путь**, а фолбэк (`CreateInContainerFallback`), когда
обычное создание не удалось:

1. `CreateObject(cls, "0 0 0")` на полу → `CanAddEntityToInventory` +
   `AddEntityToInventory` (добавить в контейнер).
2. Если не вышло — «танец на полу»: `TakeToDst(SERVER, parentLoc, ground)` →
   `AddEntityToInventory(item)` → `TakeToDst(SERVER, ground, parentLoc)`, с
   `RemoteObjectTreeDelete`/`RemoteObjectTreeCreate` вокруг.
3. Не вышло и так — предмет удаляется, логируется провал.

Эталон — `ExpLootSpawner.c` → `DMCreateInInventory`.

## Слоты

- `Hands` — особый случай: `pawn.GetHumanInventory().CreateInHands(cls)`.
- Остальные имена резолвятся в id через `InventorySlots.GetSlotIdFromString(name)`
  → `CreateAttachmentEx(cls, slotId)`.
- Пустое/невалидное имя → `GetSlotIdFromString` вернёт `INVALID` → авто-назначение
  через `CreateAttachment(cls)` (магазин → WeaponMagazine, оптика → WeaponOptics).
  Пустой `SlotName` во вложенных аттачах оружия обрабатывается так же.

## Случайности

- **Пресет**: `r = RandomFloat01() * сумма_весов`; пресет — первый, у кого
  накопленный вес превысил `r`.
- **Слот (взвешенный выбор с порогом пустоты)**:
  `r = RandomFloat01() * max(сумма_весов_допустимых, 1.0)`. Победитель — первый
  допустимый, у кого накопленный `Chance` превысил `r`; если `r >= сумма` —
  возвращается `null` (слот пуст). При сумме < 1.0 слот пустует с шансом
  `1 − сумма`.
- **Карго**: `RandomFloat01() < Chance` для каждого элемента независимо.
- **Класс предмета**: случайный из `ClassName`.
- **Health/Quantity**: случайное значение в `[Min, Max]` диапазона.

## Здоровье и количество

- `Health` — доля 0..1 → `item.SetHealth01("", "Health", x)` (GlobalHealth).
- `Quantity` — доля 0..1 → `item.SetQuantity(Lerp(GetQuantityMin(),
  GetQuantityMax(), x))` (только для стакаемых, `HasQuantity()`).
- Магазины (`Magazine`, не патронные пачки): `Quantity` = доля ёмкости
  (`ServerSetAmmoCount(Lerp(0, ammoMax, x))`); без `Quantity` — полный магазин.

## Ошибки

- Файл не найден / не парсится → `Load()` возвращает `null` (команда отвечает
  ошибкой, логирует через `dmBotLog.Error`).
- Невалидный класс / слот занят / нет места → `Create*` вернёт `null`, предмет
  **пропускается вместе со своими аттачами**, в лог пишется `ERROR`
  (`Loadout FAILED: <cls> -> ...`), остальные предметы применяются дальше.
- `ERROR`-логи провалов **не гейтятся** (видны всегда); `DEBUG`/`TRACE`-логи
  гейтятся `#ifdef DM_BOT_DEBUG_LOADOUT` (включён в `config.cpp`). Профиль —
  `#ifdef DM_BOT_PROFILE` (спаны `Loadout.Load` / `Loadout.Apply` /
  `Loadout.Create`).

## Замечание по `Chance`

- JSON опускает `Chance`, когда он равен `1.0`. Движковый `JsonSerializer` не
  применяет инициализаторы полей при десериализации, поэтому опущенный
  `Chance` читается как `0.0` (предмет «никогда не выбирается»). В `Load()` это
  нормализуется: `Chance <= 0.0` → `1.0` (рекурсивно по слотам/карго/пресетам).
