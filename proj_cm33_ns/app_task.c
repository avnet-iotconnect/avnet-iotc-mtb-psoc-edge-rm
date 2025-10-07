/******************************************************************************
* File Name:   mqtt_task.c
*
* Description: This file contains the task that handles initialization & 
*              connection of Wi-Fi and the MQTT client. The task then starts 
*              the subscriber and the publisher tasks. The task also implements
*              reconnection mechanisms to handle WiFi and MQTT disconnections.
*              The task also handles all the cleanup operations to gracefully 
*              terminate the Wi-Fi and MQTT connections in case of any failure.
*
* Related Document: See README.md
*
*
*******************************************************************************
* * Copyright 2024-2025, Cypress Semiconductor Corporation (an Infineon company) or
* * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
* *
* * This software, including source code, documentation and related
* * materials ("Software") is owned by Cypress Semiconductor Corporation
* * or one of its affiliates ("Cypress") and is protected by and subject to
* * worldwide patent protection (United States and foreign),
* * United States copyright laws and international treaty provisions.
* * Therefore, you may use this Software only as provided in the license
* * agreement accompanying the software package from which you
* * obtained this Software ("EULA").
* * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* * non-transferable license to copy, modify, and compile the Software
* * source code solely for use in connection with Cypress's
* * integrated circuit products.  Any reproduction, modification, translation,
* * compilation, or representation of this Software except as specified
* * above is prohibited without the express written permission of Cypress.
* *
* * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* * reserves the right to make changes to the Software without notice. Cypress
* * does not assume any liability arising out of the application or use of the
* * Software or any product or circuit described in the Software. Cypress does
* * not authorize its products for use in any products where a malfunction or
* * failure of the Cypress product may reasonably be expected to result in
* * significant property damage, injury or death ("High Risk Product"). By
* * including Cypress's product in a High Risk Product, the manufacturer
* * of such system or application assumes all risk of such use and in doing
* * so agrees to indemnify Cypress against all liability.
*******************************************************************************/
/* SPDX-License-Identifier: MIT
 * Copyright (C) 2025 Avnet
 * Authors: Nikola Markovic <nikola.markovic@avnet.com>, Shu Liu <shu.liu@avnet.com> et al.
 */

#include "cybsp.h"

/* FreeRTOS header files */
#include "FreeRTOS.h"
#include "task.h"


/* Middleware libraries */
#include "cy_retarget_io.h"
#include "cy_wcm.h"

#include "cy_mqtt_api.h"
#include "clock.h"

/* LwIP header files */
#include "lwip/netif.h"


#include "iotconnect.h"

#include "retarget_io_init.h"
#include "ipc_communication.h"
#include "wifi_config.h"
#include "app_config.h"


/******************************************************************************
* Macros
******************************************************************************/

/* Time in milliseconds to wait before creating the publisher task. */
#define TASK_CREATION_DELAY_MS           (2000u)

/* Flag Masks for tracking which cleanup functions must be called. */
#define WCM_INITIALIZED                             (1lu << 0)
#define WIFI_CONNECTED                              (1lu << 1)
#define TIME_DIV_MS                                 (60000u)
#define APP_SDIO_INTERRUPT_PRIORITY                 (7U)
#define APP_HOST_WAKE_INTERRUPT_PRIORITY            (2U)
#define APP_SDIO_FREQUENCY_HZ                       (25000000U)
#define SDHC_SDIO_64BYTES_BLOCK                     (64U)


/* Macro to check if the result of an operation was successful and set the 
 * corresponding bit in the status_flag based on 'init_mask' parameter. When 
 * it has failed, print the error message and return the result to the 
 * calling function.
 */
#define CHECK_RESULT(result, init_mask, error_message...)      \
                     do                                        \
                     {                                         \
                         if ((int)result == CY_RSLT_SUCCESS)   \
                         {                                     \
                             status_flag |= init_mask;         \
                         }                                     \
                         else                                  \
                         {                                     \
                             printf(error_message);            \
                             return result;                    \
                         }                                     \
                     } while(0)

/******************************************************************************
* Global Variables
*******************************************************************************/

/* Flag to denote initialization status of various operations. */
uint32_t status_flag;

/* Pointer to the network buffer needed by the MQTT library for MQTT send and 
 * receive operations.
 */
static mtb_hal_sdio_t sdio_instance;
static cy_stc_sd_host_context_t sdhc_host_context;
static cy_wcm_config_t wcm_config;


#define APP_VERSION		"1.0.0"
static bool is_demo_mode = false;

static int reporting_interval = 2000;

/******************************************************************************
 * Function Name: wifi_connect
 ******************************************************************************
 * Summary:
 *  Function that initiates connection to the Wi-Fi Access Point using the
 *  specified SSID and PASSWORD. The connection is retried a maximum of
 *  'MAX_WIFI_CONN_RETRIES' times with interval of 'WIFI_CONN_RETRY_INTERVAL_MS'
 *  milliseconds.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  cy_rslt_t : CY_RSLT_SUCCESS upon a successful Wi-Fi connection, else an
 *              error code indicating the failure.
 *
 ******************************************************************************/
static cy_rslt_t wifi_connect(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_wcm_connect_params_t connect_param;
    cy_wcm_ip_address_t ip_address;

    /* Check if Wi-Fi connection is already established. */
    if (!(cy_wcm_is_connected_to_ap()))
    {
        /* Configure the connection parameters for the Wi-Fi interface. */
        memset(&connect_param, 0, sizeof(cy_wcm_connect_params_t));
        memcpy(connect_param.ap_credentials.SSID, WIFI_SSID, sizeof(WIFI_SSID));
        memcpy(connect_param.ap_credentials.password, WIFI_PASSWORD, sizeof(WIFI_PASSWORD));
        connect_param.ap_credentials.security = WIFI_SECURITY;

        printf("\nWi-Fi Connecting to '%s'\n", connect_param.ap_credentials.SSID);

        /* Connect to the Wi-Fi AP. */
        for (uint32_t retry_count = 0; retry_count < MAX_WIFI_CONN_RETRIES; retry_count++)
        {
            result = cy_wcm_connect_ap(&connect_param, &ip_address);

            if (CY_RSLT_SUCCESS == result)
            {
                printf("\nSuccessfully connected to Wi-Fi network '%s'.\n", connect_param.ap_credentials.SSID);

                /* Set the appropriate bit in the status_flag to denote
                 * successful Wi-Fi connection, print the assigned IP address.
                 */
                status_flag |= WIFI_CONNECTED;
                if (ip_address.version == CY_WCM_IP_VER_V4)
                {
                    printf("IPv4 Address Assigned: %s\n\n", ip4addr_ntoa((const ip4_addr_t *) &ip_address.ip.v4));
                }
                else if (ip_address.version == CY_WCM_IP_VER_V6)
                {
                    printf("IPv6 Address Assigned: %s\n\n", ip6addr_ntoa((const ip6_addr_t *) &ip_address.ip.v6));
                }
                return result;
            }

            printf("Wi-Fi Connection failed. Error code:0x%0X. Retrying in %d ms. Retries left: %d\n",
                (int)result, WIFI_CONN_RETRY_INTERVAL_MS, (int)(MAX_WIFI_CONN_RETRIES - retry_count - 1));
            vTaskDelay(pdMS_TO_TICKS(WIFI_CONN_RETRY_INTERVAL_MS));
        }

        printf("\nExceeded maximum Wi-Fi connection attempts!\n");
        printf("Wi-Fi connection failed after retrying for %d mins\n\n",
            (int)(WIFI_CONN_RETRY_INTERVAL_MS * MAX_WIFI_CONN_RETRIES) / TIME_DIV_MS);
    }
    return result;
}


/*******************************************************************************
* Function Name: sdio_interrupt_handler
********************************************************************************
* Summary:
* Interrupt handler function for SDIO instance.
*******************************************************************************/
static void sdio_interrupt_handler(void)
{
    mtb_hal_sdio_process_interrupt(&sdio_instance);
}

/*******************************************************************************
* Function Name: host_wake_interrupt_handler
********************************************************************************
* Summary:
* Interrupt handler function for the host wake up input pin.
*******************************************************************************/
static void host_wake_interrupt_handler(void)
{
    mtb_hal_gpio_process_interrupt(&wcm_config.wifi_host_wake_pin);
}

/*******************************************************************************
* Function Name: app_sdio_init
********************************************************************************
* Summary:
* This function configures and initializes the SDIO instance used in
* communication between the host MCU and the wireless device.
*******************************************************************************/
static void app_sdio_init(void)
{
    cy_rslt_t result;
    mtb_hal_sdio_cfg_t sdio_hal_cfg;
    cy_stc_sysint_t sdio_intr_cfg =
    {
        .intrSrc = CYBSP_WIFI_SDIO_IRQ,
        .intrPriority = APP_SDIO_INTERRUPT_PRIORITY
    };

    cy_stc_sysint_t host_wake_intr_cfg =
    {
            .intrSrc = CYBSP_WIFI_HOST_WAKE_IRQ,
            .intrPriority = APP_HOST_WAKE_INTERRUPT_PRIORITY
    };

    /* Initialize the SDIO interrupt and specify the interrupt handler. */
    cy_en_sysint_status_t interrupt_init_status = Cy_SysInt_Init(&sdio_intr_cfg, sdio_interrupt_handler);

    /* SDIO interrupt initialization failed. Stop program execution. */
    if(CY_SYSINT_SUCCESS != interrupt_init_status)
    {
        handle_app_error();
    }

    /* Enable NVIC interrupt. */
    NVIC_EnableIRQ(CYBSP_WIFI_SDIO_IRQ);

    /* Setup SDIO using the HAL object and desired configuration */
    result = mtb_hal_sdio_setup(&sdio_instance, &CYBSP_WIFI_SDIO_sdio_hal_config, NULL, &sdhc_host_context);

    /* SDIO setup failed. Stop program execution. */
    if(CY_RSLT_SUCCESS != result)
    {
        handle_app_error();
    }

    /* Initialize and Enable SD HOST */
    Cy_SD_Host_Enable(CYBSP_WIFI_SDIO_HW);
    Cy_SD_Host_Init(CYBSP_WIFI_SDIO_HW, CYBSP_WIFI_SDIO_sdio_hal_config.host_config, &sdhc_host_context);
    Cy_SD_Host_SetHostBusWidth(CYBSP_WIFI_SDIO_HW, CY_SD_HOST_BUS_WIDTH_4_BIT);

    sdio_hal_cfg.frequencyhal_hz = APP_SDIO_FREQUENCY_HZ;
    sdio_hal_cfg.block_size = SDHC_SDIO_64BYTES_BLOCK;

    /* Configure SDIO */
    mtb_hal_sdio_configure(&sdio_instance, &sdio_hal_cfg);

    /* Setup GPIO using the HAL object for WIFI WL REG ON  */
    mtb_hal_gpio_setup(&wcm_config.wifi_wl_pin, CYBSP_WIFI_WL_REG_ON_PORT_NUM, CYBSP_WIFI_WL_REG_ON_PIN);

    /* Setup GPIO using the HAL object for WIFI HOST WAKE PIN  */
    mtb_hal_gpio_setup(&wcm_config.wifi_host_wake_pin, CYBSP_WIFI_HOST_WAKE_PORT_NUM, CYBSP_WIFI_HOST_WAKE_PIN);

    /* Initialize the Host wakeup interrupt and specify the interrupt handler. */
    cy_en_sysint_status_t interrupt_init_status_host_wake =  Cy_SysInt_Init(&host_wake_intr_cfg, host_wake_interrupt_handler);

    /* Host wake up interrupt initialization failed. Stop program execution. */
    if(CY_SYSINT_SUCCESS != interrupt_init_status_host_wake)
    {
        handle_app_error();
    }

    /* Enable NVIC interrupt. */
    NVIC_EnableIRQ(CYBSP_WIFI_HOST_WAKE_IRQ);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IOTCONNECT

static void on_connection_status(IotConnectConnectionStatus status) {
    // Add your own status handling
    switch (status) {
        case IOTC_CS_MQTT_CONNECTED:
            printf("IoTConnect Client Connected notification.\n");
            break;
        case IOTC_CS_MQTT_DISCONNECTED:
            printf("IoTConnect Client Disconnected notification.\n");
            break;
        default:
            printf("IoTConnect Client ERROR notification\n");
            break;
    }
}

static void on_ota(IotclC2dEventData data) {
    const char *ota_host = iotcl_c2d_get_ota_url_hostname(data, 0);
    if (ota_host == NULL){
        printf("OTA host is invalid.\n");
        return;
    }
    const char *ota_path = iotcl_c2d_get_ota_url_resource(data, 0);
    if (ota_path == NULL) {
        printf("OTA resource is invalid.\n");
        return;
    }
    printf("OTA download request received for https://%s%s, but it is not implemented.\n", ota_host, ota_path);
}

// returns success on matching the expected format. Returns is_on, assuming "on" for true, "off" for false
static bool parse_on_off_command(const char* command, const char* name, bool *arg_parsing_success, bool *is_on, const char** message) {
    *arg_parsing_success = false;
    *message = NULL;
    size_t name_len = strlen(name);
    if (0 == strncmp(command, name, name_len)) {
        if (strlen(command) < name_len + 2) { // one for space and at least one character for the argument
            printf("ERROR: Expected command \"%s\" to have an argument\n", command);
            *message = "Command requires an argument";
            *arg_parsing_success = false;
        } else if (0 == strcmp(&command[name_len + 1], "on")) {
            *is_on = true;
            *message = "Value is now \"on\"";
            *arg_parsing_success = true;
        } else if (0 == strcmp(&command[name_len + 1], "off")) {
            *is_on = false;
            *message = "Value is now \"off\"";
            *arg_parsing_success = true;
        } else {
            *message = "Command argument";
            *arg_parsing_success = false;
        }
        // we matches the command
        return true;
    }

    // not our command
    return false;
}

static void on_command(IotclC2dEventData data) {
    const char * const BOARD_STATUS_LED = "board-user-led";
    const char * const DEMO_MODE_CMD = "demo-mode";
    const char * const SET_REPORTING_INTERVAL = "set-reporting-interval "; // with a space

    bool command_success = false;
    const char * message = NULL;

    const char *command = iotcl_c2d_get_command(data);
    const char *ack_id = iotcl_c2d_get_ack_id(data);

    if (command) {
        bool arg_parsing_success;
        printf("Command %s received with %s ACK ID\n", command, ack_id ? ack_id : "no");
        // could be a command without acknowledgment, so ackID can be null
        bool led_on;
        if (parse_on_off_command(command, BOARD_STATUS_LED, &arg_parsing_success, &led_on, &message)) {
            command_success = arg_parsing_success;
            if (arg_parsing_success) {
                if (led_on) {
                    Cy_GPIO_Set(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
                } else {
                    Cy_GPIO_Clr(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
                }
            }
        } else if (parse_on_off_command(command, DEMO_MODE_CMD,  &arg_parsing_success, &is_demo_mode, &message)) {
            command_success = arg_parsing_success;
        } else if (0 == strncmp(SET_REPORTING_INTERVAL, command, strlen(SET_REPORTING_INTERVAL))) {
        	int value = atoi(&command[strlen(SET_REPORTING_INTERVAL)]);
        	if (0 == value) {
                message = "Argument parsing error";
        	} else {
        		reporting_interval = value;
        		printf("Reporting interval set to %d\n", value);
        		message = "Reporting interval set";
        		command_success =  true;
        	}
        } else {
            printf("Unknown command \"%s\"\n", command);
            message = "Unknown command";
        }
    } else {
        printf("Failed to parse command. Command or argument missing?\n");
        message = "Parsing error";
    }

    // could be a command without ack, so ack ID can be null
    // the user needs to enable acknowledgments in the template to get an ack ID
    if (ack_id) {
        iotcl_mqtt_send_cmd_ack(
                ack_id,
                command_success ? IOTCL_C2D_EVT_CMD_SUCCESS_WITH_ACK : IOTCL_C2D_EVT_CMD_FAILED,
                message // allowed to be null, but should not be null if failed, we'd hope
        );
    } else {
        // if we send an ack
        printf("Message status is %s. Message: %s\n", command_success ? "SUCCESS" : "FAILED", message ? message : "<none>");
    }
}

static cy_rslt_t publish_telemetry(void) {
    ipc_payload_t payload;
    // useful fro debugging - making sure we have te latest data:
    // printf("Has IPC Data: %s\n", cm33_ipc_has_received_message() ? "true" : "false");
    cm33_ipc_safe_get_and_clear_cached_detection(&payload);
    IotclMessageHandle msg = iotcl_telemetry_create();
    iotcl_telemetry_set_string(msg, "version", APP_VERSION);
    iotcl_telemetry_set_number(msg, "random", rand() % 100); // test some random numbers
    iotcl_telemetry_set_number(msg, "class_id", payload.label_id);
    iotcl_telemetry_set_string(msg, "class", payload.label);
	iotcl_telemetry_set_bool(msg, "event_detected", payload.label_id > 0);
	
    iotcl_mqtt_send_telemetry(msg, false);
    iotcl_telemetry_destroy(msg);
    return CY_RSLT_SUCCESS;
}

void app_task(void *pvParameters) {
    (void) pvParameters;
    
    //E84 needs this for WiFi
    app_sdio_init();
 
    printf("===============================================================\n");
    printf("Starting The App Task\n");
    printf("===============================================================\n\n");


    uint64_t hwuid = Cy_SysLib_GetUniqueId();
    uint32_t hwuidhi = (uint32_t)(hwuid >> 32);
    // not using low bytes in the name because they appear to be the same across all boards of the same type
    // feel free to modify the application to use these bytes
    // uint32_t hwuidlo = (uint32_t)(hwuid & 0xFFFFFFFF);

    char iotc_duid[IOTCL_CONFIG_DUID_MAX_LEN] = {0};
    sprintf(iotc_duid, IOTCONNECT_DUID_PREFIX"%08lx", (unsigned long) hwuidhi);

    printf("Generated device unique ID (DUID) is: %s\n", iotc_duid);

    if (strlen(IOTCONNECT_DEVICE_CERT) == 0) {
		printf("Device certificate is missing.\n");
        while (1) { taskYIELD(); }
	}

    IotConnectClientConfig config;
    iotconnect_sdk_init_config(&config);
    config.connection_type = IOTCONNECT_CONNECTION_TYPE;
    config.cpid = IOTCONNECT_CPID;
    config.env =  IOTCONNECT_ENV;
    config.duid = iotc_duid;
    config.qos = 1;
    config.verbose = true;
    config.x509_config.device_cert = IOTCONNECT_DEVICE_CERT;
    config.x509_config.device_key = IOTCONNECT_DEVICE_KEY;
    config.callbacks.status_cb = on_connection_status;
    config.callbacks.cmd_cb = on_command;
    config.callbacks.ota_cb = on_ota;

    const char * conn_type_str = "(UNKNOWN)";
    if (config.connection_type == IOTC_CT_AWS) {
        conn_type_str = "AWS";
    } else if(config.connection_type == IOTC_CT_AZURE) {
        conn_type_str = "Azure";
    }

    printf("Current Settings:\n");
    printf("Platform: %s\n", conn_type_str);
    printf("DUID: %s\n", config.duid);
    printf("CPID: %s\n", config.cpid);
    printf("ENV: %s\n", config.env);

    //WIFI CONNECT
    /* Configure the Wi-Fi interface as a Wi-Fi STA (i.e. Client). */
    wcm_config.interface = CY_WCM_INTERFACE_TYPE_STA;
    wcm_config.wifi_interface_instance = &sdio_instance;

    /* Initialize the Wi-Fi Connection Manager and jump to the cleanup block 
     * upon failure.
     */
    if (CY_RSLT_SUCCESS != cy_wcm_init(&wcm_config)) {
        CY_ASSERT(0);
    }

    /* Set the appropriate bit in the status_flag to denote successful 
     * WCM initialization.
     */
    status_flag |= WCM_INITIALIZED;
    printf("\nWi-Fi Connection Manager initialized.\n");

    /* Initiate connection to the Wi-Fi AP and cleanup if the operation fails. */
    if (CY_RSLT_SUCCESS == wifi_connect()) {
		printf("wifi is connected.\n");
	} else {
		if (CY_RSLT_SUCCESS != wifi_connect()) {
			printf("wifi failed to connect.\n");
			goto exit_cleanup;
		}
	}

    cy_rslt_t ret = iotconnect_sdk_init(&config);
    if (CY_RSLT_SUCCESS != ret) {
        printf("Failed to initialize the IoTConnect SDK. Error code: %lu\n", ret);
        goto exit_cleanup;
    }


    for (int i = 0; i < 10; i++) {
        ret = iotconnect_sdk_connect();
        if (CY_RSLT_SUCCESS != ret) {
            printf("Failed to initialize the IoTConnect SDK. Error code: %lu\n", ret);
            goto exit_cleanup;
        }
        
        int max_messages = is_demo_mode ? 6000 : 300;
        for (int j = 0; iotconnect_sdk_is_connected() && j < max_messages; j++) {
            cy_rslt_t result = publish_telemetry();
            if (result != CY_RSLT_SUCCESS) {
                break;
            }
            iotconnect_sdk_poll_inbound_mq(reporting_interval);
        }
        iotconnect_sdk_disconnect();
    }
    iotconnect_sdk_deinit();

    printf("\nAppTask Done.\n");
    while (1) {
        taskYIELD();
    }
    return;

    exit_cleanup:
    printf("\nError encountered. AppTask Done.\n");
    while (1) {
        taskYIELD();
    }
}

/* [] END OF FILE */
