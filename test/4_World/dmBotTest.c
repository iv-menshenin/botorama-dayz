//! Firing: a bot with a loaded AKM receives a damage threat, must enter the
//! Shooting state and fire — the ammo count of the magazine in hands drops.
class dmBotTest_Shoot : dmTestSuite_TestCase
{
	int m_Phase = 0;
	EntityAI m_Target;
	int m_StartAmmo = 0;

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		// do not forget!
		super.Setup(bot, player);

		bot.SetFSM(dmBotPreset_Combat.Create(bot));

		PlayerBase pawn = bot.GetPawn();
		if (!pawn) return;

		//! CreateInHands places the weapon directly in the bot's hands (no move
		//! dance). SpawnAmmo attaches a magazine AND chambers a round (default
		//! WeaponWithAmmoFlags.CHAMBER), so the weapon is ready to fire.
		Weapon_Base gun = Weapon_Base.Cast(pawn.GetHumanInventory().CreateInHands("AKM"));
		if (!gun)
			return;

		gun.SpawnAmmo("Mag_AKM_30Rnd");

		Magazine mag = gun.GetMagazine(gun.GetCurrentMuzzle());
		if (mag)
			mag.ServerSetAmmoCount(30);
	}

	override string GetSummary()
	{
		return "Тест «Стрельба». Боту дают заряженный АКМ и регистрируют враждебную болванку-игрока (15 м). Ожидается: бот входит в Shooting и стреляет — патроны в магазине убывают.";
	}

	override float GetInterval() { return 1.0; }

	override float GetDuration() { return 25.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		if (m_Phase == 0)
		{
			if (elapsed < 5.0)
				return "";

            m_Target = SpawnEmeny();
			if (!m_Target)
				return "FAIL: не удалось заспавнить цель";

			m_StartAmmo = AmmoInHands();
			m_Phase = 1;
			return "цель (болванка-игрок) заспавнена в 15 м, враждебна (патронов в руках: " + m_StartAmmo + ")";
		}

		if (m_Phase == 1)
		{
			if (elapsed < 8.0)
				return "";

			int ammo = AmmoInHands();
			if (ammo < m_StartAmmo)
			{
				EmptyMagazineInHands();
				m_Phase = 2;
				return "бот выстрелил, патроны обнулены";
			}
			return FailDebug(ammo);
		}

		if (m_Phase == 2)
		{
			if (elapsed < 9.0)
				return "";

			string state = CurrentStateName();
			if (state != "Shooting")
				return "PASS: бот вышел из Shooting (state=" + state + ")";
			return "FAIL: бот не вышел из Shooting (state=" + state + ")";
		}

		return "";
	}

	//! Ammo count of the magazine currently in the bot's hands (-1 if none).
	int AmmoInHands()
	{
		Weapon_Base wpn = m_Bot.GetWeaponInHands();
		if (!wpn)
			return -1;
		Magazine mag = wpn.GetMagazine(wpn.GetCurrentMuzzle());
		if (mag)
			return mag.GetAmmoCount();
		return -1;
	}

	//! Force the magazine in hands to zero rounds (the chambered round still fires
	//! once more, then HasNoAmmo() turns true and the bot leaves Shooting).
	void EmptyMagazineInHands()
	{
		Weapon_Base wpn = m_Bot.GetWeaponInHands();
		if (!wpn)
			return;
		Magazine mag = wpn.GetMagazine(wpn.GetCurrentMuzzle());
		if (mag)
			mag.ServerSetAmmoCount(0);
	}

	//! Name of the current FSM state ("none" when unavailable).
	string CurrentStateName()
	{
		string state = "none";
		dmBotFSM fsm = m_Bot.GetFSM();
		if (fsm && fsm.GetCurrentState())
			state = fsm.GetCurrentState().GetName();
		return state;
	}

	string FailDebug(int ammo)
	{
		string msg = "FAIL: бот не выстрелил (патроны " + ammo + " / " + m_StartAmmo;
		msg = msg + ", state=" + CurrentStateName() + ", HasNoAmmo=" + m_Bot.HasNoAmmo();
		msg = msg + ", HasFirearm=" + m_Bot.HasFirearmInHands() + ")";
		return msg;
	}
}