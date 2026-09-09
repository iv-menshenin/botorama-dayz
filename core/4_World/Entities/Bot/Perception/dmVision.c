//! dmVision — perception: server-side discovery and per-target line-of-sight.
//!
//! Two independent cadences: a cheap discovery pass (registry -> radius ->
//! DiscoverTarget) runs every DM_PERCEPTION_BOX_INTERVAL and only ADDS new
//! candidates to the bot's target memory; a per-target LOS refresh runs every tick,
//! throttled per target by GetRefreshTime. Discovery never checks FOV/LOS — the LOS
//! pass is the only place that updates m_HasLOS/m_LastPosition/m_LastContact (and
//! forgets stale targets).
//!
//! Candidates come from the global dmEntityRegistry (zombies/animals/players that
//! self-registered in their constructor), not from a spatial box query. The registry
//! is cleaned of dead/null entities at the start of every scan.

class dmVision
{
	//! Accumulated time toward the next discovery scan.
	float m_BoxAccum = 0.0;

	//! Cached head-bone index of the bot's pawn (-1 = not resolved yet).
	int m_HeadBone = -1;

	//! Accumulate frame time: discovery scan every DM_PERCEPTION_BOX_INTERVAL,
	//! per-target LOS refresh every tick.
	void Update(dmAISurvivor bot, float pDt)
	{
		m_BoxAccum += pDt;
		if (m_BoxAccum >= DM_PERCEPTION_BOX_INTERVAL) { m_BoxAccum = 0.0; Scan(bot); }

		UpdateLOS(bot);
		
		float now = GetGame().GetTickTime();
		ref array<ref dmTarget> targets = bot.GetTargets();
		for (int i = 0; i < targets.Count(); i++)
		{
			dmTarget t = targets[i];
			if ( t.m_HasLOS || now - t.m_LastContact < 15.0 || now - t.m_LastDamage < 60.0 || t.m_LastDistance < 200.0 )
				bot.RecalcTargetThreat(t, pDt);
		}
	}

	//! 1 Hz discovery pass: registry -> radius -> DiscoverTarget (create new targets
	//! only). No FOV/LOS here — those run per-target in UpdateLOS.
	void Scan(dmAISurvivor bot)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Vision.Scan");
		#endif

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;

		dmEntityRegistry.Cleanup();

		vector botPos = pawn.GetPosition();

		ScanZombies(bot, botPos);
		ScanAnimals(bot, botPos);
		ScanPlayers(bot, pawn, botPos);

		#ifdef DM_BOT_DEBUG_VISION
		dmBotLog.Debug("[Vision] scan: targets=" + bot.GetTargets().Count());
		dmBotLog.Debug("[Vision] registry: zombies=" + dmEntityRegistry.GetZombies().Count());
		dmBotLog.Debug("[Vision] registry: players=" + dmEntityRegistry.GetPlayers().Count() + " animals=" + dmEntityRegistry.GetAnimals().Count());
		#endif
	}

	//! Discover every registered zombie within DM_PERCEPTION_ZOMBIE_RADIUS.
	private void ScanZombies(dmAISurvivor bot, vector botPos)
	{
		array<ZombieBase> zombies = dmEntityRegistry.GetZombies();
		float radiusSq = DM_PERCEPTION_ZOMBIE_RADIUS * DM_PERCEPTION_ZOMBIE_RADIUS;
		int i;
		for (i = 0; i < zombies.Count(); i++)
		{
			ZombieBase z = zombies[i];
			if (!z)
				continue;
			vector pos = z.GetPosition();
			if (vector.DistanceSq(botPos, pos) > radiusSq)
				continue;
			bot.DiscoverTarget(z, DM_TARGET_THREAT_ZOMBIE, DM_TARGET_ATTRACT_ZOMBIE, false);
		}
	}

	//! Discover every registered animal within DM_PERCEPTION_ANIMAL_RADIUS.
	private void ScanAnimals(dmAISurvivor bot, vector botPos)
	{
		array<AnimalBase> animals = dmEntityRegistry.GetAnimals();
		float radiusSq = DM_PERCEPTION_ANIMAL_RADIUS * DM_PERCEPTION_ANIMAL_RADIUS;
		int i;
		for (i = 0; i < animals.Count(); i++)
		{
			AnimalBase a = animals[i];
			if (!a)
				continue;
			vector pos = a.GetPosition();
			if (vector.DistanceSq(botPos, pos) > radiusSq)
				continue;
			bot.DiscoverTarget(a, DM_TARGET_THREAT_ANIMAL, DM_TARGET_ATTRACT_ANIMAL, false);
		}
	}

	//! Discover every registered player (incl. other bots) within
	//! DM_PERCEPTION_PLAYER_RADIUS.
	private void ScanPlayers(dmAISurvivor bot, PlayerBase pawn, vector botPos)
	{
		array<PlayerBase> players = dmEntityRegistry.GetPlayers();
		EntityAI follow = bot.GetFollowTarget();
		float radiusSq = DM_PERCEPTION_PLAYER_RADIUS * DM_PERCEPTION_PLAYER_RADIUS;
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase p = players[i];
			if (!p)
				continue;
			if (p == pawn)
				continue;
			bool friendly = (p == follow);
			vector pos = p.GetPosition();
			if (vector.DistanceSq(botPos, pos) > radiusSq)
				continue;
			bot.DiscoverTarget(p, DM_TARGET_THREAT_PLAYER, DM_TARGET_ATTRACT_PLAYER, friendly);
		}
	}

	//! Per-target LOS refresh (every tick, throttled by GetRefreshTime). The only
	//! place that updates m_HasLOS/m_LastPosition/m_LastContact and forgets stale
	//! targets.
	void UpdateLOS(dmAISurvivor bot)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Vision.LOS.Loop");
		#endif

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;

		vector botPos = pawn.GetPosition();
		vector lookDir = GetLookDir(pawn);
		float now = GetGame().GetTickTime();

		ref array<ref dmTarget> targets = bot.GetTargets();
		int i;
		for (i = 0; i < targets.Count(); i++)
		{
			dmTarget t = targets[i];
			EntityAI e = t.m_Entity;
			if (!e)
				continue;

			if (now < t.m_NextLOSUpdate)
				continue;
			t.m_NextLOSUpdate = now + GetRefreshTime(t);

			vector targetPos = t.GetAimPosition();
			vector toTarget = targetPos - botPos;
			toTarget[1] = 0.0;
			t.m_LastDistance = toTarget.Length();

			#ifdef DM_BOT_DEBUG_VISION
			dmBotLog.Debug("[Vision] Update LOS target=" + e.GetType() + " pos=" + targetPos + " dist=" + t.m_LastDistance);
			#endif

			if (t.m_LastDistance < 0.01)
				continue;

			//! LOS-гейт: райкаст только в пределах радиуса релевантности типа цели
			//! (тот же радиус, что и при открытии в Scan).
			if (t.m_Threat < DM_ATTACK_THREAT_THRESHOLD && t.m_LastDistance > GetLOSMaxDist(t))
			{
				#ifdef DM_BOT_DEBUG_VISION
				dmBotLog.Debug("[Vision] No interest " + e.GetType() + " m_Threat=" + t.m_Threat + " dist:" + t.m_LastDistance + ">" + GetLOSMaxDist(t));
				#endif
				t.m_HasLOS = false;
				continue;
			}

			//! Конус: не смотрим в сторону цели → сразу НЕ видим, без райкаста.
			float targetYaw = toTarget.VectorToAngles()[0];
			float lookYaw = lookDir.VectorToAngles()[0];
			float ang = dmAISurvivor.AngleDiff(targetYaw, lookYaw);
			if (Math.AbsFloat(ang) > DM_PERCEPTION_FOV * 0.5)
			{
				#ifdef DM_BOT_DEBUG_VISION
				dmBotLog.Debug("[Vision] Angle too big " + e.GetType() + " ang:" + Math.AbsFloat(ang) + ">" + (DM_PERCEPTION_FOV * 0.5));
				#endif
				t.m_HasLOS = false;
				continue;
			}

			t.m_HasLOS = HasLOS(pawn, e, botPos, targetPos);
			if (t.m_HasLOS)
			{
				t.m_LastPosition = targetPos;
				t.m_LastPositionSpread = 0.0;
				t.m_LastContact = now;
			}
		}

		bot.ForgetStaleTargets(DM_TARGET_FORGET_TIME);
	}

	//! World-space look direction (body + head turn). Prefers the head-bone forward
	//! (includes the head turn); falls back to the body direction when the bone can't
	//! resolve. Horizontal only (pitch zeroed) and normalized.
	private vector GetLookDir(PlayerBase pawn)
	{
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
		return lookDir;
	}

	//! Per-target LOS refresh interval (seconds). Creatures/friendly targets are cheap
	//! to keep fresh; low-threat targets re-check slower, high-threat fastest.
	private float GetRefreshTime(dmTarget t)
	{
		if (AnimalBase.Cast(t.m_Entity) || ZombieBase.Cast(t.m_Entity))
			return DM_PERCEPTION_REFRESH_CREATURE;
		if (t.m_Friendly)
			return DM_PERCEPTION_REFRESH_FRIENDLY;
		if (t.m_Threat < 0.5)
			return DM_PERCEPTION_REFRESH_LOW_THREAT;
		return DM_PERCEPTION_REFRESH_HIGH_THREAT;
	}

	//! Максимальная дистанция, на которой этот тип цели ещё стоит проверять LOS.
	//! Совпадает с радиусом открытия (Scan): дальше цель не релевантна для действий.
	private float GetLOSMaxDist(dmTarget t)
	{
		if (ZombieBase.Cast(t.m_Entity))
			return DM_PERCEPTION_ZOMBIE_RADIUS;
		if (AnimalBase.Cast(t.m_Entity))
			return DM_PERCEPTION_ANIMAL_RADIUS;
		return DM_PERCEPTION_PLAYER_RADIUS;
	}

	//! Line-of-sight from the bot's eye to the target's head (fallback: feet + eye
	//! height). Visible when the closest raycast hit is the target itself.
	bool HasLOS(PlayerBase pawn, EntityAI target, vector botPos, vector targetPos)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Vision.LOS");
		#endif

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

		bool hasLOS = false;
		foreach(RaycastRVResult hit: hits)
		{
			if ( hasLOS ) continue;

			Object o = hit.obj;
			Object p = hit.parent;
			hasLOS = hasLOS || (o == target) || (p == target); // maybe for parent we could use o.GetHierarchyRoot or something

			#ifdef DM_BOT_DEBUG_VISION
			if ( o && p )
				dmBotLog.Debug("[Vision] Raycast (looking for " + target.GetType() + ") hit at " + hit.pos + " to " + o.GetType() + " parent=" + p.GetType() + " hasLOS=" + hasLOS);
			if ( o && !p )
				dmBotLog.Debug("[Vision] Raycast (looking for " + target.GetType() + ") hit at " + hit.pos + " to " + o.GetType() + " hasLOS=" + hasLOS);
			if ( !o && p )
				dmBotLog.Debug("[Vision] Raycast (looking for " + target.GetType() + ") hit at " + hit.pos + " to None parent=" + p.GetType() + " hasLOS=" + hasLOS);
			#endif
		}
		return hasLOS;
	}
}
