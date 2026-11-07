//! modded TripwireTrap — forward the victim (who tripped the wire) to the attached
//! grenade. The tripwire itself does NOT explode: SetInactive(false) drops the
//! attached grenade and calls OnActivatedByItem on it, so the explosion's source is
//! the grenade, while the victim binding (written by modded TrapBase.SnapOnObject)
//! sits on the tripwire. Bind the victim to the attached grenade BEFORE super (while
//! it is still attached), and clear the tripwire's own binding (it never explodes).
modded class TripwireTrap
{
	#ifdef SERVER
	override void OnSteppedOn(EntityAI victim)
	{
		if (victim)
		{
			int att;
			for (att = 0; att < GetInventory().AttachmentCount(); att++)
			{
				ItemBase attachment = ItemBase.Cast(GetInventory().GetAttachmentFromIndex(att));
				if (attachment)
				{
					dmNoiseThrowerBridge.BindTriggered(attachment, victim);
					#ifdef DM_PERCEPTION_DEBUG
					dmBotLog.Debug("", "[Noise] bind grenade (tripwire) " + attachment.GetType());
					#endif
				}
			}
		}
		//! The tripwire itself never explodes — drop its own binding (left by
		//! modded TrapBase.SnapOnObject) so it does not become a stale entry.
		dmNoiseThrowerBridge.ClearTriggered(this);
		super.OnSteppedOn(victim);
	}
	#endif
}
