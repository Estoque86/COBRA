################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../ns-3/src/test/csma-system-test-suite.cc \
../ns-3/src/test/global-routing-test-suite.cc \
../ns-3/src/test/mobility-test-suite.cc \
../ns-3/src/test/static-routing-test-suite.cc 

OBJS += \
./ns-3/src/test/csma-system-test-suite.o \
./ns-3/src/test/global-routing-test-suite.o \
./ns-3/src/test/mobility-test-suite.o \
./ns-3/src/test/static-routing-test-suite.o 

CC_DEPS += \
./ns-3/src/test/csma-system-test-suite.d \
./ns-3/src/test/global-routing-test-suite.d \
./ns-3/src/test/mobility-test-suite.d \
./ns-3/src/test/static-routing-test-suite.d 


# Each subdirectory must supply rules for building sources it contributes
ns-3/src/test/%.o: ../ns-3/src/test/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


