// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ArduinoProject : ModuleRules
{
	public ArduinoProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });


		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "MediaAssets" });

        if (Target.Type != TargetType.Editor && Target.ProjectFile != null)
        {
            string Root = Target.ProjectFile.Directory.FullName;
            RuntimeDependencies.Add("$(TargetOutputDir)/IDA_Videos.json", Path.Combine(Root, "IDA_Videos.json"), StagedFileType.NonUFS);
            foreach (string Video in Directory.GetFiles(Path.Combine(Root, "Videos"), "*", SearchOption.AllDirectories))
            {
                string Relative = Path.GetRelativePath(Path.Combine(Root, "Videos"), Video);
                RuntimeDependencies.Add("$(TargetOutputDir)/Videos/" + Relative, Video, StagedFileType.NonUFS);
            }
        }

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "BlueprintGraph", "KismetCompiler", "MediaAssets", "GraphEditor", "Slate", "SlateCore" });
        }


        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
