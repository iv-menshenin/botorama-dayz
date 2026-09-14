class CfgPatches
{
	class dmBotorama_Cons
	{
		units[]= {};
		weapons[]={};
		requiredVersion=0.1;
		requiredAddons[]= {};
	};
};

class CfgMods 
{
	class dmBotorama_Cons
	{
		name = "botorama";
		author = "devalio";
		type = "mod";
		class defs 
		{
			class gameScriptModule {
				value = "";
				files[] = {
					"dm_cons/3_Game",
				};
			};
			class worldScriptModule {
				value = "";
				files[] = {
					"dm_cons/4_World",
				};
			};
			class missionScriptModule {
				value = "";
				files[] = {
					"dm_cons/5_Mission",
				};
			};
		}; 
	};
};
