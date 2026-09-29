using System.IO;
using UnrealBuildTool;

public class ArduinoSerial : ModuleRules
{
	public ArduinoSerial(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "Projects" });


		// Default IDA_Arduino.json shipped with packaged builds.
		// A project can override it by placing its own copy in <Project>/Config/.
		string ConfigSource = Path.Combine(PluginDirectory, "Resources", "IDA_Arduino.json");

		if (Target.ProjectFile != null)
		{
			string ProjectConfig = Path.Combine(Target.ProjectFile.Directory.FullName, "Config", "IDA_Arduino.json");

			if (File.Exists(ProjectConfig))
			{
				ConfigSource = ProjectConfig;
			}
		}

		// Staged next to the real exe (Binaries/Win64). Unreal's packager cannot put
		// loose files in the build root, so the plugin copies it next to the
		// top-level exe when the game starts (see FArduinoSerialModule).
		if (Target.Type != TargetType.Editor)
		{
			RuntimeDependencies.Add("$(TargetOutputDir)/IDA_Arduino.json", ConfigSource);
		}
	}
}
