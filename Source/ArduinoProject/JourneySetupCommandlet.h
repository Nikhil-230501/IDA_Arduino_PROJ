#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "JourneySetupCommandlet.generated.h"

/** Repeatable editor tooling for inspecting and updating the journey Blueprint. */
UCLASS()
class UJourneySetupCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UJourneySetupCommandlet();
    virtual int32 Main(const FString& Params) override;

    /** Update reset behavior, remove unused nodes, and arrange using Slate node sizes. */
    UFUNCTION(BlueprintCallable, Category = "Editor|Journey")
    static bool TidyJourneyBlueprint(bool GestureControls = false);
};
