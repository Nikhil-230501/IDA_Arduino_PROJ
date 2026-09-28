#include "ArduinoJsonComponent.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"

#if WITH_EDITOR
#include "Interfaces/IPluginManager.h"
#endif


UArduinoJsonComponent::UArduinoJsonComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    Port = TEXT("COM3");

    BaudRate = 9600;
}


void UArduinoJsonComponent::BeginPlay()
{
    Super::BeginPlay();

    LoadConfig();
}


FString UArduinoJsonComponent::GetConfigFilePath()
{
#if WITH_EDITOR

    const FString ProjectPath =
        FPaths::Combine(
            FPaths::ProjectConfigDir(),
            TEXT("IDA_Arduino.json")
        );


    // New project: seed Config/IDA_Arduino.json from the plugin's default.
    if (!FPaths::FileExists(ProjectPath))
    {
        const TSharedPtr<IPlugin> Plugin =
            IPluginManager::Get().FindPlugin(TEXT("ArduinoSerial"));

        if (Plugin.IsValid())
        {
            const FString DefaultPath =
                FPaths::Combine(
                    Plugin->GetBaseDir(),
                    TEXT("Resources"),
                    TEXT("IDA_Arduino.json")
                );

            FPlatformFileManager::Get().GetPlatformFile().CopyFile(
                *ProjectPath,
                *DefaultPath
            );
        }
    }


    return ProjectPath;

#else

    // RootDir() is the packaged build folder that contains
    // ArduinoProject.exe, Engine/ and ArduinoProject/.
    const FString RootPath =
        FPaths::ConvertRelativePathToFull(
            FPaths::Combine(
                FPaths::RootDir(),
                TEXT("IDA_Arduino.json")
            )
        );


    // First launch: seed the root file from the copy staged
    // next to the real exe (Binaries/Win64) by Build.cs.
    IPlatformFile& PlatformFile =
        FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.FileExists(*RootPath))
    {
        const FString StagedPath =
            FPaths::Combine(
                FPlatformProcess::BaseDir(),
                TEXT("IDA_Arduino.json")
            );

        if (PlatformFile.FileExists(*StagedPath))
        {
            PlatformFile.CopyFile(*RootPath, *StagedPath);
        }
    }


    return RootPath;

#endif
}


bool UArduinoJsonComponent::ReadConfigFile(
    FString& OutPort,
    int32& OutBaudRate
)
{
    FString JsonString;

    const FString FilePath = GetConfigFilePath();


    if (!FFileHelper::LoadFileToString(
        JsonString,
        *FilePath
    ))
    {
        return false;
    }


    TSharedPtr<FJsonObject> JsonObject;

    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(
            JsonString
        );


    if (!FJsonSerializer::Deserialize(
        Reader,
        JsonObject
    ) || !JsonObject.IsValid())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Failed to parse %s"),
            *FilePath
        );

        return false;
    }


    FString LoadedPort;

    if (JsonObject->TryGetStringField(
        TEXT("port"),
        LoadedPort
    ))
    {
        OutPort = LoadedPort;
    }


    double LoadedBaudRate = 0;

    if (JsonObject->TryGetNumberField(
        TEXT("baudRate"),
        LoadedBaudRate
    ))
    {
        OutBaudRate =
            static_cast<int32>(
                LoadedBaudRate
                );
    }


    UE_LOG(
        LogTemp,
        Display,
        TEXT("IDA Arduino Config Loaded from %s: %s @ %d"),
        *FilePath,
        *OutPort,
        OutBaudRate
    );


    return true;
}


bool UArduinoJsonComponent::LoadConfig()
{
    // ---------------------------------------------------------
    // If the file doesn't exist, create the default one.
    // ---------------------------------------------------------

    if (!FPaths::FileExists(GetConfigFilePath()))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("IDA_Arduino.json not found. Creating default configuration.")
        );

        return SaveConfig();
    }


    return ReadConfigFile(
        Port,
        BaudRate
    );
}


bool UArduinoJsonComponent::SaveConfig()
{
    TSharedPtr<FJsonObject> JsonObject =
        MakeShared<FJsonObject>();


    JsonObject->SetStringField(
        TEXT("port"),
        Port
    );


    JsonObject->SetNumberField(
        TEXT("baudRate"),
        BaudRate
    );


    FString OutputString;


    TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(
            &OutputString
        );


    if (!FJsonSerializer::Serialize(
        JsonObject.ToSharedRef(),
        Writer
    ))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Failed to serialize IDA Arduino configuration")
        );

        return false;
    }


    const FString FilePath =
        GetConfigFilePath();


    if (!FFileHelper::SaveStringToFile(
        OutputString,
        *FilePath
    ))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Failed to save IDA_Arduino.json")
        );

        return false;
    }


    UE_LOG(
        LogTemp,
        Display,
        TEXT("IDA_Arduino.json saved")
    );


    return true;
}


FString UArduinoJsonComponent::GetPort() const
{
    return Port;
}


int32 UArduinoJsonComponent::GetBaudRate() const
{
    return BaudRate;
}


void UArduinoJsonComponent::SetPort(
    FString NewPort
)
{
    Port = NewPort;
}


void UArduinoJsonComponent::SetBaudRate(
    int32 NewBaudRate
)
{
    BaudRate = NewBaudRate;
}