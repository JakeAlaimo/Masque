// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MasqueBridge : ModuleRules
{
	public MasqueBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        bAddDefaultIncludePaths = true;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

        PrivateDependencyModuleNames.AddRange(new string[] { "LiveLink", "LiveLinkInterface", "LiveLinkMessageBusFramework" });

        PublicDefinitions.Add("ALLOW_UDP_MESSAGING_SHIPPING=1");
        PublicDefinitions.Add("PLATFORM_SUPPORTS_MESSAGEBUS=1");

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
