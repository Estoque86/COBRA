################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../ns-3/src/uan/examples/uan-cw-example.cc \
../ns-3/src/uan/examples/uan-rc-example.cc 

OBJS += \
./ns-3/src/uan/examples/uan-cw-example.o \
./ns-3/src/uan/examples/uan-rc-example.o 

CC_DEPS += \
./ns-3/src/uan/examples/uan-cw-example.d \
./ns-3/src/uan/examples/uan-rc-example.d 


# Each subdirectory must supply rules for building sources it contributes
ns-3/src/uan/examples/%.o: ../ns-3/src/uan/examples/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


