#include "ArduinoSerialComponent.h"
#include "ArduinoJsonComponent.h"

#include "Async/Async.h"
#include "HAL/PlatformProcess.h"

#include <atomic>

#if PLATFORM_WINDOWS

#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"

#endif


// =============================================================
// PRIVATE SERIAL READER
// =============================================================

#if PLATFORM_WINDOWS

class FSerialReader : public FRunnable
{
public:

    FSerialReader(
        UArduinoSerialComponent* InOwner,
        HANDLE InHandle
    )
        : Owner(InOwner)
        , Handle(InHandle)
        , bStop(false)
    {
    }


    virtual ~FSerialReader()
    {
    }


    virtual uint32 Run() override
    {
        uint8 Buffer[256];


        while (!bStop.load())
        {
            // -------------------------------------------------
            // Check whether data is available.
            // We DO NOT block waiting for ReadFile().
            // -------------------------------------------------

            COMSTAT Status = {};
            DWORD Errors = 0;


            if (!ClearCommError(
                Handle,
                &Errors,
                &Status
            ))
            {
                break;
            }


            if (Status.cbInQue == 0)
            {
                FPlatformProcess::Sleep(0.005f);
                continue;
            }


            DWORD BytesToRead =
                FMath::Min<DWORD>(
                    Status.cbInQue,
                    sizeof(Buffer)
                );


            DWORD BytesRead = 0;


            BOOL Success =
                ReadFile(
                    Handle,
                    Buffer,
                    BytesToRead,
                    &BytesRead,
                    nullptr
                );


            if (!Success)
            {
                break;
            }


            if (BytesRead == 0)
            {
                continue;
            }


            // -------------------------------------------------
            // Convert incoming bytes to FString
            // -------------------------------------------------

            FUTF8ToTCHAR Converter(
                reinterpret_cast<const ANSICHAR*>(Buffer),
                BytesRead
            );


            FString Incoming(
                Converter.Get(),
                Converter.Length()
            );


            ReceiveBuffer += Incoming;


            // -------------------------------------------------
            // Find complete messages.
            //
            // Arduino:
            //
            // Serial.println("2");
            //
            // sends:
            //
            // 2\r\n
            // -------------------------------------------------

            while (true)
            {
                int32 NewLineIndex =
                    ReceiveBuffer.Find(
                        TEXT("\n")
                    );


                if (NewLineIndex == INDEX_NONE)
                {
                    break;
                }


                FString Message =
                    ReceiveBuffer.Left(
                        NewLineIndex
                    );


                ReceiveBuffer =
                    ReceiveBuffer.Mid(
                        NewLineIndex + 1
                    );


                // Remove \r
                Message.TrimEndInline();


                if (Message.IsEmpty())
                {
                    continue;
                }


                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Arduino RECEIVE: %s"),
                    *Message
                );


                // -------------------------------------------------
                // Copy weak reference.
                //
                // We must NOT directly execute Blueprint from
                // this background thread.
                // -------------------------------------------------

                TWeakObjectPtr<UArduinoSerialComponent>
                    WeakOwner(Owner);


                AsyncTask(
                    ENamedThreads::GameThread,
                    [
                        WeakOwner,
                        Message
                    ]()
                    {
                        if (!WeakOwner.IsValid())
                        {
                            return;
                        }


                        WeakOwner
                            ->OnDataReceived
                            .Broadcast(Message);
                    }
                );
            }
        }


        return 0;
    }


    virtual void Stop() override
    {
        bStop.store(true);
    }


private:

    UArduinoSerialComponent* Owner;

    HANDLE Handle;

    std::atomic<bool> bStop;

    FString ReceiveBuffer;
};

#endif


// =============================================================
// CONSTRUCTOR
// =============================================================

UArduinoSerialComponent::UArduinoSerialComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    SerialHandle = nullptr;

    ReaderThread = nullptr;

    SerialReader = nullptr;
}


// =============================================================
// BEGIN PLAY
// =============================================================

void UArduinoSerialComponent::BeginPlay()
{
    Super::BeginPlay();
}


// =============================================================
// END PLAY
// =============================================================

void UArduinoSerialComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    Disconnect();

    Super::EndPlay(EndPlayReason);
}


// =============================================================
// CONNECT
// =============================================================

bool UArduinoSerialComponent::Connect(
    FString PortName,
    int32 BaudRate
)
{
#if PLATFORM_WINDOWS

    if (SerialHandle != nullptr)
    {
        return true;
    }


    if (bUseConfigFile)
    {
        UArduinoJsonComponent::ReadConfigFile(
            PortName,
            BaudRate
        );
    }


    FString DeviceName =
        FString::Printf(
            TEXT("\\\\.\\%s"),
            *PortName
        );


    HANDLE Handle =
        CreateFile(
            *DeviceName,
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );


    if (Handle == INVALID_HANDLE_VALUE)
    {
        const DWORD Error = GetLastError();

        UE_LOG(
            LogTemp,
            Error,
            TEXT("Failed to open Arduino port: %s (Windows error %u%s)"),
            *PortName,
            Error,
            Error == ERROR_ACCESS_DENIED
                ? TEXT(" - port is in use by another program, e.g. the editor or Arduino Serial Monitor")
                : Error == ERROR_FILE_NOT_FOUND
                    ? TEXT(" - no device on this port, check IDA_Arduino.json")
                    : TEXT("")
        );

        return false;
    }


    // =========================================================
    // SERIAL SETTINGS
    // =========================================================

    DCB Settings = {};

    Settings.DCBlength = sizeof(DCB);


    if (!GetCommState(
        Handle,
        &Settings
    ))
    {
        CloseHandle(Handle);

        return false;
    }


    Settings.BaudRate = BaudRate;
    Settings.ByteSize = 8;
    Settings.StopBits = ONESTOPBIT;
    Settings.Parity = NOPARITY;


    if (!SetCommState(
        Handle,
        &Settings
    ))
    {
        CloseHandle(Handle);

        return false;
    }


    // =========================================================
    // NON-BLOCKING READ
    // =========================================================

    COMMTIMEOUTS Timeouts = {};

    Timeouts.ReadIntervalTimeout = MAXDWORD;

    Timeouts.ReadTotalTimeoutConstant = 0;

    Timeouts.ReadTotalTimeoutMultiplier = 0;

    Timeouts.WriteTotalTimeoutConstant = 50;

    Timeouts.WriteTotalTimeoutMultiplier = 10;


    if (!SetCommTimeouts(
        Handle,
        &Timeouts
    ))
    {
        CloseHandle(Handle);

        return false;
    }


    // =========================================================
    // STORE HANDLE
    // =========================================================

    SerialHandle = Handle;


    UE_LOG(
        LogTemp,
        Display,
        TEXT("Arduino connected on %s @ %d"),
        *PortName,
        BaudRate
    );


    // =========================================================
    // CREATE READER
    // =========================================================

    SerialReader =
        new FSerialReader(
            this,
            Handle
        );


    ReaderThread =
        FRunnableThread::Create(
            SerialReader,
            TEXT("ArduinoSerialReader"),
            0,
            TPri_Normal
        );


    if (ReaderThread == nullptr)
    {
        delete SerialReader;

        SerialReader = nullptr;

        CloseHandle(Handle);

        SerialHandle = nullptr;

        UE_LOG(
            LogTemp,
            Error,
            TEXT("Failed to create Arduino reader thread")
        );

        return false;
    }


    return true;


#else

    return false;

#endif
}


// =============================================================
// DISCONNECT
// =============================================================

void UArduinoSerialComponent::Disconnect()
{
#if PLATFORM_WINDOWS

    // ---------------------------------------------------------
    // Stop reader
    // ---------------------------------------------------------

    if (SerialReader != nullptr)
    {
        SerialReader->Stop();
    }


    // ---------------------------------------------------------
    // Wait for reader to finish
    // ---------------------------------------------------------

    if (ReaderThread != nullptr)
    {
        ReaderThread->WaitForCompletion();

        delete ReaderThread;

        ReaderThread = nullptr;
    }


    // ---------------------------------------------------------
    // Delete reader
    // ---------------------------------------------------------

    if (SerialReader != nullptr)
    {
        delete SerialReader;

        SerialReader = nullptr;
    }


    // ---------------------------------------------------------
    // Close COM port
    // ---------------------------------------------------------

    if (SerialHandle != nullptr)
    {
        HANDLE Handle =
            static_cast<HANDLE>(
                SerialHandle
                );


        CloseHandle(Handle);

        SerialHandle = nullptr;


        UE_LOG(
            LogTemp,
            Display,
            TEXT("Arduino disconnected")
        );
    }

#endif
}


// =============================================================
// SEND STRING
// =============================================================

bool UArduinoSerialComponent::SendString(
    FString Data
)
{
#if PLATFORM_WINDOWS

    if (SerialHandle == nullptr)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Arduino is not connected")
        );

        return false;
    }


    FString Message =
        Data + TEXT("\n");


    FTCHARToUTF8 UTF8Data(
        *Message
    );


    HANDLE Handle =
        static_cast<HANDLE>(
            SerialHandle
            );


    DWORD BytesWritten = 0;


    BOOL Success =
        WriteFile(
            Handle,
            UTF8Data.Get(),
            UTF8Data.Length(),
            &BytesWritten,
            nullptr
        );


    if (!Success)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Failed to send Arduino data")
        );

        return false;
    }


    UE_LOG(
        LogTemp,
        Display,
        TEXT("Arduino SEND: %s"),
        *Data
    );


    return true;


#else

    return false;

#endif
}


// =============================================================
// IS CONNECTED
// =============================================================

bool UArduinoSerialComponent::IsConnected() const
{
    return SerialHandle != nullptr;
}