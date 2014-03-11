################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../ns-3/src/mpi/model/distributed-simulator-impl.cc \
../ns-3/src/mpi/model/mpi-interface.cc \
../ns-3/src/mpi/model/mpi-receiver.cc 

OBJS += \
./ns-3/src/mpi/model/distributed-simulator-impl.o \
./ns-3/src/mpi/model/mpi-interface.o \
./ns-3/src/mpi/model/mpi-receiver.o 

CC_DEPS += \
./ns-3/src/mpi/model/distributed-simulator-impl.d \
./ns-3/src/mpi/model/mpi-interface.d \
./ns-3/src/mpi/model/mpi-receiver.d 


# Each subdirectory must supply rules for building sources it contributes
ns-3/src/mpi/model/%.o: ../ns-3/src/mpi/model/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


