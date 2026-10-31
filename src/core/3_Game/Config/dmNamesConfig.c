//! dmNamesConfig — schema of names-long.json (bot name generation).
//!
//! Fields serialize by exact name (no "m_" prefix), matching the JSON keys
//! (snake_case). Loaded via JsonFileLoader<dmNamesConfig> directly (no Version
//! field, so dmJsonConfigBase/dmJsonFile are not used).

//! Один набор имён национальности.
class dmNameNationality
{
	ref array<string> male_first;
	ref array<string> female_first;
	ref array<string> last;
}

//! Корень names-long.json.
class dmNamesConfig
{
	ref map<string, ref dmNameNationality> nationalities;
	ref map<string, string> model_mapping;
}
