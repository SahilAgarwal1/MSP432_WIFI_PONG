################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
G8RTOS_Lab3/%.obj: ../G8RTOS_Lab3/%.s $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"/home/sahil/ti/ccs1020/ccs/tools/compiler/ti-cgt-arm_20.2.4.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=FPv4SPD16 -me --include_path="/home/sahil/ti/ccs1020/ccs/ccs_base/arm/include" --include_path="/home/sahil/Documents/workspace/Lab3/BoardSupportPackage/inc" --include_path="/home/sahil/ti/ccs1020/ccs/ccs_base/arm/include/CMSIS" --include_path="/home/sahil/Documents/workspace/Lab3/BoardSupportPackage/DriverLib" --include_path="/home/sahil/Documents/workspace/Lab4" --include_path="/home/sahil/ti/ccs1020/ccs/tools/compiler/ti-cgt-arm_20.2.4.LTS/include" --advice:power=all --define=__MSP432P401R__ --define=ccs -g --gcc --diag_warning=225 --diag_wrap=off --display_error_number --abi=eabi --preproc_with_compile --preproc_dependency="G8RTOS_Lab3/$(basename $(<F)).d_raw" --obj_directory="G8RTOS_Lab3" $(GEN_OPTS__FLAG) "$(shell echo $<)"
	@echo 'Finished building: "$<"'
	@echo ' '

G8RTOS_Lab3/%.obj: ../G8RTOS_Lab3/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"/home/sahil/ti/ccs1020/ccs/tools/compiler/ti-cgt-arm_20.2.4.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=FPv4SPD16 -me --include_path="/home/sahil/ti/ccs1020/ccs/ccs_base/arm/include" --include_path="/home/sahil/Documents/workspace/Lab3/BoardSupportPackage/inc" --include_path="/home/sahil/ti/ccs1020/ccs/ccs_base/arm/include/CMSIS" --include_path="/home/sahil/Documents/workspace/Lab3/BoardSupportPackage/DriverLib" --include_path="/home/sahil/Documents/workspace/Lab4" --include_path="/home/sahil/ti/ccs1020/ccs/tools/compiler/ti-cgt-arm_20.2.4.LTS/include" --advice:power=all --define=__MSP432P401R__ --define=ccs -g --gcc --diag_warning=225 --diag_wrap=off --display_error_number --abi=eabi --preproc_with_compile --preproc_dependency="G8RTOS_Lab3/$(basename $(<F)).d_raw" --obj_directory="G8RTOS_Lab3" $(GEN_OPTS__FLAG) "$(shell echo $<)"
	@echo 'Finished building: "$<"'
	@echo ' '


