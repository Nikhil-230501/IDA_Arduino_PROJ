#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "ArduinoJsonComponent.generated.h"


UCLASS(ClassGroup = (Arduino), meta = (BlueprintSpawnableComponent))
class ARDUINOSERIAL_API UArduinoJsonComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    UArduinoJsonComponent();


    // =========================================================
    // CONFIGURATION
    // =========================================================

    UFUNCTION(BlueprintCallable, Category = "Arduino|JSON")
    bool LoadConfig();

    UFUNCTION(BlueprintCallable, Category = "Arduino|JSON")
    bool SaveConfig();


    // =========================================================
    // GET
    // =========================================================

    UFUNCTION(BlueprintPure, Category = "Arduino|JSON")
    FString GetPort() const;

    UFUNCTION(BlueprintPure, Category = "Arduino|JSON")
    int32 GetBaudRate() const;


    // =========================================================
    // SET
    // =========================================================

    UFUNCTION(BlueprintCallable, Category = "Arduino|JSON")
    void SetPort(FString NewPort);

    UFUNCTION(BlueprintCallable, Category = "Arduino|JSON")
    void SetBaudRate(int32 NewBaudRate);


    // =========================================================
    // SHARED HELPERS
    // =========================================================

    // Packaged build: <Build>/Windows/IDA_Arduino.json (next to ArduinoProject.exe)
    // Editor:         <Project>/Config/IDA_Arduino.json
    UFUNCTION(BlueprintPure, Category = "Arduino|JSON")
    static FString GetConfigFilePath();

    // Reads port / baudRate from IDA_Arduino.json. Returns false if the file is missing or invalid.
    static bool ReadConfigFile(FString& OutPort, int32& OutBaudRate);


protected:

    virtual void BeginPlay() override;


private:

    FString Port;

    int32 BaudRate;
};