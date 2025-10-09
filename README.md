## Introduction

This demo project is the integration of Infineon's [PSOC&trade; Edge MCU: DEEPCRAFT&trade; Ready Model deployment](https://github.com/Infineon/mtb-example-psoc-edge-ml-deepcraft-deploy-ready-model/tree/master)
and Avnet's [/IOTCONNECT ModusToolbox&trade; Basic Sample](https://github.com/avnet-iotconnect/avnet-iotc-mtb-basic-example/tree/release-v7.0.2). 
The project includes five different models, where four models detect different sounds: 
- Baby cry detection
- Cough detection 
- Alarm detection 
- Siren detection
- Gesture detection using the radar sensor (only on KIT_PSE84_AI):

These models use data from pulse-density modulation (PDM) to pulse-code modulation (PCM), which is then sent to the model for detection.

The fifth model detects hand gestures using data from the XENSIV&trade; radar sensor and this is applicable only for KIT_PSE84_AI.

Pre-trained models that are ready for production, referred to as "Ready Models," can be found on the [Imagimob Ready Model Landing Page](https://www.imagimob.com/ready-models). These models, when deployed on a device, are intended specifically for testing purposes and come with a limited number of inferences.

> **Note:** This version of the code example supports only quantised models (INT8x8).

This project has a three project structure: CM33 secure, CM33 non-secure, and CM55 projects. All three projects are programmed to the external QSPI flash and executed in Execute in Place (XIP) mode. Extended boot launches the CM33 secure project from a fixed location in the external flash, which then configures the protection settings and launches the CM33 non-secure application. Additionally, CM33 non-secure application enables CM55 CPU and launches the CM55 application.

## Requirements

- [ModusToolbox&trade;](https://www.infineon.com/modustoolbox) v3.6 or later (tested with v3.6)
- Board support package (BSP) minimum required version: 1.0.0
- Programming language: C
- Associated parts: All [PSOC&trade; Edge MCU](https://www.infineon.com/products/microcontroller/32-bit-psoc-arm-cortex/32-bit-psoc-edge-arm) parts

## Supported toolchains (make variable 'TOOLCHAIN')

- GNU Arm&reg; Embedded Compiler v14.2.1 (`GCC_ARM`) – Default value of `TOOLCHAIN`

## Supported kits (make variable 'TARGET')

- [PSOC&trade; Edge E84 AI Kit](https://www.infineon.com/KIT_PSE84_AI) (`KIT_PSE84_AI`)
- [PSOC&trade; Edge E84 Evaluation Kit](https://www.infineon.com/KIT_PSE84_EVAL) (`KIT_PSE84_EVAL_EPC2`)

## Hardware setup

This example uses the board's default configuration. See the kit user guide to ensure that the board is configured correctly.

Ensure the following jumper and pin configuration on board.
- BOOT SW must be in the HIGH/ON position
- J20 and J21 must be in the tristate/not connected (NC) position

> **Note:** This hardware setup is not required for KIT_PSE84_AI.

## Setup the Project



To build the project, please refer to the 
[/IOTCONNECT ModusToolbox&trade; DEVELOPER GUIDE](DEVELOPER_GUIDE.md) 
and note the following:

- To select the model, update the `MODEL_SELECTION` variable in the *Makefile* of .proj_cm55 project.
   Model name           |  Macro
   :--------            | :-------------
   Cough detection      | `COUGH_MODEL`
   Alarm detection      | `ALARM_MODEL`
   Baby cry detection   | `BABYCRY_MODEL`
   Siren detection      | `SIREN_MODEL`
   Gesture detection    | `GESTURE_MODEL`

> **Note:** Currently, gesture detection model is supported only for the PSOC&trade; Edge AI kit. 
<br>

- Use the [device-template.json](/files/device-template.json) as the Device Template on /IOTCONNECT. Right-click the link and select "Save Link As" to download the file.

- [Tutorial video](https://saleshosted.z13.web.core.windows.net/media/ifx/videos/IFX%20Modus%20with%20IoTConnect.mp4) of creating a new project with /IOTCONNECT in ModusToolbox&trade; for your reference.


## Running the Demo

- For sound models, once the board connects to /IOTCONNECT, it will start processing microphone input and attempt to detect the corresponding sound. 
This can be tested by placing the board in such way so that the microphone close to the PC speaker.
For best results, the microphone should be placed very close and pointed directly towards the speaker.

- The following YouTube sound clips can be used for testing:
  * [Baby Cry](https://www.youtube.com/watch?v=Rwj1_eWltJQ&t=227s)
  * [Cough](https://www.youtube.com/watch?v=Qp09X74kjBc)
  * [Alarm](https://www.youtube.com/watch?v=hFIJaB6kVzk)
  * [Siren](https://www.youtube.com/watch?v=s5bwBS27A1g)


- For Gesture detection model, place the kit at a distance of approximately 60 cms away from you for the gestures to be detected correctly. 
    * Push
    * Swipe Up
    * Swipe Down
    * Swipe Left
    * Swipe Right

- After a few seconds, the device will connect to /IOTCONNECT, and begin sending telemetry packets similar to the example below:
```
>: {"d":[{"d":{"version":"1.0.0","random":32,"class":"baby_cry"}}]}
```
- The following commands can be sent to the device using the /IOTCONNECT Web UI:

    | Command                  | Argument Type     | Description                                                                                                                                                                 |
    |:-------------------------|-------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
    | `board-user-led`         | String (on/off)   | Turn the board LED on or off                                                                                                                                                |
    | `set-reporting-interval` | Number (eg. 4000) | Set telemetry reporting interval in milliseconds.  By default, the application will report gestures every 1000ms and Audio every 2500ms                                     |
    | `demo-mode`              | String (on/off)   | Enable demo mode. In this mode the application will send telemetry to /IOTCONNECT for a longer period         