//! dmE2EBridge — file bridge for E2E auto-tests (phase 3: movement + deferred automaton).
//!
//! An AI agent drops a JSON job into $profile:dmBotorama/e2e/in/, the bridge
//! executes it on the server and writes the result to e2e/out/, then moves the
//! input to e2e/done/. The bridge is inert unless the e2e/enabled marker file
//! exists, so there is zero overhead when no agent is driving.
//!
//! Ops: ping | spawn | moveto | follow | patrol | speed | loadout | look | say |
//! shock | restrain | give | wait | assert | snapshot | clearall (named bots)
//! plus the perf ops sleep | prof | army (two-team fight) | meleefight (machete
//! bot vs zombies) and the world/physics probe ops spawnobj | raycast | scanbox |
//! surfprobe | surfshootout | roadwalk | botdump | getpos | setpos | clearobj (named objects), the car ops spawncar |
//! drive | cardump, and observe (teleport a connected player). `wait` and `sleep`
//! are the deferred ops: they tick across frames (wait until a condition is met
//! or its timeout expires; sleep until its timeout). Everything else executes in
//! a single tick.

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
	string Cond;      // wait/assert condition: state|reached|distance|alive|moving|carpos|carspeed
	string Value;     // condition value (state name / "true"/"false" / lineId for say)
	float Tolerance;  // reached/distance tolerance (meters)
	float Timeout;    // wait timeout / sleep duration (seconds)
	string ClassName; // CfgVehicles class (spawnobj, spawncar)
	string Wheel;     // wheel CfgVehicles class (spawncar)
	vector From;      // raycast start point (world)
	vector To;        // raycast end point (world)
	vector Min;       // scanbox min corner (world)
	vector Max;       // scanbox max corner (world)
	string Obj;       // object name (getpos, setpos, clearobj, drive, cardump, carpos, carspeed)
	int Count;        // number of bots to spawn (army)
	string Settlement; // settlement name for the army center (optional)
	float Radius;     // spawn scatter radius around the center (army, default 50)
	string Preset;    // combat preset: "shooting" (default) | "combat" (army); "nomad" (meleefight)
	float Spread;     // hostile threat blur (army, default DM_INVASION_SPREAD)
	int Zombies;      // number of zombies to spawn (meleefight, default 2)
	float ZombieDist; // zombie spawn distance from the bot (meleefight, default 2.5)
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

	//! Default army spawn scatter radius (meters) when step.Radius is 0.
	static const float DM_E2E_ARMY_RADIUS = 50.0;

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

		if (step.Op == "sleep")
		{
			m_WaitTimer = m_WaitTimer + dt;
			if (m_WaitTimer < step.Timeout)
				return;

			dmE2EStepResult sleepResult = new dmE2EStepResult();
			sleepResult.Index = m_StepIndex;
			sleepResult.Op = step.Op;
			sleepResult.Dump = new array<string>();
			sleepResult.Ok = true;
			sleepResult.Reason = "slept";
			m_Result.Steps.Insert(sleepResult);
			m_WaitTimer = 0.0;
			m_StepIndex = m_StepIndex + 1;
			LogStep(sleepResult);
			return;
		}

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
		else if (step.Op == "shock")
		{
			RunShock(step, r);
		}
		else if (step.Op == "restrain")
		{
			RunRestrain(step, r);
		}
		else if (step.Op == "give")
		{
			RunGive(step, r);
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
		else if (step.Op == "prof")
		{
			RunProf(step, r);
		}
		else if (step.Op == "army")
		{
			RunArmy(step, r);
		}
		else if (step.Op == "meleefight")
		{
			RunMeleeFight(step, r);
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
		else if (step.Op == "surfprobe")
		{
			RunSurfProbe(step, r);
		}
		else if (step.Op == "surfshootout")
		{
			RunSurfShootout(step, r);
		}
		else if (step.Op == "roadwalk")
		{
			RunRoadWalk(step, r);
		}
		else if (step.Op == "roadgraph")
		{
			RunRoadGraph(step, r);
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
		else if (step.Op == "spawncar")
		{
			RunSpawnCar(step, r);
		}
		else if (step.Op == "drive")
		{
			RunDrive(step, r);
		}
		else if (step.Op == "markroute")
		{
			RunMarkRoute(step, r);
		}
		else if (step.Op == "cardump")
		{
			RunCarDump(step, r);
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

	//! "markroute" — расставить бочки-маркеры по точкам маршрута (диагностика
	//! вождения): первая — зелёная, последняя — красная, остальные — жёлтые.
	//! Бочки декоративные (без физики/коллизии) — машине не мешают.
	private void RunMarkRoute(dmE2EStep step, dmE2EStepResult r)
	{
		int count = step.Points.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			string cls = "Barrel_Yellow";
			if (i == 0)
				cls = "Barrel_Green";
			if (i == count - 1)
				cls = "Barrel_Red";

			Object obj = GetGame().CreateObject(cls, ResolveWorldPos(step.Points[i]), false);
			if (obj)
				m_Objects.Insert(step.Who + "_" + i, obj);
		}

		r.Ok = true;
		r.Reason = "marked " + count;
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

	//! "shock" — set the bot's Shock stat (0..100; <=25 knocks it out).
	private void RunShock(dmE2EStep step, dmE2EStepResult r)
	{
		dmAISurvivor bot;
		if (!m_Named.Find(step.Who, bot))
		{
			r.Ok = false;
			r.Reason = "no such bot";
			return;
		}

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
		{
			r.Ok = false;
			r.Reason = "no pawn";
			return;
		}

		pawn.SetHealth("", "Shock", step.Value.ToFloat());
		r.Ok = true;
		r.Reason = "shock set";
	}

	//! "restrain" — restrain the bot (flag + locked restraint item in hands).
	private void RunRestrain(dmE2EStep step, dmE2EStepResult r)
	{
		dmAISurvivor bot;
		if (!m_Named.Find(step.Who, bot))
		{
			r.Ok = false;
			r.Reason = "no such bot";
			return;
		}

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
		{
			r.Ok = false;
			r.Reason = "no pawn";
			return;
		}

		pawn.SetRestrained(true);
		//! "HandcuffsLocked" — конкретный CfgVehicles-класс наручников (находится в руках
		//! цели при ванильном связывании). Абстрактный скриптовый "RestrainingToolLocked"
		//! не имеет config-записи → CreateInHands давал "Bad vehicle type".
		pawn.GetHumanInventory().CreateInHands("HandcuffsLocked");
		pawn.OnItemInHandsChanged();
		r.Ok = true;
		r.Reason = "restrained";
	}

	//! "give" — place the named item class directly in the bot's hands.
	private void RunGive(dmE2EStep step, dmE2EStepResult r)
	{
		dmAISurvivor bot;
		if (!m_Named.Find(step.Who, bot))
		{
			r.Ok = false;
			r.Reason = "no such bot";
			return;
		}

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
		{
			r.Ok = false;
			r.Reason = "no pawn";
			return;
		}

		pawn.GetHumanInventory().CreateInHands(step.Value);
		r.Ok = true;
		r.Reason = "gave " + step.Value;
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

	//! "prof" — control the profiler via Value: start | stop | clear | dump.
	private void RunProf(dmE2EStep step, dmE2EStepResult r)
	{
		if (step.Value == "start")
		{
			dmBotProfiler.SetEnabled(true);
			r.Ok = true;
			r.Reason = "started";
		}
		else if (step.Value == "stop")
		{
			dmBotProfiler.SetEnabled(false);
			r.Ok = true;
			r.Reason = "stopped";
		}
		else if (step.Value == "clear")
		{
			dmBotProfiler.Clear();
			r.Ok = true;
			r.Reason = "cleared";
		}
		else if (step.Value == "dump")
		{
			string path = dmBotProfiler.Dump();
			if (path == "")
			{
				r.Ok = false;
				r.Reason = "empty";
			}
			else
			{
				r.Dump.Insert(path);
				r.Ok = true;
				r.Reason = "dumped";
			}
		}
		else
		{
			r.Ok = false;
			r.Reason = "bad action";
		}
	}

	//! "army" — spawn Count bots in two mutually hostile teams around a center.
	//! Center: step.Settlement (resolved via dmWorldPOIRegistry) or step.Pos.
	private void RunArmy(dmE2EStep step, dmE2EStepResult r)
	{
		if (step.Count <= 0)
		{
			r.Ok = false;
			r.Reason = "bad count";
			return;
		}

		vector center = ResolveWorldPos(step.Pos);
		bool settlementFound = false;
		int i;
		if (step.Settlement != "")
		{
			dmWorldPOIRegistry registry = dmWorldPOIRegistry.Get();
			for (i = 0; i < registry.SettlementCount(); i++)
			{
				dmWorldPoiLocation loc = registry.GetSettlement(i);
				if (loc && loc.Name == step.Settlement)
				{
					settlementFound = true;
					center = SnapToGroundExactly(loc.Position);
					break;
				}
			}
		}
		if (step.Settlement != "" && !settlementFound)
		{
			r.Ok = false;
			r.Reason = "settlement not found: " + step.Settlement;
			return;
		}

		float spread = step.Spread;
		if (spread <= 0.0)
			spread = DM_INVASION_SPREAD;

		float radius = step.Radius;
		if (radius <= 0.0)
			radius = DM_E2E_ARMY_RADIUS;

		int countA = step.Count / 2;
		if (step.Count % 2 == 1)
			countA = countA + 1;

		array<ref dmAISurvivor> teamA = new array<ref dmAISurvivor>();
		array<ref dmAISurvivor> teamB = new array<ref dmAISurvivor>();

		int spawnedA = 0;
		int spawnedB = 0;

		for (i = 0; i < step.Count; i++)
		{
			vector spawnPos = RollArmySpawn(center, radius);
			ref dmAISurvivor bot = new dmAISurvivor();
			bot.SetModel(dmSurvivor.GetRandom());
			PlayerBase pawn = bot.Spawn(spawnPos, Vector(Math.RandomFloat(0.0, 360.0), 0.0, 0.0));
			if (!pawn)
				continue;

			GiveWeapon(pawn);

			if (i < countA)
			{
				m_Named.Insert("army_A_" + spawnedA, bot);
				teamA.Insert(bot);
				spawnedA = spawnedA + 1;
			}
			else
			{
				m_Named.Insert("army_B_" + spawnedB, bot);
				teamB.Insert(bot);
				spawnedB = spawnedB + 1;
			}

			if (step.Preset == "combat")
				bot.SetFSM(dmBotTestPreset_Combat.Create(bot));
			else
				bot.SetFSM(dmBotTestPreset_Shooting.Create(bot));
		}

		int a;
		int b;
		for (a = 0; a < teamA.Count(); a++)
		{
			for (b = 0; b < teamB.Count(); b++)
			{
				teamA[a].RegisterHostile(teamB[b].GetPawn(), 1.0, spread);
				teamB[b].RegisterHostile(teamA[a].GetPawn(), 1.0, spread);
			}
		}

		if (spawnedA + spawnedB == 0)
		{
			r.Ok = false;
			r.Reason = "no bots spawned";
			return;
		}

		r.Ok = true;
		r.Reason = "spawned " + (spawnedA + spawnedB) + " bots (A=" + spawnedA + ", B=" + spawnedB + ")";
	}

	//! Random ground-snapped spawn position within ±radius of the center.
	private vector RollArmySpawn(vector center, float radius)
	{
		float offX = Math.RandomFloat(-radius, radius);
		float offZ = Math.RandomFloat(-radius, radius);
		return SnapToGroundExactly(Vector(center[0] + offX, 0.0, center[2] + offZ));
	}

	//! Random rifle + matching ammo (mirrors dmLaunchCommand.GiveWeapon).
	private void GiveWeapon(PlayerBase pawn)
	{
		array<string> weapons = {"B95", "Mosin9130", "Izh18", "Repeater"};
		int idx = Math.RandomIntInclusive(0, weapons.Count() - 1);
		string weapon = weapons[idx];

		string ammo = "Ammo_308Win";
		if (weapon == "Mosin9130")
			ammo = "Ammo_762x54";
		else if (weapon == "Izh18")
			ammo = "Ammo_762x39";
		else if (weapon == "Repeater")
			ammo = "Ammo_357";

		Weapon_Base w = Weapon_Base.Cast(pawn.GetHumanInventory().CreateInHands(weapon));
		if (w)
			w.SpawnAmmo(ammo, WeaponWithAmmoFlags.CHAMBER);

		pawn.GetInventory().CreateInInventory(ammo);
		pawn.GetInventory().CreateInInventory(ammo);
	}

	//! "meleefight" — spawn an armed bot (machete) plus Zombies brain-driven
	//! zombies scattered around it, registered hostile to the bot.
	private void RunMeleeFight(dmE2EStep step, dmE2EStepResult r)
	{
		ref dmAISurvivor bot = new dmAISurvivor();
		bot.SetModel(dmSurvivor.GetRandom());
		PlayerBase pawn = bot.Spawn(ResolveWorldPos(step.Pos), Vector(step.Yaw, 0, 0));
		if (!pawn)
		{
			r.Ok = false;
			r.Reason = "spawn failed";
			return;
		}

		m_Named.Insert(step.Who, bot);

		pawn.GetHumanInventory().CreateInHands("Machete");

		if (step.Preset == "nomad")
			bot.SetFSM(dmBotPreset_Nomad.Create(bot));
		else
			bot.SetFSM(dmBotPreset_Survivor.Create(bot));

		int zombies = step.Zombies;
		if (zombies <= 0)
			zombies = 2;

		float zdist = step.ZombieDist;
		if (zdist <= 0.0)
			zdist = 2.5;

		vector botPos = bot.GetPosition();
		vector zpos;
		int spawned = 0;
		int i;
		for (i = 0; i < zombies; i++)
		{
			zpos = botPos + Vector(Math.RandomFloat(-zdist, zdist), 0.0, Math.RandomFloat(-zdist, zdist));
			zpos = SnapToGroundExactly(zpos);
			ZombieBase z = ZombieBase.Cast(GetGame().CreateObject("ZmbM_PatrolNormal_Autumn", zpos, false, true));
			if (z)
			{
				bot.RegisterHostile(z, 1.0, DM_INVASION_SPREAD);
				spawned = spawned + 1;
			}
		}

		if (spawned == 0)
		{
			r.Ok = false;
			r.Reason = "no zombies spawned";
			return;
		}

		r.Ok = true;
		r.Reason = "armed bot + " + zombies + " zombies";
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
		Object carObj;
		CarScript car;
		bool fast;

		//! Car conditions operate on a named probe object (m_Objects), not a bot.
		if (step.Cond == "carpos")
		{
			if (!m_Objects.Find(step.Obj, carObj) || !carObj)
			{
				r.Ok = false;
				r.Reason = "no such car";
				return false;
			}
			result = vector.Distance(carObj.GetPosition(), ResolveWorldPos(step.Pos)) < step.Tolerance;
			r.Ok = result;
			r.Reason = "";
			return result;
		}

		if (step.Cond == "carspeed")
		{
			if (!m_Objects.Find(step.Obj, carObj) || !carObj)
			{
				r.Ok = false;
				r.Reason = "no such car";
				return false;
			}
			fast = false;
			car = CarScript.Cast(carObj);
			if (car)
				fast = car.GetSpeedometerAbsolute() > step.Value.ToFloat();
			r.Ok = fast;
			r.Reason = "";
			return fast;
		}

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

	//! "surfprobe" — dump raw surface-sensor readings on a 29-point cross around
	//! step.Pos (center + ±X/±Z at {1,2,3,5,8,12,16} m). Research probe: raw
	//! SurfaceGetType / SurfaceY / SurfaceRoadY / GetSurface(Roadway) values only,
	//! no classification.
	private void RunSurfProbe(dmE2EStep step, dmE2EStepResult r)
	{
		float cx = step.Pos[0];
		float cz = step.Pos[2];

		array<float> dists = {1.0, 2.0, 3.0, 5.0, 8.0, 12.0, 16.0};
		array<vector> dirs = new array<vector>();
		dirs.Insert(Vector(1, 0, 0));
		dirs.Insert(Vector(-1, 0, 0));
		dirs.Insert(Vector(0, 0, 1));
		dirs.Insert(Vector(0, 0, -1));

		int i;
		int d;

		ProbeSample(cx, cz, 0.0, 0.0, r);

		for (i = 0; i < dirs.Count(); i++)
		{
			for (d = 0; d < dists.Count(); d++)
			{
				ProbeSample(cx + dirs[i][0] * dists[d], cz + dirs[i][2] * dists[d], dirs[i][0] * dists[d], dirs[i][2] * dists[d], r);
			}
		}

		r.Ok = true;
		r.Reason = "29 samples";
	}

	//! One surfprobe sample: raw surface sensors at (wx,wz), offset (dx,dz) from
	//! the probe center. Appends one parseable dump line.
	private void ProbeSample(float wx, float wz, float dx, float dz, dmE2EStepResult r)
	{
		float terrainY = GetGame().SurfaceY(wx, wz);
		float roadY = GetGame().SurfaceRoadY(wx, wz);
		float dy = roadY - terrainY;

		string stype = "";
		float surfY = GetGame().SurfaceGetType(wx, wz, stype);

		SurfaceDetectionParameters p = new SurfaceDetectionParameters();
		p.type = SurfaceDetectionType.Roadway;
		p.position = Vector(wx, terrainY + 50.0, wz);
		SurfaceDetectionResult res = new SurfaceDetectionResult();
		bool ok = GetGame().GetSurface(p, res);

		string objType = "null";
		if (res.object)
			objType = res.object.GetType();
		string objSurf = "null";
		if (res.surface)
			objSurf = res.surface.GetName();
		string objSurfType = "null";
		if (res.surface)
			objSurfType = res.surface.GetSurfaceType();

		string line = "off=(" + dx + "," + dz + ")";
		line += " type=\"" + stype + "\"";
		line += " surfY=" + surfY + " terrainY=" + terrainY + " roadY=" + roadY;
		line += " dy=" + dy + " obj=" + objType;
		line += " objSurf=\"" + objSurf + "\" objSurfType=\"" + objSurfType + "\"";
		line += " roadH=" + res.height + " ok=" + ok;
		AppendDump(r, line);
	}

	//! "surfshootout" — one-point shootout of every native surface/road detection
	//! API: SurfaceGetType+friction, GetSurface(Scenery|Roadway), SurfaceRoadY vs
	//! SurfaceY, RaycastRVProxy result.surface, GetObjectsAtPosition+IsRoadObject,
	//! and SceneGetEntitiesInBox(ONLY_ROADWAYS). Research probe to pick the working
	//! API for road-object detection (SurfaceGetType is terrain-only).
	private void RunSurfShootout(dmE2EStep step, dmE2EStepResult r)
	{
		float x;
		float z;
		string line;
		string stype;
		float friction;
		float terrainY;
		SurfaceDetectionParameters p;
		SurfaceDetectionResult res;
		string surfName;
		string surfType;
		string objName;
		float roadY;
		float dy;
		RaycastRVParams rp;
		array<ref RaycastRVResult> hits;
		int i;
		RaycastRVResult hit;
		bool isRoad;
		array<Object> objs;
		array<CargoBase> cargos;
		Object obj;
		array<EntityAI> roads;
		EntityAI road;

		x = step.Pos[0];
		z = step.Pos[2];

		line = "seed=(" + x + "," + z + ")";
		AppendDump(r, line);

		// 1. SurfaceGetType + friction
		stype = "";
		GetGame().SurfaceGetType(x, z, stype);
		friction = GetGame().ConfigGetFloat("CfgSurfaces " + stype + " friction");
		line = "sgt type=\"" + stype + "\" friction=" + friction;
		AppendDump(r, line);

		// 2. GetSurface(Scenery) and GetSurface(Roadway), traced from above.
		terrainY = GetGame().SurfaceY(x, z);
		p = new SurfaceDetectionParameters();
		p.position = Vector(x, terrainY + 50.0, z);
		res = new SurfaceDetectionResult();

		p.type = SurfaceDetectionType.Scenery;
		GetGame().GetSurface(p, res);
		surfName = "null";
		surfType = "null";
		objName = "null";
		if (res.surface)
			surfName = res.surface.GetName();
		if (res.surface)
			surfType = res.surface.GetSurfaceType();
		if (res.object)
			objName = res.object.GetType();
		line = "scenery name=\"" + surfName + "\"";
		line += " stype=\"" + surfType + "\"";
		line += " obj=" + objName;
		AppendDump(r, line);

		p.type = SurfaceDetectionType.Roadway;
		GetGame().GetSurface(p, res);
		surfName = "null";
		surfType = "null";
		objName = "null";
		if (res.surface)
			surfName = res.surface.GetName();
		if (res.surface)
			surfType = res.surface.GetSurfaceType();
		if (res.object)
			objName = res.object.GetType();
		line = "roadway name=\"" + surfName + "\"";
		line += " stype=\"" + surfType + "\"";
		line += " obj=" + objName;
		AppendDump(r, line);

		// 3. Heights
		roadY = GetGame().SurfaceRoadY(x, z);
		dy = roadY - terrainY;
		line = "heights terrainY=" + terrainY + " roadY=" + roadY + " dy=" + dy;
		AppendDump(r, line);

		// 4. RaycastRVProxy from above, dump each hit surface/obj/IsRoadObject.
		rp = new RaycastRVParams(Vector(x, terrainY + 30.0, z), Vector(x, terrainY - 5.0, z));
		hits = new array<ref RaycastRVResult>();
		DayZPhysics.RaycastRVProxy(rp, hits);
		for (i = 0; i < hits.Count(); i++)
		{
			hit = hits[i];
			surfName = "null";
			if (hit.surface)
				surfName = hit.surface.GetName();
			objName = "null";
			if (hit.obj)
				objName = hit.obj.GetType();
			isRoad = dmRoadSensor.IsRoadObject(hit.obj);
			line = "ray i=" + i + " surf=\"" + surfName + "\"";
			line += " obj=" + objName + " isRoad=" + isRoad;
			AppendDump(r, line);
		}

		// 5. GetObjectsAtPosition (r=5 m)
		objs = new array<Object>();
		cargos = new array<CargoBase>();
		GetGame().GetObjectsAtPosition(Vector(x, terrainY, z), 5.0, objs, cargos);
		for (i = 0; i < objs.Count(); i++)
		{
			obj = objs[i];
			objName = obj.GetType();
			isRoad = dmRoadSensor.IsRoadObject(obj);
			line = "near i=" + i + " type=" + objName;
			line += " isRoad=" + isRoad;
			AppendDump(r, line);
		}

		// 6. SceneGetEntitiesInBox(ONLY_ROADWAYS, 10 m box)
		roads = new array<EntityAI>();
		DayZPlayerUtils.SceneGetEntitiesInBox(Vector(x - 5.0, terrainY - 5.0, z - 5.0), Vector(x + 5.0, terrainY + 5.0, z + 5.0), roads, QueryFlags.ONLY_ROADWAYS);
		line = "roadbox count=" + roads.Count();
		AppendDump(r, line);
		for (i = 0; i < roads.Count(); i++)
		{
			road = roads[i];
			objName = road.GetType();
			line = "rb i=" + i + " type=" + objName;
			AppendDump(r, line);
		}

		r.Ok = true;
		r.Reason = "surfshootout";
	}

	//! "roadwalk" — run the road discovery probe from step.Pos and dump every
	//! branch's centerline polyline: status+branch count, then per branch a
	//! "b=" header line followed by one "pt=" line per point.
	private void RunRoadWalk(dmE2EStep step, dmE2EStepResult r)
	{
		dmRoadProbeResult res = dmRoadProbe.Walk(Vector(step.Pos[0], 0.0, step.Pos[2]));
		int branchCount = 0;
		if (res.Branches)
			branchCount = res.Branches.Count();
		AppendDump(r, "status=" + res.Status + " branches=" + branchCount);

		int i;
		int bi;
		string line;
		vector p;
		float w;
		int c;
		if (res.Branches)
		{
			for (bi = 0; bi < res.Branches.Count(); bi++)
			{
				dmRoadBranch b = res.Branches[bi];
				AppendDump(r, "b=" + bi + " status=" + b.Status + " steps=" + b.Steps);
				if (b.Points)
				{
					for (i = 0; i < b.Points.Count(); i++)
					{
						p = b.Points[i];
						w = 0.0;
						if (b.Widths && i < b.Widths.Count())
							w = b.Widths[i];
						c = dmRoadSurfaceClass.ROAD_UNKNOWN;
						if (b.Surfaces && i < b.Surfaces.Count())
							c = b.Surfaces[i];
						line = "pt=(" + p[0] + "," + p[2] + ")";
						line += " y=" + p[1];
						line += " w=" + w + " c=" + c;
						AppendDump(r, line);
					}
				}
			}
		}
		r.Ok = (res.Status == "ok");
		r.Reason = res.Status + " / " + branchCount + " branches / " + res.TotalSteps + " pts";
	}

	//! "roadgraph" — build the road network graph from the seed points, save it
	//! to JSON and dump the graph: nodes summary, one "n=" line per vertex, one
	//! "e=" header + one "ep=" polyline line per edge.
	private void RunRoadGraph(dmE2EStep step, dmE2EStepResult r)
	{
		array<vector> seeds = new array<vector>();
		int i;
		for (i = 0; i < step.Points.Count(); i++)
			seeds.Insert(step.Points[i]);

		dmRoadGraph graph = dmRoadGraphBuilder.Build(seeds);
		bool saved = dmRoadGraphIO.Save(graph, DM_ROADS_GRAPH_FILE);

		int nodeCount = graph.Nodes.Count();
		int edgeCount = graph.Edges.Count();
		AppendDump(r, "nodes=" + nodeCount + " edges=" + edgeCount);

		int ni;
		for (ni = 0; ni < nodeCount; ni++)
		{
			dmRoadGraphNode n = graph.Nodes[ni];
			string nline = "n=" + n.Id + " pos=(" + n.Pos[0] + "," + n.Pos[2] + ") kind=" + n.Kind;
			AppendDump(r, nline);
		}

		int ei;
		float total = 0.0;
		for (ei = 0; ei < edgeCount; ei++)
		{
			dmRoadGraphEdge e = graph.Edges[ei];
			total = total + e.Length;
			string eline = "e=" + e.Id + " from=" + e.From + " to=" + e.To;
			eline = eline + " len=" + e.Length + " surf=" + e.SurfaceType + " pts=" + e.Points.Count();
			AppendDump(r, eline);

			string pline = "";
			int pi;
			for (pi = 0; pi < e.Points.Count(); pi++)
			{
				if (pi > 0)
					pline = pline + " ";
				pline = pline + "ep=(" + e.Points[pi][0] + "," + e.Points[pi][2] + ")";
			}
			AppendDump(r, pline);
		}
		AppendDump(r, "totalLen=" + total);

		r.Ok = saved;
		r.Reason = "" + nodeCount + " nodes / " + edgeCount + " edges";
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

		EntityAI ih = pawn.GetItemInHands();
		string inHandsName = "none";
		if (ih)
			inHandsName = ih.GetType();

		line = "restrained=" + pawn.IsRestrained() + " bleeding=" + pawn.IsBleeding() + " inHands=" + inHandsName;
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

	//! "spawncar" — create a car, outfit it (wheels/battery/spark plug/radiator/
	//! fluids) and release the brakes, registered in m_Objects under step.Who.
	private void RunSpawnCar(dmE2EStep step, dmE2EStepResult r)
	{
		string cls = step.ClassName;
		if (cls == "")
			cls = "CivilianSedan";

		string wheel = step.Wheel;
		if (wheel == "")
			wheel = "CivSedanWheel";

		Object obj = GetGame().CreateObject(cls, ResolveWorldPos(step.Pos), false);
		if (!obj)
		{
			r.Ok = false;
			r.Reason = "spawn failed";
			return;
		}

		CarScript car = CarScript.Cast(obj);
		if (!car)
		{
			EntityAI badEntity = EntityAI.Cast(obj);
			if (badEntity)
				badEntity.DeleteSafe();
			r.Ok = false;
			r.Reason = "not a car";
			return;
		}

		car.SetOrientation(Vector(step.Yaw, 0, 0));

		//! 4 колеса подряд без guard'ов (как ванильный CivilianSedan.OnDebugSpawn:
		//! первые CreateInInventory заполняют хабы), затем 1 запасное в карго.
		car.GetInventory().CreateInInventory(wheel);
		car.GetInventory().CreateInInventory(wheel);
		car.GetInventory().CreateInInventory(wheel);
		car.GetInventory().CreateInInventory(wheel);
		car.GetInventory().CreateInInventory(wheel);

		#ifdef DM_BOT_DEBUG_E2E
		dmBotLog.Debug("[E2E] spawncar wheels: present=" + car.WheelCountPresent() + " count=" + car.WheelCount());
		int i;
		EntityAI wheelEnt;
		string wheelStatus;
		for (i = 0; i < car.WheelCount(); i++)
		{
			wheelEnt = car.WheelGetEntity(i);
			if (wheelEnt)
			{
				wheelStatus = "ok";
				if (wheelEnt.IsRuined())
					wheelStatus = "ruined";
				dmBotLog.Debug("[E2E] spawncar wheel[" + i + "] " + wheelStatus + " " + wheelEnt.GetType());
			}
			else
			{
				dmBotLog.Debug("[E2E] spawncar wheel[" + i + "] null");
			}
		}
		#endif

		EntityAI battery = car.GetBattery();
		if (!battery || battery.IsRuined())
		{
			EntityAI bat = car.GetInventory().CreateInInventory("CarBattery");
			if (bat && bat.HasEnergyManager())
				bat.GetCompEM().AddEnergy(bat.GetCompEM().GetEnergyMax());
		}

		if (!car.FindAttachmentBySlotName("SparkPlug"))
			car.GetInventory().CreateInInventory("SparkPlug");

		if (!car.FindAttachmentBySlotName("CarRadiator"))
			car.GetInventory().CreateInInventory("CarRadiator");

		car.Fill(CarFluid.FUEL, car.GetFluidCapacity(CarFluid.FUEL));
		car.Fill(CarFluid.COOLANT, car.GetFluidCapacity(CarFluid.COOLANT));
		car.Fill(CarFluid.OIL, car.GetFluidCapacity(CarFluid.OIL));
		car.Fill(CarFluid.BRAKE, car.GetFluidCapacity(CarFluid.BRAKE));

		car.SetHandbrake(0.0);
		car.SetBrake(0.0);
		car.ShiftTo(CarGear.NEUTRAL);

		m_Objects.Insert(step.Who, obj);
		r.Ok = true;
		r.Reason = "spawned " + cls;
	}

	//! "drive" — issue a dmBotIntent_Drive: the named bot boards the named car
	//! and drives along the route from Points (or the single Pos if Points is empty).
	private void RunDrive(dmE2EStep step, dmE2EStepResult r)
	{
		dmAISurvivor bot;
		if (!m_Named.Find(step.Who, bot))
		{
			r.Ok = false;
			r.Reason = "no such bot";
			return;
		}

		Object carObj;
		if (!m_Objects.Find(step.Obj, carObj) || !carObj)
		{
			r.Ok = false;
			r.Reason = "no such car";
			return;
		}

		Transport transport = Transport.Cast(carObj);
		if (!transport)
		{
			r.Ok = false;
			r.Reason = "not a transport";
			return;
		}

		dmBotIntent_Drive drive = new dmBotIntent_Drive();
		drive.m_Transport = transport;
		drive.m_Seat = 0;
		drive.m_DriveRoute = new array<vector>();
		if (step.Points.Count() > 0)
		{
			int i;
			for (i = 0; i < step.Points.Count(); i++)
				drive.m_DriveRoute.Insert(ResolveWorldPos(step.Points[i]));
		}
		else
		{
			drive.m_DriveRoute.Insert(ResolveWorldPos(step.Pos));
		}
		bot.AddCommandIntent(drive);

		r.Ok = true;
		r.Reason = "drive issued";
	}

	//! "cardump" — dump the named car's speed/gear/gearbox/RPM/steering/engine.
	private void RunCarDump(dmE2EStep step, dmE2EStepResult r)
	{
		Object obj;
		if (!m_Objects.Find(step.Obj, obj) || !obj)
		{
			r.Ok = false;
			r.Reason = "no such car";
			return;
		}

		CarScript car = CarScript.Cast(obj);
		if (!car)
		{
			r.Ok = false;
			r.Reason = "not a car";
			return;
		}

		string line = "pos=" + car.GetPosition();
		AppendDump(r, line);

		line = "speed=" + car.GetSpeedometerAbsolute();
		AppendDump(r, line);

		line = "wheels=" + car.WheelCountPresent() + "/" + car.WheelCount();
		AppendDump(r, line);

		line = "gear=" + car.GetCurrentGear();
		AppendDump(r, line);

		string gearbox = "MANUAL";
		if (car.GearboxGetType() == CarGearboxType.AUTOMATIC)
			gearbox = "AUTOMATIC";
		line = "gearbox=" + gearbox;
		AppendDump(r, line);

		if (car.GearboxGetType() == CarGearboxType.AUTOMATIC)
		{
			string mode = "D";
			CarAutomaticGearboxMode gm = car.GearboxGetMode();
			if (gm == CarAutomaticGearboxMode.P)
				mode = "P";
			else if (gm == CarAutomaticGearboxMode.R)
				mode = "R";
			else if (gm == CarAutomaticGearboxMode.N)
				mode = "N";
			else
				mode = "D";
			line = "gearmode=" + mode;
			AppendDump(r, line);
		}

		line = "rpm=" + car.EngineGetRPM();
		AppendDump(r, line);

		line = "steer=" + car.GetSteering();
		AppendDump(r, line);

		line = "engine=" + car.EngineIsOn();
		AppendDump(r, line);

		r.Ok = true;
		r.Reason = "dumped";
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
