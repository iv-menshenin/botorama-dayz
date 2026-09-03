//! dmBotIntent_TidyInventory — tidy up ammo/magazines when hands are free.
//! PARALLEL, low priority: it only acts when the bot is out of combat, then
//! stacks ammo piles, reloads the weapon in hands, and (out of combat or with no
//! loaded mags) loads magazines — one small step per throttled tick.
class dmBotIntent_TidyInventory : dmBotIntent
{
	float m_Cooldown = 0.0;

	void dmBotIntent_TidyInventory()
	{
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Priority = dmBotIntentPriority.IDLE;
		m_Manage = dmBotIntentsChannel.NONE;
	}

	override string GetIntentName()
	{
		return "TidyInventory";
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		//! Don't tidy while fighting (the Shooting/Fighting states own the weapon).
		if (bot.GetHostileTarget() != null)
			return;

		if (m_Cooldown > 0.0)
		{
			m_Cooldown -= pDt;
			return;
		}

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return;

		//! 1) Stack ammo (server, no animation) — always, if there's something to merge.
		if (pawn.StackAmmoAI())
		{
			m_Cooldown = DM_TIDY_STEP_INTERVAL;
			return;
		}

		//! 2) Reload the weapon in hands when it has run dry. Reuses ReloadWeaponAI.
		if (bot.GetWeaponInHands() && bot.HasNoAmmo())
		{
			if (pawn.ReloadWeaponAI())
			{
				m_Cooldown = DM_TIDY_STEP_INTERVAL;
				return;
			}
		}

		//! 3) Load magazines from ammo piles (only out of combat — here we always are).
		if (pawn.LoadMagazineAI())
		{
			m_Cooldown = DM_TIDY_STEP_INTERVAL;
			return;
		}

		m_Cooldown = DM_TIDY_SCAN_INTERVAL;
	}
}
