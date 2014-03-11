################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../ns-3/src/internet/test/error-channel.cc \
../ns-3/src/internet/test/error-net-device.cc \
../ns-3/src/internet/test/global-route-manager-impl-test-suite.cc \
../ns-3/src/internet/test/ipv4-address-generator-test-suite.cc \
../ns-3/src/internet/test/ipv4-address-helper-test-suite.cc \
../ns-3/src/internet/test/ipv4-fragmentation-test.cc \
../ns-3/src/internet/test/ipv4-header-test.cc \
../ns-3/src/internet/test/ipv4-list-routing-test-suite.cc \
../ns-3/src/internet/test/ipv4-packet-info-tag-test-suite.cc \
../ns-3/src/internet/test/ipv4-raw-test.cc \
../ns-3/src/internet/test/ipv4-test.cc \
../ns-3/src/internet/test/ipv6-address-generator-test-suite.cc \
../ns-3/src/internet/test/ipv6-address-helper-test-suite.cc \
../ns-3/src/internet/test/ipv6-dual-stack-test-suite.cc \
../ns-3/src/internet/test/ipv6-extension-header-test-suite.cc \
../ns-3/src/internet/test/ipv6-fragmentation-test.cc \
../ns-3/src/internet/test/ipv6-list-routing-test-suite.cc \
../ns-3/src/internet/test/ipv6-packet-info-tag-test-suite.cc \
../ns-3/src/internet/test/ipv6-raw-test.cc \
../ns-3/src/internet/test/ipv6-test.cc \
../ns-3/src/internet/test/rtt-test.cc \
../ns-3/src/internet/test/tcp-test.cc \
../ns-3/src/internet/test/udp-test.cc 

OBJS += \
./ns-3/src/internet/test/error-channel.o \
./ns-3/src/internet/test/error-net-device.o \
./ns-3/src/internet/test/global-route-manager-impl-test-suite.o \
./ns-3/src/internet/test/ipv4-address-generator-test-suite.o \
./ns-3/src/internet/test/ipv4-address-helper-test-suite.o \
./ns-3/src/internet/test/ipv4-fragmentation-test.o \
./ns-3/src/internet/test/ipv4-header-test.o \
./ns-3/src/internet/test/ipv4-list-routing-test-suite.o \
./ns-3/src/internet/test/ipv4-packet-info-tag-test-suite.o \
./ns-3/src/internet/test/ipv4-raw-test.o \
./ns-3/src/internet/test/ipv4-test.o \
./ns-3/src/internet/test/ipv6-address-generator-test-suite.o \
./ns-3/src/internet/test/ipv6-address-helper-test-suite.o \
./ns-3/src/internet/test/ipv6-dual-stack-test-suite.o \
./ns-3/src/internet/test/ipv6-extension-header-test-suite.o \
./ns-3/src/internet/test/ipv6-fragmentation-test.o \
./ns-3/src/internet/test/ipv6-list-routing-test-suite.o \
./ns-3/src/internet/test/ipv6-packet-info-tag-test-suite.o \
./ns-3/src/internet/test/ipv6-raw-test.o \
./ns-3/src/internet/test/ipv6-test.o \
./ns-3/src/internet/test/rtt-test.o \
./ns-3/src/internet/test/tcp-test.o \
./ns-3/src/internet/test/udp-test.o 

CC_DEPS += \
./ns-3/src/internet/test/error-channel.d \
./ns-3/src/internet/test/error-net-device.d \
./ns-3/src/internet/test/global-route-manager-impl-test-suite.d \
./ns-3/src/internet/test/ipv4-address-generator-test-suite.d \
./ns-3/src/internet/test/ipv4-address-helper-test-suite.d \
./ns-3/src/internet/test/ipv4-fragmentation-test.d \
./ns-3/src/internet/test/ipv4-header-test.d \
./ns-3/src/internet/test/ipv4-list-routing-test-suite.d \
./ns-3/src/internet/test/ipv4-packet-info-tag-test-suite.d \
./ns-3/src/internet/test/ipv4-raw-test.d \
./ns-3/src/internet/test/ipv4-test.d \
./ns-3/src/internet/test/ipv6-address-generator-test-suite.d \
./ns-3/src/internet/test/ipv6-address-helper-test-suite.d \
./ns-3/src/internet/test/ipv6-dual-stack-test-suite.d \
./ns-3/src/internet/test/ipv6-extension-header-test-suite.d \
./ns-3/src/internet/test/ipv6-fragmentation-test.d \
./ns-3/src/internet/test/ipv6-list-routing-test-suite.d \
./ns-3/src/internet/test/ipv6-packet-info-tag-test-suite.d \
./ns-3/src/internet/test/ipv6-raw-test.d \
./ns-3/src/internet/test/ipv6-test.d \
./ns-3/src/internet/test/rtt-test.d \
./ns-3/src/internet/test/tcp-test.d \
./ns-3/src/internet/test/udp-test.d 


# Each subdirectory must supply rules for building sources it contributes
ns-3/src/internet/test/%.o: ../ns-3/src/internet/test/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


