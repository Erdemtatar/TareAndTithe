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
			"RenderCore",
			"RHI"
		});

		bool bEnableDlssModule = false;
		string ResolvedDlssDescriptor = string.Empty;
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string ProjectDir = Target.ProjectFile != null ? Path.GetDirectoryName(Target.ProjectFile.FullName) : string.Empty;
			string[] CandidateDirectPaths = new string[] {
				!string.IsNullOrEmpty(ProjectDir) ? Path.Combine(ProjectDir, "Plugins", "DLSS", "DLSS.uplugin") : string.Empty,
				Path.Combine(EngineDirectory, "Plugins", "Marketplace", "DLSS", "DLSS.uplugin"),
				Path.Combine(EngineDirectory, "Plugins", "Runtime", "Nvidia", "DLSS", "DLSS.uplugin")
			};

			foreach (string Candidate in CandidateDirectPaths)
			{
				if (!string.IsNullOrEmpty(Candidate) && File.Exists(Candidate))
				{
					bEnableDlssModule = true;
					ResolvedDlssDescriptor = Candidate;
					break;
				}
			}

			if (!bEnableDlssModule)
			{
				string EnginePluginsDir = Path.Combine(EngineDirectory, "Plugins");
				if (Directory.Exists(EnginePluginsDir))
				{
					string[] FoundDescriptors = Directory.GetFiles(EnginePluginsDir, "DLSS.uplugin", SearchOption.AllDirectories);
					if (FoundDescriptors.Length > 0)
					{
						bEnableDlssModule = true;
						ResolvedDlssDescriptor = FoundDescriptors[0];
					}
				}
			}
		}

		if (bEnableDlssModule)
		{
			PrivateDependencyModuleNames.Add("DLSSBlueprint");
			PublicDefinitions.Add("WITH_TARETITHE_DLSS=1");
			System.Console.WriteLine("TareAndTithe.Build.cs: WITH_TARETITHE_DLSS=1 (" + ResolvedDlssDescriptor + ")");
		}
		else
		{
			PublicDefinitions.Add("WITH_TARETITHE_DLSS=0");
			System.Console.WriteLine("TareAndTithe.Build.cs: WITH_TARETITHE_DLSS=0");
		}
	}
}
