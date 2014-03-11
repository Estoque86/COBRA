################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../ns-3/src/stats/model/data-calculator.cc \
../ns-3/src/stats/model/data-collector.cc \
../ns-3/src/stats/model/data-output-interface.cc \
../ns-3/src/stats/model/omnet-data-output.cc \
../ns-3/src/stats/model/packet-data-calculators.cc \
../ns-3/src/stats/model/sqlite-data-output.cc \
../ns-3/src/stats/model/time-data-calculators.cc 

OBJS += \
./ns-3/src/stats/model/data-calculator.o \
./ns-3/src/stats/model/data-collector.o \
./ns-3/src/stats/model/data-output-interface.o \
./ns-3/src/stats/model/omnet-data-output.o \
./ns-3/src/stats/model/packet-data-calculators.o \
./ns-3/src/stats/model/sqlite-data-output.o \
./ns-3/src/stats/model/time-data-calculators.o 

CC_DEPS += \
./ns-3/src/stats/model/data-calculator.d \
./ns-3/src/stats/model/data-collector.d \
./ns-3/src/stats/model/data-output-interface.d \
./ns-3/src/stats/model/omnet-data-output.d \
./ns-3/src/stats/model/packet-data-calculators.d \
./ns-3/src/stats/model/sqlite-data-output.d \
./ns-3/src/stats/model/time-data-calculators.d 


# Each subdirectory must supply rules for building sources it contributes
ns-3/src/stats/model/%.o: ../ns-3/src/stats/model/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


