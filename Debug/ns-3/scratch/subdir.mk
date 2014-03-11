################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../ns-3/scratch/ndn-PROVA-inform-scenario.cc \
../ns-3/scratch/ndn-bloom-container-scenario-PROVA.cc \
../ns-3/scratch/ndn-bloom-container-scenario.cc \
../ns-3/scratch/ndn-triangle-calculate-routes.cc \
../ns-3/scratch/rocketfuel-maps-cch-to-annotaded.cc \
../ns-3/scratch/scratch-simulator.cc 

OBJS += \
./ns-3/scratch/ndn-PROVA-inform-scenario.o \
./ns-3/scratch/ndn-bloom-container-scenario-PROVA.o \
./ns-3/scratch/ndn-bloom-container-scenario.o \
./ns-3/scratch/ndn-triangle-calculate-routes.o \
./ns-3/scratch/rocketfuel-maps-cch-to-annotaded.o \
./ns-3/scratch/scratch-simulator.o 

CC_DEPS += \
./ns-3/scratch/ndn-PROVA-inform-scenario.d \
./ns-3/scratch/ndn-bloom-container-scenario-PROVA.d \
./ns-3/scratch/ndn-bloom-container-scenario.d \
./ns-3/scratch/ndn-triangle-calculate-routes.d \
./ns-3/scratch/rocketfuel-maps-cch-to-annotaded.d \
./ns-3/scratch/scratch-simulator.d 


# Each subdirectory must supply rules for building sources it contributes
ns-3/scratch/%.o: ../ns-3/scratch/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


