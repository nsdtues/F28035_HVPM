################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

CG_TOOL_ROOT := C:/Program Files (x86)/Texas Instruments/ccsv7/tools/compiler/ti-cgt-c2000_16.9.6.LTS

GEN_OPTS__FLAG := 
GEN_CMDS__FLAG := 

ORDERED_OBJS += \
"../rts2800_fpu32_fast_supplement.lib" \
-lrts2800_fpu32.lib \
"../IQmath_fpu32.lib" \
"./DLOG4CHC.obj" \
"./DRV8301_SPI.obj" \
"./F2806x_CodeStartBranch.obj" \
"./F2806x_GlobalVariableDefs.obj" \
"./F2806x_usDelay.obj" \
"./PM_Sensorless-DevInit_F2806x.obj" \
"./PM_Sensorless.obj" \
"./F2806x_CpuTimers.obj" \
"../F2806x_Headers_nonBIOS.cmd" \
"../F2806x_RAM_PM_Sensorless.CMD" \
"../IQmath.lib" \
$(GEN_CMDS__FLAG) \

-include ../makefile.init

RM := DEL /F
RMDIR := RMDIR /S/Q

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir_vars.mk
-include source/subdir_vars.mk
-include subdir_rules.mk
-include source/subdir_rules.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C55_DEPS)),)
-include $(C55_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(S67_DEPS)),)
-include $(S67_DEPS)
endif
ifneq ($(strip $(S62_DEPS)),)
-include $(S62_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(OPT_DEPS)),)
-include $(OPT_DEPS)
endif
ifneq ($(strip $(C??_DEPS)),)
-include $(C??_DEPS)
endif
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(S??_DEPS)),)
-include $(S??_DEPS)
endif
ifneq ($(strip $(C64_DEPS)),)
-include $(C64_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(S64_DEPS)),)
-include $(S64_DEPS)
endif
ifneq ($(strip $(INO_DEPS)),)
-include $(INO_DEPS)
endif
ifneq ($(strip $(CLA_DEPS)),)
-include $(CLA_DEPS)
endif
ifneq ($(strip $(S55_DEPS)),)
-include $(S55_DEPS)
endif
ifneq ($(strip $(SV7A_DEPS)),)
-include $(SV7A_DEPS)
endif
ifneq ($(strip $(C62_DEPS)),)
-include $(C62_DEPS)
endif
ifneq ($(strip $(C67_DEPS)),)
-include $(C67_DEPS)
endif
ifneq ($(strip $(PDE_DEPS)),)
-include $(PDE_DEPS)
endif
ifneq ($(strip $(K_DEPS)),)
-include $(K_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C43_DEPS)),)
-include $(C43_DEPS)
endif
ifneq ($(strip $(S43_DEPS)),)
-include $(S43_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(SA_DEPS)),)
-include $(SA_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
EXE_OUTPUTS += \
PM_Sensorless_DRV8301_F2806x.out \

EXE_OUTPUTS__QUOTED += \
"PM_Sensorless_DRV8301_F2806x.out" \

BIN_OUTPUTS += \
PM_Sensorless_DRV8301_F2806x.hex \

BIN_OUTPUTS__QUOTED += \
"PM_Sensorless_DRV8301_F2806x.hex" \


# All Target
all: PM_Sensorless_DRV8301_F2806x.out

# Tool invocations
PM_Sensorless_DRV8301_F2806x.out: $(OBJS) $(CMD_SRCS) $(CMD_UPPER_SRCS) $(LIB_SRCS) $(GEN_CMDS)
	@echo 'Building target: "$@"'
	@echo 'Invoking: C2000 Linker'
	"C:/Program Files (x86)/Texas Instruments/ccsv7/tools/compiler/ti-cgt-c2000_16.9.6.LTS/bin/cl2000" -v28 -ml --cla_support=cla0 --float_support=fpu32 --vcu_support=vcu0 --define="_DEBUG" --define="DRV8301" --define="LARGE_MODEL" --define="FLOATING_MATH" -g --printf_support=full --diag_warning=225 --quiet -z -m"PM_Sensorless_DRV8301_F2806x.map" --heap_size=0x400 --stack_size=0x1980 --warn_sections -i"C:/Program Files (x86)/Texas Instruments/ccsv7/tools/compiler/ti-cgt-c2000_16.9.6.LTS/lib" -i"C:/Users/win10/workspace_v7/PM_Sensorless" -i"C:/Program Files (x86)/Texas Instruments/ccsv7/tools/compiler/ti-cgt-c2000_16.9.6.LTS/include" --reread_libs --xml_link_info="PM_Sensorless_DRV8301_F2806x_linkInfo.xml" --rom_model -o "PM_Sensorless_DRV8301_F2806x.out" $(ORDERED_OBJS)
	@echo 'Finished building target: "$@"'
	@echo ' '

PM_Sensorless_DRV8301_F2806x.hex: $(EXE_OUTPUTS)
	@echo 'Building files: $(strip $(EXE_OUTPUTS__QUOTED))'
	@echo 'Invoking: C2000 Hex Utility'
	"C:/Program Files (x86)/Texas Instruments/ccsv7/tools/compiler/ti-cgt-c2000_16.9.6.LTS/bin/hex2000"  -o "PM_Sensorless_DRV8301_F2806x.hex" $(EXE_OUTPUTS__QUOTED)
	@echo 'Finished building: $(strip $(EXE_OUTPUTS__QUOTED))'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(BIN_OUTPUTS__QUOTED)$(EXE_OUTPUTS__QUOTED)
	-$(RM) "DLOG4CHC.obj" "DRV8301_SPI.obj" "F2806x_CodeStartBranch.obj" "F2806x_GlobalVariableDefs.obj" "F2806x_usDelay.obj" "PM_Sensorless-DevInit_F2806x.obj" "PM_Sensorless.obj" "F2806x_CpuTimers.obj" 
	-$(RM) "DRV8301_SPI.d" "F2806x_GlobalVariableDefs.d" "PM_Sensorless-DevInit_F2806x.d" "PM_Sensorless.d" "source\F2806x_CpuTimers.d" 
	-$(RM) "DLOG4CHC.d" "F2806x_CodeStartBranch.d" "F2806x_usDelay.d" 
	-@echo 'Finished clean'
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets

