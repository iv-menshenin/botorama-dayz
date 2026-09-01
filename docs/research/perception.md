# Research: зрение и слух ботов (perception)

Статус: **реализовано (T3, T4 — память целей)**. Задача T3 плана
(`docs/plans/ai-development-plan.md`) — детект игроков, заражённых и животных; T4 —
память целей (`dmTarget` как память + оценка). Слух (звуки) и перцепция предметов
(лут) — следующие вехи (T13). Ведёт субагент `dayz-research`.

## Цель

Бот «видит» заражённых (зомби), животных и игроков в радиусе/FOV/при прямой
видимости; результат → цели (`dmTarget`) для боя/реакции. Позже — и предметы (лут)
и звуки (слух).

## Реализация (T3)

`core/4_World/Entities/Bot/Perception/dmVision.c` — поле `ref dmVision m_Vision` в
мозге `dmAISurvivor`; тикает из `OnUpdate` с троттлингом `DM_PERCEPTION_INTERVAL`
(0.3 c). Пайплайн `Scan()`: box-запрос → классификация → дистанция/FOV → LOS →
`dmTarget` (DESTROY). Переключение Scene/Physics — `ToggleQuery()` (команда
`/bot vision switch`).

## Реализация (T4) — память целей

`dmTarget` — теперь память + оценка (не снимок видимости): память `m_LastPosition` /
`m_HasLOS` / `m_LastContact` (время = `GetGame().GetTickTime()`, float секунды) +
оценка `m_Threat` / `m_Attractiveness` (0..1) / `m_Friendly` (пока = только цель
эскорта). `m_Priority` убран. `Scan()` больше не делает `ClearTargets()`: `BeginTargetScan()`
сбрасывает `m_HasLOS`, на каждый LOS-успех — `RememberTarget(entity, threat, attract,
friendly, pos)` (создать/обновить, `m_LastContact = now`), после цикла —
`ForgetStaleTargets(DM_TARGET_FORGET_TIME = 300с)` выкидывает цели без контакта дольше
таймаута. Скрывшаяся цель остаётся в списке с последней известной позицией до забывания.
Константы оценки/таймаута — в `cons/4_World/constants.c`.

## TODO (переработка запроса)

Scan-box (физика) — временно и не масштабируется. Цель: зомби/животные — все в радиусе
50 м из глобального регистра (если ванильного нет — закастомить: регистрировать в
конструкторе, снимать в деструкторе); игроки — сканировать всех в радиусе 1 км.
Детальнее — `docs/techdebt.md` (раздел C).

## Проверенные сигнатуры (ваниль, `DayZ Projects/scripts`)

- `4_world/entities/dayzplayerutils.c`:
  - `static proto native void PhysicsGetEntitiesInBox(vector min, vector max, notnull out array<EntityAI> entList);`
  - `static proto native void SceneGetEntitiesInBox(vector min, vector max, notnull out array<EntityAI> entList, int flags = QueryFlags.DYNAMIC);`
- `Object.IsInherited(typename baseType)` → `bool` (напр. `entity.IsInherited(ZombieBase)`).
  Игрок — `PlayerBase.Cast(entity)` (null, если не игрок).
- Классы существ: `ZombieBase extends DayZInfected`
  (`4_world/entities/creatures/infected/zombiebase.c`), `AnimalBase extends DayZAnimal`
  (`4_world/entities/creatures/animals/animalbase.c`).
- Кости: `GetBoneIndexByName("Head")` → `int` (-1 = нет) — **НЕ на `EntityAI`**, а на
  `Human` (игроки) и `DayZCreature` (зомби/животные) → каст `Human.Cast` / `DayZCreature.Cast`
  перед вызовом. `GetBonePositionWS(int)` → `vector` и `GetBoneTransformWS(int, out vector
  transform[4])` — на `Object` (общий базовый, без каста). **forward = `transform[1]`**
  (костное пространство, индекс 1).
- LOS (`3_game/global/dayzphysics.c`):
  - `proto static bool RaycastRVProxy(notnull RaycastRVParams in, out notnull array<ref RaycastRVResult> results, array<Object> excluded = null);`
  - `class RaycastRVParams` (нативный, создаётся `new` БЕЗ `ref`): поля `vector begPos`,
    `vector endPos`, `Object ignore`, `Object with`, `float radius`, `CollisionFlags flags`,
    `int type`, `bool sorted`, `bool groundOnly`. Конструктор
    `RaycastRVParams(vBeg, vEnd, pIgnore = null, fRadius = 0.0)` ставит дефолты
    `flags = CollisionFlags.NEARESTCONTACT`, `type = ObjIntersectView`, `sorted = false`.
  - `class RaycastRVResult`: `Object obj`, `Object parent`, `vector pos`, `vector dir`,
    `int hierLevel`, `int component`, `SurfaceInfo surface`, `bool entry`, `bool exit`.
  - Массив результатов — `array<ref RaycastRVResult>` (эталон `actiontargets.c:217`).

## Решения открытых вопросов

1. **Как перечислить сущности рядом**: box-запрос, НЕ конус и НЕ `GetPlayers()` вручную.
   `SceneGetEntitiesInBox` (по сцене, ловит DYNAMIC) и `PhysicsGetEntitiesInBox` (по
   физике) — обе дешёвые; выбор вынесен в переключатель `m_UseScene` (по умолчанию
   Scene). Дешевизну конкретной сцены можно проверить `/bot vision switch` + дебаг-лог.
2. `GetEntitiesInCone` — **не использован** (box + FOV-фильтр достаточен и проще).
3. Фильтр по типу — `PlayerBase.Cast(e)` (игрок/бот), `e.IsInherited(ZombieBase)`,
   `e.IsInherited(AnimalBase)`; порядок player → zombie → animal.
4. Троттлинг — `DM_PERCEPTION_INTERVAL = 0.3` c; радиус `DM_PERCEPTION_RADIUS = 30.0` м;
   FOV `DM_PERCEPTION_FOV = 120.0`° (полуугол 60°); высота бокса `DM_PERCEPTION_HEIGHT = 2.0` м.
5. Встроенного «видит ли A объект B» нет — свой raycast через `RaycastRVProxy`
   (глаза→голова цели; видно, если ближайшее попадание — сама цель).

## Источники

- `DayZ Projects/scripts/4_world/classes/dayzplayerutils.c`
- `DayZ Projects/scripts/3_game/global/dayzphysics.c`
- `DayZ Projects/scripts/4_world/classes/useractionscomponent/actiontargets.c`
- `DayZ Projects/scripts/4_world/entities/creatures/infected/zombiebase.c`
- `DayZ Projects/scripts/4_world/entities/creatures/animals/animalbase.c`
- `DayZ-Expansion-Scripts/.../eAIBase.c` (их перцепция — референс, но Expansion-специфична)

## Слух (noise)

Статус: **исследование** (веха T13). Приём через нативный `NoiseSystem` недоступен
(он только `AddNoise*` — sink для НАТИВНОЙ сенсорики зомби/животных; сам шум не
вызывает script-колбэков). Поэтому, как Expansion, делаем **свой** сигнал
(`ScriptInvoker`) и вешаем его на точки генерации шума (modded-хуки). Приём — свой
фильтр дистанции/типа → обновить/добавить `dmTarget` (HasLOS=false + последняя позиция).

### Резюме

- Ваниль: `NoiseSystem` = нативный sink, `GetNoiseSystem().AddNoise*/AddNoiseTarget`
  — этим пользуются зомби/животные (нативно). Сенсорика НЕ script-доступна.
- Выстрел — НЕ генерит script-шум (нативно в движке); bullet-impact/взрыв/шаги/крик
  зомби/двери — генерит script-вызовы `GetNoiseSystem().AddNoise*` (точки ниже).
- Expansion: `eAINoiseSystem` (статический `ScriptInvoker` `SI_OnNoiseAdded` + `AddNoise*`
  overloads + кэш `eAINoiseParams`), каждый ИИ подписывается `eAI_OnNoiseEvent`.
- Наш аналог: `dmNoiseSystem` (сигнал) + `dmHearing` (приём, фильтр, обновление `dmTarget`).

### Ванильные сигнатуры

- `3_game/noise.c`:
  - `class NoiseSystem` — `proto void AddNoise(EntityAI source_entity, NoiseParams noise_params, float external_strenght_multiplier = 1.0);` (стр. 6);
    `proto void AddNoisePos(EntityAI source_entity, vector pos, NoiseParams noise_params, float external_strenght_multiplier = 1.0);` (стр. 7);
    `proto void AddNoiseTarget(vector pos, float lifetime, NoiseParams noise_params, float external_strength_multiplier = 1.0);` (стр. 10) — единственный с `lifetime` («пинг» в точке).
  - `class NoiseParams` — `void NoiseParams()`; `proto native void Load(string noise_name);` (стр. 20); `proto native void LoadFromPath(string noise_path);` (стр. 22).
- `3_game/global/game.c:737` — `proto native NoiseSystem GetNoiseSystem();`
- `strength` (радиус слышимости, м) задаётся в конфиге; `NoiseParams.Load/LoadFromPath`
  читает его из конфига (см. Expansion `ConfigGetFloat(path + " strength")`).
  Мультипликаторы: погода `NoiseAIEvaluate.GetNoiseReduction(g_Game.GetWeather())`
  (sensesaievaluate.c:18), поверхность `SurfaceGetNoiseMultiplier`
  (`3_game/global/game.c:1172` — `proto native float SurfaceGetNoiseMultiplier(Object directHit, vector pos, int componentIndex);`).
- `4_world/static/sensesaievaluate.c` — `NoiseAIEvaluate.GetNoiseMultiplier(DayZPlayerImplement)` (стр. 5):
  скорость (`PlayerConstants.AI_NOISE_IDLE/WALK/CROUCH_RUN/RUN/SPRINT/ROLL`) × обувь
  (`AI_NOISE_SHOES_NONE/SNEAKERS/BOOTS`, `GetBootsType()`) × поверхность
  (`GetSurfaceNoise()`, вес 0.25); `GetNoiseReduction(Weather)` (стр. 18).
- `4_world/static/miscgameplayfunctions.c:1753` — `static void GenerateAINoiseAtPosition(vector position, float lifeTime, NoiseParams noiseParams)` → `AddNoiseTarget(position, lifeTime, noiseParams, noiseReduction)` — готовый helper для своих «пингов».

### Точки генерации (первый проход)

**Выстрел** — НЕ ПОДТВЕРЖДЕНО в скриптах (нет script-`AddNoise`/`NoiseShot`):
- Путь выстрела: `WeaponFire.OnEntry` (`4_world/entities/firearms/fsm/states/weaponfire.c:66`
  `TryFireWeapon(m_weapon, mi)` → стр. 71 `m_weapon.OnFire(mi)`) → `proto native bool Fire(int muzzleIndex, vector pos, vector dir, vector speed);`
  (`4_world/entities/core/inherited/weapon.c:58`). Шум выстрела генерит движок нативно.
- В конфиге есть только `NoiseHit`/`NoiseExplosion` (не `NoiseShot`) → strength выстрела
  из конфига НЕ читается (Expansion «дорисовывает» масштабом `* 13.75`, clamp 1100).
- **Hook**: наш `modded Weapon_Base` (уже есть `SyncEventToRemote` для звука выстрела) —
  там же звать `dmNoiseSystem.AddNoise(стрелок, pos, path, strength, SHOT)`. Strength брать
  из `cfgAmmo <ammoType>` (если есть `NoiseShot`) либо свой пресет. Открытый вопрос.

**Шаги игрока** — `4_world/entities/dayzplayerimplement.c`:
- `OnStepEvent(string pEventType, string pUserString, int pUserInt)` (стр. 3215); серверная
  ветка (стр. 3317–3344): выбор `NoiseParams` по стойке — `type.GetNoiseParamsStand()/Crouch()/Prone()`
  (стр. 3324–3334), `noiseMultiplier = NoiseAIEvaluate.GetNoiseMultiplier(this) * GetNoiseReduction(weather)`
  (стр. 3336), `AddNoise(noiseParams, noiseMultiplier)` (стр. 3338).
- `AddNoise(NoiseParams noisePar, float noiseMultiplier = 1.0)` (стр. 3204) → `GetNoiseSystem().AddNoise(this, noisePar, noiseMultiplier)` (стр. 3207).
- `3_game/dayzplayer.c`: `GetNoiseParamsStand/Crouch/Prone` (стр. 366/371/376);
  `m_pNoiseStepStand/Crouch/Prone.LoadFromPath(cfgPath + "NoiseStepStand/Crouch/Prone")` (стр. 516–523).
- **Hook**: `modded DayZPlayerImplement.OnStepEvent` (или наш собственный код шага для
  бота). Для слуха ЧУЖИХ шагов — override `OnStepEvent` → `dmNoiseSystem.AddNoise(...)`.
- Смежное: `OnSoundEvent(...)` (стр. 3347) — общий anim-sound noise: стр. 3569–3570
  `if (soundEvent.m_NoiseParams != NULL) GetNoiseSystem().AddNoise(this, soundEvent.m_NoiseParams, ...)`.

**Падение пули рядом (bullet impact)** — `3_game/dayzgame.c`:
- `FirearmEffects(...)` (стр. 3614), серверная ветка (стр. 3641–3664): `m_NoiseParams.LoadFromPath("cfgAmmo " + ammoType + " NoiseHit")` (стр. 3656),
  `surfaceCoef = SurfaceGetNoiseMultiplier(directHit, pos, componentIndex)` (стр. 3658),
  `coefAdjusted = surfaceCoef * inSpeed.Length() / ConfigGetFloat("cfgAmmo " + ammoType + " initSpeed")` (стр. 3659),
  `AddNoiseTarget(pos, 10, m_NoiseParams, coefAdjusted * noiseReduction)` (стр. 3663).
- `CloseCombatEffects(...)` (стр. 3668) — то же с `AddNoisePos(EntityAI.Cast(source), pos, ...)` (стр. 3701).
- **Hook**: `FirearmEffects` — метод `DayZGame` (не переопределить per-entity), но
  `directHit.OnReceivedHit(impactEffectsData)` (стр. 3634) — per-entity и moddable
  (`4_world/entities/itembase.c` / `playerbase.c:1380 OnReceivedHit`). Либо вешать на
  `modded ItemBase`/`modded PlayerBase.OnReceivedHit`.

**Крик/звук зомби** — `4_world/entities/creatures/infected/zombiebase.c`:
- `OnSoundVoiceEvent(int event_id, string event_user_string)` (стр. 550) →
  `ProcessSoundVoiceEvent(voice_event, m_LastSoundVoiceAW)` (стр. 571), где
  `voice_event = GetCreatureAIType().GetSoundVoiceEvent(event_id)` (стр. 553) = `AnimSoundVoiceEvent`.
- `ProcessSoundVoiceEvent(AnimSoundVoiceEvent sound_event, out AbstractWave aw)` (стр. 577);
  сервер: стр. 593–594 `if (sound_event.m_NoiseParams != NULL) g_Game.GetNoiseSystem().AddNoise(this, sound_event.m_NoiseParams, NoiseAIEvaluate.GetNoiseReduction(g_Game.GetWeather()));`.
- `AnimSoundVoiceEvent` (`3_game/dayzanimevents.c:218–273`): поле `autoptr NoiseParams m_NoiseParams`
  (стр. 223); на сервере грузится из конфига `soundPath + "noise"` (стр. 242–255).
- **Механизм найден**: крик/рык зомби = anim-событие `SoundVoice`; каждое такое событие в
  конфиге анимации имеет запись `noise` → `NoiseParams.Load(noiseName)`; на сервере
  `OnSoundVoiceEvent` вызывает `AddNoise(this, ...)`. **Hook**: `modded ZombieBase.OnSoundVoiceEvent`.
  (Ваниль не хранит `SoundSet` для крика как отдельный `NoiseParams` у `ZombieBase` — только
  через anim-события; `CaptureSound()/ReleaseSound()` в zombiemale/femalebase.c — это строки
  SoundSet, НЕ NoiseParams.)

### TODO-таблица «как добавить» (только точки, не реализовывать)

| Источник | Файл:строка | Что звать |
|---|---|---|
| Взрыв (гранаты/боеприпас) | `3_game/dayzgame.c:3470` `ExplosionEffectsEx` (noise: стр. 3482 `LoadFromPath("cfgAmmo %1 NoiseExplosion")`, стр. 3488 `AddNoiseTarget(pos, 21, ...)`) | per-entity hook `Object.OnExplosionEffects` (`3_game/entities/object.c:189`, `entityai.c:943`, `explosivesbase.c:64`); граната — `grenade_base.c:151 InitiateExplosion`. Override → `dmNoiseSystem.AddNoise(pos, lifetime, "cfgAmmo <ammoType> NoiseExplosion", EXPLOSION)` |
| Двери (открытие/закрытие) | `actionopendoors.c:71` `OnEndServer` → стр. 81 `noise.AddNoisePos(action_data.m_Player, target.GetPosition(), m_NoisePar, ...)`; `actionclosedoors.c` (стр. 63–70); fences `actionopenfence.c:60`/`actionclosefence.c:60`; params `CfgVehicles SurvivorBase NoiseActionDefault` (стр. 76/65) | modded action `OnEndServer` → `dmNoiseSystem.AddNoise(player, targetPos, "CfgVehicles SurvivorBase NoiseActionDefault", SOUND)` |
| Падение предметов (item impact) | `4_world/entities/itembase.c:1194` `EOnContact` → `ProcessImpactSoundEx` (стр. 1199) — **ваниль НЕ генерит NoiseSystem-шум** (только клиентский звук `PlayImpactSound`/`m_WantPlayImpactSound`, стр. 1203–1207) | своей точки нет; вешать `dmNoiseSystem.AddNoise(...)` в `modded ItemBase.EOnContact` (по `ProcessImpactSoundEx`/весу/скорости) или в наш drop-код |

### Expansion — эталон

- `DayZExpansion/AI/Scripts/3_Game/DayZExpansion_AI/eAINoiseSystem.c`:
  - `enum eAINoiseType { SHOT, SOUND, EXPLOSION, BULLETIMPACT }` (стр. 1–7).
  - `class eAINoiseParams` (стр. 9–50): `m_Path`/`m_Strength`/`m_Type`; ctor читает
    `g_Game.ConfigGetFloat(path + " strength")` (стр. 18); SHOT: `m_Strength = Math.Min(m_Strength * 13.75, 1100)` (стр. 40); BULLETIMPACT: `m_Strength *= 2` (стр. 44).
  - `class eAINoiseSystem` (стр. 52–114): `static ref ScriptInvoker SI_OnNoiseAdded = new ScriptInvoker;` (стр. 54);
    `static ref map<string, eAINoiseParams> s_NoiseParams` (стр. 56); `GetNoiseParams(path, type = -1)` (стр. 58);
    overloads `AddNoise(source[, pos[, lifetime]], path, strengthMultiplier, type)` (стр. 70/76/82) и
    `AddNoiseEx(source[, pos[, lifetime]], params, strengthMultiplier)` (стр. 88/93/98) — все
    `SI_OnNoiseAdded.Invoke(...)`; очистка кэша в `OnGameDestroy` (стр. 103–113).
- `.../Entities/AI/eAIBase.c`:
  - подписка в `Init()`: `eAINoiseSystem.SI_OnNoiseAdded.Insert(eAI_OnNoiseEvent)` (стр. 563),
    отписка в `eAI_Cleanup`: `SI_OnNoiseAdded.Remove(eAI_OnNoiseEvent)` (стр. 1427).
  - `void eAI_OnNoiseEvent(EntityAI source, vector position, float lifetime, eAINoiseParams params, float strengthMultiplier)` (стр. 4105–4278) — рецепт фильтра:
    1. `m_eAI_NoiseInvestigationDistanceLimit <= 0` → skip (стр. 4111); `IsUnconscious()` → skip (стр. 4114).
    2. `strength = params.m_Strength * strengthMultiplier`; `strength <= 0` → skip (стр. 4117–4119).
    3. source: `root = source.GetHierarchyRoot()`; `root == this` → skip (стр. 4122–4127);
       если уже целишься в root с LOS и threat ≥ 0.4 → skip (стр. 4129–4134).
    4. friendly-фильтр (не `PlayerIsEnemy`, намеренно): игнор шумов от союзных групп (стр. 4137–4153);
       игнор, если source — игрок с LOS к нему (стр. 4157–4159).
    5. `if (source && position == vector.Zero) position = source.GetPosition();` (стр. 4162).
    6. **дистанционный фильтр**: `strengthSq = strength * strength`; `distSq = vector.DistanceSq(GetPosition(), position)`; `if (distSq > strengthSq) return;` (стр. 4165–4168).
    7. BULLETIMPACT + source-игрок: добавить/обновить игрока как цель, threat = 1.0 если threat > 0.2 (стр. 4170–4202).
    8. конверсия strength→threat: `ExpansionMath.LinearConversion(distMin, strength * 1.1, distance, threatLevelMax, 0.1024)` (стр. 4245);
       **noise threat cap 0.4** (коммент стр. 4207; стр. 4242), BULLETIMPACT: `distMin=0.5, threatMax=0.2` (стр. 4234–4238), иначе `distMin=min(strength, limit), threatMax=0.4` (стр. 4240–4243).
    9. lifetime: threat ≥ 0.4 → `LinearConversion(0, 500, distance, 3.0, 240.0)`; иначе `RandomFloat(2.0, 4.0)` (стр. 4253–4256).
    10. задержка (скорость звука + реакция): `delay = distance * 2.915452 + 170` (стр. 4262); `SetNoiseParams(source, position, strength, lifetime, threatLevel)` (стр. 4258); `CallLater(eAI_AddNoiseTarget, delay, false, threatLevel)` (стр. 4263).
  - `void eAI_AddNoiseTarget(float threatLevel)` (стр. 4280–4298): `m_eAI_NoiseTargetInfo.AddAI(this, maxTime, true, created)` — создаёт/обновляет noise-цель (как отдельный `eAINoiseTargetInformation`).

### Что портировать → в botorama

| Expansion | botorama | Заметки |
|---|---|---|
| `eAINoiseSystem` (статический `SI_OnNoiseAdded` + `AddNoise*` + кэш params) | **`dmNoiseSystem`** (новый, напр. `core/4_World/Entities/Bot/Perception/dmNoiseSystem.c`) | `ScriptInvoker` + overloads `AddNoise(source[, pos[, lifetime]], path[, type])`; кэш `dmNoiseParams` (path+strength+type) как `eAINoiseParams` |
| `eAINoiseParams` (path/strength/type) | **`dmNoiseParams`** | читать strength из конфига `ConfigGetFloat(path + " strength")`; тип `SHOT/SOUND/EXPLOSION/BULLETIMPACT` |
| `eAINoiseType` | `dmNoiseType` (или enum в constants) | |
| подписка в `Init`/`eAI_Cleanup` | мозг `dmAISurvivor` (создание/уничтожение) | `SI_OnNoiseAdded.Insert(OnNoise)` / `Remove` |
| `eAI_OnNoiseEvent` (фильтр + threat/lifetime/delay) | **`dmHearing`** (новый, как `dmVision`) — `OnNoise(source, position, lifetime, params, mult)` | фильтр: distSq > strengthSq → skip; threat cap 0.4 (bullet 0.2); обновить/добавить `dmTarget` с `m_HasLOS=false`, `m_LastPosition=position`, `m_LastContact=now`, threat |
| modded-генераторы (не у Expansion — они вешают сигнал на свои сущности) | `modded Weapon_Base` (выстрел), `modded ZombieBase.OnSoundVoiceEvent` (крик), `modded DayZPlayerImplement.OnStepEvent` (шаги), `modded EntityAI.OnExplosionEffects` (взрыв), `modded ItemBase.EOnContact` (падение), `modded action ...OnEndServer` (двери) | ваниль зовёт `GetNoiseSystem().AddNoise*`; нам в тех же точках звать `dmNoiseSystem.AddNoise(...)` |
| константы (`NoiseInvestigationDistanceLimit`, threat caps, lifetime, delay) | `cons/4_World/constants.c` — `DM_NOISE_*` | напр. `DM_NOISE_INVESTIGATION_DISTANCE`, `DM_NOISE_THREAT_CAP 0.4`, `DM_NOISE_THREAT_CAP_IMPACT 0.2`, `DM_NOISE_DELAY_MS_PER_METER 2.915452` |

### Открытые вопросы

1. **Strength выстрела**: в конфиге ванили нет `NoiseShot` (только `NoiseHit`/`NoiseExplosion`).
   Expansion «масштабирует» SHOT `×13.75` (clamp 1100) от собственных `eAINoiseParams`.
   Для `dmNoiseSystem` нужно решить: читать ли `cfgAmmo <ammoType> NoiseShot` (если есть),
   либо задавать strength/пресет выстрела в своём конфиге.
2. **Приём чужих шагов**: `OnStepEvent` вызывается у игрока-источника; чтобы бот «слышал»
   шаги игрока, нужен `modded DayZPlayerImplement.OnStepEvent`, шлющий сигнал. То же для
   `OnSoundEvent` (действия/приземления). Подтверждено как точка, реализация не делалась.
3. **Нативный `AddNoise*` vs свой сигнал**: нативные вызовы `GetNoiseSystem().AddNoise*`
   НЕ видны нашему коду (нет событий); поэтому каждый источник надо продублировать своим
   `dmNoiseSystem`-вызовом в modded-хуке (ванильные вызовы остаются для зомби/животных).
4. **Троттлинг `dmHearing`**: как и `dmVision` — решить интервал (шумов может быть много,
   напр. очередь выстрелов); Expansion обрабатывает каждый `AddNoise*` без троттлинга, но с
   дешёвым `distSq`-фильтром первым делом.

### Источники (раздел «Слух»)

- `DayZ Projects/scripts/3_game/noise.c`
- `DayZ Projects/scripts/3_game/global/game.c` (стр. 737, 1172)
- `DayZ Projects/scripts/3_game/dayzgame.c` (стр. 3470–3490, 3614–3703)
- `DayZ Projects/scripts/3_game/dayzplayer.c` (стр. 366–401, 516–538)
- `DayZ Projects/scripts/3_game/dayzanimevents.c` (стр. 134–273)
- `DayZ Projects/scripts/4_world/static/sensesaievaluate.c`
- `DayZ Projects/scripts/4_world/entities/dayzplayerimplement.c` (стр. 3204–3345, 3567–3571)
- `DayZ Projects/scripts/4_world/entities/creatures/infected/zombiebase.c` (стр. 550–594)
- `DayZ Projects/scripts/4_world/entities/firearms/fsm/states/weaponfire.c` (стр. 44–71)
- `DayZ Projects/scripts/4_world/entities/core/inherited/weapon.c` (стр. 58)
- `DayZ Projects/scripts/4_world/entities/itembase.c` (стр. 1194–1210)
- `DayZ Projects/scripts/4_world/classes/useractionscomponent/actions/interact/actionopendoors.c` + `actionclosedoors.c`
- `DayZ-Expansion-Scripts/DayZExpansion/AI/Scripts/3_Game/DayZExpansion_AI/eAINoiseSystem.c`
- `DayZ-Expansion-Scripts/DayZExpansion/AI/Scripts/4_World/DayZExpansion_AI/Entities/AI/eAIBase.c` (стр. 563, 1427, 4105–4298)
