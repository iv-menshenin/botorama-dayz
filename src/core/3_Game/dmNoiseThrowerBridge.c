//! Cross-module bridge for attributing explosion/drop noise to the culprit.
//!
//! The "who threw this / who triggered this" association cannot live as a field
//! on the item: InventoryItem (3_Game) is an ENGINE class (`Engine class
//! 'InventoryItem' cannot be modded`), and ItemBase (4_World) is not visible from
//! 3_Game (DayZGame.ExplosionEffectsEx / HandActionThrow). The only type visible
//! from both 3_Game and 4_World is EntityAI (engine), so the association lives
//! here as static maps keyed by the item's EntityAI (same pattern as
//! dmBallisticsBridge). 3_Game binds/consumes; 4_World (ItemBase.EOnContact,
//! TrapBase.SnapOnObject) reads/writes through these statics.
class dmNoiseThrowerBridge
{
	static ref map<EntityAI, EntityAI> s_Thrower = new map<EntityAI, EntityAI>();
	static ref map<EntityAI, EntityAI> s_Triggered = new map<EntityAI, EntityAI>();

	static void BindThrower(EntityAI item, EntityAI thrower)
	{
		if (item)
			s_Thrower[item] = thrower;
	}

	static EntityAI GetThrower(EntityAI item)
	{
		if (item && s_Thrower.Contains(item))
			return s_Thrower[item];
		return null;
	}

	static void ClearThrower(EntityAI item)
	{
		if (item)
			s_Thrower.Remove(item);
	}

	static void BindTriggered(EntityAI trap, EntityAI victim)
	{
		if (trap)
			s_Triggered[trap] = victim;
	}

	static EntityAI GetTriggered(EntityAI trap)
	{
		if (trap && s_Triggered.Contains(trap))
			return s_Triggered[trap];
		return null;
	}

	static void ClearTriggered(EntityAI trap)
	{
		if (trap)
			s_Triggered.Remove(trap);
	}
}
