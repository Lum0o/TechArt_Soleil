// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Tech_Art_Soleil : ModuleRules
{
	public Tech_Art_Soleil(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Foliage", "Landscape" });
	}
}
