//! modded HandActionThrow — bind the thrower (player) to the thrown item via the
//! cross-module bridge so that later ItemBase.EOnContact (landing) and
//! DayZGame.ExplosionEffectsEx (grenade explosion) can attribute the noise to the
//! right entity. The item cannot carry the field itself (InventoryItem is an
//! engine class), so the association lives in dmNoiseCulpritBridge keyed by EntityAI.
modded class HandActionThrow
{
	override void Action(HandEventBase e)
	{
		super.Action(e);
		#ifdef SERVER
		EntityAI item = e.GetSrcEntity();
		DayZPlayer player = DayZPlayer.Cast(e.m_Player);
		if (item && player)
		{
			dmNoiseCulpritBridge.Bind(item, player);
			#ifdef DM_PERCEPTION_DEBUG
			dmBotLog.Debug("", "[Noise] bind thrower " + item.GetType());
			#endif
		}
		#endif
	}
}
