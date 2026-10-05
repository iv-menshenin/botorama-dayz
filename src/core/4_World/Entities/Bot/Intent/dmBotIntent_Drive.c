//! dmBotIntent_Drive — бот садится за руль и едет по маршруту из точек.
//!
//! Наследует dmBotIntent_GetInVehicle и переиспользует весь lifecycle посадки
//! (walk к двери → открыть дверь → GetInVehicle → сел). После посадки переводит
//! канал на DRIVE и ведёт машину нативным приводом: газ по отклонению от целевой
//! скорости (dm_DriveThrottle через SetThrottle в CarScript.OnInput) + передачи
//! ShiftTo (вперёд по скорости) + нативный руль SetSteering (через
//! dm_DriveSteering) — поворот делает нативный руль, как у реальной машины.
//! Маршрут — инкрементальный: интент доливает точки чанками из dmRoadRouter
//! (RefillRoute → NextChunk) в хвост очереди; предзаполненный владельцем маршрут
//! (E2E-мост) долива не требует.
//! Вождение — фазовая машина m_DrivingPhase (DPH_*): STARTING (запуск мотора +
//! прогрев) → DRIVING (едем к waypoint'у, лучи препятствий ВКЛ) → DETOUR (объезд
//! по сотам dmRoadHoneycomb, лучи ВЫКЛ) → DRIVING. Достижение waypoint'а — по
//! проекции носа на ломаную маршрута (m_RouteCumDist), а не по 3D-дистанции.
//! Объезд — только соты (коробка-объезд удалена). Застревание детектится по
//! прогрессу дистанции (TickStuck) с реверсом. Финиш — только когда маршрут
//! исчерпан И машина достигла последней точки. Graceful-завершение глушит
//! двигатель (StopCar) и высаживает бота штатным выходом.
class dmBotIntent_Drive : dmBotIntent_GetInVehicle
{
	//! Руль: мёртвая зона угла (рад), ниже — не рулим (анти-джиттер).
	static const float DRIVE_STEER_ANGLE_DEADZONE = 0.01;

	//! Локальный объезд: дистанция детекта препятствий вперёд (м). 70 м (не 50)
	//! компенсирует квантование по вейпоинтам (~14 м шаг): препятствие входит в
	//! бюджет детекта только когда m_DriveRouteIdx подходит к нему вплотную —
	//! реально детект срабатывал на ~28–40 м, а не на 50.
	static const float DM_DRIVE_DETECT_DISTANCE = 70.0;
	//! Локальный объезд: интервал детекта препятствий (с).
	static const float DM_DRIVE_DETECT_INTERVAL = 0.3;
	//! Локальный объезд: радиус широкого райкаста препятствия (м, ~пол-ширины машины).
	static const float DM_DRIVE_OBSTACLE_RAY_RADIUS = 1.5;
	//! Локальный объезд: старт луча впереди бампера (м) — выносим точку старта за
	//! коллайдер машины, иначе луч стартует внутри коллайдера и самопопадает на t=0.
	static const float DM_DRIVE_DETECT_START_OFFSET = 4.0;
	//! Опорная точка цели/достижения — НОС автомобиля, не центр: вынос ориджина
	//! вперёд по курсу на пол-длины седана (ориджин → передний бампер, подбирается).
	//! Водитель смотрит на нос — целиться носом в точку маршрута проще, чем центром,
	//! а вынос корпуса не искажает объезд.
	static const float DM_DRIVE_NOSE_OFFSET = 2.0;
	//! Соты (грид-объезд): вынос стартовой точки территории вперёд от центра
	//! машины (м). Нос (DM_DRIVE_NOSE_OFFSET = 2.0 м, передний бампер) + радиус
	//! вертикальной капсулы клетки сот (DM_GRID_RAY_RADIUS = 1.5 м) = 3.5: капсула
	//! нулевого ряда стартует ЗА бампером, кузов/капот остаются позади территории
	//! и не красятся RED (иначе m_SeenRed ставится рано и грид гаснет до барьера).
	static const float DM_DRIVE_HONEY_ORIGIN_OFFSET = 3.5;
	//! Соты: бюджет CPU на один кадр для асинхронной раскраски/маркировки (мс).
	//! Рейкасты сот дёшевы (~1.4 мкс/под-шаг), но троттлинг «1 под-шаг/тик» даёт ~9.4 мин
	//! на 110 рядов — бюджетный цикл сжимает до долей секунды без заметного спайка кадра.
	static const float DM_DRIVE_GRID_BUDGET_MS = 3.0;
	//! Локальный объезд: высота нижнего детект-луча над землёй (м). Верхний луч на
	//! высоте кузова перелетает низкие заграждения (бордюры, низкие бетонные барьеры
	//! ~0.5–1 м) и ловит их только вплотную — нижний луч у земли цепляет их на полной
	//! дистанции детекта.
	static const float DM_DRIVE_DETECT_GROUND_RAY_HEIGHT = 0.3;
	//! Локальный объезд: высота верхнего детект-луча над землёй (м) — реальная высота
	//! кузова машины (carPos[1] = origin у земли, не высота кузова). Верхний луч ловит
	//! высокие препятствия (деревья, стены), нижний (GROUND_RAY_HEIGHT) — низкие
	//! заграждения: два луча реально разнесены по высоте.
	static const float DM_DRIVE_DETECT_BODY_OFFSET = 1.0;
	//! Локальный объезд: порог нормали поверхности для отличия земли от препятствия
	//! при obj == null (террейн/вода/статическая коллизия без script-объекта приходят
	//! одинаково без obj). У земли нормаль вверх (dir[1]≈1), у барьера/камня/насыпи —
	//! горизонтально (dir[1]≈0). 0.7 = cos 45°: нормаль с dir[1] > 0.7 считаем «вверх»
	//! (земля, пропускаем), dir[1] <= 0.7 — препятствие.
	static const float DM_DRIVE_GROUND_NORMAL_EPS = 0.7;
	//! Локальный объезд: сила тормоза при превышении лимита скорости (0..1). Реальный
	//! тормоз (SetBrake в CarScript.OnInput), а не сброс газа: на 47 км/ч машина без
	//! тормоза катится ещё десятки метров и не успевает замедлиться до барьера.
	//! 0.5, а не 1.0: полный тормоз (1.0) на ручной КПП на высокой передаче глушит
	//! мотор (столл). Умеренный тормоз замедляет достаточно, но не блокирует колёса
	//! наглухо (полный тормоз — только в нейтрали, в TickHoneycomb).
	static const float DM_DRIVE_BRAKE_STRENGTH = 0.5;

	//! Локальный объезд: минимальная скорость для детекта (км/ч). На старте машина
	//! ещё не выровнялась на оси дороги и едва едет — детект от невыровненной позиции
	//! даёт ложное препятствие. Не детектим, пока не поедем.
	static const float DM_DRIVE_DETECT_MIN_SPEED = 5.0;

	//! Фазовая машина вождения (m_DrivingPhase): детерминированные переходы
	//! STARTING → DRIVING → DETOUR → DRIVING.
	static const int DPH_STARTING = 0;   // заводим мотор + прогрев
	static const int DPH_DRIVING  = 1;   // едем к waypoint'у (лучи препятствий ВКЛ)
	static const int DPH_DETOUR   = 2;   // объезд по сотам (лучи препятствий ВЫКЛ)

	//! Достижение waypoint'а по проекции носа на маршрут: число сегментов ломаной,
	//! сканируемых вперёд от m_DriveRouteIdx при поиске ближайшего сегмента.
	static const int DM_DRIVE_PROGRESS_SEGMENTS = 8;

	//! K-разворот: фазы стейт-машины реверс-разворота (NONE → REVERSING → FORWARD).
	static const int K_TURN_NONE = 0;
	static const int K_TURN_REVERSING = 1;
	static const int K_TURN_FORWARD = 2;

	//! K-разворот: угол (град) между курсом машины и маршрутом, выше которого при
	//! низкой скорости машина боком не может довернуться нативным рулём — запускаем
	//! реверс-разворот (триггер).
	static const float DM_DRIVE_REVERSE_TURN_ANGLE = 50.0;
	//! K-разворот: гистерезис (град) — выход из реверса, когда |angle| упал ниже
	//! ANGLE - MARGIN (40°). Держит фазу реверса, иначе флап реверс↔передний ход.
	static const float DM_DRIVE_REVERSE_TURN_MARGIN = 10.0;
	//! K-разворот: угол (град) выхода из FORWARD в NONE (нос почти на маршруте).
	static const float DM_DRIVE_REVERSE_TURN_DONE_ANGLE = 15.0;
	//! K-разворот: макс. скорость (км/ч) для входа в манёвр (ниже — можно) и целевая
	//! скорость в самом манёвре (медленный разворот без разгона).
	static const float DM_DRIVE_REVERSE_TURN_MAX_SPEED = 8.0;
	//! K-разворот: таймаут (с) фазы манёвра — страховка от залипания в фазе.
	static const float DM_DRIVE_REVERSE_TURN_TIMEOUT = 8.0;

	//! Застревание (TickStuck): фиксированный боковой выворот руля в реверсе
	//! (единицы руля, ~25° = 0.28/1.57 рад), чтобы зад уходил вбок ОТ маршрута и
	//! машина не возвращалась в ту же точку при следующем вперёд. Применяется только
	//! в реверсе от застревания (m_Reverse при m_TurnState == NONE); у K-разворота
	//! свой руль (ApplySteering в фазах REVERSING/FORWARD).
	static const float DM_DRIVE_STUCK_REVERSE_STEER = 0.28;

	//! Рефилл маршрута: число перегонов за чанк NextChunk («текущий + следующий»).
	static const int DM_DRIVE_LOOKAHEAD = 2;
	//! Рефилл: дозапрашиваем чанк, когда до конца очереди осталось ≤ N точек.
	static const int DM_DRIVE_REFILL_MARGIN = 5;

	//! Уже за рулём (выставляет команда, случай 1): OnStart не идёт к двери и не
	//! играет get-in, а сразу переходит в PHASE_SEATED.
	bool m_AlreadySeated = false;

	//! Маршрут исчерпан (NextChunk вернул false): финиш — только по достижении
	//! последней точки. Для предзаполненного владельцем списка ставится сразу true.
	bool m_RouteExhausted = false;

	//! Маршрут из точек (заполняется инкрементально через RefillRoute; старый
	//! владелец мог предзаполнить его целиком до OnStart).
	ref array<vector> m_DriveRoute;

	//! Индекс текущей точки маршрута.
	int m_DriveRouteIdx = 0;

	//! Конечная точка назначения маршрута. Для маршрута из роутера берётся из
	//! dmRoadRouter.GetTargetPos() в OnStart; для предзаполненного — последняя
	//! точка m_DriveRoute. Нужна, чтобы после обходного пути сот ре-снэпнуть
	//! роутер на продолжение маршрута до цели (иначе InsertHoneycombPath обрезает
	//! целевую точку и условие финиша срабатывает на хвосте обходного пути).
	vector m_TargetPos;

	//! Машина (получаем из m_Transport после посадки).
	CarScript m_Car;

	//! Флаг, что EngineStart() уже вызван.
	bool m_EngineStarted = false;

	//! Секунд с момента посадки (до запуска двигателя).
	float m_SeatedFor = 0.0;

	//! Секунд после запуска двигателя (прогрев до трогания).
	float m_WarmupFor = 0.0;

	//! Текущий лимит скорости (км/ч, сглаживается).
	float m_SpeedLimit = DM_DRIVE_MAX_SPEED_STRAIGHT;

	//! Застревание: счётчик тиков, после порога — реверс.
	int m_StuckCounter = 0;
	bool m_Reverse = false;

	//! Прогресс по дистанции до текущего вейпоинта: последняя дистанция (м),
	//! -1.0 = «не измерена» (первый тик / сброс при смене вейпоинта/курса).
	float m_LastWaypointDist = -1.0;

	//! Длительность текущего реверса (тиков) — ограничивает залипание реверса.
	int m_ReverseTicks = 0;

	//! Текущее значение руля (пишется в m_Car.dm_DriveSteering), сглаживается.
	float m_WheelSteer = 0.0;

	//! K-разворот: фаза реверс-разворота (K_TURN_*). NONE — обычное вождение.
	int m_TurnState = K_TURN_NONE;

	//! K-разворот: секунд в текущей фазе манёвра (таймаут-страховка).
	float m_TurnTimer = 0.0;

	//! Последнее время драйв-лога (троттлинг ~2 c).
	float m_LastDriveLogTime = 0.0;

	//! Последнее время лога «водитель/фаза посадки» (троттлинг ~2 c) — отдельный
	//! таймер, чтобы не глушить драйв-телеметрию в конце TickDriving.
	float m_LastSeatLogTime = 0.0;

	//! Локальный объезд: таймер троттлинга детекта (с).
	float m_DetectTimer = 0.0;

	//! Фазовая машина вождения (DPH_*): STARTING → DRIVING → DETOUR → DRIVING.
	//! Посадка (родитель GetInVehicle до PHASE_SEATED) — вне этой машины: DPH_STARTING
	//! стартует уже после посадки, в первом тике TickDriving.
	int m_DrivingPhase = DPH_STARTING;

	//! Кумулятивные длины вдоль m_DriveRoute: m_RouteCumDist[i] = путь по ломаной от
	//! точки 0 до точки i. Пересчитывается при каждом изменении m_DriveRoute
	//! (RefillRoute / InsertHoneycombPath).
	ref array<float> m_RouteCumDist;

	//! Индекс последнего waypoint'а объезда сот в m_DriveRoute (предпоследняя точка
	//! вставленного пути, перед точкой возврата на маршрут). Когда m_DriveRouteIdx
	//! проезжает его по проекции — DETOUR → DRIVING (лучи снова ВКЛ).
	int m_DetourLastIdx = 0;

	//! Соты (грид-объезд): 0 = не активны, 1 = считают маршрут объезда (асинхронно).
	//! Включается при детекте препятствия (TickDetect) и в тест-режиме immediate-grid.
	int m_HoneyState = 0;

	//! Тест-режим: сразу строить сетку объезда при старте вождения (без детекта).
	bool m_ImmediateGrid = false;

	void dmBotIntent_Drive()
	{
		//! Посадка = walk → канал MOVE (как у GetInVehicle); в фазе вождения
		//! m_Manage станет DRIVE (см. OnUpdate).
		m_Manage = dmBotIntentsChannel.MOVE;
		m_Concurrency = dmBotIntentConcurrency.EXCLUSIVE;
		m_Priority = dmBotIntentPriority.CRITICAL;
	}

	override string GetIntentName()
	{
		return "Drive";
	}

	override void OnStart(dmAISurvivor bot)
	{
		//! super.OnStart наследует точку входа у двери (m_Goal) и walk к ней;
		//! для m_AlreadySeated эта фаза не запускается (сразу PHASE_SEATED ниже).
		super.OnStart(bot);

		//! Маршрут либо предзаполнен владельцем (E2E-мост RunDrive: список уже весь →
		//! рефилл не нужен), либо пуст/null → доливается чанками из dmRoadRouter.
		if (m_DriveRoute && m_DriveRoute.Count() > 0)
		{
			m_RouteExhausted = true;
			m_TargetPos = m_DriveRoute[m_DriveRoute.Count() - 1];
		}
		else
		{
			if (!m_DriveRoute)
				m_DriveRoute = new array<vector>();
			m_RouteExhausted = false;
			m_TargetPos = dmRoadRouter.Get().GetTargetPos();
		}

		m_DriveRouteIdx = 0;
		m_Car = null;
		m_EngineStarted = false;
		m_SeatedFor = 0.0;
		m_WarmupFor = 0.0;
		m_SpeedLimit = DM_DRIVE_MAX_SPEED_STRAIGHT;
		m_StuckCounter = 0;
		m_Reverse = false;
		m_LastWaypointDist = -1.0;
		m_ReverseTicks = 0;
		m_WheelSteer = 0.0;
		m_TurnState = K_TURN_NONE;
		m_TurnTimer = 0.0;
		m_LastDriveLogTime = 0.0;
		m_LastSeatLogTime = 0.0;
		m_DetectTimer = 0.0;
		m_DrivingPhase = DPH_STARTING;
		m_DetourLastIdx = 0;
		m_HoneyState = 0;
		RebuildCumDist();

		//! Уже за рулём (команда резолвила машину по случаю 1): не walk к двери и
		//! не get-in — сразу фаза вождения на канале DRIVE.
		if (m_AlreadySeated)
		{
			m_CommandInvoked = true;
			m_Phase = PHASE_SEATED;
			m_Manage = dmBotIntentsChannel.DRIVE;
		}
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Intent.Drive");
		#endif

		//! Сел — ведём машину (канал DRIVE ставится однократно).
		if (m_Phase == PHASE_SEATED)
		{
			if (m_Manage != dmBotIntentsChannel.DRIVE)
				m_Manage = dmBotIntentsChannel.DRIVE;
			TickDriving(bot, pDt);
			return;
		}

		//! Посадка (WALK/OPEN_IN/ENTERING) и выход (OPEN_OUT/EXITING/CLOSING) —
		//! целиком переиспользуем super-логику GetInVehicle.
		super.OnUpdate(bot, pDt);
	}

	//! Один тик вождения: цель → скорость → газ/тормоз (нативный) → передача → руль.
	void TickDriving(dmAISurvivor bot, float pDt)
	{
		m_Car = CarScript.Cast(m_Transport);
		if (!m_Car)
		{
			dmBotLog.Error("Drive: транспорт не машина (CarScript), abort");
			Fail();
			return;
		}

		//! ГЕЙТ: машину нельзя заводить/двигать, пока бот физически не за рулём
		//! (экипаж зарегистрирован И get-in анимация завершена). Иначе интент доезжает
		//! до PHASE_SEATED по таймауту (без фактической посадки) и заводит мотор без
		//! водителя (engine=true, rpm в отсечку, колёса не крутятся).
		HumanCommandVehicle hcv = m_Pawn.GetCommand_Vehicle();
		bool gettingIn = (hcv != null && hcv.IsGettingIn());
		if (m_Car.CrewMember(DayZPlayerConstants.VEHICLESEAT_DRIVER) != m_Pawn || gettingIn)
		{
			#ifdef DM_BOT_DEBUG_CAR
			dmBotLog.Debug("[CAR] Drive: gate blocked — driver not seated (crew=" + (m_Car.CrewMember(DayZPlayerConstants.VEHICLESEAT_DRIVER) != null) + " gettingIn=" + gettingIn + ")");
			#endif
			return;
		}

		#ifdef DM_BOT_DEBUG_CAR
		float seatNow = GetGame().GetTickTime();
		if (seatNow - m_LastSeatLogTime >= 2.0)
		{
			m_LastSeatLogTime = seatNow;
			bool driverSeated = (m_Car.CrewMember(DayZPlayerConstants.VEHICLESEAT_DRIVER) != null);
			dmBotLog.Debug("[CAR] Drive: driver=" + driverSeated + " phase=" + m_Phase);
		}
		#endif

		//! DPH_STARTING: пауза после посадки → запуск мотора → прогрев. Пока фаза не
		//! переключится на DRIVING — газ/руль не трогаем (return на каждом под-шаге).
		if (m_DrivingPhase == DPH_STARTING)
		{
			m_SeatedFor += pDt;
			if (m_SeatedFor < DM_DRIVE_START_DELAY)
				return;

			//! Старт двигателя (однократно; напрямую — ванильный ActionStartEngine
			//! режет серверных ботов).
			if (!m_EngineStarted)
			{
				m_Car.EngineStart();
				m_Car.SetBrake(0.0);
				m_Car.SetHandbrake(0.0);
				m_Car.SetBrakesActivateWithoutDriver(false);
				m_EngineStarted = true;
				#ifdef DM_BOT_DEBUG_CAR
				dmBotLog.Debug("[CAR] Drive: двигатель запущен, прогрев " + DM_DRIVE_ENGINE_WARMUP + " с");
				#endif
				return;
			}

			m_WarmupFor += pDt;
			if (m_WarmupFor < DM_DRIVE_ENGINE_WARMUP)
				return;

			//! Восстановление после заглохания: ручная КПП глохнет при жёстком тормозе
			//! на высокой передаче (rpm=0, engine=false). Перезапускаем мотор и едем.
			if (!m_Car.EngineIsOn())
			{
				m_Car.EngineStart();
				m_Car.SetBrake(0.0);
				m_Car.SetHandbrake(0.0);
				m_Car.SetBrakesActivateWithoutDriver(false);
				#ifdef DM_BOT_DEBUG_CAR
				dmBotLog.Debug("[CAR] Drive: двигатель заглох — перезапуск");
				#endif
			}

			//! Мотор заведён И прогрев закончился → STARTING → DRIVING.
			SetDrivingPhase(DPH_DRIVING);
		}

		//! Рефилл маршрута: доливаем чанк, когда очередь пуста или подходит к концу.
		RefillRoute();
		if (m_DriveRoute.Count() == 0)
		{
			//! Очередь пуста И маршрут исчерпан (NextChunk ничего не дал) — не должно
			//! случаться после успешного Setup, но страховка от пустого списка.
			dmBotLog.Error("Drive: маршрут пуст и исчерпан, abort");
			Fail();
			return;
		}

		vector carPos = m_Car.GetPosition();

		//! DPH_DETOUR + соты ещё считают: жёсткий тормоз + асинхронный расчёт объезда.
		if (m_DrivingPhase == DPH_DETOUR && m_HoneyState == 1)
		{
			TickHoneycomb(carPos, pDt);
			return;
		}

		//! DPH_DRIVING: детект препятствий (лучи ВКЛ) + тест-режим immediate-grid.
		if (m_DrivingPhase == DPH_DRIVING)
		{
			if (m_ImmediateGrid)
			{
				m_ImmediateGrid = false;
				m_Reverse = false;
				vector gridDir = m_Car.GetDirection();
				gridDir[1] = 0.0;
				if (gridDir.Length() < 0.01)
					gridDir = Vector(1.0, 0.0, 0.0);
				else
					gridDir.Normalize();
				vector gridOrigin = carPos + gridDir * DM_DRIVE_HONEY_ORIGIN_OFFSET;
				Human gridDriver = m_Car.CrewMember(DayZPlayerConstants.VEHICLESEAT_DRIVER);
				dmRoadHoneycomb.Get().Start(gridOrigin, gridDir, m_DriveRoute, m_DriveRouteIdx, m_Car, gridDriver, 0.0, 0);
				m_HoneyState = 1;
				SetDrivingPhase(DPH_DETOUR);
				#ifdef DM_BOT_DEBUG_CAR
				dmBotLog.Debug("[CAR] Drive: immediate grid origin=" + gridOrigin);
				#endif
				TickHoneycomb(carPos, pDt);
				return;
			}

			TickDetect(carPos, pDt);
			if (m_DrivingPhase != DPH_DRIVING)
			{
				//! TickDetect поймал препятствие и запустил соты (Start + DPH_DETOUR).
				TickHoneycomb(carPos, pDt);
				return;
			}
		}

		//! Направление машины (горизонталь).
		vector carDirRaw = m_Car.GetDirection();
		vector carDir = carDirRaw;
		carDir[1] = 0.0;
		if (carDir.Length() < 0.01)
			carDir = Vector(1.0, 0.0, 0.0);
		else
			carDir.Normalize();

		//! Опорная точка цели/достижения — НОС автомобиля. carDir уже горизонтален
		//! (carDir[1] == 0), поэтому nose[1] == carPos[1] — Y остаётся от carPos.
		vector nose = carPos + carDir * DM_DRIVE_NOSE_OFFSET;

		vector target = m_DriveRoute[m_DriveRouteIdx];
		vector toTarget = target - nose;
		toTarget[1] = 0.0;
		float dist = toTarget.Length();

		float toLen = toTarget.Length();
		if (toLen < 0.01)
			toLen = 1.0;
		float toDirX = toTarget[0] / toLen;
		float toDirZ = toTarget[2] / toLen;

		//! Угол до вейпоинта (знак через cross-произведение) — один раз на точку.
		float cross = carDir[0] * toDirZ - carDir[2] * toDirX;
		float dot = carDir[0] * toDirX + carDir[2] * toDirZ;
		float angle = Math.Atan2(cross, dot);

		//! Прогресс машины вдоль маршрута — проекция носа на ломаную m_DriveRoute.
		float progress = ComputeProgress(nose);

		//! Промежуточная точка пройдена по ПРОЕКЦИИ (не по 3D-дистанции): проехали её
		//! вдоль дороги дальше REACH. Конечную точку (isLast) по проекции не пропускаем —
		//! до неё доезжаем по DM_DRIVE_REACH (см. финиш ниже).
		bool isLast = (m_DriveRouteIdx >= m_DriveRoute.Count() - 1);
		if (!isLast && progress > m_RouteCumDist[m_DriveRouteIdx] + DM_DRIVE_WAYPOINT_REACH)
		{
			m_DriveRouteIdx = m_DriveRouteIdx + 1;
			//! Сменили точку — дистанция до новой резко прыгнула вверх; сброс
			//! «лучшей» дистанции, иначе TickStuck примет скачок за «нет прогресса».
			m_LastWaypointDist = -1.0;
			target = m_DriveRoute[m_DriveRouteIdx];
			toTarget = target - nose;
			toTarget[1] = 0.0;
			dist = toTarget.Length();

			//! Пересчёт курса к новой точке (одна точка = один набор cross/dot/angle).
			toLen = toTarget.Length();
			if (toLen < 0.01)
				toLen = 1.0;
			toDirX = toTarget[0] / toLen;
			toDirZ = toTarget[2] / toLen;
			cross = carDir[0] * toDirZ - carDir[2] * toDirX;
			dot = carDir[0] * toDirX + carDir[2] * toDirZ;
			angle = Math.Atan2(cross, dot);
		}

		//! Выход из объезда: доехали до последнего waypoint'а сот → DETOUR → DRIVING
		//! (лучи снова ВКЛ). Ре-снэп роутера к цели уже сделан в InsertHoneycombPath.
		if (m_DrivingPhase == DPH_DETOUR && m_DriveRouteIdx > m_DetourLastIdx)
		{
			m_HoneyState = 0;
			SetDrivingPhase(DPH_DRIVING);
		}

		//! Конец маршрута: исчерпан И машина достигла последней точки
		//! (гэп DM_DRIVE_REACH). До исчерпания финиш не наступает — маршрут ещё
		//! доливается чанками.
		if (m_RouteExhausted && vector.Distance(nose, m_DriveRoute[m_DriveRoute.Count() - 1]) < DM_DRIVE_REACH)
		{
			Finish();
			return;
		}

		float speedAbs = m_Car.GetSpeedometerAbsolute();

		//! K-разворот: при большом угле к маршруту на низкой скорости запускаем
		//! реверс-разворот (стейт-машина управляет m_Reverse своим флагом).
		TickKTurn(angle, speedAbs, pDt);

		//! Целевая скорость: ниже в повороте; в манёвре K-разворота — медленный
		//! разворот. Сглаживаем.
		float limit = DM_DRIVE_MAX_SPEED_STRAIGHT;
		if (m_TurnState != K_TURN_NONE)
			limit = DM_DRIVE_REVERSE_TURN_MAX_SPEED;
		else if (Math.AbsFloat(angle) > DM_DRIVE_TURN_ANGLE_THRESHOLD)
			limit = DM_DRIVE_MAX_SPEED_TURNING;
		m_SpeedLimit = Math.Lerp(m_SpeedLimit, limit, DM_DRIVE_SPEED_SMOOTH);

		//! Нативный газ/тормоз (dm_DriveThrottle) + передачи.
		bool pushing = ApplyDriveForce(speedAbs);

		//! Передачи (вперёд по скорости).
		ShiftGear(speedAbs);

		//! Руль (нативный).
		ApplySteering(angle, speedAbs, pDt);

		//! Застревание → реверс. Гейт по K_TURN_NONE: во время K-разворота реверсом
		//! управляет сам манёвр (свой m_Reverse), TickStuck не должен его дёргать.
		if (m_TurnState == K_TURN_NONE)
			TickStuck(dist);

		#ifdef DM_BOT_DEBUG_DRIVE_TELEMETRY
		{
			float headYaw = carDir.VectorToAngles()[0];
			vector toDirV = Vector(toDirX, 0.0, toDirZ);
			float bearYaw = toDirV.VectorToAngles()[0];
			dmBotLog.Debug("[DRIVE-TELE] pos=" + Vector(carPos[0], 0.0, carPos[2]) + " head=" + headYaw + " bear=" + bearYaw);
			dmBotLog.Debug("[DRIVE-TELE] angle=" + angle + " steer=" + m_WheelSteer + " speed=" + speedAbs + " dist=" + dist + " idx=" + m_DriveRouteIdx + " last=" + isLast);
		}
		#endif

		#ifdef DM_BOT_DEBUG_CAR
		float now = GetGame().GetTickTime();
		if (now - m_LastDriveLogTime >= 2.0)
		{
			m_LastDriveLogTime = now;
			int curGear = m_Car.GetCurrentGear();
			float rpm = m_Car.EngineGetRPM();
			dmBotLog.Debug("[CAR] Drive: speed=" + speedAbs + " wp=" + target + " dist=" + dist);
			dmBotLog.Debug("[CAR] Drive: limit=" + m_SpeedLimit + " angle=" + angle + " gear=" + curGear + " rpm=" + rpm);
			dmBotLog.Debug("[CAR] Drive: steer=" + m_WheelSteer + " push=" + pushing + " reverse=" + m_Reverse);
		}
		#endif
	}

	//! Инкрементальный рефилл маршрута: дозапрашивает у dmRoadRouter следующий чанк
	//! (NextChunk) и добавляет его точки в ХВОСТ m_DriveRoute. Пропускается, если
	//! маршрут исчерпан или в очереди ещё хватает точек (запас DM_DRIVE_REFILL_MARGIN).
	//! NextChunk ЧИСТИТ переданный массив, поэтому передаём отдельный локальный
	//! буфер, а не m_DriveRoute — иначе Clear снёс бы пройденные/непройденные точки
	//! очереди. NextChunk выдаёт финальный чанк (включая целевую точку) И ТОЛЬКО
	//! ПОТОМ возвращает false — поэтому точки вставляются ДО проверки more.
	void RefillRoute()
	{
		if (m_RouteExhausted)
			return;
		if (m_DriveRouteIdx < m_DriveRoute.Count() - DM_DRIVE_REFILL_MARGIN)
			return;

		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Drive.Refill");
		#endif

		//! Отдельный локальный буфер (не m_DriveRoute): NextChunk чистит входной
		//! массив перед выдачей, иначе рефилл снёс бы уже пройденные точки.
		array<vector> buffer = new array<vector>();
		bool more = dmRoadRouter.Get().NextChunk(buffer, DM_DRIVE_LOOKAHEAD);

		int i;
		for (i = 0; i < buffer.Count(); i++)
			m_DriveRoute.Insert(buffer[i]);

		if (buffer.Count() > 0)
			RebuildCumDist();

		if (!more)
			m_RouteExhausted = true;

		#ifdef DM_BOT_DEBUG_CAR
		dmBotLog.Debug("[CAR] Drive: refill +" + buffer.Count() + " точек, exhausted=" + m_RouteExhausted);
		#endif
	}

	//! Нативный привод: газ по отклонению от целевой скорости (dm_DriveThrottle,
	//! применяется через SetThrottle в CarScript.OnInput) + передачи ShiftTo.
	//! Превышение лимита — реальный тормоз (dm_DriveBrake > 0), а не просто сброс газа:
	//! сброс газа катит машину ещё десятки метров (врез в барьер). Возвращает true,
	//! если сейчас разгоняемся (используется в драйв-логе).
	bool ApplyDriveForce(float speedAbs)
	{
		float margin = 3.0;
		bool pushing = false;

		if (speedAbs > m_SpeedLimit + margin)
		{
			//! Превышаем лимит — реально тормозим (не катимся): быстрое замедление
			//! до целевой скорости (напр. с 47 до 20 км/ч при детекте препятствия).
			m_Car.dm_DriveThrottle = 0.0;
			m_Car.dm_DriveBrake = DM_DRIVE_BRAKE_STRENGTH;
		}
		else if (speedAbs < m_SpeedLimit - margin)
		{
			//! Ниже цели — разгоняемся нативным газом (реальная тяга от двигателя);
			//! тормоз обязательно снимаем, иначе не разгонится.
			pushing = true;
			m_Car.dm_DriveThrottle = 0.6;
			m_Car.dm_DriveBrake = 0.0;
		}
		else
		{
			//! Накат в коридоре — газ убираем, тормоз снят.
			m_Car.dm_DriveThrottle = 0.0;
			m_Car.dm_DriveBrake = 0.0;
		}

		//! Руль остаётся активным через dm_DriveActive (SetSteering в CarScript.OnInput).
		m_Car.dm_DriveActive = true;

		//! RPM-звук — нативный (dm_DriveThrottle поднимает EngineGetRPM через
		//! OnInput); dm_DriveSimRPM остаётся выключенным (-1).
		m_Car.dm_DriveSimRPM = -1.0;

		return pushing;
	}

	//! Передачи: МКПП по скорости, АКПП по режиму D/R/N.
	void ShiftGear(float speedAbs)
	{
		CarGearboxType type = m_Car.GearboxGetType();
		if (type == CarGearboxType.MANUAL)
		{
			//! МКПП: передача вперёд по скорости, реверс при m_Reverse.
			int targetGear = CarGear.FIRST;
			if (m_Reverse)
				targetGear = CarGear.REVERSE;
			else if (speedAbs < 15.0)
				targetGear = CarGear.FIRST;
			else if (speedAbs < 30.0)
				targetGear = CarGear.SECOND;
			else if (speedAbs < 45.0)
				targetGear = CarGear.THIRD;
			else
				targetGear = CarGear.FOURTH;

			if (m_Car.GetCurrentGear() != targetGear)
				m_Car.ShiftTo(targetGear);
		}
		else
		{
			CarAutomaticGearboxMode target = CarAutomaticGearboxMode.D;
			if (m_Reverse)
				target = CarAutomaticGearboxMode.R;
			else if (speedAbs < 2.0)
				target = CarAutomaticGearboxMode.N;

			if (m_Car.GearboxGetMode() != target)
				m_Car.ShiftTo(target);
		}
	}

	//! Нейтраль: отсоединяем двигатель от колёс. Нужна перед жёстким тормозом
	//! (фаза сот/объезда) — на ручной КПП тормоз на высокой передаче глушит мотор
	//! (rpm=0 → engine=false), машина глохнет и не едет дальше.
	void ShiftToNeutral()
	{
		CarGearboxType type = m_Car.GearboxGetType();
		if (type == CarGearboxType.MANUAL)
		{
			if (m_Car.GetCurrentGear() != CarGear.NEUTRAL)
				m_Car.ShiftTo(CarGear.NEUTRAL);
		}
		else
		{
			if (m_Car.GearboxGetMode() != CarAutomaticGearboxMode.N)
				m_Car.ShiftTo(CarAutomaticGearboxMode.N);
		}
	}

	//! Руль: нативный (пишется в m_Car.dm_DriveSteering, применяется через
	//! SetSteering в CarScript.OnInput). steerTarget в <-1,1> от угла (±90°).
	//! ВАЖНО (знак, телеметрия DRIVE-TELE): angle=Atan2(cross,dot) положителен,
	//! когда цель СЛЕВА от курса, а SetSteering положителен = поворот ВПРАВО —
	//! поэтому steerTarget = -angle/1.57 (иначе руль отворачивает от цели и
	//! траектория уходит в спираль ±π). Поворот делает нативный руль —
	//! боковой импульс не нужен (на низком сцеплении он толкал кузов поперёк).
	//! K-разворот: в фазах REVERSING/FORWARD руль выворачивается НЕЗАВИСИМО от
	//! speedAbs (на ~0 км/ч колёса должны встать ДО подачи газа) — гейт
	//! DM_DRIVE_STEER_MIN_SPEED обходится для манёвра.
	void ApplySteering(float angle, float speedAbs, float pDt)
	{
		float steerTarget = 0.0;
		float absAngle = Math.AbsFloat(angle);

		if (m_TurnState == K_TURN_REVERSING)
		{
			//! Реверс: руль в ПРОТИВОПОЛОЖНУЮ от маршрута сторону (знак +angle/1.57,
			//! против обычного -angle/1.57). При заднем ходе поворот колёс вправо
			//! (steer>0) качнёт НОС влево — к маршруту: angle>0 (маршрут слева) →
			//! steer>0 (вправо) → нос влево, на маршрут.
			steerTarget = Math.Clamp(angle / 1.57, -1.0, 1.0);
		}
		else if (m_TurnState == K_TURN_FORWARD)
		{
			//! Передний ход: обычный руль к маршруту (доворот носа).
			steerTarget = Math.Clamp(-angle / 1.57, -1.0, 1.0);
		}
		else if (m_Reverse)
		{
			//! Реверс от застревания (TickStuck), НЕ K-разворот (фазы выше уже
			//! обработали m_TurnState != NONE): выворачиваем руль на ФИКСИРОВАННЫЙ
			//! угол ОТ маршрута, чтобы зад уходил вбок и машина не возвращалась в
			//! ту же точку. Знак: angle>0 = маршрут слева; на заднем ходу нос идёт
			//! ПРОТИВ руля — чтобы нос ушёл ОТ маршрута (вправо), руль влево
			//! (steer<0). Фикс, а не -angle/1.57: при лобовом застревании |angle|≈0
			//! и медленном ходе (speedAbs < DM_DRIVE_STEER_MIN_SPEED) руль не
			//! выворачивается вовсе (deadzone) → прямой реверс в ту же точку.
			if (angle > 0.0)
				steerTarget = -DM_DRIVE_STUCK_REVERSE_STEER;
			else
				steerTarget = DM_DRIVE_STUCK_REVERSE_STEER;
		}
		else if (speedAbs >= DM_DRIVE_STEER_MIN_SPEED && absAngle >= DRIVE_STEER_ANGLE_DEADZONE)
		{
			steerTarget = Math.Clamp(-angle / 1.57, -1.0, 1.0);
		}

		float t = Math.Min(1.0, DM_DRIVE_WHEEL_STEER_SPEED * pDt);
		m_WheelSteer = Math.Lerp(m_WheelSteer, steerTarget, t);
		m_Car.dm_DriveSteering = m_WheelSteer;
	}

	//! K-разворот (реверс-разворот): машина боком к маршруту на ~0 км/ч не может
	//! довернуться нативным рулём (тот работает только в движении — на месте машина
	//! «качается» вперёд-назад реверсом TickStuck, руль не выворачивается). Манёвр:
	//! задним ходом выворачиваем нос к маршруту (руль в противоположную от маршрута
	//! сторону), затем передним доворачиваем. Стейт-машина NONE → REVERSING →
	//! FORWARD → NONE. Управляет m_Reverse СВОИМ флагом (не конфликтует с TickStuck —
	//! тот гейтится по m_TurnState == NONE) и выставляет руль ДО подачи газа
	//! (ApplySteering в фазах манёвра обходит гейт DM_DRIVE_STEER_MIN_SPEED).
	void TickKTurn(float angle, float speedAbs, float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Drive.KTurn");
		#endif

		float absAngle = Math.AbsFloat(angle);

		if (m_TurnState == K_TURN_NONE)
		{
			//! Триггер: большой угол к маршруту И низкая скорость. На ~0 км/ч нативный
			//! руль не разворачивает машину — нужен реверс-разворот.
			float trigRad = DM_DRIVE_REVERSE_TURN_ANGLE * Math.DEG2RAD;
			if (absAngle > trigRad && speedAbs < DM_DRIVE_REVERSE_TURN_MAX_SPEED)
			{
				m_TurnState = K_TURN_REVERSING;
				m_TurnTimer = 0.0;
				m_Reverse = true;
				//! Ребейз прогресса застревания: после манёвра дистанция резко
				//! прыгнула (ехали назад) — иначе TickStuck примет её за «нет прогресса».
				m_LastWaypointDist = -1.0;
				m_StuckCounter = 0;
				#ifdef DM_BOT_DEBUG_CAR
				dmBotLog.Debug("[CAR] KTurn: REVERSING angle=" + absAngle);
				#endif
			}
			return;
		}

		m_TurnTimer = m_TurnTimer + pDt;

		if (m_TurnState == K_TURN_REVERSING)
		{
			//! Реверс: задняя передача (ShiftGear по m_Reverse), нос доворачиваем к
			//! маршруту. Выход по гистерезису (|angle| упал ниже ANGLE-MARGIN ~40°)
			//! ИЛИ по таймаут-страховке.
			m_Reverse = true;
			float exitRad = (DM_DRIVE_REVERSE_TURN_ANGLE - DM_DRIVE_REVERSE_TURN_MARGIN) * Math.DEG2RAD;
			if (absAngle < exitRad || m_TurnTimer > DM_DRIVE_REVERSE_TURN_TIMEOUT)
			{
				m_TurnState = K_TURN_FORWARD;
				m_TurnTimer = 0.0;
				m_Reverse = false;
				#ifdef DM_BOT_DEBUG_CAR
				dmBotLog.Debug("[CAR] KTurn: FORWARD angle=" + absAngle);
				#endif
			}
			return;
		}

		//! K_TURN_FORWARD: передний ход (m_Reverse=false), обычный руль к маршруту
		//! (ApplySteering), доворот носа до выхода в NONE.
		m_Reverse = false;
		float doneRad = DM_DRIVE_REVERSE_TURN_DONE_ANGLE * Math.DEG2RAD;
		if (absAngle < doneRad || m_TurnTimer > DM_DRIVE_REVERSE_TURN_TIMEOUT)
		{
			m_TurnState = K_TURN_NONE;
			m_TurnTimer = 0.0;
			#ifdef DM_BOT_DEBUG_CAR
			dmBotLog.Debug("[CAR] KTurn: DONE angle=" + absAngle);
			#endif
		}
	}

	//! Застревание: не уменьшается дистанция до вейпоинта (прогресс < EPS за тик)
	//! → считаем тики; порог → реверс. Ловит и медленное ползание (~1.4 км/ч),
	//! которое старый порог speedAbs<0.5 пропускал. Реверс ограничен по длительности
	//! (DM_DRIVE_REVERSE_MAX_TICKS) и имеет грейс-период после флипа
	//! (m_StuckCounter=20), иначе машина оседает на ~0.9 км/ч и реверс залипает
	//! навсегда.
	void TickStuck(float dist)
	{
		if (m_Reverse)
		{
			m_ReverseTicks = m_ReverseTicks + 1;
			if ((float)m_ReverseTicks > DM_DRIVE_REVERSE_MAX_TICKS)
			{
				//! Реверс длится слишком долго — принудительно возвращаемся вперёд.
				m_Reverse = false;
				m_ReverseTicks = 0;
				m_StuckCounter = 0;
			}
		}

		if (m_LastWaypointDist < 0.0)
		{
			//! Первый тик (или после сброса) — только запоминаем дистанцию.
			m_LastWaypointDist = dist;
			return;
		}

		if (dist < m_LastWaypointDist - DM_DRIVE_PROGRESS_EPS)
		{
			//! Есть прогресс — дистанция до вейпоинта падает.
			m_LastWaypointDist = dist;
			m_StuckCounter = 0;
		}
		else
		{
			//! Дистанция не падает (стоим/ползём/откатываемся) — копим тики.
			m_StuckCounter = m_StuckCounter + 1;
		}

		if ((float)m_StuckCounter > DM_DRIVE_STUCK_THRESHOLD)
		{
			m_Reverse = !m_Reverse;
			m_StuckCounter = 20;
			m_LastWaypointDist = -1.0;
			if (!m_Reverse)
				m_ReverseTicks = 0;
		}
	}

	//! Локальный объезд: широкий райкаст от from до to (radius). Физическая геометрия
	//! (ObjIntersectGeom) — ловит обломки/баррикады по коллизии, а не view-листву/кроны.
	//! Возвращает первое НЕ-self попадание: пропускаем машину (obj/parent), водителя,
	//! террейн/воду с нормалью вверх (obj == null + dir[1] > EPS — земля не препятствие,
	//! машина едет по ней; obj == null с горизонтальной нормалью — барьер) и
	//! растительность (кусты IsBush — проходимы; деревья IsTree остаются препятствием).
	//! pIgnore не исключает самопопадание, когда луч стартует внутри коллайдера, поэтому
	//! self фильтруется вручную. true = попадание; hitPos — позиция первого не-self хита.
	bool RaycastHits(vector from, vector to, float radius, out vector hitPos)
	{
		hitPos = Vector(0.0, 0.0, 0.0);
		RaycastRVParams params = new RaycastRVParams(from, to, m_Car, radius);
		params.flags = CollisionFlags.ALLOBJECTS;
		params.type = ObjIntersectGeom;
		ref array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
		if (DayZPhysics.RaycastRVProxy(params, hits))
		{
			Human driver = m_Car.CrewMember(DayZPlayerConstants.VEHICLESEAT_DRIVER);
			int i;
			for (i = 0; i < hits.Count(); i++)
			{
				RaycastRVResult hit = hits[i];
				if (hit.obj == m_Car || hit.parent == m_Car)
					continue;
				if (driver && hit.obj == driver)
					continue;
				//! obj == null — террейн/вода/статическая коллизия без script-объекта:
				//! земля И бетонные заграждения приходят одинаково (без obj). Различаем по
				//! нормали поверхности: у земли нормаль вверх (dir[1]≈1), у барьера/камня/
				//! насыпи — горизонтально (dir[1]≈0). Пропускаем только землю (нормаль вверх):
				//! горизонтальный луч на склоне задевает землю впереди и давал ложный
				//! «obstacle ahead»; барьер с горизонтальной нормалью остаётся препятствием.
				if (!hit.obj && hit.dir[1] > DM_DRIVE_GROUND_NORMAL_EPS)
					continue;
				//! Растительность проходима: пропускаем кусты (BushHard/BushSoft дают
				//! IsBush()==true), иначе придорожные кусты дают ложный «obstacle ahead».
				//! hit.obj && — куст проверяем только при не-null obj (барьер без obj
				//! уже прошёл нормаль-фильтр выше и не должен упасть на null-deref).
				if (hit.obj && hit.obj.IsBush())
					continue;
				hitPos = hit.pos;
				return true;
			}
		}
		return false;
	}

	//! Детект препятствий впереди (троттлинг DM_DRIVE_DETECT_INTERVAL): широкий райкаст
	//! по сегментам будущих вейпоинтов [carPos→W[idx]]→[W[idx]→W[idx+1]]→... пока суммарная
	//! длина ≤ DM_DRIVE_DETECT_DISTANCE. Работает ТОЛЬКО в DPH_DRIVING (лучи ВКЛ). Попадание
	//! → запуск грид-объезда (StartGridDetour → DPH_DETOUR, лучи ВЫКЛ).
	void TickDetect(vector carPos, float pDt)
	{
		m_DetectTimer = m_DetectTimer + pDt;
		if (m_DetectTimer < DM_DRIVE_DETECT_INTERVAL)
			return;
		m_DetectTimer = 0.0;

		//! Гейт по скорости: на старте/при ползании машина ещё не выровнялась на оси
		//! дороги — детект от невыровненной позиции даёт ложное препятствие. Не
		//! детектим, пока не поедем.
		if (m_Car.GetSpeedometerAbsolute() < DM_DRIVE_DETECT_MIN_SPEED)
		{
			#ifdef DM_BOT_DEBUG_CAR
			dmBotLog.Debug("[CAR] Drive: detect skipped, speed too low");
			#endif
			return;
		}

		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Drive.Detect");
		#endif

		//! Высота скана — реальная высота кузова машины (carPos[1] = origin у земли,
		//! + DM_DRIVE_DETECT_BODY_OFFSET). Верхний луч ловит высокие препятствия сквозь
		//! кузов; нижний луч (SurfaceY + GROUND_RAY_HEIGHT) — низкие заграждения.
		//! Террейн в райкаст попадает (obj == null), но RaycastHits его пропускает —
		//! земля не препятствие.
		float scanY = carPos[1] + DM_DRIVE_DETECT_BODY_OFFSET;

		//! Направление машины (горизонталь) — выносим старт луча вперёд бампера, вне
		//! коллайдера, иначе луч стартует внутри корпуса и самопопадает на t=0.
		vector carDirRaw = m_Car.GetDirection();
		vector carDir = carDirRaw;
		carDir[1] = 0.0;
		if (carDir.Length() < 0.01)
			carDir = Vector(1.0, 0.0, 0.0);
		else
			carDir.Normalize();

		vector from = carPos + carDir * DM_DRIVE_DETECT_START_OFFSET;
		float total = 0.0;
		int n = m_DriveRoute.Count();
		int i = m_DriveRouteIdx;

		while (i < n)
		{
			vector to = m_DriveRoute[i];
			vector seg = to - from;
			seg[1] = 0.0;
			float segLen = seg.Length();
			if (segLen < 0.01)
			{
				from = to;
				i = i + 1;
				continue;
			}
			if (total + segLen > DM_DRIVE_DETECT_DISTANCE)
				break;

			total = total + segLen;

			vector segFrom = from;
			vector segTo = to;
			segFrom[1] = scanY;
			segTo[1] = scanY;

			//! Нижний луч у земли (параллельно верхнему, та же горизонталь XZ): ловит
			//! низкие заграждения, которые верхний луч на высоте кузова перелетает.
			//! Y старта — земля под точкой старта сегмента + высота; Y конца — segToLow
			//! (копия вейпоинта, чей Y уже на уровне дороги SurfaceY) + высота, т.е.
			//! земля у вейпоинта + высота.
			vector segFromLow = from;
			vector segToLow = to;
			float groundY = GetGame().SurfaceY(segFromLow[0], segFromLow[2]);
			segFromLow[1] = groundY + DM_DRIVE_DETECT_GROUND_RAY_HEIGHT;
			segToLow[1] = segToLow[1] + DM_DRIVE_DETECT_GROUND_RAY_HEIGHT;

			vector hitPos;
			vector hitPosLow;
			if (RaycastHits(segFrom, segTo, DM_DRIVE_OBSTACLE_RAY_RADIUS, hitPos))
			{
				#ifdef DM_BOT_DEBUG_CAR
				dmBotLog.Debug("[CAR] Drive: obstacle ahead pos=" + hitPos + " blockIdx=" + i);
				#endif
				StartGridDetour(carPos);
				return;
			}
			if (RaycastHits(segFromLow, segToLow, DM_DRIVE_OBSTACLE_RAY_RADIUS, hitPosLow))
			{
				#ifdef DM_BOT_DEBUG_CAR
				dmBotLog.Debug("[CAR] Drive: obstacle ahead (low) pos=" + hitPosLow + " blockIdx=" + i);
				#endif
				StartGridDetour(carPos);
				return;
			}

			from = to;
			i = i + 1;
		}
	}

	//! Смена фазы вождения с логом (детерминированные переходы STARTING/DRIVING/DETOUR).
	void SetDrivingPhase(int phase)
	{
		m_DrivingPhase = phase;
		#ifdef DM_BOT_DEBUG_CAR
		string phaseName = "STARTING";
		if (phase == DPH_DRIVING)
			phaseName = "DRIVING";
		else if (phase == DPH_DETOUR)
			phaseName = "DETOUR";
		dmBotLog.Debug("[CAR] Drive: phase=" + phaseName);
		#endif
	}

	//! Запуск грид-объезда (соты): origin — НОС машины (вперёд по реальному курсу),
	//! направление территории d — направление дороги (по вейпоинтам маршрута). Ставит
	//! m_HoneyState = 1 и фазу DETOUR; лучи препятствий до конца объезда ВЫКЛ.
	void StartGridDetour(vector carPos)
	{
		vector carDirRaw = m_Car.GetDirection();
		vector carDir = carDirRaw;
		carDir[1] = 0.0;
		if (carDir.Length() < 0.01)
			carDir = Vector(1.0, 0.0, 0.0);
		else
			carDir.Normalize();
		vector honeyOrigin = carPos + carDir * DM_DRIVE_HONEY_ORIGIN_OFFSET;

		vector d;
		int j = m_DriveRouteIdx;
		int n = m_DriveRoute.Count();
		if (j > 0)
			d = m_DriveRoute[j] - m_DriveRoute[j - 1];
		else if (n > 1)
			d = m_DriveRoute[1] - m_DriveRoute[0];
		else
			d = Vector(1.0, 0.0, 0.0);
		d[1] = 0.0;
		if (d.Length() < 0.01)
			d = Vector(1.0, 0.0, 0.0);
		else
			d.Normalize();

		Human driver = m_Car.CrewMember(DayZPlayerConstants.VEHICLESEAT_DRIVER);
		dmRoadHoneycomb.Get().Start(honeyOrigin, d, m_DriveRoute, m_DriveRouteIdx, m_Car, driver, 0.0, 0);
		m_HoneyState = 1;
		m_Reverse = false;
		SetDrivingPhase(DPH_DETOUR);

		#ifdef DM_BOT_DEBUG_CAR
		dmBotLog.Debug("[CAR] Drive: honeycomb start origin=" + honeyOrigin);
		#endif
	}

	//! Пересчёт кумулятивных длин вдоль m_DriveRoute: m_RouteCumDist[i] = путь по
	//! ломаной от точки 0 до точки i (XZ). Зовётся при каждом изменении m_DriveRoute.
	void RebuildCumDist()
	{
		m_RouteCumDist = new array<float>();
		int n = m_DriveRoute.Count();
		int i;
		vector a;
		vector b;
		float dx;
		float dz;
		if (n == 0)
			return;
		m_RouteCumDist.Insert(0.0);
		for (i = 1; i < n; i++)
		{
			a = m_DriveRoute[i - 1];
			b = m_DriveRoute[i];
			dx = b[0] - a[0];
			dz = b[2] - a[2];
			m_RouteCumDist.Insert(m_RouteCumDist[i - 1] + Math.Sqrt(dx * dx + dz * dz));
		}
	}

	//! Прогресс машины вдоль маршрута: проекция носа на ближайший сегмент ломаной
	//! (скан от m_DriveRouteIdx вперёд до +DM_DRIVE_PROGRESS_SEGMENTS сегментов).
	//! Возвращает расстояние вдоль маршрута (cumDist) до точки-проекции носа.
	float ComputeProgress(vector nose)
	{
		int n = m_DriveRoute.Count();
		if (n < 2 || !m_RouteCumDist)
			return 0.0;
		int start = m_DriveRouteIdx;
		if (start < 0)
			start = 0;
		int maxIdx = start + DM_DRIVE_PROGRESS_SEGMENTS;
		if (maxIdx > n - 1)
			maxIdx = n - 1;
		float bestProgress = m_RouteCumDist[start];
		float bestDistSq = 1000000000.0;
		int i;
		vector a;
		vector b;
		float abx;
		float abz;
		float lenSq;
		float apx;
		float apz;
		float t;
		float projX;
		float projZ;
		float dx;
		float dz;
		float distSq;
		for (i = start; i < maxIdx; i++)
		{
			a = m_DriveRoute[i];
			b = m_DriveRoute[i + 1];
			abx = b[0] - a[0];
			abz = b[2] - a[2];
			lenSq = abx * abx + abz * abz;
			if (lenSq < 0.0001)
				continue;
			apx = nose[0] - a[0];
			apz = nose[2] - a[2];
			t = (apx * abx + apz * abz) / lenSq;
			if (t < 0.0)
				t = 0.0;
			else if (t > 1.0)
				t = 1.0;
			projX = a[0] + abx * t;
			projZ = a[2] + abz * t;
			dx = nose[0] - projX;
			dz = nose[2] - projZ;
			distSq = dx * dx + dz * dz;
			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				dx = projX - a[0];
				dz = projZ - a[2];
				bestProgress = m_RouteCumDist[i] + Math.Sqrt(dx * dx + dz * dz);
			}
		}
		return bestProgress;
	}

	//! Тик сот: машина жёстко тормозит, соты считают асинхронно. По DONE — вставляем
	//! путь и продолжаем к цели (ре-снэп роутера), по FAILED — настоящий тупик (Fail,
	//! но только при скорости ≈ 0).
	void TickHoneycomb(vector carPos, float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Drive.Honeycomb");
		#endif

		float gridStart = GetGame().GetTickTime();
		while (!dmRoadHoneycomb.Get().IsDone() && !dmRoadHoneycomb.Get().IsFailed())
		{
			dmRoadHoneycomb.Get().Tick(pDt);
			if ((GetGame().GetTickTime() - gridStart) * 1000.0 >= DM_DRIVE_GRID_BUDGET_MS)
				break;
		}

		//! Жёсткий тормоз до полной остановки, пока соты считают объезд (маршрута
		//! ещё нет). Нейтраль перед тормозом, чтобы ручная КПП не глохла на высокой
		//! передаче; в нейтрали полный тормоз (1.0) безопасен и гасит скорость
		//! быстрее щадящего DM_DRIVE_BRAKE_STRENGTH (0.5, для КПП на передаче).
		float speedAbs = m_Car.GetSpeedometerAbsolute();
		ShiftToNeutral();
		m_SpeedLimit = 0.0;
		m_Car.dm_DriveThrottle = 0.0;
		m_Car.dm_DriveBrake = DM_DRIVE_HARD_BRAKE;
		m_Car.dm_DriveActive = true;
		m_Car.dm_DriveSimRPM = -1.0;
		ApplySteering(0.0, speedAbs, pDt);

		if (dmRoadHoneycomb.Get().IsFailed())
		{
			//! Гейт высадки: сдаёмся только когда машина реально остановилась.
			//! Пока катится быстрее DM_DRIVE_EXIT_MAX_SPEED — остаёмся за рулём и
			//! продолжаем тормозить (соты уже FAILED, Tick() — no-op).
			if (speedAbs <= DM_DRIVE_EXIT_MAX_SPEED)
			{
				m_HoneyState = 0;
				#ifdef DM_BOT_DEBUG_CAR
				dmBotLog.Debug("[CAR] honeycomb: failed (stopped, speed=" + speedAbs + ")");
				#endif
				Fail();
			}
			return;
		}

		if (dmRoadHoneycomb.Get().IsDone())
		{
			array<vector> path = new array<vector>();
			//! Путь из ≥2 точек (вход+выход — прямая дорога без препятствия, или
			//! объезд) валиден: вставляем и едем по нему.
			if (dmRoadHoneycomb.Get().GetPath(path) && path.Count() >= 2)
			{
				InsertHoneycombPath(path);
				m_LastWaypointDist = -1.0;
				m_HoneyState = 0;
				#ifdef DM_BOT_DEBUG_CAR
				dmBotLog.Debug("[CAR] honeycomb: path " + path.Count() + " точек first=" + path[0] + " last=" + path[path.Count() - 1]);
				#endif
			}
			else
			{
				//! Гейт высадки: путь < 2 точек (нет выхода) — настоящий тупик,
				//! но сдаёмся только когда машина реально остановилась. Пока катится
				//! быстрее DM_DRIVE_EXIT_MAX_SPEED — остаёмся за рулём и тормозим
				//! (как в ветке IsFailed).
				if (speedAbs <= DM_DRIVE_EXIT_MAX_SPEED)
				{
					m_HoneyState = 0;
					#ifdef DM_BOT_DEBUG_CAR
					dmBotLog.Debug("[CAR] honeycomb: degenerate path -> fail (stopped, speed=" + speedAbs + ")");
					#endif
					Fail();
				}
			}
		}
	}

	//! Вставить путь сот в маршрут: удалить вейпоинты с m_DriveRouteIdx до конца
	//! очереди, затем вставить сглаженный путь. Ре-снэпаем роутер на продолжение
	//! маршрута от дальней стороны обходного пути до исходной цели — иначе
	//! RefillRoute на следующем тике дёрнет уже исчерпанный роутер (NextChunk
	//! вернёт 0 точек) → m_RouteExhausted=true → условие финиша сработает на
	//! хвосте обходного пути, и бот высадится ДО цели.
	void InsertHoneycombPath(array<vector> path)
	{
		while (m_DriveRoute.Count() > m_DriveRouteIdx)
			m_DriveRoute.Remove(m_DriveRoute.Count() - 1);

		int i;
		for (i = 0; i < path.Count(); i++)
			m_DriveRoute.Insert(path[i]);

		//! Последний waypoint объезда = предпоследняя точка вставленного пути (перед
		//! точкой возврата на маршрут). Когда m_DriveRouteIdx проедет его по проекции —
		//! DETOUR → DRIVING (лучи снова ВКЛ).
		m_DetourLastIdx = m_DriveRouteIdx + path.Count() - 2;
		RebuildCumDist();

		vector farSide = path[path.Count() - 1];
		if (m_TargetPos.Length() > 0.01)
			dmRoadRouter.Get().Setup(farSide, m_TargetPos);

		m_RouteExhausted = false;
	}

	//! Graceful: гасим поля ввода (OnInput больше не прикладывает руль, OnSound
	//! больше не подменяет RPM), глушим двигатель, тормозим, включаем авто-тормоз
	//! без водителя. Гейт по m_EngineStarted: не трогаем машину, в которую бот так
	//! и не сел (Fail во время WALK-фазы посадки).
	void StopCar()
	{
		if (!m_Car)
			m_Car = CarScript.Cast(m_Transport);
		if (m_Car)
		{
			m_Car.dm_DriveActive = false;
			m_Car.dm_DriveThrottle = 0.0;
			m_Car.dm_DriveBrake = 0.0;
			m_Car.dm_DriveSteering = 0.0;
			m_Car.dm_DriveSimRPM = -1.0;
		}
		if (m_Car && m_EngineStarted)
		{
			m_Car.SetBrake(1.0);
			m_Car.EngineStop();
			m_Car.SetBrakesActivateWithoutDriver(true);
		}
		m_EngineStarted = false;
	}

	override void Finish()
	{
		#ifdef DM_BOT_DEBUG_CAR
		float exitSpeed = -1.0;
		if (m_Car)
			exitSpeed = m_Car.GetSpeedometerAbsolute();
		int routeLen = -1;
		if (m_DriveRoute)
			routeLen = m_DriveRoute.Count();
		dmBotLog.Debug("[CAR] Drive: Finish speed=" + exitSpeed + " idx=" + m_DriveRouteIdx + "/" + routeLen + " exhausted=" + m_RouteExhausted);
		#endif
		if (m_CommandInvoked && m_Phase == PHASE_SEATED)
			StopCar();
		super.Finish();
	}

	override void Fail()
	{
		#ifdef DM_BOT_DEBUG_CAR
		float exitSpeed = -1.0;
		if (m_Car)
			exitSpeed = m_Car.GetSpeedometerAbsolute();
		int routeLen = -1;
		if (m_DriveRoute)
			routeLen = m_DriveRoute.Count();
		dmBotLog.Debug("[CAR] Drive: Fail speed=" + exitSpeed + " idx=" + m_DriveRouteIdx + "/" + routeLen + " exhausted=" + m_RouteExhausted);
		#endif
		StopCar();
		super.Fail();
	}

	override void OnCancel(dmAISurvivor bot)
	{
		StopCar();
		super.OnCancel(bot);
	}
}
