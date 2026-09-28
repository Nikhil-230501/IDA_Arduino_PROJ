# IDA Arduino Service for Unreal Engine

**IDA's Proprietary Arduino Service for Unreal Engine**

A native Arduino communication service developed by **IDA** for integrating Arduino hardware with Unreal Engine projects.

The service provides a reusable Unreal Engine plugin for communicating with Arduino devices over a serial connection, allowing Unreal Engine applications to send commands to Arduino and receive data from Arduino in real time.

---

## Overview

The **IDA Arduino Service** provides a native C++ serial communication layer for Unreal Engine.

It is designed to make Arduino integration simple and reusable across Unreal Engine projects without requiring developers to implement serial communication from scratch.

The plugin provides:

- Arduino serial connection
- Configurable COM port
- Configurable baud rate
- Sending data from Unreal Engine to Arduino
- Receiving data from Arduino
- Background serial reading
- Blueprint events for received data
- External JSON configuration
- Runtime configuration without rebuilding the Unreal project

---

## Features

### Serial Communication

Connect Unreal Engine directly to an Arduino device through a Windows serial/COM port.

Example:

```text
Unreal Engine
      |
      | Serial / USB
      |
      v
   Arduino
