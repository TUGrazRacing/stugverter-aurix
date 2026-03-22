################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../inverter/config/app_config.c" 

COMPILED_SRCS += \
"inverter/config/app_config.src" 

C_DEPS += \
"./inverter/config/app_config.d" 

OBJS += \
"inverter/config/app_config.o" 


# Each subdirectory must supply rules for building sources it contributes
"inverter/config/app_config.src":"../inverter/config/app_config.c" "inverter/config/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/probstn/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"inverter/config/app_config.o":"inverter/config/app_config.src" "inverter/config/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-inverter-2f-config

clean-inverter-2f-config:
	-$(RM) ./inverter/config/app_config.d ./inverter/config/app_config.o ./inverter/config/app_config.src

.PHONY: clean-inverter-2f-config

