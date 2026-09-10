https://steamcommunity.com/sharedfiles/filedetails/?id=3796005638

[h1] BOTORAMA – Customizable Intent-Based FSM [/h1]
[h1] WARNING! MOD IS IN DEVELOPMENT [/h1]

[h1] Overview [/h1]
This mod introduces a highly customizable AI system for DayZ, built around a flexible **Intent-Based Finite State Machine (FSM)**.
Bots are autonomous, react to threats, navigate the world, interact with doors, and engage in combat with realistic aiming.

[h1] Key Features [/h1]

[b]🧠 Intent System[/b]
Each bot controls five independent mobility channels:
[list]
[*] Movement
[*] Look
[*] Emotion
[*] Stance
[*] Attack
[/list]

Intents are atomic actions assigned to one channel. They can be [b]parallel[/b] or [b]exclusive[/b], with [b]critical[/b] and [b]non-critical[/b] priorities.

Three intent pools manage behavior:
[list]
[*] [b]Personal pool[/b] – instant reactions to threats (dodge, counterattack, pick up a weapon)
[*] [b]FSM pool[/b] – intents added by the current FSM state (reset on state change)
[*] [b]Command pool[/b] – orders from other bots in the group
[/list]
This allows a bot to simultaneously walk, look at you, and wave its hand.

[b]⚙️ Hierarchical FSM[/b]
The high-level state machine (Idle, Patrol, Combat…) manages behavior and populates the FSM intent pool.
Transitions have [b]weights[/b] and conditions (Require / BlockWhen), with weighted random selection.

[b]🎯 Realistic AIM[/b]
The aiming component is realistic:
[list]
[*] Base accuracy depends on optics
[*] Realistic recoil, bullet dispersion depends on the weapon
[*] Proactive fire-mode selection by distance (single / burst / auto)
[*] Penalties for movement of the target
[*] Distance influence on accuracy
[*] Bot's health affects base accuracy
[/list]
Bots will not instantly headshot you; they take time to aim, react to damage, and manage ammo.

[b]🚪 Smart Door Handling[/b]
Bots detect closed doors, open them (stepping back if needed), and pass through.

[b]🧭 Advanced Navigation[/b]
Utilizes DayZ NavMesh with custom path filter costs.
Point reachability checks ensure bots don’t try to walk through walls or off the mesh.

[b]🔧 Extensibility[/b]
Component-based architecture (sensors, navigation, AIM, FSM, intents).
Modular design – new intents and states plug in without touching the core.

[h1] Notes [/h1]
This mod is designed for server owners and modders who want full control over AI behavior.
It can be used as a foundation for custom AI scenarios (military, zombies, friendly NPCs).

[h1] WARNING! MOD IS IN DEVELOPMENT [/h1]

----------------------------------

[h1] BOTORAMA – Настраиваемый FSM на основе интентов [/h1]
[h1] ВНИМАНИЕ! МОД В СТАДИИ РАЗРАБОТКИ [/h1]

[h1] Обзор [/h1]
Этот мод добавляет в DayZ гибкую систему искусственного интеллекта, построенную на конечном автомате (FSM) с использованием интентов.
Боты действуют автономно, реагируют на угрозы, перемещаются по миру, взаимодействуют с дверями и ведут бой с реалистичным прицеливанием.

[h1] Ключевые особенности [/h1]

[b]🧠 Система интентов[/b]
Каждый бот управляет пятью независимыми каналами мобильности:
[list]
[*] Перемещение
[*] Взгляд
[*] Эмоция
[*] Стойка
[*] Удар
[/list]

Интенты – это атомарные действия, привязанные к одному каналу. Они могут быть [b]параллельными[/b] или [b]эксклюзивными[/b], с [b]критическими[/b] и [b]некритическими[/b] приоритетами.

Три пула интентов управляют поведением:
[list]
[*] [b]Личный пул[/b] – мгновенные реакции на угрозу (отойти, ударить в ответ, подобрать оружие)
[*] [b]FSM пул[/b] – интенты, добавленные текущим состоянием конечного автомата (сбрасываются при смене состояния)
[*] [b]Командный пул[/b] – приказы от других ботов в группе
[/list]
Это позволяет боту одновременно идти, смотреть на вас и махать рукой.

[b]⚙️ Иерархический FSM[/b]
Высокоуровневый конечный автомат (Idle, Patrol, Combat…) управляет логикой и заполняет FSM пул интентов.
Переходы имеют [b]веса[/b] и условия (Require / BlockWhen) со взвешенным случайным выбором.

[b]🎯 Реалистичное прицеливание (AIM)[/b]
Компонент прицеливания реалистичный:
[list]
[*] Базовая точность, зависящая от наличия оптики
[*] Реалистичная отдача, разброс пуль зависит от оружия
[*] Проактивный выбор режима огня по дистанции (одиночный / очередь / авто)
[*] Штраф за движение цели
[*] Влияние дистанции на точность
[*] Влияние здоровья бота на его базовую точность
[/list]
Боты не будут мгновенно попадать в голову – им нужно время на прицеливание, они реагируют на урон и следят за боезапасом.

[b]🚪 Умная обработка дверей[/b]
Боты обнаруживают закрытые двери, открывают их (при необходимости отступая назад) и проходят.

[b]🧭 Продвинутая навигация[/b]
Используется NavMesh DayZ с настраиваемыми стоимостями областей в фильтре пути.
Проверки достижимости точек гарантируют, что боты не пытаются пройти сквозь стены или за пределы сетки.

[b]🔧 Расширяемость[/b]
Компонентная архитектура (сенсоры, навигация, AIM, FSM, интенты).
Модульная архитектура – новые интенты и состояния подключаются без изменения ядра.

[h1] Примечания [/h1]
Мод предназначен для владельцев серверов и моддеров, которым нужен полный контроль над поведением ИИ.
Его можно использовать как основу для собственных сценариев (военные, зомби, дружелюбные NPC).

[h1] ВНИМАНИЕ! МОД В СТАДИИ РАЗРАБОТКИ [/h1]