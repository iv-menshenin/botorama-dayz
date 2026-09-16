//! dmBotIntent_Drive — бот садится за руль и едет по маршруту из точек.
//!
//! Наследует dmBotIntent_GetInVehicle и переиспользует весь lifecycle посадки
//! (walk к двери → открыть дверь → GetInVehicle → сел). После посадки переводит
//! канал на DRIVE и ведёт машину нативным приводом: газ по отклонению от целевой
//! скорости (dm_DriveThrottle через SetThrottle в CarScript.OnInput) + передачи
//! ShiftTo (вперёд по скорости) + нативный руль SetSteering (через
//! dm_DriveSteering) — поворот делает нативный руль, как у реальной машины.
//! Маршрут — явный список точек (m_DriveRoute, выставляет владелец до OnStart), точка
//! за точкой; застревание детектится по прогрессу дистанции (TickStuck) с реверсом.
//! Graceful-завершение глушит двигатель (StopCar) и высаживает бота штатным выходом.
class dmBotIntent_Drive : dmBotIntent_GetInVehicle
{
	//! Руль: мёртвая зона угла (рад), ниже — не рулим (анти-джиттер).
	static const float DRIVE_STEER_ANGLE_DEADZONE = 0.01;

	//! Маршрут из точек (выставляет владелец/команда до OnStart).
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

	//! Последнее время драйв-лога (троттлинг ~2 c).
	float m_LastDriveLogTime = 0.0;

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
		//! m_DriveRoute уже выставлен владельцем (как m_Transport/m_Seat);
		//! super.OnStart наследует точку входа у двери (m_Goal) и walk к ней.
		super.OnStart(bot);

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

		//! 1. Пустой маршрут — нечего вести (фейлим сразу, до пауз старта).
		if (!m_DriveRoute || m_DriveRoute.Count() == 0)
		{
			dmBotLog.Error("Drive: маршрут пуст, abort");
			Fail();
			return;
		}

		//! 2. Пауза после посадки: сидим, двигатель ещё не заводим (газ/руль не трогаем).
		m_SeatedFor += pDt;
		if (m_SeatedFor < DM_DRIVE_START_DELAY)
			return;

		//! 3. Старт двигателя (однократно; напрямую — ванильный ActionStartEngine
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

		//! 4. Прогрев двигателя: стоим, не трогаемся.
		m_WarmupFor += pDt;
		if (m_WarmupFor < DM_DRIVE_ENGINE_WARMUP)
			return;

		vector carPos = m_Car.GetPosition();
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

		//! 3-4. Промежуточная точка считается пройденной, если корпус заехал на неё
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

		//! 5. Конец маршрута / достигли последней точки (гэп DM_DRIVE_REACH).
		if (vector.Distance(carPos, m_DriveRoute[m_DriveRoute.Count() - 1]) < DM_DRIVE_REACH)
		{
			Finish();
			return;
		}

		float speedAbs = m_Car.GetSpeedometerAbsolute();

		//! 6. Целевая скорость: ниже в повороте; сглаживаем.
		float limit = DM_DRIVE_MAX_SPEED_STRAIGHT;
		if (Math.AbsFloat(angle) > DM_DRIVE_TURN_ANGLE_THRESHOLD)
			limit = DM_DRIVE_MAX_SPEED_TURNING;
		m_SpeedLimit = Math.Lerp(m_SpeedLimit, limit, DM_DRIVE_SPEED_SMOOTH);

		//! 7. Нативный газ/тормоз (dm_DriveThrottle) + передачи.
		bool pushing = ApplyDriveForce(speedAbs);

		//! 8. Передачи (вперёд по скорости).
		ShiftGear(speedAbs);

		//! 9. Руль (нативный).
		ApplySteering(angle, speedAbs, pDt);

		//! 10. Застревание → реверс.
		TickStuck(dist);

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
	//! Поворот делает нативный руль — боковой импульс не нужен (на низком
	//! сцеплении он толкал кузов поперёк вместо вращения).
	void ApplySteering(float angle, float speedAbs, float pDt)
	{
		float steerTarget = 0.0;

		if (speedAbs >= DM_DRIVE_STEER_MIN_SPEED && Math.AbsFloat(angle) >= DRIVE_STEER_ANGLE_DEADZONE)
			steerTarget = Math.Clamp(angle / 1.57, -1.0, 1.0);

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
