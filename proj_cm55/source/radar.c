/****************************************************************************
* File Name        : radar.c
*
* Description      : This file implements the interface with the radar sensor, as
*                    well as the radar sensor ISR to feed the pre-processor.
*
* Related Document : See README.md
*
*****************************************************************************
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
*****************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/

#include "cybsp.h"
#include "mtb_hal_spi.h"
#include "retarget_io_init.h"

#include "ready_models/gesture_lib.h"
#ifdef COMPONENT_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#endif

#include "resource_map.h"

#include "xensiv_bgt60trxx_mtb.h"
#include "xensiv_radar_data_management.h"
#define XENSIV_BGT60TRXX_CONF_IMPL
#include "radar_settings.h"

#include "preprocess.h"
#include "extractions.h"


#include <stdlib.h>

#include "ipc_communication.h"

/*******************************************************************************
* Constants
*******************************************************************************/

/*******************************************************************************
* Global Variables
*******************************************************************************/


/*****************************************************************************
 * Macros
 *****************************************************************************/
#define INIT_SUCCESS            (0UL)
#define INIT_FAILURE            (1UL)
#define XENSIV_BGT60TRXX_IRQ_PRIORITY                      (1U)

#define SPI_INTR_NUM        ((IRQn_Type) CYBSP_SPI_CONTROLLER_IRQ)
#define SPI_INTR_PRIORITY   (2U)

#define XENSIV_BGT60TRXX_SPI_FREQUENCY      (12000000UL)

#define NUM_SAMPLES_PER_FRAME               (XENSIV_BGT60TRXX_CONF_NUM_SAMPLES_PER_CHIRP *\
                                            XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME *\
                                            XENSIV_BGT60TRXX_CONF_NUM_RX_ANTENNAS)

#define NUM_CHIRPS_PER_FRAME                XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME
#define NUM_SAMPLES_PER_CHIRP               XENSIV_BGT60TRXX_CONF_NUM_SAMPLES_PER_CHIRP

/* RTOS tasks */
#define RADAR_TASK_NAME                     "radar_task"
#define RADAR_TASK_STACK_SIZE               (configMINIMAL_STACK_SIZE * 10)
#define RADAR_TASK_PRIORITY                 (configMAX_PRIORITIES - 2)
#define PROCESSING_TASK_NAME                "processing_task"
#define PROCESSING_TASK_STACK_SIZE          (configMINIMAL_STACK_SIZE * 10)
#define PROCESSING_TASK_PRIORITY            (configMAX_PRIORITIES - 1)

/* Interrupt priorities */
#define GPIO_INTERRUPT_PRIORITY             (6)

#define GESTURE_HOLD_TIME                   (10) /* count value used to hold gesture before evaluating new one */
#define GESTURE_DETECTION_THRESHOLD         (0)


/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/
static void radar_task(void *pvParameters);
static void processing_task(void *pvParameters);
static int32_t radar_init(void);
void get_time_from_millisec_radar(unsigned long milliseconds, char* output);



/*******************************************************************************
* Global Variables
********************************************************************************/
cy_en_scb_spi_status_t init_status;
cy_stc_scb_spi_context_t SPI_context;
static volatile bool data_available = false;
cy_stc_sysint_t irq_cfg;
xensiv_bgt60trxx_mtb_t sensor;

static uint16_t bgt60_buffer[NUM_SAMPLES_PER_FRAME] __attribute__((aligned(2)));

static TaskHandle_t radar_task_handler;
static TaskHandle_t processing_task_handler;

float32_t gesture_frame[NUM_SAMPLES_PER_CHIRP * NUM_CHIRPS_PER_FRAME * XENSIV_BGT60TRXX_CONF_NUM_RX_ANTENNAS];

preproc_work_arrays work_arrays;
frame_cfg f_cfg = {
        .n_channels = 3,
        .n_chirps = 32,
        .n_samples = 64,
        .n_range_bins = 32};


volatile bool is_settings_mode = false;
mtb_hal_lptimer_t lptimer_obj;
uint32_t before;
uint32_t after;

/*System tick variables*/
uint32_t tick = 0;
bool radarreset = false;

void xensiv_bgt60trxx_interrupt_handler(void);


/*******************************************************************************
* Function Name: mSPI_Interrupt
********************************************************************************
* Summary: This is the SPI interrupt handler.
*
* Parameters:
*   None
*
* Return:
*   None
*
*
*******************************************************************************/
void mSPI_Interrupt(void)
{
    Cy_SCB_SPI_Interrupt(CYBSP_SPI_CONTROLLER_HW, &SPI_context);
}

/*******************************************************************************
* Function Name: systick_isr
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
void systick_isr(void)
{
    tick++;
}


/*******************************************************************************
* Function Name: deinterleave_antennas
********************************************************************************
* Summary:
* This function de-interleaves multiple antennas data from single radar HW FIFO
*
* Parameters:
*  buffer_ptr - Pointer to the buffer containing radar raw data.
*
* Return:
*  none
*
*******************************************************************************/
void deinterleave_antennas(uint16_t * buffer_ptr)
{
    uint8_t antenna = 0;
    int32_t index = 0;
    static const float norm_factor = 1.0f;

    for (int i = 0; i < (NUM_SAMPLES_PER_CHIRP * NUM_CHIRPS_PER_FRAME * XENSIV_BGT60TRXX_CONF_NUM_RX_ANTENNAS); ++i)
    {
        /* Normalise the data received from multiple antennas into a single buffer */
        gesture_frame[index + antenna * NUM_SAMPLES_PER_CHIRP * NUM_CHIRPS_PER_FRAME] = buffer_ptr[i] * norm_factor;
        antenna++;
        if (antenna == XENSIV_BGT60TRXX_CONF_NUM_RX_ANTENNAS)
        {
            antenna = 0;
            index++;
        }
    }
}


/*******************************************************************************
* Function Name: radar_task
********************************************************************************
* Summary:
* This is the main task.
*    1. Creates a timer to toggle user LED
*    2. Create the processing RTOS task
*    3. Initializes the hardware interface to the sensor and LEDs
*    4. Initializes the radar device
*    5. Initializes gesture library
*    6. In an infinite loop
*       - Waits for interrupt from radar device indicating availability of data
*       - Read from software buffer the raw radar frame
*       - De-interleaves the radar data frame
*       - Acknowledges the radar data manager the consumption of read data
*       - Sends notification to processing task
* Parameters:
*  pvParameters: unused
*
* Return:
*  none
*
*******************************************************************************/
void radar_task(void *pvParameters)
{
    (void)pvParameters;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if (radar_init() != 0)
    {
        CY_ASSERT(0);
    }
    
    if (xTaskCreate(processing_task, PROCESSING_TASK_NAME, PROCESSING_TASK_STACK_SIZE, NULL, PROCESSING_TASK_PRIORITY, &processing_task_handler) != pdPASS)
    {
        CY_ASSERT(0);
    }

    IMAI_AED_init();
    
    /* Init preprocessing */
    work_arrays = new_preproc_work_arrays(&f_cfg);

    /* Inference time measurement */
    Cy_SysTick_Init(CY_SYSTICK_CLOCK_SOURCE_CLK_IMO , (8000000/1000)-1);
    Cy_SysTick_SetCallback(0, systick_isr);

    if (xensiv_bgt60trxx_start_frame(&sensor.dev, true) != XENSIV_BGT60TRXX_STATUS_OK)
    {
        CY_ASSERT(0);
    }

    for(;;)
    {
        if (data_available == true)
        {
            data_available = false;
            if (xensiv_bgt60trxx_get_fifo_data(&sensor.dev, bgt60_buffer, NUM_SAMPLES_PER_FRAME) == XENSIV_BGT60TRXX_STATUS_OK)
            {
                deinterleave_antennas(bgt60_buffer);
                /* Tell processing task to take over */
                xTaskNotifyGive(processing_task_handler);
            }
            else
            {
                printf ("Radar error. Check SPI configuration \r\n");
                  CY_ASSERT(0);
            }    
        }
    }
}
     


/*******************************************************************************
* Function Name: processing_task
********************************************************************************
* Summary:
* This is the data processing task.
*    1. It creates a console task to handle parameter configuration for the library
*    2. In a loop
*       - wait for the frame data available for process
*       - Runs the Gesture algorithm and provides the result
*       - Interprets the results
*
* Parameters:
*  pvParameters: unused
*
* Return:
*  None
*
*******************************************************************************/
void processing_task(void *pvParameters)
{
    (void)pvParameters;
    int model_out[IMAI_DATA_OUT_COUNT] = {0};
    const char* class_map[] = IMAI_DATA_OUT_SYMBOLS;
    const float norm_mean[IMAI_DATA_OUT_COUNT] = {9.26814552650607, 4.391583164927378, 0.27332462978312866, -0.02838213175529301, 0.00026668613549266876};
    const float norm_scale[IMAI_DATA_OUT_COUNT] = {5.801363069954616, 7.547439540930497, 0.5629401789624862, 0.41502512890635995, 0.0007474111364241666};

    for(;;)
    {
        /* Wait for frame data available to process */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        /* pass on the de-interleaved data on to Algorithmic kernel */

        float model_in[IMAI_DATA_IN_COUNT];
        uint16_t min_range_bin = 3;
        slim_algo_output res;
        slim_algo(&res, gesture_frame, &f_cfg, min_range_bin, &work_arrays);
        model_in[0] = ((float)res.detection.range_bin - norm_mean[0]) / norm_scale[0];
        model_in[1] = ((float)res.detection.doppler_bin - norm_mean[1]) / norm_scale[1];
        model_in[2] = ((float)res.detection.azimuth - norm_mean[2]) / norm_scale[2];
        model_in[3] = ((float)res.detection.elevation - norm_mean[3]) / norm_scale[3];
        model_in[4] = ((float)res.detection.value - norm_mean[4]) / norm_scale[4];

        /* Input the processed radar to model */
        int imai_result_enqueue = IMAI_AED_enqueue(model_in);
        if (IMAI_RET_SUCCESS != imai_result_enqueue)
        {
            printf("Insufficient memory to enqueue sensor data. Inferencing is not keeping up.\n");
        }

        /* Get model results */
        int imai_result = IMAI_AED_dequeue(model_out);
        int pred_idx = 0;
        static int prediction_count = 0;

        /* LED variables */
        static int led_off = 0;
        int ledon_t = 0;

        switch (imai_result)
        {
            static uint8_t success_flag;
            case IMAI_RET_SUCCESS:
                ipc_payload_t* payload = cm55_ipc_get_payload_ptr();

                success_flag = 1;
                prediction_count += 1;

                for (uint8_t i = 0; i < IMAI_DATA_OUT_COUNT; i++)
                {
                    if (model_out[i] == 1)
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
                    ledon_t = tick;
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
                    if((tick - ledon_t) > 500)
                    {
                        /* turn on LED */
                        // Do not control the LED:
                        // Cy_GPIO_Write(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_PIN, CYBSP_LED_STATE_OFF);
                        ;
                    }
                    led_off = 1;
                }

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



/*******************************************************************************
* Function Name: get_time_from_millisec_radar
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
void get_time_from_millisec_radar(unsigned long milliseconds, char* timeString) {
  unsigned int seconds = (milliseconds / 1000) % 60;
  unsigned int minutes = (milliseconds / (1000 * 60)) % 60;
  unsigned int hours = (milliseconds / (1000 * 60 * 60));
  sprintf(timeString, "%02u:%02u:%02u", hours, minutes, seconds);
}


/*******************************************************************************
* Function Name: radar_init
********************************************************************************
* Summary:
* This function configures the SPI interface, initializes radar and interrupt
* service routine to indicate the availability of radar data.
*
* Parameters:
*  void
*
* Return:
*  Success or error
*
*******************************************************************************/
static int32_t radar_init(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    uint32_t status = INIT_SUCCESS;
    /* Enable the RADAR. */
    sensor.iface.scb_inst = CYBSP_SPI_CONTROLLER_HW;
    sensor.iface.spi = &SPI_context;
    sensor.iface.sel_port = CYBSP_RSPI_CS_PORT;
    sensor.iface.sel_pin = CYBSP_RSPI_CS_PIN;
    sensor.iface.rst_port = CYBSP_RADAR_RESET_PORT;
    sensor.iface.rst_pin = CYBSP_RADAR_RESET_PIN;
    sensor.iface.irq_port = CYBSP_RADAR_INT_PORT;
    sensor.iface.irq_pin = CYBSP_RADAR_INT_PIN;
    sensor.iface.irq_num = CYBSP_RADAR_INT_IRQ;
        
    irq_cfg.intrSrc = sensor.iface.irq_num;
    irq_cfg.intrPriority = XENSIV_BGT60TRXX_IRQ_PRIORITY;

    init_status = Cy_SCB_SPI_Init(CYBSP_SPI_CONTROLLER_HW, &CYBSP_SPI_CONTROLLER_config, &SPI_context);

    /* If the initialization fails, update status */
    if ( CY_SCB_SPI_SUCCESS != init_status )
    {
        status = INIT_FAILURE;
    }

    if (status == INIT_SUCCESS)
    {
        cy_stc_sysint_t spiIntrConfig =
        {
            .intrSrc      = SPI_INTR_NUM,
            .intrPriority = SPI_INTR_PRIORITY,
        };

        Cy_SysInt_Init(&spiIntrConfig, &mSPI_Interrupt);
        NVIC_EnableIRQ(SPI_INTR_NUM);

        /* Set active target select to line 0 */
        Cy_SCB_SPI_SetActiveSlaveSelect(CYBSP_SPI_CONTROLLER_HW, CY_SCB_SPI_SLAVE_SELECT1);
        /* Enable SPI Controller block. */
        Cy_SCB_SPI_Enable(CYBSP_SPI_CONTROLLER_HW);
    }
    
    /* Reduce drive strength to improve EMI */
    Cy_GPIO_SetSlewRate(CYBSP_RSPI_MOSI_PORT, CYBSP_RSPI_MOSI_PIN, CY_GPIO_SLEW_FAST);
    Cy_GPIO_SetDriveSel(CYBSP_RSPI_MOSI_PORT, CYBSP_RSPI_MOSI_PIN, CY_GPIO_DRIVE_1_8);
    Cy_GPIO_SetSlewRate(CYBSP_RSPI_CLK_PORT, CYBSP_RSPI_CLK_PIN, CY_GPIO_SLEW_FAST);
    Cy_GPIO_SetDriveSel(CYBSP_RSPI_CLK_PORT, CYBSP_RSPI_CLK_PIN, CY_GPIO_DRIVE_1_8);

    result = xensiv_bgt60trxx_mtb_init(&sensor, register_list, XENSIV_BGT60TRXX_CONF_NUM_REGS);
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    result = xensiv_bgt60trxx_mtb_interrupt_init(&sensor, NUM_SAMPLES_PER_FRAME);
    CY_ASSERT(result == CY_RSLT_SUCCESS);
    
    Cy_SysInt_Init(&irq_cfg, xensiv_bgt60trxx_interrupt_handler);

    NVIC_ClearPendingIRQ(irq_cfg.intrSrc);
    NVIC_EnableIRQ(irq_cfg.intrSrc);

    Cy_GPIO_ClearInterrupt(CYBSP_RADAR_INT_PORT, CYBSP_RADAR_INT_NUM);
    NVIC_ClearPendingIRQ(irq_cfg.intrSrc);

    Cy_SysLib_Delay(1000);

    return 0;
}


/*******************************************************************************
* Function Name: xensiv_bgt60trxx_interrupt_handler
********************************************************************************
* Summary:
* This is the interrupt handler to react on sensor indicating the availability
* of new data
*    1. Triggers the radar data manager for buffering radar data into software buffer.
*
* Parameters:
*  void
*
* Return:
*  none
*
*******************************************************************************/
void xensiv_bgt60trxx_interrupt_handler(void)
{
    data_available = true;
    Cy_GPIO_ClearInterrupt(CYBSP_RADAR_INT_PORT, CYBSP_RADAR_INT_NUM);
    NVIC_ClearPendingIRQ(irq_cfg.intrSrc);
}

/*******************************************************************************
 * Function Name: create_radar_task
 ********************************************************************************
 * Summary:
 *  Function that creates the radar sensor task.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  CY_RSLT_SUCCESS upon successful creation of the radar sensor task, else a
 *  non-zero value that indicates the error.
 *
 *******************************************************************************/
cy_rslt_t create_radar_task(void)
{
    BaseType_t status;

    #ifdef CM55_ENABLE_STARTUP_PRINTS
    printf("****************** DEEPCRAFT Ready Model: gesture ****************** \r\n\n");
    #endif

    /* Create the RTOS task */
    status = xTaskCreate(radar_task, RADAR_TASK_NAME,
                         RADAR_TASK_STACK_SIZE, NULL,
                         RADAR_TASK_PRIORITY, &radar_task_handler);

    return (pdPASS == status) ? CY_RSLT_SUCCESS : (cy_rslt_t) status;
}

/* [] END OF FILE */

