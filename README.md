# IDA Arduino Video Journey for Unreal Engine

IDA's proprietary Arduino service and interactive video project for Unreal Engine 5.8 on Windows.

## Interaction

- Arduino sends `0` plus a newline for a single tap: start the main video from idle, then pause/resume on subsequent taps.
- Arduino sends `1` plus a newline for a double tap: reset the whole journey to idle.
- No interaction for five minutes returns to idle. The timeout is editable in `BP_ArduinoController`.
- Keyboard testing: K/T/P simulate a single tap; R simulates a double tap.

## Replace the videos

Edit `IDA_Videos.json` or replace the files in `Videos/` while playback is stopped:

```json
{
  "idleVideo": "Videos/Video1.mp4",
  "mainVideo": "Videos/Video2.mp4"
}
```

Video1 is the looping idle video; Video2 is the interaction video. Restart Play/application after replacing them. No Blueprint editing or rebuild is needed.

## Open the project

Install Unreal Engine 5.8 and a compatible Visual Studio C++ toolchain. Clone with Git LFS so the actual Unreal assets and videos are downloaded:

```powershell
git lfs install
git clone https://github.com/Nikhil-230501/IDA_Arduino_PROJ.git
cd IDA_Arduino_PROJ
git lfs pull
```

Open `ArduinoProject.uproject`, allow native modules to rebuild, and open `ArduinoSampleMap`. See [SETUP.md](SETUP.md) for configuration, packaging details, and verification.

## Arduino connection

Set the COM port and matching firmware baud rate in `Config/IDA_Arduino.json`. Enable **Enable Arduino** on the controller for hardware input. Close Arduino Serial Monitor before connecting. Gesture detection and debounce run on the Arduino; a double tap must emit only one `1` command.

The included ArduinoSerial plugin provides Connect, Disconnect, Is Connected, Send String, and On Data Received Blueprint functionality using background serial reads.

This repository contains the editable project, native plugin source, Blueprints, and Git LFS video assets. Generated build folders are excluded. Public visibility does not change ownership or grant an open-source license.