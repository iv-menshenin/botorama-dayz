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

    **Ловушка «скопированного цикла»**: два похожих цикла подряд (напр. дамп динамики и
    статики) провоцируют объявить тело цикла (`string line`, `Object obj`, …) в обоих —
    это **компиляция падает** (`Multiple declaration of variable`). Выноси объявление
    ТЕЛА цикла до циклов, в циклах только присваивание (как с индексом `i`). Живой пример
    ошибки — `RunScanBox` в `src/test/4_World/dmE2EBridge.c` (был двойной `string line`).

4. **Побитовые операторы НЕ поддерживаются** — `<<`, `>>`, `&`, `|`, `^`, `~` дают
   `Unknown operator`. Степень двойки/умножение — через цикл или `Math.Pow`:
   ```c
   // НЕЛЬЗЯ
   float div = 1.0 << (n - 1);
   // НАДО
   float div = 1.0;
   int k;
   for (k = 1; k < n; k++)
       div = div * 2.0;
   ```
   **Следствие для флагов-запросов**: `QueryFlags` и `CollisionFlags` — это
   **последовательные enum'ы, а НЕ битмаски** (`QueryFlags`: `NONE=0, STATIC=1,
   DYNAMIC=2, ORIGIN_DISTANCE=3, ONLY_ROADWAYS=4`; `CollisionFlags`: `FIRSTCONTACT=0,
   NEARESTCONTACT=1, ONLYSTATIC=2, ONLYDYNAMIC=3, ONLYWATER=4, ALLOBJECTS=5`).
   Комбинировать их через `|` нельзя (даст чужой член: `STATIC|DYNAMIC` = `1|2 = 3 =
   ORIGIN_DISTANCE`). Если нужны и статика, и динамика — **два отдельных вызова**
   (`SceneGetEntitiesInBox(..., QueryFlags.DYNAMIC)` + `...(..., QueryFlags.STATIC)`).
   Один флаг на вызов — эталон `dmLoot.c`/`dmRedZone.c` (DYNAMIC), `dmExplorer.c` (STATIC).

5. **Имя поля/переменной НЕ должно совпадать с именем встроенного ТИПА/класса** —
   движок компилирует скрипты на загрузке и падает `Variable name '<X>' already used
   as type name` → `Can't compile "World" script module!` → segfault на старте сервера.
   Пример: поле `int Surface;` конфликтует со встроенным классом `Surface`
   (`4_world/static/surface.c`). **AddonBuilder это НЕ ловит** (билд `exit=0`) — ошибка
   только при загрузке на сервере. Переименуй поле (напр. `SurfaceType`/`SurfaceName`).
   Другие рискованные имена полей: `Object`, `Game`, `Shape`, `LOD`, `Car`, `Human`.

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
- **`array`/`map`/`set` — reference-типы**: переданные параметром БЕЗ `ref` делят один
  объект с вызывающим (мутации `Set`/`Remove`/`Insert`/`Clear` видны вызывающему). Поэтому
  рекурсивный обход с «посещённым»-множеством передаёт `map<string,bool> visited` (plain),
  а НЕ `ref map<...>` — иначе `FIX-ME`. `ref` на контейнере нужен только в ПОЛЯХ/элементах
  (`ref array<ref T>`, `ref map<...> m_Cache` — владение), а не в параметрах.
- **`out`/`inout` — ТОЛЬКО в сигнатуре, НА МЕСТЕ ВЫЗОВА НЕ ПИШУТСЯ.** Объявление:
  `bool FindCarWithPlayer(out int freeSeat)` / `bool FindPath(vector from, vector to, inout array<vector> waypoints)`;
  вызов: `FindCarWithPlayer(freeSeat)` / `m_Pathfinder.FindPath(GetPosition(), sampled, path)`.
  Ошибка `FindCarWithPlayer(out freeSeat)` — не компилируется (не как C#/Pascal, где
  `out` дублируется в вызове).
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
- **`modded class X` — файл должен лежать в том же модуле, что и `X`.** `X` из
  3_Game (напр. `DayZGame`) → `core/3_Game`; `X` из 4_World (`Weapon_Base`/`ZombieBase`/
  `DayZPlayerImplement`) → `reg/4_World`/`core/4_World`. Иначе `Unknown type 'X'`
  (модуль не видит класс из соседнего модуля). Модули: `3_Game` → `4_World` → `5_Mission`.
- **Движковые классы НЕЛЬЗЯ `modded`.** В цепочке персонажа ВСЕ классы 3_Game —
  движковые: `Object → Entity → EntityAI → Man → Human → DayZPlayer` (все члены
  `proto native`, нет скриптовой реализации) → `modded class X` даёт
  `Engine class 'X' cannot be modded`. Первый скриптовый класс — `DayZPlayerImplement`
  (4_World), далее `ManBase`/`PlayerBase` (4_World). Следствие: **из 3_Game нельзя
  достучаться до пешки виртуальным хуком через предка** — подходящего скриптового
  предка в 3_Game нет. Решение — статический мост-класс в 3_Game (хранит состояние в
  `map` по ключу `EntityAI`, 4_World читает/пишет через его статики): `dmBallisticsBridge`
  (`core/3_Game/dmBallisticsBridge.c`).
- **Оверрайд требует ТОЧНОЙ сигнатуры (типы параметров входят).** Метод мода с другим
  типом параметра (напр. `DropItem(EntityAI)` vs ванильный `DropItem(ItemBase)`) НЕ
  оверрайдит ванильный — это оверлоад. При вызове с аргументом базового типа компилятор
  выберет ванильный, а твой метод молча не вызовется. Перед добавлением метода проверяй
  ванильные с тем же именем: `grep -rn 'bool DropItem\b' DayZ-Script-Diff/scripts/`.

## Конфиг (`config.cpp`): наследование классов из другого аддона

Классы `config.cpp` (`CfgVehicles` / `CfgSoundShaders` / `CfgSoundSets` / ...)
наследуются только от классов, объявленных в уже загруженных аддонах. Для
**кросс-аддонного** наследования нужна **forward-декларация** базового класса ВНУТРИ
того же блока, иначе `CfgConvert` падает на сборке PBO:

```
File: botorama\config.cpp
/CfgSoundShaders.<Класс>: Undefined base class "baseCharacter_SoundShader"
```

- `requiredAddons[]` это **НЕ решает**: он задаёт порядок загрузки в рантайме, а
  `CfgConvert` резолвит имена базовых классов при конвертации и требует forward-декларацию.
- Пишется `class <Base>;` (с точкой с запятой) перед первым наследником в том же блоке:

```cpp
class CfgSoundShaders
{
	class baseCharacter_SoundShader;
	class dmBotVoice_test_SoundShader : baseCharacter_SoundShader { ... };
};
class CfgSoundSets
{
	class baseCharacter_SoundSet;
	class dmBotVoice_test_SoundSet : baseCharacter_SoundSet { ... };
};
```

- Эталон — рабочий мод `TerjeMods/TerjeMedicine/Sounds/config.cpp`: наследует
  `baseCharacter_SoundShader`/`baseCharacter_SoundSet` с forward-декларацией и БЕЗ
  `DZ_Sounds_Effects` в `requiredAddons`.

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
- **Конкатенация должна начинаться со `string`**: выражение `int + string` не компилируется
  (`Incompatible parameter '<строка>'`). Число-первое слагаемое — ошибка:
  ```c
  // НЕЛЬЗЯ
  string s = nodeCount + " nodes / " + edgeCount + " edges";
  // НАДО — ведущий строковый литерал (или .ToString())
  string s = "" + nodeCount + " nodes / " + edgeCount + " edges";
  ```

## JSON (конфиги)

- `JsonFileLoader<T>.LoadFile(path, out data, out error)` / `SaveFile(...)`;
  внутри — `JsonSerializer.ReadFromString`.
- **Бот-обёртка `dmJsonFile<T>` (`src/cons/3_Game/Config/dmJsonFile.c`) требует, чтобы `T`
  наследовал `dmJsonConfigBase`** — внутри она делает `dmJsonConfigBase.Cast(config)`
  (версионирование). Для plain-структур БЕЗ наследования инстанцирование `dmJsonFile<T>`
  падает на компиляции: `Types 'dmJsonConfigBase' and '<T>' are not related`. Для таких
  структур используй `JsonFileLoader<T>` напрямую (эталон — `dmE2EBridge` для
  `dmE2EJob`/`dmE2EResult`). `dmJsonFile<T>` — только для версионируемых конфигов с
  полем `Version`.
- Каталог `*.json`: `FindFile(path + "/*.json", fileName, fileAttr, FindFileFlags.DIRECTORIES)`
  → `FindNextFile` → `CloseFindFile`. `FindFileHandle` — `typedef int[]`.
- Поля конфиг-структур сериализуются по точному имени, без префикса `m_` (напр.
  `int Version` ↔ ключ `"Version"`).
- **Поля JSON-структур тоже под запретом «имя поля = имя типа/класса»** (ошибка
  `Variable name 'World' already used as type name`). Т.к. в JSON-структурах нельзя
  спрятать поле за `m_`-префиксом (ключ должен совпадать), используй уточняющие имена:
  `World` → `WorldName`, `Class` → `ClassName` (аналогично `Type` как метод — ок, но
  `Class`/`World`/`Object`/`Entity`/`Man` и т.п. — имена типов, нельзя).
- **`JsonSerializer` НЕ применяет инициализаторы полей при десериализации**:
  `float Chance = 1.0;` при отсутствии ключа `"Chance"` читается как `0.0`. Либо
  всегда пиши значение явно, либо нормализуй после `Load()`. Для массивов-по-умолчанию
  — `autoptr array<ref T> X;` (автоинициализация пустым массивом, в отличие от `ref`,
  который останется `null`).
- **`autoptr array<ref T>` надёжен только для ДЕСЕРИАЛИЗАЦИИ (входного JSON).**
  Если объект создаётся через `new` и поле заполняется в коде через `Insert`, а затем
  сериализуется — элементы НЕ сохраняются (в выходе `[]`; `Insert` в такое поле —
  silent no-op). Для output-структур (заполняются в коде → сериализуются) объявляй
  поле как `ref array<ref T>` и явно `new array<ref T>()` сразу после создания объекта.
- **`bool` в JSON сериализуется как `1`/`0`**, не `true`/`false` (в `SaveFile`-выводе).

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
- **NULL-проверка должна оборачивать ВСЕ последующие обращения к ссылке.** Отдельный
  `if (!ref) { ... }` перед строкой `ref.Method()` НЕ защищает её: если `ref == null`,
  выполнение дойдёт до `ref.Method()` и упадёт (`NULL pointer to instance`). Объединяй в
  один `if` с коротким замыканием (`||`/`&&` в Enfusion коротко замыкаются):
  ```c
  // ПЛОХО: после удаления по !t.m_Entity строка ниже всё равно упадёт на null
  if (!t.m_Entity) m_Targets.RemoveItem(t);
  if (!t.m_Entity.IsAlive()) m_Targets.RemoveItem(t);   // NULL-deref
  // ХОРОШО: null-проверка первой, IsAlive() вызовется только при не-null
  if (!t.m_Entity || now - t.m_LastContact > timeout || !t.m_Entity.IsAlive())
      m_Targets.RemoveItem(t);
  ```
- `enum Name { A, B, C }` — int-перечисления (`FileAttr`, `FindFileFlags`, ...).
  **Значения enum НЕ попадают в глобальную область видимости — доступ только
  квалифицированно:** `Name.A`, `Name.B`. Голое `A` (даже в том же файле, где объявлен
  enum, и в `switch`-`case` по этому enum) даёт ошибку `Can't find variable 'A'`.
  `case A:` → `case Name.A:`.
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
- **Составное присваивание на элементе вектора** `v[i] += x` / `v[i] -= x` — НЕ
  поддерживается: ошибка `Operator '-=' not supported with array accessor [] (TODO)`.
  Пиши `v[i] = v[i] + x;` / `v[i] = v[i] - x;` — либо скопируй в локальную `float` и
  делай `-=` на ней (эталон — `LookAtPoint` в `dmAISurvivor.c`).
- **Нормировка вектора — только `v.Normalize()`, НЕ скалярное деление `v = v / len`.**
  `vector / float` недопустимо (не нормирует/не компилируется). Нормировать — `dir.Normalize()`;
  уменьшить длину — `dir * (1.0 / len)` (только умножение на скаляр). Ошибка:
  `dir = dir / distTo;` → правильно `dir.Normalize();`.

## Проверочный список при написании кода

- Нет тернарников.
- Никаких переносов строк внутри выражения/цепочки вызовов.
- В функции каждое имя переменной объявлено ровно один раз.
- Нельзя использовать имя переменной/поля, совпадающее с именем ТИПА/КЛАССА (напр. `P2` —
  есть такой тип в DayZ) — конфликт имён. Конвенция: переменные/поля — camelCase (с маленькой
  буквы), классы/типы — с заглавной. Не начинай имя переменной с заглавной буквы.
- `ref` для не-Managed-ссылок (поля/локалы), но НЕ на параметрах.
- `out`/`inout` — только в сигнатуре; на месте вызова без `out`/`inout`.
- Генерик не вызывает методы `T` без каста.
- Логи — через `#ifdef` на месте вызова; `dmBotLog.Error` не гейтится.
- Лог-строка не длиннее ~6 конкатенаций (`Formula too complex`); длинные логи — несколькими `Debug`-вызовами.
- Векторная арифметика — без inline-вызовов методов (сначала локалы, потом `-`/`+`; дистанция — `vector.Distance`).
- RHS присваивания элементу вектора (`v[i] = ...`) — только скаляр (не вектор и не `F(v)`).
- Нормировка вектора — `Normalize()`, не `v / len`.
- Недопустимо деление вектора на скалар: `v / 2`. Вместо этого используй эквивалент-умножение.
- **Масштаб whole-map/merge (~44k узлов/рёбер): аудируй ВЕСЬ путь на O(n²), не только замёрзшее место.** Любой скан узлов/рёбер внутри цикла по ним же — O(n²) → зависание сервера на десятки минут (v3.238 починил только `SewNodes` spatial-hash, но пропустил `HasEdge`-дедуп, `ClassifyNodes` `nodes×edges` и gap-детектор `deadends²×edges`). Приёмы: degree-массив (индекс = id узла; после `SewNodes` id контигуальны 0..N-1), `map<string,bool>` для дедупа, spatial hash для близости. После оптимизации одного места — пройдись grep'ом по всему merge-пути на вложенные циклы.
- **Граф: Id узлов обязаны быть контигуальными 0..N-1 при сериализации тайла.** Удаление узлов (`Nodes.Remove`) без перенумерации оставляет дыры в Id; склейка полагает `Id == индекс массива` (`nodeOffset = Count()`, `SewNodes` индексирует `parent` по `edge.From`). Дыры → коллизии Id соседних тайлов → рёбра ремапятся на чужой узел (длинные рёбра до 13 км при «здоровых» тайловых сегментах ≤32 м). После любого удаления узлов добавляй перенумерацию (`RenumberContiguous`).
- **`map<K,V>.Find(key, out v)` для ПРИМИТИВНЫХ value-типов при промахе пишет в `out` дефолт (0.0/0/false), а НЕ оставляет прежнее значение** (reference-типы — оставляет). Не полагайся на сентинел, предустановленный до `Find`: проверяй возврат и при `false` явно ставь сентинел (`if (!m.Find(k, v)) v = INF;`). Баг: A*-релаксация `gv=INF; g.Find(v,gv); if (nd<gv)` обнуляла gv → релаксация никогда не шла → пустой путь при связных парах (тишина скрывала баг — всегда логируй неудачу `dmBotLog.Error`).
- **`EntityAI.SetLifetime(0)` = «истечь сразу», а НЕ «бесконечно».** Объект из `CreateObject` без CE-профиля молча деспавнится (CE-чистка «протухшего» `lifetime <= 0`). Чтобы тестовый/спавнимый объект жил — ставь большой положительный `SetLifetime(3600)` + `SetLifetimeMax(3600)` (потолок; канонический «никогда» у Expansion — `SetLifetimeMax(3888000)` = 45 дней). Эталоны: `ExplosivesBase.c` (`0.15` = живи 0.15 с), `ExpansionGarageVehicle.c` (`3888000`).
- **Enforce Script отвергает смешанную арифметику `int*float` и `float/int` без каста** — грамматическая ошибка `Syntax error`, которая каскадит на объявление класса (`Syntax error` на строке `class X` + `Unexpected scope` на `{` + `Function definition inside block` где-то ещё — парсер не восстанавливается). Кастуй явно: `(float)intVar * floatConst`, `float / (float)intVar`. Эталон — `(float)chosenStart * DM_DRIVE_LANE_STEP` в `dmBotIntent_Drive`. (Смежно: `Math.Round` ненадёжен — используй `Math.Floor(x + 0.5)`; `array[method()] = v` и `RaycastRVParams(a,b)` (2 арг) — тоже вызывали проблемы, замени на локаль индекса и 3-арг `(a,b,null)`.)
- Сомневаешься в синтаксисе → ищи пример в ванильных/Expansion скриптах.
