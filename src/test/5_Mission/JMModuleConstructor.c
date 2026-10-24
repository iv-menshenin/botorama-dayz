//! dmBotorama — регистрация COT-модулей команд botorama.
//!
//! Файл целиком под #ifdef JM_COT: без COT класса JMModuleConstructor не
//! существует, и modded class дал бы "base class not found".

#ifdef JM_COT

modded class JMModuleConstructor
{
	override void RegisterModules(out TTypenameArray modules)
	{
		super.RegisterModules(modules);

		modules.Insert(dmCOTBotModule);
		modules.Insert(dmCOTFSMModule);
		modules.Insert(dmCOTTestModule);
		modules.Insert(dmCOTProfModule);
		modules.Insert(dmCOTTpModule);
		modules.Insert(dmCOTShowModule);
		modules.Insert(dmCOTLaunchModule);
		modules.Insert(dmCOTPoiModule);
		modules.Insert(dmCOTSpawnCityModule);
		modules.Insert(dmCOTCarModule);
	}
}

#endif
