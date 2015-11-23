################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/datatypes/calqueue.c \
../src/datatypes/list.c \
../src/datatypes/nonblocking_queue.c 

OBJS += \
./src/datatypes/calqueue.o \
./src/datatypes/list.o \
./src/datatypes/nonblocking_queue.o 

C_DEPS += \
./src/datatypes/calqueue.d \
./src/datatypes/list.d \
./src/datatypes/nonblocking_queue.d 


# Each subdirectory must supply rules for building sources it contributes
src/datatypes/%.o: ../src/datatypes/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -DARCH_X86_64 -O0 -g3 -Wall -Wextra -Wconversion -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


