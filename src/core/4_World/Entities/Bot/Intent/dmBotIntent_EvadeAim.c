//! dmBotIntent_EvadeAim — уворот от прицела: бот бежит (спринт) к укрытию, где
//! агрессор его не видит, не отворачиваясь от агрессора (взгляд держит парный
//! HoldLook FULL). Реверс dmBotIntent_Flank: те же точки по кругу, но выбирается
//! точка, где луч «агрессор → кандидат» ЗАБЛОКИРОВАН (укрытие), а не открыт.
//!
//! Круг: центр — агрессор, радиус = max(дистанция, DM_EVADE_AIM_MIN_DIST); при
//! исчерпании свипа радиус растёт на DM_EVADE_AIM_DIST_STEP (бот бежит дальше).
//! Свип — ПОЛУКРУГ (±DM_EVADE_AIM_MAX_ANGLE=90° от направления «агрессор→бот»),
//! чтобы бот не прятался за спину агрессора.
//!
//! Завершается: (a) агрессор > DM_EVADE_AIM_END_DIST И угол прицела >
//! DM_EVADE_AIM_END_AIM_ANGLE; (b) опустил ствол И угол головы >
//! DM_EVADE_AIM_END_HEAD_ANGLE; (c) агрессор вне LOS бота; (d) бой начался
//! (GetHostileTarget != null). Пока активен — добавляет threat агрессору.
class dmBotIntent_EvadeAim : dmBotIntent_MoveTo
{
	EntityAI m_Aggressor;

	float m_BaseDir;       // yaw направления «агрессор→бот» на старте
	float m_Dist;          // радиус круга (дистанция, >= DM_EVADE_AIM_MIN_DIST)
	float m_FlankAngle;    // текущий угол свипа
	float m_NeckHeight;    // высота шеи бота над землёй
	float m_StallTimer;    // общий таймаут попытки

	float m_TickAccum;     // аккумулятор по-секундного threat
	int m_SeenShotSerial;  // последний обработанный серийный номер выстрела

	float m_StrafeSign = 1.0;
	float m_StrafeTimer = 0.0;
	float m_LogAccum = 0.0;

	void dmBotIntent_EvadeAim()
	{
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	override string GetIntentName()
	{
		return "EvadeAim";
	}

	override bool IsContinuous()
	{
		return true;
	}

	//! Взгляд на агрессора держит парный HoldLook (FULL), а не цель движения.
	override bool KeepLookAtGoal()
	{
		return false;
	}

	override float GetMoveSpeed(dmAISurvivor bot)
	{
		return 3.0; // спринт
	}

	int m_StillAggressionAcc = 0;

	// ActiveTick выполняется, если бота все еще держат под прицелом
	void ActiveTick(dmAISurvivor bot)
	{
		m_StillAggressionAcc += 1;
		if (m_StillAggressionAcc >= 5)
		{
			m_StillAggressionAcc = 0;
			UpdateEvadeDestination(bot);
		}
	}

	override void OnStart(dmAISurvivor bot)
	{
		super.OnStart(bot);

		m_StallTimer = 0.0;
		m_TickAccum = 0.0;
		m_SeenShotSerial = bot.GetShotSerial();
		m_FlankAngle = DM_EVADE_AIM_START_ANGLE;
		m_NeckHeight = DM_EYE_HEIGHT;
		m_StrafeSign = 1.0;
		m_StrafeTimer = 0.0;

		if (!m_Aggressor)
			return;

		#ifdef DM_BOT_DEBUG_EVADE
		dmBotLog.Debug("[Evade] OnStart");
		#endif

		UpdateEvadeDestination(bot);
	}

	void UpdateEvadeDestination(dmAISurvivor bot)
	{
		vector botPos = bot.GetPosition();
		vector tPos = m_Aggressor.GetPosition();
		vector d = botPos - tPos;
		d[1] = 0.0;
		m_BaseDir = d.VectorToAngles()[0];
		m_Dist = d.Length();
		if (m_Dist < DM_EVADE_AIM_MIN_DIST)
			m_Dist = DM_EVADE_AIM_MIN_DIST;

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn)
		{
			int neck = pawn.GetBoneIndexByName("Neck");
			if (neck >= 0)
			{
				vector neckPos = pawn.GetBonePositionWS(neck);
				m_NeckHeight = neckPos[1] - botPos[1];
			}
		}
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		if (!m_Aggressor)
		{
			Finish();
			return;
		}

		m_LogAccum += pDt;
		if (m_LogAccum >= 1.0)
		{
			m_LogAccum = 0.0;
			#ifdef DM_BOT_DEBUG_EVADE
			dmTarget evT = bot.FindTarget(m_Aggressor);
			dmBotLog.Debug("[Evade] tick baseDir=" + m_BaseDir + " mDist=" + m_Dist + " flank=" + m_FlankAngle + " hasPath=" + m_HasPath);
			if (evT)
				dmBotLog.Debug("[Evade] tick los=" + evT.m_HasLOS + " threat=" + evT.m_Threat);
			#endif
		}

		//! По-секундное давление: пока уворачиваемся, угроза растёт.
		m_TickAccum += pDt;
		if (m_TickAccum >= 1.0)
		{
			m_TickAccum -= 1.0;
			bot.AddThreat(m_Aggressor, DM_EVADE_AIM_RATE);
		}

		//! Выстрел агрессора (даже мимо) — попытка убийства.
		if (bot.GetLastShotEntity() == m_Aggressor && bot.GetShotSerial() != m_SeenShotSerial)
		{
			m_SeenShotSerial = bot.GetShotSerial();
			bot.AddThreat(m_Aggressor, DM_EVADE_AIM_SHOT_THREAT);
		}

		//! Конечные условия (a)-(d).
		if (CheckEndConditions(bot))
		{
			bot.SetMove(0.0, 0.0);
			Finish();
			return;
		}

		//! Таймаут всей попытки.
		m_StallTimer += pDt;
		if (m_StallTimer > DM_EVADE_AIM_STALL_TIMEOUT)
		{
			dmBotLog.Error("EvadeAim: stall timeout, abort");
			bot.SetMove(0.0, 0.0);
			Fail();
			return;
		}

		//! Свип, только пока нет активного пути.
		if (!m_HasPath)
		{
			Sweep(bot);
			if (IsFinished())
				return;
		}

		//! Укрытия не нашли — страйф вбок (удерживать дистанцию), если страйф включён.
		if (!m_HasPath)
		{
			//! Страйф: чтобы отключить, поставь DM_EVADE_AIM_STRAFE = false в constants.c.
			if (DM_EVADE_AIM_STRAFE)
			{
				m_StrafeTimer += pDt;
				if (m_StrafeTimer >= DM_EVADE_AIM_STRAFE_SWITCH)
				{
					m_StrafeTimer = 0.0;
					m_StrafeSign = -m_StrafeSign;
				}
				bot.SetMove(m_StrafeSign * 90.0, 3.0);
			}
			else
			{
				bot.SetMove(0.0, 0.0);
			}
			return;
		}

		super.OnUpdate(bot, pDt);
	}

	override void OnReachedGoal(dmAISurvivor bot, vector pos)
	{
		bot.SetMove(0.0, 0.0);

		#ifdef DM_BOT_DEBUG_EVADE
		dmBotLog.Debug("[Evade] reached goal");
		#endif

		if (CheckEndConditions(bot))
		{
			Finish();
			return;
		}

		//! Дошли до укрытия, но угроза не снята — ищем следующее.
		m_Goal = vector.Zero;
		m_HasPath = false;
		m_Path = null;
	}

	//! Конечные условия (a)-(d).
	bool CheckEndConditions(dmAISurvivor bot)
	{
		if (bot.IsInCombat())
		{
			#ifdef DM_BOT_DEBUG_EVADE
			dmBotLog.Debug("[Evade] end: (d) combat");
			#endif
			return true;   // (d) бой начался
		}

		if (!m_Aggressor || !m_Aggressor.IsAlive())
		{
			#ifdef DM_BOT_DEBUG_EVADE
			dmBotLog.Debug("[Evade] end: aggressor dead/null");
			#endif
			return true;
		}

		dmTarget t = bot.FindTarget(m_Aggressor);
		if (!t || !t.m_HasLOS)
		{
			#ifdef DM_BOT_DEBUG_EVADE
			dmBotLog.Debug("[Evade] end: (c) no LOS");
			#endif
			return true;   // (c) агрессор вне видимости
		}

		//! Прицел/голова/IsRaised — методы PlayerBase; детект «в меня целятся»
		//! ловит только игроков, но m_Aggressor хранится как EntityAI — каст.
		PlayerBase aggr = PlayerBase.Cast(m_Aggressor);
		if (!aggr)
			return false;

		vector botPos = bot.GetPosition();
		vector agPos = m_Aggressor.GetPosition();
		vector toBot = botPos - agPos;
		toBot[1] = 0.0;
		float dist = toBot.Length();
		if (dist < 0.01)
			return false;
		toBot.Normalize();
		float toBotYaw = toBot.VectorToAngles()[0];

		// (a) далеко И прицел смотрит мимо.
		if (dist > DM_EVADE_AIM_END_DIST)
		{
			vector aimDir = dmVision.GetPlayerAimDir(aggr);
			float aimYaw = aimDir.VectorToAngles()[0];
			float aimAngle = Math.AbsFloat(dmAISurvivor.AngleDiff(aimYaw, toBotYaw));
			if (aimAngle > DM_EVADE_AIM_END_AIM_ANGLE)
			{
				#ifdef DM_BOT_DEBUG_EVADE
				dmBotLog.Debug("[Evade] end: (a) far + aim away");
				#endif
				return true;
			}
		}

		// (b) опустил ствол И голова отвернулась.
		if (!aggr.IsRaised())
		{
			vector headDir = dmVision.GetPlayerHeadDir(aggr);
			float headYaw = headDir.VectorToAngles()[0];
			float headAngle = Math.AbsFloat(dmAISurvivor.AngleDiff(headYaw, toBotYaw));
			if (headAngle > DM_EVADE_AIM_END_HEAD_ANGLE)
			{
				#ifdef DM_BOT_DEBUG_EVADE
				dmBotLog.Debug("[Evade] end: (b) lowered + head away");
				#endif
				return true;
			}
		}

		return false;
	}

	//! Свип полукруга; при исчерпании радиус растёт на DM_EVADE_AIM_DIST_STEP.
	void Sweep(dmAISurvivor bot)
	{
		m_FlankAngle += DM_EVADE_AIM_ANGLE_STEP;
		if (m_FlankAngle > DM_EVADE_AIM_MAX_ANGLE)
		{
			if (m_Dist >= DM_EVADE_AIM_MAX_DIST)
			{
				m_FlankAngle = DM_EVADE_AIM_START_ANGLE;
				m_Dist = DM_EVADE_AIM_MAX_DIST;
				return;
			}
			m_Dist = m_Dist + DM_EVADE_AIM_DIST_STEP;
			m_FlankAngle = DM_EVADE_AIM_START_ANGLE;
		}

		vector candidate;
		if (FindCoverCandidate(bot, 1, candidate))
		{
			#ifdef DM_BOT_DEBUG_EVADE
			dmBotLog.Debug("[Evade] cover found side=R angle=" + m_FlankAngle + " cand=" + candidate);
			#endif
			m_Goal = candidate;
			RePath(bot);
			return;
		}
		if (FindCoverCandidate(bot, -1, candidate))
		{
			#ifdef DM_BOT_DEBUG_EVADE
			dmBotLog.Debug("[Evade] cover found side=L angle=" + m_FlankAngle + " cand=" + candidate);
			#endif
			m_Goal = candidate;
			RePath(bot);
			return;
		}
	}

	//! Кандидат-укрытие: точка на круге с navmesh-путём, на земле, и луч
	//! «агрессор (голова) → кандидат (шея бота)» ЗАБЛОКИРОВАН.
	bool FindCoverCandidate(dmAISurvivor bot, int sign, out vector candidate)
	{
		vector tPos = m_Aggressor.GetPosition();

		float yaw = m_BaseDir + (sign * m_FlankAngle);
		vector angles = Vector(yaw, 0.0, 0.0);
		vector dir = angles.AnglesToVector();
		vector cand = tPos + dir * m_Dist;

		ref array<vector> path = new array<vector>();
		if (!bot.FindPathTo(cand, path))
			return false;
		if (path.Count() == 0)
			return false;

		vector p = path[path.Count() - 1];

		float groundY = GetGame().SurfaceY(p[0], p[2]);
		if (Math.AbsFloat(p[1] - groundY) > DM_EVADE_AIM_MAX_SURFACE_DELTA)
			return false;

		vector rayFrom = GetHeadPosition(tPos);
		vector rayTo = Vector(p[0], groundY + m_NeckHeight, p[2]);

		if (!IsBlocked(rayFrom, rayTo))
			return false;

		candidate = p;
		return true;
	}

	//! Голова агрессора (фолбэк: ноги + высота глаз).
	vector GetHeadPosition(vector fallbackPos)
	{
		int hb = -1;
		Human human = Human.Cast(m_Aggressor);
		if (human)
			hb = human.GetBoneIndexByName("Head");
		vector end;
		if (hb >= 0)
			end = m_Aggressor.GetBonePositionWS(hb);
		else
			end = fallbackPos + Vector(0.0, DM_EYE_HEIGHT, 0.0);
		return end;
	}

	//! True, когда луч от агрессора до кандидата перекрыт (укрытие). Агрессор игнорируется.
	bool IsBlocked(vector from, vector to)
	{
		RaycastRVParams rp = new RaycastRVParams(from, to, m_Aggressor);
		rp.sorted = true;
		rp.type = ObjIntersectView;
		rp.flags = CollisionFlags.NEARESTCONTACT;

		array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
		if (!DayZPhysics.RaycastRVProxy(rp, hits) || hits.Count() == 0)
			return false;

		return true;
	}
}
