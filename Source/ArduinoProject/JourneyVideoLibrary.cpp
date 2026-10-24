#include "JourneyVideoLibrary.h"
#include "FileMediaSource.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"

namespace JourneyVideos
{
    FString Root()
    {
#if WITH_EDITOR
        return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
#else
        return FPaths::ConvertRelativePathToFull(FPaths::RootDir());
#endif
    }

    // Packaging stages defaults beside the inner executable. On first launch,
    // seed the editable files beside the top-level launcher; never overwrite replacements.
    bool Seed(const FString& Root)
    {
#if !WITH_EDITOR
        const FString Staged = FPlatformProcess::BaseDir();
        const FString Json = Root / TEXT("IDA_Videos.json");
        if (!FPaths::FileExists(Json))
        {
            // Copy videos first so a failed setup can be retried next launch.
            TArray<FString> Files;
            IFileManager::Get().FindFilesRecursive(Files, *(Staged / TEXT("Videos")), TEXT("*"), true, false);
            for (const FString& File : Files)
            {
                FString Relative = File;
                FPaths::MakePathRelativeTo(Relative, *(Staged / TEXT("Videos/")));
                const FString Destination = Root / TEXT("Videos") / Relative;
                IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination), true);
                if (!FPaths::FileExists(Destination) && IFileManager::Get().Copy(*Destination, *File) != COPY_OK) return false;
            }
            if (IFileManager::Get().Copy(*Json, *(Staged / TEXT("IDA_Videos.json"))) != COPY_OK) return false;
        }
#endif
        return true;
    }
}

bool UJourneyVideoLibrary::LoadVideoConfiguration(UFileMediaSource*& IdleVideo, UFileMediaSource*& MainVideo)
{
    IdleVideo = nullptr;
    MainVideo = nullptr;
    const FString Root = JourneyVideos::Root();
    const FString Config = Root / TEXT("IDA_Videos.json");
    if (!JourneyVideos::Seed(Root))
    {
        UE_LOG(LogTemp, Error, TEXT("Video configuration: cannot seed loose video files beside the application. Check folder write access."));
        return false;
    }
    FString Text;
    TSharedPtr<FJsonObject> Json;
    if (!FFileHelper::LoadFileToString(Text, *Config) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) || !Json.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Video configuration: missing or invalid JSON: %s. Keeping Blueprint media defaults."), *Config);
        return false;
    }
    FString IdlePath, MainPath;
    if (!Json->TryGetStringField(TEXT("idleVideo"), IdlePath) || !Json->TryGetStringField(TEXT("mainVideo"), MainPath) ||
        IdlePath.TrimStartAndEnd().IsEmpty() || MainPath.TrimStartAndEnd().IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Video configuration requires non-empty idleVideo and mainVideo paths: %s"), *Config);
        return false;
    }
    auto Resolve = [&Root](const FString& Path)
    {
        FString Full = FPaths::IsRelative(Path) ? FPaths::ConvertRelativePathToFull(Root, Path) : Path;
        FPaths::NormalizeFilename(Full);
        FPaths::CollapseRelativeDirectories(Full);
        return Full;
    };
    IdlePath = Resolve(IdlePath);
    MainPath = Resolve(MainPath);
    if (!FPaths::FileExists(IdlePath) || !FPaths::FileExists(MainPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Video configuration file not found. Idle: %s; Main: %s. Keeping Blueprint media defaults."), *IdlePath, *MainPath);
        return false;
    }
    IdleVideo = NewObject<UFileMediaSource>();
    MainVideo = NewObject<UFileMediaSource>();
    IdleVideo->SetFilePath(IdlePath);
    MainVideo->SetFilePath(MainPath);
    UE_LOG(LogTemp, Display, TEXT("Journey videos loaded from %s | Idle: %s | Main: %s"), *Config, *IdlePath, *MainPath);
    return true;
}
