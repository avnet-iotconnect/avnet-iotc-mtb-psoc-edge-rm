## Introduction

This document demonstrates the steps of setting up the Infineon  PSOC™ Edge MCU boards
for connecting to Avnet's IoTConnect Platform. Supported boards are listed in 
the [README.md](README.md).

## Supported Toolchains (make variable 'TOOLCHAIN')

- GNU Arm&reg; Embedded Compiler v14.2.1 (`GCC_ARM`) – Default value of `TOOLCHAIN`

## Prerequisites
* PC with Windows. The project is tested with Windows 10, though the setup should work with Linux or Mac as well.
* USB-A to USB-C data cable
* 2.4GHz WiFi Network
* A serial terminal application such as [Tera Term](https://ttssh2.osdn.jp/index.html.en) or a browser-based application like [Google Chrome Labs Serial Terminal](https://googlechromelabs.github.io/serial-terminal/)
* A registered [myInfineon Account](https://www.infineon.com/sec/login)

## Hardware Setup
* Connect the board to a USB port on your PC. A new USB device should be detected.
Firmware logs will be available on that COM port.
* Open the Serial Terminal application and configure as shown below:
  * Port: (Select the COM port with the device)
  * Speed: `115200`
  * Data: `8 bits`
  * Parity: `none`
  * Stop Bits: `1`
  * Flow Control: `none`
  
## Building the Software

- Clone [this repository](https://github.com/avnet-iotconnect/iotc-mtb-e84-deepcraft-ready-model/tree/main). 
- Download [ModusToolbox&trade; software](https://www.infineon.com/cms/en/design-support/tools/sdk/modustoolbox-software/). Install the ***ModusToolbox&trade; Setup*** software. The software may require you to log into your Infineon account. In ***ModusToolbox&trade; Setup*** software, download & install the items below:
  - *ModusToolbox&trade; Tools Package* 3.6 or above.
  - *ModusToolbox&trade; Programming tools* 1.6.0.
  - Arm GCC Toolchain (GCC) 14.2.1.
  - Microsoft Visual Studio Code 1.105.0 or above. [(VS Code for ModusToolbox&trade; guide)](https://www.infineon.com/assets/row/public/documents/30/44/infineon-visual-studio-code-user-guide-usermanual-en.pdf?fileId=8ac78c8c92416ca50192787be52923b2)

- Launch ModusToolbox&trade; Dashboard. Select Target IDE `Microsoft Visual Studio` from drop down and then click *Launch Project Creator*.
- Select one of the supported boards from [README.md](README.md) and click *Next*.
- Click *Browse…* next to *Application(s) Root Path* to create or specify a folder where the application will
be created.
- Pull down the Target IDE menu and select *Microsoft Visual Studio Code*.
- Click the button *`Browse for Application`* and browse to the previously cloned repo's folder *iotc-mtb-e84-deepcraft-ready-model*.
- Close the Project Creater when the project is created successfully.
- Open VS code, and Select *File > Open Workspace from File*, navigate to the location of the application that was just
created, select the workspace file, and click Open.
- Depending on your settings in VS Code, you may see a message about trusting the authors. If so, click Yes, I
trust the authors.
- To build the project, select *Terminal > Run Task*. Then select *Build* from the dropdown.
- To debug the project, connect the board, select *Run > Start Debugging*. 
- When running or debugging, examine the console messages on the serial terminal. Once the device boots up,
it will print the auto-generated DUID (Device Unique ID) that you will use to 
create the device in the steps at [Cloud Account Setup](#cloud-account-setup) below in this guide:
  ``` 
   Generated device unique ID (DUID) is: e84-rm-xxxxxxxx
  ```
- Once the Cloud Account Setup is complete,
In the **.proj_cm33_ns** project directory modify **app_config.h** per your
IoTConnect device setup and **wifi_config.h** per your WiFi connection settings.
- Run or debug to connect the board to /IOTCONNECT.

## Cloud Account Setup
An IoTConnect account is required.  If you need to create an account, a free 2-month subscription is available.

Please follow the 
[Creating a New IoTConnect Account](https://github.com/avnet-iotconnect/avnet-iotconnect.github.io/blob/main/documentation/iotconnect/subscription/subscription.md)
guide and select one of the two implementations of IoTConnect: 
* [AWS Version](https://subscription.iotconnect.io/subscribe?cloud=aws)  
* [Azure Version](https://subscription.iotconnect.io/subscribe?cloud=azure)  

* Be sure to check any SPAM folder for the temporary password.

### Acquire IoTConnect Account Information

* Login to IoTConnect using the corresponding link below to the version to which you registered:  
    * [IoTConnect on AWS](https://console.iotconnect.io) 
    * [IoTConnect on Azure](https://portal.iotconnect.io)

* The Company ID (**CPID**) and Environment (**ENV**) variables are required to be stored into the device. Take note of these values for later reference.
<details><summary>Acquire <b>CPID</b> and <b>ENV</b> parameters from the IoTConnect Key Vault and save for later use</summary>
<img style="width:75%; height:auto" src="https://github.com/avnet-iotconnect/avnet-iotconnect.github.io/blob/bbdc9f363831ba607f40805244cbdfd08c887e78/assets/cpid_and_env.png"/>
</details>


#### IoTConnect Device Template Setup

An IoTConnect *Device Template* will need to be created or imported.
* Download the premade  [Device Template](files/device-template.json).
* Import the template into your IoTConnect instance:  [Importing a Device Template](https://github.com/avnet-iotconnect/avnet-iotconnect.github.io/blob/main/documentation/iotconnect/import_device_template.md) guide  
> **Note:**  
> For more information on [Template Management](https://docs.iotconnect.io/iotconnect/concepts/cloud-template/) please see the [IoTConnect Documentation](https://iotconnect.io) website.

#### IoTConnect Device Creation and Setup

* Create a new device in the IoTConnect portal. (Follow the [Create a New Device](https://github.com/avnet-iotconnect/avnet-iotconnect.github.io/blob/main/documentation/iotconnect/create_new_device.md) guide for a detailed walkthrough.)
* Enter the **DUID** noted from earlier into the *Unique ID* field
* Enter the same DUID or descriptive name of your choosing as *Display Name* to help identify your device
* Select the template from the dropdown box that was just imported ("psoc6mtb")
* Ensure "Auto-generated" is selected under *Device certificate*
* Click **Save & View**
* In the info panel, click the Connection Infohyperink on top right and 
download the certificate by clicking the download icon on the top right
![download-cert.png](media/download-cert.png).
* You will need to open the device certificate and private key files and 
provide them in **.proj_cm33_ns/app_config.h** formatted specified as a C string #define like so:
  ```
  #define IOTCONNECT_DEVICE_CERT \
  "-----BEGIN CERTIFICATE-----\n" \
  "MIICwTCCAakCFFUmScR+Y+XTcu0YKMQcFDXSENLKMA0GCSqGSIb3DQEBCwUAMB0x\n" \
  "GzAZBgNVBAMMEmF2dGRzLWlvdGNlZjJmNDAyMjAeFw0yNDA1MDIxOTEyMzRaFw0y\n" \
  "NTA1MDIxOTEyMzRaMB0xGzAZBgNVBAMMEmF2dGRzLWlvdGNlZjJmNDAyMjCCASIw\n" \
  "DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANA1q0MJwVLGn2uz7O/I2Wo80vnf\n" \
  "ho+U/LW7bHW3JkzrkWIsc2cnT9fDhbSHmUkNlj5yUl+DtsM5LlAV/QO+EHd1xubU\n" \
  "fmtXmk+/vB5g4OhGAI                               CvxlEqm2jW239sU\n" \
  "po153s2XPfO0A0NN8L                               PPTVVA/SlwmuKOp\n" \
  "YdtfTzhdBNiPtnt6xP      THIS IS AN EXAMPLE       mP25wfeCeNh1e64\n" \
  "KWMVsY1wBsPLsC7KmC                               aIaEJsTRiECAwEA\n" \
  "ATANBgkqhkiG9w0BAQ                               quxEn1IkGjmNF2I\n" \
  "m/3+BM/2qPTxZVnfZfgKr3xD3hedymY0JRiKHKZGVWQSClobrbL5p6DraYBwWSFe\n" \
  "h/lKhhBl0quu1vqXPhbMQaVcrBh4NGU8uDi3kezytqVhewR7wru/V3pdwvSer+Am\n" \
  "qr5Sg/2HGybLHGsYhiRqU6bEYhPUzmQJs5FBR9HPd1xsME0qP6MW9FnR7S06G+z4\n" \
  "UkWMseIlcxY6mGViLZGS362rAOAFQE9QYA9qdWyM+AIvjZjlQCbkTOiaEd6GXQIU\n" \
  "khPBBRXBKbDpQ02LgX6tsJUEbGbnPC94LfwnkTuVx/CFKWZfmg==\n" \
  "-----END CERTIFICATE-----"
  ```
  The same format needs to be used for the private key as `#define IOTCONNECT_DEVICE_KEY`

At this point, the device can be reprogrammed with the newly built firmware.

## OTA Support

Not availabe now.