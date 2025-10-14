/* SPDX-License-Identifier: MIT
 * Copyright (C) 2025 Avnet
 * Authors: Nikola Markovic <nikola.markovic@avnet.com>, Shu Liu <shu.liu@avnet.com> et al.
 */

#include "cybsp.h"
#include <string.h>

#include "cy_syslib.h" // for Cy_SysLib_GetUniqueId

#include "FreeRTOS.h"

#include "retarget_io_init.h"
#include "ipc_communication.h"

#include "wifi_config.h"
#include "wifi_app.h"

#include "iotconnect.h"

#include "app_config.h"



#define APP_VERSION_BASE "1.1.0"

// Defined in common.mk then dereference in this Makefile with DEFINES+=
#if defined(COUGH_MODEL)
#define APP_VERSION ("C-" APP_VERSION_BASE)
#elif defined(ALARM_MODEL)
#define APP_VERSION ("A-" APP_VERSION_BASE)
#elif defined(BABYCRY_MODEL)
#define APP_VERSION ("B-" APP_VERSION_BASE)
#elif defined(DIRECTIONOFARRIVAL_MODEL)
#define APP_VERSION ("D-" APP_VERSION_BASE)
#elif defined(FALLDETECTION_MODEL)
#define APP_VERSION ("F-" APP_VERSION_BASE)
#elif defined(GESTURE_MODEL)
#define APP_VERSION ("B-" APP_VERSION_BASE)
#else
#define APP_VERSION ("?-" APP_VERSION_BASE)
#endif

static bool is_demo_mode = false;
static int reporting_interval = 2000;

/////////////////////////////////////////////////////////////////////////////

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
    
    // DO NOT print anything before we receive a message to avoice garbled output

    // we want to wait for CM33 to start receiving messages to prevent halts and errors below.
    while (!cm33_ipc_has_received_message()) {
        taskYIELD(); // wait for CM55
    }
    printf("App Task: CM55 IPC is ready. Resuming the application...\n");

    char iotc_duid[IOTCL_CONFIG_DUID_MAX_LEN] = IOTCONNECT_DUID;
    if (0 == strlen(iotc_duid)) {
        uint64_t hwuid = Cy_SysLib_GetUniqueId();
        uint32_t hwuidhi = (uint32_t)(hwuid >> 32);
        // not using low bytes in the name because they appear to be the same across all boards of the same type
        // feel free to modify the application to use these bytes
        // uint32_t hwuidlo = (uint32_t)(hwuid & 0xFFFFFFFF);
        sprintf(iotc_duid, IOTCONNECT_DUID_PREFIX"%08lx", (unsigned long) hwuidhi);
        printf("Generated device unique ID (DUID) is: %s\n", iotc_duid);
    }

    if (strlen(IOTCONNECT_DEVICE_CERT) == 0) {
		printf("ERROR: Device certificate is missing. Please configure the /IOTCONNECT credentials in app_config.h\n");
        goto exit_cleanup;
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

    // This will not return if it fails
    wifi_app_connect();

    cy_rslt_t ret = iotconnect_sdk_init(&config);
    if (CY_RSLT_SUCCESS != ret) {
        printf("Failed to initialize the IoTConnect SDK. Error code: %u\n", (unsigned int) ret);
        goto exit_cleanup;
    }

    for (int i = 0; i < 10; i++) {
        ret = iotconnect_sdk_connect();
        if (CY_RSLT_SUCCESS != ret) {
            printf("Failed to initialize the IoTConnect SDK. Error code: %u\n", (unsigned int) ret);
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
