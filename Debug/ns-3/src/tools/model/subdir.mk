################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../ns-3/src/tools/model/delay-jitter-estimation.cc \
../ns-3/src/tools/model/event-garbage-collector.cc \
../ns-3/src/tools/model/gnuplot.cc 

OBJS += \
./ns-3/src/tools/model/delay-jitter-estimation.o \
./ns-3/src/tools/model/event-garbage-collector.o \
./ns-3/src/tools/model/gnuplot.o 

CC_DEPS += \
./ns-3/src/tools/model/delay-jitter-estimation.d \
./ns-3/src/tools/model/event-garbage-collector.d \
./ns-3/src/tools/model/gnuplot.d 


# Each subdirectory must supply rules for building sources it contributes
ns-3/src/tools/model/%.o: ../ns-3/src/tools/model/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


