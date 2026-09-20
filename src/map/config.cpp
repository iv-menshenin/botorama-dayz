class CfgPatches
{
	class dmBotorama_Map
	{
		units[]= {};
		weapons[]={};
		requiredVersion=0.1;
		requiredAddons[]= {
			"dmBotorama_Cons",
		};
	};
};

class CfgMods 
{
	class dmBotorama_Map
	{
		name = "botorama";
		author = "devalio";
		type = "mod";
		defines[] = { "DM_BOT_PROFILE", "DM_BOT_DEBUG_SPAWN" };
		class defs 
		{
			class gameScriptModule {
				value = "";
				files[] = {
					"dm_map/3_Game",
				};
			};
			class worldScriptModule {
				value = "";
				files[] = {
					"dm_map/4_World",
				};
			};
		}; 
	};
};
