//! dmAiming — AI bot shooting accuracy model (Phase 1).
//!
//! Computes the real shot direction with dispersion, following the
//! eAIAimingProfile model from dayz-devaliada. In this phase the class is only
//! created (by the pawn) and updated; it is NOT yet wired into the fire path
//! (that is Phase 3, see modded_WeaponFire/dmBot_Fire).

class dmAiming
{
	//! The bot pawn this aiming model belongs to (managed, no ref).
	private dmAISurvivorBase m_Unit;

	//! Current target (dmTarget is a plain class -> ref).
	private ref dmTarget m_Target;

	//! Aim point (world space).
	private vector m_AimPosition;

	//! Shot direction (world space, normalized, with dispersion).
	private vector m_AimDirection;

	//! Accumulated tracking time (feeds GetAccuracyByTrackingTime).
	private float m_TrackingTime;

	//! Last time the debug log was printed (throttle).
	private float m_LastLogTime;

	//! Есть ли на винтовке оптика с увеличением (не коллиматор).
	private float m_HasRealOptic;

	//! Recoil pitch offset (degrees, positive = up). Kicks on shot, decays in Update.
	private float m_RecoilPitch = 0.0;

	//! Детерминированная точность (hitProbability), дистанция до точки прицела и
	//! угловая скорость цели, сохранённые из Update() для per-shot разброса в
	//! GetShotDispersion().
	private float m_HitProbability = 1.0;
	private float m_Dist = 0.0;
	private float m_TargetSpeedMult = 0.0;

	void dmAiming(dmAISurvivorBase unit)
	{
		m_Unit = unit;
	}

	void SetTarget(dmTarget target)
	{
		if (target != m_Target)
			m_TrackingTime = 0.0;
		m_Target = target;
	}

	dmTarget GetTarget()
	{
		return m_Target;
	}

	vector GetAimDirection()
	{
		return m_AimDirection;
	}

	vector GetAimPosition()
	{
		return m_AimPosition;
	}

	//! Kick the barrel up after a shot (modifier scales DM_AIM_RECOIL_DEGREE).
	float AddRecoil(float modifier)
	{
		m_RecoilPitch = m_RecoilPitch + modifier * DM_AIM_RECOIL_DEGREE;
		return m_RecoilPitch;
	}

	//! Случайный линейный разброс (в ширинах силуэта) на основе сохранённой hitProbability.
	void RollDeviation(out float deviationLR, out float deviationUD)
	{
		float hp = m_HitProbability;
		if (hp < 0.01)
			hp = 0.01;
		float rollUD = Math.RandomFloat(0.0, 1.0);
		float rollLR = Math.RandomFloat(0.0, 1.0);
		if (rollLR <= hp)
			deviationLR = Math.Lerp(0.0, 0.25, rollLR / hp);
		else
			deviationLR = Math.Lerp(0.25, 1.0, (rollLR - hp) / (1.0 - hp));
		if (rollUD <= hp)
			deviationUD = Math.Lerp(0.0, 0.9, rollUD / hp);
		else
			deviationUD = Math.Lerp(0.9, 2.0, (rollUD - hp) / (1.0 - hp));
		if (Math.RandomIntInclusive(0, 1))
			deviationLR = -deviationLR;
		if (Math.RandomIntInclusive(0, 1))
			deviationUD = -deviationUD;
	}

	//! Per-shot личный разброс (угловой, радианы) для текущей цели. Возвращает false
	//! для не-человеческой цели (зомби/животное = 100% попадание, без разброса).
	bool GetShotDispersion(out float angLR, out float angUD)
	{
		angLR = 0.0;
		angUD = 0.0;
		if (!m_Target || !m_Target.m_Entity || !m_Target.m_Entity.IsAlive())
			return false;
		if (ZombieBase.Cast(m_Target.m_Entity) || AnimalBase.Cast(m_Target.m_Entity))
			return false;
		float devLR;
		float devUD;
		RollDeviation(devLR, devUD);
		float dist = m_Dist;
		if (dist < 0.01)
			dist = 0.01;
		angLR = (devLR / dist) * (1.0 + m_TargetSpeedMult);
		angUD = (devUD / dist) * (0.5 + m_TargetSpeedMult);
		return true;
	}

	void Update(float pDt)
	{
		EntityAI targetEntity;
		vector targetPos;
		vector aimPos;
		int bone;
		Human human;
		DayZCreature creature;
		vector tv;
		bool standing;
		vector transform[4];
		float healthModifier;
		vector position;
		vector direction;
		float dist;
		float accuracyMin;
		float accuracyMax;
		float accuracyDeg;
		float accTT;
		DayZPlayerImplement targetPlayer;
		Weapon_Base weapon;
		ItemOptics optics;
		float zoomMin;
		float zoomMax;
		float visibility;
		float hitProbability;
		float distFactor;
		float farFactor;
		float targetSpeedMult;
		vector targetVelocity;
		vector dirNorm;
		float vRadial;
		vector vTangential;
		float vCross;
		float angularSpeed;
		vector aimOrientation;

		m_HasRealOptic = HasRealOptics();

		//! Recoil recovery: the barrel lowers back over time.
		m_RecoilPitch = m_RecoilPitch - DM_AIM_RECOIL_RECOVERY * pDt;
		if (m_RecoilPitch < 0.0)
			m_RecoilPitch = 0.0;

		//! No valid target: reset and face forward.
		if (!m_Target || !m_Target.m_Entity || !m_Target.m_Entity.IsAlive())
		{
			m_AimPosition = vector.Zero;
			m_AimDirection = m_Unit.GetDirection();
			m_TrackingTime = 0.0;
			return;
		}

		m_TrackingTime += pDt;
		if (m_TrackingTime > DM_AIM_MAX_TRACKING_TIME)
			m_TrackingTime = DM_AIM_MAX_TRACKING_TIME;

		targetEntity = m_Target.m_Entity;

		//! Aim point: head for a standing target under real optics, center mass
		//! (Spine3) otherwise; creatures use the head. Fallback to feet + eye height.
		targetPos = targetEntity.GetPosition();
		aimPos = targetPos + Vector(0, DM_EYE_HEIGHT, 0);
		bone = -1;
		human = Human.Cast(targetEntity);
		if (human)
		{
			human.PhysicsGetVelocity(tv);
			standing = tv.Length() < DM_AIM_HEADSHOT_SPEED_EPS;
			if (standing && m_HasRealOptic)
			{
				bone = human.GetBoneIndexByName("Head");
			}
			else
			{
				bone = human.GetBoneIndexByName("Spine3");
				if (bone < 0)
					bone = human.GetBoneIndexByName("Head");
			}
		}
		else
		{
			creature = DayZCreature.Cast(targetEntity);
			if (creature)
				bone = creature.GetBoneIndexByName("Head");
		}
		if (bone >= 0)
			aimPos = targetEntity.GetBonePositionWS(bone);
		m_AimPosition = aimPos;

		m_Unit.GetTransform(transform);
		healthModifier = m_Unit.GetHealth01();

		position = m_Unit.GetBonePositionWS(m_Unit.GetBoneIndexByName("neck"));
		direction = vector.Direction(position, m_AimPosition);

		//! Model-space aim angles + recoil (pitch up). Applied to both creature
		//! and player paths so the barrel kicks regardless of the target type.
		aimOrientation = direction.InvMultiply3(transform).VectorToAngles();
		aimOrientation[1] = aimOrientation[1] + m_RecoilPitch;

		//! Zombies/animals: 100% hit (only the recoil offsets the barrel).
		if (ZombieBase.Cast(targetEntity) || AnimalBase.Cast(targetEntity))
		{
			direction = aimOrientation.AnglesToVector().Multiply3(transform);
			direction.Normalize();
			m_AimDirection = direction;
			return;
		}

		//! More complex accuracy for a human target.
		dist = direction.Length();
		if (dist == 0.0)
			dist = 0.01;
		accuracyMin = DM_AIM_ACCURACY_MIN;
		accuracyMax = DM_AIM_ACCURACY_MAX;

		if (Class.CastTo(targetPlayer, targetEntity))
		{
			accuracyDeg = 0.0;
			if (healthModifier < 1.0)
				accuracyDeg = 0.3 * (1.0 - healthModifier);
			accTT = GetAccuracyByTrackingTime();
			accuracyMin = accuracyMin * accTT - accuracyDeg;
			accuracyMax = accuracyMax * accTT - accuracyDeg;
		}

		if (accuracyMin < 1.0 && Class.CastTo(weapon, m_Unit.GetHumanInventory().GetEntityInHands()))
		{
			if (Class.CastTo(optics, weapon.GetAttachedOptics()))
			{
				zoomMin = optics.GetZeroingDistanceZoomMin();
				zoomMax = optics.GetZeroingDistanceZoomMax();

				//! If target distance is within zeroing range, give accuracy bonus.
				if (zoomMax > 0.0 && dist >= zoomMin)
					accuracyMin = Math.Lerp(accuracyMax, (accuracyMax + accuracyMin) / 2.0, Math.Clamp(dist / (zoomMax + zoomMin), 0.0, 1.0));
			}

			if (weapon.ShootsExplosiveAmmo())
			{
				//! Increase accuracy if shooting explosive ammo to offset for slow projectile and other factors.
				accuracyMin = Math.Lerp(0.5, 1.0, accuracyMin);
				accuracyMax = Math.Lerp(0.5, 1.0, accuracyMax);
			}
		}

		accuracyMin = Math.Clamp(accuracyMin, 0.01, 1.0);
		accuracyMax = Math.Clamp(accuracyMax, 0.01, 1.0);

		//! Influence of the target's angular velocity on accuracy (deterministic,
		//! stored and applied per-shot in GetShotDispersion).
		targetSpeedMult = 0.0;
		if (Class.CastTo(targetPlayer, targetEntity))
		{
			targetPlayer.PhysicsGetVelocity(targetVelocity);
			dirNorm = direction.Normalized();
			vRadial = vector.Dot(targetVelocity, dirNorm);
			vTangential = targetVelocity - dirNorm * vRadial;
			vCross = vTangential.Length();
			angularSpeed = vCross / dist;
			targetSpeedMult = angularSpeed;
		}
		m_TargetSpeedMult = targetSpeedMult;

		visibility = 1.0;
		if (dist <= DM_AIM_MAX_ACCURACY_DIST)
		{
			distFactor = Math.Clamp(dist / DM_AIM_MAX_ACCURACY_DIST, 0.0, 1.0);
			hitProbability = Math.Lerp(accuracyMax, accuracyMin, distFactor) * visibility;
		}
		else
		{
			farFactor = Math.Clamp((dist - DM_AIM_MAX_ACCURACY_DIST) / DM_AIM_MAX_ACCURACY_DIST, 0.0, 1.0);
			hitProbability = Math.Lerp(accuracyMin, 0.01, farFactor * farFactor) * visibility;
		}

		m_Dist = dist;
		m_HitProbability = hitProbability;

		direction = aimOrientation.AnglesToVector().Multiply3(transform);
		direction.Normalize();
		m_AimDirection = direction;

		#ifdef DM_BOT_DEBUG_FSM
		if (GetGame().GetTickTime() - m_LastLogTime >= 2.0)
		{
			m_LastLogTime = GetGame().GetTickTime();
			dmBotLog.Debug("[Aim] dist=" + dist + " hitProb=" + hitProbability);
			dmBotLog.Debug("[Aim] aimPos=" + m_AimPosition + " dir=" + m_AimDirection);
		}
		#endif
	}

	private float GetAccuracyByTrackingTime()
	{
		float maxAccuracyMultiplier = 1.0;
		if ( m_HasRealOptic ) maxAccuracyMultiplier = 2.0;
		float halfAccuracyMultiplier = maxAccuracyMultiplier / 2.0;

		float growPerc;
		if (m_TrackingTime >= DM_AIM_MAX_TRACKING_TIME) return maxAccuracyMultiplier;

		growPerc = m_TrackingTime / DM_AIM_MAX_TRACKING_TIME;
		return halfAccuracyMultiplier + halfAccuracyMultiplier * (growPerc * growPerc);
	}

	private bool HasRealOptics()
	{
		Weapon_Base weapon = Weapon_Base.Cast(m_Unit.GetHumanInventory().GetEntityInHands());
		if (!weapon)
			return false;
		ItemOptics optics = weapon.GetAttachedOptics();
		if (!optics)
			return false;
		return optics.GetZoomMax() > 0.0;
	}
}
