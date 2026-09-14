//! dmLoadoutApplier — load a dmLoadoutConfig and apply it to a bot's pawn.
//!
//! Items are created DIRECTLY in place, exactly like the vanilla spawn-gear
//! system (cfgplayerspawnhandler.c): CreateAttachmentEx for equipment slots,
//! CreateInHands for the hands, CreateInInventory for cargo/nested children and
//! wep.SpawnAmmo for weapon magazines. This is the correct path — moving a
//! freshly CreateObject'd item via TakeEntityTo* does NOT work, because such an
//! item has no inventory location yet.
//!
//! The engine cannot CreateEntityInCargoEx an item into a container that is
//! itself nested inside another container's cargo. When direct creation returns
//! null, CreateInContainerFallback applies the known workaround: move the parent
//! container out to the ground, add the item, move the container back
//! (reference: ExpLootSpawner.c DMCreateInInventory).
//!
//! Application is ADDITIVE: existing items on the pawn are left alone.
//! See loadout/4_World/README.md ("Как это работает").

class dmLoadoutApplier
{
	//! Load a loadout config by name (file name without ".json").
	static dmLoadoutConfig Load(string name)
	{
		string path = DM_LOADOUT_DIR + "/" + name + ".json";
		if (!FileExist(path))
			return null;

		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Loadout.Load");
		#endif

		dmJsonFile<dmLoadoutConfig> reader = new dmJsonFile<dmLoadoutConfig>(path);
		dmLoadoutConfig cfg;
		if (!reader.Load(cfg))
		{
			dmBotLog.Error("Loadout read error " + path + ": " + reader.Errors());
			return null;
		}

		#ifdef DM_BOT_DEBUG_LOADOUT
		dmBotLog.Debug("Loadout '" + name + "' loaded (version " + cfg.GetVersion() + ")");
		#endif
		Normalize(cfg);
		return cfg;
	}

	//! The JSON omits "Chance" when it is 1.0, and the engine's JsonSerializer
	//! does not run field initializers, so an omitted Chance deserializes as 0.0
	//! and the item/preset would never be selected. Treat Chance <= 0.0 as 1.0.
	static void Normalize(dmLoadoutConfig cfg)
	{
		int i;
		if (cfg.Presets)
		{
			for (i = 0; i < cfg.Presets.Count(); i++)
			{
				if (cfg.Presets[i].Chance <= 0.0)
					cfg.Presets[i].Chance = 1.0;
			}
		}
		if (cfg.Slots)
		{
			for (i = 0; i < cfg.Slots.Count(); i++)
				NormalizeItems(cfg.Slots[i].Items);
		}
		NormalizeItems(cfg.Cargo);
	}

	//! Recursively normalize the Chance of a list of items.
	static void NormalizeItems(array<ref dmLoadoutItem> items)
	{
		if (!items)
			return;

		int i;
		int j;
		for (i = 0; i < items.Count(); i++)
		{
			dmLoadoutItem it = items[i];
			if (it.Chance <= 0.0)
				it.Chance = 1.0;
			if (it.Attachments)
			{
				for (j = 0; j < it.Attachments.Count(); j++)
					NormalizeItems(it.Attachments[j].Items);
			}
			NormalizeItems(it.Cargo);
		}
	}

	//! Collect the names of all loadout files (without ".json") in DM_LOADOUT_DIR.
	static void List(out array<string> names)
	{
		names.Clear();

		if (!FileExist(DM_LOADOUT_DIR))
			return;

		string fileName;
		FileAttr fileAttr;
		FindFileHandle handle = FindFile(DM_LOADOUT_DIR + "/*.json", fileName, fileAttr, FindFileFlags.DIRECTORIES);

		bool hasMatch = fileName != "";
		while (hasMatch)
		{
			names.Insert(fileName.Substring(0, fileName.Length() - 5));
			hasMatch = FindNextFile(handle, fileName, fileAttr);
		}
		CloseFindFile(handle);
	}

	//! Apply a loadout to a pawn. Rolls the active preset once, then fills every
	//! slot and the general cargo.
	static void Apply(PlayerBase pawn, dmLoadoutConfig cfg)
	{
		if (!pawn || !cfg)
			return;

		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Loadout.Apply");
		#endif

		string preset = RollPreset(cfg.Presets);

		#ifdef DM_BOT_DEBUG_LOADOUT
		dmBotLog.Debug("Loadout apply to pawn=" + pawn + " preset='" + preset + "'");
		#endif

		int i;
		dmLoadoutItem item;
		dmLoadoutSlot slot;

		if (cfg.Slots)
		{
			for (i = 0; i < cfg.Slots.Count(); i++)
			{
				slot = cfg.Slots[i];
				item = PickItem(slot.Items, preset);
				if (item)
				{
					CreateInSlot(pawn, slot.SlotName, item, preset);
				}
				else
				{
					#ifdef DM_BOT_DEBUG_LOADOUT
					dmBotLog.Debug("Loadout: slot '" + slot.SlotName + "' - nothing picked");
					#endif
				}
			}
		}

		if (cfg.Cargo)
		{
			for (i = 0; i < cfg.Cargo.Count(); i++)
			{
				item = cfg.Cargo[i];
				if (!ConformOk(item, preset) || !RollChance(item.Chance))
					continue;
				CreateInCargo(pawn, item, preset);
			}
		}
	}

	//------------------------------------------------------------------
	// Random rolls
	//------------------------------------------------------------------

	//! Roll the active preset (weighted by Chance). Empty string when the pool
	//! is missing, empty, or its total weight is <= 0.
	static string RollPreset(array<ref dmLoadoutPreset> presets)
	{
		if (!presets || presets.Count() == 0)
			return "";

		int i;
		float total = 0.0;
		for (i = 0; i < presets.Count(); i++)
			total += presets[i].Chance;

		if (total <= 0.0)
			return "";

		float r = Math.RandomFloat01() * total;
		float acc = 0.0;
		for (i = 0; i < presets.Count(); i++)
		{
			acc += presets[i].Chance;
			if (r < acc)
				return presets[i].Name;
		}
		return presets[presets.Count() - 1].Name;
	}

	//! True when the item is eligible under the active preset. An item with no
	//! Conform list (or no rolled preset) is always eligible.
	static bool ConformOk(dmLoadoutItem item, string preset)
	{
		if (preset == "")
			return true;
		if (!item.Conform || item.Conform.Count() == 0)
			return true;

		int i;
		for (i = 0; i < item.Conform.Count(); i++)
		{
			if (item.Conform[i] == preset)
				return true;
		}
		return false;
	}

	//! Weighted pick of one candidate item from a slot. Uses the "nothing
	//! threshold": r = RandomFloat01() * max(total, 1.0), pick the item whose
	//! cumulative Chance first exceeds r, or return null when r >= total. So a
	//! slot whose eligible total is < 1.0 has a (1 - total) chance to stay empty.
	static dmLoadoutItem PickItem(array<ref dmLoadoutItem> items, string preset)
	{
		if (!items || items.Count() == 0)
			return null;

		int i;
		float total = 0.0;
		for (i = 0; i < items.Count(); i++)
		{
			if (ConformOk(items[i], preset))
				total += items[i].Chance;
		}

		if (total <= 0.0)
			return null;

		float r = Math.RandomFloat01() * Math.Max(total, 1.0);
		float acc = 0.0;
		for (i = 0; i < items.Count(); i++)
		{
			if (!ConformOk(items[i], preset))
				continue;
			acc += items[i].Chance;
			if (r < acc)
				return items[i];
		}
		return null;
	}

	//! Independent chance roll (cargo items).
	static bool RollChance(float chance)
	{
		return Math.RandomFloat01() < chance;
	}

	//! Pick a random class name from the candidates; "" when none.
	static string RandomClassName(array<string> classNames)
	{
		if (!classNames || classNames.Count() == 0)
			return "";
		return classNames[Math.RandomInt(0, classNames.Count())];
	}

	//! Roll a value in a range; 1.0 when the range is absent.
	static float RollRange(dmLoadoutRange range)
	{
		if (!range)
			return 1.0;
		return Math.RandomFloat(range.Min, range.Max);
	}

	//------------------------------------------------------------------
	// Creation (in place, vanilla spawn-gear style)
	//------------------------------------------------------------------

	//! Create a top-level item directly in one of the pawn's slots. "Hands" is a
	//! special location; every other name resolves to an attachment slot.
	static void CreateInSlot(PlayerBase pawn, string slotName, dmLoadoutItem itemCfg, string preset)
	{
		string cls = RandomClassName(itemCfg.ClassName);
		if (cls == "")
			return;

		string slotKey = slotName;
		slotKey.ToLower();

		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Loadout.Create");
		#endif

		EntityAI item;
		if (slotKey == "hands")
		{
			item = pawn.GetHumanInventory().CreateInHands(cls);
		}
		else
		{
			int slotId = InventorySlots.GetSlotIdFromString(slotName);
			if (slotId == InventorySlots.INVALID)
				item = pawn.GetInventory().CreateAttachment(cls);
			else
				item = pawn.GetInventory().CreateAttachmentEx(cls, slotId);
		}

		if (!item)
		{
			dmBotLog.Error("Loadout FAILED: " + cls + " -> slot '" + slotName + "'");
			return;
		}

		#ifdef DM_BOT_DEBUG_LOADOUT
		dmBotLog.Debug("Loadout + " + cls + " -> slot '" + slotName + "'");
		#endif

		FinishItem(item, itemCfg, preset);
	}

	//! Create a top-level item in the pawn's general cargo.
	static void CreateInCargo(EntityAI parent, dmLoadoutItem itemCfg, string preset)
	{
		string cls = RandomClassName(itemCfg.ClassName);
		if (cls == "")
			return;

		EntityAI item = CreateChild(parent, cls);
		if (!item)
		{
			dmBotLog.Error("Loadout FAILED: " + cls + " -> cargo");
			return;
		}

		#ifdef DM_BOT_DEBUG_LOADOUT
		dmBotLog.Debug("Loadout + " + cls + " -> cargo");
		#endif

		FinishItem(item, itemCfg, preset);
	}

	//! Create one nested child (attachment or cargo) inside the parent.
	static void CreateChildItem(EntityAI parent, dmLoadoutItem childCfg, string preset)
	{
		string cls = RandomClassName(childCfg.ClassName);
		if (cls == "")
			return;

		EntityAI child = CreateChild(parent, cls);
		if (!child)
		{
			dmBotLog.Error("Loadout FAILED: " + cls + " -> " + parent.GetType());
			return;
		}

		#ifdef DM_BOT_DEBUG_LOADOUT
		dmBotLog.Debug("Loadout + " + cls + " -> " + parent.GetType());
		#endif

		FinishItem(child, childCfg, preset);
	}

	//! Create a child item of class `cls` in/on the parent. Weapons load
	//! magazines via SpawnAmmo (internal/external mags); everything else goes
	//! through CreateInInventory, with the nested-container fallback.
	static EntityAI CreateChild(EntityAI parent, string cls)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Loadout.Create");
		#endif

		Weapon_Base wep;
		if (parent.IsWeapon() && g_Game.ConfigIsExisting(CFG_MAGAZINESPATH + " " + cls))
		{
			if (Class.CastTo(wep, parent) && wep.SpawnAmmo(cls) && !wep.HasInternalMagazine(-1))
			{
				Magazine mag;
				int i;
				int muzzleCount = wep.GetMuzzleCount();
				for (i = 0; i < muzzleCount; i++)
				{
					if (Class.CastTo(mag, wep.GetMagazine(i)) && mag.GetType() == cls)
						return mag;
				}
			}
			return null;
		}

		EntityAI item = parent.GetInventory().CreateInInventory(cls);
		if (item)
			return item;

		return CreateInContainerFallback(parent, cls);
	}

	//! Vanilla workaround for the nested-container bug: the engine cannot create
	//! an item inside a container that is itself inside another container's
	//! cargo. Spawn the item on the ground and add it; if that still fails, move
	//! the parent container to the ground, add the item, move it back
	//! (reference: ExpLootSpawner.c DMCreateInInventory).
	static EntityAI CreateInContainerFallback(EntityAI parent, string cls)
	{
		#ifdef DM_BOT_DEBUG_LOADOUT
		dmBotLog.Debug("Loadout fallback (nested container): " + cls + " in " + parent.GetType());
		#endif

		EntityAI item = EntityAI.Cast(GetGame().CreateObject(cls, "0 0 0"));
		if (!item)
			return null;

		if (parent.GetInventory().CanAddEntityToInventory(item))
		{
			if (parent.GetInventory().AddEntityToInventory(item))
				return item;
		}

		InventoryLocation parentLoc = new InventoryLocation();
		if (!parent.GetInventory().GetCurrentInventoryLocation(parentLoc))
		{
			GetGame().ObjectDelete(item);
			return null;
		}

		vector transform[4];
		InventoryLocation ground = new InventoryLocation();
		ground.SetGround(parent, transform);

		GetGame().RemoteObjectTreeDelete(parent);

		bool placedWell = false;
		if (parent.GetInventory().TakeToDst(InventoryMode.SERVER, parentLoc, ground))
		{
			placedWell = parent.GetInventory().AddEntityToInventory(item);
			parent.GetInventory().TakeToDst(InventoryMode.SERVER, ground, parentLoc);
		}

		GetGame().RemoteObjectTreeCreate(parent);

		if (placedWell)
		{
			item.SetSynchDirty();
			return item;
		}

		GetGame().ObjectDelete(item);
		return null;
	}

	//! Apply health/quantity and recurse into attachments and cargo.
	static void FinishItem(EntityAI item, dmLoadoutItem itemCfg, string preset)
	{
		ApplyHealth(item, itemCfg.Health);
		ApplyQuantity(item, itemCfg.Quantity);
		FillAttachments(item, itemCfg, preset);
		FillCargo(item, itemCfg, preset);
	}

	//! Fill the item's attachment slots (one weighted winner per slot).
	static void FillAttachments(EntityAI item, dmLoadoutItem itemCfg, string preset)
	{
		if (!itemCfg.Attachments)
			return;

		int i;
		dmLoadoutSlot attSlot;
		dmLoadoutItem attCfg;
		for (i = 0; i < itemCfg.Attachments.Count(); i++)
		{
			attSlot = itemCfg.Attachments[i];
			attCfg = PickItem(attSlot.Items, preset);
			if (!attCfg)
				continue;
			CreateChildItem(item, attCfg, preset);
		}
	}

	//! Fill the item's cargo (each child rolls its Chance independently).
	static void FillCargo(EntityAI item, dmLoadoutItem itemCfg, string preset)
	{
		if (!itemCfg.Cargo)
			return;

		int i;
		dmLoadoutItem cargoCfg;
		for (i = 0; i < itemCfg.Cargo.Count(); i++)
		{
			cargoCfg = itemCfg.Cargo[i];
			if (!ConformOk(cargoCfg, preset) || !RollChance(cargoCfg.Chance))
				continue;
			CreateChildItem(item, cargoCfg, preset);
		}
	}

	//! Apply a Health fraction (0..1) to GlobalHealth.
	static void ApplyHealth(EntityAI item, dmLoadoutRange health)
	{
		if (!health)
			return;
		item.SetHealth01("", "Health", RollRange(health));
	}

	//! Apply a Quantity fraction (0..1). Magazines default to a full mag (their
	//! "quantity" is the ammo count); ammo piles and other stackables use
	//! Lerp(min, max, fraction) like the vanilla spawn-gear system.
	static void ApplyQuantity(EntityAI item, dmLoadoutRange quantity)
	{
		Magazine mag = Magazine.Cast(item);
		if (mag && !mag.IsAmmoPile())
		{
			if (quantity)
				mag.ServerSetAmmoCount(Math.Round(Math.Lerp(0, mag.GetAmmoMax(), RollRange(quantity))));
			else
				mag.ServerSetAmmoCount(mag.GetAmmoMax());
			return;
		}

		if (!quantity)
			return;

		ItemBase ib = ItemBase.Cast(item);
		if (!ib || !ib.HasQuantity())
			return;

		ib.SetQuantity(Math.Round(Math.Lerp(ib.GetQuantityMin(), ib.GetQuantityMax(), RollRange(quantity))));
	}
}
