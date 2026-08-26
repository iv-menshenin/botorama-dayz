//! dmJsonConfigBase — base class for versioned JSON config structs.
//!
//! A config struct that needs schema versioning inherits this. The version field
//! is named "Version" (no "m_" prefix) so it serializes as "Version" in the JSON
//! and config files stay clean. dmJsonFile<T> detects this base via a cast and,
//! when the loaded version is older than the current one, runs FixVersion() and
//! writes the migrated config back to disk.

class dmJsonConfigBase
{
	//! Schema version of the loaded data (0 = not versioned yet).
	int Version;

	//! Version stored in the currently loaded config.
	int GetVersion()
	{
		return Version;
	}

	//! Migrate fields from Version up to the current VERSION. Concrete classes
	//! override this: run one `if (Version < N)` block per step, fill
	//! newly-added fields with their defaults, then set Version = VERSION.
	void FixVersion()
	{
	}

	//! Set hardcoded defaults (and Version = VERSION) for a fresh config.
	void Defaults()
	{
	}
}
