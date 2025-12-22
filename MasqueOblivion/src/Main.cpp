// Copyright (C) 2025 Breakloop, LLC - All rights reserved

#include <obse64/PluginAPI.h>
#include <obse64/UnrealTypes.h>
#include <obse64_common/Relocation.h>
#include <obse64_common/SafeWrite.h>
#include <obse64_common/obse64_version.h>
#include <windows.h>

class FSharedReferencer
{
public:
    uintptr_t ReferenceController = (uintptr_t)nullptr;
}; 

class TSharedPtr
{
public:
    uintptr_t ObjectPointer = (uintptr_t)nullptr;
    FSharedReferencer Referencer;
};

RelocAddr<void> UdpMessagingModuleInitializeBridge(0x5488380);
RelocAddr<void> UdpMessagingModuleHandleAppPreExit(0x5487120);
RelocAddr<void> StartUdpMessaging(0x547A340);
RelocAddr<void> RestartUdpMessagingServices(0x547A590);

typedef uintptr_t(*_LoadModuleChecked)(uintptr_t ModuleManager, FName ModuleName);
RelocAddr<_LoadModuleChecked> LoadModuleChecked(0xF11420);

typedef TSharedPtr* (*_CreateMessagingBus)(uintptr_t MessagingModule, TSharedPtr* RecipientAuthorizer);
RelocAddr<_CreateMessagingBus> CreateMessagingBus(0x1B8A190);
RelocAddr<void> NewIntrusiveReferenceController_IAuthorizeMessageRecipients(0x1b8c290);

typedef void (*_MessageBusShutdown)(uintptr_t MessageBus);
RelocAddr<_MessageBusShutdown> MessageBusShutdown(0x1B98A30);

TSharedPtr* DefaultMessagingBus = nullptr;
TSharedPtr* RecipientAuthorizer = nullptr;
void* HandleAppPreExitTrampoline;

uintptr_t GetOrCreateDefaultMessagingBus(uintptr_t MessagingModule)
{
    uintptr_t ModuleDefaultMessageBusPtr = MessagingModule + 0x8;

    if (DefaultMessagingBus == nullptr)
    {
        RecipientAuthorizer = new TSharedPtr();
        DefaultMessagingBus = CreateMessagingBus(MessagingModule, RecipientAuthorizer);
        memcpy((void*)ModuleDefaultMessageBusPtr, DefaultMessagingBus, sizeof(TSharedPtr));
    }

    return ModuleDefaultMessageBusPtr;
}

uintptr_t LoadMessagingModuleAndReturnDefaultBus(uintptr_t ModuleManager, FName MessagingModuleName)
{
    uintptr_t MessagingModule = LoadModuleChecked(ModuleManager, MessagingModuleName);
    return GetOrCreateDefaultMessagingBus(MessagingModule);
}

void ShutDownDefaultMessageBus()
{
    MessageBusShutdown(DefaultMessagingBus->ObjectPointer);
    ((void(*)())HandleAppPreExitTrampoline)();
}

void PatchDefaultMessageBusRetrieval()
{
    // This patch is a bit more involved. Unreal shipping builds strip out the creation
    // of a default message bus through the messaging module, which the UDP messaging depends on existing.
    // This patch here hijacks the moment that FUdpMessagingModule::InitializeBridge is creating a 
    // FMessageBridgeBuilder. This builder will fail by default because it receives no bus in a shipping build, 
    // so we replace the moment the default is retrieved from the messaging module and instead provide a new bus of our own

    uint8_t Bytes[23];

    //MOV RAX, (Abs. function addr)
    Bytes[0] = 0x48;
    Bytes[1] = 0xB8;

    _LoadModuleChecked ModifiedLoadModule = &LoadMessagingModuleAndReturnDefaultBus;
    memcpy(Bytes + 2, &ModifiedLoadModule, sizeof(uintptr_t));

    //CALL RAX
    Bytes[10] = 0xFF;
    Bytes[11] = 0xD0;
    
    memset(Bytes + 12, 0x90, 23-12); // NOP, just to pad the end to be compliant with the ASM we are replacing    
    safeWriteBuf(UdpMessagingModuleInitializeBridge.getUIntPtr() + 0x615, &Bytes, 22 * sizeof(uint8_t));

    // Force IAuthorizeMessageRecipients shared pointer copying to always believe it's been handed a null shared pointer
    // I'm opting to go down this path because the default message bus we're missing is given a null shared pointer here
    // but handing a null pointer of my own creation is leading to it still trying to increment the reference count

    //XOR EAX, EAX <- Set RAX to 0
    Bytes[0] = 0x31;
    Bytes[1] = 0xC0;
    memset(Bytes + 2, 0x90, 21); // NOP, for padding replaced instructions

    safeWriteBuf(NewIntrusiveReferenceController_IAuthorizeMessageRecipients.getUIntPtr() + 0x78, &Bytes, 3 * sizeof(uint8_t));
    safeWriteBuf(NewIntrusiveReferenceController_IAuthorizeMessageRecipients.getUIntPtr() + 0x7F, &Bytes, 4 * sizeof(uint8_t));
}

void PatchDefaultMessageBusShutdownIntoAppPreExit()
{
    // Since the default messagebus is stripped in shipping builds, the pre-exit cleanup is also stripped
    // As a result, the messagebus thread will continue to function during game shutdown and eventually lead to a crash
    // To counteract that, I place a trampoline into the UdpMessagingModule's HandleAppPreExit behavior that manually
    // shuts down the default messagebus and then returns the regular proceedings

    const size_t OverwrittenBytes = 14;
    const size_t NumJumpBytes = 12;
    uint8_t Bytes[NumJumpBytes];

    // Prepare trampoline (since we'll be overwriting the first 14 bytes of the function)

    HandleAppPreExitTrampoline = { VirtualAlloc(nullptr, OverwrittenBytes + NumJumpBytes,
        MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE) };

    memcpy(HandleAppPreExitTrampoline, (void *)UdpMessagingModuleHandleAppPreExit.getUIntPtr(), OverwrittenBytes * sizeof(uint8_t));

    //MOV RAX, (Abs. function addr)
    Bytes[0] = 0x48;
    Bytes[1] = 0xB8;

    uintptr_t TrampolineReturnAddress = UdpMessagingModuleHandleAppPreExit.getUIntPtr() + OverwrittenBytes;
    memcpy(Bytes + 2, &TrampolineReturnAddress, sizeof(uintptr_t));

    //JMP RAX
    Bytes[10] = 0xFF;
    Bytes[11] = 0xE0;

    memcpy((uint8_t *)HandleAppPreExitTrampoline + OverwrittenBytes, &Bytes, NumJumpBytes * sizeof(uint8_t));

    // Prepare hook to tear down default message bus on PreExit

    //MOV RAX, (Abs. function addr)
    Bytes[0] = 0x48;
    Bytes[1] = 0xB8;

    void(*ShutDownPointer)() = &ShutDownDefaultMessageBus; // This will call the trampoline we've created above
    memcpy(Bytes + 2, &ShutDownPointer, sizeof(uintptr_t));

    //JMP RAX
    Bytes[10] = 0xFF;
    Bytes[11] = 0xE0;

    safeWriteBuf(UdpMessagingModuleHandleAppPreExit.getUIntPtr(), &Bytes, NumJumpBytes * sizeof(uint8_t));
}

void PatchUdpMessagingNotEnabledJumps()
{
    // There are a few explicit checks in code that will prevent start and restarting the UDP messaging module in shipping builds
    // We just no-op over those jumps here

    const uintptr_t UdpMessagingSupportNotEnabledJump = StartUdpMessaging.getUIntPtr() + 0x17;
    const uintptr_t UdpMessagingTransportNotEnabledJump = RestartUdpMessagingServices.getUIntPtr() + 0x3B;

    // Patch the UDP Messaging guards into no-ops
    constexpr int NoOpCount = 6;
    uint8_t NoOpBuffer[NoOpCount];
    for (size_t i = 0; i < NoOpCount; i++)
    {
        NoOpBuffer[i] = 0x90;
    }

    safeWriteBuf(UdpMessagingSupportNotEnabledJump, &NoOpBuffer, 6 * sizeof(uint8_t));
    safeWriteBuf(UdpMessagingTransportNotEnabledJump, &NoOpBuffer, 2 * sizeof(uint8_t));
}

// --- OBSE Plugin Exports ---
extern "C" {
    __declspec(dllexport) OBSEPluginVersionData OBSEPlugin_Version = {
        OBSEPluginVersionData::kVersion,
        1, // Plugin version
        "Masque",
        "Jake Alaimo", // Author
        OBSEPluginVersionData::kAddressIndependence_Signatures,
        OBSEPluginVersionData::kStructureIndependence_InitialLayout,
        {RUNTIME_VERSION_1_512_105, 0}, // Compatible Oblivion.exe version (usually { RUNTIME_VERSION_1_2_416, 0 } or left {0} for any)
        0,   // OBSE major version requirement (0 for any)
        0, 0, {0} // Reserved
    };

    __declspec(dllexport) bool OBSEPlugin_Load(const OBSEInterface* Obse)
    {
        PatchDefaultMessageBusRetrieval();
        PatchDefaultMessageBusShutdownIntoAppPreExit();
        PatchUdpMessagingNotEnabledJumps();

        return true;
    }
}