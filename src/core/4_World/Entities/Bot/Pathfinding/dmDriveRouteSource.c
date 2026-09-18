//! dmDriveRouteSource — base abstraction for a road-route source (core).
//!
//! The drive intent (dmBotIntent_Drive, core) depends on THIS abstraction, not on
//! the concrete router (dmRoadRouter, roads): `roads` loads AFTER `core`, so a
//! direct dmRoadRouter call from core would be an Unknown type at compile time.
//! The router inherits this class and overrides NextChunk; the owning command
//! sets the concrete source into the intent's m_RouteSource before OnStart.
//! Enfusion has no abstract methods, so the base implementation is a no-op false.
class dmDriveRouteSource
{
	bool NextChunk(inout array<vector> waypoints, int lookahead)
	{
		return false;
	}
}
