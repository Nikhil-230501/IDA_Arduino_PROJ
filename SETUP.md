# Opening and editing this project

This repository contains the editable Unreal Engine project, Blueprints, Arduino Serial plugin source, and both video files. It is not a packaged application.

## Requirements

- Windows and Unreal Engine 5.8.2.
- Git with Git LFS installed, or GitHub Desktop with Git LFS support.
- Access to this private repository after accepting the owner's collaborator invitation.
- A compatible Visual Studio C++ toolchain for rebuilding the native project and Arduino Serial plugin. Generated binaries are intentionally not committed.

## Download

Clone the repository rather than downloading a ZIP, so Git LFS downloads the actual Unreal assets and videos.

```powershell
git lfs install
git clone https://github.com/Nikhil-230501/IDA_Arduino_PROJ.git
cd IDA_Arduino_PROJ
git lfs pull
```

Open `ArduinoProject.uproject` with Unreal Engine 5.8.2. If Unreal requests a module rebuild, allow it with the compatible C++ toolchain installed. The interaction logic and widget remain editable in Blueprints.

## Videos

- Idle source: `Content/Movies/MS_Idle.uasset` -> `./Movies/Video1.mp4`.
- Interaction source: `Content/Movies/MS_Interaction.uasset` -> `./Movies/Video2.mp4`.
- Both MP4 files are included through Git LFS. Paths are relative to the project's Content directory, so the original author's drive layout is not required.

## Arduino connection

In the editor, connection settings are in `Config/IDA_Arduino.json`. Use the COM port assigned to the connected board and match the baud rate to its firmware. The agreed interaction trigger is an incoming newline-terminated `1` from Arduino to Unreal; Unreal does not need to send a command back for this flow. Hardware validation is still pending.

## Palm-touch video flow (keyboard test)

Open `ArduinoSampleMap`, press **Play**, and click the game viewport so it receives keyboard input.

1. `Video1.mp4` starts and loops as the idle screen.
2. Press **T** once to simulate placing a palm on the sensor.
3. `Video2.mp4` plays once. Further T presses or Arduino triggers are ignored while it is opening or playing. Releasing T does not interrupt it.
4. The media player's **On End Reached** event automatically returns to the idle loop. A new T press can start another interaction.

All interaction logic remains editable in `Content/Arduino/BP_ArduinoController`:

- **EventGraph**: creates the existing screen widget, binds the media events, starts idle playback, and closes the player on End Play.
- **Touch Input**: T and incoming serial `1` both call **StartInteraction**. The optional connection section reads the COM port and baud rate through the existing serial plugin.
- **Video Playback**: **PlayIdle**, **StartInteraction**, **MediaOpened**, and **MediaFinished** control the sequence. **IsInteractionActive** prevents a touch from restarting the main video. The error section returns to idle if the main source fails to open, and reports an idle-source failure without retrying endlessly.

Comment boxes separate the sections, and flow runs left to right. The unused K-key send command, touch-release events, and unused playback-state variables have been removed.

### Switching to Arduino later

1. Connect the board and set `port` and `baudRate` in `Config/IDA_Arduino.json` (currently `COM3` and `9600`).
2. Select `BP_ArduinoController` in the level, or open its Class Defaults, and enable **Enable Arduino**. It is **off by default** for hardware-free testing.
3. Have the firmware send `Serial.println("1");` **once when a new touch begins**, with sensor debounce. Do not continuously transmit `1` while a palm remains on the sensor; rearm the firmware after the palm is removed.
4. Close Arduino Serial Monitor before running Unreal so the COM port is available.

No video-graph rewiring is required. T remains usable for testing even when Arduino is enabled or a connection attempt fails. Serial `0` and other non-trigger messages do not stop the main video. Physical board/sensor testing is still required when the hardware becomes available.

## Collaborating

Accept the GitHub invitation, clone the project, and commit/push changes from your own GitHub account. Unreal `.uasset` and `.umap` files are binary: coordinate before editing the same Blueprint or map because Git cannot merge their graphs like text files.

Save all assets in Unreal before committing. Pull the latest changes before starting work. Keep generated folders such as `Binaries`, `Intermediate`, `Saved`, and `DerivedDataCache` out of commits.
