//! dmRoads — road discovery probe constants (game module, compiles first).

//! Friction threshold (CfgSurfaces): >= this is a road / drivable surface.
static const float DM_ROAD_FRICTION_MIN = 0.90;
//! Friction threshold: >= this is asphalt/concrete (otherwise dirt/gravel).
static const float DM_ROAD_FRICTION_PAVED = 0.95;

//! Probe step along the road centerline (meters).
static const float DM_ROAD_STEP = 3.0;
//! Short probe step (meters) for boundary protection: a shorter hop in the
//! same direction through a torn road boundary before turning.
static const float DM_ROAD_SMALL_STEP = 2.0;
//! Edge scan step along the perpendicular (meters).
static const float DM_ROAD_EDGE_STEP = 1.0;
//! Max edge scan half-width from the center (meters).
static const float DM_ROAD_MAX_HALF_WIDTH = 12.0;
//! Min road width for a branch to count as a road (vs a sidewalk/driveway).
static const float DM_ROAD_MIN_WIDTH = 6.0;
//! Hard cap on probe steps.
static const int   DM_ROAD_MAX_STEPS = 800;
//! Min steps before the loop-closure check kicks in.
static const int   DM_ROAD_MIN_CLOSE_STEPS = 10;
//! Distance below which the walker considers the road a closed loop (meters).
static const float DM_ROAD_CLOSE_DIST = 6.0;
//! Branch detection circle radius around the seed (meters).
static const float DM_ROAD_BRANCH_RADIUS = 6.0;
//! Min drivable density in the ±3-sample window for a branch peak.
static const int   DM_ROAD_BRANCH_MIN_DENSITY = 3;
//! Min sample gap between branch peaks (samples, 10° each).
static const int   DM_ROAD_BRANCH_MIN_GAP = 6;

//! Graph builder: dedup-grid cell size for visited road cells (meters).
static const float DM_ROAD_DEDUP_CELL = 2.0;
//! Graph builder: dedup-grid cell size for vertex positions (meters).
static const float DM_ROAD_NODE_CELL = 3.0;
//! Angle below which a branch is considered "going back" (degrees).
static const float DM_ROAD_BACK_ANGLE = 30.0;
//! Angle below which two road directions are considered the same (degrees).
static const float DM_ROAD_BRANCH_MATCH_ANGLE = 60.0;
//! Every N steps, check the current point for a junction (side branch).
static const int   DM_ROAD_JUNCTION_CHECK_STEP = 5;
//! Branches shorter than this (in steps) are discarded as garbage.
static const int   DM_ROAD_MIN_BRANCH_STEPS = 5;

//! Grid step of the road-object scan (meters).
static const float DM_ROAD_SCAN_STEP = 20.0;
//! Endpoint snap distance for joining adjacent segments (meters).
static const float DM_ROAD_ENDPOINT_SNAP = 5.0;

//! Output path of the discovered road graph JSON.
static const string DM_ROADS_GRAPH_FILE = "$profile:dmBotorama/roads/road_graph.json";
