################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../inverter/math/current_math.c" \
"../inverter/math/foc_math.c" \
"../inverter/math/lut.c" \
"../inverter/math/resolver_math.c" 

COMPILED_SRCS += \
"inverter/math/current_math.src" \
"inverter/math/foc_math.src" \
"inverter/math/lut.src" \
"inverter/math/resolver_math.src" 

C_DEPS += \
"./inverter/math/current_math.d" \
"./inverter/math/foc_math.d" \
"./inverter/math/lut.d" \
"./inverter/math/resolver_math.d" 

OBJS += \
"inverter/math/current_math.o" \
"inverter/math/foc_math.o" \
"inverter/math/lut.o" \
"inverter/math/resolver_math.o" 


# Each subdirectory must supply rules for building sources it contributes
"inverter/math/current_math.src":"../inverter/math/current_math.c" "inverter/math/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/probstn/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"inverter/math/current_math.o":"inverter/math/current_math.src" "inverter/math/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"inverter/math/foc_math.src":"../inverter/math/foc_math.c" "inverter/math/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/probstn/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"inverter/math/foc_math.o":"inverter/math/foc_math.src" "inverter/math/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"inverter/math/lut.src":"../inverter/math/lut.c" "inverter/math/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/probstn/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"inverter/math/lut.o":"inverter/math/lut.src" "inverter/math/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"inverter/math/resolver_math.src":"../inverter/math/resolver_math.c" "inverter/math/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/probstn/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"inverter/math/resolver_math.o":"inverter/math/resolver_math.src" "inverter/math/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-inverter-2f-math

clean-inverter-2f-math:
	-$(RM) ./inverter/math/current_math.d ./inverter/math/current_math.o ./inverter/math/current_math.src ./inverter/math/foc_math.d ./inverter/math/foc_math.o ./inverter/math/foc_math.src ./inverter/math/lut.d ./inverter/math/lut.o ./inverter/math/lut.src ./inverter/math/resolver_math.d ./inverter/math/resolver_math.o ./inverter/math/resolver_math.src

.PHONY: clean-inverter-2f-math

