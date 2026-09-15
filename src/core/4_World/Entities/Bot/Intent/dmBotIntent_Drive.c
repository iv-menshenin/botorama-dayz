//! dmBotIntent_Drive — бот садится за руль и едет по ДОРОГЕ к точке назначения.
//!
//! Наследует dmBotIntent_GetInVehicle и переиспользует весь lifecycle посадки
//! (walk к двери → открыть дверь → GetInVehicle → сел). После посадки переводит
//! канал на DRIVE и ведёт машину: толчок импульсом (dBodyApplyImpulseAt в точке
//! двигателя) + нативный газ/тормоз/передачи + боковой рулевой импульс + нативный
//! руль (SetSteering). Маршрут — дорожный navmesh-путь (FindRoadPathTo), вейпоинт
//! за вейпоинтом. Graceful-завершение глушит двигатель (StopCar) и высаживает бота
//! штатным выходом GetInVehicle.
class dmBotIntent_Drive : dmBotIntent_GetInVehicle
{
	//! Уклон: множитель силы толчка при подъёме (аналог SLOPE_FORCE_FACTOR).
	static const float DRIVE_SLOPE_FACTOR = 8.0;
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

	//! Текущее значение руля (SetSteering), сглаживается.
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

		//! 2. Дорожный маршрут (однократно).
		if (!m_HasRoadPath)
		{
			m_RoadPath = new array<vector>();
			if (!bot.FindRoadPathTo(m_Car.GetPosition(), m_Destination, m_RoadPath) || m_RoadPath.Count() == 0)
			{
				dmBotLog.Error("Drive: нет дорожного маршрута к " + m_Destination + ", abort");
				Fail();
				return;
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
	//! сейчас толкаем (газ) — для детекции застревания.
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
			ApplyBrakeImpulse(brakeIntensity, speedSigned, carDir);
		}
		else if (speedAbs < m_SpeedLimit - margin)
		{
			//! Ниже цели — разгоняемся.
			throttle = 0.6;
			pushing = true;
			ApplyPushImpulse(throttle, carDir, carPitch);
		}
		//! Иначе — накат в коридоре (газ/тормоз = 0).

		m_Car.SetThrottle(throttle);
		m_Car.SetBrake(brake);

		//! Восстановление от отката назад (не в реверсе).
		if (!m_Reverse)
			ApplyRollRecovery(speedSigned, carDir);

		return pushing;
	}

	//! Толчок вперёд (или назад в реверсе) с компенсацией уклона.
	void ApplyPushImpulse(float throttleVal, vector carDir, float carPitch)
	{
		float bodyMass = dBodyGetMass(m_Car);
		if (bodyMass <= 0.0)
			return;

		vector impulseDir = carDir;
		if (m_Reverse)
			impulseDir = -impulseDir;

		//! Компенсация уклона: в горку толкаем сильнее.
		float sinPitch = carPitch;
		float slopeMultiplier = 1.0;
		if (sinPitch > 0.05)
		{
			slopeMultiplier = 1.0 + sinPitch * DRIVE_SLOPE_FACTOR;
			slopeMultiplier = Math.Clamp(slopeMultiplier, 1.0, 3.5);
		}

		float forceMag = bodyMass * DM_DRIVE_PUSH_FORCE * throttleVal * slopeMultiplier;
		if (forceMag > bodyMass * 1000.0)
			forceMag = bodyMass * 1000.0;

		vector impulse = impulseDir * forceMag * (1.0 / bodyMass);
		vector applyPoint = m_Car.ModelToWorld(m_Car.GetEnginePos());
		dBodyApplyImpulseAt(m_Car, impulse, applyPoint);
	}

	//! Торможение импульсом против текущей скорости.
	void ApplyBrakeImpulse(float brakeIntensity, float speedSigned, vector carDir)
	{
		float bodyMass = dBodyGetMass(m_Car);
		if (bodyMass <= 0.0)
			return;

		vector impulseDir = carDir;
		if (speedSigned > 0.5)
			impulseDir = -impulseDir;  // едем вперёд → тормоз назад
		else if (speedSigned < -0.5)
			impulseDir = carDir;       // едем назад → тормоз вперёд
		else
			return;                    // скорость ~0, тормозить нечего

		float speedMS = speedSigned / 3.6;
		float maxPossibleImpulse = Math.AbsFloat(speedMS) * bodyMass * 100.0;
		float desiredForce = bodyMass * DM_DRIVE_BRAKE_FORCE * brakeIntensity;
		float forceMag = Math.Min(desiredForce, maxPossibleImpulse);

		vector impulse = impulseDir * forceMag * (1.0 / bodyMass);
		vector applyPoint = m_Car.ModelToWorld(m_Car.GetEnginePos());
		dBodyApplyImpulseAt(m_Car, impulse, applyPoint);
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
			int targetGear = CarGear.NEUTRAL;
			if (m_Reverse)
				targetGear = CarGear.REVERSE;
			else if (speedAbs < 2.0)
				targetGear = CarGear.NEUTRAL;
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

	//! Руль: боковой импульс по знаку угла + нативный SetSteering (колёса визуально).
	void ApplySteering(float angle, float speedAbs, vector carDir, float pDt)
	{
		float steerTarget = 0.0;

		if (speedAbs >= DM_DRIVE_STEER_MIN_SPEED && Math.AbsFloat(angle) >= DRIVE_STEER_ANGLE_DEADZONE)
		{
			int direction = 1;
			if (angle < 0.0)
				direction = -1;
			ApplySideImpulse(direction, DM_DRIVE_SIDE_IMPULSE, carDir);
			steerTarget = Math.Clamp(angle / 3.14159265, -1.0, 1.0);
		}

		float t = Math.Min(1.0, DM_DRIVE_WHEEL_STEER_SPEED * pDt);
		m_WheelSteer = Math.Lerp(m_WheelSteer, steerTarget, t);
		m_Car.SetSteering(m_WheelSteer);
	}

	//! Боковой рулевой импульс (порт AutoCarSteering.ApplySideImpulse).
	void ApplySideImpulse(int direction, float forceMagnitude, vector carDir)
	{
		vector carPos = m_Car.GetPosition();
		vector applyPoint = carPos + carDir * 1.5;

		vector sideDir;
		sideDir[0] = -carDir[2];
		sideDir[1] = 0.0;
		sideDir[2] = carDir[0];
		sideDir.Normalize();

		vector finalForceDir = sideDir * (-(float)direction);
		vector impulseVector = finalForceDir * forceMagnitude;
		dBodyApplyImpulseAt(m_Car, impulseVector, applyPoint);
	}

	//! Застревание: не едем, но толкаем → считаем тики; порог → реверс.
	void TickStuck(float speedAbs, bool pushing)
	{
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
			m_StuckCounter = 0;
		}
	}

	//! Graceful: глушим двигатель, тормозим, включаем авто-тормоз без водителя.
	//! Гейт по m_EngineStarted: не трогаем машину, в которую бот так и не сел
	//! (Fail во время WALK-фазы посадки).
	void StopCar()
	{
		if (!m_Car)
			m_Car = CarScript.Cast(m_Transport);
		if (m_Car && m_EngineStarted)
		{
			m_Car.SetThrottle(0.0);
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
