# EnfusionScript — язык и движок (готчи синтаксиса)

Чисто **языковые/движковые** грабли Enfusion, выясненные при разработке `botorama`.
Всё, что касается механики DayZ-ботов (спавн, синхронизация, движение, навигация,
инвентарь, системы тела) — в скилле `dayz-ai-bot` (`.opencode/skills/dayz-ai-bot/`),
а research-заметки по API — в `docs/research/`.

Референсы кода: ванильные скрипты (`DayZ Projects/scripts`), Expansion
(`DayZ-Expansion-Scripts`).

## Общий принцип

**Не использовать синтаксического сахара, которого нет в референсах.** Enfusion
лишь похож на C-подобные языки; он поддерживает очень немного. Сомневаешься —
ищи аналогичный код в ванильных/Expansion скриптах и делай так же.

## Что НЕ поддерживается (запрещено)

1. **Тернарный оператор** `cond ? a : b` — не поддерживается.
   ```c
   // НЕЛЬЗЯ
   string s = from ? from.GetName() : "null";
   // НАДО
   string s = "null";
   if (from)
       s = from.GetName();
   ```

2. **Перенос строки в незавершённом statement** — цепочка вызовов/выражение должна
   быть на одной строке:
   ```c
   // НЕЛЬЗЯ
   idle.AddTransition(surrender, 0.2)
       .Require(dmBotConditions.LowHealth().And(dmBotConditions.NoAmmo()));
   // НАДО — в одну строку
   idle.AddTransition(surrender, 0.2).Require(dmBotConditions.LowHealth().And(dmBotConditions.NoAmmo()));
   ```

3. **Блочная область видимости / повторное объявление имени** — областей видимости
   НЕТ: любая переменная function-scoped. Внутри одной функции нельзя объявить имя
   дважды, даже в разных блоках/циклах (касается и индекса цикла, и класс-типированных):
   ```c
   // НЕЛЬЗЯ — два объявления i / повторное `intent`
   for (int i = 0; i < a.Count(); i++) { ... }
   for (int i = 0; i < b.Count(); i++) { ... }
   // НАДО — одно объявление в начале функции, в блоках только присваивание
   int i;
   for (i = 0; i < a.Count(); i++) { ... }
   for (i = 0; i < b.Count(); i++) { ... }
   ```
   Единичное объявление внутри одного цикла/блока допустимо (в коде оно одно).

## Типы и ссылки (`ref`)

- **Managed-классы** (наследуют `Managed`, напр. `EntityAI` → `PlayerBase`) — ссылки
  без `ref` (`private PlayerBase m_Pawn;`): они уже ссылочные.
- **Не-Managed-классы** (просто `class`, напр. `dmAISurvivor`, `dmBotFSM`) — ссылки
  через `ref`, особенно в полях и контейнерах (иначе поле не удерживает объект):
  ```c
  static ref array<ref dmAISurvivor> s_All = new array<ref dmAISurvivor>;
  private ref dmBotFSM m_FSM;
  private ref array<ref dmBotState> m_States = new array<ref dmBotState>();
  ```
- `ref`-циклы (A→B и B→A) потенциально текут; на прототипе допустимо, при удалении
  чистить явно.
- Примитивы (`int`, `float`, `bool`, `string`, `vector`) — value-типы, без `ref`.
- **`ref` ЗАПРЕЩЁН на параметрах методов** — параметр объявляется без `ref`
  (`void RollWeighted(array<ref dmBotTransition> eligible, ...)`), иначе
  `FIX-ME: Method argument can't be strong reference`. `ref` — только для ПОЛЕЙ и
  ЛОКАЛЬНЫХ переменных не-Managed-классов. Исключение — `out`/`inout` (другой механизм).
- Нативные классы: «движок владеет → без `ref` (напр. `AIWorld`), `new`-ишь сам →
  с `ref` (напр. `PGFilter`)» — конкретика в скилле `dayz-ai-bot` (Pathfinding).

## Классы, конструкторы, наследование

- Класс без своего конструктора наследует конструктор базового.
- **Безопасный паттерн**: параметризованного конструктора-наследования избегаем —
  базовый конструктор без параметров, а ссылки проставляет владелец сеттерами
  (`SetFSM`/`SetName`), затем `AddState(new dmBotState_Idle(), "Idle")`.
- `static const int/float/bool` — константы компиляции, доступны как `Class.NAME`
  и без квалификатора в наследниках.
- Каст: `Base.Cast(instance)` → `Base` или `null` (`PlayerBase pawn = PlayerBase.Cast(entity);`).

## Генерики

- `class X<Class T>` — ограничений `<Class T : Base>` НЕТ; `T` ведёт себя как `Class`
  (object) — вызывать произвольные методы `T` нельзя.
- Вызвать метод на `T` — каст к базовому классу:
  ```c
  dmJsonConfigBase base = dmJsonConfigBase.Cast(config); // config : out T
  if (base) base.FixVersion();
  ```
- Native-API принимают `T` как объект (`JsonFileLoader<T>.LoadFile(...)`,
  `JsonSerializer.ReadFromString(...)`).

## Препроцессор

- `#define` в одном файле НЕ виден в другом (per-file). Дефайны мода задаются
  глобально в `config.cpp` → `CfgMods.defines[]` (см. скилл).
- Отсечение логов — `#ifdef` **на месте вызова**, а не внутри функции: Enfusion не
  оптимизирует пустой вызов, а дорогая часть — конкатенация строк в аргументах:
  ```c
  #ifdef DM_BOT_DEBUG
  dmBotLog.Debug("...");
  #endif
  ```
- **Ограничение на число конкатенаций в одном выражении**: длинная цепочка `a + b + c + ...`
  (в типичном логе > ~8-10 слагаемых) даёт ошибку компиляции
  `Formula too complex — слишком много конкатенаций`. Разбивай лог на несколько
  отдельных вызовов `dmBotLog.Debug(...)` по 3-6 слагаемых:
  ```c
  dmBotLog.Debug("[FSM] MoveTo: subGoal=" + subGoal + " pos=" + pos + " dist=" + dist);
  dmBotLog.Debug("[FSM] MoveTo: moveAngle=" + moveAngle + " speed=" + speed + " deadline=" + deadline);
  ```

## JSON (конфиги)

- `JsonFileLoader<T>.LoadFile(path, out data, out error)` / `SaveFile(...)`;
  внутри — `JsonSerializer.ReadFromString`.
- Каталог `*.json`: `FindFile(path + "/*.json", fileName, fileAttr, FindFileFlags.DIRECTORIES)`
  → `FindNextFile` → `CloseFindFile`. `FindFileHandle` — `typedef int[]`.
- Поля конфиг-структур сериализуются по точному имени, без префикса `m_` (напр.
  `int Version` ↔ ключ `"Version"`).
- **`JsonSerializer` НЕ применяет инициализаторы полей при десериализации**:
  `float Chance = 1.0;` при отсутствии ключа `"Chance"` читается как `0.0`. Либо
  всегда пиши значение явно, либо нормализуй после `Load()`. Для массивов-по-умолчанию
  — `autoptr array<ref T> X;` (автоинициализация пустым массивом, в отличие от `ref`,
  который останется `null`).

## Полезные примитивы (язык)

- `Math.RandomFloat01()` — [0, 1). Взвешенный выбор: `r = RandomFloat01() * Σweight`,
  затем по накопленной сумме.
- `string.Split(delim, out array<string>)`, `LastIndexOf`, `Substring`, `Trim`.
- **`string.ToLower()`/`ToUpper()` — `proto int ToLower()`, мутируют строку на месте
  и возвращают `int` (не строку!)**. Нельзя в выражении/сравнении —
  `str.ToLower() == "x"` → `Incompatible parameter`. НАДО как отдельный statement:
  ```c
  slotName.ToLower();
  if (slotName == "hands") { ... }
  ```
- `foreach (T x : array)` работает; для `array<ref T>` надёжнее `for (i = 0; i < arr.Count(); i++)`
  с `int i;` один раз в функции.
- `Class.Cast` / `Class.CastTo(out, instance)` — штатные касты.
- `enum Name { A, B, C }` — int-перечисления (`FileAttr`, `FindFileFlags`, ...).
- Параметры по умолчанию поддерживаются (`void F(int x = 0)`).
- **Векторная арифметика — без inline-вызовов методов.** Цепочка вызовов в одном
  векторном выражении компилируется/вычисляется неверно: `a.GetPosition() - b.GetPosition()`
  даёт неверный результат (наблюдали: `(target.GetPosition() - bot.GetPosition()).Length()`
  → `0` при дистанции ~15 м, тогда как сохранение позиций в локалы и `vector.Distance` —
  верно). Правило: сначала сохранить результат вызова в локальную переменную, затем
  арифметика; для дистанции надёжнее `vector.Distance(a, b)` (ванильный идиом).
- **Присваивание элементу вектора** `v[i] = ...` — в правой части НЕ должно быть
  вектора (особенно того же `v` или вызова с `v`-аргументом): `pos[1] = GroundYAt(pos)`
  даёт `Cannot convert 'float' to 'vector' for argument '0'` (компилятор подставил
  `pos[1]` вместо `pos`). RHS элемента — только скалярная переменная/константа;
  векторную логику выноси в отдельную функцию/локаль, результат — отдельный `vector`
  (`vector p = SnapToGround(pos);`).

## Проверочный список при написании кода

- Нет тернарников.
- Никаких переносов строк внутри выражения/цепочки вызовов.
- В функции каждое имя переменной объявлено ровно один раз.
- `ref` для не-Managed-ссылок (поля/локалы), но НЕ на параметрах.
- Генерик не вызывает методы `T` без каста.
- Логи — через `#ifdef` на месте вызова; `dmBotLog.Error` не гейтится.
- Лог-строка не длиннее ~6 конкатенаций (`Formula too complex`); длинные логи — несколькими `Debug`-вызовами.
- Векторная арифметика — без inline-вызовов методов (сначала локалы, потом `-`/`+`; дистанция — `vector.Distance`).
- RHS присваивания элементу вектора (`v[i] = ...`) — только скаляр (не вектор и не `F(v)`).
- Сомневаешься в синтаксисе → ищи пример в ванильных/Expansion скриптах.
