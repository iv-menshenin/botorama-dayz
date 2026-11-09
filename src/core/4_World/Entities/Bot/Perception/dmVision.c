//! dmVision — perception: server-side discovery and per-target line-of-sight.
//!
//! Two independent cadences: a cheap discovery pass (registry -> radius ->
//! DiscoverTarget) runs every DM_PERCEPTION_BOX_INTERVAL and only ADDS new
//! candidates to the bot's target memory; a per-target LOS refresh runs every tick,
//! throttled per target by GetRefreshTime. Discovery never checks FOV/LOS — the LOS
//! pass is the only place that updates the target's LOS/position/contact memory (and
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
		UpdateAggro(bot, pDt);
		
		float now = GetGame().GetTickTime();
		ref array<ref dmTarget> targets = bot.GetTargets();
		for (int i = 0; i < targets.Count(); i++)
		{
			dmTarget t = targets[i];
			if ( t.HasLOS() || now - t.LastContact() < 15.0 || now - t.LastDamage() < 60.0 || t.LastDistance() < 200.0 )
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
		dmBotLog.Debug(bot.GetName(), "[Vision] scan: targets=" + bot.GetTargets().Count());
		dmBotLog.Debug(bot.GetName(), "[Vision] registry: zombies=" + dmEntityRegistry.GetZombies().Count());
		dmBotLog.Debug(bot.GetName(), "[Vision] registry: players=" + dmEntityRegistry.GetPlayers().Count() + " animals=" + dmEntityRegistry.GetAnimals().Count());
		#endif
	}

	//! Discover every registered zombie within the bot's own light-scaled vision
	//! distance (emit=0 — зомби не светится). Упрощение «открытие ≠ гейт» остаётся
	//! только для существ (зомби/животные: emit=0). Подсвеченный ИГРОК открывается
	//! с подсвеченной дистанции (см. ScanPlayers), симметрично LOS-гейту.
	private void ScanZombies(dmAISurvivor bot, vector botPos)
	{
		array<ZombieBase> zombies = dmEntityRegistry.GetZombies();
		float radius = bot.GetVisionDist(dmIllumTargetType.ZOMBIE, 0.0);
		float radiusSq = radius * radius;
		int i;
		for (i = 0; i < zombies.Count(); i++)
		{
			ZombieBase z = zombies[i];
			if (!z)
				continue;
			vector pos = z.GetPosition();
			if (vector.DistanceSq(botPos, pos) > radiusSq)
				continue;
			bot.DiscoverTarget(z, DM_TARGET_THREAT_ZOMBIE, false);
		}
	}

	//! Discover every registered animal within the bot's own light-scaled vision
	//! distance (emit=0 — животные не светятся).
	private void ScanAnimals(dmAISurvivor bot, vector botPos)
	{
		array<AnimalBase> animals = dmEntityRegistry.GetAnimals();
		float radius = bot.GetVisionDist(dmIllumTargetType.ANIMAL, 0.0);
		float radiusSq = radius * radius;
		int i;
		for (i = 0; i < animals.Count(); i++)
		{
			AnimalBase a = animals[i];
			if (!a)
				continue;
			vector pos = a.GetPosition();
			if (vector.DistanceSq(botPos, pos) > radiusSq)
				continue;
			bot.DiscoverTarget(a, DM_TARGET_THREAT_ANIMAL, false);
		}
	}

	//! Discover every registered player (incl. other bots) within the bot's
	//! light-scaled vision distance per target (emit цели) — подсвеченный игрок
	//! открывается с подсвеченной дистанции, симметрично LOS-гейту в UpdateLOS.
	private void ScanPlayers(dmAISurvivor bot, PlayerBase pawn, vector botPos)
	{
		array<PlayerBase> players = dmEntityRegistry.GetPlayers();
		EntityAI follow = bot.GetFollowTarget();
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase p = players[i];
			if (!p)
				continue;
			if (p == pawn)
				continue;
			bool friendly = (p == follow);
			float radius = bot.GetVisionDist(dmIllumTargetType.PLAYER, p.EmitLightIntensivity());
			float radiusSq = radius * radius;
			vector pos = p.GetPosition();
			if (vector.DistanceSq(botPos, pos) > radiusSq)
				continue;
			bot.DiscoverTarget(p, DM_TARGET_THREAT_PLAYER, friendly);
		}
	}

	//! Per-target LOS refresh (every tick, throttled by GetRefreshTime). The only
	//! place that updates the target's LOS/position/contact memory and forgets stale
	//! targets.
	void UpdateLOS(dmAISurvivor bot)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Vision.LOS.Loop");
		#endif

		dmAISurvivorBase pawn = bot.GetPawn();
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
			EntityAI e = t.GetEntity();
			if (!e)
				continue;

			if (now < t.NextLOSUpdate())
				continue;
			t.SetNextLOSUpdate(now + GetRefreshTime(t));

			vector targetPos = t.GetLivePosition();
			vector toTarget = targetPos - botPos;
			toTarget[1] = 0.0;
			t.SetLastDistance(toTarget.Length());

			#ifdef DM_BOT_DEBUG_VISION
			dmBotLog.Debug(bot.GetName(), "[Vision] Update LOS target=" + e.GetType() + " pos=" + targetPos + " dist=" + t.LastDistance());
			#endif

			if (t.LastDistance() < 0.01)
				continue;

			//! LOS-гейт: райкаст только в пределах свет-масштабированной дальности
			//! видимости типа цели. Открытие (Scan) шло по собственному зрению бота
			//! (emit=0); здесь учитываем и подсветку цели (emit), т.е. яркая цель видна
			//! дальше собственного радиуса зрения — но только если она уже открыта.
			float maxDist = GetLOSMaxDist(bot, t);
			if (t.Threat() < DM_ATTACK_THREAT_THRESHOLD && t.LastDistance() > maxDist)
			{
				#ifdef DM_BOT_DEBUG_VISION
				dmBotLog.Debug(bot.GetName(), "[Vision] No interest " + e.GetType() + " threat=" + t.Threat() + " dist:" + t.LastDistance() + ">" + maxDist);
				#endif
				t.SetHasLOS(false);
				continue;
			}

			//! Конус: не смотрим в сторону цели → сразу НЕ видим, без райкаста.
			float targetYaw = toTarget.VectorToAngles()[0];
			float lookYaw = lookDir.VectorToAngles()[0];
			float ang = dmAISurvivor.AngleDiff(targetYaw, lookYaw);
			if (Math.AbsFloat(ang) > DM_PERCEPTION_FOV * 0.5)
			{
				#ifdef DM_BOT_DEBUG_VISION
				dmBotLog.Debug(bot.GetName(), "[Vision] Angle too big " + e.GetType() + " ang:" + Math.AbsFloat(ang) + ">" + (DM_PERCEPTION_FOV * 0.5));
				#endif
				t.SetHasLOS(false);
				continue;
			}

			// TODO угол уже рассчитан!
			if ( !ProbabilityOfDetection(pawn, targetPos, GetVelocity(e), t.LastContact()) )
			{
				#ifdef DM_BOT_DEBUG_VISION
				dmBotLog.Debug(bot.GetName(), "[Vision] Цель " + e.GetType() + " не замечена!");
				#endif
				t.SetHasLOS(false);
				continue;
			}

			t.SetHasLOS(HasLOS(pawn, e, botPos, targetPos));
			if (t.HasLOS())
			{
				t.SetLastPosition(targetPos);
				t.SetLastPositionSpread(0.0);
				t.SetLastContact(now);
				t.SetEverSeen(true);
				t.SetLastSeen(now);
			}
		}

		bot.ForgetStaleTargets(DM_TARGET_FORGET_TIME);
	}

	//! Детект враждебного намерения: игрок с поднятым огнестрелом целится в силуэт
	//! бота (угловая ширина/высота с запасом DM_AGGRO_AIM_ENLARGE) → бот копит threat.
	//! «Промах рядом» (пуля в землю рядом) — отдельный, будущий шаг.
	private void UpdateAggro(dmAISurvivor bot, float pDt)
	{
		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;

		vector botPos = pawn.GetPosition();

		ref array<ref dmTarget> targets = bot.GetTargets();
		int i;
		for (i = 0; i < targets.Count(); i++)
		{
			dmTarget t = targets[i];
			if (!t.HasLOS() || t.Friendly())
				continue;
			PlayerBase p = PlayerBase.Cast(t.GetEntity());
			if (!p || !p.IsAlive())
				continue;
			if (!p.IsRaised())
				continue;

			EntityAI inHands = p.GetHumanInventory().GetEntityInHands();
			if (!inHands || !Weapon_Base.Cast(inHands) || dmLoot.IsMelee(inHands))
				continue;

			vector playerPos = p.GetPosition();
			float dist = vector.Distance(playerPos, botPos);
			if (dist < 0.01)
				continue;

			// origin — голова игрока (целится из глаз, а не из ног)
			vector headPos = playerPos;
			int userHead = p.GetBoneIndexByName("Head");
			if (userHead >= 0)
				headPos = p.GetBonePositionWS(userHead);

			vector aimDir = GetPlayerAimDir(p);

			// target — центр масс бота (Spine3), фолбэк — грудь 1.2 м
			vector botTarget = botPos + Vector(0, 1.2, 0);
			int botSpine = pawn.GetBoneIndexByName("Spine3");
			if (botSpine >= 0)
				botTarget = pawn.GetBonePositionWS(botSpine);

			vector toBot = botTarget - headPos;
			toBot.Normalize();

			vector aimAngles = aimDir.VectorToAngles();
			float aimYaw = aimAngles[0];
			float aimPitch = aimAngles[1];
			if (aimPitch > 180.0) aimPitch -= 360.0;
			vector toAngles = toBot.VectorToAngles();
			float toYaw = toAngles[0];
			float toPitch = toAngles[1];
			if (toPitch > 180.0) toPitch -= 360.0;
			float yawDiff = Math.AbsFloat(dmAISurvivor.AngleDiff(toYaw, aimYaw));
			float pitchDiff = Math.AbsFloat(dmAISurvivor.AngleDiff(toPitch, aimPitch));

			float halfW = Math.Atan(DM_AGGRO_AIM_TARGET_WIDTH * DM_AGGRO_AIM_ENLARGE / (2.0 * dist)) * Math.RAD2DEG;
			float halfH = Math.Atan(DM_AGGRO_AIM_TARGET_HEIGHT * DM_AGGRO_AIM_ENLARGE / (2.0 * dist)) * Math.RAD2DEG;
			if (halfW < DM_AGGRO_AIM_MIN_HALF_W) halfW = DM_AGGRO_AIM_MIN_HALF_W;
			if (halfH < DM_AGGRO_AIM_MIN_HALF_H) halfH = DM_AGGRO_AIM_MIN_HALF_H;

			#ifdef DM_BOT_DEBUG_VISION
			dmBotLog.Debug(bot.GetName(), "[Aggro] " + p.GetType() + " dist=" + dist + " yaw=" + yawDiff + " halfW=" + halfW);
			dmBotLog.Debug(bot.GetName(), "[Aggro] pitch=" + pitchDiff + " halfH=" + halfH + " threat=" + t.Threat());
			#endif

			if (yawDiff <= halfW && pitchDiff <= halfH)
			{
				bot.AddThreat(p, DM_AGGRO_AIM_RATE * pDt);
				#ifdef DM_BOT_DEBUG_EVADE
				dmBotLog.Debug(bot.GetName(), "[Evade] aim detected " + p.GetType() + " dist=" + dist);
				#endif
				bot.EvadeAim(p);
			}
		}
	}

	//! World-space look direction (body + head turn). Prefers the head-bone forward
	//! (includes the head turn); falls back to the body direction when the bone can't
	//! resolve. Horizontal only (pitch zeroed) and normalized.
	private vector GetLookDir(dmAISurvivorBase pawn)
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

	//! Направление прицела игрока: кость головы (forward, с питчем). Fallback —
	//! направление корпуса. В отличие от GetLookDir питч НЕ зануляется (нужен для
	//! сравнения с угловой высотой силуэта бота).
	static vector GetPlayerAimDir(PlayerBase player)
	{
		int hb = player.GetBoneIndexByName("Head");
		vector aim;
		if (hb >= 0)
		{
			vector transform[4];
			player.GetBoneTransformWS(hb, transform);
			aim = transform[1];

			// Кость головы отклонена от ствола: на сервере прицел = голова + поправка
			// (эталон — Expansion.Expansion_GetAimDirection: +5° яу, +12.5° питч).
			vector angles = aim.VectorToAngles();
			angles[0] = angles[0] + DM_AGGRO_AIM_HEAD_YAW;
			if (angles[0] > 360.0) angles[0] = angles[0] - 360.0;
			angles[1] = angles[1] + DM_AGGRO_AIM_HEAD_PITCH;
			if (angles[1] > 360.0) angles[1] = angles[1] - 360.0;
			aim = angles.AnglesToVector();
		}
		else
		{
			aim = player.GetDirection();
		}
		aim.Normalize();
		return aim;
	}

	//! Направление головы игрока (сырая кость, без поправки прицела) — для условия
	//! (b) уворота: «голова отвернулась».
	static vector GetPlayerHeadDir(PlayerBase player)
	{
		int hb = player.GetBoneIndexByName("Head");
		vector aim;
		if (hb >= 0)
		{
			vector transform[4];
			player.GetBoneTransformWS(hb, transform);
			aim = transform[1];
		}
		else
		{
			aim = player.GetDirection();
		}
		aim.Normalize();
		return aim;
	}

	//! Per-target LOS refresh interval (seconds). Creatures/friendly targets are cheap
	//! to keep fresh; low-threat targets re-check slower, high-threat fastest.
	private float GetRefreshTime(dmTarget t)
	{
		if (AnimalBase.Cast(t.GetEntity()) || ZombieBase.Cast(t.GetEntity()))
			return DM_PERCEPTION_REFRESH_CREATURE;
		if (t.Friendly())
			return DM_PERCEPTION_REFRESH_FRIENDLY;
		if (t.Threat() < 0.5)
			return DM_PERCEPTION_REFRESH_LOW_THREAT;
		return DM_PERCEPTION_REFRESH_HIGH_THREAT;
	}

	//! Максимальная дистанция, на которой этот тип цели ещё стоит проверять LOS.
	//! Свет-масштабированная дальность видимости бота для типа цели, учитывающая
	//! подсветку самой цели: emit = EmitLightIntensivity() цели, если она PlayerBase
	//! (зомби/животные не светятся → 0). Открытие в Scan шло по emit=0 (собственное
	//! зрение), здесь яркая цель видна дальше — гейт шире, чем радиус открытия.
	private float GetLOSMaxDist(dmAISurvivor bot, dmTarget t)
	{
		dmIllumTargetType type = dmIllumTargetType.PLAYER;
		if (ZombieBase.Cast(t.GetEntity()))
			type = dmIllumTargetType.ZOMBIE;
		if (AnimalBase.Cast(t.GetEntity()))
			type = dmIllumTargetType.ANIMAL;

		float emit = 0.0;
		PlayerBase targetPlayer = PlayerBase.Cast(t.GetEntity());
		if (targetPlayer)
			emit = targetPlayer.EmitLightIntensivity();

		return bot.GetVisionDist(type, emit);
	}

	//! Line-of-sight from the bot's eye to the target's head (fallback: feet + eye
	//! height). Visible when the closest raycast hit is the target itself.
	bool HasLOS(dmAISurvivorBase pawn, EntityAI target, vector botPos, vector targetPos)
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
		rp.type = ObjIntersectGeom;
		rp.flags = CollisionFlags.NEARESTCONTACT;

		array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
		if (!dmPhysicsProbe.CastProxy(rp, hits) || hits.Count() == 0)
			return true;

		//! sorted=true + NEARESTCONTACT → ближайший хит первый; для LOS важен
		//! только он (ближайшее препятствие между глазом и целью).
		RaycastRVResult first = hits[0];
		Object o = first.obj;
		Object p = first.parent;
		return (o == target) || (p == target);
	}

	// ProbabilityOfDetection - вероятность обнаружить цель зрением
	bool ProbabilityOfDetection(dmAISurvivorBase pawn, vector position, vector velocity, float lastContact)
	{
		float forgettingTime = GetGame().GetTickTime() - lastContact;
		if ( forgettingTime < 30.0 ) return true;
		float probForg = 1.1 - Math.Clamp( forgettingTime / 60, 0.1, 0.6 );						// 50-100%

		vector botPos = pawn.GetPosition();
		float distSq = vector.DistanceSq(botPos, position);

		float dAng = Math.AbsFloat( pawn.GetLookDiffAngle(position) );
		float dist = Math.Sqrt(distSq);
		float probAng = 1.1 - Math.Clamp( ((1.0 + dAng) * 2 * dist) / 1000.0, 0.1, 1.0 ); 	// 10-100%

		float probVel = 1.0;
		float speed = velocity.LengthSq();
		if ( distSq > 10000.0 ) // actual more than 100 meters far
		{
			probVel = Math.Clamp( 1000000 / (distSq * Math.Max(45 - speed, 0.0000001)), 0.01, 1.0 );	// 1-100%
		}

		#ifdef DM_PERCEPTION_DEBUG
		dmBotLog.Debug(pawn.GetCachedName(), "[Vision] Chance: " + ( probAng * probVel * probForg ) + " probAng=" + probAng + " probVel=" + probVel + " probForg=" + probForg + " [" + speed +"]");
		#endif

		return Math.RandomFloat(0.0, 1.0) < ( probAng * probVel * probForg );
	}
}
