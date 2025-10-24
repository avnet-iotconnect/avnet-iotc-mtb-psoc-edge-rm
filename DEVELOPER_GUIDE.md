## Introduction

This document demonstrates the steps of setting up the Infineon  PSOC™ Edge MCU boards
for connecting to Avnet's /IOTCONNECT Platform. Supported boards are listed in 
the [README.md](README.md).

## Prerequisites
* PC with Windows. The project is tested with Windows 10, though the setup should work with Linux or Mac as well.
* USB-A to USB-C data cable
* 2.4GHz WiFi Network
* A serial terminal application such as [Tera Term](https://ttssh2.osdn.jp/index.html.en) or a browser-based application like [Google Chrome Labs Serial Terminal](https://googlechromelabs.github.io/serial-terminal/)
* A registered [myInfineon Account](https://www.infineon.com/sec/login)

## Hardware Setup
* See the kit user guide to ensure that the board is configured correctly.
* If using the EVK board, ensure the following jumper and pin configuration on board.
  * BOOT SW must be in the HIGH/ON position
  * J20 and J21 must be in the tristate/not connected (NC) position (these should be default)
* Identify the debug USB port for your board from the board's user manual.
* Connect the board's debug port to a USB port on your PC. A new USB device should be detected.
Firmware logs will be available on that COM port.
* Open the Serial Terminal application and configure as shown below:
  * Port: (Select the COM port with the device)
  * Speed: `115200`
  * Data: `8 bits`
  * Parity: `none`
  * Stop Bits: `1`
  * Flow Control: `none`
  
## Building the Software

> [!NOTE]
> If you wish to contribute to this project, work with your own git fork,
> or evaluate an application version that is not yet released, the setup steps will change 
> the setup steps slightly.
> In that case, read [DEVELOPER_LOCAL_SETUP.md](https://github.com/avnet-iotconnect/avnet-iotc-mtb-basic-example/blob/main/DEVELOPER_LOCAL_SETUP.md)
> (From the PSOC6 Basic Sample repo)
> before continuing to the steps below.
> Follow the [Contributing Guidelines](https://github.com/avnet-iotconnect/iotc-c-lib/blob/master/CONTRIBUTING.md) 
> if you are contributing to this project.

- Download [ModusToolbox&trade; software](https://www.infineon.com/cms/en/design-support/tools/sdk/modustoolbox-software/). Install the ***ModusToolbox&trade; Setup*** software. The software may require you to log into your Infineon account. In ***ModusToolbox&trade; Setup*** software, download & install the items below:
  - ModusToolbox&trade; Tools Package 3.6.
  - ModusToolbox&trade; Edge Protect Security Suite 1.6.0.
  - ModusToolbox&trade; Programming Tools 1.6.0.
  - Arm GCC Toolchain (GCC) 14.2.1.
  - Microsoft Visual Studio Code.

- Install and set up VS Code per [VS Code for ModusToolbox&trade; guide](https://www.infineon.com/assets/row/public/documents/30/44/infineon-visual-studio-code-user-guide-usermanual-en.pdf).
At the time of writing this guide, it is only required to follow the first few sections
that explain how to install VS Code itself, the required VS Code Plugins and the J-Link Software.
- Launch ModusToolbox&trade; Dashboard. Select Target IDE `Microsoft Visual Studio` 
from the dropdown on top-right and then click *Launch Project Creator*.
- Select one of the supported boards from [README.md](README.md) and click *Next*.
- For the Application(s) Root Path, specify or browse to a directory where the application will be created.
It is preferred to use a short path due to Windows OS file path limits.
- Ensure that the Target IDE is *Microsoft Visual Studio Code*.
- Checkmark this repo's application by browsing Template Applications or searching for this application name. 
We suggest searching for "Avnet" first to reduce the list.
- It is recommended to override the New Application Name value to a shorter name.
- Click *Create* and close the Project Creator when the project is created successfully.
- Open VS Code, and Select *File -> Open Workspace from File*, navigate to the location of the application that was just
created, select the workspace file, and click *Open*.
- Depending on your settings in VS Code and VS Code version, you may see a message about trusting the authors. 
If so, click *Yes, I trust the authors*.

- Once the [Cloud Account Setup](#cloud-account-setup) below is complete,
In the *proj_cm33_ns* project directory modify **app_config.h** per your
/IOTCONNECT device setup and **wifi_config.h** per your WiFi connection settings.

- To build the project, select *Terminal -> Run Task*. Then select *Build* from the dropdown.
- To program the project onto the board, connect the board, 
select *Terminal -> Run Task*. Then select *Program* from the dropdown.
- If you wish to debug the project, select *Run > Start Debugging* instead.


## Cloud Account Setup

### Create an /IOTCONNECT Account
An /IOTCONNECT account with an AWS backend is required.  If you need to create an account, a free trial subscription is available.
The free subscription may be obtained directly from [iotconnect.io](https://iotconnect.io) or through the AWS Marketplace.

* Option #1 **(Recommended)**   
/IOTCONNECT via [AWS Marketplace](https://github.com/avnet-iotconnect/avnet-iotconnect.github.io/blob/main/documentation/iotconnect/subscription/iotconnect_aws_marketplace.md) - 60 day trial; AWS account creation required  


* Option #2  
/IOTCONNECT via [iotconnect.io](https://subscription.iotconnect.io/subscribe?cloud=aws) - 30 day trial; no credit card required

> [!NOTE]
> Be sure to check any SPAM folder for the temporary password after registering.

Login to the platform by navigating to [console.iotconnect.io](https://console.iotconnect.io)

### Acquire /IOTCONNECT Account Information

* Login to /IOTCONNECT using the corresponding link below to the version to which you registered:  
    * [/IOTCONNECT on AWS](https://console.iotconnect.io) 
    * [/IOTCONNECT on Azure](https://portal.iotconnect.io)

* The Company ID (**CPID**) and Environment (**ENV**) variables are required to be stored into the device. Take note of these values for later reference.
<details><summary>Acquire <b>CPID</b> and <b>ENV</b> parameters from the /IOTCONNECT Key Vault and save for later use</summary>
<img style="width:75%; height:auto" src="https://github.com/avnet-iotconnect/avnet-iotconnect.github.io/blob/bbdc9f363831ba607f40805244cbdfd08c887e78/assets/cpid_and_env.png"/>
</details>


### /IOTCONNECT Device Template Setup

An /IOTCONNECT *Device Template* will need to be created or imported.
* Download the premade [device-template.json](files/device-template.json) 
(Open the link then click the *Download Raw File* icon on the right).
* Import the template into your /IOTCONNECT instance:  [Importing a Device Template](https://github.com/avnet-iotconnect/avnet-iotconnect.github.io/blob/main/documentation/iotconnect/import_device_template.md) guide  
> **Note:**  
> For more information on [Template Management](https://docs.iotconnect.io/iotconnect/concepts/cloud-template/) 
> please see the [/IOTCONNECT Documentation](https://iotconnect.io) website.

### /IOTCONNECT Device Creation and Setup

* Create a new device in the /IOTCONNECT portal. (Follow the [Create a New Device](https://github.com/avnet-iotconnect/avnet-iotconnect.github.io/blob/main/documentation/iotconnect/create_new_device.md) guide for a detailed walkthrough).
* Choose a name for your device and enter it into the *Unique ID* field (also called Device Unique ID - DUID in this guide).
* Enter the same DUID or descriptive name of your choosing as *Display Name* to help identify your device.
* Select the template from the dropdown box that was just imported.
* Ensure "Auto-generated" is selected under *Device certificate*.
* Click **Save & View**.
* In the *Info* panel, click the *Connection Info* hyperink on top right and 
download the certificate by clicking the download icon on the top right
![download-cert.png](media/download-cert.png).
* Provide values for DUID, CPID and ENV from the above steps into the **proj_cm33_ns/app_config.h** file.
* Set your IOTCONNECT_CONNECTION_TYPE in the same file, per comments.
* Unzip the previously downloaded certificates zip into a directory.
Open the device certificate and private key files in an editor.
Use the VS Code Copilot AI Agent to set up the certificate and private key for you with the following prompt:
> ```
> Set IOTCONNECT_DEVICE_CERT and IOTCONNECT_DEVICE_KEY in app_config.h.
> Use the instructions in the file and these PEM contents:
> (copy and paste the cert and private key as text here)
> ```

At this point, the application is set up with /IOTCONNECT credentials.
