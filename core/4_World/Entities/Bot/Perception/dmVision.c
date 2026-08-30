//! dmVision — perception: server-side scan for visible threats around the bot.
//!
//! Two independent cadences: an expensive registry scan (classify + distance/FOV/
//! LOS + RememberTarget) runs every DM_PERCEPTION_BOX_INTERVAL, while a cheap
//! visibility re-check (LOS for every remembered target) runs every
//! DM_PERCEPTION_INTERVAL. The registry scan merges results into the bot's target
//! memory (m_Targets) via BeginTargetScan + RememberTarget; the visibility pass
//! updates m_HasLOS/m_LastPosition/m_LastContact and forgets stale targets.
//!
//! Candidates come from the global dmEntityRegistry (zombies/animals/players that
//! self-registered in their constructor), not from a spatial box query. The registry
//! is cleaned of dead/null entities at the start of every scan.

class dmVision
{
	//! Accumulated time toward the next visibility re-check (LOS).
	float m_Accum = 0.0;

	//! Accumulated time toward the next registry scan.
	float m_BoxAccum = 0.0;

	//! Cached head-bone index of the bot's pawn (-1 = not resolved yet).
	int m_HeadBone = -1;

	//! Accumulate frame time: registry scan every DM_PERCEPTION_BOX_INTERVAL,
	//! visibility re-check every DM_PERCEPTION_INTERVAL.
	void Update(dmAISurvivor bot, float pDt)
	{
		m_Accum += pDt;
		m_BoxAccum += pDt;
		if (m_BoxAccum >= DM_PERCEPTION_BOX_INTERVAL) { m_BoxAccum = 0.0; ScanBox(bot); }
		if (m_Accum >= DM_PERCEPTION_INTERVAL) { m_Accum = 0.0; UpdateVisibility(bot); }
	}

	//! Snapshot the visible threats: registry -> classify -> distance/FOV/LOS -> targets.
	void ScanBox(dmAISurvivor bot)
	{
		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;

		dmEntityRegistry.Cleanup();

		vector botPos = pawn.GetPosition();

		//! Look direction (body + head). Prefer the head bone forward (includes the
		//! head turn); fall back to the body direction when the bone can't resolve.
		int hb = m_HeadBone;
		if (hb < 0)
		{
			hb = pawn.GetBoneIndexByName("Head");
			m_HeadBone = hb;
		}
		vector lookDir;
		if (hb >= 0)
		{
			vector transform[4];
			pawn.GetBoneTransformWS(hb, transform);
			lookDir = transform[1];
		}
		else
		{
			lookDir = pawn.GetDirection();
		}
		lookDir[1] = 0.0;
		lookDir.Normalize();

		bot.BeginTargetScan();

		ScanPlayers(bot, pawn, botPos, lookDir);
		ScanZombies(bot, pawn, botPos, lookDir);
		ScanAnimals(bot, pawn, botPos, lookDir);

		bot.ForgetStaleTargets(DM_TARGET_FORGET_TIME);

		#ifdef DM_BOT_DEBUG_VISION
		dmBotLog.Debug("[Vision] scan: targets=" + bot.GetTargets().Count());
		dmBotLog.Debug("[Vision] registry: zombies=" + dmEntityRegistry.GetZombies().Count());
		dmBotLog.Debug("[Vision] registry: players=" + dmEntityRegistry.GetPlayers().Count() + " animals=" + dmEntityRegistry.GetAnimals().Count());
		#endif
	}

	//! Consider every registered player (incl. other bots) within the player radius.
	private void ScanPlayers(dmAISurvivor bot, PlayerBase pawn, vector botPos, vector lookDir)
	{
		array<PlayerBase> players = dmEntityRegistry.GetPlayers();
		EntityAI follow = bot.GetFollowTarget();
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase p = players[i];
			if (!p)
				continue;
			bool friendly = (p == follow);
			ConsiderEntity(bot, pawn, botPos, lookDir, p, DM_TARGET_THREAT_PLAYER, DM_TARGET_ATTRACT_PLAYER, friendly, DM_PERCEPTION_PLAYER_RADIUS);
		}
	}

	//! Consider every registered zombie within the creature radius.
	private void ScanZombies(dmAISurvivor bot, PlayerBase pawn, vector botPos, vector lookDir)
	{
		array<ZombieBase> zombies = dmEntityRegistry.GetZombies();
		int i;
		for (i = 0; i < zombies.Count(); i++)
		{
			ZombieBase z = zombies[i];
			if (!z)
				continue;
			ConsiderEntity(bot, pawn, botPos, lookDir, z, DM_TARGET_THREAT_ZOMBIE, DM_TARGET_ATTRACT_ZOMBIE, false, DM_PERCEPTION_CREATURE_RADIUS);
		}
	}

	//! Consider every registered animal within the creature radius.
	private void ScanAnimals(dmAISurvivor bot, PlayerBase pawn, vector botPos, vector lookDir)
	{
		array<AnimalBase> animals = dmEntityRegistry.GetAnimals();
		int i;
		for (i = 0; i < animals.Count(); i++)
		{
			AnimalBase a = animals[i];
			if (!a)
				continue;
			ConsiderEntity(bot, pawn, botPos, lookDir, a, DM_TARGET_THREAT_ANIMAL, DM_TARGET_ATTRACT_ANIMAL, false, DM_PERCEPTION_CREATURE_RADIUS);
		}
	}

	//! Distance/FOV/LOS gate for a single candidate; merges it into target memory.
	private void ConsiderEntity(dmAISurvivor bot, PlayerBase pawn, vector botPos, vector lookDir, EntityAI e, float threat, float attract, bool friendly, float radius)
	{
		if (e == pawn)
			return;

		vector targetPos = e.GetPosition();
		vector toTarget = targetPos - botPos;
		toTarget[1] = 0.0;
		float dist = toTarget.Length();
		if (dist > radius)
			return;
		if (dist < 0.01)
			return;

		float targetYaw = toTarget.VectorToAngles()[0];
		float lookYaw = lookDir.VectorToAngles()[0];
		float ang = dmAISurvivor.AngleDiff(targetYaw, lookYaw);
		if (Math.AbsFloat(ang) > DM_PERCEPTION_FOV * 0.5)
			return;

		if (!HasLOS(pawn, e, botPos, targetPos))
			return;

		bot.RememberTarget(e, threat, attract, friendly, targetPos);

		#ifdef DM_BOT_DEBUG_VISION
		dmBotLog.Debug("[Vision] hit: " + e + " dist=" + dist + " threat=" + threat);
		dmBotLog.Debug("[Vision] hit: " + e + " attract=" + attract + " friendly=" + friendly);
		#endif
	}

	//! Re-check line of sight for every remembered target at the visibility cadence.
	//! Updates m_HasLOS/m_LastPosition/m_LastContact and forgets stale targets.
	void UpdateVisibility(dmAISurvivor bot)
	{
		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;

		vector botPos = pawn.GetPosition();
		float now = GetGame().GetTickTime();
		ref array<ref dmTarget> targets = bot.GetTargets();
		int i;
		for (i = 0; i < targets.Count(); i++)
		{
			dmTarget t = targets[i];
			EntityAI e = t.m_Entity;
			if (!e)
				continue;
			vector targetPos = e.GetPosition();
			bool visible = HasLOS(pawn, e, botPos, targetPos);
			t.m_HasLOS = visible;
			if (visible)
			{
				t.m_LastPosition = targetPos;
				t.m_LastContact = now;
			}
		}

		bot.ForgetStaleTargets(DM_TARGET_FORGET_TIME);
	}

	//! Line-of-sight from the bot's eye to the target's head (fallback: feet + eye
	//! height). Visible when the closest raycast hit is the target itself.
	bool HasLOS(PlayerBase pawn, EntityAI target, vector botPos, vector targetPos)
	{
		vector eye = botPos + Vector(0, DM_EYE_HEIGHT, 0);

		//! GetBoneIndexByName lives on Human (players) and DayZCreature (zombies/
		//! animals), NOT on EntityAI — cast to the bone-owning class first.
		int hb = -1;
		Human human = Human.Cast(target);
		if (human)
			hb = human.GetBoneIndexByName("Head");
		else
		{
			DayZCreature creature = DayZCreature.Cast(target);
			if (creature)
				hb = creature.GetBoneIndexByName("Head");
		}

		vector end;
		if (hb >= 0)
			end = target.GetBonePositionWS(hb);
		else
			end = targetPos + Vector(0, DM_EYE_HEIGHT, 0);

		if (end == vector.Zero)
			end = targetPos;

		RaycastRVParams rp = new RaycastRVParams(eye, end, pawn);
		rp.sorted = true;
		rp.type = ObjIntersectView;
		rp.flags = CollisionFlags.NEARESTCONTACT;

		array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
		if (!DayZPhysics.RaycastRVProxy(rp, hits) || hits.Count() == 0)
			return true;

		Object o = hits[0].obj;
		Object p = hits[0].parent;
		return (o == target) || (p == target);
	}
}
