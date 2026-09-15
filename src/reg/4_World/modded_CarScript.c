//! modded CarScript — нативный хук ввода для ИИ-вождения.
//!
//! Сеттеры газа/тормоза/руля (SetThrottle/SetBrake/SetSteering) задают будущее
//! состояние ввода машины, но применяются только из Transport.OnInput(float dt) —
//! штатного хука «Called after every input simulation step» (research:
//! vehicles.md «Ключевой хук: Transport.OnInput»). Из OnUpdate/интента они мёртвые:
//! SetThrottle(0.6) не поднимает EngineGetRPM() (держит idle 900).
//!
//! Поэтому интент dmBotIntent_Drive пишет желаемое состояние в публичные поля
//! dm_Drive* (газ 0..1, тормоз 0..1, руль -1..1), а этот override применяет их на
//! каждом OnInput, пока dm_DriveActive. OnInput вызывается на сервере для машины с
//! серверным ИИ-водителем (PHYSICS-стратегия симуляции) и безвреден на клиенте
//! (там dm_DriveActive == false).
modded class CarScript
{
	float dm_DriveThrottle;    // 0..1 — пишет интент
	float dm_DriveBrake;       // 0..1
	float dm_DriveSteering;    // -1..1
	bool  dm_DriveActive;      // true = ведём

	override void OnInput(float dt)
	{
		super.OnInput(dt);
		if (dm_DriveActive)
		{
			SetThrottle(dm_DriveThrottle);
			SetBrake(dm_DriveBrake);
			SetSteering(dm_DriveSteering);
		}
	}
}
