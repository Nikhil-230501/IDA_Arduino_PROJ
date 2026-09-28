#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArduinoSerialComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FArduinoDataReceived,
    const FString&,
    Data
);


UCLASS(ClassGroup = (Arduino), meta = (BlueprintSpawnableComponent))
class ARDUINOSERIAL_API UArduinoSerialComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    UArduinoSerialComponent();


    // When true, Connect() uses the port / baudRate from IDA_Arduino.json
    // (if the file exists) instead of the values passed in.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arduino|Connection")
    bool bUseConfigFile = true;


    // =========================================================
    // CONNECTION
    // =========================================================

    UFUNCTION(BlueprintCallable, Category = "Arduino|Connection")
    bool Connect(FString PortName, int32 BaudRate);

    UFUNCTION(BlueprintCallable, Category = "Arduino|Connection")
    void Disconnect();

    UFUNCTION(BlueprintPure, Category = "Arduino|Connection")
    bool IsConnected() const;


    // =========================================================
    // SEND
    // =========================================================

    UFUNCTION(BlueprintCallable, Category = "Arduino|Send")
    bool SendString(FString Data);


    // =========================================================
    // RECEIVE
    // =========================================================

    UPROPERTY(BlueprintAssignable, Category = "Arduino|Receive")
    FArduinoDataReceived OnDataReceived;


protected:

    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;


private:

    void* SerialHandle;

    class FRunnableThread* ReaderThread;

    class FSerialReader* SerialReader;
};