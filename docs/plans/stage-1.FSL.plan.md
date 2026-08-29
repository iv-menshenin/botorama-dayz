# Stage 1 — FSM движка ботов (dmBotFSM)

> **Статус: исторический план (завершён / замещён).** Это ранний пофазный TODO
> (Phase 0–4) и модель, согласованная до реализации. Актуальный дизайн кода,
> текущие этапы и следующий шаг (движение к точке) — в
> `Reference/fsm-implementation-plan.md`. Модель ниже местами устарела
> (переходы без `BlockWhen`/`Require`/`dmBotCondition`, `dmBotJson`→`dmJsonFile`,
> старая файловая схема без `Intent/`).

План и подробное TODO. Зафиксировано по итогам обсуждения архитектуры FSM для
`botorama`. Цель — вынести «что делает бот» из захардкоженного
`dmAISurvivor.OnUpdate` (сейчас там `LookAtPoint(m_LookTarget)` + `UpdateLook`) в
конечный автомат, оставив в пешке `dmAISurvivorBase` только анимацию
(голова/переступание/движение).

---

## Зафиксированные решения

1. **Переходы** — каждый переход имеет `weight` (шанс 0.0–1.0) + опциональный
   `Guard()` (условие). Выбор — взвешенный случайный.
2. **JSON-конфиг** — один файл на пресет (`fsm_<preset>.json`).
3. **Конфиг** — жёсткие дефолты в коде + переопределение из `$profile:` на этапе
   init мода (как Expansion `Defaults()` + `JsonFileLoader`).
4. **Топология FSM** — только кодом (состояния/переходы/веса — Enfusion-классы и
   пресеты-фабрики). JSON управляет только параметрами.

---

## Модель

### `dmBotTransition`
```c
class dmBotTransition
{
	dmBotState m_To;        // целевое состояние
	float m_Weight;         // шанс 0..1 (нормируется суммой при выборе)

	dmBotState GetDestination();
	float GetWeight();
	bool Guard();           // опциональное условие, по умолчанию true
}
```

### `dmBotState`
```c
class dmBotState
{
	static const int EXIT = 0;      // состояние завершено -> переход
	static const int CONTINUE = 1;  // продолжаем

	string m_Name;
	autoptr array<ref dmBotTransition> m_Transitions;

	void AddTransition(dmBotState to, float weight);
	array<ref dmBotTransition> GetTransitions();

	void OnEntry(dmBotState from);
	void OnExit(dmBotState to);
	int OnUpdate(float pDt);        // EXIT | CONTINUE
}
```

### `dmBotFSM`
```c
class dmBotFSM
{
	dmAISurvivor m_Owner;           // мозг бота
	autoptr array<ref dmBotState> m_States;
	dmBotState m_CurrentState;
	string m_DefaultState;

	void AddState(dmBotState s);
	dmBotState GetState(string name);
	bool Start(string name = "");
	int Update(float pDt);          // OnUpdate(current); при EXIT -> выбор -> OnExit/OnEntry
	bool SelectTransition(dmBotState from, out dmBotState dst); // взвешенный выбор
}
```

### Взвешенный случайный переход (требование «разнообразие»)
```
собрать переходы, у которых Guard() == true
sum = Σ weight
если sum <= 0 -> перехода нет
r = Math.RandomFloat01() * sum        // [0, sum)
acc = 0
для каждого eligible-перехода:
    acc += weight
    если r < acc -> выбран этот переход
```
Ровно «рандом от 0 до суммы шансов» — первый в списке не получает предпочтения.

### Пресеты (требование «гибкость установки»)
- Состояние = класс-наследник `dmBotState` (новое состояние = новый класс в
  `States/`).
- Пресет = класс-фабрика `dmBotPreset_X.Create(owner)` → готовый `dmBotFSM`.
- Бот грузит пресет по имени (`LoadFSM("Default")`), у разных ботов могут быть
  разные пресеты (не один FSM на всех).

### Конфиг (требование «гибкость настройки»)
- `dmBotConfig` (реестр) на init читает `$profile:dmBotorama/fsm_<preset>.json`.
- Дефолты параметров заданы в коде (`Defaults()`), JSON переопределяет только
  присутствующие ключи.
- Состояние берёт параметр (напр. «порог гидрации») через конфиг:
  `dmBotConfig.GetFloat(preset, "Drink.hydrationThreshold", 50.0)`.

---

## Файловая система (дополнение к текущей)

```
core/
├── 3_Game/
│   ├── Logging/dmBotLog.c
│   └── Config/                     NEW
│       ├── dmBotJson.c             обёртка над JsonFileLoader/JsonSerializer
│       └── dmBotConfig.c           реестр конфигов: Defaults() + $profile: override
├── 4_World/Entities/Bot/
│   ├── dmAISurvivor.c              мозг (владеет FSM, действия LookAt/MoveTo/Use)
│   ├── dmAISurvivorBase.c          пешка (анимации)
│   ├── FSM/                        NEW
│   │   ├── dmBotFSM.c
│   │   ├── dmBotState.c
│   │   └── dmBotTransition.c
│   ├── States/                     NEW
│   │   └── dmBotState_*.c
│   └── Presets/                    NEW
│       └── dmBotPreset_*.c
```

`config.cpp` уже перекрывает `core/3_Game` и `core/4_World` рекурсивно — новых
записей в `files[]` не требуется. `fstructure.md` дополняется правилами:
`FSM/`, `States/`, `Presets/`, `Config/`.

---

## Зависимости (до / параллельно FSM)

1. **`core/3_Game/Config`** — модуль чтения JSON (обязателен, Phase 0).
2. **Интерфейс «мозг → действие»** — формализовать на `dmAISurvivor`:
   `LookAt(entity/point)`, `MoveTo(vector)`, `Use(entity)`, `GetThirst/GetHunger/GetThreat`,
   чтобы состояния не лезли в пешку напрямую (рефакторинг).
3. **Движение** — для Wander/Drink/Flee: `OverrideMovementAngle/Speed` либо
   кастомный `HumanCommandMove` по образцу Expansion `eAICommandMove`
   (параллельный воркстрим).
4. **Статы** — обёртка над статами `PlayerBase` (жажда/голод/здоровье/угроза).

---

## Этапы

| Phase | Содержание |
|-------|-----------|
| 0 | JSON-модуль (`dmBotJson` + `dmBotConfig`), дефолты + `$profile:` override |
| 1 | Ядро FSM (`dmBotFSM/State/Transition`) + взвешенный выбор + guard |
| 2 | Первые состояния (`Idle`, `Observe`) + `dmBotPreset_Default`, замена хардкода в `OnUpdate` |
| 3 | Параметры состояний из JSON (пороги, веса, длительности) |
| 4 | Расширение состояний (`Wander`/`Drink`/`Eat`/`Flee`), движение, выбор пресета на спавне |

---

## Подробное TODO

### Phase 0 — модуль чтения JSON
- [ ] `core/3_Game/Config/dmBotJson.c` — generic-обёртка над
      `JsonFileLoader<T>`/`JsonSerializer`: `Load(path, out T, out error)`,
      `Save(path, T)`, `LoadData(string, out T, out error)`.
- [ ] `core/3_Game/Config/dmBotConfig.c` — реестр: `Init()` (загрузка конфигов
      пресетов), `GetFloat/GetInt/GetBool/GetString(preset, key, default)`,
      `GetConfig(preset)`; путь `$profile:dmBotorama/fsm_<preset>.json`.
- [ ] Базовый класс конфига пресета (`JsonApiStruct`-совместимый) с `Defaults()`.
- [ ] Пример JSON + проверка в логе: один ключ читается, дефолт подставляется
      при отсутствии файла/ключа.

### Phase 1 — ядро FSM
- [ ] `core/4_World/Entities/Bot/FSM/dmBotTransition.c` — `m_To`, `m_Weight`,
      `Guard()`, геттеры.
- [ ] `core/4_World/Entities/Bot/FSM/dmBotState.c` — `m_Name`, `m_Transitions`,
      `AddTransition`, `OnEntry/OnExit/OnUpdate`, константы `EXIT/CONTINUE`.
- [ ] `core/4_World/Entities/Bot/FSM/dmBotFSM.c` — `AddState/GetState/Start/Update`,
      `SelectTransition` (взвешенный выбор + фильтр по `Guard`).
- [ ] Проверка распределения: пресет из 3 состояний с весами (напр. 0.2/0.3/0.5),
      прогон N тиков, залогировать частоты — убедиться что близко к весам.

### Phase 2 — первые состояния + пресет, замена хардкода
- [ ] `States/dmBotState_Idle.c` — базовое «осмотреться/ждать».
- [ ] `States/dmBotState_Observe.c` — наблюдение за целью (LookAt).
- [ ] `Presets/dmBotPreset_Default.c` — фабрика: Idle ↔ Observe с весами.
- [ ] `dmAISurvivor`: поле `ref dmBotFSM m_FSM`; `LoadFSM(string presetName)`;
      `OnUpdate` → `m_FSM.Update(pDt)` (убрать хардкод `SetLookTarget`/`UpdateLook`).
- [ ] `MissionServer.HandleBotSpawnTest`: вместо `SetLookTarget(player)` →
      `bot.LoadFSM("Default")` (пресет сам решает, наблюдать ли за игроком).

### Phase 3 — конфигурируемые параметры
- [ ] Перевести веса/длительности/пороги состояний на чтение из `dmBotConfig`
      (дефолты в коде + override из JSON).
- [ ] Пример `fsm_default.json` + загрузка из `$profile:dmBotorama/`.
- [ ] Документировать ключи конфига (в `fstructure.md` или рядом с JSON).

### Phase 4 — расширение и разнообразие
- [ ] `States/dmBotState_Wander.c` (движение к точке), `dmBotState_Drink.c`,
      `dmBotState_Eat.c`, `dmBotState_Flee.c`.
- [ ] Движение: `dmAISurvivor.MoveTo(vector)` (OverrideMovementAngle/Speed или
      кастомный command).
- [ ] Статы: обёртки `GetThirst/GetHunger/GetThreat`.
- [ ] Выбор пресета на спавне: `/bot spawn test <preset>`.
- [ ] Обновить `fstructure.md` и `Reference/GrowUp-1.txt` по итогам.

---

## Приёмка Stage 1
- Бот перестаёт «зацикленно смотреть на игрока»: поведением рулит FSM.
- Один пресет показывает веер переходов из состояния (разные исходы в разные
  прогоны — разнообразие работает).
- Параметры состояния меняются правкой JSON в `$profile:` без пересборки мода.
- Новое состояние добавляется новым классом без правки ядра FSM.
