## Avnet PSOC™ Edge DEEPCRAFT™ Ready Models

This demo project is the integration of Infineon's 
[PSOC&trade; Edge MCU: DEEPCRAFT&trade; Ready Model deployment](https://github.com/Infineon/mtb-example-psoc-edge-ml-deepcraft-deploy-ready-model/tree/release-v1.2.0)
and [Avnet /IOTCONNECT ModusToolbox&trade; SDK](https://github.com/avnet-iotconnect/avnet-iotc-mtb-sdk). 
The project includes five different models, where four models detect different sounds: 
- Baby cry detection
- Cough detection 
- Alarm detection
- Gesture detection using the radar sensor (only on KIT_PSE84_AI):

These models use data from pulse-density modulation (PDM) to pulse-code modulation (PCM), which is then sent to the model for detection.

The fifth model detects hand gestures using data from the XENSIV&trade; radar sensor and this is applicable only for KIT_PSE84_AI.

Pre-trained models that are ready for production, referred to as "Ready Models," can be found on the [Imagimob Ready Model Landing Page](https://www.imagimob.com/ready-models). These models, when deployed on a device, are intended specifically for testing purposes and come with a limited number of inferences.

> **Note:** This version of the code example supports only quantised models (INT8x8).

This project has a three project structure: CM33 secure, CM33 non-secure, and CM55 projects. All three projects are programmed to the external QSPI flash and executed in Execute in Place (XIP) mode. Extended boot launches the CM33 secure project from a fixed location in the external flash, which then configures the protection settings and launches the CM33 non-secure application. Additionally, CM33 non-secure application enables CM55 CPU and launches the CM55 application.

The M55 processor performs the DEEPCRAFT™ model heavy lifting and reports the data via IPC to the M33 processor.
The M33 Non-Secure application is a custom /IOTCONNECT application that is receiving the IPC messages, 
processing the data and sending it to /IOTCONNECT. 
This application can receive Cloud-To-Device commands as well and control one of the board LEDs or control the application flow.    

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

This example uses the board's default configuration. 
See the kit user guide to ensure that the board is configured correctly.

Ensure the following jumper and pin configuration on board.
- BOOT SW must be in the HIGH/ON position
- J20 and J21 must be in the tristate/not connected (NC) position

> **Note:** This hardware setup is not required for KIT_PSE84_AI.

## Setup The Project

To setup the project, please refer to the 
[/IOTCONNECT ModusToolbox&trade; PSOC Edge Developer Guide](DEVELOPER_GUIDE.md)

- To select the model, update the `MODEL_SELECTION` variable in the *Makefile* of proj_cm55 project.

| Model name         | Macro           |
|:-------------------|:----------------|
| Cough detection    | `COUGH_MODEL`   |
| Alarm detection    | `ALARM_MODEL`   |
| Baby cry detection | `BABYCRY_MODEL` |
| Gesture detection  | `GESTURE_MODEL` |

> **Note:** Currently, gesture detection model is supported only for the PSOC&trade; Edge AI kit.


## Running The Demo

- For audio models, once the board connects to /IOTCONNECT, 
it will start processing microphone input and attempt to detect the corresponding sound. 
This can be tested by placing the board in such way so that the microphone close to the PC speaker.


- The following YouTube sound clips can be used for testing:
  * [Baby Cry](https://www.youtube.com/watch?v=Rwj1_eWltJQ&t=265s)
  * [Cough](https://www.youtube.com/watch?v=Qp09X74kjBc)
  * [Alarm](https://www.youtube.com/watch?v=hFIJaB6kVzk)


- For Gesture detection model, if having issues with detections, 
place the kit at a distance of approximately 60 cms away from you,
for the gestures to be detected correctly. 
See the original Infineon project github page for more details on how to perform gestures:
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

    | Command                  | Argument Type     | Description                                                        |
    |:-------------------------|-------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
    | `board-user-led`         | String (on/off)   | Turn the board LED on or off                                                                                                                                                |
    | `set-reporting-interval` | Number (eg. 2000) | Set telemetry reporting interval in milliseconds.  By default, the application will report every 2000ms                                     |
