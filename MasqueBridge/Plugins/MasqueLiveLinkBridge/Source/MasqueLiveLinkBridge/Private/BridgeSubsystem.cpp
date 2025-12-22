// Copyright (C) 2025 Breakloop, LLC - All rights reserved

#include "BridgeSubsystem.h"
#include "LiveLinkClient.h"
#include "LiveLinkProvider.h"
#include "LiveLinkSubjectSettings.h"
#include "MasqueLiveLinkBridge.h"
#include "Features/IModularFeatures.h"

/**
 * @returns The active LiveLink Client, which is responsible for registering and handling sources and subjects
 */
FLiveLinkClient& GetLiveLinkClient()
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	return ModularFeatures.GetModularFeature<FLiveLinkClient>(ILiveLinkClient::ModularFeatureName);
}

void UBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LiveLinkProvider = ILiveLinkProvider::CreateLiveLinkProvider(TEXT("Masque"));

	FLiveLinkClient& Client = GetLiveLinkClient();
	Client.OnLiveLinkTicked().AddUObject(this, &ThisClass::OnLiveLinkTicked);
	Client.OnLiveLinkSubjectAdded().AddUObject(this, &ThisClass::OnLiveLinkSubjectAdded);
	Client.OnLiveLinkSubjectRemoved().AddUObject(this, &ThisClass::OnLiveLinkSubjectRemoved);

	UE_LOG(LogMasque, Log, TEXT("Bridge subsystem initialized"));
}

void UBridgeSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UBridgeSubsystem::OnLiveLinkTicked()
{
	FLiveLinkClient& Client = GetLiveLinkClient();

	for (TTuple<FName, FBridgeSubjectContext>& Mapping : Subjects)
	{
		UpdateBridgeSubject(Client, Mapping.Key, Mapping.Value);
	}
}

void UBridgeSubsystem::UpdateBridgeSubject(FLiveLinkClient& Client, FName Subject, FBridgeSubjectContext& Context) const
{
	// Only one subject of a given name should be active, even if multiple sources are connected
	const FLiveLinkSubjectKey* ActiveSource = Context.Sources.FindByPredicate([&Client](const FLiveLinkSubjectKey& Subject)
	{
		return Client.IsSubjectValid(Subject) && Client.IsSubjectEnabled(Subject.SubjectName);
	});

	if (!ActiveSource)
	{
		// Invalid subjects may represent a data interruption, which we should wait out
		// Disabled subjects should not influence the receiving application
		// Subjects may also be removed before that status is communicated via OnLiveLinkSubjectRemoved, as
		// that can occur on separate threads. In those cases, early out here and await the formal message
		UE_LOG(LogMasque, Verbose, TEXT("Awaiting subject reconnection or removal { Subject: %s }"),
			*Subject.ToString());
		return;
	}

	auto Settings = Cast<ULiveLinkSubjectSettings>(Client.GetSubjectSettings(*ActiveSource));
	
	FLiveLinkSubjectFrameData Frame;
	Client.EvaluateFrame_AnyThread(Subject, Settings->Role, Frame);

	bool HasValidData = Frame.StaticData.GetBaseData() != nullptr
		&& Frame.FrameData.GetBaseData() != nullptr;

	if (!HasValidData)
	{
		// It's possible for a subject to be added before it has a frame to evaluate
		// This is true for most sources, actually. We don't need to do anything but wait for a frame to come in
		UE_LOG(LogMasque, Verbose, TEXT("Awaiting complete data { Subject: %s }"),
			*Subject.ToString());
		return;
	}

	auto FrameData = Frame.FrameData.GetBaseData();
	if (FrameData->WorldTime.GetSourceTime() != Context.LastSent.GetSourceTime())
	{
		if (Context.StaticData != Frame.StaticData)
		{
			Context.StaticData = FLiveLinkStaticDataStruct(Frame.StaticData.GetStruct());
			Context.StaticData.InitializeWith(Frame.StaticData);				
			LiveLinkProvider->UpdateSubjectStaticData(Subject, Settings->Role, MoveTemp(Frame.StaticData));

			UE_LOG(LogMasque, Log, TEXT("Updated static data { Subject: %s, Source: %s, WorldTime: %f, LastSent: %f }"),
				*Subject.ToString(), *ActiveSource->Source.ToString(),
				FrameData->WorldTime.GetSourceTime(), Context.LastSent.GetSourceTime());
		}

		LiveLinkProvider->UpdateSubjectFrameData(Subject, MoveTemp(Frame.FrameData));

		UE_LOG(LogMasque, VeryVerbose, TEXT("Updated frame data { Subject: %s, Source: %s, WorldTime: %f, LastSent: %f }"),
			*Subject.ToString(), *ActiveSource->Source.ToString(),
			FrameData->WorldTime.GetSourceTime(), Context.LastSent.GetSourceTime());

		Context.LastSent = FrameData->WorldTime;
	}
}

void UBridgeSubsystem::OnLiveLinkSubjectAdded(FLiveLinkSubjectKey SubjectKey)
{
	// The function of the bridge is to treat new subjects as sources, as a kind of pass-through
	// that squashes every integration down to a MessageBus source
	// As a result, a single subject name may actually by filled by more than one source, though only one may be active at a time
	FBridgeSubjectContext& Context = Subjects.FindOrAdd(SubjectKey.SubjectName);
	Context.Sources.Add(SubjectKey);

	UE_LOG(LogMasque, Log, TEXT("Source added { Subject: %s, Source: %s }"),
		*SubjectKey.SubjectName.ToString(), *SubjectKey.Source.ToString());
}

void UBridgeSubsystem::OnLiveLinkSubjectRemoved(FLiveLinkSubjectKey SubjectKey)
{
	if (!Subjects.Contains(SubjectKey.SubjectName))
	{
		return;
	}

	FBridgeSubjectContext& Context = Subjects.FindChecked(SubjectKey.SubjectName);
	Context.Sources.Remove(SubjectKey);

	UE_LOG(LogMasque, Log, TEXT("Source removed { Subject: %s, Source: %s }"),
		*SubjectKey.SubjectName.ToString(), *SubjectKey.Source.ToString());

	if (Context.Sources.IsEmpty())
	{		
		UE_LOG(LogMasque, Log, TEXT("All sources removed, removing Masque subject { Subject: %s }"),
			*SubjectKey.SubjectName.ToString());

		LiveLinkProvider->RemoveSubject(SubjectKey.SubjectName);
		Subjects.Remove(SubjectKey.SubjectName);
	}
}
