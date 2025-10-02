/******************************************************************************
* File Name:   audio.c
*
* Description: This file implements the interface with the PDM, as
*              well as the PDM ISR to feed the audio processing block.
*
* Related Document: See README.md
*
*
********************************************************************************
* Copyright 2024-2025, Cypress Semiconductor Corporation (an Infineon company) or
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
*******************************************************************************/
#include "cybsp.h"

#include "audio.h"
#include "baby_cry.h"
#include <math.h>

#include "ipc_communication.h"

/******************************************************************************
 * Macros
 *****************************************************************************/
/* PDM PCM interrupt priority */
#define PDM_PCM_ISR_PRIORITY                    (2u)

/* Channel Index */
#define RIGHT_CH_INDEX                          (3u)
#define RIGHT_CH_CONFIG                         channel_3_config

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

/* Threshold for the output score to be considered a valid detection.
*  The threshold can be adjusted to increase or decrease the sensitivity of the
 * detection. A lower value will result in more false positives, while a higher
 * value will result in more false negatives. */
#define OUTPUT_THRESHOLD_SCORE                  (0.6f)

/******************************************************************************
 * Global Variables
 *****************************************************************************/
/* Set up one buffer for data collection and one for processing */
static int16_t audio_buffer0[FRAME_SIZE] = {0};
static int16_t audio_buffer1[FRAME_SIZE] = {0};
static int16_t* active_rx_buffer;
static int16_t* full_rx_buffer;

/* PDM PCM interrupt configuration parameters */
static const cy_stc_sysint_t PDM_IRQ_cfg =
{
    .intrSrc = (IRQn_Type)CYBSP_PDM_CHANNEL_3_IRQ,
    .intrPriority = PDM_PCM_ISR_PRIORITY
};

/* Flag to check if the data from PDM/PCM block is ready for processing. */
static volatile bool pdm_pcm_flag;

/*******************************************************************************
* Local Function Prototypes
*******************************************************************************/
static void pdm_pcm_event_handler(void);

/*******************************************************************************
* Function Definitions
*******************************************************************************/

/*******************************************************************************
* Function Name: pdm_init
********************************************************************************
* Summary:
*  A function used to initialize and configure the PDM. Sets up an interrupt
*  to trigger when the PDM FIFO level passes the trigger level.
*
* Parameters:
*  None
*
* Return:
*  The status of the initialization.
*
*******************************************************************************/
cy_rslt_t pdm_init(void)
{
    cy_rslt_t result;

    /* Initialize PDM PCM block */
    result = Cy_PDM_PCM_Init(CYBSP_PDM_HW, &CYBSP_PDM_config);
    if(CY_PDM_PCM_SUCCESS != result)
    {
        return result;
    }

    /* Initialize and enable PDM PCM channel 3 -Right */
    Cy_PDM_PCM_Channel_Init(CYBSP_PDM_HW, &RIGHT_CH_CONFIG, (uint8_t)RIGHT_CH_INDEX);
    Cy_PDM_PCM_Channel_Enable(CYBSP_PDM_HW, RIGHT_CH_INDEX);

    /* An interrupt is registered for right channel, clear and set masks for it. */
    Cy_PDM_PCM_Channel_ClearInterrupt(CYBSP_PDM_HW, RIGHT_CH_INDEX, CY_PDM_PCM_INTR_MASK);
    Cy_PDM_PCM_Channel_SetInterruptMask(CYBSP_PDM_HW, RIGHT_CH_INDEX, CY_PDM_PCM_INTR_MASK);

    /* Register the IRQ handler */
    result = Cy_SysInt_Init(&PDM_IRQ_cfg, &pdm_pcm_event_handler);
    if(CY_SYSINT_SUCCESS != result)
    {
        return result;
    }
    NVIC_ClearPendingIRQ(PDM_IRQ_cfg.intrSrc);
    NVIC_EnableIRQ(PDM_IRQ_cfg.intrSrc);

    /* Global variable used to determine if PDM data is available */
    pdm_pcm_flag = false;

    /* Set up pointers to two buffers to implement a ping-pong buffer system.
     * One gets filled by the PDM while the other can be processed. */
    active_rx_buffer = audio_buffer0;
    full_rx_buffer = audio_buffer1;

    Cy_PDM_PCM_Activate_Channel(CYBSP_PDM_HW, RIGHT_CH_INDEX);

    return CY_RSLT_SUCCESS;
}

/*******************************************************************************
* Function Name: pdm_pcm_event_handler
********************************************************************************
* Summary:
*  PDM/PCM ISR handler. Check the interrupt status and clears it.
*  Fills a buffer and then swaps that buffer with an empty one.
*  Once a buffer is full, a flag is set which is used in main.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
static void pdm_pcm_event_handler(void)
{
    /* Used to track how full the buffer is */
    static uint16_t frame_counter = 0;

    /* Check the interrupt status */
    uint32_t intr_status = Cy_PDM_PCM_Channel_GetInterruptStatusMasked(CYBSP_PDM_HW, RIGHT_CH_INDEX);
    if(CY_PDM_PCM_INTR_RX_TRIGGER & intr_status)
    {
        /* Move data from the PDM fifo and place it in a buffer */
        for(uint32_t index=0; index < RX_FIFO_TRIG_LEVEL; index++)
        {
            int32_t data = (int32_t)Cy_PDM_PCM_Channel_ReadFifo(CYBSP_PDM_HW, RIGHT_CH_INDEX);
            active_rx_buffer[frame_counter * RX_FIFO_TRIG_LEVEL + index] = (int16_t)(data);
        }
        Cy_PDM_PCM_Channel_ClearInterrupt(CYBSP_PDM_HW, RIGHT_CH_INDEX, CY_PDM_PCM_INTR_RX_TRIGGER);
        frame_counter++;
    }
    /* Check if the buffer is full */
    if((NUMBER_INTERRUPTS_FOR_FRAME) <= frame_counter)
    {
        /* Flip the active and the next rx buffers */
        int16_t* temp = active_rx_buffer;
        active_rx_buffer = full_rx_buffer;
        full_rx_buffer = temp;

        /* Set the PDM_PCM flag as true, signaling there is data ready for use */
        pdm_pcm_flag = true;
        frame_counter = 0;
    }

    if((CY_PDM_PCM_INTR_RX_FIR_OVERFLOW | CY_PDM_PCM_INTR_RX_OVERFLOW|
            CY_PDM_PCM_INTR_RX_IF_OVERFLOW | CY_PDM_PCM_INTR_RX_UNDERFLOW) & intr_status)
    {
        Cy_PDM_PCM_Channel_ClearInterrupt(CYBSP_PDM_HW, RIGHT_CH_INDEX, CY_PDM_PCM_INTR_MASK);
    }
}

/*******************************************************************************
* Function Name: pdm_data_process
********************************************************************************
* Summary:
*  This function feeds the data to the DEEPCRAFT pre-processor and returns the
*  processed results.
*
* Parameters:
*  None
*
* Return:
*  CY_RSLT_SUCCESS if successful, otherwise an error code indicating failure.
*
*******************************************************************************/
cy_rslt_t pdm_data_process(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    int16_t best_label = 0;
    float sample = 0.0f;
    float max_score = 0.0f;
    float label_scores[IMAI_DATA_OUT_COUNT];
    char *label_text[] = IMAI_DATA_OUT_SYMBOLS;

    /* Check if PDM PCM Data is ready to be processed */
    if (!pdm_pcm_flag)
    {
        result = PDM_PCM_DATA_NOT_READY;
        return result;
    }

    /* Reset the flag to false, indicating that the data is being processed */
    pdm_pcm_flag = false;

#ifdef PRINT_CM55
    printf("\033[H\n");

#ifdef COMPONENT_CM33
    printf("DEEPCRAFT Studio Deploy Audio Example - CM33\r\n\n");
#else
    printf("DEEPCRAFT Studio Deploy Audio Example - CM55\r\n\n");
#endif /* COMPONENT_CM33 */
#endif

    for (uint32_t index = 0; index < FRAME_SIZE ; index++)
    {
        int16_t val_temp = full_rx_buffer[index];
        sample = SAMPLE_NORMALIZE(val_temp) * DIGITAL_BOOST_FACTOR;
        if (sample > 1.0)
        {
            sample = 1.0;
        }
        else if (sample < -1.0)
        {
            sample = -1.0;
        }

        /* Pass the audio sample for enqueue */
        result = IMAI_enqueue(&sample);
        CY_ASSERT(IMAI_RET_SUCCESS == result);

        best_label = 0;
        max_score = -1000.0f;

        /* Check if there is any model output to process */
        switch(IMAI_dequeue(label_scores))
        {
            case IMAI_RET_SUCCESS:      /* We have data, display it */
            {
                for(int i = 0; i < IMAI_DATA_OUT_COUNT; i++)
                {
                    #ifdef PRINT_CM55
                    printf("label: %-11s: score: %.4f\r\n", label_text[i], label_scores[i]);
                    #endif
                    if (label_scores[i] > max_score)
                    {
                        max_score = label_scores[i];
                        best_label = i;
                    }
                }

                ipc_payload_t* payload = cm55_ipc_get_payload_ptr();

                if(max_score >= OUTPUT_THRESHOLD_SCORE)
                {
                    payload->label_id = best_label;
                    strcpy(payload->label, label_text[best_label]);
                    payload->confidence = label_scores[best_label];
                    #ifdef PRINT_CM55
                    printf("\n\nOutput: %-10s\r\n", label_text[best_label]);
                    #endif
                }
                else
                {
                    payload->label_id = 0;
                    strcpy(payload->label, label_text[0]);
                    payload->confidence = label_scores[0];

                    #ifdef PRINT_CM55
                    printf("\n\nOutput: %-10s\r\n", "");
                    #endif
                }
                cm55_ipc_send_to_cm33();

                break;
            }

            case IMAI_RET_NODATA:   /* No new output, continue with sampling */
            {
                break;
            }
            case IMAI_RET_ERROR:    /* Abort on error */
            {
                CY_ASSERT(0);
                break;
            }
        }
    }

    return result;
}

/* [] END OF FILE */
