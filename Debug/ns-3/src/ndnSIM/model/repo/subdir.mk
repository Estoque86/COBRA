################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../ns-3/src/ndnSIM/model/repo/ndn-repo.cc \
../ns-3/src/ndnSIM/model/repo/repo-impl.cc 

OBJS += \
./ns-3/src/ndnSIM/model/repo/ndn-repo.o \
./ns-3/src/ndnSIM/model/repo/repo-impl.o 

CC_DEPS += \
./ns-3/src/ndnSIM/model/repo/ndn-repo.d \
./ns-3/src/ndnSIM/model/repo/repo-impl.d 


# Each subdirectory must supply rules for building sources it contributes
ns-3/src/ndnSIM/model/repo/%.o: ../ns-3/src/ndnSIM/model/repo/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


