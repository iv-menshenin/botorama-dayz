//! modded ItemBase — publish a drop noise when a thrown non-explosive item lands
//! (EOnContact). The thrower is read from the cross-module bridge (bound in
//! HandActionThrow on the server); the entry is cleared after consumption to
//! suppress repeat EOnContact (bounce) and leaks.
modded class ItemBase
{
	override void EOnContact(IEntity other, Contact extra)
	{
		super.EOnContact(other, extra);
		#ifdef SERVER
		EntityAI thrower = dmNoiseThrowerBridge.GetThrower(this);
		if (thrower && !ExplosivesBase.Cast(this))
		{
			dmNoiseSystem.AddNoise(thrower, GetPosition(), DM_NOISE_DROP_STRENGTH, dmNoiseType.DROP);
			dmNoiseThrowerBridge.ClearThrower(this);   // одноразово: гасит повторные EOnContact (отскок)
		}
		#endif
	}
}
