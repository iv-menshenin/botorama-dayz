# Research: освещённость для зрения ботов (lighting)

Статус: исследование ванильного API (`DayZ Projects/scripts`). Цель — дать серверному
коду бота (`dmVision`) непрерывный фактор освещённости в точке: ночь (луна) + бункер
(underground) + песчаная буря. Буря уже разобрана в `docs/research/sandstorm.md`
(`GetSandstorm().IsActive()` + `GetIntensity(worldPos)` / `GetIntensityForPlayer`).

---

## 1. Ночь — нативы `World`

`3_game/global/world.c`:

| Сигнатура | Строка | Возвращает |
|---|---|---|
| `proto native bool IsNight()` | 56 | bool «сейчас ночь» |
| `proto native float GetMoonIntensity()` | 54 | float (семантика не подтверждена) |
| `proto native float GetSunOrMoon()` | 55 | float (семантика не подтверждена) |
| `proto native float GetLatitude()` | 52 | float, широта карты |
| `proto native float GetLongitude()` | 53 | float, долгота карты |
| `proto void GetDate(out int year, out int month, out int day, out int hour, out int minute)` | 33 | месяц 1..12, день 1..31, час 0..23, мин 0..59 |
| `proto native void SetDate(int y, int m, int d, int h, int min)` | 51 | форсировать время (для теста) |
| `proto native float GetEyeAccom()` / `SetEyeAccom(float)` | 58/59 | **клиентская** адаптация зрачка (не источник света) |

Доступ: `g_Game.GetWorld().IsNight()` и т.д.

**Что подтверждено:** `IsNight()` — единственный из этих нативов, реально используемый в
ванили (`fireplacebase.c:1916`, `roadflare.c:205`, `undergroundhandlerclient.c:394` — везде
как bool-условие). Используется и Expansion AI (`eAI_ProcessLights`, `Entities/PlayerBase.c:99`).

**Что НЕ подтверждено (все три — чистые native-объявления, в ванильных и Expansion скриптах
не вызываются, grep пуст):** диапазоны и семантика `GetMoonIntensity()`, `GetSunOrMoon()`,
`GetLatitude()/GetLongitude()`.

Гипотезы по именам:
- `GetMoonIntensity()` — предположительно 0..1, «яркость лунного света» (зависит от фазы
  луны и/или облачности); ночной диапазон от безлуния (~0) к полнолунию (~1). **Не подтверждено.**
- `GetSunOrMoon()` — предположительно «интенсивность светила» (солнце днём, луна ночью) либо
  высота светила над горизонтом. Диапазон неизвестен. **Не подтверждено.**
- `GetLatitude()/GetLongitude()` — широта/долгота карты в градусах (для нативного расчёта
  восхода/заката и позиции солнца). В скриптах не читается — только нативные вычисления.

### ✅ ПОДТВЕРЖДЕНО эмпирикой (E2E, атом А1, `lightdump` 2015-4-8)

Полдень (12:01) vs полночь (00:01):

| Натив | Полдень | Полночь | Вывод |
|---|---|---|---|
| `IsNight()` | false | true | бинарный ✅ |
| `GetMoonIntensity()` | 0 | **0.674913** | реальный сигнал фазы (почти полная луна ~4 дня после полнолуния 4.4) |
| `GetSunOrMoon()` | **1** | **0** | **солнечный** фактор (1 день / 0 ночь), НЕ луна |
| `GetLatitude()` | -0.610865 | -0.610865 | **радианы**, не градусы |
| `GetLongitude()` | 1.09607 | 1.09607 | радианы |

**Гипотеза «`GetSunOrMoon` = светило (солнце днём/луна ночью)» — ОПРОВЕРГНУТА**: фактически
это чисто солнечный фактор (1 днём, 0 ночью), для ночного лунного буста бесполезен, а как
дневной фактор дублирует наш `daylightFactor` (от `GetApproxSunriseTime/SunsetTime`).

**Для лунной ночной базы использовать `GetMoonIntensity()`**: диапазон [0,1], 0 днём, положительное
значение ночью по фазе луны. Маппинг 20→30 м (`lerp(DM_VIS_DIST_NIGHT, DM_VIS_DIST_MOON, moon)`) —
атом А1b.

**Ограничение калибровки**: `settime`-ручка фиксирует дату (`DM_E2E_SETTIME_* = 2015/4/8`), поэтому
свип фаз «новолуние→полнолуние» через e2e-мост недоступен без новой ручки `setdate` (год/месяц/день)
— gap, не блокер (сам факт «луна маппится» подтверждён двумя сэмплами).

---

## 2. Underground (бункеры) — данные на сервере

Файл `3_game/undergroundarealoader.c`:

```c
class JsonUndergroundTriggers {                    // строка 1
    ref array<ref JsonUndergroundAreaTriggerData> Triggers;
    static void SpawnParentedTriggers(EntityAI parent);   // CustomSpawn-триггеры у объекта
}

class JsonUndergroundAreaTriggerData {             // строка 68
    vector GetPosition()    { return Vector(Position[0..2]); }    // строка 70
    vector GetOrientation() { return Vector(Orientation[0..2]); } // строка 75 — углы
    vector GetSize()        { return Vector(Size[0..2]); }        // строка 79 — полные размеры
    bool   CustomSpawn;                              // строка 84
    string Tag;                                      // строка 85
    ref TIntArray ParentNetworkId;                   // строка 86
    ref array<float> Position;                       // строка 88
    ref array<float> Orientation;                    // строка 89
    ref array<float> Size;                           // строка 90
    float  EyeAccommodation;                         // строка 91 — 1.0=светло, <1.0=темнее
    float  InterpolationSpeed;                       // строка 92
    ref array<ref JsonUndergroundAreaBreadcrumb> Breadcrumbs; // строка 97
}

class JsonUndergroundAreaBreadcrumb {               // строка 36
    vector GetPosition() { return Vector(Position[0..2]); }
    ref array<float> Position;
    float EyeAccommodation;
    bool  UseRaycast;
    float Radius;
    bool  LightLerp;
    ref BreadcrumbExternalValueController ExternalValueController;
}

class UndergroundAreaLoader {                        // строка 102
    private static string m_Path = "$mission:cfgundergroundtriggers.json"; // 104
    static ref JsonUndergroundTriggers m_JsonData;   // строка 106 — СТАТИК
    static JsonUndergroundTriggers GetData();        // строка 108 — читает файл (fallback dz/worlds/<world>/ce/)
    static void SpawnAllTriggerCarriers();           // строка 134 — m_JsonData = GetData() + спавн carrier'ов
    static void SyncDataSend(PlayerIdentity);        // RPC клиентам (RPC_UNDERGROUND_SYNC)
    static void OnRPC(ParamsReadContext);            // приём на клиенте
}
```

### Ключевой вопрос: доступно ли на сервере — ДА (данные), НО НЕ детекция

1. **Данные загружаются на сервере**: `UndergroundAreaLoader.SpawnAllTriggerCarriers()`
   вызывается в `MissionServer.OnInit()` (`5_mission/mission/missionserver.c:89`), что
   присваивает статику `m_JsonData = GetData()`. Значит серверный код бота может читать
   **полный список триггеров** `UndergroundAreaLoader.m_JsonData.Triggers` (все триггеры,
   включая `CustomSpawn` — те не спавнят carrier, но остаются в массиве).

2. **Детекция «кто внутри» — ТОЛЬКО клиент**: `UndergroundTriggerCarrier.OnVariablesSynchronized`
   (`4_world/entities/scriptedentities/triggers/undergroundtrigger.c`) спавнит `UndergroundTrigger`
   с `ECE_LOCAL` (клиент-only). Сам `UndergroundTrigger : ManTrigger` ставит
   `SetExtents(±data.GetSize()*0.5)` и `m_Accommodation = data.EyeAccommodation`
   (`undergroundtrigger.c::Init`), а enter/leave обрабатывает `UndergroundHandlerClient`
   (клиент), записывая `PlayerBase.m_UndergroundPresence` (`playerbase.c:75`).

3. **`m_UndergroundPresence` НЕ реплицируется на сервер** — поле клиентское, пишется только
   из `UndergroundHandlerClient.SetUndergroundPresence` (`undergroundhandlerclient.c:482` →
   `playerbase.c:2664 SetUnderground`). Метода `GetUndergroundPresence()` в этом дампе **нет**
   (в sandstorm.md он цитируется из дампа 1.30, здесь отсутствует). Вывод: **на сервере нет
   готового «бот под землёй» — надо считать самим.**

### Рецепт: point-in-OBB на сервере

```c
JsonUndergroundTriggers data = UndergroundAreaLoader.m_JsonData; // статик, заполнен на сервере
if (data && data.Triggers)
{
    foreach (JsonUndergroundAreaTriggerData t : data.Triggers)
    {
        vector pos = t.GetPosition();          // центр бокса
        vector size = t.GetSize();             // полные размеры (X/Y/Z), extents = size*0.5
        vector ori = t.GetOrientation();       // Эйлер (yaw,pitch,roll) в ГРАДУСАХ
        float eyeAccom = t.EyeAccommodation;   // 1.0=светло, <1.0=темнее (цель адаптации глаза)
        // 1) d = точка - pos
        // 2) d_local = inverse-rotate(d, ori)   // бокс может быть повёрнут
        // 3) |d_local[i]| <= size[i]*0.5  → точка внутри
    }
}
```

**Orientation — вектор углов в градусах.** Подтверждено контрактом `Object.SetOrientation/
GetOrientation` (`3_game/entities/object.c:311/317`: «yaw, pitch, roll in degrees»,
x=yaw, y=pitch, z=roll). `UndergroundTriggerCarrier` применяет её как `carrier.SetOrientation(
data.GetOrientation())`. Значит бокс в общем случае повёрнут → перед AABB-тестом нужен
**inverse-rotate точки** в локальное пространство триггера.

Для поворота — `Math3D` (`1_core/proto/enmath3d.c`):
- `Math3D.YawPitchRollMatrix(vector ang, out vector mat[3])` — строка 133;
- `Math3D.MatrixInverse3(vector mat[3])` — строка 230 (или `MatrixInvMultiply3`, строка 218).
  Дальше локальная точка = `invMat · (point − pos)` (умножение матрицы на вектор — руками, в
  Math3D нет готового `Matrix*Vector`).

**EyeAccommodation** — целевое значение адаптации глаза, `∈ [0,1]`, `1.0` = полный свет
(поверхность), меньше = темнее. Классификация в `undergroundtrigger.c::Init`: если есть
breadcrumbs → `TRANSITIONING` (гладкий градиент вдоль пути); иначе `EyeAccommodation==1.0` →
`OUTER`, иначе → `INNER`. `UndergroundHandlerClient` ставит `m_EyeAccoTarget =
bestTrigger.m_Accommodation` (с учётом breadcrumbs — плавный переход). Для бота достаточно
читать статичное `EyeAccommodation` триггера (без lerp/breadcrumbs) как «целевой уровень
темноты»; если нужен плавный вход — учитывать breadcrumbs (`UseLinePointFade`/`LightLerp`)
аналогично `undergroundhandlerclient.c` (стр. 120–280).

### [нужно подтвердить] — эмпирический прогон

1. **Доступен ли `UndergroundAreaLoader.m_JsonData` на выделенном сервере и сколько там
   триггеров** (на Chernarus/Livonia/Sakhal). Ручка: probe-команда `/ugdump` (через
   `dmE2EBridge`), логирующая `m_JsonData != null`, `Triggers.Count()`, и для каждого —
   `Position/Size/Orientation/EyeAccommodation/CustomSpawn`. Ожидание: на картах с бункерами
   (Livonia `Land_WarheadStorage_*`) — ненулевой список; на карте без бункеров — null/пусто.
2. **Конвенция `YawPitchRollMatrix`** (строки vs столбцы) для inverse-rotate: проверить на
   заведомо повёрнутом триггере — точка в центре бокса должна остаться в центре после
   transform; точка на оси +X локального бокса должна дать `local≈(size.x*0.5, 0, 0)`.
3. **Единицы `Orientation`** (градусы — по контракту `SetOrientation`, но в JSON триггеров
   может быть иначе): проверить фактическими значениями `/ugdump` (у большинства триггеров
   ожидаем `0 0 0`, у наклонных — «красивые» 90/180/270).

---

## 3. Восход/закат → непрерывный `daylightFactor`

`3_game/worlddata.c` (класс `WorldData`, доступ `g_Game.GetMission().GetWorldData()`):

```c
float GetApproxSunriseTime(float monthday)   // строка 89 — возвращает ЧАСЫ (float, 0..24)
float GetApproxSunsetTime(float monthday)    // строка 96
int   GetDaytime()                           // строка 104 → WorldDataDaytime enum
```

- Константы (поля `protected`, переопределяются миром): `m_Sunrise_Jan=8.54`, `m_Sunset_Jan=15.52`,
  `m_Sunrise_Jul=3.26`, `m_Sunset_Jul=20.73` (строки 24–27, 67–70).
- Формула — **кусочно-линейная годовая аппроксимация** от `monthday` (дробный месяц):
  при `monthday ≤ 8` lerp `Jan→Jul` (минимум в `monthday=8`), при `> 8` — `Jul→Jan`
  (симметрично назад к `monthday=13`). Возвращает час восхода/заката.
- `GetDaytime()` (строка 104): `DAWN = [sunrise, sunrise+2)`, `DAY = [sunrise+2, sunset)`,
  `DUSK = [sunset, sunset+2)`, иначе `NIGHT`. Вызывает `GetApproxSunriseTime(month)` с
  **только месяцем (int)**.
- `class WorldDataDaytime` (строка 463): `ANY=-1, NIGHT=0, DAY=1, DUSK=2, DAWN=3`.

**Важно:** `GetApproxSunriseTime(month)` принимает месяц `1..12` (не день) — в `GetDaytime`
и в эталоне Expansion передаётся int-месяц, поэтому фактор не меняется внутри месяца (только
рампом по часу/минуте). Это приемлемо для ботов.

### Эталон Expansion `environment.c::Expansion_GetDaylightVisibility`

`DayZExpansion/AI/Scripts/4_World/DayZExpansion_AI/Classes/environment.c`:

```c
float Expansion_GetDaylightVisibility() {                       // строка 12
    int year, month, day, hour, minute;
    g_Game.GetWorld().GetDate(year, month, day, hour, minute);
    return Expansion_GetDaylightVisibilityEx(month, hour, minute);
}
float Expansion_GetDaylightVisibilityEx(int month, int hour, int minute) { // строка 20
    float sr = m_WorldData.GetApproxSunriseTime(month);
    float ss = m_WorldData.GetApproxSunsetTime(month);
    float time[6] = { max(sr-0.5,0), sr, sr+0.5, ss-0.5, ss, min(ss+1.0, 23+59/60.0) };
    float out[6] = { 0, 0.5, 1, 1, 0.75, 0 };                    // поля класса, строка 1–8
    return ExpansionMath.LookUp(hour + minute/60.0, 6, time, out);
}
```

Итог: `daylightFactor` — кусочно-линейный рамп: 0→0.5→1 за 0.5 ч на рассвете, 1 днём,
1→0.75→0 за 1 ч на закате (асимметрия намеренная). Это и есть готовая модель
`daylightFactor ∈ [0,1]`, которую портировать (вместо bool `IsNight()`).

---

## 4. Кости

Подтверждённые имена (ваниль):

| Кость | Источник | Назначение |
|---|---|---|
| `"Head"` | `bleedingsourcesmanagerbase.c:24`; `playerbase.c:2273,6788`; `zombiebase.c` (голова) | голова |
| `"Spine3"` | `bleedingsourcesmanagerbase.c:31`; `aitargetcallbacksplayer.c:11`; `playerbase.c:9532`; `zombiebase.c:1044` | **грудь** (vision/target point) |
| `"LeftShoulder"` | `bleedingsourcesmanagerbase.c:33` | левое плечо |
| `"RightShoulder"` | `bleedingsourcesmanagerbase.c:38` | правое плечо |
| доп. зоны | `bleedingsourcesmanagerbase.c:25–43` | `Neck`, `Pelvis`, `Spine`, `Spine1`, `Spine2`, `LeftArm*`, `RightArm*`, … |

`aitargetcallbacksplayer.c:11` — канонический пример: `m_iChestBoneIndex =
m_Player.GetBoneIndexByName("Spine3")` (грудь = точка прицеливания для неalerted заражённых,
`aitargetcallbacksplayer.c:38`), и `GetHeadPositionWS()` через
`MiscGameplayFunctions.GetHeadBonePos(m_Player, position)`.

Сигнатуры:

- `proto native int GetBoneIndexByName(string pBoneName)` → int (**-1 = нет**):
  - `Human` — `3_game/human.c:1387` (игроки/ИИ-выжившие).
  - `DayZCreature` — `3_game/entities/dayzanimal.c:53` (зомби/животные: `ZombieBase extends
    DayZInfected extends DayZCreature`; `DayZAnimal extends DayZCreatureAI extends DayZCreature`).
  - Перед вызовом кастовать: `Human.Cast(entity)` / `DayZCreature.Cast(entity)`.
- `proto native vector GetBonePositionWS(int pivot)` — `3_game/entities/object.c:244` (на `Object`, без каста).
- `proto native void GetBoneTransformWS(int pivot, out vector transform[4])` — `object.c:254`;
  **forward = `transform[1]`** (костное пространство, см. `docs/research/perception.md`).

---

## Открытые вопросы

1. **[нужно подтвердить]** Диапазон/семантика `GetMoonIntensity()` и `GetSunOrMoon()`
   (см. §1) — без этого нельзя корректно построить ночной moonlight-фактор.
2. **[нужно подтвердить]** `IsNight()` — порог (по какому часу/высоте солнца он возвращает
   `true`), чтобы решить: полагаться на него или строить фактор от `GetApproxSunriseTime`.
3. **[нужно проверить]** Наличие/наполнение `UndergroundAreaLoader.m_JsonData` на целевых
   картах и конвенция `YawPitchRollMatrix` для inverse-rotate (§2).
4. **Песчаная буря** — на текущих картах не возникает естественно (`docs/research/sandstorm.md`);
   учитывать как дополнительный множитель только если сервер форсирует бурю.

## Источники

- `3_game/global/world.c` (стр. 33, 51–59)
- `3_game/undergroundarealoader.c` (стр. 1–170)
- `3_game/triggercarrierbase.c`
- `4_world/entities/scriptedentities/triggers/undergroundtrigger.c`
- `4_world/classes/undergroundhandlerclient.c` (стр. 1–12, 120–280, 360–490)
- `4_world/entities/manbase/playerbase.c` (стр. 75, 2664)
- `5_mission/mission/missionserver.c` (стр. 89)
- `3_game/worlddata.c` (стр. 24–27, 67–70, 89–126, 463–469)
- `3_game/entities/object.c` (стр. 244, 254, 311, 317)
- `3_game/human.c` (стр. 1387); `3_game/entities/dayzanimal.c` (стр. 24, 53)
- `1_core/proto/enmath3d.c` (стр. 133, 218, 230)
- `4_world/classes/bleedingsources/bleedingsourcesmanagerbase.c` (стр. 24–43)
- `4_world/entities/manbase/playerbase/aitargetcallbacksplayer.c` (стр. 1–40)
- `4_world/entities/creatures/infected/zombiebase.c` (стр. 1044)
- Expansion эталон: `DayZExpansion/AI/Scripts/4_World/DayZExpansion_AI/Classes/environment.c`
  (`Expansion_GetDaylightVisibility`); `.../Entities/PlayerBase.c` (`eAI_ProcessLights`).

---

## Существенные развилки / принятые решения (для docs/decisions.md)

1. **Underground-детекцию считать самим (point-in-OBB по `m_JsonData`), а не полагаться на
   ванильный триггер/`m_UndergroundPresence`.** Почему: ванильный `UndergroundTrigger`
   спавнится `ECE_LOCAL` и `m_UndergroundPresence` пишется только клиентом — на сервере
   готового флага нет; но полный список триггеров (позиция/размер/поворот/EyeAccommodation)
   на сервере доступен через статик `UndergroundAreaLoader.m_JsonData` (заполнен в
   `MissionServer.OnInit`). Бот серверный → только свой тест точки-в-боксе.
2. **`daylightFactor` строить от `GetApproxSunriseTime/SunsetTime` + линейный рамп (эталон
   `Expansion_GetDaylightVisibility`), а не от бинарного `IsNight()`.** Почему: `IsNight()` —
   только bool, а для зрения нужен непрерывный 0..1 (рассвет/закат плавно меняют видимость).
   Гранулярность — месяц (int), чего достаточно.
3. **Лунный фактор отложить до эмпирики `GetMoonIntensity/GetSunOrMoon`.** Почему: оба —
   чистые native без использования в скриптах, диапазон/семантика неизвестны; строить модель
   на них до подтверждения рискованно. Пока ночью использовать `daylightFactor≈0` + (если
   подтвердится) лунную добавку.
