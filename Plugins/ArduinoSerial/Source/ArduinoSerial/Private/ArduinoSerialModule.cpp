#include "ArduinoSerial.h"
#include "ArduinoJsonComponent.h"


class FArduinoSerialModule : public IModuleInterface
{
public:

    virtual void StartupModule() override
    {
#if !WITH_EDITOR
        // Create IDA_Arduino.json next to the packaged exe as soon as the game starts,
        // so it is there to edit even before any Arduino actor runs.
        UArduinoJsonComponent::EnsureConfigFile();
#endif
    }
};


IMPLEMENT_MODULE(FArduinoSerialModule, ArduinoSerial)
