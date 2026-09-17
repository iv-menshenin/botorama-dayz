class CfgPatches
{
	class dmBotorama_Roads
	{
		units[]= {};
		weapons[]={};
		requiredVersion=0.1;
		requiredAddons[]= {
			"dmBotorama_Cons",
			"dmBotorama_Core",
		};
	};
};

class CfgMods 
{
	class dmBotorama_Roads
	{
		name = "botorama";
		author = "devalio";
		type = "mod";
		defines[] = { "DM_BOT_PROFILE", "DM_BOT_DEBUG_ROADS" };
		class defs 
		{
			class gameScriptModule {
				value = "";
				files[] = {
					"dm_roads/3_Game",
				};
			};
			class worldScriptModule {
				value = "";
				files[] = {
					"dm_roads/4_World",
				};
			};
		}; 
	};
};
