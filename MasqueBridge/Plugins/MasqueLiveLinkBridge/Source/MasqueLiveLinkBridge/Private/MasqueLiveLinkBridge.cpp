// Copyright (C) 2025 Breakloop, LLC - All rights reserved

#include "MasqueLiveLinkBridge.h"
#include "Features/IModularFeatures.h"

#define LOCTEXT_NAMESPACE "FMasqueLiveLinkBridgeModule"

DEFINE_LOG_CATEGORY(LogMasque);

void FMasqueLiveLinkBridgeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FMasqueLiveLinkBridgeModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMasqueLiveLinkBridgeModule, MasqueLiveLinkBridge)