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

		if (Target.Type != TargetType.Editor)
		{
			RuntimeDependencies.Add("$(TargetOutputDir)/IDA_Arduino.json", ConfigSource);
		}
	}
}
