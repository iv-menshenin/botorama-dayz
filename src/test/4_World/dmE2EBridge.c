//! dmE2EBridge — file bridge for E2E auto-tests (phase 3: movement + deferred automaton).
//!
//! An AI agent drops a JSON job into $profile:dmBotorama/e2e/in/, the bridge
//! executes it on the server and writes the result to e2e/out/, then moves the
//! input to e2e/done/. The bridge is inert unless the e2e/enabled marker file
//! exists, so there is zero overhead when no agent is driving.
//!
//! Ops: ping | spawn | moveto | follow | patrol | speed | loadout | look | say |
//! shock | healplayer | restrain | give | givebayonet | wait | assert | snapshot | clearall (named bots)
//! plus the perf ops sleep | prof | army (two-team fight) | meleefight (machete
//! bot vs zombies) and the world/physics probe ops spawnobj | raycast | scanbox |
//! surfprobe | surfshootout | roadwalk | roadgraph | roadnet | roadobj | botdump | strike | evade | orient | raiseweapon | getpos | setpos | clearobj (named objects), the car ops spawncar |
//! driveto | drive | cardump | carstate | cardamage | carrepair | griddump, and observe (teleport a connected player). `wait`, `sleep`
//! and `drivetest` are the deferred ops: they tick across frames (wait until a
//! condition is met or its timeout expires; sleep until its timeout; drivetest
//! until the bot drives to the exit or a guard aborts). Everything else executes
//! in a single tick.

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
	string Op;        // "ping" | "spawn" | "moveto" | "follow" | "patrol" | ... | "drivetest"
	string Who;       // bot name (spawn, moveto, ...) / object name (spawnobj)
	string Target;    // target bot name (follow, distance, look)
	string Turn;      // look turn mode: "auto" (default, head-first) | "full" | "none"
	vector Pos;       // [x,y,z] world position (spawn, moveto, reached, look, ...)
	float Yaw;        // orientation in degrees (spawn, spawnobj, setpos, move)
	autoptr array<vector> Points;  // patrol points [[x,z],...]
	float Speed;      // preferred movement speed (1..3)
	string Loadout;   // loadout name
	string Cond;      // wait/assert condition: state|reached|distance|alive|moving|carpos|carspeed|enginestate|carspeedavg|driver|intent
	string Value;     // condition value (state name / "true"/"false" / lineId for say)
	float Tolerance;  // reached/distance tolerance (meters)
	float Timeout;    // wait timeout / sleep duration (seconds)
	string ClassName; // CfgVehicles class (spawnobj, spawncar)
	string Wheel;     // wheel CfgVehicles class (spawncar)
	vector From;      // raycast start point (world)
	vector To;        // raycast end point (world)
	string Type;      // raycast intersect type: fire|view|geom|ifire|none (empty -> view)
	float RayRadius;  // raycast ray radius (0 = thin)
	string Flags;     // raycast collision flags: allobjects|nearestcontact|firstcontact|onlystatic|onlydynamic|onlywater (empty -> allobjects)
	bool Sorted;      // raycast sorted flag
	bool GroundOnly;  // raycast groundOnly flag
	vector Min;       // scanbox min corner (world)
	vector Max;       // scanbox max corner (world)
	string Obj;       // object name (getpos, setpos, clearobj, driveto, drive, cardump, carstate, cardamage, carrepair, carpos, carspeed)
	int Count;        // number of bots to spawn (army)
	string Settlement; // settlement name for the army center (optional)
	float Radius;     // spawn scatter radius around the center (army, default 50)
	string Preset;    // FSM preset: "nomad"|"survivor"|"combat"|"escort"|"hunter" (spawn); "shooting" (default) | "combat" (army); "nomad" (meleefight)
	float Spread;     // hostile threat blur (army, default DM_INVASION_SPREAD)
	float Threat;     // threat value for the "hostile" op (default 1.0 when <= 0)
	int Zombies;      // number of zombies to spawn (meleefight, default 2)
	float ZombieDist; // zombie spawn distance from the bot (meleefight, default 2.5)
	vector TargetPos; // griddump end point (world); `string Target` is a bot name, so the end position uses a vector field
	int WidthCells;   // griddump territory width in cells (0 = default 90)
	float LengthMeters; // griddump territory length in meters (0 = default 260)
	bool ImmediateGrid; // drive/driveto: сразу строить сетку объезда (тест-режим)
	float SeatTimeout;    // drivetest: дедлайн посадки (с), default 45
	float StopWindow;     // drivetest: окно скользящего среднего скорости (с), default 30
	float MinSpeed;       // drivetest: порог средней скорости (км/ч), default 0.5
	float IdleRpmMargin;  // drivetest: превышение RPM над idle, чтобы считать мотор "работает" (default 100)
	float GuardGrace;     // drivetest: грейс (с) до включения speed/engine guard'ов (default 10)
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

//! One honeycomb cell in the grid dump (color + world XZ center).
class dmGridDumpCell
{
	int Row;
	int Col;
	int Color;
	float X;
	float Z;
}

//! Grid dump payload (griddump.json): honeycomb territory + route + computed
//! road edges (left/right at ±½ width per route point).
class dmGridDump
{
	vector Origin;
	vector Dir;
	vector Side;
	int Rows;
	int Width;
	int HalfWidth;
	string Status;
	int CellCount;
	ref array<ref dmGridDumpCell> Cells;
	ref array<ref dmGridDumpCell> WaypointCells;
	ref array<vector> Route;
	ref array<vector> LeftEdge;
	ref array<vector> RightEdge;
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

//! Singleton executor: picks up e2e/in/job.json (fixed name) and runs one job at
//! a time. A job is
//! advanced across ticks (deferred `wait` automaton); each tick either advances
//! the current step or stays on it. Ticked by its own self-rescheduling
//! CallLater (Get() schedules the first one-shot; each tick re-schedules the
//! next one explicitly, so the cadence does NOT depend on the repeating flag) —
//! not by MissionServer.OnUpdate, which is unusable across modules in 1.30 (two
//! modded MissionServer blocks both overriding OnUpdate silently drop one).
class dmE2EBridge
{
	static ref dmE2EBridge s_Instance;
	static bool s_TickLogged = false;
	static int s_TickCount = 0;

	//! Default army spawn scatter radius (meters) when step.Radius is 0.
	static const float DM_E2E_ARMY_RADIUS = 50.0;

	//! Self-scheduled tick cadence: interval (ms) and per-tick delta (s). The
	//! bridge no longer rides MissionServer.OnUpdate (1.30 cross-module override
	//! conflict), so Get() schedules the first one-shot CallLater and each tick
	//! re-schedules the next one explicitly. dt == interval: one tick advances
	//! simulated time by one second.
	static const int DM_E2E_TICK_INTERVAL_MS = 1000;
	static const float DM_E2E_TICK_DT = 1.0;

	//! drivetest: fixed window (seconds) for the per-car rolling speed EMA.
	//! step.StopWindow currently does NOT override this (Tick has no current step).
	static const float DM_E2E_STOP_WINDOW = 30.0;

	//! drivetest: defaults for the deferred judge. JsonSerializer не применяет
	//! инициализаторы полей, поэтому нули в step трактуем как эти дефолты.
	static const float DM_E2E_DRIVETEST_MIN_SPEED = 0.5;
	static const float DM_E2E_DRIVETEST_SEAT_TIMEOUT = 45.0;
	static const float DM_E2E_DRIVETEST_GUARD_GRACE = 10.0;
	static const float DM_E2E_DRIVETEST_IDLE_RPM_MARGIN = 100.0;

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

	private int m_DriveTestPhase;         // drivetest: 0=PREP, 1=TEST
	private float m_DriveTestElapsed;     // drivetest: time inside the step
	private float m_DriveTestTestElapsed; // drivetest: time inside TEST phase (GuardGrace)
	private bool m_DriveTestMovedOnce;    // drivetest: latch, car showed speed > MinSpeed
	private ref map<string, float> m_CarSpeedAvg; // drivetest: per-car rolling speed EMA

	static dmE2EBridge Get()
	{
		if (!s_Instance)
		{
			s_Instance = new dmE2EBridge();
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(s_Instance.E2ETick, DM_E2E_TICK_INTERVAL_MS, false);
		}
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
		m_DriveTestPhase = 0;
		m_DriveTestElapsed = 0.0;
		m_DriveTestTestElapsed = 0.0;
		m_DriveTestMovedOnce = false;
		m_CarSpeedAvg = new map<string, float>();
	}

	//! Self-scheduled tick entry (CallLater target). Logs once on the first tick
	//! and a heartbeat every 5th tick so a tester can tell "tick is firing"
	//! apart from "no job.json is present"; then re-schedules the next tick
	//! explicitly (one-shot) so repetition never depends on the repeating flag.
	void E2ETick()
	{
		s_TickCount = s_TickCount + 1;

		if (!s_TickLogged)
		{
			s_TickLogged = true;
			#ifdef DM_BOT_DEBUG_E2E
			dmBotLog.Debug("[E2E] bridge tick active (CallLater)");
			#endif
		}

		#ifdef DM_BOT_DEBUG_E2E
		if (s_TickCount % 5 == 0)
			dmBotLog.Debug("[E2E] tick N=" + s_TickCount);
		#endif

		Tick(DM_E2E_TICK_DT);

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(s_Instance.E2ETick, DM_E2E_TICK_INTERVAL_MS, false);
	}

	void Tick(float dt)
	{
		if (!FileExist(DM_E2E_ENABLED_FILE))
		{
			#ifdef DM_BOT_DEBUG_E2E
			if (s_TickCount % 5 == 0)
				dmBotLog.Debug("[E2E] enabled missing");
			#endif
			return;
		}

		UpdateCarSpeedAvg(dt);

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

		string jobPath = DM_E2E_IN_DIR + "/job.json";
		if (FileExist(jobPath))
			StartJob(jobPath, "job.json");
	}

	//! drivetest: advance the per-car rolling speed EMA (fixed window
	//! DM_E2E_STOP_WINDOW) for every named object that is a CarScript.
	private void UpdateCarSpeedAvg(float dt)
	{
		TStringArray names = m_Objects.GetKeyArray();
		int i;
		for (i = 0; i < names.Count(); i++)
		{
			Object obj;
			if (!m_Objects.Find(names[i], obj) || !obj)
				continue;
			CarScript car = CarScript.Cast(obj);
			if (!car)
				continue;

			float speed = car.GetSpeedometerAbsolute();
			float avg = 0.0;
			m_CarSpeedAvg.Find(names[i], avg);
			avg = avg + (dt / DM_E2E_STOP_WINDOW) * (speed - avg);
			m_CarSpeedAvg.Set(names[i], avg);
		}
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
		m_DriveTestPhase = 0;
		m_DriveTestElapsed = 0.0;
		m_DriveTestTestElapsed = 0.0;
		m_DriveTestMovedOnce = false;
		m_CarSpeedAvg.Clear();
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

		if (step.Op == "drivetest")
		{
			TickDriveTest(step, dt);
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

	//! "drivetest" — deferred judge for the obstacle-avoidance drive builder. PREP
	//! waits for the bot to seat the car (guarding the Drive intent); TEST guards
	//! the intent, driver seat, engine state and rolling speed until the car reaches
	//! the exit. The first violated guard aborts the whole job.
	private void TickDriveTest(dmE2EStep step, float dt)
	{
		dmAISurvivor bot;
		CarScript car;
		PlayerBase pawn;
		HumanCommandVehicle hcv;
		bool gettingIn;
		vector exitPos;
		float minSpeed;
		float seatTimeout;
		float guardGrace;
		float margin;
		float avgSpeed;

		m_DriveTestElapsed = m_DriveTestElapsed + dt;

		if (!m_Named.Find(step.Who, bot))
		{
			AbortDriveTest(step, "no such bot/car");
			return;
		}
		if (!FindCar(step.Obj, car))
		{
			AbortDriveTest(step, "no such bot/car");
			return;
		}
		pawn = bot.GetPawn();
		if (!pawn)
		{
			AbortDriveTest(step, "no such bot/car");
			return;
		}

		//! JsonSerializer не применяет инициализаторы полей — нули трактуем как
		//! «дефолт по умолчанию» (как Radius/Spread в остальных раннерах).
		minSpeed = step.MinSpeed;
		if (minSpeed <= 0.0)
			minSpeed = DM_E2E_DRIVETEST_MIN_SPEED;
		seatTimeout = step.SeatTimeout;
		if (seatTimeout <= 0.0)
			seatTimeout = DM_E2E_DRIVETEST_SEAT_TIMEOUT;
		guardGrace = step.GuardGrace;
		if (guardGrace <= 0.0)
			guardGrace = DM_E2E_DRIVETEST_GUARD_GRACE;

		exitPos = ResolveWorldPos(step.Pos);

		if (!m_DriveTestMovedOnce && car.GetSpeedometerAbsolute() > minSpeed)
			m_DriveTestMovedOnce = true;

		if (m_DriveTestPhase == 0)
		{
			if (!HasCommandIntent(bot, "Drive"))
			{
				AbortDriveTest(step, "intent died in prep (Fail/removed)");
				return;
			}
			//! Переход в TEST только когда бот реально сидит: член экипажа И
			//! get-in анимация завершена (CrewMember кратковременно true во время
			//! get-in, хотя экипаж так и не формируется).
			hcv = pawn.GetCommand_Vehicle();
			gettingIn = false;
			if (hcv != null && hcv.IsGettingIn())
				gettingIn = true;
			if (car.CrewMember(DayZPlayerConstants.VEHICLESEAT_DRIVER) == pawn && !gettingIn)
			{
				#ifdef DM_BOT_DEBUG_E2E
				dmBotLog.Debug("[E2E] drivetest: seated");
				#endif
				m_DriveTestPhase = 1;
				m_DriveTestTestElapsed = 0.0;
			}
			else if (m_DriveTestElapsed > seatTimeout)
			{
				AbortDriveTest(step, "seating timeout");
				return;
			}
			return;
		}

		m_DriveTestTestElapsed = m_DriveTestTestElapsed + dt;

		//! Достижение выхода — приоритетно над guard'ами (успешный финиш): как
		//! только машина в пределах Tolerance, это PASS независимо от состояния
		//! двигателя/интента (Drive-интент штатно финиширует и удаляется из пула,
		//! мотор глохнет, EMA скорости падает — всё это уже не должно абортить).
		if (vector.Distance(car.GetPosition(), exitPos) < step.Tolerance)
		{
			dmE2EStepResult passResult = new dmE2EStepResult();
			passResult.Index = m_StepIndex;
			passResult.Op = step.Op;
			passResult.Dump = new array<string>();
			passResult.Ok = true;
			passResult.Reason = "reached exit";
			m_Result.Steps.Insert(passResult);
			m_StepIndex = m_StepIndex + 1;
			m_DriveTestPhase = 0;
			m_WaitTimer = 0.0;
			LogStep(passResult);
			return;
		}

		//! Выход ещё не достигнут — включаются guard'ы.
		if (!HasCommandIntent(bot, "Drive"))
		{
			AbortDriveTest(step, "intent Fail/removed");
			return;
		}
		if (car.CrewMember(DayZPlayerConstants.VEHICLESEAT_DRIVER) != pawn)
		{
			AbortDriveTest(step, "driver lost (unseat/escape)");
			return;
		}

		if (m_DriveTestMovedOnce || m_DriveTestTestElapsed >= guardGrace)
		{
			avgSpeed = 0.0;
			m_CarSpeedAvg.Find(step.Obj, avgSpeed);
			if (avgSpeed <= minSpeed)
			{
				AbortDriveTest(step, "stop-guard: avg speed 0");
				return;
			}

			margin = step.IdleRpmMargin;
			if (margin == 0.0)
				margin = DM_E2E_DRIVETEST_IDLE_RPM_MARGIN;
			if (!EngineRunning(car, margin))
			{
				AbortDriveTest(step, "engine off / rpm idle");
				return;
			}
		}
	}

	//! Abort the drivetest step (Ok=false) and finish the job immediately. The
	//! job folds to "error" because the step is Ok=false.
	private void AbortDriveTest(dmE2EStep step, string reason)
	{
		string dump = DriveTestCarDump(step);

		#ifdef DM_BOT_DEBUG_E2E
		dmBotLog.Debug("[E2E] drivetest abort: " + reason);
		dmBotLog.Debug("[E2E] drivetest cardump: " + dump);
		#endif

		dmE2EStepResult abortResult = new dmE2EStepResult();
		abortResult.Index = m_StepIndex;
		abortResult.Op = step.Op;
		abortResult.Dump = new array<string>();
		abortResult.Dump.Insert(dump);
		abortResult.Ok = false;
		abortResult.Reason = reason;
		m_Result.Steps.Insert(abortResult);
		LogStep(abortResult);

		FinalizeJob();
	}

	//! True while the bot has a live command intent named `name`. «Живой» =
	//! присутствует в пуле И не IsFailed(). Успешно завершившийся, но ещё не
	//! удалённый пулом интент ловится reach-first PASS в TickDriveTest, поэтому
	//! !IsFailed() достаточно (не путать с «активный»: Fail() ставит и m_Failed,
	//! и m_Finished).
	private bool HasCommandIntent(dmAISurvivor bot, string name)
	{
		dmBotIntentPool pool = bot.GetCommandIntents();
		if (!pool)
			return false;
		ref array<ref dmBotIntent> intents = pool.GetIntents();
		int i;
		for (i = 0; i < intents.Count(); i++)
		{
			dmBotIntent intent = intents[i];
			if (intent && intent.GetIntentName() == name && !intent.IsFailed())
				return true;
		}
		return false;
	}

	//! True while the car engine is running above idle (engine-guard + enginestate).
	private bool EngineRunning(CarScript car, float margin)
	{
		return car.EngineIsOn() && car.EngineGetRPM() > car.EngineGetRPMIdle() + margin;
	}

	//! Resolve a named probe object to a CarScript. Returns false when the object
	//! is not found or is not a CarScript; on success fills `car` (out).
	private bool FindCar(string name, out CarScript car)
	{
		Object obj;
		car = null;
		if (!m_Objects.Find(name, obj) || !obj)
			return false;
		car = CarScript.Cast(obj);
		return car != null;
	}

	//! Reason string for a failed FindCar: "not a car" when the object exists but
	//! is not a CarScript, else "no such car" (keeps car-conditions consistent).
	private string CarFailReason(string name)
	{
		Object obj;
		if (m_Objects.Find(name, obj) && obj)
			return "not a car";
		return "no such car";
	}

	//! One-line car dump (pos/speed/engine/rpm/gear/driver) for the abort result.
	private string DriveTestCarDump(dmE2EStep step)
	{
		CarScript car;
		if (!FindCar(step.Obj, car))
			return "car=none";
		string driverState = "absent";
		if (car.CrewMember(DayZPlayerConstants.VEHICLESEAT_DRIVER) != null)
			driverState = "present";
		string line = "pos=" + car.GetPosition() + " speed=" + car.GetSpeedometerAbsolute();
		line = line + " engine=" + car.EngineIsOn() + " rpm=" + car.EngineGetRPM();
		line = line + " gear=" + car.GetCurrentGear() + " driver=" + driverState;
		return line;
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
		else if (step.Op == "healplayer")
		{
			RunHealPlayer(step, r);
		}
		else if (step.Op == "restrain")
		{
			RunRestrain(step, r);
		}
		else if (step.Op == "give")
		{
			RunGive(step, r);
		}
		else if (step.Op == "givebayonet")
		{
			RunGiveBayonet(step, r);
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
		else if (step.Op == "hostile")
		{
			RunHostile(step, r);
		}
		else if (step.Op == "bullet")
		{
			RunBullet(step, r);
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
		else if (step.Op == "roadnet")
		{
			RunRoadNet(step, r);
		}
		else if (step.Op == "roadobj")
		{
			RunRoadObj(step, r);
		}
		else if (step.Op == "botdump")
		{
			RunBotDump(step, r);
		}
		else if (step.Op == "strike")
		{
			RunStrike(step, r);
		}
		else if (step.Op == "evade")
		{
			RunEvade(step, r);
		}
		else if (step.Op == "orient")
		{
			RunOrient(step, r);
		}
		else if (step.Op == "raiseweapon")
		{
			RunRaiseWeapon(step, r);
		}
		else if (step.Op == "move")
		{
			RunMove(step, r);
		}
		else if (step.Op == "poidump")
		{
			RunPoiDump(step, r);
		}
		else if (step.Op == "worldnames")
		{
			RunWorldNames(step, r);
		}
		else if (step.Op == "buildingdump")
		{
			RunBuildingDump(step, r);
		}
		else if (step.Op == "explorerdump")
		{
			RunExplorerDump(step, r);
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
		else if (step.Op == "driveto")
		{
			RunDriveTo(step, r);
		}
		else if (step.Op == "markroute")
		{
			RunMarkRoute(step, r);
		}
		else if (step.Op == "cardump")
		{
			RunCarDump(step, r);
		}
		else if (step.Op == "carstate")
		{
			RunCarState(step, r);
		}
		else if (step.Op == "cardamage")
		{
			RunCarDamage(step, r);
		}
		else if (step.Op == "carrepair")
		{
			RunCarRepair(step, r);
		}
		else if (step.Op == "griddump")
		{
			RunGridDump(step, r);
		}
		else if (step.Op == "dumpgrid")
		{
			RunDumpGridState(step, r);
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
			GivePants(pawn);
			m_Named.Insert(step.Who, bot);
			if (step.Preset == "nomad")
				bot.SetFSM(dmBotPreset_Nomad.Create(bot));
			else if (step.Preset == "survivor")
				bot.SetFSM(dmBotPreset_Survivor.Create(bot));
			else if (step.Preset == "combat")
				bot.SetFSM(dmBotPreset_Combat.Create(bot));
			else if (step.Preset == "escort")
				bot.SetFSM(dmBotPreset_Escort.Create(bot));
			else if (step.Preset == "hunter")
				bot.SetFSM(dmBotPreset_Hunter.Create(bot));
			else if (step.Preset == "medical")
				bot.SetFSM(dmBotTestPreset_Medical.Create(bot));
			else if (step.Preset == "kamping")
				bot.SetFSM(dmBotTestPreset_Kamping.Create(bot));
			else if (step.Preset == "flanking")
				bot.SetFSM(dmBotTestPreset_Flanking.Create(bot));
			else if (step.Preset == "hunting")
				bot.SetFSM(dmBotTestPreset_Hunting.Create(bot));
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

	//! "hostile" — register Who as hostile toward Target (threat, blur spread).
	private void RunHostile(dmE2EStep step, dmE2EStepResult r)
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

		float threat = step.Threat;
		if (threat <= 0.0)
			threat = 1.0;

		float spread = step.Spread;
		if (spread <= 0.0)
			spread = DM_INVASION_SPREAD;

		bot.RegisterHostile(target.GetPawn(), threat, spread);

		r.Ok = true;
		r.Reason = "hostile " + step.Who + " -> " + step.Target + " threat=" + threat;
	}

	//! "bullet" — детерминированное огнестрельное ранение. Штатный путь: урон через
	//! ProcessDirectDamage(FIRE_ARM) → EEHitBy → RegisterBulletHit (флаг + brown zone)
	//! и ванильное кровотечение. Но эмпирически НЕ подтверждено, что ProcessDirectDamage
	//! прогонит EEHitBy и поставит флаг/кровотечение, поэтому итог гарантирован: после op
	//! бот обязан иметь IsBulletDanger()==true, IsBleeding()==true и >=1 зону. Если штатный
	//! путь не дал — RegisterBulletHit() + AttemptAddBleedingSourceBySelection("Pelvis")
	//! напрямую (допустимо для детерминизма теста; «честный стрелок» — отдельная задача).
	//! Опциональный Pos = позиция источника угрозы: там спавнится «безмозглый» зомби
	//! (init_ai=false, не ходит/не атакует) и регистрируется враждебным, чтобы dmCoverFinder
	//! имел осмысленный threatPoint. Без Pos — фолбэк (угроза не регистрируется).
	private void RunBullet(dmE2EStep step, dmE2EStepResult r)
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

		EntityAI source = null;
		if (step.Pos != vector.Zero)
		{
			ZombieBase z = ZombieBase.Cast(GetGame().CreateObject("ZmbM_PatrolNormal_Autumn", SnapToGroundExactly(step.Pos), false, false));
			if (z)
			{
				SetProbeLifetime(z);
				bot.RegisterHostile(z, 1.0, DM_INVASION_SPREAD);
				source = z;
			}
		}

		pawn.ProcessDirectDamage(DamageType.FIRE_ARM, source, "", "Bullet_762x39", pawn.GetPosition(), 0.3);

		if (!bot.IsBulletDanger())
			bot.RegisterBulletHit();

		if (!pawn.IsBleeding())
			pawn.GetBleedingManagerServer().AttemptAddBleedingSourceBySelection("Pelvis");

		r.Ok = true;
		r.Reason = "bullet applied";
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
			{
				EntityAI entity = EntityAI.Cast(obj);
				if (entity)
					SetProbeLifetime(entity);
				m_Objects.Insert(step.Who + "_" + i, obj);
			}
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
		look.m_Turn = dmBotLookTurn.AUTO;
		if (step.Turn == "full")
			look.m_Turn = dmBotLookTurn.FULL;
		else if (step.Turn == "none")
			look.m_Turn = dmBotLookTurn.NONE;
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

	//! "healplayer" — refill the HUMAN observer's vitals (health/shock/water/
	//! energy) so a long test doesn't kill it — a dead player cannot be revived
	//! programmatically. AI survivors (dmAISurvivorBase) pollute the player
	//! registry, so they must be skipped (same filter as observe).
	private void RunHealPlayer(dmE2EStep step, dmE2EStepResult r)
	{
		array<PlayerBase> players = dmEntityRegistry.GetPlayers();
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase p = players[i];
			if (!p || dmAISurvivorBase.Cast(p))
				continue;

			p.SetHealth("", "Health", 100.0);
			p.SetHealth("", "Shock", 100.0);

			PlayerStat<float> water = p.GetStatWater();
			PlayerStat<float> energy = p.GetStatEnergy();
			float waterMax = 5000.0;
			float energyMax = 5000.0;
			if (water)
				waterMax = water.GetMax();
			if (energy)
				energyMax = energy.GetMax();

			float waterVal = 0.0;
			float energyVal = 0.0;
			if (water)
			{
				water.Set(waterMax);
				waterVal = water.Get();
			}
			if (energy)
			{
				energy.Set(energyMax);
				energyVal = energy.Get();
			}

			string name = "unknown";
			if (p.GetIdentity())
				name = p.GetIdentity().GetName();
			r.Ok = true;
			r.Reason = "healed " + name + " at " + p.GetPosition();

			string line = "healplayer health=" + p.GetHealth("", "Health") + " shock=" + p.GetHealth("", "Shock");
			AppendDump(r, line);
			line = "healplayer water=" + waterVal + " energy=" + energyVal;
			AppendDump(r, line);
			return;
		}

		r.Ok = false;
		r.Reason = "no player connected";
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

	//! "givebayonet" — дать боту винтовку (Mosin9130) с НАДЕТЫМ штык-ножом
	//! (Mosin_Bayonet) в дульном слоте weaponMuzzleMosin, чтобы HasBayonetAttached()
	//! вернул true и мили-удар стал WPN_STAB. Фолбэк: если CreateAttachmentEx не
	//! триггерит OnWasAttached (не ставит SetBayonetAttached), ставим флаг вручную.
	private void RunGiveBayonet(dmE2EStep step, dmE2EStepResult r)
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

		Weapon_Base w = Weapon_Base.Cast(pawn.GetHumanInventory().CreateInHands("Mosin9130"));
		if (!w)
		{
			r.Ok = false;
			r.Reason = "weapon create failed";
			return;
		}

		int slotId = InventorySlots.GetSlotIdFromString("weaponMuzzleMosin");
		w.GetInventory().CreateAttachmentEx("Mosin_Bayonet", slotId);

		if (!w.HasBayonetAttached())
			w.SetBayonetAttached(true, slotId);

		string line = "weapon=" + w.GetType() + " bayonetSlot=" + slotId + " bayonet=" + w.HasBayonetAttached();
		AppendDump(r, line);

		r.Ok = true;
		r.Reason = "gave bayonet rifle";
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

			GivePants(pawn);
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

	//! Дать тест-боту штаны с карго — без одежды CreateInInventory некуда класть
	//! предметы (бот спавнится голым). GorkaPants_Autumn — штаны с карманами
	//! (тот же класс, что в SurvivorLoadout).
	private void GivePants(PlayerBase pawn)
	{
		pawn.GetInventory().CreateAttachmentEx("GorkaPants_Autumn", InventorySlots.LEGS);
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

		GivePants(pawn);
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
		Transport transport;
		float margin;
		float avgSpeed;

		//! Car conditions operate on a named probe object (m_Objects), not a bot.
		if (step.Cond == "carpos")
		{
			if (!FindCar(step.Obj, car))
			{
				r.Ok = false;
				r.Reason = CarFailReason(step.Obj);
				return false;
			}
			result = vector.Distance(car.GetPosition(), ResolveWorldPos(step.Pos)) < step.Tolerance;
			r.Ok = result;
			r.Reason = "";
			return result;
		}

		if (step.Cond == "carspeed")
		{
			if (!FindCar(step.Obj, car))
			{
				r.Ok = false;
				r.Reason = CarFailReason(step.Obj);
				return false;
			}
			fast = car.GetSpeedometerAbsolute() > step.Value.ToFloat();
			r.Ok = fast;
			r.Reason = "";
			return fast;
		}

		if (step.Cond == "enginestate")
		{
			if (!FindCar(step.Obj, car))
			{
				r.Ok = false;
				r.Reason = CarFailReason(step.Obj);
				return false;
			}
			margin = step.IdleRpmMargin;
			if (margin == 0.0)
				margin = DM_E2E_DRIVETEST_IDLE_RPM_MARGIN;
			result = EngineRunning(car, margin);
			r.Ok = result;
			r.Reason = "";
			return result;
		}

		if (step.Cond == "carspeedavg")
		{
			if (!FindCar(step.Obj, car))
			{
				r.Ok = false;
				r.Reason = CarFailReason(step.Obj);
				return false;
			}
			avgSpeed = 0.0;
			m_CarSpeedAvg.Find(step.Obj, avgSpeed);
			fast = avgSpeed > step.Value.ToFloat();
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
		else if (step.Cond == "driver")
		{
			if (m_Objects.Find(step.Obj, carObj) && carObj)
			{
				transport = Transport.Cast(carObj);
				pawn = bot.GetPawn();
				if (transport && pawn && transport.CrewMember(DayZPlayerConstants.VEHICLESEAT_DRIVER) == pawn)
					result = true;
			}
		}
		else if (step.Cond == "intent")
		{
			result = HasCommandIntent(bot, step.Value);
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

	//! Give a freshly spawned probe object a long Central Economy lifetime so it
	//! is not silently cleaned up mid-test. Lifetime is the remaining seconds
	//! before cleanup, so SetLifetime(0) would expire it immediately (not
	//! "infinite") — a large positive value (1 hour) keeps it alive for the test.
	private void SetProbeLifetime(EntityAI entity)
	{
		entity.SetLifetime(3600.0);
		entity.SetLifetimeMax(3600.0);
	}

	//! Spawn an arbitrary CfgVehicles object at a ground-snapped position,
	//! registered under the step's Who name (in m_Objects, not m_Named).
	private void RunSpawnObj(dmE2EStep step, dmE2EStepResult r)
	{
		Object obj = GetGame().CreateObject(step.ClassName, ResolveWorldPos(step.Pos), false);
		if (obj)
		{
			obj.SetOrientation(Vector(step.Yaw, 0, 0));
			EntityAI entity = EntityAI.Cast(obj);
			if (entity)
				SetProbeLifetime(entity);
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

	//! Raycast between two points (ground-snapped, raised by DM_E2E_EYE_HEIGHT).
	//! Every hit is dumped in full (obj/parent/pos/dir/dist/component/hier/
	//! entry/exit/surface/surfaceType) so the detector obstacle diagnostics can
	//! see exactly what each ray touches.
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

		RaycastRVParams params = new RaycastRVParams(fromPos, toPos, null);
		params.type = ResolveIntersectType(step.Type);
		params.flags = ResolveCollisionFlags(step.Flags);
		params.radius = step.RayRadius;
		params.sorted = step.Sorted;
		params.groundOnly = step.GroundOnly;

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
			string surfName = "null";
			if (hit.surface)
				surfName = hit.surface.GetName();
			string surfType = "null";
			if (hit.surface)
				surfType = hit.surface.GetSurfaceType();
			float dist = vector.Distance(fromPos, hit.pos);

			string line = "hit: obj=" + objName + " parent=" + parentName;
			line += " pos=" + hit.pos;
			line += " dir=" + hit.dir;
			line += " dist=" + dist;
			line += " component=" + hit.component;
			line += " hier=" + hit.hierLevel;
			line += " entry=" + hit.entry;
			line += " exit=" + hit.exit;
			line += " surf=" + surfName;
			line += " surfType=" + surfType;
			AppendDump(r, line);
		}

		r.Ok = true;
		if (hits.Count() == 0)
			r.Reason = "0 hits (clear)";
		else
			r.Reason = hits.Count().ToString() + " hits";
	}

	//! Map the raycast "Type" string to an ObjIntersect enum; empty -> ObjIntersectView.
	private int ResolveIntersectType(string t)
	{
		if (t == "fire")
			return ObjIntersectFire;
		if (t == "view")
			return ObjIntersectView;
		if (t == "geom")
			return ObjIntersectGeom;
		if (t == "ifire")
			return ObjIntersectIFire;
		if (t == "none")
			return ObjIntersectNone;
		return ObjIntersectView;
	}

	//! Map the raycast "Flags" string to a CollisionFlags enum; empty -> ALLOBJECTS.
	private CollisionFlags ResolveCollisionFlags(string f)
	{
		if (f == "allobjects")
			return CollisionFlags.ALLOBJECTS;
		if (f == "nearestcontact")
			return CollisionFlags.NEARESTCONTACT;
		if (f == "firstcontact")
			return CollisionFlags.FIRSTCONTACT;
		if (f == "onlystatic")
			return CollisionFlags.ONLYSTATIC;
		if (f == "onlydynamic")
			return CollisionFlags.ONLYDYNAMIC;
		if (f == "onlywater")
			return CollisionFlags.ONLYWATER;
		return CollisionFlags.ALLOBJECTS;
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
		p.position = Vector(x, terrainY, z);
		res = new SurfaceDetectionResult();

		p.type = SurfaceDetectionType.Scenery;
		p.rsd = RoadSurfaceDetection.CLOSEST;
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
		p.rsd = RoadSurfaceDetection.CLOSEST;
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
			eline = eline + " len=" + e.Length + " surf=" + e.SurfaceCategory + " fric=" + e.AvgFriction;
			eline = eline + " pts=" + e.Points.Count();
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

	//! "roadnet" — build the road graph from road objects (memory points) in the
	//! Min..Max area (fallback: Pos ±500 m when Min/Max are zero), save it to JSON
	//! and dump it like roadgraph: nodes summary, one "n=" line per vertex, one
	//! "e=" header + one "ep=" polyline line per edge.
	private void RunRoadNet(dmE2EStep step, dmE2EStepResult r)
	{
		vector min;
		vector max;
		if (step.Min == vector.Zero && step.Max == vector.Zero)
		{
			vector center = ResolveWorldPos(step.Pos);
			min = center - Vector(500.0, 0.0, 500.0);
			max = center + Vector(500.0, 0.0, 500.0);
		}
		else
		{
			min = ResolveWorldPos(step.Min);
			max = ResolveWorldPos(step.Max);
		}

		float t0 = GetGame().GetTickTime();
		dmRoadGraph graph = dmRoadObjectGraphBuilder.Build(min, max);
		float t1 = GetGame().GetTickTime();
		bool saved = dmRoadGraphIO.Save(graph, DM_ROADS_GRAPH_FILE);

		dmRoadGapList gaps = dmRoadGapDetector.Detect(graph);
		dmRoadGapIO.Save(gaps, DM_ROADS_GAP_FILE);
		AppendDump(r, "gaps=" + gaps.Gaps.Count());

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
			eline = eline + " len=" + e.Length + " surf=" + e.SurfaceCategory + " fric=" + e.AvgFriction;
			eline = eline + " rise=" + e.Rise + " fall=" + e.Fall + " obs=" + e.Obstacles.Count();
			eline = eline + " pts=" + e.Points.Count();
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
		AppendDump(r, "buildTime=" + (t1 - t0));

		r.Ok = saved;
		r.Reason = "" + nodeCount + " nodes / " + edgeCount + " edges";
	}

	//! "roadobj" — dump the road objects (class=="road") near a point: position,
	//! the segment-end memory points (LB/PB/LE/PE/LD/LH/PD/PH) and the bounding
	//! box. Research probe for road-object geometry: roads are class=="road"
	//! proxies with an empty GetType(), so their geometry is read from the memory
	//! points and ClippingInfo.
	private void RunRoadObj(dmE2EStep step, dmE2EStepResult r)
	{
		float x = step.Pos[0];
		float z = step.Pos[2];
		float terrainY = GetGame().SurfaceY(x, z);

		array<Object> objs = new array<Object>();
		array<CargoBase> cargos = new array<CargoBase>();
		GetGame().GetObjectsAtPosition(Vector(x, terrainY, z), 12.0, objs, cargos);

		array<string> memPoints = {"LB", "PB", "LE", "PE", "LD", "LH", "PD", "PH"};

		int roadCount = 0;
		int i;
		int mi;
		Object obj;
		bool isRoad;
		string line;
		vector mp;
		vector mm[2];
		float rad;

		for (i = 0; i < objs.Count(); i++)
		{
			obj = objs[i];
			isRoad = dmRoadSensor.IsRoadObject(obj);
			if (isRoad)
			{
				roadCount = roadCount + 1;
				line = "robj i=" + i + " type=\"" + obj.GetType() + "\" pos=" + obj.GetPosition();
				AppendDump(r, line);

				for (mi = 0; mi < memPoints.Count(); mi++)
				{
					if (obj.MemoryPointExists(memPoints[mi]))
					{
						mp = obj.ModelToWorld(obj.GetMemoryPointPos(memPoints[mi]));
						line = "  mp " + memPoints[mi] + "=" + mp;
						AppendDump(r, line);
					}
				}

				rad = obj.ClippingInfo(mm);
				line = "  clip min=" + mm[0] + " max=" + mm[1] + " rad=" + rad;
				AppendDump(r, line);
			}
			else
			{
				line = "obj i=" + i + " type=\"" + obj.GetType() + "\" isRoad=false";
				AppendDump(r, line);
			}
		}

		r.Ok = true;
		r.Reason = roadCount.ToString() + " road objects";
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

		bool bayonet = false;
		if (ih)
			bayonet = ih.HasBayonetAttached();
		line = "bayonet=" + bayonet;
		AppendDump(r, line);

		line = "health=" + pawn.GetHealth01() + " blood=" + pawn.GetHealth("", "Blood") + " shock=" + pawn.GetHealth("", "Shock");
		AppendDump(r, line);

		line = "healthRaw=" + pawn.GetHealth("", "Health");
		AppendDump(r, line);

		line = "bulletDanger=" + bot.IsBulletDanger();
		AppendDump(r, line);

		line = "brownZones=" + bot.GetBrownZone().Count();
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

		dmAISurvivorBase aiPawn = dmAISurvivorBase.Cast(pawn);
		bool meleeStriking = false;
		bool meleeEvading = false;
		bool receivedHit = false;
		if (aiPawn)
		{
			meleeStriking = aiPawn.IsMeleeStriking();
			meleeEvading = aiPawn.IsMeleeEvade();
			receivedHit = aiPawn.ReceivedHit();
		}
		line = "meleeStriking=" + meleeStriking;
		AppendDump(r, line);
		line = "meleeEvading=" + meleeEvading;
		AppendDump(r, line);
		line = "receivedHit=" + receivedHit;
		AppendDump(r, line);

		dmBotFSM fsm = bot.GetFSM();
		string fsmName = "none";
		if (fsm && fsm.GetCurrentState())
			fsmName = fsm.GetCurrentState().GetName();
		line = "fsm=" + fsmName;
		AppendDump(r, line);

		line = "fsmIntents=" + bot.GetFSMIntents().Count();
		AppendDump(r, line);

		dmBotIntentPool cmdPool = bot.GetCommandIntents();
		string driveState = "none";
		if (cmdPool)
		{
			ref array<ref dmBotIntent> cmdIntents = cmdPool.GetIntents();
			int i;
			for (i = 0; i < cmdIntents.Count(); i++)
			{
				dmBotIntent cmdIntent = cmdIntents[i];
				if (cmdIntent && cmdIntent.GetIntentName() == "Drive")
				{
					if (cmdIntent.IsFailed())
						driveState = "failed";
					else
						driveState = "active";
					break;
				}
			}
		}
		line = "commandIntents=" + bot.GetCommandIntents().Count() + " drive=" + driveState;
		AppendDump(r, line);

		r.Ok = true;
		r.Reason = "dumped";
	}

	//! "strike" — request one melee strike against a target bot by calling the pawn
	//! directly (dmAISurvivorBase.RequestMeleeAttack). No brain/FSM involved: the
	//! one-shot flag is consumed by dmBotMeleeFightLogic_LightHeavy.HandleFightLogic
	//! on the next CommandHandler and the MELEE2 command starts (melee.md §5).
	private void RunStrike(dmE2EStep step, dmE2EStepResult r)
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

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
		{
			r.Ok = false;
			r.Reason = "no pawn";
			return;
		}

		PlayerBase targetPawn = target.GetPawn();
		if (!targetPawn)
		{
			r.Ok = false;
			r.Reason = "no target pawn";
			return;
		}

		pawn.RequestMeleeAttack(targetPawn);

		#ifdef DM_BOT_DEBUG_FIGHT
		dmBotLog.Debug("[E2E] strike " + step.Who + " -> " + step.Target);
		#endif

		string line = "strike requested target=" + targetPawn.GetType();
		AppendDump(r, line);

		r.Ok = true;
		r.Reason = "strike requested";
	}

	//! "evade" — request one melee evade (roll) by calling the pawn directly
	//! (dmAISurvivorBase.RequestMeleeEvade). No brain/FSM involved: the one-shot
	//! flag is consumed by dmBotMeleeFightLogic_LightHeavy.HandleFightLogic on the
	//! next CommandHandler, which starts a native StartMeleeEvadeA roll. Yaw = the
	//! evade angle relative to the body (-90 left / +90 right).
	private void RunEvade(dmE2EStep step, dmE2EStepResult r)
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

		pawn.RequestMeleeEvade(step.Yaw);

		#ifdef DM_BOT_DEBUG_FIGHT
		dmBotLog.Debug("[E2E] evade " + step.Who + " angle=" + step.Yaw);
		#endif

		string line = "evade requested angle=" + step.Yaw;
		AppendDump(r, line);

		r.Ok = true;
		r.Reason = "evade requested";
	}

	//! "orient" — set the named bot's body orientation AND its target body yaw in
	//! one go, so ApplyBodyTurn does not foot-step it back to the old facing.
	//! Spawn() only sets the physical orientation and leaves m_TargetBodyYaw at 0,
	//! so a fresh pawn would otherwise turn back toward yaw 0 — that turn noise is
	//! exactly what fast-exits the native melee command (clean strike context).
	private void RunOrient(dmE2EStep step, dmE2EStepResult r)
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

		bot.SetOrientation(Vector(step.Yaw, 0, 0));
		pawn.SetTargetBodyYaw(step.Yaw);

		#ifdef DM_BOT_DEBUG_FIGHT
		dmBotLog.Debug("[E2E] orient " + step.Who + " yaw=" + step.Yaw);
		#endif

		string line = "orient yaw=" + step.Yaw + " actual=" + bot.GetOrientation();
		AppendDump(r, line);

		r.Ok = true;
		r.Reason = "oriented yaw=" + step.Yaw;
	}

	//! "raiseweapon" — raise/lower the bot's weapon by calling the pawn directly
	//! (dmAISurvivorBase.RaiseWeapon). Value = "on"/"true" (raise) or "off"/"false"
	//! (lower). No brain/FSM involved.
	private void RunRaiseWeapon(dmE2EStep step, dmE2EStepResult r)
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

		bool up = true;
		if (step.Value == "off" || step.Value == "false")
			up = false;

		pawn.RaiseWeapon(up);

		string state = "on";
		if (!up)
			state = "off";

		#ifdef DM_BOT_DEBUG_FIGHT
		dmBotLog.Debug("[E2E] raiseweapon " + step.Who + " raise=" + state);
		#endif

		string line = "raise=" + state;
		AppendDump(r, line);

		r.Ok = true;
		r.Reason = "raise=" + state;
	}

	//! "move" — set the named bot's desired movement by calling the pawn directly
	//! (dmAISurvivorBase.SetMove). Yaw = movement direction relative to the body
	//! (0 forward, ±90 strafe, ±180 backward); Speed = 1..3. No brain/FSM involved:
	//! the desired state is applied by ApplyMovement each frame. Used by the melee
	//! block test to drive the bot backwards while raised.
	private void RunMove(dmE2EStep step, dmE2EStepResult r)
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

		pawn.SetMove(step.Yaw, step.Speed);

		#ifdef DM_BOT_DEBUG_FIGHT
		dmBotLog.Debug("[E2E] move " + step.Who + " yaw=" + step.Yaw + " speed=" + step.Speed);
		#endif

		string line = "move yaw=" + step.Yaw + " speed=" + step.Speed;
		AppendDump(r, line);

		r.Ok = true;
		r.Reason = "moved yaw=" + step.Yaw + " speed=" + step.Speed;
	}

	//! "poidump" — dump the POI registry: total location count and the location at
	//! step.Pos (Id:Name:Type) or "none".
	private void RunPoiDump(dmE2EStep step, dmE2EStepResult r)
	{
		dmWorldPOIRegistry reg = dmWorldPOIRegistry.Get();
		AppendDump(r, "total=" + reg.LocationCount());

		vector pos = ResolveWorldPos(step.Pos);
		dmWorldPoiLocation loc = reg.GetLocationAt(pos);
		if (loc)
			AppendDump(r, "at=" + loc.Id + ":" + loc.Name + ":" + loc.Type);
		else
			AppendDump(r, "at=none");

		r.Ok = true;
	}

	//! "worldnames" — dump the raw CfgWorlds <world> Names config: world name,
	//! child count, then one pipe-delimited line per named location
	//! (index|key|name|type|x|z). Raw offline source for world_poi.json.
	private void RunWorldNames(dmE2EStep step, dmE2EStepResult r)
	{
		string path = "CfgWorlds " + g_Game.GetWorldName() + " Names";
		int count = g_Game.ConfigGetChildrenCount(path);
		AppendDump(r, "world=" + g_Game.GetWorldName());
		AppendDump(r, "count=" + count);

		int i;
		string childName;
		string name;
		string type;
		string childPath;
		array<float> pos;
		string line;
		for (i = 0; i < count; i++)
		{
			childName = "";
			g_Game.ConfigGetChildName(path, i, childName);
			childPath = path + " " + childName;

			name = "";
			type = "";
			g_Game.ConfigGetText(childPath + " name", name);
			g_Game.ConfigGetText(childPath + " type", type);

			pos = new array<float>();
			g_Game.ConfigGetFloatArray(childPath + " position", pos);

			line = "" + i + "|" + childName;
			line = line + "|" + name + "|" + type + "|";
			if (pos && pos.Count() == 2)
				line = line + pos[0] + "|" + pos[1];
			else
				line = line + "-1|-1";
			AppendDump(r, line);
		}

		r.Ok = true;
	}

	//! "buildingdump" — dump the live building registry: flat count, location-map
	//! count, then the location at step.Pos and its bound-building count.
	private void RunBuildingDump(dmE2EStep step, dmE2EStepResult r)
	{
		dmLiveBuildingRegistry reg = dmLiveBuildingRegistry.Get();
		AppendDump(r, "registered=" + reg.m_Buildings.Count());
		AppendDump(r, "byLoc=" + reg.m_ByLocation.Count());

		vector pos = ResolveWorldPos(step.Pos);
		dmWorldPoiLocation loc = dmWorldPOIRegistry.Get().GetLocationAt(pos);
		if (loc)
		{
			AppendDump(r, "locId=" + loc.Id);
			array<Building> buildings = new array<Building>();
			reg.GetBuildingsForLocation(loc, buildings);
			AppendDump(r, "locCount=" + buildings.Count());
		}
		else
		{
			AppendDump(r, "locId=-1");
		}

		r.Ok = true;
	}

	//! "explorerdump" — dump the bot's explorer memory: current location, visited
	//! locations, building counts for the current location, flags and timer.
	private void RunExplorerDump(dmE2EStep step, dmE2EStepResult r)
	{
		dmAISurvivor bot;
		if (!m_Named.Find(step.Who, bot))
		{
			r.Ok = false;
			r.Reason = "no such bot";
			return;
		}

		dmExplorer explorer = bot.GetExplorer();
		if (!explorer)
		{
			r.Ok = false;
			r.Reason = "no explorer";
			return;
		}

		dmWorldPoiLocation loc = explorer.GetCurrentLocation();
		if (loc)
		{
			AppendDump(r, "locId=" + loc.Id);
			AppendDump(r, "locName=" + loc.Name);
			AppendDump(r, "locType=" + loc.Type);
		}
		else
		{
			AppendDump(r, "loc=none");
		}

		AppendDump(r, "visited=" + explorer.VisitedLocationCount());
		int bldTotal = explorer.LocationVisitedCount() + explorer.LocationUnvisitedCount();
		AppendDump(r, "bldTotal=" + bldTotal);
		AppendDump(r, "bldVisited=" + explorer.LocationVisitedCount());
		AppendDump(r, "bldUnvisited=" + explorer.LocationUnvisitedCount());

		int nothingToDo = 0;
		if (explorer.IsNothingToDo())
			nothingToDo = 1;
		AppendDump(r, "nothingToDo=" + nothingToDo);

		int inTransit = 0;
		if (explorer.IsInTransit())
			inTransit = 1;
		AppendDump(r, "inTransit=" + inTransit);

		AppendDump(r, "timeInLoc=" + explorer.GetTimeInLocation());

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
		SetProbeLifetime(car);

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
		drive.m_ImmediateGrid = step.ImmediateGrid;
		bot.AddCommandIntent(drive);

		r.Ok = true;
		r.Reason = "drive issued";
	}

	//! "driveto" — issue a router-driven dmBotIntent_Drive: the named bot boards
	//! the named car and drives the road route to Pos. The route comes from
	//! dmRoadRouter (Setup once; the intent pulls chunks via NextChunk directly),
	//! so m_DriveRoute is left empty.
	private void RunDriveTo(dmE2EStep step, dmE2EStepResult r)
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

		vector targetPos = ResolveWorldPos(step.Pos);
		if (!dmRoadRouter.Get().Setup(transport.GetPosition(), targetPos))
		{
			r.Ok = false;
			r.Reason = "no route";
			return;
		}

		dmBotIntent_Drive drive = new dmBotIntent_Drive();
		drive.m_Transport = transport;
		drive.m_Seat = 0;
		drive.m_ImmediateGrid = step.ImmediateGrid;
		bot.AddCommandIntent(drive);

		r.Ok = true;
		r.Reason = "driveto issued";
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

		string driverState = "absent";
		if (car.CrewMember(DayZPlayerConstants.VEHICLESEAT_DRIVER) != null)
			driverState = "present";
		line = "driver=" + driverState;
		AppendDump(r, line);

		r.Ok = true;
		r.Reason = "dumped";
	}

	//! "carstate" — dump the named car's damage zones, attachments (health +
	//! energy when present) and fluid fractions, via AppendDump.
	private void RunCarState(dmE2EStep step, dmE2EStepResult r)
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

		array<string> zones = new array<string>();
		car.GetDamageZones(zones);
		AppendDump(r, "zones=" + zones.Count());

		int i;
		for (i = 0; i < zones.Count(); i++)
			AppendDump(r, "zone[" + zones[i] + "]=" + car.GetHealth01(zones[i], "Health"));
		AppendDump(r, "zone[]=" + car.GetHealth01("", "Health"));

		int attCount = car.GetInventory().AttachmentCount();
		AppendDump(r, "attachments=" + attCount);

		EntityAI att;
		string line;
		for (i = 0; i < attCount; i++)
		{
			att = car.GetInventory().GetAttachmentFromIndex(i);
			if (!att)
				continue;
			line = "att[" + i + "]=" + att.GetType() + " hp=" + att.GetHealth01("", "Health");
			if (att.HasEnergyManager())
				line = line + " energy=" + att.GetCompEM().GetEnergy() + "/" + att.GetCompEM().GetEnergyMax();
			AppendDump(r, line);
		}

		AppendDump(r, "oil=" + car.GetFluidFraction(CarFluid.OIL));
		AppendDump(r, "brake=" + car.GetFluidFraction(CarFluid.BRAKE));
		AppendDump(r, "fuel=" + car.GetFluidFraction(CarFluid.FUEL));
		AppendDump(r, "coolant=" + car.GetFluidFraction(CarFluid.COOLANT));

		r.Ok = true;
		r.Reason = "dumped";
	}

	//! "cardamage" — deterministic damage: every zone and attachment to 30%
	//! health, remove one optional part (spark plug; else the last non-wheel
	//! attachment), and set fluids to known fractions (oil/brake empty, fuel and
	//! coolant exactly 0.5).
	private void RunCarDamage(dmE2EStep step, dmE2EStepResult r)
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

		array<string> zones = new array<string>();
		car.GetDamageZones(zones);
		int i;
		for (i = 0; i < zones.Count(); i++)
			car.SetHealth01(zones[i], "Health", 0.3);
		car.SetHealth01("", "Health", 0.3);

		int attCount = car.GetInventory().AttachmentCount();
		EntityAI att;
		for (i = 0; i < attCount; i++)
		{
			att = car.GetInventory().GetAttachmentFromIndex(i);
			if (att)
				att.SetHealth01("", "Health", 0.3);
		}

		//! Удаляем одну необязательную деталь, чтобы repair её НЕ вернул.
		ItemBase plug = ItemBase.Cast(car.FindAttachmentBySlotName("SparkPlug"));
		if (plug)
		{
			plug.Delete();
		}
		else
		{
			int vi;
			EntityAI victim;
			bool isWheel;
			int wi;
			for (vi = attCount - 1; vi >= 0; vi--)
			{
				victim = car.GetInventory().GetAttachmentFromIndex(vi);
				isWheel = false;
				for (wi = 0; wi < car.WheelCount(); wi++)
				{
					if (car.WheelGetEntity(wi) == victim)
						isWheel = true;
				}
				if (victim && !isWheel)
				{
					victim.Delete();
					break;
				}
			}
		}

		car.LeakAll(CarFluid.OIL);
		car.LeakAll(CarFluid.BRAKE);

		car.LeakAll(CarFluid.FUEL);
		car.Fill(CarFluid.FUEL, car.GetFluidCapacity(CarFluid.FUEL) * 0.5);
		car.LeakAll(CarFluid.COOLANT);
		car.Fill(CarFluid.COOLANT, car.GetFluidCapacity(CarFluid.COOLANT) * 0.5);

		r.Ok = true;
		r.Reason = "damaged";
	}

	//! "carrepair" — call the shared dmCarRepair core on the named car and dump
	//! the zone/attachment counts.
	private void RunCarRepair(dmE2EStep step, dmE2EStepResult r)
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

		int z;
		int a;
		dmCarRepair.Repair(car, z, a);
		AppendDump(r, "zones=" + z + " atts=" + a);

		r.Ok = true;
		r.Reason = "repaired";
	}

	//! "griddump" — run the road honeycomb over the route from Pos to TargetPos
	//! (no car/driver), then dump the colored territory + route + computed road
	//! edges to $profile:dmBotorama/e2e/griddump.json for the SVG renderer.
	private void RunGridDump(dmE2EStep step, dmE2EStepResult r)
	{
		vector start = ResolveWorldPos(step.Pos);
		vector target = ResolveWorldPos(step.TargetPos);

		if (!dmRoadRouter.Get().Setup(start, target))
		{
			r.Ok = false;
			r.Reason = "no route";
			return;
		}

		//! Полный маршрут чанками (NextChunk чистит входной буфер — отдельный
		//! локальный массив, точки копируем в route).
		array<vector> route = new array<vector>();
		array<vector> chunk = new array<vector>();
		bool more = true;
		int guard = 0;
		int ci;
		while (more)
		{
			more = dmRoadRouter.Get().NextChunk(chunk, 16);
			for (ci = 0; ci < chunk.Count(); ci++)
				route.Insert(chunk[ci]);
			guard = guard + 1;
			if (guard > 1000)
			{
				dmBotLog.Error("[E2E] griddump: NextChunk loop guard hit");
				break;
			}
		}

		//! Направление территории: сам отрезок дороги (target − start), а не дельта
		//! до первого вейпоинта — иначе грид уходит по оси X вместо дороги.
		vector d = target - start;
		d[1] = 0.0;
		if (d.Length() < 0.01)
			d = Vector(1.0, 0.0, 0.0);
		else
			d.Normalize();
		vector side = Vector(-d[2], 0.0, d[0]);

		//! Расчётные края дороги: ±½ ширины вдоль перпендикуляра в каждой точке.
		array<vector> leftEdge = new array<vector>();
		array<vector> rightEdge = new array<vector>();
		int wi;
		vector wp;
		float w;
		float halfW;
		for (wi = 0; wi < route.Count(); wi++)
		{
			wp = route[wi];
			w = dmRoadProbe.RoadWidth(wp, d);
			halfW = w * 0.5;
			leftEdge.Insert(wp - side * halfW);
			rightEdge.Insert(wp + side * halfW);
		}

		int widthCells = step.WidthCells;
		float lengthMeters = step.LengthMeters;
		if (lengthMeters <= 0.0)
			lengthMeters = 260.0;

		float t0 = GetGame().GetTickTime();
		dmRoadHoneycomb.Get().Start(start, d, route, 0, null, null, lengthMeters, widthCells);

		int ticks = 0;
		while (!dmRoadHoneycomb.Get().IsDone() && !dmRoadHoneycomb.Get().IsFailed() && ticks < 300000)
		{
			dmRoadHoneycomb.Get().Tick(0.016);
			ticks = ticks + 1;
		}

		float buildMs = (GetGame().GetTickTime() - t0) * 1000.0;

		string hStatus = "done";
		if (dmRoadHoneycomb.Get().IsFailed())
			hStatus = "failed";
		else if (!dmRoadHoneycomb.Get().IsDone())
			hStatus = "capped";

		dmGridDump dump = new dmGridDump();
		dump.Origin = dmRoadHoneycomb.Get().GetOrigin();
		dump.Dir = dmRoadHoneycomb.Get().GetDir();
		dump.Side = dmRoadHoneycomb.Get().GetSide();
		dump.Rows = dmRoadHoneycomb.Get().GetRowCount();
		dump.Width = dmRoadHoneycomb.Get().GetWidth();
		dump.HalfWidth = dmRoadHoneycomb.Get().GetHalfWidth();
		dump.Status = hStatus;
		dump.Cells = new array<ref dmGridDumpCell>();
		dump.Route = new array<vector>();
		dump.LeftEdge = new array<vector>();
		dump.RightEdge = new array<vector>();

		int cellRow;
		int cellCol;
		int cellCount = 0;
		vector center;
		dmGridDumpCell cell;
		for (cellRow = 0; cellRow < dump.Rows; cellRow++)
		{
			for (cellCol = -dump.HalfWidth; cellCol <= dump.HalfWidth; cellCol++)
			{
				if ((cellCol + cellRow) % 2 != 0)
					continue;
				cell = new dmGridDumpCell();
				cell.Row = cellRow;
				cell.Col = cellCol;
				cell.Color = dmRoadHoneycomb.Get().GetCellColor(cellCol, cellRow);
				center = dmRoadHoneycomb.Get().GetCellCenter(cellCol, cellRow);
				cell.X = center[0];
				cell.Z = center[2];
				dump.Cells.Insert(cell);
				cellCount = cellCount + 1;
			}
		}
		dump.CellCount = cellCount;

		dump.WaypointCells = new array<ref dmGridDumpCell>();
		int wcount = dmRoadHoneycomb.Get().GetWaypointCount();
		int wi2;
		int wcol;
		int wrow;
		dmGridDumpCell wcell;
		for (wi2 = 0; wi2 < wcount; wi2++)
		{
			if (dmRoadHoneycomb.Get().GetWaypointCell(wi2, wcol, wrow))
			{
				wcell = new dmGridDumpCell();
				wcell.Row = wrow;
				wcell.Col = wcol;
				wcell.Color = 0;
				center = dmRoadHoneycomb.Get().GetCellCenter(wcol, wrow);
				wcell.X = center[0];
				wcell.Z = center[2];
				dump.WaypointCells.Insert(wcell);
			}
		}

		int ri;
		for (ri = 0; ri < route.Count(); ri++)
			dump.Route.Insert(route[ri]);
		for (ri = 0; ri < leftEdge.Count(); ri++)
			dump.LeftEdge.Insert(leftEdge[ri]);
		for (ri = 0; ri < rightEdge.Count(); ri++)
			dump.RightEdge.Insert(rightEdge[ri]);

		string dumpPath = DM_E2E_DIR + "/griddump.json";
		EnsureDir(dumpPath);
		string saveError;
		bool saved = JsonFileLoader<dmGridDump>.SaveFile(dumpPath, dump, saveError);
		if (!saved)
			dmBotLog.Error("[E2E] griddump save failed: " + saveError);

		#ifdef DM_BOT_DEBUG_E2E
		dmBotLog.Debug("[E2E] griddump: " + hStatus + " cells=" + cellCount + " route=" + route.Count());
		#endif

		AppendDump(r, "buildMs=" + buildMs);

		r.Ok = saved;
		r.Reason = hStatus + " cells=" + cellCount + " route=" + route.Count() + " -> " + dumpPath;
		r.Reason = r.Reason + " buildMs=" + buildMs;
	}

	//! "dumpgrid" — дамп ТЕКУЩЕГО состояния сот (после драйв-потока) БЕЗ перезапуска
	//! сетки: геометрия/цвета/путь берутся из dmRoadHoneycomb геттерами, а не
	//! пересчитываются. Формат тот же, что у griddump; LeftEdge/RightEdge пустые
	//! (для PNG линии объезда важнее WaypointCells).
	private void RunDumpGridState(dmE2EStep step, dmE2EStepResult r)
	{
		string hStatus = "running";
		if (dmRoadHoneycomb.Get().GetStatusId() == 3)
			hStatus = "done";
		else if (dmRoadHoneycomb.Get().GetStatusId() == 4)
			hStatus = "failed";

		dmGridDump dump = new dmGridDump();
		dump.Origin = dmRoadHoneycomb.Get().GetOrigin();
		dump.Dir = dmRoadHoneycomb.Get().GetDir();
		dump.Side = dmRoadHoneycomb.Get().GetSide();
		dump.Rows = dmRoadHoneycomb.Get().GetRowCount();
		dump.Width = dmRoadHoneycomb.Get().GetWidth();
		dump.HalfWidth = dmRoadHoneycomb.Get().GetHalfWidth();
		dump.Status = hStatus;
		dump.Cells = new array<ref dmGridDumpCell>();
		dump.WaypointCells = new array<ref dmGridDumpCell>();
		dump.Route = new array<vector>();
		dump.LeftEdge = new array<vector>();
		dump.RightEdge = new array<vector>();

		int cellRow;
		int cellCol;
		int cellCount = 0;
		vector center;
		dmGridDumpCell cell;
		for (cellRow = 0; cellRow < dump.Rows; cellRow++)
		{
			for (cellCol = -dump.HalfWidth; cellCol <= dump.HalfWidth; cellCol++)
			{
				if ((cellCol + cellRow) % 2 != 0)
					continue;
				cell = new dmGridDumpCell();
				cell.Row = cellRow;
				cell.Col = cellCol;
				cell.Color = dmRoadHoneycomb.Get().GetCellColor(cellCol, cellRow);
				center = dmRoadHoneycomb.Get().GetCellCenter(cellCol, cellRow);
				cell.X = center[0];
				cell.Z = center[2];
				dump.Cells.Insert(cell);
				cellCount = cellCount + 1;
			}
		}
		dump.CellCount = cellCount;

		int wcount = dmRoadHoneycomb.Get().GetWaypointCount();
		int wi2;
		int wcol;
		int wrow;
		dmGridDumpCell wcell;
		for (wi2 = 0; wi2 < wcount; wi2++)
		{
			if (dmRoadHoneycomb.Get().GetWaypointCell(wi2, wcol, wrow))
			{
				wcell = new dmGridDumpCell();
				wcell.Row = wrow;
				wcell.Col = wcol;
				wcell.Color = 0;
				center = dmRoadHoneycomb.Get().GetCellCenter(wcol, wrow);
				wcell.X = center[0];
				wcell.Z = center[2];
				dump.WaypointCells.Insert(wcell);
			}
		}

		array<vector> route = dmRoadHoneycomb.Get().GetRoute();
		if (route)
		{
			int ri;
			for (ri = 0; ri < route.Count(); ri++)
				dump.Route.Insert(route[ri]);
		}

		string dumpPath = DM_E2E_DIR + "/griddump.json";
		EnsureDir(dumpPath);
		string saveError;
		bool saved = JsonFileLoader<dmGridDump>.SaveFile(dumpPath, dump, saveError);
		if (!saved)
			dmBotLog.Error("[E2E] dumpgrid save failed: " + saveError);

		#ifdef DM_BOT_DEBUG_E2E
		dmBotLog.Debug("[E2E] dumpgrid: " + hStatus + " cells=" + cellCount + " route=" + dump.Route.Count());
		#endif

		r.Ok = saved;
		r.Reason = "dumped " + cellCount + " cells";
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
