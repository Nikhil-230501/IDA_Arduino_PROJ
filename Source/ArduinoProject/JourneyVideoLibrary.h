#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "JourneyVideoLibrary.generated.h"

class UFileMediaSource;

UCLASS()
class ARDUINOPROJECT_API UJourneyVideoLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Read IDA_Videos.json at startup. Relative paths resolve beside the JSON.
     * Existing sources are retained if either configured video is invalid. */
    UFUNCTION(BlueprintCallable, Category = "Journey|Video Configuration")
    static bool LoadVideoConfiguration(UFileMediaSource*& IdleVideo, UFileMediaSource*& MainVideo);
};
