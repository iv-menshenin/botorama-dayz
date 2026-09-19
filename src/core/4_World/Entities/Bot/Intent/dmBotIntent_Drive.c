//! dmBotIntent_Drive — бот садится за руль и едет по маршруту из точек.
//!
//! Наследует dmBotIntent_GetInVehicle и переиспользует весь lifecycle посадки
//! (walk к двери → открыть дверь → GetInVehicle → сел). После посадки переводит
//! канал на DRIVE и ведёт машину нативным приводом: газ по отклонению от целевой
//! скорости (dm_DriveThrottle через SetThrottle в CarScript.OnInput) + передачи
//! ShiftTo (вперёд по скорости) + нативный руль SetSteering (через
//! dm_DriveSteering) — поворот делает нативный руль, как у реальной машины.
//! Маршрут — инкрементальный: владелец выставляет источник маршрута (m_RouteSource,
//! абстракция dmDriveRouteSource) до OnStart, а интент доливает точки чанками через
//! NextChunk (RefillRoute) в хвост очереди; застревание детектится по прогрессу
//! дистанции (TickStuck) с реверсом. Финиш — только когда маршрут исчерпан И машина
//! достигла последней точки. Graceful-завершение глушит двигатель (StopCar) и
//! высаживает бота штатным выходом.
class dmBotIntent_Drive : dmBotIntent_GetInVehicle
{
	//! Руль: мёртвая зона угла (рад), ниже — не рулим (анти-джиттер).
	static const float DRIVE_STEER_ANGLE_DEADZONE = 0.01;

	//! Локальный объезд: дистанция детекта препятствий вперёд (м).
	static const float DM_DRIVE_DETECT_DISTANCE = 50.0;
	//! Локальный объезд: интервал детекта препятствий (с).
	static const float DM_DRIVE_DETECT_INTERVAL = 0.3;
	//! Локальный объезд: радиус широкого райкаста препятствия (м, ~пол-ширины машины).
	static const float DM_DRIVE_OBSTACLE_RAY_RADIUS = 1.5;
	//! Локальный объезд: старт луча впереди бампера (м) — выносим точку старта за
	//! коллайдер машины, иначе луч стартует внутри коллайдера и самопопадает на t=0.
	static const float DM_DRIVE_DETECT_START_OFFSET = 4.0;

	//! Локальный объезд: боковое смещение коробки (м).
	static const float DM_DRIVE_DETOUR_OFFSET = 9.0;
	//! Локальный объезд: продольный размах коробки (м).
	static const float DM_DRIVE_DETOUR_SPAN = 15.0;
	//! Локальный объезд: скорость в манёвре (км/ч).
	static const float DM_DRIVE_DETOUR_SPEED = 20.0;

	//! Локальный объезд: радиус боковой пробы чистой стороны (м).
	static const float DM_DRIVE_DETOUR_PROBE_RADIUS = 1.0;
	//! Локальный объезд: допустимый перепад высоты земли на боку (м), иначе — обрыв.
	static const float DM_DRIVE_DETOUR_GROUND_EPS = 1.5;

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

	//! Источник маршрута (выставляет владелец-команда до OnStart). Абстракция
	//! над конкретным роутером (roads): интент (core) не зависит от роутера.
	//! null — предзаполненный маршрут (E2E-мост), рефилл не нужен.
	ref dmDriveRouteSource m_RouteSource;

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

	//! Последнее время драйв-лога (троттлинг ~2 c).
	float m_LastDriveLogTime = 0.0;

	//! Локальный объезд: таймер троттлинга детекта (с).
	float m_DetectTimer = 0.0;

	//! Локальный объезд: активен (препятствие найдено, выполняем манёвр).
	bool m_DetourActive = false;

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

		//! Маршрут приходит инкрементально через RefillRoute (источник — m_RouteSource).
		//! Предзаполненный владельцем список (старый путь) — «уже исчерпан»: рефилл
		//! не нужен, финиш по последней точке как раньше.
		if (!m_DriveRoute)
		{
			m_DriveRoute = new array<vector>();
			m_RouteExhausted = false;
		}
		else
		{
			m_RouteExhausted = true;
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
		m_LastDriveLogTime = 0.0;
		m_DetectTimer = 0.0;
		m_DetourActive = false;
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

		//! 6. Целевая скорость: ниже в повороте; при активном объезде — тормозим. Сглаживаем.
		float limit = DM_DRIVE_MAX_SPEED_STRAIGHT;
		if (m_DetourActive)
			limit = DM_DRIVE_DETOUR_SPEED;
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

		//! 11. Застревание → реверс.
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

	//! Инкрементальный рефилл маршрута: дозапрашивает у источника маршрута
	//! (m_RouteSource) следующий чанк (NextChunk) и добавляет его точки в ХВОСТ
	//! m_DriveRoute. Пропускается, если маршрут исчерпан, источник не задан или в
	//! очереди ещё хватает точек (запас DM_DRIVE_REFILL_MARGIN).
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

		//! Источник не задан (предзаполненный маршрут, напр. E2E-мост) — маршрут
		//! уже весь в очереди, доливать нечего.
		if (!m_RouteSource)
		{
			m_RouteExhausted = true;
			return;
		}

		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Drive.Refill");
		#endif

		//! Отдельный локальный буфер (не m_DriveRoute): NextChunk чистит входной
		//! массив перед выдачей, иначе рефилл снёс бы уже пройденные точки.
		array<vector> buffer = new array<vector>();
		bool more = m_RouteSource.NextChunk(buffer, DM_DRIVE_LOOKAHEAD);

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
	//! Торможение/накат — газ 0 (тормоз всегда 0). Возвращает true, если сейчас
	//! разгоняемся (используется в драйв-логе).
	bool ApplyDriveForce(float speedAbs)
	{
		float margin = 3.0;
		bool pushing = false;

		if (speedAbs > m_SpeedLimit + margin)
		{
			//! Превышаем — газ убираем (торможение двигателем).
			m_Car.dm_DriveThrottle = 0.0;
		}
		else if (speedAbs < m_SpeedLimit - margin)
		{
			//! Ниже цели — разгоняемся нативным газом (реальная тяга от двигателя).
			pushing = true;
			m_Car.dm_DriveThrottle = 0.6;
		}
		else
		{
			//! Накат в коридоре — газ убираем.
			m_Car.dm_DriveThrottle = 0.0;
		}

		//! Тормоз — всегда 0 (тормозим отпусканием газа); руль остаётся активным
		//! через dm_DriveActive (SetSteering в CarScript.OnInput).
		m_Car.dm_DriveBrake = 0.0;
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
	void ApplySteering(float angle, float speedAbs, float pDt)
	{
		float steerTarget = 0.0;

		if (speedAbs >= DM_DRIVE_STEER_MIN_SPEED && Math.AbsFloat(angle) >= DRIVE_STEER_ANGLE_DEADZONE)
			steerTarget = Math.Clamp(-angle / 1.57, -1.0, 1.0);

		float t = Math.Min(1.0, DM_DRIVE_WHEEL_STEER_SPEED * pDt);
		m_WheelSteer = Math.Lerp(m_WheelSteer, steerTarget, t);
		m_Car.dm_DriveSteering = m_WheelSteer;
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
	//! Возвращает первое НЕ-self попадание: пропускаем машину (obj/parent) и водителя —
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
		m_DetectTimer = m_DetectTimer + pDt;
		if (m_DetectTimer < DM_DRIVE_DETECT_INTERVAL)
			return;
		m_DetectTimer = 0.0;

		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Drive.Detect");
		#endif

		//! Высота скана — корпус машины: препятствие торчит сквозь неё, а плоская дорога
		//! ниже корпуса (террейн не ловится — groundOnly=false по умолчанию).
		float scanY = carPos[1];

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

			vector hitPos;
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

			from = to;
			i = i + 1;
		}
	}

	//! Манёвр «коробка» вокруг препятствия: направление дороги d, перпендикуляр p, проба
	//! обеих сторон, коробка [A,B,C,D,E] вместо заблокированных вейпоинтов. Нет чистой
	//! стороны → Fail (обе стороны заняты).
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

		//! 2. Проба обеих сторон (+p / -p). side = +1 / -1 / 0 (обе заняты).
		int side = ProbeClearSide(obs, p);
		if (side == 0)
		{
			#ifdef DM_BOT_DEBUG_CAR
			dmBotLog.Debug("[CAR] detour: no clear side -> fail");
			#endif
			//! TODO: отмена команды — что делать? (сообщить игроку / остаться / выйти)
			Fail();
			return;
		}

		#ifdef DM_BOT_DEBUG_CAR
		dmBotLog.Debug("[CAR] detour: obstacle=" + obs + " side=" + side);
		#endif

		//! 3. Коробка (XZ; Y через SurfaceY). sideSign = ±1.
		float sideSign = 1.0;
		if (side < 0)
			sideSign = -1.0;

		float offX = p[0] * DM_DRIVE_DETOUR_OFFSET * sideSign;
		float offZ = p[2] * DM_DRIVE_DETOUR_OFFSET * sideSign;
		float spanX = d[0] * DM_DRIVE_DETOUR_SPAN;
		float spanZ = d[2] * DM_DRIVE_DETOUR_SPAN;

		//! A = O - d*SPAN, B = A + side*OFFSET, C = O + side*OFFSET,
		//! D = O + d*SPAN + side*OFFSET, E = O + d*SPAN.
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

		//! 4. W[j-1] уже позади машины — убираем и его, иначе вход в коробку окажется
		//!    за спиной. Заменяем вейпоинты на коробку.
		bool removePrev = false;
		if (j - 1 >= 0 && CarPassedWaypoint(carPos, m_DriveRoute[j - 1]))
			removePrev = true;
		ReplaceBlock(j, removePrev, boxA, boxB, boxC, boxD, boxE);

		m_DetourActive = false;
		m_LastWaypointDist = -1.0;
	}

	//! Проба обеих сторон от препятствия: боковой райкаст из obs на ±p * OFFSET. Чистая
	//! сторона = нет попадания И земля в конце смещения без обрыва. Возвращает +1 / -1 / 0.
	int ProbeClearSide(vector obs, vector p)
	{
		float groundRef = GetGame().SurfaceY(obs[0], obs[2]);

		vector endP = Vector(obs[0] + p[0] * DM_DRIVE_DETOUR_OFFSET, obs[1], obs[2] + p[2] * DM_DRIVE_DETOUR_OFFSET);
		vector endN = Vector(obs[0] - p[0] * DM_DRIVE_DETOUR_OFFSET, obs[1], obs[2] - p[2] * DM_DRIVE_DETOUR_OFFSET);

		vector hitPos;
		bool clearP = !RaycastHits(obs, endP, DM_DRIVE_DETOUR_PROBE_RADIUS, hitPos);
		if (clearP)
			clearP = SidePassable(endP, groundRef);

		bool clearN = !RaycastHits(obs, endN, DM_DRIVE_DETOUR_PROBE_RADIUS, hitPos);
		if (clearN)
			clearN = SidePassable(endN, groundRef);

		if (clearP)
			return 1;
		if (clearN)
			return -1;
		return 0;
	}

	//! Земля в точке проходима (нет обрыва/ямы): высота SurfaceY в пределах
	//! DM_DRIVE_DETOUR_GROUND_EPS от земли под препятствием.
	bool SidePassable(vector point, float groundRef)
	{
		float groundY = GetGame().SurfaceY(point[0], point[2]);
		return Math.AbsFloat(groundY - groundRef) <= DM_DRIVE_DETOUR_GROUND_EPS;
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
