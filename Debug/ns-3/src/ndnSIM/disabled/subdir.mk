################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../ns-3/src/ndnSIM/disabled/ndn-content-object-header-ccnb.cc \
../ns-3/src/ndnSIM/disabled/ndn-decoding-helper.cc \
../ns-3/src/ndnSIM/disabled/ndn-encoding-helper.cc \
../ns-3/src/ndnSIM/disabled/ndn-interest-header-ccnb.cc 

OBJS += \
./ns-3/src/ndnSIM/disabled/ndn-content-object-header-ccnb.o \
./ns-3/src/ndnSIM/disabled/ndn-decoding-helper.o \
./ns-3/src/ndnSIM/disabled/ndn-encoding-helper.o \
./ns-3/src/ndnSIM/disabled/ndn-interest-header-ccnb.o 

CC_DEPS += \
./ns-3/src/ndnSIM/disabled/ndn-content-object-header-ccnb.d \
./ns-3/src/ndnSIM/disabled/ndn-decoding-helper.d \
./ns-3/src/ndnSIM/disabled/ndn-encoding-helper.d \
./ns-3/src/ndnSIM/disabled/ndn-interest-header-ccnb.d 


# Each subdirectory must supply rules for building sources it contributes
ns-3/src/ndnSIM/disabled/%.o: ../ns-3/src/ndnSIM/disabled/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


