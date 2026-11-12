// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class TareAndTithe : ModuleRules
{
	public TareAndTithe(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] {
			Path.Combine(ModuleDirectory, "Public")
		});

		PrivateIncludePaths.AddRange(new string[] {
			Path.Combine(ModuleDirectory, "Private")
		});

		SetupModulePhysicsSupport(Target);

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GeometryCore",
			"GeometryAlgorithms",
			"DynamicMesh",
			"GeometryFramework",
			"GeometryScriptingCore",
			"PhysicsCore",
			"AudioExtensions",
			"MetasoundEngine",
			"Niagara",
			"UMG"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore",
			"RenderCore"
		});
	}
}
