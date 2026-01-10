################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../Cpu0_Main.c" \
"../Cpu1_Main.c" \
"../Cpu2_Main.c" \
"../Cpu3_Main.c" \
"../adc.c" \
"../foc.c" \
"../foc_math.c" \
"../gate_driver.c" \
"../lut.c" \
"../pwm.c" \
"../serialio.c" 

COMPILED_SRCS += \
"Cpu0_Main.src" \
"Cpu1_Main.src" \
"Cpu2_Main.src" \
"Cpu3_Main.src" \
"adc.src" \
"foc.src" \
"foc_math.src" \
"gate_driver.src" \
"lut.src" \
"pwm.src" \
"serialio.src" 

C_DEPS += \
"./Cpu0_Main.d" \
"./Cpu1_Main.d" \
"./Cpu2_Main.d" \
"./Cpu3_Main.d" \
"./adc.d" \
"./foc.d" \
"./foc_math.d" \
"./gate_driver.d" \
"./lut.d" \
"./pwm.d" \
"./serialio.d" 

OBJS += \
"Cpu0_Main.o" \
"Cpu1_Main.o" \
"Cpu2_Main.o" \
"Cpu3_Main.o" \
"adc.o" \
"foc.o" \
"foc_math.o" \
"gate_driver.o" \
"lut.o" \
"pwm.o" \
"serialio.o" 


# Each subdirectory must supply rules for building sources it contributes
"Cpu0_Main.src":"../Cpu0_Main.c" "subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/Zbook Firefly14 G8/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"Cpu0_Main.o":"Cpu0_Main.src" "subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"Cpu1_Main.src":"../Cpu1_Main.c" "subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/Zbook Firefly14 G8/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"Cpu1_Main.o":"Cpu1_Main.src" "subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"Cpu2_Main.src":"../Cpu2_Main.c" "subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/Zbook Firefly14 G8/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"Cpu2_Main.o":"Cpu2_Main.src" "subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"Cpu3_Main.src":"../Cpu3_Main.c" "subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/Zbook Firefly14 G8/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"Cpu3_Main.o":"Cpu3_Main.src" "subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"adc.src":"../adc.c" "subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/Zbook Firefly14 G8/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"adc.o":"adc.src" "subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"foc.src":"../foc.c" "subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/Zbook Firefly14 G8/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"foc.o":"foc.src" "subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"foc_math.src":"../foc_math.c" "subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/Zbook Firefly14 G8/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"foc_math.o":"foc_math.src" "subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"gate_driver.src":"../gate_driver.c" "subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/Zbook Firefly14 G8/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"gate_driver.o":"gate_driver.src" "subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"lut.src":"../lut.c" "subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/Zbook Firefly14 G8/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"lut.o":"lut.src" "subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"pwm.src":"../pwm.c" "subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/Zbook Firefly14 G8/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"pwm.o":"pwm.src" "subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"serialio.src":"../serialio.c" "subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc38x "-fC:/Users/Zbook Firefly14 G8/AURIX-v1.10.28-workspace/stugverter-aurix/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc38x -Y0 -N0 -Z0 -o "$@" "$<"
"serialio.o":"serialio.src" "subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean--2e-

clean--2e-:
	-$(RM) ./Cpu0_Main.d ./Cpu0_Main.o ./Cpu0_Main.src ./Cpu1_Main.d ./Cpu1_Main.o ./Cpu1_Main.src ./Cpu2_Main.d ./Cpu2_Main.o ./Cpu2_Main.src ./Cpu3_Main.d ./Cpu3_Main.o ./Cpu3_Main.src ./adc.d ./adc.o ./adc.src ./foc.d ./foc.o ./foc.src ./foc_math.d ./foc_math.o ./foc_math.src ./gate_driver.d ./gate_driver.o ./gate_driver.src ./lut.d ./lut.o ./lut.src ./pwm.d ./pwm.o ./pwm.src ./serialio.d ./serialio.o ./serialio.src

.PHONY: clean--2e-

