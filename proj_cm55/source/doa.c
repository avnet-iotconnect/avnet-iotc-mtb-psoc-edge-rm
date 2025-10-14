/******************************************************************************
 * File Name:   doa.c
 *
 * Description: This file contains the task that initializes and configures the
 *              Direction of Arrival model.
 * 
 * Direction of Arrival model requires PDM data from four different mics pointing
 * to four different directions. Since the PSOC Edge AI kit hardware does not 
 * support this, the sample audio data is being passed to model for detecting
 * the direction of sound source. 
 *
 * The sample audio data shows the sound coming from "South" direction. 
 *
 * Related Document: See README.md
 *
 *
 *******************************************************************************
* (c) 2025, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
* 
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
 *******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "doa.h"
#include "audio_data.h"
#include "cy_result.h"
#include "retarget_io_init.h"

#include "ipc_communication.h"

/*******************************************************************************
 * Global Variables
 ********************************************************************************/

volatile long tick1 = 0;

/* DOA task handle */
static TaskHandle_t doa_task_handle;


/******************************************************************************
 * Macros
 ******************************************************************************/
#define DELAY_MS              (200)
/* Task priority and stack size for the DOA task */
#define TASK_DOA_PRIORITY     (configMAX_PRIORITIES - 1)
#define TASK_DOA_STACK_SIZE   (1024U)
/* I2C Clock frequency in Hz */
#define I2C_CLK_FREQ_HZ       (400000U)

#define DETECTCOUNT           10
#define LED_STOP_COUNT        500

/*******************************************************************************
* Function Name: systick_isr1
********************************************************************************
* Summary: This increments every time the SysTick counter decrements to 0.
*
* Parameters:
*   None
*
* Return:
*   None
*
*
*******************************************************************************/
void systick_isr1(void)
{
    tick1++;
}


/*******************************************************************************
* Function Name: get_time_from_millisec_audio
********************************************************************************
* Summary: This function prints the time when a output class is detected.
*
* Parameters:
*   milliseconds : time when a output class is detected.
*   timeString   : time of detected class in hr:m:s format.
*
* Return:
*   None
*
*
*******************************************************************************/
void get_time_from_millisec_audio(unsigned long milliseconds, char* timeString) {
  unsigned int seconds = (milliseconds / 1000) % 60;
  unsigned int minutes = (milliseconds / (1000 * 60)) % 60;
  unsigned int hours = (milliseconds / (1000 * 60 * 60));
  sprintf(timeString, "%02u:%02u:%02u", hours, minutes, seconds);
}

/*******************************************************************************
 * Function Name: doa_init
 ********************************************************************************
 * Summary:
 *  Function that configures the Systick timer for time keeping.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *
 *******************************************************************************/
static void doa_init(void)
{
    /* timer set up */
    Cy_SysTick_Init(CY_SYSTICK_CLOCK_SOURCE_CLK_IMO , (8000000/1000)-1);
    Cy_SysTick_SetCallback(0, systick_isr1);        /* point to SysTick ISR to increment the 1ms count */

    /* Initialize DEEPCRAFT pre-processing library */
    IMAI_DOA_init();
}


/*******************************************************************************
 * Function Name: doa_task
 ********************************************************************************
 * Summary:
 *  Task that processes the sample PDM data to detect the direction of sound.
 *
 * Parameters:
 *  void *pvParameters : Task parameter defined during task creation (unused)
 *
 * Return:
 *  None
 *
 *******************************************************************************/
static void doa_task(void* pvParameters)
{
    cy_rslt_t result;
    /* LED variables */
    static int led_off = 0;
    static int led_on = 0;
    static int send_data = 0;
    int label_scores[IMAI_DATAOUT_COUNT];
    static int prediction_count = 0;

    /* Initialize SysTick timer */
    doa_init();

    unsigned long start_t = tick1;
    const char* class_map[] = IMAI_DATAOUT_SYMBOLS;

    for (;;)
    {
        int num_rows = sizeof(audio_data) / sizeof(audio_data[0]);
        int row_size = sizeof(audio_data[0]) / sizeof(audio_data[0][0]);
        uint8 pred_idx =  0;
        
        for (int i = 0; i < num_rows; i++)
        {
            float data_in[4];
            for (int j = 0; j < row_size; j++)
            {
                data_in[j] = (float)audio_data[i][j];
            }
            result = IMAI_DOA_enqueue(data_in);

            switch (IMAI_DOA_dequeue(label_scores))
            {
                static uint8_t success_flag;
                case IMAI_RET_SUCCESS:
                    ipc_payload_t* payload = cm55_ipc_get_payload_ptr();

                    success_flag = 1;
                    prediction_count += 1;

                    for (uint8_t i = 0; i < IMAI_DATAOUT_COUNT; i++)
                    {
                        if (label_scores[i] == 1)
                        {
                            pred_idx = i;
                        }
                    }
                    
                    payload->label_id = pred_idx;
                    strcpy(payload->label, class_map[pred_idx]);
                    cm55_ipc_send_to_cm33();

                    if (pred_idx != 0)
                    {
                        if ((led_off - CYBSP_LED_STATE_ON) > 0)
                        {
                            printf("\r\n");
                        }
                        /* print triggered class and the triggered time since IMAI Initial. */
                        printf("%s\n", class_map[pred_idx]);
                        // Do not control the LED:
                        // Cy_GPIO_Write(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_PIN, CYBSP_LED_STATE_ON);
                        led_off = 0;
                        led_on = tick1;
                    }
                    else
                    {
                        /* only print non-label class very 10 predictions */
                        if (prediction_count>9)
                        {
                            printf(".");
                            fflush( stdout );
                            prediction_count = 0;
                        }
                        /* turn off LED after the LED is on for 500ms */
                        if((tick1 - led_on) > 500)
                        {
                            /* turn on green LED */
                            // Do not control the LED:
                            // Cy_GPIO_Write(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_PIN, CYBSP_LED_STATE_OFF);
                        }
                        led_off = 1;
                    }

                    break;

                case IMAI_RET_NOMEM:
                    /* Something went wrong, stop the program */
                    printf("Unable to perform inference. Internal memory error.\n");
                    break;
                case IMAI_RET_TIMEDOUT:
                    if (success_flag == 1)
                        {
                        printf("The evaluation period has ended. Please rerun the evaluation or purchase a license for the ready model.\r\n");
                        }
                        success_flag = 0;
                    break;

            }
        }
    }

}

/*******************************************************************************
 * Function Name: create_doa_task
 ********************************************************************************
 * Summary:
 *  Function that creates the Direction of arrival task.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  CY_RSLT_SUCCESS upon successful creation of the DOA task, else a
 *  non-zero value that indicates the error.
 *
 *******************************************************************************/
cy_rslt_t create_doa_task(void)
{
    BaseType_t status;
    
    #ifdef CM55_ENABLE_STARTUP_PRINTS
    printf("****************** DEEPCRAFT Ready Model: Direction of arrival ****************** \r\n\n");
    #endif

    status = xTaskCreate(doa_task, "DOA Task", TASK_DOA_STACK_SIZE,
            NULL, TASK_DOA_PRIORITY, &doa_task_handle);

    return (pdPASS == status) ? CY_RSLT_SUCCESS : (cy_rslt_t) status;
}

/* [] END OF FILE */
