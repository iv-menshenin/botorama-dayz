//! Cross-module bridge for attributing explosion/drop noise to the culprit.
//!
//! The "who threw this / who triggered this" association cannot live as a field
//! on the item: InventoryItem (3_Game) is an ENGINE class (`Engine class
//! 'InventoryItem' cannot be modded`), and ItemBase (4_World) is not visible from
//! 3_Game. The only type visible from both is EntityAI, so the association lives
//! here as a static map keyed by the item's EntityAI. The thrower and the tripwire
//! victim are DISJOINT (a grenade is thrown, a mine is stepped on) — both are just
//! the "culprit" the noise is attributed to, so they share one map.
class dmNoiseCulpritBridge
{
	static ref map<EntityAI, EntityAI> s_Culprit = new map<EntityAI, EntityAI>();

	static void Bind(EntityAI item, EntityAI culprit)
	{
		if (item)
			s_Culprit[item] = culprit;
	}

	static EntityAI Get(EntityAI item)
	{
		if (item && s_Culprit.Contains(item))
			return s_Culprit[item];
		return null;
	}

	static void Clear(EntityAI item)
	{
		if (item)
			s_Culprit.Remove(item);
	}
}
