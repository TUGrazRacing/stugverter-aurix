################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../inverter/pwm/pwm.c" 

COMPILED_SRCS += \
"inverter/pwm/pwm.src" 

C_DEPS += \
"./inverter/pwm/pwm.d" 

OBJS += \
"inverter/pwm/pwm.o" 


# Each subdirectory must supply rules for building sources it contributes
"inverter/pwm/pwm.src":"../inverter/pwm/pwm.c" "inverter/pwm/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/probstn/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"inverter/pwm/pwm.o":"inverter/pwm/pwm.src" "inverter/pwm/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-inverter-2f-pwm

clean-inverter-2f-pwm:
	-$(RM) ./inverter/pwm/pwm.d ./inverter/pwm/pwm.o ./inverter/pwm/pwm.src

.PHONY: clean-inverter-2f-pwm

