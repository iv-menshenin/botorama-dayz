//! dmE2EBridge — file bridge for E2E auto-tests (phase 3: movement + deferred automaton).
//!
//! An AI agent drops a JSON job into $profile:dmBotorama/e2e/in/, the bridge
//! executes it on the server and writes the result to e2e/out/, then moves the
//! input to e2e/done/. The bridge is inert unless the e2e/enabled marker file
//! exists, so there is zero overhead when no agent is driving.
//!
//! Ops: ping | spawn | moveto | follow | patrol | speed | loadout | look | say |
//! wait | assert | snapshot | clearall (named bots) plus the world/physics probe
//! ops spawnobj | raycast | scanbox | botdump | getpos | setpos | clearobj
//! (named objects) and observe (teleport a connected player). `wait` is the only
//! deferred op: it ticks across frames until its condition is met or its timeout
//! expires. Everything else executes in a single tick.

//! A job (input): an id and the list of steps to run.
class dmE2EJob
{
	string Name;     // job id (= file name)
	float Timeout;   // overall timeout (seconds); 0 = none
	autoptr array<ref dmE2EStep> Steps;
}

//! One job step (op plus per-op parameters).
class dmE2EStep
{
	string Op;        // "ping" | "spawn" | "moveto" | "follow" | "patrol" | ...
	string Who;       // bot name (spawn, moveto, ...) / object name (spawnobj)
	string Target;    // target bot name (follow, distance, look)
	vector Pos;       // [x,y,z] world position (spawn, moveto, reached, look, ...)
	float Yaw;        // orientation in degrees (spawn, spawnobj, setpos)
	autoptr array<vector> Points;  // patrol points [[x,z],...]
	float Speed;      // preferred movement speed (1..3)
	string Loadout;   // loadout name
	string Cond;      // wait/assert condition: state|reached|distance|alive|moving
	string Value;     // condition value (state name / "true"/"false" / lineId for say)
	float Tolerance;  // reached/distance tolerance (meters)
	float Timeout;    // wait step timeout (seconds)
	string ClassName; // CfgVehicles class (spawnobj)
	vector From;      // raycast start point (world)
	vector To;        // raycast end point (world)
	vector Min;       // scanbox min corner (world)
	vector Max;       // scanbox max corner (world)
	string Obj;       // object name (getpos, setpos, clearobj)
}

//! Per-step outcome.
class dmE2EStepResult
{
	int Index;
	string Op;
	bool Ok;
	string Reason;
	ref array<string> Dump;   // probe-op dump lines (raycast/scanbox/botdump/getpos)
}

//! Snapshot of one named bot.
class dmE2ESnapshot
{
	string Name;
	bool Alive;
	vector Pos;
	string State;
	bool Moving;
}

//! Job result (output): status + step outcomes + snapshot.
class dmE2EResult
{
	string Name;
	string Status;   // "ok" | "error" | "timeout"
	string Error;
	ref array<ref dmE2EStepResult> Steps;
	ref array<ref dmE2ESnapshot> Snapshot;
}

//! Singleton executor: scans e2e/in/*.json and runs one job at a time. A job is
//! advanced across ticks (deferred `wait` automaton); each tick either advances
//! the current step or stays on it. Ticked from MissionServer.OnUpdate.
class dmE2EBridge
{
	static ref dmE2EBridge s_Instance;
	private ref map<string, ref dmAISurvivor> m_Named;
	private ref map<string, Object> m_Objects;
	private float m_ScanAccum;

	private ref dmE2EJob m_Job;         // active job (null = idle, scanning)
	private ref dmE2EResult m_Result;   // accumulating result
	private string m_JobPath;           // input path (for done/)
	private string m_JobFileName;       // input file name
	private int m_StepIndex;            // next step to run
	private float m_JobTimer;           // elapsed time of the whole job
	private float m_WaitTimer;          // elapsed time of the current wait step

	static dmE2EBridge Get()
	{
		if (!s_Instance)
			s_Instance = new dmE2EBridge();
		return s_Instance;
	}

	void dmE2EBridge()
	{
		m_Named = new map<string, ref dmAISurvivor>();
		m_Objects = new map<string, Object>();
		m_ScanAccum = 0.0;
		m_Job = null;
		m_Result = null;
		m_JobPath = "";
		m_JobFileName = "";
		m_StepIndex = 0;
		m_JobTimer = 0.0;
		m_WaitTimer = 0.0;
	}

	void Tick(float dt)
	{
		if (!FileExist(DM_E2E_ENABLED_FILE))
			return;

		if (m_Job != null)
		{
			m_JobTimer = m_JobTimer + dt;
			if (m_Job.Timeout > 0.0 && m_JobTimer > m_Job.Timeout)
			{
				m_Result.Status = "timeout";
				FinalizeJob();
				return;
			}
			AdvanceStep(dt);
			return;
		}

		m_ScanAccum += dt;
		if (m_ScanAccum < DM_E2E_SCAN_INTERVAL)
			return;
		m_ScanAccum = 0.0;

		if (!FileExist(DM_E2E_IN_DIR))
			return;

		string fileName;
		FileAttr fileAttr;
		FindFileHandle handle = FindFile(DM_E2E_IN_DIR + "/*.json", fileName, fileAttr, FindFileFlags.DIRECTORIES);
		if (fileName != "")
			StartJob(DM_E2E_IN_DIR + "/" + fileName, fileName);
		CloseFindFile(handle);
	}

	//! Load the job file and initialize the deferred automaton state. On load
	//! failure the input is immediately failed and moved to done/ (as before).
	private void StartJob(string jobPath, string fileName)
	{
		#ifdef DM_BOT_DEBUG_E2E
		dmBotLog.Debug("[E2E] job picked up: " + fileName);
		#endif

		dmE2EJob job;
		string loadError;
		if (!JsonFileLoader<dmE2EJob>.LoadFile(jobPath, job, loadError))
		{
			dmBotLog.Error("[E2E] load failed: " + fileName + ": " + loadError);

			dmE2EResult failResult = new dmE2EResult();
			failResult.Steps = new array<ref dmE2EStepResult>();
			failResult.Snapshot = new array<ref dmE2ESnapshot>();
			failResult.Name = BaseName(fileName);
			failResult.Status = "error";
			failResult.Error = loadError;
			SaveResult(failResult);
			MoveToDone(jobPath, fileName);
			m_Job = null;
			return;
		}

		m_Job = job;
		m_JobPath = jobPath;
		m_JobFileName = fileName;

		m_Result = new dmE2EResult();
		m_Result.Steps = new array<ref dmE2EStepResult>();
		m_Result.Snapshot = new array<ref dmE2ESnapshot>();
		m_Result.Name = m_Job.Name;
		if (m_Result.Name == "")
			m_Result.Name = BaseName(fileName);
		m_Result.Status = "ok";

		m_StepIndex = 0;
		m_JobTimer = 0.0;
		m_WaitTimer = 0.0;
	}

	//! Advance the automaton one step (or stay on the current wait step).
	private void AdvanceStep(float dt)
	{
		if (m_StepIndex >= m_Job.Steps.Count())
		{
			FinalizeJob();
			return;
		}

		dmE2EStep step = m_Job.Steps[m_StepIndex];

		if (step.Op == "wait")
		{
			m_WaitTimer = m_WaitTimer + dt;

			dmE2EStepResult waitResult = new dmE2EStepResult();
			waitResult.Index = m_StepIndex;
			waitResult.Op = step.Op;
			waitResult.Dump = new array<string>();

			bool done = false;
			if (EvaluateCondition(step, waitResult))
			{
				waitResult.Ok = true;
				waitResult.Reason = "condition met";
				done = true;
			}
			else if (step.Timeout > 0.0 && m_WaitTimer >= step.Timeout)
			{
				waitResult.Ok = false;
				waitResult.Reason = "timeout";
				done = true;
			}

			if (done)
			{
				m_Result.Steps.Insert(waitResult);
				m_WaitTimer = 0.0;
				m_StepIndex = m_StepIndex + 1;
				LogStep(waitResult);
			}
			return;
		}

		dmE2EStepResult stepResult = new dmE2EStepResult();
		stepResult.Index = m_StepIndex;
		stepResult.Op = step.Op;
		stepResult.Dump = new array<string>();
		RunInstantOp(step, stepResult);
		m_Result.Steps.Insert(stepResult);
		m_StepIndex = m_StepIndex + 1;
		LogStep(stepResult);
	}

	//! Finish the active job: fold step outcomes into the final status, persist
	//! the result, move the input to done/ and reset the automaton.
	private void FinalizeJob()
	{
		if (m_Result.Status != "timeout")
			m_Result.Status = "ok";

		int i;
		for (i = 0; i < m_Result.Steps.Count(); i++)
		{
			if (!m_Result.Steps[i].Ok)
			{
				m_Result.Status = "error";
				break;
			}
		}

		SaveResult(m_Result);
		MoveToDone(m_JobPath, m_JobFileName);
		ClearNamed();

		#ifdef DM_BOT_DEBUG_E2E
		dmBotLog.Debug("[E2E] job done: " + m_Result.Name + " status=" + m_Result.Status);
		#endif

		m_Job = null;
		m_Result = null;
		m_JobPath = "";
		m_JobFileName = "";
		m_StepIndex = 0;
		m_JobTimer = 0.0;
		m_WaitTimer = 0.0;
	}

	//! Dispatch a single-tick op to its runner.
	private void RunInstantOp(dmE2EStep step, dmE2EStepResult r)
	{
		if (step.Op == "ping")
		{
			r.Ok = true;
			r.Reason = "";
		}
		else if (step.Op == "spawn")
		{
			RunSpawn(step, r);
		}
		else if (step.Op == "moveto")
		{
			RunMoveTo(step, r);
		}
		else if (step.Op == "follow")
		{
			RunFollow(step, r);
		}
		else if (step.Op == "patrol")
		{
			RunPatrol(step, r);
		}
		else if (step.Op == "speed")
		{
			RunSpeed(step, r);
		}
		else if (step.Op == "loadout")
		{
			RunLoadout(step, r);
		}
		else if (step.Op == "look")
		{
			RunLook(step, r);
		}
		else if (step.Op == "say")
		{
			RunSay(step, r);
		}
		else if (step.Op == "assert")
		{
			RunAssert(step, r);
		}
		else if (step.Op == "snapshot")
		{
			RunSnapshot(step, r);
		}
		else if (step.Op == "clearall")
		{
			RunClearAll(step, r);
		}
		else if (step.Op == "spawnobj")
		{
			RunSpawnObj(step, r);
		}
		else if (step.Op == "raycast")
		{
			RunRaycast(step, r);
		}
		else if (step.Op == "scanbox")
		{
			RunScanBox(step, r);
		}
		else if (step.Op == "botdump")
		{
			RunBotDump(step, r);
		}
		else if (step.Op == "getpos")
		{
			RunGetPos(step, r);
		}
		else if (step.Op == "setpos")
		{
			RunSetPos(step, r);
		}
		else if (step.Op == "clearobj")
		{
			RunClearObj(step, r);
		}
		else if (step.Op == "observe")
		{
			RunObserve(step, r);
		}
		else
		{
			r.Ok = false;
			r.Reason = "unknown op";
		}
	}

	//! "spawn" — create a named bot at a ground-snapped position.
	private void RunSpawn(dmE2EStep step, dmE2EStepResult r)
	{
		ref dmAISurvivor bot = new dmAISurvivor();
		PlayerBase pawn = bot.Spawn(ResolveWorldPos(step.Pos), Vector(step.Yaw, 0, 0));
		if (pawn)
		{
			m_Named.Insert(step.Who, bot);
			r.Ok = true;
			r.Reason = "spawned";
		}
		else
		{
			r.Ok = false;
			r.Reason = "spawn failed";
		}
	}

	//! "moveto" — issue a critical MoveTo command intent toward Pos.
	private void RunMoveTo(dmE2EStep step, dmE2EStepResult r)
	{
		dmAISurvivor bot;
		if (!m_Named.Find(step.Who, bot))
		{
			r.Ok = false;
			r.Reason = "no such bot";
			return;
		}

		dmBotIntent_MoveTo move = new dmBotIntent_MoveTo();
		move.m_Goal = ResolveWorldPos(step.Pos);
		move.m_Priority = dmBotIntentPriority.CRITICAL;
		move.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		move.m_Deadline = DM_E2E_MOVE_DEADLINE;
		bot.AddCommandIntent(move);

		r.Ok = true;
		r.Reason = "moveto issued";
	}

	//! "follow" — make the bot escort the named target bot.
	private void RunFollow(dmE2EStep step, dmE2EStepResult r)
	{
		dmAISurvivor bot;
		if (!m_Named.Find(step.Who, bot))
		{
			r.Ok = false;
			r.Reason = "no such bot";
			return;
		}

		dmAISurvivor target;
		if (!m_Named.Find(step.Target, target))
		{
			r.Ok = false;
			r.Reason = "no such target";
			return;
		}

		bot.SetFollowTarget(target.GetPawn());
		bot.ClearFSMIntents();
		bot.SetFSM(dmBotPreset_Escort.Create(bot));

		r.Ok = true;
		r.Reason = "follow issued";
	}

	//! "patrol" — replace the bot's patrol points with the step's Points.
	private void RunPatrol(dmE2EStep step, dmE2EStepResult r)
	{
		dmAISurvivor bot;
		if (!m_Named.Find(step.Who, bot))
		{
			r.Ok = false;
			r.Reason = "no such bot";
			return;
		}

		bot.ClearPatrolPoints();
		int i;
		for (i = 0; i < step.Points.Count(); i++)
			bot.AddPatrolPoint(ResolveWorldPos(step.Points[i]));

		r.Ok = true;
		r.Reason = "patrol issued";
	}

	//! "speed" — set the bot's preferred movement speed.
	private void RunSpeed(dmE2EStep step, dmE2EStepResult r)
	{
		dmAISurvivor bot;
		if (!m_Named.Find(step.Who, bot))
		{
			r.Ok = false;
			r.Reason = "no such bot";
			return;
		}

		bot.SetPreferredSpeed(step.Speed);

		r.Ok = true;
		r.Reason = "speed issued";
	}

	//! "loadout" — load a loadout by name and apply it to the bot's pawn.
	private void RunLoadout(dmE2EStep step, dmE2EStepResult r)
	{
		dmAISurvivor bot;
		if (!m_Named.Find(step.Who, bot))
		{
			r.Ok = false;
			r.Reason = "no such bot";
			return;
		}

		dmLoadoutConfig cfg = dmLoadoutApplier.Load(step.Loadout);
		if (cfg)
		{
			dmLoadoutApplier.Apply(bot.GetPawn(), cfg);
			r.Ok = true;
			r.Reason = "loadout issued";
		}
		else
		{
			r.Ok = false;
			r.Reason = "loadout not found";
		}
	}

	//! "look" — issue a critical HoldLook intent at the target bot (or Pos).
	private void RunLook(dmE2EStep step, dmE2EStepResult r)
	{
		dmAISurvivor bot;
		if (!m_Named.Find(step.Who, bot))
		{
			r.Ok = false;
			r.Reason = "no such bot";
			return;
		}

		dmBotIntent_HoldLook look = new dmBotIntent_HoldLook();
		look.m_Turn = dmBotLookTurn.FULL;
		look.m_Priority = dmBotIntentPriority.CRITICAL;
		look.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		look.m_Deadline = DM_E2E_MOVE_DEADLINE;

		dmAISurvivor target;
		if (m_Named.Find(step.Target, target))
			look.m_Entity = target.GetPawn();
		else
			look.m_Point = ResolveWorldPos(step.Pos);

		bot.AddCommandIntent(look);

		r.Ok = true;
		r.Reason = "look issued";
	}

	//! "say" — make the bot speak the voice line identified by Value (lineId).
	private void RunSay(dmE2EStep step, dmE2EStepResult r)
	{
		dmAISurvivor bot;
		if (!m_Named.Find(step.Who, bot))
		{
			r.Ok = false;
			r.Reason = "no such bot";
			return;
		}

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn)
			pawn.SpeakLine(step.Value.ToInt());

		r.Ok = true;
		r.Reason = "say issued";
	}

	//! "assert" — instantaneous condition check; Ok = condition result.
	private void RunAssert(dmE2EStep step, dmE2EStepResult r)
	{
		bool ok = EvaluateCondition(step, r);
		if (ok)
			r.Reason = "condition met";
		else if (r.Reason == "")
			r.Reason = "condition not met";
	}

	//! "snapshot" — record every named bot's state (alive/pos/state/moving).
	private void RunSnapshot(dmE2EStep step, dmE2EStepResult r)
	{
		TStringArray names = m_Named.GetKeyArray();
		int j;
		for (j = 0; j < names.Count(); j++)
		{
			dmAISurvivor snapBot;
			if (!m_Named.Find(names[j], snapBot))
				continue;

			dmE2ESnapshot snap = new dmE2ESnapshot();
			snap.Name = names[j];
			snap.Pos = snapBot.GetPosition();
			if (snapBot.GetPawn() && snapBot.GetPawn().IsAlive())
				snap.Alive = true;
			else
				snap.Alive = false;

			dmBotFSM fsm = snapBot.GetFSM();
			if (fsm && fsm.GetCurrentState())
				snap.State = fsm.GetCurrentState().GetName();

			snap.Moving = false;
			PlayerBase snapPawn = snapBot.GetPawn();
			if (snapPawn)
			{
				vector vel = GetVelocity(snapPawn);
				if (vector.Distance(vel, vector.Zero) > DM_E2E_MOVING_THRESHOLD)
					snap.Moving = true;
			}

			m_Result.Snapshot.Insert(snap);
		}
		r.Ok = true;
		r.Reason = "";
	}

	//! "clearall" — remove every spawned bot and empty the named registry.
	private void RunClearAll(dmE2EStep step, dmE2EStepResult r)
	{
		int cleared = dmAISurvivor.ClearAll();
		m_Named.Clear();
		r.Ok = true;
		r.Reason = "cleared " + cleared;
	}

	//! Evaluate a wait/assert condition for the bot named by step.Who. Returns
	//! true when the condition holds; fills r.Ok/r.Reason (used by both wait and
	//! assert).
	private bool EvaluateCondition(dmE2EStep step, dmE2EStepResult r)
	{
		dmAISurvivor bot;
		dmAISurvivor target;
		dmBotFSM fsm;
		PlayerBase pawn;
		vector vel;
		bool want;
		bool alive;
		bool mv;
		bool result;

		if (!m_Named.Find(step.Who, bot))
		{
			r.Ok = false;
			r.Reason = "no such bot";
			return false;
		}

		result = false;

		if (step.Cond == "state")
		{
			fsm = bot.GetFSM();
			if (fsm && fsm.GetCurrentState() && fsm.GetCurrentState().GetName() == step.Value)
				result = true;
		}
		else if (step.Cond == "reached")
		{
			if (vector.Distance(bot.GetPosition(), ResolveWorldPos(step.Pos)) < step.Tolerance)
				result = true;
		}
		else if (step.Cond == "distance")
		{
			if (m_Named.Find(step.Target, target))
			{
				if (vector.Distance(bot.GetPosition(), target.GetPosition()) < step.Tolerance)
					result = true;
			}
		}
		else if (step.Cond == "alive")
		{
			want = step.Value == "true";
			alive = false;
			pawn = bot.GetPawn();
			if (pawn && pawn.IsAlive())
				alive = true;
			if (alive == want)
				result = true;
		}
		else if (step.Cond == "moving")
		{
			want = step.Value == "true";
			vel = vector.Zero;
			pawn = bot.GetPawn();
			if (pawn)
				vel = GetVelocity(pawn);
			mv = vector.Distance(vel, vector.Zero) > DM_E2E_MOVING_THRESHOLD;
			if (mv == want)
				result = true;
		}

		r.Ok = result;
		if (result)
			r.Reason = "";
		return result;
	}

	//! Echo a completed step to RPT (gated).
	private void LogStep(dmE2EStepResult r)
	{
		#ifdef DM_BOT_DEBUG_E2E
		dmBotLog.Debug("[E2E] step " + r.Index + " " + r.Op + " ok=" + r.Ok);
		dmBotLog.Debug("[E2E] step reason=" + r.Reason);
		#endif
	}

	//! Drop both registries after a job. Probe objects are deleted from the world;
	//! spawned BOTS persist (they stay in dmAISurvivor.s_All) so an observer client
	//! can watch them — a scenario must end with `clearall` to clean them up.
	void ClearNamed()
	{
		m_Named.Clear();
		ClearObjects();
	}

	//! Delete every probe object from the world and empty the registry.
	//! Returns the number of objects deleted.
	private int ClearObjects()
	{
		TStringArray keys = m_Objects.GetKeyArray();
		int cleared = 0;
		int i;
		for (i = 0; i < keys.Count(); i++)
		{
			Object obj;
			if (m_Objects.Find(keys[i], obj) && obj)
			{
				EntityAI entity = EntityAI.Cast(obj);
				if (entity)
					entity.DeleteSafe();
				cleared = cleared + 1;
			}
		}
		m_Objects.Clear();
		return cleared;
	}

	//! Resolve a world point: Y==0 (unspecified) -> snap to ground; else keep explicit Y.
	private vector ResolveWorldPos(vector p)
	{
		if (p[1] == 0.0)
			return SnapToGroundExactly(p);
		return p;
	}

	//! Spawn an arbitrary CfgVehicles object at a ground-snapped position,
	//! registered under the step's Who name (in m_Objects, not m_Named).
	private void RunSpawnObj(dmE2EStep step, dmE2EStepResult r)
	{
		Object obj = GetGame().CreateObject(step.ClassName, ResolveWorldPos(step.Pos), false);
		if (obj)
		{
			obj.SetOrientation(Vector(step.Yaw, 0, 0));
			m_Objects.Insert(step.Who, obj);
			r.Ok = true;
			r.Reason = "spawned " + step.ClassName;
		}
		else
		{
			r.Ok = false;
			r.Reason = "spawn failed";
		}
	}

	//! Append a dump line to the step result and echo it to RPT (gated).
	private void AppendDump(dmE2EStepResult r, string line)
	{
		r.Dump.Insert(line);
		#ifdef DM_BOT_DEBUG_E2E
		dmBotLog.Debug("[E2E] " + line);
		#endif
	}

	//! Raycast between two eye-height points (ground-snapped, raised by
	//! DM_E2E_EYE_HEIGHT). Every hit is dumped (obj/parent/pos/dist/component).
	private void RunRaycast(dmE2EStep step, dmE2EStepResult r)
	{
		vector fromPos = step.From;
		vector toPos = step.To;
		if (fromPos[1] == 0.0)
		{
			fromPos = SnapToGroundExactly(fromPos);
			fromPos[1] = fromPos[1] + DM_E2E_EYE_HEIGHT;
		}
		if (toPos[1] == 0.0)
		{
			toPos = SnapToGroundExactly(toPos);
			toPos[1] = toPos[1] + DM_E2E_EYE_HEIGHT;
		}

		RaycastRVParams params = new RaycastRVParams(fromPos, toPos);
		params.flags = CollisionFlags.ALLOBJECTS;

		array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>();
		DayZPhysics.RaycastRVProxy(params, hits);

		int i;
		for (i = 0; i < hits.Count(); i++)
		{
			RaycastRVResult hit = hits[i];
			string objName = "null";
			if (hit.obj)
				objName = hit.obj.GetType();
			string parentName = "null";
			if (hit.parent)
				parentName = hit.parent.GetType();
			float dist = vector.Distance(fromPos, hit.pos);

			string line = "hit: obj=" + objName + " parent=" + parentName;
			line += " pos=" + hit.pos;
			line += " dist=" + dist;
			line += " component=" + hit.component;
			AppendDump(r, line);
		}

		r.Ok = true;
		if (hits.Count() == 0)
			r.Reason = "0 hits (clear)";
		else
			r.Reason = hits.Count().ToString() + " hits";
	}

	//! Scene box query (static + dynamic) with a per-entity dump line. Static
	//! and dynamic entities are fetched in two separate calls (one flag each).
	private void RunScanBox(dmE2EStep step, dmE2EStepResult r)
	{
		array<EntityAI> dynamics = new array<EntityAI>();
		DayZPlayerUtils.SceneGetEntitiesInBox(ResolveWorldPos(step.Min), ResolveWorldPos(step.Max), dynamics, QueryFlags.DYNAMIC);

		array<EntityAI> statics = new array<EntityAI>();
		DayZPlayerUtils.SceneGetEntitiesInBox(ResolveWorldPos(step.Min), ResolveWorldPos(step.Max), statics, QueryFlags.STATIC);

		int i;
		string line;
		for (i = 0; i < dynamics.Count(); i++)
		{
			line = "ent[D] " + dynamics[i].GetType() + " pos=" + dynamics[i].GetPosition();
			AppendDump(r, line);
		}
		for (i = 0; i < statics.Count(); i++)
		{
			line = "ent[S] " + statics[i].GetType() + " pos=" + statics[i].GetPosition();
			AppendDump(r, line);
		}

		r.Ok = true;
		r.Reason = (dynamics.Count() + statics.Count()).ToString() + " entities";
	}

	//! Dump the named bot's body/motion/brain state as a set of lines.
	private void RunBotDump(dmE2EStep step, dmE2EStepResult r)
	{
		dmAISurvivor bot;
		if (!m_Named.Find(step.Who, bot))
		{
			r.Ok = false;
			r.Reason = "no such bot";
			return;
		}

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
		{
			r.Ok = false;
			r.Reason = "no pawn";
			return;
		}

		string line = "pos=" + bot.GetPosition();
		AppendDump(r, line);

		line = "alive=" + pawn.IsAlive() + " unconscious=" + pawn.IsUnconscious();
		AppendDump(r, line);

		line = "restrained=" + pawn.IsRestrained() + " bleeding=" + pawn.IsBleeding();
		AppendDump(r, line);

		line = "health=" + pawn.GetHealth01() + " blood=" + pawn.GetHealth("", "Blood") + " shock=" + pawn.GetHealth("", "Shock");
		AppendDump(r, line);

		float stamina = -1.0;
		StaminaHandler sh = pawn.GetStaminaHandler();
		if (sh)
			stamina = sh.GetStaminaNormalized();
		line = "stamina=" + stamina;
		AppendDump(r, line);

		vector vel = GetVelocity(pawn);
		line = "vel=" + vel;
		AppendDump(r, line);

		line = "orient=" + bot.GetOrientation();
		AppendDump(r, line);

		dmBotFSM fsm = bot.GetFSM();
		string fsmName = "none";
		if (fsm && fsm.GetCurrentState())
			fsmName = fsm.GetCurrentState().GetName();
		line = "fsm=" + fsmName;
		AppendDump(r, line);

		line = "fsmIntents=" + bot.GetFSMIntents().Count();
		AppendDump(r, line);

		r.Ok = true;
		r.Reason = "dumped";
	}

	//! Report the named probe object's position and yaw.
	private void RunGetPos(dmE2EStep step, dmE2EStepResult r)
	{
		Object obj;
		if (!m_Objects.Find(step.Obj, obj) || !obj)
		{
			r.Ok = false;
			r.Reason = "no such object";
			return;
		}

		string line = "pos=" + obj.GetPosition() + " yaw=" + obj.GetOrientation()[0];
		AppendDump(r, line);

		r.Ok = true;
		r.Reason = "";
	}

	//! Reposition the named probe object (ground-snapped) and set its yaw.
	private void RunSetPos(dmE2EStep step, dmE2EStepResult r)
	{
		Object obj;
		if (!m_Objects.Find(step.Obj, obj) || !obj)
		{
			r.Ok = false;
			r.Reason = "no such object";
			return;
		}

		obj.SetPosition(ResolveWorldPos(step.Pos));
		obj.SetOrientation(Vector(step.Yaw, 0, 0));

		r.Ok = true;
		r.Reason = "";
	}

	//! Delete one probe object (by name) or all of them ("*" or empty).
	private void RunClearObj(dmE2EStep step, dmE2EStepResult r)
	{
		int cleared = 0;
		if (step.Obj == "*" || step.Obj == "")
		{
			cleared = ClearObjects();
		}
		else
		{
			Object obj;
			if (m_Objects.Find(step.Obj, obj) && obj)
			{
				EntityAI entity = EntityAI.Cast(obj);
				if (entity)
					entity.DeleteSafe();
				m_Objects.Remove(step.Obj);
				cleared = 1;
			}
		}

		r.Ok = true;
		r.Reason = "cleared " + cleared;
	}

	//! Teleport the first HUMAN player (observer) to Pos and face Yaw.
	//! AI survivors (dmAISurvivorBase) are PlayerBase too and pollute the registry,
	//! so they must be skipped.
	private void RunObserve(dmE2EStep step, dmE2EStepResult r)
	{
		array<PlayerBase> players = dmEntityRegistry.GetPlayers();
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase p = players[i];
			if (p && !dmAISurvivorBase.Cast(p))
			{
				p.SetPosition(ResolveWorldPos(step.Pos));
				p.SetOrientation(Vector(step.Yaw, 0, 0));
				r.Ok = true;
				r.Reason = "teleported player";
				return;
			}
		}

		r.Ok = false;
		r.Reason = "no player connected";
	}

	//! Create the parent directory chain of a file path (mirrors dmJsonFile.EnsureDirectory,
	//! but non-generic so it works for versionless e2e structs too).
	private void EnsureDir(string path)
	{
		int lastSlash = path.LastIndexOf("/");
		if (lastSlash < 0)
			return;

		TStringArray comps = new TStringArray();
		path.Substring(0, lastSlash).Split("/", comps);

		int startFrom = 0;
		string dir = "";
		if (comps.Count() > 0 && comps[0] == "$profile:")
		{
			dir = "$profile:";
			startFrom = 1;
		}

		for (int i = startFrom; i < comps.Count(); i++)
		{
			if (dir != "")
				dir += "/";
			dir += comps[i];
			if (!FileExist(dir))
				MakeDirectory(dir);
		}
	}

	//! Write the result to e2e/out/<Name>.result.json (creating the directory).
	private void SaveResult(dmE2EResult result)
	{
		string outPath = DM_E2E_OUT_DIR + "/" + result.Name + ".result.json";
		EnsureDir(outPath);
		string saveError;
		if (!JsonFileLoader<dmE2EResult>.SaveFile(outPath, result, saveError))
			dmBotLog.Error("[E2E] write result failed: " + outPath + ": " + saveError);
	}

	//! Move the processed input to e2e/done/ (copy then delete).
	private void MoveToDone(string jobPath, string fileName)
	{
		string donePath = DM_E2E_DONE_DIR + "/" + fileName;
		EnsureDir(donePath);
		CopyFile(jobPath, donePath);
		DeleteFile(jobPath);
	}

	//! Job id from a file name (strip the ".json" suffix).
	private string BaseName(string fileName)
	{
		string name = fileName;
		if (name.Length() > 5)
			name = name.Substring(0, name.Length() - 5);
		return name;
	}
}
