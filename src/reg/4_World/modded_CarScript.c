//! modded CarScript — нативный хук ввода/звука для ИИ-вождения.
//!
//! Сеттеры газа/тормоза/руля (SetThrottle/SetBrake/SetSteering) задают будущее
//! состояние ввода машины, но применяются только из Transport.OnInput(float dt) —
//! штатного хука «Called after every input simulation step» (research:
//! vehicles.md «Ключевой хук: Transport.OnInput»). Из OnUpdate/интента они мёртвые.
//!
//! Интент dmBotIntent_Drive ведёт машину импульсом (dBodyApplyImpulseAt): на ручной
//! КПП без сцепления нативный газ буксует (rpm высокий, колёса не крутятся) и глохнет
//! при замыкании сцепления, поэтому газ/тормоз при езде пишутся нулями, а OnInput
//! применяет только руль (dm_DriveSteering) пока dm_DriveActive. Тормоз (dm_DriveBrake)
//! — только в фазе остановки сот/объезда (нейтраль, мотор не глохнет). Обороты двигателя
//! (RPM-звук) нативный газ не поднимает; OnSound может подменять контроллер RPM
//! симуляцией dm_DriveSimRPM по скорости (пишет интент; <0 — off). OnInput/OnSound
//! вызываются на сервере для машины с серверным ИИ-водителем (PHYSICS-стратегия) и
//! безвредны на клиенте (dm_DriveActive == false / dm_DriveSimRPM == -1).
modded class CarScript
{
	float dm_DriveThrottle;    // 0..1 — при езде 0 (импульсный привод), нативно не используется
	float dm_DriveBrake;       // 0..1 — 0 при езде; 1.0 только в фазе остановки (соты)
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
