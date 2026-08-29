//! dmLoadoutConfig — JSON schema for a bot loadout.
//!
//! A loadout is a weighted recipe for what a bot wears and carries. It is a
//! plain data structure deserialized from a "*.json" file in DM_LOADOUT_DIR;
//! dmLoadoutApplier turns it into actual items on a pawn.
//!
//! Field reference and examples: loadouts.md (mod root).

//! A random range (inclusive bounds) for Health/Quantity fractions.
class dmLoadoutRange
{
	//! Lower bound (inclusive).
	float Min;

	//! Upper bound (inclusive).
	float Max;
}

//! One entry in the preset pool. The active preset is rolled once per
//! application, weighted by Chance.
class dmLoadoutPreset
{
	//! Preset name, matched (case-sensitively) against an item's Conform list.
	string Name;

	//! Weight when rolling the active preset.
	float Chance = 1.0;
}

//! A single item to spawn. Recursive: it can carry attachments (slots) and
//! cargo (items) of its own.
class dmLoadoutItem
{
	//! Candidate class names; one is picked at random.
	ref array<string> ClassName;

	//! Weight in a slot pick, or independent spawn chance in cargo.
	float Chance = 1.0;

	//! Health fraction range (0..1), applied to GlobalHealth.
	ref dmLoadoutRange Health;

	//! Quantity fraction range (0..1 of the item's max stack size).
	ref dmLoadoutRange Quantity;

	//! Presets this item belongs to; empty = always eligible.
	ref array<string> Conform;

	//! Attachment slots to fill on this item.
	ref array<ref dmLoadoutSlot> Attachments;

	//! Cargo items to place inside this item.
	ref array<ref dmLoadoutItem> Cargo;
}

//! One slot to fill: a target slot name and the weighted candidates for it.
class dmLoadoutSlot
{
	//! Target slot name ("Hands", "Legs", "WeaponOptics", ...).
	string SlotName;

	//! Candidate items (weighted pick, one winner).
	ref array<ref dmLoadoutItem> Items;
}

//! Root of a loadout file. Inherits dmJsonConfigBase for schema versioning
//! (the "Version" field).
class dmLoadoutConfig : dmJsonConfigBase
{
	//! Current schema version.
	static const int VERSION = 1;

	//! Preset pool, rolled once per application.
	ref array<ref dmLoadoutPreset> Presets;

	//! Equipment slots to fill on the pawn.
	ref array<ref dmLoadoutSlot> Slots;

	//! Items to place into the pawn's general cargo.
	ref array<ref dmLoadoutItem> Cargo;

	override void Defaults()
	{
		Presets = new array<ref dmLoadoutPreset>();
		Slots = new array<ref dmLoadoutSlot>();
		Cargo = new array<ref dmLoadoutItem>();
		Version = VERSION;
	}

	override void FixVersion()
	{
		//! No migrations yet (v1 is the first schema). Just stamp the version so
		//! older/version-less files get a write-back and become valid v1.
		if (Version < VERSION)
			Version = VERSION;
	}
}
