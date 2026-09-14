//! dmE2EBridge — file bridge for E2E auto-tests (hello-world, phase 1).
//!
//! An AI agent drops a JSON job into $profile:dmBotorama/e2e/in/, the bridge
//! executes it on the server and writes the result to e2e/out/, then moves the
//! input to e2e/done/. The bridge is inert unless the e2e/enabled marker file
//! exists, so there is zero overhead when no agent is driving.
//!
//! Hello-world ops: ping | spawn | snapshot | clearall (all instantaneous).

//! A job (input): an id and the list of steps to run.
class dmE2EJob
{
	string Name;     // job id (= file name)
	float Timeout;   // overall timeout (unused in hello-world, but read)
	autoptr array<ref dmE2EStep> Steps;
}

//! One job step (op plus per-op parameters).
class dmE2EStep
{
	string Op;       // "ping" | "spawn" | "snapshot" | "clearall"
	string Who;      // bot name (spawn)
	vector Pos;      // [x,y,z] world position (spawn)
	float Yaw;       // orientation in degrees (spawn)
}

//! Per-step outcome.
class dmE2EStepResult
{
	int Index;
	string Op;
	bool Ok;
	string Reason;
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
	string Status;   // "ok" | "error"
	string Error;
	ref array<ref dmE2EStepResult> Steps;
	ref array<ref dmE2ESnapshot> Snapshot;
}

//! Singleton executor: scans e2e/in/*.json and runs one job per tick.
//! Ticked from MissionServer.OnUpdate.
class dmE2EBridge
{
	static ref dmE2EBridge s_Instance;
	private ref map<string, ref dmAISurvivor> m_Named;
	private float m_ScanAccum;

	static dmE2EBridge Get()
	{
		if (!s_Instance)
			s_Instance = new dmE2EBridge();
		return s_Instance;
	}

	void dmE2EBridge()
	{
		m_Named = new map<string, ref dmAISurvivor>();
		m_ScanAccum = 0.0;
	}

	void Tick(float dt)
	{
		if (!FileExist(DM_E2E_ENABLED_FILE))
			return;

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
			RunJob(DM_E2E_IN_DIR + "/" + fileName, fileName);
		CloseFindFile(handle);
	}

	void RunJob(string jobPath, string fileName)
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
			return;
		}

		dmE2EResult result = new dmE2EResult();
		result.Steps = new array<ref dmE2EStepResult>();
		result.Snapshot = new array<ref dmE2ESnapshot>();
		result.Name = job.Name;
		if (result.Name == "")
			result.Name = BaseName(fileName);
		result.Status = "ok";

		int i;
		for (i = 0; i < job.Steps.Count(); i++)
		{
			dmE2EStep step = job.Steps[i];
			dmE2EStepResult stepResult = new dmE2EStepResult();
			stepResult.Index = i;
			stepResult.Op = step.Op;

			if (step.Op == "ping")
			{
				stepResult.Ok = true;
				stepResult.Reason = "";
			}
			else if (step.Op == "spawn")
			{
				ref dmAISurvivor bot = new dmAISurvivor();
				PlayerBase pawn = bot.Spawn(SnapToGroundExactly(step.Pos), Vector(step.Yaw, 0, 0));
				if (pawn)
				{
					m_Named.Insert(step.Who, bot);
					stepResult.Ok = true;
					stepResult.Reason = "spawned";
				}
				else
				{
					stepResult.Ok = false;
					stepResult.Reason = "spawn failed";
					result.Status = "error";
				}
			}
			else if (step.Op == "snapshot")
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
					result.Snapshot.Insert(snap);
				}
				stepResult.Ok = true;
				stepResult.Reason = "";
			}
			else if (step.Op == "clearall")
			{
				int cleared = dmAISurvivor.ClearAll();
				m_Named.Clear();
				stepResult.Ok = true;
				stepResult.Reason = "cleared " + cleared;
			}
			else
			{
				stepResult.Ok = false;
				stepResult.Reason = "unknown op";
				result.Status = "error";
			}

			result.Steps.Insert(stepResult);

			#ifdef DM_BOT_DEBUG_E2E
			dmBotLog.Debug("[E2E] step " + i + " " + step.Op + " ok=" + stepResult.Ok);
			dmBotLog.Debug("[E2E] step reason=" + stepResult.Reason);
			#endif
		}

		SaveResult(result);
		MoveToDone(jobPath, fileName);
		ClearNamed();

		#ifdef DM_BOT_DEBUG_E2E
		dmBotLog.Debug("[E2E] job done: " + result.Name + " status=" + result.Status);
		#endif
	}

	void ClearNamed()
	{
		m_Named.Clear();
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
