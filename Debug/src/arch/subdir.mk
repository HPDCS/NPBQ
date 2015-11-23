################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/arch/x86.c 

OBJS += \
./src/arch/x86.o 

C_DEPS += \
./src/arch/x86.d 


# Each subdirectory must supply rules for building sources it contributes
src/arch/%.o: ../src/arch/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -DARCH_X86_64 -O0 -g3 -Wall -Wextra -Wconversion -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


