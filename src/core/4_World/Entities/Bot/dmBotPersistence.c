//! dmBotPersistence — бинарная персистенция физического состояния settlement-бота.
//!
//! Сохраняет/восстанавливает в бинарный файл ($profile:dmBotorama/settlement/<name>.bin):
//! позицию/ориентацию, script-цепочку PlayerBase (OnStoreSave/OnStoreLoad), здоровье по
//! зонам и инвентарь (экипировка + руки) рекурсивно. Внутреннее состояние оружия/магазина/
//! энергомеханики НЕ сериализуем (вне скоупа v1 — techdebt).
//!
//! Формат строго последовательный (запись == чтение в одном порядке):
//!   int    persistVersion  (= DM_PERSIST_FORMAT_VERSION)
//!   string name / modelClass / settlement
//!   vector position / orientation
//!   int    saveVersion     (g_Game.SaveVersion())
//!   ...    OnStoreSave пейлоад
//!   ...    здоровье (7 зон × (zone + Health + Blood))
//!   ...    инвентарь (13 слотов + руки, рекурсивно)
//!
//! Все методы статические, персистенция выполняется только на сервере.

#ifdef SERVER

class dmBotPersistence
{
	//! Зоны здоровья (совпадают с эталоном devaliada OSD_WritePlayerHealth).
	static ref TStringArray s_HealthZones = {
		"LeftLeg", "RightLeg", "RightFoot", "LeftFoot", "Head", "Torso", "GlobalHealth"
	};

	//! Слоты экипировки (эталон devaliada osd_PlayerAttachmentSlots без HANDS).
	static ref array<int> s_WearSlots = {
		InventorySlots.SHOULDER,
		InventorySlots.MELEE,
		InventorySlots.HEADGEAR,
		InventorySlots.MASK,
		InventorySlots.EYEWEAR,
		InventorySlots.GLOVES,
		InventorySlots.ARMBAND,
		InventorySlots.BODY,
		InventorySlots.VEST,
		InventorySlots.BACK,
		InventorySlots.HIPS,
		InventorySlots.LEGS,
		InventorySlots.FEET
	};

	//! Каталог бинарников персистенции.
	static string PersistDir()
	{
		return DM_PERSIST_DIR;
	}

	//! Имя → безопасное имя файла (без пробелов и разделителей пути).
	static string SanitizeName(string name)
	{
		string result = name;
		result.Replace(" ", "_");
		result.Replace("/", "_");
		result.Replace("\\", "_");
		return result;
	}

	//! Полный путь бинарника по отображаемому имени.
	static string PathFor(string name)
	{
		return DM_PERSIST_DIR + "/" + SanitizeName(name) + ".bin";
	}

	//! Создать цепочку каталогов персистенции ($profile:dmBotorama/settlement).
	static void EnsureDir()
	{
		EnsureDirectory(DM_PERSIST_DIR);
		if (!FileExist(DM_PERSIST_DIR))
			MakeDirectory(DM_PERSIST_DIR);
	}

	//! Записать полное состояние бота (заголовок + OnStoreSave + здоровье + инвентарь).
	static bool Save(dmAISurvivor bot, string settlement)
	{
		if (!bot)
		{
			dmBotLog.Error("", "[Persist] Save: bot is null");
			return false;
		}

		dmAISurvivorBase pawn = bot.GetPawn();
		if (!pawn)
		{
			dmBotLog.Error(bot.GetName(), "[Persist] Save: pawn is null");
			return false;
		}

		EnsureDir();

		string path = PathFor(bot.GetName());

		FileSerializer file = new FileSerializer();
		if (!file.Open(path, FileMode.WRITE))
		{
			dmBotLog.Error(bot.GetName(), "[Persist] Save: cannot open " + path);
			return false;
		}

		bool ok = WriteHeader(file, bot, pawn, settlement);
		if (ok)
			ok = WriteStateAndBody(file, pawn);

		file.Close();

		if (!ok)
		{
			dmBotLog.Error(bot.GetName(), "[Persist] Save: write failed for " + path);
			return false;
		}

		#ifdef DM_BOT_DEBUG_PERSIST
		dmBotLog.Debug(bot.GetName(), "[Persist] Save: wrote " + path);
		#endif
		return true;
	}

	//! Прочитать только заголовок (без создания пешки) — для решения «восстанавливать/спавнить».
	static bool ReadHeader(string path, out string name, out string modelClass, out string settlement, out vector position, out vector orientation)
	{
		FileSerializer file = new FileSerializer();
		if (!file.Open(path, FileMode.READ))
		{
			dmBotLog.Error("", "[Persist] ReadHeader: cannot open " + path);
			return false;
		}

		int persistVersion;
		bool ok = file.Read(persistVersion);
		if (ok && persistVersion != DM_PERSIST_FORMAT_VERSION)
		{
			dmBotLog.Error("", "[Persist] unsupported format version " + persistVersion + " (expected " + DM_PERSIST_FORMAT_VERSION + ")");
			file.Close();
			return false;
		}
		if (ok)
			ok = file.Read(name);
		if (ok)
			ok = file.Read(modelClass);
		if (ok)
			ok = file.Read(settlement);
		if (ok)
			ok = file.Read(position);
		if (ok)
			ok = file.Read(orientation);

		file.Close();

		if (!ok)
		{
			dmBotLog.Error("", "[Persist] ReadHeader: read failed for " + path);
			return false;
		}
		return true;
	}

	//! Восстановить state+health+inventory в уже созданную пешку (позицию/ориентацию применяет вызывающий).
	static bool RestoreState(dmAISurvivorBase pawn, string path)
	{
		if (!pawn)
		{
			dmBotLog.Error("", "[Persist] RestoreState: pawn is null");
			return false;
		}

		FileSerializer file = new FileSerializer();
		if (!file.Open(path, FileMode.READ))
		{
			dmBotLog.Error(pawn.GetCachedName(), "[Persist] RestoreState: cannot open " + path);
			return false;
		}

		bool ok = SkipHeader(file);
		if (ok)
			ok = ReadStateAndBody(file, pawn);

		file.Close();

		if (!ok)
		{
			dmBotLog.Error(pawn.GetCachedName(), "[Persist] RestoreState: read failed for " + path);
			return false;
		}

		#ifdef DM_BOT_DEBUG_PERSIST
		dmBotLog.Debug(pawn.GetCachedName(), "[Persist] RestoreState: restored " + path);
		#endif
		return true;
	}

	//! Удалить бинарник (по смерти бота).
	static bool Delete(string path)
	{
		if (path == "")
			return false;

		if (!FileExist(path))
			return false;

		if (!DeleteFile(path))
		{
			dmBotLog.Error("", "[Persist] Delete: cannot delete " + path);
			return false;
		}

		#ifdef DM_BOT_DEBUG_PERSIST
		dmBotLog.Debug("", "[Persist] Delete: removed " + path);
		#endif
		return true;
	}

	//! Заголовок: persistVersion, name, modelClass, settlement, position, orientation.
	private static bool WriteHeader(Serializer ctx, dmAISurvivor bot, dmAISurvivorBase pawn, string settlement)
	{
		vector position = pawn.GetPosition();
		vector orientation = pawn.GetOrientation();

		if (!ctx.Write(DM_PERSIST_FORMAT_VERSION))
			return false;
		if (!ctx.Write(bot.GetName()))
			return false;
		if (!ctx.Write(bot.GetModel()))
			return false;
		if (!ctx.Write(settlement))
			return false;
		if (!ctx.Write(position))
			return false;
		if (!ctx.Write(orientation))
			return false;
		return true;
	}

	//! saveVersion + OnStoreSave + здоровье + инвентарь.
	private static bool WriteStateAndBody(Serializer ctx, dmAISurvivorBase pawn)
	{
		int saveVersion = g_Game.SaveVersion();
		if (!ctx.Write(saveVersion))
			return false;

		pawn.OnStoreSave(ctx);

		if (!StoreHealth(ctx, pawn))
			return false;

		if (!StoreInventory(ctx, pawn))
			return false;

		return true;
	}

	//! Пропустить заголовок (значения уже применены вызывающим).
	private static bool SkipHeader(Serializer ctx)
	{
		int persistVersion;
		string name;
		string modelClass;
		string settlement;
		vector position;
		vector orientation;

		bool ok = ctx.Read(persistVersion);
		if (ok && persistVersion != DM_PERSIST_FORMAT_VERSION)
		{
			dmBotLog.Error("", "[Persist] SkipHeader: unsupported format version " + persistVersion);
			return false;
		}
		if (ok)
			ok = ctx.Read(name);
		if (ok)
			ok = ctx.Read(modelClass);
		if (ok)
			ok = ctx.Read(settlement);
		if (ok)
			ok = ctx.Read(position);
		if (ok)
			ok = ctx.Read(orientation);
		return ok;
	}

	//! saveVersion + OnStoreLoad + здоровье + инвентарь + AfterStoreLoad.
	private static bool ReadStateAndBody(Serializer ctx, dmAISurvivorBase pawn)
	{
		int saveVersion;
		if (!ctx.Read(saveVersion))
			return false;

		if (!pawn.OnStoreLoad(ctx, saveVersion))
		{
			dmBotLog.Error(pawn.GetCachedName(), "[Persist] RestoreState: OnStoreLoad failed");
			return false;
		}

		if (!LoadHealth(ctx, pawn))
			return false;

		if (!LoadInventory(ctx, pawn))
			return false;

		pawn.AfterStoreLoad();
		return true;
	}

	//! Здоровье: 7 зон × (zone + Health + Blood).
	private static bool StoreHealth(Serializer ctx, dmAISurvivorBase pawn)
	{
		int i;
		for (i = 0; i < s_HealthZones.Count(); i++)
		{
			string zone = s_HealthZones.Get(i);
			if (!ctx.Write(zone))
				return false;
			if (!ctx.Write(pawn.GetHealth(zone, "Health")))
				return false;
			if (!ctx.Write(pawn.GetHealth(zone, "Blood")))
				return false;
		}
		return true;
	}

	//! Здоровье: чтение симметрично StoreHealth.
	private static bool LoadHealth(Serializer ctx, dmAISurvivorBase pawn)
	{
		int i;
		for (i = 0; i < s_HealthZones.Count(); i++)
		{
			string zone;
			if (!ctx.Read(zone))
				return false;

			float health;
			if (!ctx.Read(health))
				return false;

			float blood;
			if (!ctx.Read(blood))
				return false;

			pawn.SetHealth(zone, "Health", health);
			pawn.SetHealth(zone, "Blood", blood);
		}
		return true;
	}

	//! Инвентарь: 13 слотов экипировки + руки.
	private static bool StoreInventory(Serializer ctx, dmAISurvivorBase pawn)
	{
		int i;
		for (i = 0; i < s_WearSlots.Count(); i++)
		{
			int slotID = s_WearSlots.Get(i);
			if (!ctx.Write(slotID))
				return false;

			ItemBase item = ItemBase.Cast(pawn.GetInventory().FindAttachment(slotID));
			bool hasItem = item != null;
			if (!ctx.Write(hasItem))
				return false;

			if (hasItem)
			{
				if (!StoreItem(ctx, item))
					return false;
			}
		}

		ItemBase inHands = ItemBase.Cast(pawn.GetItemInHands());
		bool hasInHands = inHands != null;
		if (!ctx.Write(hasInHands))
			return false;
		if (hasInHands)
		{
			if (!StoreItem(ctx, inHands))
				return false;
		}
		return true;
	}

	//! Инвентарь: чтение симметрично StoreInventory.
	private static bool LoadInventory(Serializer ctx, dmAISurvivorBase pawn)
	{
		int i;
		for (i = 0; i < s_WearSlots.Count(); i++)
		{
			int slotID;
			if (!ctx.Read(slotID))
				return false;

			bool hasItem;
			if (!ctx.Read(hasItem))
				return false;

			if (hasItem)
			{
				if (!LoadAttachment(ctx, pawn.GetInventory(), slotID))
					return false;
			}
		}

		bool hasInHands;
		if (!ctx.Read(hasInHands))
			return false;
		if (hasInHands)
		{
			if (!LoadHands(ctx, pawn))
				return false;
		}
		return true;
	}

	//! Рекурсивная запись предмета: className + health01 + quantity + cargo + attachments.
	private static bool StoreItem(Serializer ctx, ItemBase item)
	{
		if (!ctx.Write(item.GetType()))
			return false;

		if (!ctx.Write(item.GetHealth01()))
			return false;

		float qty = -1.0;
		if (item.HasQuantity())
			qty = item.GetQuantity();
		if (!ctx.Write(qty))
			return false;

		if (!StoreCargo(ctx, item))
			return false;

		if (!StoreAttachments(ctx, item))
			return false;

		return true;
	}

	//! Чтение тела предмета (после className): health01 + quantity + cargo + attachments.
	private static bool LoadItemBody(Serializer ctx, ItemBase item)
	{
		float health;
		if (!ctx.Read(health))
			return false;

		float qty;
		if (!ctx.Read(qty))
			return false;

		if (qty >= 0.0)
			item.SetQuantity(qty);

		item.SetHealth01("", "", health);

		if (!LoadCargo(ctx, item))
			return false;

		if (!LoadAttachments(ctx, item))
			return false;

		return true;
	}

	//! Cargo предмета: hasCargo + itemCount + для каждого (InventoryLocation + рекурсия).
	private static bool StoreCargo(Serializer ctx, ItemBase item)
	{
		CargoBase cargo = item.GetInventory().GetCargo();
		bool hasCargo = cargo != null;
		if (!ctx.Write(hasCargo))
			return false;
		if (!hasCargo)
			return true;

		int itemCount = cargo.GetItemCount();
		if (!ctx.Write(itemCount))
			return false;

		int i;
		for (i = 0; i < itemCount; i++)
		{
			ItemBase cargoItem = ItemBase.Cast(cargo.GetItem(i));
			if (!cargoItem)
			{
				dmBotLog.Error("", "[Persist] StoreCargo: null item at index " + i);
				return false;
			}

			if (!StoreLocation(ctx, cargoItem))
				return false;

			if (!StoreItem(ctx, cargoItem))
				return false;
		}
		return true;
	}

	//! InventoryLocation cargo-предмета: idx + row + col + flip.
	private static bool StoreLocation(Serializer ctx, ItemBase item)
	{
		InventoryLocation loc = new InventoryLocation();
		if (!item.GetInventory().GetCurrentInventoryLocation(loc))
		{
			dmBotLog.Error("", "[Persist] StoreLocation: no InventoryLocation for " + item.GetType());
			return false;
		}

		int idx = loc.GetIdx();
		int row = loc.GetRow();
		int col = loc.GetCol();
		bool flip = loc.GetFlip();

		if (!ctx.Write(idx))
			return false;
		if (!ctx.Write(row))
			return false;
		if (!ctx.Write(col))
			return false;
		if (!ctx.Write(flip))
			return false;
		return true;
	}

	//! Attachments предмета: slotsCount + для каждого (slotID + present + рекурсия).
	private static bool StoreAttachments(Serializer ctx, ItemBase item)
	{
		GameInventory inventory = item.GetInventory();
		int slotsCount = inventory.GetAttachmentSlotsCount();
		if (!ctx.Write(slotsCount))
			return false;

		int i;
		for (i = 0; i < slotsCount; i++)
		{
			int slotID = inventory.GetAttachmentSlotId(i);
			if (!ctx.Write(slotID))
				return false;

			ItemBase attachment = ItemBase.Cast(inventory.FindAttachment(slotID));
			bool present = attachment != null;
			if (!ctx.Write(present))
				return false;

			if (present)
			{
				if (!StoreItem(ctx, attachment))
					return false;
			}
		}
		return true;
	}

	//! Cargo чтение: hasCargo + itemCount + для каждого (location + создание + рекурсия).
	private static bool LoadCargo(Serializer ctx, ItemBase item)
	{
		bool hasCargo;
		if (!ctx.Read(hasCargo))
			return false;
		if (!hasCargo)
			return true;

		int itemCount;
		if (!ctx.Read(itemCount))
			return false;

		GameInventory parent = item.GetInventory();

		int i;
		for (i = 0; i < itemCount; i++)
		{
			int idx;
			if (!ctx.Read(idx))
				return false;

			int row;
			if (!ctx.Read(row))
				return false;

			int col;
			if (!ctx.Read(col))
				return false;

			bool flip;
			if (!ctx.Read(flip))
				return false;

			if (!LoadCargoItem(ctx, parent, idx, row, col, flip))
				return false;
		}
		return true;
	}

	//! Создание cargo-предмета: CreateObject → тело → flip → помещение в карго.
	private static bool LoadCargoItem(Serializer ctx, GameInventory parent, int idx, int row, int col, bool flip)
	{
		string className;
		if (!ctx.Read(className))
			return false;

		ItemBase item = ItemBase.Cast(GetGame().CreateObject(className, Vector(0, 0, 0)));
		if (!item)
		{
			dmBotLog.Error("", "[Persist] LoadCargoItem: cannot create " + className);
			return false;
		}

		if (!LoadItemBody(ctx, item))
			return false;

		item.GetInventory().SetFlipCargo(flip);

		CargoBase cargo = parent.GetCargoFromIndex(idx);
		if (!parent.TakeEntityToTargetCargoEx(InventoryMode.SERVER, cargo, item, row, col))
		{
			if (!parent.TakeEntityToInventory(InventoryMode.SERVER, FindInventoryLocationType.CARGO, item))
			{
				dmBotLog.Error("", "[Persist] LoadCargoItem: cannot place " + className + " into cargo");
				GetGame().ObjectDelete(item);
				return false;
			}
		}
		return true;
	}

	//! Attachments чтение: slotsCount + для каждого (slotID + present + создание).
	private static bool LoadAttachments(Serializer ctx, ItemBase item)
	{
		int slotsCount;
		if (!ctx.Read(slotsCount))
			return false;

		GameInventory inventory = item.GetInventory();

		int i;
		for (i = 0; i < slotsCount; i++)
		{
			int slotID;
			if (!ctx.Read(slotID))
				return false;

			bool present;
			if (!ctx.Read(present))
				return false;

			if (!present)
				continue;

			if (!LoadAttachment(ctx, inventory, slotID))
				return false;
		}
		return true;
	}

	//! Создание attachment: CreateAttachmentEx(className, slotID) → тело.
	private static bool LoadAttachment(Serializer ctx, GameInventory inventory, int slotID)
	{
		string className;
		if (!ctx.Read(className))
			return false;

		ItemBase attachment = ItemBase.Cast(inventory.CreateAttachmentEx(className, slotID));
		if (!attachment)
		{
			dmBotLog.Error("", "[Persist] LoadAttachment: cannot create " + className + " in slot " + slotID);
			return false;
		}

		return LoadItemBody(ctx, attachment);
	}

	//! Создание предмета в руках: CreateInHands(className) → тело.
	private static bool LoadHands(Serializer ctx, dmAISurvivorBase pawn)
	{
		string className;
		if (!ctx.Read(className))
			return false;

		ItemBase item = ItemBase.Cast(pawn.GetHumanInventory().CreateInHands(className));
		if (!item)
		{
			dmBotLog.Error(pawn.GetCachedName(), "[Persist] LoadHands: cannot create " + className);
			return false;
		}

		return LoadItemBody(ctx, item);
	}

	//! Создать цепочку родительских каталогов пути файла (локальная копия
	//! dmJsonFile.EnsureDirectory: статик generic-класса на месте вызова ненадёжен).
	private static void EnsureDirectory(string path)
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

		int i;
		for (i = startFrom; i < comps.Count(); i++)
		{
			if (dir != "")
				dir += "/";
			dir += comps[i];
			if (!FileExist(dir))
				MakeDirectory(dir);
		}
	}
}

#endif
