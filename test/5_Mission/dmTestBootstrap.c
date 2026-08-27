//! dmTestBootstrap — registers the test command modules into the command manager.
//!
//! Lives in test/5_Mission because the modules (dmBotCommand/dmFSMCommand/dmTestCommand)
//! are defined here, and the module load order is cons -> core -> test.

modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();

		dmBotLog.LogVersion();

		dmCommandManager mgr = dmCommandManager.GetInstance();
		mgr.Register(new dmBotCommand());
		mgr.Register(new dmFSMCommand());
		mgr.Register(new dmTestCommand());
		mgr.Register(new dmProfCommand());
	}
}
