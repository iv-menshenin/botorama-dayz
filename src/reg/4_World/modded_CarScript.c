//! modded CarScript — нативный хук ввода/звука для ИИ-вождения.
//!
//! Сеттеры газа/тормоза/руля (SetThrottle/SetBrake/SetSteering) задают будущее
//! состояние ввода машины, но применяются только из Transport.OnInput(float dt) —
//! штатного хука «Called after every input simulation step» (research:
//! vehicles.md «Ключевой хук: Transport.OnInput»). Из OnUpdate/интента они мёртвые:
//! SetThrottle(0.6) не поднимает EngineGetRPM() (держит idle 900).
//!
//! Интент dmBotIntent_Drive ведёт машину ТОЛЬКО импульсом (dBodyApplyImpulseAt):
//! нативный газ/тормоз сопротивляются импульсу и пишутся нулями, поэтому OnInput
//! применяет только руль (dm_DriveSteering) пока dm_DriveActive. Газ/тормоз —
//! заделка под будущее. Обороты двигателя (RPM-звук) нативный газ не поднимает,
//! поэтому OnSound подменяет контроллер RPM симуляцией dm_DriveSimRPM по скорости
//! (пишет интент; <0 — off). OnInput/OnSound вызываются на сервере для машины с
//! серверным ИИ-водителем (PHYSICS-стратегия) и безвредны на клиенте
//! (dm_DriveActive == false / dm_DriveSimRPM == -1).
modded class CarScript
{
	float dm_DriveThrottle;    // 0..1 — пишет интент (нативно не используется: газ сопротивляется импульсу)
	float dm_DriveBrake;       // 0..1
	float dm_DriveSteering;    // -1..1
	bool  dm_DriveActive;      // true = ведём
	float dm_DriveSimRPM = -1.0; // -1 = off; иначе желаемое значение RPM-звука (пишет интент)

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

	//! Симуляция оборотов двигателя (аудио): нативный газ убран, поэтому RPM-звук
	//! подменяем желаемым значением по скорости. Отрицательное значение — off
	//! (возвращаем native-значение).
	override float OnSound(CarSoundCtrl ctrl, float oldValue)
	{
		if (ctrl == CarSoundCtrl.RPM && dm_DriveSimRPM >= 0.0)
			return dm_DriveSimRPM;
		return super.OnSound(ctrl, oldValue);
	}
}
