//! dmAISurvivor — server-side bot controller (Layer 0 + head look).
//!
//! The "brain" object is separate from the visual model (pawn). The pawn is a
//! dmAISurvivorBase entity (client-server mod) created via GetGame().CreatePlayer.
//!
//! Head look: the controller computes a look offset (relative to the body) and
//! pushes it to the pawn, whose HeadingModel override turns the head while the
//! body stays still.

class dmAISurvivor
{
	//! Global list of all spawned bots (keeps controllers alive and enumerable).
	static ref array<ref dmAISurvivor> s_All = new array<ref dmAISurvivor>;

	//! Reverse lookup: pawn -> controller.
	static ref map<PlayerBase, ref dmAISurvivor> s_ByPawn = new map<PlayerBase, ref dmAISurvivor>;

	//! Survivor model class (e.g. "dmAI_SurvivorM_Denis").
	private string m_Model = DM_DEFAULT_MODEL;

	//! The pawn (visual/physical body) in the world. null until Spawn().
	private PlayerBase m_Pawn;

	//! Desired look direction.
	//! m_TargetLookYawAbs - horizontal look target in WORLD space (degrees).
	//! m_TargetLookPitch  - vertical angle (degrees).
	private float m_TargetLookYawAbs = 0.0;
	private float m_TargetLookPitch = 0.0;

	//! Current (smoothed) horizontal head offset relative to the body, degrees.
	private float m_CurLookYaw = 0.0;

	//! Optional entity to keep looking at each tick (its face).
	private EntityAI m_LookTarget;

	void dmAISurvivor()
	{
	}

	//! Model class to use. Must be set before Spawn().
	void SetModel(string model)
	{
		m_Model = model;
	}

	string GetModel()
	{
		return m_Model;
	}

	//! Materialize the bot in the world.
	//! @param position    body position in world space.
	//! @param orientation body orientation (Euler angles in degrees, yaw = [0]).
	//! @return the pawn, or null on failure.
	PlayerBase Spawn(vector position, vector orientation)
	{
		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("Spawn() model=" + m_Model + " position=" + position + " orientation=" + orientation);
		#endif

		if (m_Model.Length() == 0)
		{
			#ifdef DM_BOT_DEBUG
			dmBotLog.Debug("Spawn() FAILED: model is empty");
			#endif
			return null;
		}

		Entity entity = GetGame().CreatePlayer(null, m_Model, position, 0.0, "NONE");
		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("Spawn() CreatePlayer returned entity=" + entity);
		#endif

		PlayerBase pawn = PlayerBase.Cast(entity);
		if (!pawn)
		{
			#ifdef DM_BOT_DEBUG
			dmBotLog.Debug("Spawn() FAILED: PlayerBase.Cast(entity) returned null");
			#endif
			return null;
		}

		m_Pawn = pawn;
		m_Pawn.SetPosition(position);
		m_Pawn.SetOrientation(orientation);

		s_All.Insert(this);
		s_ByPawn.Set(m_Pawn, this);

		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("Spawn() OK pawn=" + pawn + " position=" + m_Pawn.GetPosition() + " orientation=" + m_Pawn.GetOrientation() + " total=" + s_All.Count());
		#endif
		return m_Pawn;
	}

	//! Remove the bot's body from the world and unregister it.
	void Despawn()
	{
		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("Despawn() pawn=" + m_Pawn);
		#endif

		if (!m_Pawn)
			return;

		s_All.RemoveItem(this);
		s_ByPawn.Remove(m_Pawn);
		GetGame().ObjectDelete(m_Pawn);
		m_Pawn = null;
	}

	bool IsSpawned()
	{
		return m_Pawn != null;
	}

	PlayerBase GetPawn()
	{
		return m_Pawn;
	}

	vector GetPosition()
	{
		if (m_Pawn)
			return m_Pawn.GetPosition();
		return vector.Zero;
	}

	//! Set body orientation from Euler angles (degrees). yaw = orientation[0].
	void SetOrientation(vector orientation)
	{
		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("SetOrientation() orientation=" + orientation + " pawn=" + m_Pawn);
		#endif
		if (m_Pawn)
			m_Pawn.SetOrientation(orientation);
	}

	//! Set body facing from a world-space direction vector.
	//! Only the horizontal (yaw) component is used; pitch/roll are zeroed.
	void SetDirection(vector direction)
	{
		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("SetDirection() direction=" + direction + " pawn=" + m_Pawn);
		#endif
		if (m_Pawn)
		{
			vector orientation = direction.VectorToAngles();
			orientation[1] = 0.0; // pitch
			orientation[2] = 0.0; // roll
			m_Pawn.SetOrientation(orientation);

			dmAISurvivorBase pawn = dmAISurvivorBase.Cast(m_Pawn);
			if (pawn)
				pawn.SetTargetBodyYaw(orientation[0]);

			#ifdef DM_BOT_DEBUG
			dmBotLog.Debug("SetDirection() applied orientation=" + orientation + " actual=" + m_Pawn.GetOrientation());
			#endif
		}
	}

	vector GetOrientation()
	{
		if (m_Pawn)
			return m_Pawn.GetOrientation();
		return vector.Zero;
	}

	//------------------------------------------------------------------
	// Look control
	//------------------------------------------------------------------

	//! Direct the bot's sight at a world-space point.
	void LookAtPoint(vector pt)
	{
		if (!m_Pawn)
		{
			#ifdef DM_BOT_DEBUG
			dmBotLog.Debug("LookAtPoint() FAILED: no pawn");
			#endif
			return;
		}

		vector origin = m_Pawn.GetPosition() + Vector(0, DM_EYE_HEIGHT, 0);
		vector dir = pt - origin;
		vector angles = dir.VectorToAngles();

		float bodyYaw = m_Pawn.GetOrientation()[0];
		m_TargetLookYawAbs = angles[0]; // absolute world yaw to the target

		//! VectorToAngles returns pitch in [0, 360); normalize to [-180, 180]
		//! so "slightly below level" (e.g. 359.5) doesn't clamp to +85 (look up).
		float pitch = angles[1];
		if (pitch > 180.0)
			pitch -= 360.0;
		m_TargetLookPitch = pitch;

		#ifdef DM_BOT_TRACE
		dmBotLog.Trace("LookAtPoint() dir=" + dir + " angles=" + angles + " bodyYaw=" + bodyYaw + " targetYawAbs=" + m_TargetLookYawAbs + " lookPitch=" + m_TargetLookPitch);
		#endif
	}

	//! Direct the bot's sight by offsets from the body direction.
	//! @param v horizontal head turn in degrees (0 = body forward).
	//! @param h vertical head turn in degrees (0 = level).
	void LookAtDirection(float v, float h)
	{
		float bodyYaw = 0.0;
		if (m_Pawn)
			bodyYaw = m_Pawn.GetOrientation()[0];

		m_TargetLookYawAbs = bodyYaw + v;
		m_TargetLookPitch = h;
	}

	//! Keep looking at an entity (its face) each tick.
	void SetLookTarget(EntityAI target)
	{
		#ifdef DM_BOT_DEBUG
		dmBotLog.Debug("SetLookTarget() target=" + target);
		#endif
		m_LookTarget = target;
	}

	EntityAI GetLookTarget()
	{
		return m_LookTarget;
	}

	//! The bot's heartbeat. Called every frame by the server driver.
	void OnUpdate(float pDt)
	{
		if (!m_Pawn)
			return;

		if (m_LookTarget)
			LookAtPoint(m_LookTarget.GetPosition() + Vector(0, DM_EYE_HEIGHT, 0));

		UpdateLook(pDt);
	}

	//! Smoothly steer the head toward the desired look target. If the target is
	//! beyond the head's turn range, rotate the body so the bot keeps facing it
	//! (looking back over the shoulder instead of getting stuck).
	void UpdateLook(float pDt)
	{
		if (!m_Pawn)
			return;

		float t = Math.Min(1.0, DM_LOOK_TURN_SPEED * pDt);

		float bodyYaw = m_Pawn.GetOrientation()[0];
		float relTarget = AngleDiff(m_TargetLookYawAbs, bodyYaw);

		//! Head tracks the target, clamped to the head range.
		float headTarget = Math.Clamp(relTarget, -DM_LOOK_MAX_YAW, DM_LOOK_MAX_YAW);
		float dYaw = AngleDiff(headTarget, m_CurLookYaw);
		float applyYaw = dYaw * t;
		m_CurLookYaw += applyYaw;

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(m_Pawn);
		if (pawn)
		{
			pawn.SetLookYaw(m_CurLookYaw);
			pawn.SetLookPitch(m_TargetLookPitch);

			//! Desired body yaw so the target ends up at the head's edge; the pawn
			//! rotates its body (and plays the foot-stepping animation) in its
			//! CommandHandler toward this yaw.
			pawn.SetTargetBodyYaw(m_TargetLookYawAbs - headTarget);
		}

		if (Math.AbsFloat(applyYaw) > 0.1)
			#ifdef DM_BOT_TRACE
			dmBotLog.Trace("UpdateLook() targetYawAbs=" + m_TargetLookYawAbs + " bodyYaw=" + bodyYaw + " relTarget=" + relTarget + " curYaw=" + m_CurLookYaw + " pitch=" + m_TargetLookPitch);
			#endif
	}

	//! Signed angle difference, normalized to (-180, 180].
	static float AngleDiff(float a, float b)
	{
		float d = a - b;
		while (d > 180.0) d -= 360.0;
		while (d < -180.0) d += 360.0;
		return d;
	}

	//------------------------------------------------------------------
	// Registry (statics)
	//------------------------------------------------------------------

	//! Find the controller by pawn.
	static dmAISurvivor Find(PlayerBase pawn)
	{
		dmAISurvivor bot;
		if (s_ByPawn.Find(pawn, bot))
			return bot;
		return null;
	}

	//! Number of currently spawned bots.
	static int Count()
	{
		return s_All.Count();
	}

	//! Advance every bot by one frame. Called by the server driver.
	static void TickAll(float pDt)
	{
		for (int i = 0; i < s_All.Count(); i++)
			s_All[i].OnUpdate(pDt);
	}
}
