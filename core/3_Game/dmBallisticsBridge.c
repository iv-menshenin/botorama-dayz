//! Shot state for the miss-feedback: shot origin + target distance.
class dmBotShotState
{
	vector m_Origin;
	float m_TargetDist;
}

//! Cross-module bridge for the bullet-drop self-learning. DayZGame.FirearmEffects
//! lives in 3_Game and cannot reference 4_World types; the player chain's 3_Game
//! classes (EntityAI/Man/Human/DayZPlayer) are all ENGINE classes and cannot be
//! modded. So the per-pawn coefficient + last-shot state live here (static maps
//! keyed by the pawn's EntityAI); 4_World reads/writes them through these statics.
class dmBallisticsBridge
{
	static ref map<EntityAI, float> s_DropCoef = new map<EntityAI, float>();
	static ref map<EntityAI, ref dmBotShotState> s_LastShot = new map<EntityAI, ref dmBotShotState>();

	static float GetDropCoef(EntityAI pawn)
	{
		if (s_DropCoef.Contains(pawn))
			return s_DropCoef[pawn];
		return DM_DROP_COEF_INIT;
	}

	static void RecordShot(EntityAI pawn, vector origin, float targetDist)
	{
		dmBotShotState st = new dmBotShotState();
		st.m_Origin = origin;
		st.m_TargetDist = targetDist;
		s_LastShot[pawn] = st;
	}

	static void ClearShot(EntityAI pawn)
	{
		s_LastShot.Remove(pawn);
	}

	static void OnImpact(EntityAI sourceEnt, bool hitEntity, vector pos)
	{
		if (!sourceEnt || hitEntity)
			return;
		EntityAI shooter = sourceEnt.GetHierarchyRootPlayer();
		if (!shooter)
			return;
		dmBotShotState st = s_LastShot[shooter];
		if (!st || st.m_TargetDist <= 0.0)
			return;
		vector d = pos - st.m_Origin;
		d[1] = 0.0;
		float hitDist = d.Length();
		if (hitDist <= 0.0)
			return;
		float coef = GetDropCoef(shooter);
		float ratio = (st.m_TargetDist - hitDist) / hitDist;
		coef = coef * (1.0 + DM_DROP_LEARN_RATE * ratio);
		if (coef < DM_DROP_COEF_MIN)
			coef = DM_DROP_COEF_MIN;
		if (coef > DM_DROP_COEF_MAX)
			coef = DM_DROP_COEF_MAX;
		s_DropCoef[shooter] = coef;
		#ifdef DM_BOT_DEBUG_BALLISTICS
		dmBotLog.Debug("[Ballistics] FEEDBACK targetDist=" + st.m_TargetDist + " hitDist=" + hitDist + " coef=" + coef);
		#endif
	}
}
