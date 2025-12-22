// Copyright (C) 2025 Breakloop, LLC - All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "LiveLinkTypes.h"
#include "Subsystems/EngineSubsystem.h"
#include "BridgeSubsystem.generated.h"

class FLiveLinkClient;
struct ILiveLinkProvider;

UCLASS()
class MASQUELIVELINKBRIDGE_API UBridgeSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

	struct FBridgeSubjectContext
	{
		TArray<FLiveLinkSubjectKey> Sources;
		FLiveLinkStaticDataStruct StaticData;
		FLiveLinkWorldTime LastSent;
	};

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

private:

	UFUNCTION()
	void OnLiveLinkTicked();

	void UpdateBridgeSubject(FLiveLinkClient& Client, FName Subject, FBridgeSubjectContext& Context) const;
	
	UFUNCTION()
	void OnLiveLinkSubjectAdded(FLiveLinkSubjectKey SubjectKey);

	UFUNCTION()
	void OnLiveLinkSubjectRemoved(FLiveLinkSubjectKey SubjectKey);

	TSharedPtr<ILiveLinkProvider> LiveLinkProvider;
	TMap<FName, FBridgeSubjectContext> Subjects;
};
