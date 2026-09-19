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
//! (E2E-мост) долива не требует. Застревание детектится по прогрессу
//! дистанции (TickStuck) с реверсом. Финиш — только когда маршрут исчерпан И машина
//! достигла последней точки. Graceful-завершение глушит двигатель (StopCar) и
//! высаживает бота штатным выходом.
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
	//! тормоза катится ещё десятки метров и не успевает замедлиться до
	//! DM_DRIVE_DETOUR_SPEED до барьера.
	static const float DM_DRIVE_BRAKE_STRENGTH = 1.0;

	//! Локальный объезд: минимальная скорость для детекта (км/ч). На старте машина
	//! ещё не выровнялась на оси дороги и едва едет — проба полос от невыровненной
	//! позиции даёт ложный «no clear corridor». Не детектим, пока не поедем.
	static const float DM_DRIVE_DETECT_MIN_SPEED = 5.0;

	//! Локальный объезд: макс. боковое смещение коробки (м) — до него простирается
	//! проба полос (DM_DRIVE_DETOUR_OFFSET = DM_DRIVE_LANES_PER_SIDE × DM_DRIVE_LANE_STEP).
	static const float DM_DRIVE_DETOUR_OFFSET = 9.0;
	//! Локальный объезд: продольный размах коробки (м).
	static const float DM_DRIVE_DETOUR_SPAN = 15.0;
	//! Локальный объезд: скорость в манёвре (км/ч).
	static const float DM_DRIVE_DETOUR_SPEED = 20.0;

	//! Локальный объезд: кулдаун детекта после успешного объезда (с). Подавляет
	//! повторный детект ТОГО ЖЕ обломка с новой позиции коробки (второй объезд → fail).
	static const float DM_DRIVE_DETOUR_COOLDOWN = 6.0;
	//! Локальный объезд: длительность торможения после вставки коробки (с). m_DetourActive
	//! ставится в TickDetect и сбрасывается в PerformDetour в ТОМ ЖЕ кадре — без таймера
	//! лимит DM_DRIVE_DETOUR_SPEED живёт <1 кадра и машина разгоняется обратно. Таймер
	//! держит лимит до подъезда к коробке, дальше её углы притормаживают сами.
	static const float DM_DRIVE_DETOUR_BRAKE_TIME = 3.0;

	//! Локальный объезд: запас на ширину корпуса (м) — сдвиг центра коробки ДАЛЬШЕ
	//! от препятствия (~полкорпуса + зазор), чтобы ~2-м корпус не цеплял барьер
	//! краем. Центр зажимается по краям чистого коридора (PerformDetour).
	static const float DM_DRIVE_DETOUR_BODY_CLEARANCE = 1.5;

	//! Локальный объезд: шаг между полосами коридора (м).
	static const float DM_DRIVE_LANE_STEP = 3.0;
	//! Локальный объезд: радиус полосы коридора (м, ~пол-ширины машины).
	static const float DM_DRIVE_LANE_RADIUS = 1.0;
	//! Локальный объезд: число полос в одну сторону от осевой (9 м / 3 м = 3).
	static const int DM_DRIVE_LANES_PER_SIDE = 3;

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

	//! Локальный объезд: таймер троттлинга детекта (с).
	float m_DetectTimer = 0.0;

	//! Локальный объезд: активен (препятствие найдено, выполняем манёвр).
	bool m_DetourActive = false;

	//! Локальный объезд: кулдаун детекта после успешного объезда (с, декремент по тикам).
	float m_DetourCooldown = 0.0;

	//! Локальный объезд: таймер торможения после вставки коробки (с, декремент по тикам).
	//! Держит лимит DM_DRIVE_DETOUR_SPEED, пока m_DetourActive уже сброшен в PerformDetour.
	float m_DetourBrakeTimer = 0.0;

	//! Локальный объезд: позиция препятствия (проба сторон и коробка).
	vector m_DetourObstaclePos;

	//! Локальный объезд: индекс вейпоинта, к которому шли при детекте.
	int m_DetourBlockIdx = 0;

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
		}
		else
		{
			if (!m_DriveRoute)
				m_DriveRoute = new array<vector>();
			m_RouteExhausted = false;
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
		m_DetectTimer = 0.0;
		m_DetourActive = false;
		m_DetourCooldown = 0.0;
		m_DetourBrakeTimer = 0.0;
		m_DetourBlockIdx = 0;

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

		//! 1. Пауза после посадки: сидим, двигатель ещё не заводим (газ/руль не трогаем).
		m_SeatedFor += pDt;
		if (m_SeatedFor < DM_DRIVE_START_DELAY)
			return;

		//! 2. Старт двигателя (однократно; напрямую — ванильный ActionStartEngine
		//!    режет серверных ботов).
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

		//! 3. Прогрев двигателя: стоим, не трогаемся.
		m_WarmupFor += pDt;
		if (m_WarmupFor < DM_DRIVE_ENGINE_WARMUP)
			return;

		//! 4. Рефилл маршрута: доливаем чанк, когда очередь пуста или подходит к концу.
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

		//! Локальный объезд: детект препятствий впереди (троттлинг). Ставит m_DetourActive
		//! + позицию препятствия + индекс заблокированного вейпоинта.
		TickDetect(carPos, pDt);

		vector target = m_DriveRoute[m_DriveRouteIdx];
		vector toTarget = target - carPos;
		toTarget[1] = 0.0;
		float dist = toTarget.Length();

		//! Направление машины (горизонталь) — считаем ДО проверки достижения точки,
		//! чтобы dot (косинус угла к цели) был известен для гейта «проехали мимо».
		vector carDirRaw = m_Car.GetDirection();
		vector carDir = carDirRaw;
		carDir[1] = 0.0;
		if (carDir.Length() < 0.01)
			carDir = Vector(1.0, 0.0, 0.0);
		else
			carDir.Normalize();

		float toLen = toTarget.Length();
		if (toLen < 0.01)
			toLen = 1.0;
		float toDirX = toTarget[0] / toLen;
		float toDirZ = toTarget[2] / toLen;

		//! Угол до вейпоинта (знак через cross-произведение) — один раз на точку.
		float cross = carDir[0] * toDirZ - carDir[2] * toDirX;
		float dot = carDir[0] * toDirX + carDir[2] * toDirZ;
		float angle = Math.Atan2(cross, dot);

		//! Промежуточная точка считается пройденной, если корпус заехал на неё
		//! (dist < REACH) ИЛИ проехали мимо (точка позади, dot < PASSED_DOT) — тогда
		//! пропускаем её и едем к следующей, не разворачиваясь. Конечную точку по dot
		//! не пропускаем (isLast-гейт): до неё нужно доехать (см. DM_DRIVE_REACH).
		bool isLast = (m_DriveRouteIdx >= m_DriveRoute.Count() - 1);
		if (!isLast && (dist < DM_DRIVE_WAYPOINT_REACH || dot < DM_DRIVE_PASSED_DOT))
		{
			m_DriveRouteIdx = m_DriveRouteIdx + 1;
			//! Сменили точку — дистанция до новой резко прыгнула вверх; сброс
			//! «лучшей» дистанции, иначе TickStuck примет скачок за «нет прогресса».
			m_LastWaypointDist = -1.0;
			target = m_DriveRoute[m_DriveRouteIdx];
			toTarget = target - carPos;
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

		//! 5. Конец маршрута: исчерпан И машина достигла последней точки
		//! (гэп DM_DRIVE_REACH). До исчерпания финиш не наступает — маршрут ещё
		//! доливается чанками.
		if (m_RouteExhausted && vector.Distance(carPos, m_DriveRoute[m_DriveRoute.Count() - 1]) < DM_DRIVE_REACH)
		{
			Finish();
			return;
		}

		float speedAbs = m_Car.GetSpeedometerAbsolute();

		//! 6a. K-разворот: при большом угле к маршруту на низкой скорости запускаем
		//!    реверс-разворот (стейт-машина управляет m_Reverse своим флагом).
		TickKTurn(angle, speedAbs, pDt);

		//! 6. Целевая скорость: ниже в повороте; при активном объезде — тормозим;
		//!    в манёвре K-разворота — медленный разворот. Сглаживаем.
		float limit = DM_DRIVE_MAX_SPEED_STRAIGHT;
		if (m_TurnState != K_TURN_NONE)
			limit = DM_DRIVE_REVERSE_TURN_MAX_SPEED;
		else if (m_DetourActive)
			limit = DM_DRIVE_DETOUR_SPEED;
		else if (m_DetourBrakeTimer > 0.0)
		{
			limit = DM_DRIVE_DETOUR_SPEED;
			m_DetourBrakeTimer = m_DetourBrakeTimer - pDt;
		}
		else if (Math.AbsFloat(angle) > DM_DRIVE_TURN_ANGLE_THRESHOLD)
			limit = DM_DRIVE_MAX_SPEED_TURNING;
		m_SpeedLimit = Math.Lerp(m_SpeedLimit, limit, DM_DRIVE_SPEED_SMOOTH);

		//! 7. Нативный газ/тормоз (dm_DriveThrottle) + передачи.
		bool pushing = ApplyDriveForce(speedAbs);

		//! 8. Передачи (вперёд по скорости).
		ShiftGear(speedAbs);

		//! 9. Руль (нативный).
		ApplySteering(angle, speedAbs, pDt);

		//! 10. Манёвр объезда (проба сторон + коробка) — после руля, чтобы коробка
		//!    встала на следующий тик; снимает m_DetourActive.
		if (m_DetourActive)
			PerformDetour(carPos);

		//! 11. Застревание → реверс. Гейт по K_TURN_NONE: во время K-разворота реверсом
		//!    управляет сам манёвр (свой m_Reverse), TickStuck не должен его дёргать.
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
	//! длина ≤ DM_DRIVE_DETECT_DISTANCE. Попадание → m_DetourActive + позиция + индекс.
	void TickDetect(vector carPos, float pDt)
	{
		//! Кулдаун после успешного объезда: детект подавлен, пока не истечёт.
		//! Декремент каждый тик (реальное время), иначе ТОТ ЖЕ обломок ловится
		//! повторно с новой позиции коробки → второй объезд → ложный fail.
		if (m_DetourCooldown > 0.0)
		{
			m_DetourCooldown = m_DetourCooldown - pDt;
			return;
		}

		m_DetectTimer = m_DetectTimer + pDt;
		if (m_DetectTimer < DM_DRIVE_DETECT_INTERVAL)
			return;
		m_DetectTimer = 0.0;

		//! Гейт по скорости: на старте/при ползании машина ещё не выровнялась на оси
		//! дороги — проба полос даёт ложный «no clear corridor». Не детектим, пока
		//! не поедем.
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
				m_DetourObstaclePos = hitPos;
				m_DetourBlockIdx = i;
				m_DetourActive = true;
				#ifdef DM_BOT_DEBUG_CAR
				dmBotLog.Debug("[CAR] Drive: obstacle ahead pos=" + hitPos + " blockIdx=" + i);
				#endif
				return;
			}
			if (RaycastHits(segFromLow, segToLow, DM_DRIVE_OBSTACLE_RAY_RADIUS, hitPosLow))
			{
				m_DetourObstaclePos = hitPosLow;
				m_DetourBlockIdx = i;
				m_DetourActive = true;
				#ifdef DM_BOT_DEBUG_CAR
				dmBotLog.Debug("[CAR] Drive: obstacle ahead (low) pos=" + hitPosLow + " blockIdx=" + i);
				#endif
				return;
			}

			from = to;
			i = i + 1;
		}
	}

	//! Манёвр «коробка» вокруг препятствия: направление дороги d, перпендикуляр p, проба
	//! набора полос ВПЕРЁД (боковые смещения [-9..+9] шагом 3), выбор САМОГО ШИРОКОГО
	//! чистого коридора, коробка [A,B,C,D,E] через его центр. Весь путь коробки проверяется
	//! райкастом ДО вставки; нет чистого пути ни по одному коридору → Fail.
	void PerformDetour(vector carPos)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Drive.Detour");
		#endif

		int j = m_DetourBlockIdx;
		int n = m_DriveRoute.Count();

		//! 1. Направление дороги d (XZ, нормированный): W[j]-W[j-1]; j==0 → W[1]-W[0].
		vector d;
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

		//! Перпендикуляр p (влево от d).
		vector p = Vector(-d[2], 0.0, d[0]);

		vector obs = m_DetourObstaclePos;
		float scanY = carPos[1];

		//! 2. Проба полос: параллельные лучи ВПЕРЁД вдоль d (до DM_DRIVE_DETECT_DISTANCE
		//!    или до препятствия) на боковых смещениях [-9,-6,-3,0,+3,+6,+9]. Полоса
		//!    «чистая» = луч не попал. Старт луча вынесен вперёд бампера (как TickDetect).
		int lanesPerSide = DM_DRIVE_LANES_PER_SIDE;
		int laneCount = 2 * lanesPerSide + 1;
		array<bool> clear = new array<bool>();
		vector rayStart = carPos + d * DM_DRIVE_DETECT_START_OFFSET;
		vector hitPos;
		int li;
		float off = -DM_DRIVE_DETOUR_OFFSET;
		vector laneFrom;
		vector laneTo;
		for (li = 0; li < laneCount; li++)
		{
			laneFrom = rayStart + p * off;
			laneFrom[1] = scanY;
			laneTo = laneFrom + d * DM_DRIVE_DETECT_DISTANCE;
			clear.Insert(!RaycastHits(laneFrom, laneTo, DM_DRIVE_LANE_RADIUS, hitPos));
			off = off + DM_DRIVE_LANE_STEP;
		}

		#ifdef DM_BOT_DEBUG_CAR
		dmBotLog.Debug("[CAR] detour: lanes[0..3]=" + clear[0] + clear[1] + clear[2] + clear[3]);
		dmBotLog.Debug("[CAR] detour: lanes[4..6]=" + clear[4] + clear[5] + clear[6]);
		#endif

		//! 3. Непрерывные отрезки чистых полос (run'ы). Центр run'а = целевой боковой
		//!    offset коридора (адаптивный: ~0 между обломками или ±N на свободной стороне).
		array<int> runStart = new array<int>();
		array<int> runLen = new array<int>();
		int runBegin;
		int runEnd;
		li = 0;
		while (li < laneCount)
		{
			if (!clear[li])
			{
				li = li + 1;
				continue;
			}
			runBegin = li;
			runEnd = li;
			while (runEnd + 1 < laneCount && clear[runEnd + 1])
				runEnd = runEnd + 1;
			runStart.Insert(runBegin);
			runLen.Insert(runEnd - runBegin + 1);
			li = runEnd + 1;
		}
		int runCount = runStart.Count();

		//! 4. Коридоры по убыванию ширины: коробка через центр run'а, проверка ПОЛНОГО
		//!    пути коробки до вставки. Первый чистый путь → вставляем и выходим.
		int attempt;
		for (attempt = 0; attempt < runCount; attempt++)
		{
			int bestIdx = -1;
			int bestLen = 0;
			int k;
			for (k = 0; k < runCount; k++)
			{
				if (runLen[k] > bestLen)
				{
					bestLen = runLen[k];
					bestIdx = k;
				}
			}
			if (bestIdx < 0)
				break;

			int chosenStart = runStart[bestIdx];
			int chosenLen = bestLen;
			runLen[bestIdx] = 0;

			float startOff = -DM_DRIVE_DETOUR_OFFSET + (float)chosenStart * DM_DRIVE_LANE_STEP;
			float endOff = -DM_DRIVE_DETOUR_OFFSET + (float)(chosenStart + chosenLen - 1) * DM_DRIVE_LANE_STEP;
			float centerOff = (startOff + endOff) * 0.5;

			//! Запас на ширину корпуса: смещаем центр коробки ДАЛЬШЕ от препятствия.
			//! obsOff — боковая (по перпендикуляру p) проекция препятствия на линию
			//! маршрута; направление сдвига — ПРОЧЬ от неё. Зажимаем по краям чистого
			//! коридора (startOff..endOff): узкий коридор не вытолкнет коробку на
			//! заблокированную полосу.
			float obsOff = p[0] * (obs[0] - carPos[0]) + p[2] * (obs[2] - carPos[2]);
			if (centerOff >= obsOff)
				centerOff = centerOff + DM_DRIVE_DETOUR_BODY_CLEARANCE;
			else
				centerOff = centerOff - DM_DRIVE_DETOUR_BODY_CLEARANCE;
			centerOff = Math.Clamp(centerOff, startOff, endOff);

			#ifdef DM_BOT_DEBUG_CAR
			dmBotLog.Debug("[CAR] detour: corridor center=" + centerOff + " width=" + chosenLen);
			#endif

			//! Коробка (XZ; Y через SurfaceY) на offset = centerOff.
			float spanX = d[0] * DM_DRIVE_DETOUR_SPAN;
			float spanZ = d[2] * DM_DRIVE_DETOUR_SPAN;
			float offX = p[0] * centerOff;
			float offZ = p[2] * centerOff;
			vector boxA = Vector(obs[0] - spanX, 0.0, obs[2] - spanZ);
			vector boxB = Vector(boxA[0] + offX, 0.0, boxA[2] + offZ);
			vector boxC = Vector(obs[0] + offX, 0.0, obs[2] + offZ);
			vector boxD = Vector(obs[0] + spanX + offX, 0.0, obs[2] + spanZ + offZ);
			vector boxE = Vector(obs[0] + spanX, 0.0, obs[2] + spanZ);
			boxA[1] = GetGame().SurfaceY(boxA[0], boxA[2]);
			boxB[1] = GetGame().SurfaceY(boxB[0], boxB[2]);
			boxC[1] = GetGame().SurfaceY(boxC[0], boxC[2]);
			boxD[1] = GetGame().SurfaceY(boxD[0], boxD[2]);
			boxE[1] = GetGame().SurfaceY(boxE[0], boxE[2]);

			//! Путь коробки занят — берём следующий по ширине коридор.
			if (!BoxPathClear(boxA, boxB, boxC, boxD, boxE, scanY))
				continue;

			//! W[j-1] уже позади машины — убираем и его, иначе вход в коробку окажется
			//! за спиной. Заменяем вейпоинты на коробку.
			bool removePrev = false;
			if (j - 1 >= 0 && CarPassedWaypoint(carPos, m_DriveRoute[j - 1]))
				removePrev = true;
			ReplaceBlock(j, removePrev, boxA, boxB, boxC, boxD, boxE);

			//! Вооружаем кулдаун детекта: с новой позиции коробки ТОТ ЖЕ обломок
			//! снова попадает в луч через ~0.3 с — подавляем детект на время кулдауна.
			m_DetourCooldown = DM_DRIVE_DETOUR_COOLDOWN;
			m_DetourBrakeTimer = DM_DRIVE_DETOUR_BRAKE_TIME;
			m_DetourActive = false;
			m_LastWaypointDist = -1.0;
			return;
		}

		#ifdef DM_BOT_DEBUG_CAR
		dmBotLog.Debug("[CAR] detour: no clear corridor -> fail");
		#endif
		//! TODO: отмена команды — что делать? (сообщить игроку / остаться / выйти)
		Fail();
	}

	//! Проверка полного пути коробки райкастом на высоте корпуса (width-aware,
	//! self-фильтр через RaycastHits): сегменты A→B, B→C, C→D, D→E. true = весь путь чистый.
	bool BoxPathClear(vector boxA, vector boxB, vector boxC, vector boxD, vector boxE, float scanY)
	{
		vector hitPos;
		vector sFrom = boxA;
		vector sTo = boxB;
		sFrom[1] = scanY;
		sTo[1] = scanY;
		if (RaycastHits(sFrom, sTo, DM_DRIVE_LANE_RADIUS, hitPos))
			return false;

		sFrom = boxB;
		sTo = boxC;
		sFrom[1] = scanY;
		sTo[1] = scanY;
		if (RaycastHits(sFrom, sTo, DM_DRIVE_LANE_RADIUS, hitPos))
			return false;

		sFrom = boxC;
		sTo = boxD;
		sFrom[1] = scanY;
		sTo[1] = scanY;
		if (RaycastHits(sFrom, sTo, DM_DRIVE_LANE_RADIUS, hitPos))
			return false;

		sFrom = boxD;
		sTo = boxE;
		sFrom[1] = scanY;
		sTo[1] = scanY;
		if (RaycastHits(sFrom, sTo, DM_DRIVE_LANE_RADIUS, hitPos))
			return false;

		return true;
	}

	//! Машина уже проехала вейпоинт (он позади): машина ближе к препятствию, чем к нему.
	bool CarPassedWaypoint(vector carPos, vector wp)
	{
		float dx = wp[0] - carPos[0];
		float dz = wp[2] - carPos[2];
		float dWpSq = dx * dx + dz * dz;
		float ox = m_DetourObstaclePos[0] - carPos[0];
		float oz = m_DetourObstaclePos[2] - carPos[2];
		float dObsSq = ox * ox + oz * oz;
		return dObsSq < dWpSq;
	}

	//! Заменяет заблокированный вейпоинт W[j] на коробку [boxA..boxE]; если removePrev —
	//! убирает и W[j-1] (позади машины). m_DriveRouteIdx корректируется при снятии
	//! вейпоинта перед ним.
	void ReplaceBlock(int j, bool removePrev, vector boxA, vector boxB, vector boxC, vector boxD, vector boxE)
	{
		//! Удаляем сначала больший индекс (W[j]), затем W[j-1].
		m_DriveRoute.Remove(j);
		if (removePrev)
		{
			m_DriveRoute.Remove(j - 1);
			if (j - 1 < m_DriveRouteIdx)
				m_DriveRouteIdx = m_DriveRouteIdx - 1;
		}

		int at = j;
		if (removePrev)
			at = j - 1;

		m_DriveRoute.InsertAt(boxA, at);
		m_DriveRoute.InsertAt(boxB, at + 1);
		m_DriveRoute.InsertAt(boxC, at + 2);
		m_DriveRoute.InsertAt(boxD, at + 3);
		m_DriveRoute.InsertAt(boxE, at + 4);
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
		if (m_CommandInvoked && m_Phase == PHASE_SEATED)
			StopCar();
		super.Finish();
	}

	override void Fail()
	{
		StopCar();
		super.Fail();
	}

	override void OnCancel(dmAISurvivor bot)
	{
		StopCar();
		super.OnCancel(bot);
	}
}
