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

Playback reads `IDA_Videos.json` from the project root when Play begins:

```json
{
  "idleVideo": "Videos/Video1.mp4",
  "mainVideo": "Videos/Video2.mp4"
}
```

`Videos/Video1.mp4` is the looping idle video. `Videos/Video2.mp4` is the main video after interaction. To change videos, stop Play/close the application, replace those files using the same names, then start again. No Blueprint edits or rebuild are needed. MP4 is the supplied format; use media that the Windows player can decode.

To use other names or locations, edit the two JSON paths. Relative paths resolve beside `IDA_Videos.json`; absolute Windows paths are also supported (use forward slashes, e.g. `D:/Exhibit/Main.mp4`). Both paths must exist. Invalid JSON, missing keys, or missing files produce an Output Log error and retain the original Blueprint media defaults. Decoder/open failures use the existing media-failure recovery.

In packaged builds, loose defaults are staged beside the inner executable and copied beside the top-level application on first launch. After that, replace the root `Videos` files or edit the root JSON; startup never overwrites existing replacements. The installation folder must be writable on first launch. Changes are read on application restart, not during an active journey.

The original `Content/Movies/MS_Idle` and `MS_Interaction` sources remain as fallback assets. Original and external MP4 files use Git LFS.

## Arduino connection

In the editor, connection settings are in `Config/IDA_Arduino.json`. Use the COM port assigned to the connected board and match the baud rate to its firmware. Arduino sends one newline-terminated command per gesture; Unreal does not need to send a command back. Hardware validation is still pending.

| Serial command | Sensor gesture | Unreal action |
| --- | --- | --- |
| `0` | Single tap | Start the main video from idle; subsequent single taps toggle pause/resume. |
| `1` | Double tap | Reset the whole journey to looping idle and wait for the next single tap. |

The firmware recognizes single and double taps. Send `Serial.println("0");` once for a single tap or `Serial.println("1");` once for a double tap. Wait for the double-tap window before emitting a single-tap command, so a double tap sends only `1`, without preceding `0` commands. The old five-second-hold gesture and command `2` are no longer used.

## Palm-touch video flow (keyboard test)

Open `ArduinoSampleMap`, press **Play**, and click the game viewport so it receives keyboard input.

1. `Video1.mp4` starts and loops as the idle screen.
2. Press **K**, **T**, or **P** to simulate one single tap (command `0`): idle starts `Video2.mp4`, playing pauses, and paused resumes.
3. Press **R** to simulate a recognized double tap (command `1`) and reset the whole journey to idle.
4. When the main video ends, or no valid commands arrive within the inactivity timeout, the journey returns to looping idle.

### Changing the inactivity timeout

Select `BP_ArduinoController` in the level and find **Journey | Settings > Inactivity Timeout (Minutes)** in Details, or set it in the Blueprint's **Class Defaults**. The default is **5 minutes**; fractional values are supported (for example, `0.5` is 30 seconds).

The countdown starts with the main journey and restarts after each single-tap `0` command during a main journey, including keyboard simulations. It continues while the video is paused or opening. Video playback alone does not count as interaction. Unknown serial messages do not refresh it. Double-tap command `1` returns to idle and clears the countdown and pause state. Returning to idle also clears the countdown. Changing the setting takes effect the next time a valid command arms the timer.

All interaction logic remains editable in `Content/Arduino/BP_ArduinoController`:

- **EventGraph**: creates the existing screen widget, binds the media events, starts idle playback, and closes the player on End Play.
- **Touch Input**: serial and test keys use **HandleArduinoCommand**. This routes single-tap `0` through the current journey state to **StartInteraction** or **ToggleInteractionPause**, then refreshes inactivity. Double-tap `1` calls **PlayIdle**. Reset uses the existing idle path directly. The optional connection section reads the COM port and baud rate through the existing serial plugin.
- **Video Playback**: **PlayIdle**, **StartInteraction**, **MediaOpened**, and **MediaFinished** control the sequence. **IsInteractionActive** decides whether a single tap starts the main video or toggles pause/resume. **WantsPaused** remembers pause/resume requests that arrive while the media source is opening. The error section returns to idle if the main source fails to open, and reports an idle-source failure without retrying endlessly.

All runtime interaction logic remains in editable Blueprint nodes. K/T/P are local single-tap simulations and does not send a command to Arduino.

### Switching to Arduino later

1. Connect the board and set `port` and `baudRate` in `Config/IDA_Arduino.json` (currently `COM5` and `115200`).
2. Select `BP_ArduinoController` in the level, or open its Class Defaults, and enable **Enable Arduino**. It is **off by default** for hardware-free testing.
3. Have the firmware recognize gestures with sensor debounce and send the newline-terminated command from the table above once per gesture. Send only one command per recognized gesture; do not report the two taps of a double tap as separate single taps.
4. Close Arduino Serial Monitor before running Unreal so the COM port is available.

No video-graph rewiring is required. K/T, P, and R remain usable for testing even when Arduino is enabled or a connection attempt fails. Physical board/sensor testing is still required when the hardware becomes available.

### Verification and editor tooling

`Scripts/verify_journey.py` runs a Play-in-Editor integration check with the actual media player and short timeouts applied only to the PIE actor. Launch the editor with `-ExecutePythonScript="<project>/Scripts/verify_journey.py"`; results are written to `Saved/JourneyVerification.json`. The saved default remains five minutes and the editor stays open.

The editor-only implementation in `JourneySetupCommandlet` can inspect the Blueprint with `-run=JourneySetup`, writing `Saved/JourneyBlueprintGraph.txt`. `-Apply` installs the command graph if needed and applies the current reset-to-idle behavior. It compiles before saving and backs up the original asset under `Saved/JourneyBackup/`.

`Scripts/tidy_journey.py` performs cleanup inside the editor using measured Slate node sizes, removes disconnected nodes and duplicate getters within each section, and checks that functional node rectangles do not overlap. It then runs the PIE verification. The layout report is `Saved/JourneyLayoutVerification.txt`. Comment boxes intentionally surround their section's nodes. This tooling does not add runtime components or tick events.

## Collaborating

Accept the GitHub invitation, clone the project, and commit/push changes from your own GitHub account. Unreal `.uasset` and `.umap` files are binary: coordinate before editing the same Blueprint or map because Git cannot merge their graphs like text files.

Save all assets in Unreal before committing. Pull the latest changes before starting work. Keep generated folders such as `Binaries`, `Intermediate`, `Saved`, and `DerivedDataCache` out of commits.
