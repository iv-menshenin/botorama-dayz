class CfgPatches
{
	class dmBotorama_Test
	{
		units[]= {};
		weapons[]={};
		requiredVersion=0.1;
		requiredAddons[]= {
			"dmBotorama_Cons",
			"dmBotorama_Reg",
			"dmBotorama_Roads",
			"dmBotorama_Core",
			"dmBotorama_Loadout",
			"dmBotorama_Map",
		};
	};
};

class CfgMods 
{
	class dmBotorama_Test
	{
		name = "botorama";
		author = "devalio";
		type = "mod";
		defines[] = { "DM_BOT_PROFILE", "DM_BOT_DEBUG_FSM", "DM_WEAPON_DEBUG_FSM", "DM_BOT_DEBUG_SPAWN", "DM_BOT_DEBUG_E2E" };
		class defs 
		{
			class gameScriptModule {
				value = "";
				files[] = {
					"dm_test/3_Game",
				};
			};
			class worldScriptModule {
				value = "";
				files[] = {
					"dm_test/4_World",
				};
			};
			class missionScriptModule {
				value = "";
				files[] = {
					"dm_test/5_Mission",
				};
			};
		}; 
	};
};
