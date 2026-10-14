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

## Collaborating

Accept the GitHub invitation, clone the project, and commit/push changes from your own GitHub account. Unreal `.uasset` and `.umap` files are binary: coordinate before editing the same Blueprint or map because Git cannot merge their graphs like text files.

Save all assets in Unreal before committing. Pull the latest changes before starting work. Keep generated folders such as `Binaries`, `Intermediate`, `Saved`, and `DerivedDataCache` out of commits.
