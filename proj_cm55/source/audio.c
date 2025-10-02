/****************************************************************************
* File Name        : audio.c
*
* Description      : This file implements the interface with the PDM, as
*                    well as the PDM ISR to feed the pre-processor.
*
* Related Document : See README.md
*
*****************************************************************************
* Copyright 2025, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*****************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/

#include "audio.h"
#include "cy_syslib.h"
#include <time.h>

#ifdef DIRECTIONOFARRIVAL_MODEL
#include "ready_models/audio_data.h"
#endif

#ifdef COMPONENT_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#endif

#include "ipc_communication.h"

/*****************************************************************************
 * Macros
 *****************************************************************************/
#define SYSTICK_MAX_CNT (0xFFFFFF)

#define PDM_CHANNEL 3
/* Define how many samples in a frame */
#define FRAME_SIZE                  (1024)

/* Desired sample rate. Typical values: 8/16/22.05/32/44.1/48kHz */
#define SAMPLE_RATE_HZ              16000u

/* Decimation Rate of the PDM/PCM block. Typical value is 64 */
#define DECIMATION_RATE             64u

#define DETECTCOUNT                 10
#define LED_STOP_COUNT              500

/* PDM PCM hardware FIFO size */
#define HW_FIFO_SIZE                            (64u)

/* Rx FIFO trigger level/threshold configured by user */
#define RX_FIFO_TRIG_LEVEL                      (HW_FIFO_SIZE/2)

/* Total number of interrupts to get the FRAME_SIZE number of samples*/
#define NUMBER_INTERRUPTS_FOR_FRAME             (FRAME_SIZE/RX_FIFO_TRIG_LEVEL)

/* Multiplication factor of the input signal.
 * This should ideally be 1. Higher values will have a negative impact on
 * the sampling dynamic range. However, it can be used as a last resort
 * when MICROPHONE_GAIN is already at maximum and the ML model was trained
 * with data at a higher amplitude than the microphone captures.
 * Note: If you use the same board for recording training data and
 * deployment of your own ML model set this to 1.0. */
#define DIGITAL_BOOST_FACTOR                    10.0f

/* Specifies the dynamic range in bits.
 * PCM word length, see the A/D specific documentation for valid ranges. */
 #define AUIDO_BITS_PER_SAMPLE                  16

/* Converts given audio sample into range [-1,1] */
#define SAMPLE_NORMALIZE(sample)                (((float) (sample)) / (float) (1 << (AUIDO_BITS_PER_SAMPLE - 1)))

/* PDM/PCM Pins */
#define PDM_DATA                    P8_6
#define PDM_CLK                     P8_5

/* PDM PCM interrupt configuration parameters */
const cy_stc_sysint_t PDM_IRQ_cfg =
{
    .intrSrc = (IRQn_Type)CYBSP_PDM_CHANNEL_3_IRQ,
    .intrPriority = 2
};

/* RTOS tasks */
#define AUDIO_TASK_NAME                      "audio_task"
#define AUDIO_TASK_STACK_SIZE                (configMINIMAL_STACK_SIZE * 10)
#define AUDIO_TASK_PRIORITY                  (configMAX_PRIORITIES - 1)

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/
static void pdm_pcm_event_handler(void);

/*******************************************************************************
* Global Variables
********************************************************************************/
volatile long tick1 = 0;

/* Set up one buffer for data collection and one for processing */
int16_t audio_buffer0[FRAME_SIZE] = {0};
int16_t audio_buffer1[FRAME_SIZE] = {0};
int16_t* active_rx_buffer;
int16_t* full_rx_buffer;

/* Model Output variable */
int data_out[IMAI_DATA_OUT_COUNT] = {0};
static const char* LABELS[IMAI_DATA_OUT_COUNT] = IMAI_DATA_OUT_SYMBOLS;

/* Task handler */
static TaskHandle_t audio_task_handler;

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
* Function Name: audio_init
********************************************************************************
* Summary:
*    A function used to initialize and configure the PDM based on the shield
*    selected in the Makefile. Starts an asynchronous read which triggers an
*    interrupt when completed.
*
* Parameters:
*   None
*
* Return:
*     The status of the initialization.
*
*
*******************************************************************************/
cy_rslt_t audio_init(void)
{
    cy_rslt_t result;

    /* Initialize PDM PCM block */
    result = Cy_PDM_PCM_Init(CYBSP_PDM_HW, &CYBSP_PDM_config);
    if(CY_PDM_PCM_SUCCESS != result)
    {
        return result;
    }

    /* Initialize and enable PDM PCM channel 3 -Right */
    Cy_PDM_PCM_Channel_Init(CYBSP_PDM_HW, &channel_3_config, PDM_CHANNEL);
    Cy_PDM_PCM_Channel_Enable(CYBSP_PDM_HW, PDM_CHANNEL);

    /* An interrupt is registered for right channel, clear and set masks for it. */
    Cy_PDM_PCM_Channel_ClearInterrupt(CYBSP_PDM_HW, PDM_CHANNEL, CY_PDM_PCM_INTR_MASK);
    Cy_PDM_PCM_Channel_SetInterruptMask(CYBSP_PDM_HW, PDM_CHANNEL, CY_PDM_PCM_INTR_MASK);

    /* Register the IRQ handler */
    result = Cy_SysInt_Init(&PDM_IRQ_cfg, &pdm_pcm_event_handler);
    if(CY_SYSINT_SUCCESS != result)
    {
        return result;
    }
    NVIC_ClearPendingIRQ(PDM_IRQ_cfg.intrSrc);
    NVIC_EnableIRQ(PDM_IRQ_cfg.intrSrc);

    /* Set up pointers to two buffers to implement a ping-pong buffer system.
    * One gets filled by the PDM while the other can be processed. */
    active_rx_buffer = audio_buffer0;
    full_rx_buffer = audio_buffer1;


    Cy_PDM_PCM_Activate_Channel(CYBSP_PDM_HW, PDM_CHANNEL);

    /*timer set up*/
    Cy_SysTick_Init(CY_SYSTICK_CLOCK_SOURCE_CLK_IMO , (8000000/1000)-1);
    Cy_SysTick_SetCallback(0, systick_isr1);        /* point to SysTick ISR to increment the 1ms count */

    return 0;
}


/*******************************************************************************
* Function Name: audio_task
********************************************************************************
* Summary:
* This is the main task.
*    1. Initializes the PDM/PCM block.
*    2. Wait for the frame data available for process.
*    3. Runs the model and provides the result.
* Parameters:
*  pvParameters : unused
*
* Return:
*  none
*
*******************************************************************************/
void audio_task(void *pvParameters)
{
    cy_rslt_t result;
    /* LED variables */
    static int led_off = 0;
    static int led_on = 0;
    int label_scores[IMAI_DATA_OUT_COUNT];
    static int prediction_count = 0;
    static int16_t success_flag = 0;
    
    /* Initialize DEEPCRAFT pre-processing library */
    IMAI_AED_init();
    
    /* If the model selected is for baby cry detection, set the confidence threshold to 0.7 */
    #ifdef BABYCRY_MODEL
    struct PP_config postprocessing;
    postprocessing.confidence = 0.7;
    IMAI_AED_sensitivity(postprocessing);
    #endif
    
    
    result = audio_init();
    if(result != 0)
    {
        CY_ASSERT(0);
    }

    unsigned long led_start_t = tick1;

    for(;;)
    {
        /* Wait here until ISR notifies us */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        for (uint32_t index = 0; index < FRAME_SIZE; index++)
        {

            int16_t val_temp = full_rx_buffer[index];

            /*convert int to float*/
            float data_in = SAMPLE_NORMALIZE(val_temp) * DIGITAL_BOOST_FACTOR;

            if (data_in > 1.0)
            {
                data_in = 1.0f;
            }
            else if (data_in < -1.0)
            {
               data_in = -1.0f;
            }

            /*pass audio sample for enqueue*/
            result = IMAI_AED_enqueue(&data_in);

            switch(IMAI_AED_dequeue(label_scores))
            {
                case IMAI_RET_SUCCESS:
                    ipc_payload_t* payload = cm55_ipc_get_payload_ptr();

                    success_flag = 1;
                    prediction_count += 1;
                    if (label_scores[1] == 1)
                    {
                        payload->label_id = 1;
                        strcpy(payload->label, LABELS[1]);
                        /* New line when LED from off to on */
                        if ((led_off - CYBSP_LED_STATE_ON) > 0)
                        {
                            printf("\r\n");
                        }

                        /* Print triggered class and the triggered time since IMAI init.*/
                        unsigned long t = tick1 - led_start_t;
                        char timeString[9];
                        get_time_from_millisec_audio(t, timeString);
                        printf("%s %s\r\n",LABELS[1],timeString);

                        Cy_GPIO_Write(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_PIN, CYBSP_LED_STATE_ON);
                        led_off = 0;
                        led_on = tick1;
                    }
                    else
                    {
                        payload->label_id = 0;
                        strcpy(payload->label, LABELS[0]);
                        /* Only print non-label class very 10 predictions */
                        if (prediction_count>DETECTCOUNT)
                        {
                            printf(".");
                            fflush( stdout );
                            prediction_count = 0;
                        }
                        /* Turn off LED after the LED is on for 500ms */
                        if((tick1 - led_on) > LED_STOP_COUNT)
                        {
                            Cy_GPIO_Write(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_PIN, CYBSP_LED_STATE_OFF);
                        }
                        led_off = 1;
                    }
                    cm55_ipc_send_to_cm33();
                    break;
                    
                    case IMAI_RET_NOMEM:
                        /* Something went wrong, stop the program */
                        printf("Unable to perform inference. Internal memory error.\r\n");
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
 * Function Name: create_audio_task
 ********************************************************************************
 * Summary:
 *  Function that creates the audio task.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  CY_RSLT_SUCCESS upon successful creation of the radar sensor task, else a
 *  non-zero value that indicates the error.
 *
 *******************************************************************************/
cy_rslt_t create_audio_task(void)
{
    BaseType_t status;
    printf("****************** DEEPCRAFT Ready Model: %s ****************** \r\n\n", LABELS[1]);

    /* Create the RTOS task */
    status = xTaskCreate(audio_task, AUDIO_TASK_NAME, AUDIO_TASK_STACK_SIZE, NULL, AUDIO_TASK_PRIORITY, &audio_task_handler);


    return (pdPASS == status) ? CY_RSLT_SUCCESS : (cy_rslt_t) status;
}


/*******************************************************************************
* Function Name: pdm_pcm_event_handler
********************************************************************************
* Summary:
*  PDM/PCM ISR handler. A flag is set when the interrupt is generated.
*
* Parameters:
*  arg: not used
*  event: event that occurred
*
*******************************************************************************/
static void pdm_pcm_event_handler(void)
{
    /* Used to track how full the buffer is */
    static uint16_t frame_counter = 0;
    
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Check the interrupt status */
    uint32_t intr_status = Cy_PDM_PCM_Channel_GetInterruptStatusMasked(CYBSP_PDM_HW, PDM_CHANNEL);
    if(CY_PDM_PCM_INTR_RX_TRIGGER & intr_status)
    {
        /* Move data from the PDM fifo and place it in a buffer */
        for(uint32_t index=0; index < RX_FIFO_TRIG_LEVEL; index++)
        {
            int32_t data = (int32_t)Cy_PDM_PCM_Channel_ReadFifo(CYBSP_PDM_HW, PDM_CHANNEL);
            active_rx_buffer[frame_counter * RX_FIFO_TRIG_LEVEL + index] = (int16_t)(data);
        }
        Cy_PDM_PCM_Channel_ClearInterrupt(CYBSP_PDM_HW, PDM_CHANNEL, CY_PDM_PCM_INTR_RX_TRIGGER);
        frame_counter++;
    }

    /* Check if the buffer is full */
    if((NUMBER_INTERRUPTS_FOR_FRAME) <= frame_counter)
    {
        /* Flip the active and the next rx buffers */
        int16_t* temp = active_rx_buffer;
        active_rx_buffer = full_rx_buffer;
        full_rx_buffer = temp;

        /* Send a task notification to the task */
        vTaskNotifyGiveFromISR(audio_task_handler, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        frame_counter = 0;

    }

    /* Clear the remaining interrupts */
    if((CY_PDM_PCM_INTR_RX_FIR_OVERFLOW | CY_PDM_PCM_INTR_RX_OVERFLOW|
            CY_PDM_PCM_INTR_RX_IF_OVERFLOW | CY_PDM_PCM_INTR_RX_UNDERFLOW) & intr_status)
    {
        Cy_PDM_PCM_Channel_ClearInterrupt(CYBSP_PDM_HW, PDM_CHANNEL, CY_PDM_PCM_INTR_MASK);
    }

}


/* [] END OF FILE */
