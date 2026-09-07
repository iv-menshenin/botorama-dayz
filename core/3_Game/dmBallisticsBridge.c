//! Shot state for the miss-feedback: shot origin + target distance.
class dmBotShotState
{
	vector m_Origin;
	vector m_AimDir;    // горизонтальное направление прицела (нормализовано)
	float m_TargetDist;
	float m_WindPerp;    // знаковая поперечная составляющая ветра на момент выстрела
	float m_TravelTime;  // время полёта (с)
	float m_AimPosY;    // высота точки прицела (грудь) на момент выстрела
}

//! Cross-module bridge for the bullet-drop self-learning. DayZGame.FirearmEffects
//! lives in 3_Game and cannot reference 4_World types; the player chain's 3_Game
//! classes (EntityAI/Man/Human/DayZPlayer) are all ENGINE classes and cannot be
//! modded. So the per-pawn coefficient + last-shot state live here (static maps
//! keyed by the pawn's EntityAI); 4_World reads/writes them through these statics.
class dmBallisticsBridge
{
	static ref map<EntityAI, float> s_DropCoef = new map<EntityAI, float>();
	static ref map<EntityAI, float> s_WindCoef = new map<EntityAI, float>();
	static ref map<EntityAI, ref dmBotShotState> s_LastShot = new map<EntityAI, ref dmBotShotState>();

	static float GetDropCoef(EntityAI pawn)
	{
		if (s_DropCoef.Contains(pawn))
			return s_DropCoef[pawn];
		return DM_DROP_COEF_INIT;
	}

	static float GetWindCoef(EntityAI pawn)
	{
		if (s_WindCoef.Contains(pawn))
			return s_WindCoef[pawn];
		return DM_WIND_COEF_INIT;
	}

	static void RecordShot(EntityAI pawn, vector origin, vector aimDir, float targetDist, vector wind, float travelTime, float aimPosY)
	{
		dmBotShotState st = new dmBotShotState();
		st.m_Origin = origin;
		st.m_TargetDist = targetDist;
		aimDir[1] = 0.0;
		aimDir.Normalize();
		st.m_AimDir = aimDir;
		st.m_WindPerp = wind[0] * (-aimDir[2]) + wind[2] * aimDir[0];
		st.m_TravelTime = travelTime;
		st.m_AimPosY = aimPosY;
		s_LastShot[pawn] = st;
	}

	static void ClearShot(EntityAI pawn)
	{
		s_LastShot.Remove(pawn);
	}

	static void OnImpact(EntityAI sourceEnt, bool hitEntity, vector pos, float speed)
	{
		if (!sourceEnt)
			return;
		EntityAI shooter = sourceEnt.GetHierarchyRootPlayer();
		if (!shooter)
			return;
		//! Потребляем состояние выстрела на первом ударе (попадание или промах),
		//! чтобы рикошет (повторный FirearmEffects) не считался вторым промахом.
		dmBotShotState st = s_LastShot[shooter];
		s_LastShot.Remove(shooter);
		if (!st || st.m_TargetDist <= 0.0 || hitEntity || speed < DM_DROP_MIN_IMPACT_SPEED)
			return;
		vector d = pos - st.m_Origin;
		d[1] = 0.0;
		float along = d[0] * st.m_AimDir[0] + d[2] * st.m_AimDir[2];
		if (along <= 0.0)
			return;
		vector lat = d - st.m_AimDir * along;
		float lateral = lat.Length();
		float coef = GetDropCoef(shooter);
		float slope = DM_AI_GRAVITY * st.m_TravelTime / speed;
		if (slope < 0.001)
			slope = 0.001;
		float offset = (st.m_AimPosY - pos[1]) / slope;
		float ratio = (st.m_TargetDist + offset - along) / along;
		coef = coef * (1.0 + DM_DROP_LEARN_RATE * ratio);
		if (coef < DM_DROP_COEF_MIN)
			coef = DM_DROP_COEF_MIN;
		if (coef > DM_DROP_COEF_MAX)
			coef = DM_DROP_COEF_MAX;
		s_DropCoef[shooter] = coef;
		#ifdef DM_BOT_DEBUG_BALLISTICS
		dmBotLog.Debug("[Ballistics] FEEDBACK targetDist=" + st.m_TargetDist + " along=" + along + " coef=" + coef);
		dmBotLog.Debug("[Ballistics] FEEDBACK offset=" + offset + " lateral=" + lateral + " speed=" + speed);
		#endif
		float latSigned = st.m_AimDir[0] * d[2] - st.m_AimDir[2] * d[0];
		float scale = st.m_WindPerp * st.m_TravelTime;
		if (Math.AbsFloat(scale) > 0.01)
		{
			float wc = GetWindCoef(shooter);
			wc = wc - DM_WIND_LEARN_RATE * latSigned / scale;
			if (wc < DM_WIND_COEF_MIN)
				wc = DM_WIND_COEF_MIN;
			if (wc > DM_WIND_COEF_MAX)
				wc = DM_WIND_COEF_MAX;
			s_WindCoef[shooter] = wc;
			#ifdef DM_BOT_DEBUG_BALLISTICS
			dmBotLog.Debug("[Ballistics] WIND latSigned=" + latSigned + " windPerp=" + st.m_WindPerp + " coef=" + wc);
			#endif
		}
	}
}
