//! dmBotIntent_Drive — бот садится за руль и едет по ДОРОГЕ к точке назначения.
//!
//! Наследует dmBotIntent_GetInVehicle и переиспользует весь lifecycle посадки
//! (walk к двери → открыть дверь → GetInVehicle → сел). После посадки переводит
//! канал на DRIVE и ведёт машину: замкнутый контур скорости (толчок/тормоз
//! импульсом = ошибка×kp, кэп по дельта-V, в ЦМ) + боковой рулевой импульс
//! (angle×kp, на носу) + нативный газ/руль через поля dm_Drive* машины (применяются
//! в CarScript.OnInput). Маршрут — дорожный navmesh-путь (FindRoadPathTo) с
//! fallback'ом на пеший путь при усечении, вейпоинт за вейпоинтом. Graceful-
//! завершение глушит двигатель (StopCar) и высаживает бота штатным выходом.
class dmBotIntent_Drive : dmBotIntent_GetInVehicle
{
	//! Уклон: малый множитель толчка (1.0 + sinPitch * factor), не домножается на массу.
	static const float DRIVE_SLOPE_FACTOR = 2.0;
	//! Откат назад: множитель восстановления (аналог ROLL_RECOVERY_FACTOR).
	static const float DRIVE_ROLL_RECOVERY_FACTOR = 0.5;
	//! Откат назад: макс. сила восстановления на кг массы.
	static const float DRIVE_ROLL_RECOVERY_MAX_PER_KG = 0.02;
	//! Руль: мёртвая зона угла (рад), ниже — не рулим (анти-джиттер).
	static const float DRIVE_STEER_ANGLE_DEADZONE = 0.01;

	//! Конечная точка назначения (выставляет владелец/команда до OnStart).
	vector m_Destination;

	//! Дорожный маршрут (вейпоинты по ROADWAY-навмеш).
	ref array<vector> m_RoadPath;
	int m_RoadPathIdx = 0;
	bool m_HasRoadPath = false;

	//! Машина (получаем из m_Transport после посадки).
	CarScript m_Car;

	//! Флаг, что EngineStart() уже вызван.
	bool m_EngineStarted = false;

	//! Текущий лимит скорости (км/ч, сглаживается).
	float m_SpeedLimit = DM_DRIVE_MAX_SPEED_STRAIGHT;

	//! Застревание: счётчик тиков, после порога — реверс.
	int m_StuckCounter = 0;
	bool m_Reverse = false;

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
		//! m_Destination уже выставлен владельцем (как m_Transport/m_Seat);
		//! super.OnStart наследует точку входа у двери (m_Goal) и walk к ней.
		super.OnStart(bot);

		m_RoadPath = new array<vector>();
		m_RoadPathIdx = 0;
		m_HasRoadPath = false;
		m_Car = null;
		m_EngineStarted = false;
		m_SpeedLimit = DM_DRIVE_MAX_SPEED_STRAIGHT;
		m_StuckCounter = 0;
		m_Reverse = false;
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

	//! Один тик вождения: цель → скорость → газ/тормоз/толчок → передача → руль.
	void TickDriving(dmAISurvivor bot, float pDt)
	{
		m_Car = CarScript.Cast(m_Transport);
		if (!m_Car)
		{
			dmBotLog.Error("Drive: транспорт не машина (CarScript), abort");
			Fail();
			return;
		}

		//! 1. Старт двигателя (однократно; напрямую — ванильный ActionStartEngine
		//!    режет серверных ботов).
		if (!m_EngineStarted)
		{
			m_Car.EngineStart();
			m_Car.SetBrake(0.0);
			m_Car.SetHandbrake(0.0);
			m_Car.SetBrakesActivateWithoutDriver(false);
			m_EngineStarted = true;
		}

		//! 2. Дорожный маршрут (однократно). При усечённой дороге (последний
		//! вейпоинт далеко от назначения — ROADWAY-navmesh фрагментирован)
		//! fallback на пеший путь.
		if (!m_HasRoadPath)
		{
			m_RoadPath = new array<vector>();
			bool roadOk = bot.FindRoadPathTo(m_Car.GetPosition(), m_Destination, m_RoadPath);
			float lastDist = -1.0;
			if (roadOk && m_RoadPath.Count() > 0)
				lastDist = vector.Distance(m_RoadPath[m_RoadPath.Count() - 1], m_Destination);

			//! TODO: временный fallback, пока не выясним фрагментацию ROADWAY-navmesh.
			if (!roadOk || m_RoadPath.Count() == 0 || lastDist > DM_DRIVE_ROAD_FALLBACK_DIST)
			{
				#ifdef DM_BOT_DEBUG_CAR
				dmBotLog.Debug("[CAR] Drive: road path truncated, fallback to walk path");
				#endif
				if (!bot.FindPathTo(m_Destination, m_RoadPath) || m_RoadPath.Count() == 0)
				{
					dmBotLog.Error("Drive: нет маршрута (road+walk) к " + m_Destination + ", abort");
					Fail();
					return;
				}
			}
			m_RoadPathIdx = 0;
			m_HasRoadPath = true;
		}

		vector carPos = m_Car.GetPosition();
		vector target = m_RoadPath[m_RoadPathIdx];
		vector toTarget = target - carPos;
		toTarget[1] = 0.0;
		float dist = toTarget.Length();

		//! 3-4. Вейпоинт достигнут → следующий.
		if (dist < DM_DRIVE_WAYPOINT_REACH)
		{
			m_RoadPathIdx = m_RoadPathIdx + 1;
			if (m_RoadPathIdx >= m_RoadPath.Count())
			{
				Finish();
				return;
			}
			target = m_RoadPath[m_RoadPathIdx];
			toTarget = target - carPos;
			toTarget[1] = 0.0;
			dist = toTarget.Length();
		}

		//! 5. Конец маршрута / достигли назначения.
		if (vector.Distance(carPos, m_Destination) < DM_DRIVE_REACH)
		{
			Finish();
			return;
		}

		//! Направление машины (горизонталь) и курс к вейпоинту.
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

		//! Угол до вейпоинта (знак через cross-произведение).
		float cross = carDir[0] * toDirZ - carDir[2] * toDirX;
		float dot = carDir[0] * toDirX + carDir[2] * toDirZ;
		float angle = Math.Atan2(cross, dot);

		float speedAbs = m_Car.GetSpeedometerAbsolute();
		float speedSigned = m_Car.GetSpeedometer();

		//! 6. Целевая скорость: ниже в повороте; сглаживаем.
		float limit = DM_DRIVE_MAX_SPEED_STRAIGHT;
		if (Math.AbsFloat(angle) > DM_DRIVE_TURN_ANGLE_THRESHOLD)
			limit = DM_DRIVE_MAX_SPEED_TURNING;
		m_SpeedLimit = Math.Lerp(m_SpeedLimit, limit, DM_DRIVE_SPEED_SMOOTH);

		//! 7. Газ/тормоз/толчок.
		bool pushing = ApplyDriveForce(speedAbs, speedSigned, carDir, carDirRaw[1]);

		//! 8. Передачи (по скорости, нативно).
		ShiftGear(speedAbs);

		//! 9. Руль/боковой импульс.
		ApplySteering(angle, speedAbs, carDir, pDt);

		//! 10. Застревание → реверс.
		TickStuck(speedAbs, pushing);

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

	//! Газ/тормоз/толчок по отклонению от целевой скорости. Возвращает true, если
	//! сейчас толкаем (газ) — для детекции застревания. Нативный газ/тормоз пишем
	//! в поля машины dm_Drive* (применяются в CarScript.OnInput), а не зовём
	//! SetThrottle/SetBrake напрямую (из OnUpdate они мёртвые).
	bool ApplyDriveForce(float speedAbs, float speedSigned, vector carDir, float carPitch)
	{
		float margin = 3.0;
		float throttle = 0.0;
		float brake = 0.0;
		bool pushing = false;

		if (speedAbs > m_SpeedLimit + margin)
		{
			//! Превышаем — тормозим.
			float brakeIntensity = Math.InverseLerp(m_SpeedLimit + margin, m_SpeedLimit + 15.0, speedAbs);
			brakeIntensity = Math.Clamp(brakeIntensity, 0.2, 0.6);
			brake = brakeIntensity;
			ApplyBrakeImpulse(speedSigned, carDir);
		}
		else if (speedAbs < m_SpeedLimit - margin)
		{
			//! Ниже цели — разгоняемся.
			throttle = 0.6;
			pushing = true;
			ApplyPushImpulse(speedSigned, carDir, carPitch);
		}
		//! Иначе — накат в коридоре (газ/тормоз = 0).

		m_Car.dm_DriveThrottle = throttle;
		m_Car.dm_DriveBrake = brake;
		m_Car.dm_DriveActive = true;

		//! Восстановление от отката назад (не в реверсе).
		if (!m_Reverse)
			ApplyRollRecovery(speedSigned, carDir);

		return pushing;
	}

	//! Замкнутый контур скорости: толчок = ошибка × kp (кэп по импульсу), в ЦМ.
	//! Знак реверса закодирован в signed-лимите: в реверсе лимит берём со знаком
	//! минус → при недостаточной задней скорости dv отрицателен → толкаем назад.
	//! Точка приложения — ЦМ (GetPosition), НЕ точка двигателя (смещена от ЦМ и
	//! создаёт крутящий момент).
	void ApplyPushImpulse(float speedSigned, vector carDir, float carPitch)
	{
		float limit = m_SpeedLimit;
		if (m_Reverse)
			limit = -m_SpeedLimit;

		//! Ошибка скорости (км/ч, знаковая) → импульс (единицы dBodyApplyImpulseAt)
		//! с кэпом. Без конвертации в м/с: dBodyApplyImpulseAt даёт ~2 ед. импульса
		//! на км/ч скорости (эмпирически).
		float speedErr = limit - speedSigned;
		float dv = speedErr * DM_DRIVE_SPEED_KP;

		//! Малый множитель уклона (не домножаем на массу — это был источник
		//! нестабильности).
		float slopeMultiplier = 1.0 + carPitch * DRIVE_SLOPE_FACTOR;
		slopeMultiplier = Math.Clamp(slopeMultiplier, 0.5, 2.0);
		dv = dv * slopeMultiplier;

		dv = Math.Clamp(dv, -DM_DRIVE_SPEED_MAX_IMPULSE, DM_DRIVE_SPEED_MAX_IMPULSE);

		vector impulse = carDir * dv;
		dBodyApplyImpulseAt(m_Car, impulse, m_Car.GetPosition());
	}

	//! Замкнутый контур торможения (зеркально толчку): ошибка = speedSigned - лимит,
	//! импульс против движения (в ЦМ). Знак учитывает реверс: в реверсе при переизбытке
	//! задней скорости speedSigned сильно отрицателен → dv отрицателен → -carDir×dv
	//! даёт вперёд (против заднего хода). Прежний maxPossibleImpulse = |v|×mass×100
	//! был фикс-капом и больше не нужен.
	void ApplyBrakeImpulse(float speedSigned, vector carDir)
	{
		float speedErr = speedSigned - m_SpeedLimit;
		float dv = speedErr * DM_DRIVE_BRAKE_KP;
		dv = Math.Clamp(dv, -DM_DRIVE_BRAKE_MAX_IMPULSE, DM_DRIVE_BRAKE_MAX_IMPULSE);

		vector impulse = carDir * (-dv);
		dBodyApplyImpulseAt(m_Car, impulse, m_Car.GetPosition());
	}

	//! Коррекция отката назад (машина катится назад без реверса) — толкаем вперёд.
	void ApplyRollRecovery(float speedSigned, vector carDir)
	{
		if (speedSigned >= -0.5)
			return;

		float bodyMass = dBodyGetMass(m_Car);
		if (bodyMass <= 0.0)
			return;

		float speedBackwards = Math.AbsFloat(speedSigned);
		float recoveryMag = speedBackwards * bodyMass * DRIVE_ROLL_RECOVERY_FACTOR;
		float maxAllowed = bodyMass * DRIVE_ROLL_RECOVERY_MAX_PER_KG;
		if (recoveryMag > maxAllowed)
			recoveryMag = maxAllowed;

		vector impulse = carDir * recoveryMag * (1.0 / bodyMass);
		vector applyPoint = m_Car.ModelToWorld(m_Car.GetEnginePos());
		dBodyApplyImpulseAt(m_Car, impulse, applyPoint);
	}

	//! Передачи: МКПП по скорости, АКПП по режиму D/R/N.
	void ShiftGear(float speedAbs)
	{
		CarGearboxType type = m_Car.GearboxGetType();
		if (type == CarGearboxType.MANUAL)
		{
			//! Машина ВСЕГДА в передаче во время вождения: старт с места — сразу
			//! FIRST (0..15), иначе классика «курица-яйцо» — прежняя ветка
			//! speedAbs<2.0→NEUTRAL выбивала машину в нейтраль, а на 1-ю она
			//! переключалась только при speedAbs>=15, которую без 1-й не набрать.
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

	//! Руль: боковой импульс (angle×kp) + нативный руль в поле машины (применяется
	//! в CarScript.OnInput, колёса визуально). steerTarget в <-1,1> от угла (±90°).
	void ApplySteering(float angle, float speedAbs, vector carDir, float pDt)
	{
		float steerTarget = 0.0;

		if (speedAbs >= DM_DRIVE_STEER_MIN_SPEED && Math.AbsFloat(angle) >= DRIVE_STEER_ANGLE_DEADZONE)
		{
			ApplySideImpulse(angle, carDir);
			steerTarget = Math.Clamp(angle / 1.57, -1.0, 1.0);
		}

		float t = Math.Min(1.0, DM_DRIVE_WHEEL_STEER_SPEED * pDt);
		m_WheelSteer = Math.Lerp(m_WheelSteer, steerTarget, t);
		m_Car.dm_DriveSteering = m_WheelSteer;
	}

	//! Боковой рулевой импульс: толкаем нос ВЛЕВО при angle>0 (цель слева). sideDir
	//! = (-carDir[2],0,carDir[0]) = ЛЕВО; impulse = sideDir × clamp(angle×kp). БЕЗ
	//! инверсии знака (прежний ×(-direction) уводил от цели). Приложить на носу.
	void ApplySideImpulse(float angle, vector carDir)
	{
		vector sideDir;
		sideDir[0] = -carDir[2];
		sideDir[1] = 0.0;
		sideDir[2] = carDir[0];
		sideDir.Normalize();

		float steerForce = Math.Clamp(angle * DM_DRIVE_STEER_KP, -DM_DRIVE_STEER_MAX_IMPULSE, DM_DRIVE_STEER_MAX_IMPULSE);
		vector impulse = sideDir * steerForce;

		vector carPos = m_Car.GetPosition();
		vector applyPoint = carPos + carDir * 1.5;
		dBodyApplyImpulseAt(m_Car, impulse, applyPoint);
	}

	//! Застревание: не едем, но толкаем → считаем тики; порог → реверс. Реверс
	//! ограничен по длительности (DM_DRIVE_REVERSE_MAX_TICKS) и имеет грейс-период
	//! после флипа (m_StuckCounter=20), иначе машина оседает на ~0.9 км/ч и реверс
	//! залипает навсегда.
	void TickStuck(float speedAbs, bool pushing)
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

		if (speedAbs < 0.5 && pushing)
		{
			m_StuckCounter = m_StuckCounter + 1;
		}
		else
		{
			m_StuckCounter = 0;
		}

		if ((float)m_StuckCounter > DM_DRIVE_STUCK_THRESHOLD)
		{
			m_Reverse = !m_Reverse;
			m_StuckCounter = 20;
			if (!m_Reverse)
				m_ReverseTicks = 0;
		}
	}

	//! Graceful: гасим поля ввода (OnInput больше не прикладывает газ/руль),
	//! глушим двигатель, тормозим, включаем авто-тормоз без водителя. Гейт по
	//! m_EngineStarted: не трогаем машину, в которую бот так и не сел (Fail во
	//! время WALK-фазы посадки).
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
