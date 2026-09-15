//! dmBotorama — game module constants (loaded first, visible to all modules).

//! Mod version (increment on every change so you can verify the loaded build).
//! Lives in the game module because dmBotLog (also game module) prints it, and
//! the game module compiles before the world module.
static const string DM_BOTORAMA_VERSION = "3.172";

//! Bullet-drop compensation: initial per-bot learnable coefficient (start ~0.8).
static const float DM_DROP_COEF_INIT = 0.8;
//! Bullet-drop compensation: learning rate of the coefficient update per miss
//! (multiplicative: coef *= 1 + rate * ratio).
static const float DM_DROP_LEARN_RATE = 0.6;
//! Bullet-drop compensation: clamp bounds of the learned coefficient.
static const float DM_DROP_COEF_MIN = 0.1;
static const float DM_DROP_COEF_MAX = 1.5;
//! Bullet-drop learning: target closer than this (m) — learning disabled (drop/lead
//! are negligible and the feedback is mostly noise/dispersion).
static const float DM_DROP_MIN_FEEDBACK_DIST = 50.0;
//! Bullet-drop learning: ignore a shot whose impact is closer than this fraction of
//! the target distance (hit a pole/fence/tree near the shooter — not a ballistic miss).
static const float DM_DROP_MIN_FEEDBACK_FRAC = 0.66;
//! Gravity (m/s^2) for the descent-slope estimate in the drop feedback.
static const float DM_AI_GRAVITY = 9.81;

//! Lateral (horizontal) aim correction: learned per-bot angular offset (radians)
//! accumulated from the signed perpendicular miss (geometric, wind-independent).
static const float DM_LAT_COEF_INIT = 0.0;
//! Lateral correction: damping of the accumulated offset per miss.
static const float DM_LAT_LEARN_RATE = 0.2;
//! Lateral correction: clamp bounds of the learned angle (radians, ~±3°).
static const float DM_LAT_COEF_MIN = -0.05;
static const float DM_LAT_COEF_MAX = 0.05;
//! Lateral feedback: miss angle (radians, ≈1.7°) above which the impact is an
//! outlier (target changed speed/trajectory, bad lead) — skip the whole feedback.
static const float DM_FEEDBACK_MAX_MISS_ANGLE = 0.03;

//! Lead: min target speed (m/s) to consider the target moving (walking speed).
static const float DM_LEAD_SPEED_EPS = 2.0;

//! Lead: velocity change (m/s) above which the pseudo-average (EMA) velocity
//! JUMPS to the new velocity instead of smoothing — target changed speed/trajectory.
static const float DM_LEAD_VEL_CHANGE_EPS = 2.0;

//! Lead: velocity change (m/s) above which the learned lateral correction
//! (latCorr — wind+lead residual) is RESET — the old value no longer applies.
static const float DM_LAT_RESET_VEL_EPS = 4.0;

//! Ricochet filter: impacts slower than this (m/s) are ricochets, not the
//! bullet's first ground hit — ignore them in the feedback.
static const float DM_DROP_MIN_IMPACT_SPEED = 200.0;

//! Hearing: threat assigned to a target heard but not seen (below attack threshold 0.5).
static const float DM_NOISE_THREAT = 0.4;

//! Hearing: noise ranges (meters) per source, for the distance filter.
static const float DM_NOISE_GUNSHOT_STRENGTH = 3000.0;
static const float DM_NOISE_STEP_STRENGTH = 10.0;
static const float DM_NOISE_BULLETIMPACT_STRENGTH = 15.0;
static const float DM_NOISE_SCREAM_STRENGTH = 30.0;

//! Hearing: gunshot range (meters) when the weapon has a suppressor, by kind.
static const float DM_NOISE_GUNSHOT_SILENCED_PISTOL = 75.0;
static const float DM_NOISE_GUNSHOT_SILENCED_RIFLE = 100.0;
static const float DM_NOISE_GUNSHOT_SILENCED_HOMEMADE = 150.0;

//! Hearing: attractiveness (0..1) assigned to a heard target, by noise kind.
static const float DM_NOISE_ATTRACTIVENESS_NOISE = 0.6;
static const float DM_NOISE_ATTRACTIVENESS_SHOT = 0.9;

//! Кулдаун молчания бота: минимальный интервал между случайными репликами (сек, 10 мин).
static const float DM_VOICE_COOLDOWN = 600.0;
//! Верхняя граница lineId реплики (category*100 + index). Используется в net-var и guard.
static const int DM_VOICE_LINE_MAX = 1000;

//! Тип точки интереса / здания (мир). Используется reg (живой реестр зданий +
//! интерьер-карта) и map (реестр локаций). Строковые имена в JSON конфигах
//! ("WATER", "POLICE", ...) совпадают с именами значений.
enum dmWorldPOIType
{
	NONE = 0,
	WATER,
	POLICE,
	FIRE,
	MEDICAL,
	MILITARY,
	MILITARY_WRECK,
	FUEL,
	INDUSTRIAL,
	RESIDENTIAL,
	GENERIC
};