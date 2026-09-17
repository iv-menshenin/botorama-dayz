//! dmRoads — road discovery probe constants (game module, compiles first).

//! Probe step along the road centerline (meters).
static const float DM_ROAD_STEP = 3.0;
//! Direction search radius around a point (meters).
static const float DM_ROAD_DIR_RADIUS = 4.0;
//! Edge scan step along the perpendicular (meters).
static const float DM_ROAD_EDGE_STEP = 1.0;
//! Max edge scan half-width from the center (meters).
static const float DM_ROAD_MAX_HALF_WIDTH = 12.0;
//! Hard cap on probe steps.
static const int   DM_ROAD_MAX_STEPS = 200;
//! Min steps before the loop-closure check kicks in.
static const int   DM_ROAD_MIN_CLOSE_STEPS = 10;
//! Distance below which the walker considers the road a closed loop (meters).
static const float DM_ROAD_CLOSE_DIST = 6.0;
