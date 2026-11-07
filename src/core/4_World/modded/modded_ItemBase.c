//! modded ItemBase — publish a drop noise when a thrown item lands (EOnContact),
//! including grenades/mines (they also emit DROP on impact). The thrower is read
//! from the cross-module bridge (bound in HandActionThrow on the server); the entry
//! is cleared after consumption to suppress repeat EOnContact (bounce) and leaks —
//! EXCEPT for explosives, whose thrower is kept for the later explosion attribution
//! in ExplosionEffectsEx.
modded class ItemBase
{
	override void EOnContact(IEntity other, Contact extra)
	{
		super.EOnContact(other, extra);
		#ifdef SERVER
		EntityAI thrower = dmNoiseThrowerBridge.GetThrower(this);
		if (thrower)
		{
			dmNoiseSystem.AddNoise(thrower, GetPosition(), DM_NOISE_DROP_STRENGTH, dmNoiseType.DROP);
			if (!ExplosivesBase.Cast(this))
				dmNoiseThrowerBridge.ClearThrower(this);   // гасит отскок; для взрывчатки кидатель нужен позже (взрыв)
		}
		#endif
	}
}
