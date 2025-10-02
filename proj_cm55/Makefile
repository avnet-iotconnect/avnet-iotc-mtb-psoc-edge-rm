################################################################################
# \file Makefile
# \version 1.0
#
# \brief
# Makefile for configuring the Cortex-M55 core project
#
################################################################################
# Copyright 2025, Cypress Semiconductor Corporation (an Infineon company) or
# an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
#
# This software, including source code, documentation and related
# materials ("Software") is owned by Cypress Semiconductor Corporation
# or one of its affiliates ("Cypress") and is protected by and subject to
# worldwide patent protection (United States and foreign),
# United States copyright laws and international treaty provisions.
# Therefore, you may use this Software only as provided in the license
# agreement accompanying the software package from which you
# obtained this Software ("EULA").
# If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
# non-transferable license to copy, modify, and compile the Software
# source code solely for use in connection with Cypress's
# integrated circuit products.  Any reproduction, modification, translation,
# compilation, or representation of this Software except as specified
# above is prohibited without the express written permission of Cypress.
#
# Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
# reserves the right to make changes to the Software without notice. Cypress
# does not assume any liability arising out of the application or use of the
# Software or any product or circuit described in the Software. Cypress does
# not authorize its products for use in any products where a malfunction or
# failure of the Cypress product may reasonably be expected to result in
# significant property damage, injury or death ("High Risk Product"). By
# including Cypress's product in a High Risk Product, the manufacturer
# of such system or application assumes all risk of such use and in doing
# so agrees to indemnify Cypress against all liability.
################################################################################

include ../common.mk

################################################################################
# Basic Configuration
################################################################################

# model selection

# For KIT_PSE84_EVAL_EPC2, KIT_PSE84_EVAL_EPC4 and KIT_PSE84_AI below models are supported.
# COUGH_MODEL
# ALARM_MODEL
# BABYCRY_MODEL
# SIREN_MODEL
# GESTURE_MODEL (For KIT_PSE84_AI only)

MODEL_SELECTION = SIREN_MODEL

ifeq (,$(filter $(TARGET),APP_KIT_PSE84_EVAL_EPC2 APP_KIT_PSE84_EVAL_EPC4))
else
    ifeq (,$(filter $(strip $(MODEL_SELECTION)),COUGH_MODEL ALARM_MODEL BABYCRY_MODEL SIREN_MODEL))
        $(error Invalid MODEL_SELECTION "$(MODEL_SELECTION)" for TARGET=$(TARGET))
    endif
endif

ifeq (,$(filter $(TARGET),APP_KIT_PSE84_AI))
else
    ifeq (,$(filter $(strip $(MODEL_SELECTION)),COUGH_MODEL ALARM_MODEL BABYCRY_MODEL SIREN_MODEL GESTURE_MODEL))
        $(error Invalid MODEL_SELECTION "$(MODEL_SELECTION)" for TARGET=$(TARGET))
    endif
endif

# Name of application (used to derive name of final linked file).
#
# If APPNAME is edited, ensure to update or regenerate launch
# configurations for your IDE.
APPNAME=proj_cm55

# If set to "true" or "1", display full command-lines when building.
VERBOSE=

# Neural Network Configuration. Options include
#
# float    -- floating point for the input data and weights
# int8x8   -- 8-bit fixed-point for the input data and weights
NN_TYPE=int8x8

# Choose the inference engine
# tflm      -- TensorFlow Lite for Microcontrollers inference engine with interpreter
# tflm_less -- TensorFlow Lite for Microcontrollers inference engine interpreter-less
NN_INFERENCE_ENGINE=tflm

################################################################################
# Advanced Configuration
################################################################################

# Enable optional code that is ordinarily disabled by default.
#
# Available components depend on the specific targeted hardware and firmware
# in use. In general, if you have
#
#    COMPONENTS=foo bar
#
# ... then code in directories named COMPONENT_foo and COMPONENT_bar will be
# added to the build
#
COMPONENTS+=FREERTOS RTOS_AWARE

# Like COMPONENTS, but disable optional code that was enabled by default.
DISABLE_COMPONENTS=

# Set this application to run at the CM55
CORE=CM55
CORE_NAME=CM55_0
NPU=U55

# By default the build system automatically looks in the Makefile's directory
# tree for source code and builds it. The SOURCES variable can be used to
# manually add source code to the build process from a location not searched
# by default, or otherwise not found by the build system.
SOURCES+=$(wildcard ../shared/source/COMPONENT_CM55/*.c)


# Like SOURCES, but for include directories. Value should be paths to
# directories (without a leading -I).
INCLUDES+=../shared/include

# Add additional defines to the build process (without a leading -D).
DEFINES+=CY_RETARGET_IO_CONVERT_LF_TO_CRLF

# Add additional defines related to ARM Helium and DSP extensions
DEFINES+=ARM_MATH_HELIUM ARM_MATH_DSP ARM_MATH_AUTOVECTORIZE


# Add additional components related to MTB ML
COMPONENTS+=U55

# Add additional define/components to select the inference engine
ifeq (tflm, $(NN_INFERENCE_ENGINE))
COMPONENTS+=ML_TFLM
DEFINES+=TF_LITE_STATIC_MEMORY
endif

# Add define to build code for CM55 and include additional files
DEFINES+=CY_ML_MODEL_MEM=.cy_socmem_data
DEFINES+=CY_ML_ARENA_MEM=.cy_socmem_data
COMPONENTS+=CMSIS_DSP

# Depending which Neural Network Type, add a specific DEFINE and COMPONENT
ifeq (float, $(NN_TYPE))
COMPONENTS+=ML_FLOAT32
endif
ifeq (int8x8, $(NN_TYPE))
COMPONENTS+=ML_INT8x8
endif

INCLUDES+=$(SEARCH_cmsis)/COMPONENT_CMSIS_NN/Include

INCLUDES+=./ready_models

CY_IGNORE+=$(SEARCH_CMSIS-DSP)

ifeq (COUGH_MODEL, $(MODEL_SELECTION))
DEFINES+=COUGH_MODEL
# Additional / custom libraries to link in to the application.
LDLIBS+=./ready_models/CONFIG_$(CONFIG)/TOOLCHAIN_$(TOOLCHAIN)/cough_lib_eval.a
endif

ifeq (BABYCRY_MODEL, $(MODEL_SELECTION))
DEFINES+=BABYCRY_MODEL
# Additional / custom libraries to link in to the application.
LDLIBS+=./ready_models/CONFIG_$(CONFIG)/TOOLCHAIN_$(TOOLCHAIN)/babycry_lib_eval.a
endif

ifeq (SIREN_MODEL, $(MODEL_SELECTION))
DEFINES+=SIREN_MODEL
# Additional / custom libraries to link in to the application.
LDLIBS+=./ready_models/CONFIG_$(CONFIG)/TOOLCHAIN_$(TOOLCHAIN)/siren_lib_eval.a
endif

ifeq (ALARM_MODEL, $(MODEL_SELECTION))
DEFINES+=ALARM_MODEL
# Additional / custom libraries to link in to the application.
LDLIBS+=./ready_models/CONFIG_$(CONFIG)/TOOLCHAIN_$(TOOLCHAIN)/alarm_siren_lib_eval.a
endif

ifeq (GESTURE_MODEL, $(MODEL_SELECTION))
DEFINES+=GESTURE_MODEL
# Additional / custom libraries to link in to the application.
LDLIBS+=./ready_models/CONFIG_$(CONFIG)/TOOLCHAIN_$(TOOLCHAIN)/gesture_lib_eval.a
endif


ifneq (GESTURE_MODEL, $(MODEL_SELECTION))
CY_IGNORE+=source/radar.c
CY_IGNORE+=source/radar
else
CY_IGNORE+=source/audio.c
endif

# Select softfp or hardfp floating point. Default is softfp.
VFP_SELECT+=hardfp

# Additional / custom C compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
CFLAGS+=

# Additional / custom C++ compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
CXXFLAGS+=

# Additional / custom assembler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
ASFLAGS+=

# Additional / custom linker flags.
LDFLAGS+=

# Additional / custom libraries to link in to the application.
LDLIBS+=

# Path to the linker script to use (if empty, use the default linker script).
LINKER_SCRIPT=

# Custom pre-build commands to run.
PREBUILD=

# Custom post-build commands to run.
POSTBUILD=


################################################################################
# Paths
################################################################################

# Relative path to the project directory (default is the Makefile's directory).
#
# This controls where automatic source code discovery looks for code.
CY_APP_PATH=

# Relative path to the shared repo location.
#
# All .mtb files have the format, <URI>#<COMMIT>#<LOCATION>. If the <LOCATION> 
# field begins with $$ASSET_REPO$$, then the repo is deposited in the path 
# specified by the CY_GETLIBS_SHARED_PATH variable. The default location is one 
# directory level above the current app directory. This is used with 
# CY_GETLIBS_SHARED_NAME variable, which specifies the directory name.
CY_GETLIBS_SHARED_PATH=../..

# Directory name of the shared repo location.
#
CY_GETLIBS_SHARED_NAME=mtb_shared

include $(CY_TOOLS_DIR)/make/start.mk
