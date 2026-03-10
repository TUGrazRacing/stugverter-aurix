################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../inverter/control/foc.c" 

COMPILED_SRCS += \
"inverter/control/foc.src" 

C_DEPS += \
"./inverter/control/foc.d" 

OBJS += \
"inverter/control/foc.o" 


# Each subdirectory must supply rules for building sources it contributes
"inverter/control/foc.src":"../inverter/control/foc.c" "inverter/control/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/probstn/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"inverter/control/foc.o":"inverter/control/foc.src" "inverter/control/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-inverter-2f-control

clean-inverter-2f-control:
	-$(RM) ./inverter/control/foc.d ./inverter/control/foc.o ./inverter/control/foc.src

.PHONY: clean-inverter-2f-control

