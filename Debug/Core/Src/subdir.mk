################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/adc_input.c \
../Core/Src/config.c \
../Core/Src/failsafe.c \
../Core/Src/ffb_engine.c \
../Core/Src/ffb_timer.c \
../Core/Src/main.c \
../Core/Src/motor.c \
../Core/Src/serial_cmd.c \
../Core/Src/stm32f4xx_hal_msp.c \
../Core/Src/stm32f4xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f4xx.c \
../Core/Src/usb_hid_desc.c 

OBJS += \
./Core/Src/adc_input.o \
./Core/Src/config.o \
./Core/Src/failsafe.o \
./Core/Src/ffb_engine.o \
./Core/Src/ffb_timer.o \
./Core/Src/main.o \
./Core/Src/motor.o \
./Core/Src/serial_cmd.o \
./Core/Src/stm32f4xx_hal_msp.o \
./Core/Src/stm32f4xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f4xx.o \
./Core/Src/usb_hid_desc.o 

C_DEPS += \
./Core/Src/adc_input.d \
./Core/Src/config.d \
./Core/Src/failsafe.d \
./Core/Src/ffb_engine.d \
./Core/Src/ffb_timer.d \
./Core/Src/main.d \
./Core/Src/motor.d \
./Core/Src/serial_cmd.d \
./Core/Src/stm32f4xx_hal_msp.d \
./Core/Src/stm32f4xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f4xx.d \
./Core/Src/usb_hid_desc.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F411xE -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CustomHID/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/adc_input.cyclo ./Core/Src/adc_input.d ./Core/Src/adc_input.o ./Core/Src/adc_input.su ./Core/Src/config.cyclo ./Core/Src/config.d ./Core/Src/config.o ./Core/Src/config.su ./Core/Src/failsafe.cyclo ./Core/Src/failsafe.d ./Core/Src/failsafe.o ./Core/Src/failsafe.su ./Core/Src/ffb_engine.cyclo ./Core/Src/ffb_engine.d ./Core/Src/ffb_engine.o ./Core/Src/ffb_engine.su ./Core/Src/ffb_timer.cyclo ./Core/Src/ffb_timer.d ./Core/Src/ffb_timer.o ./Core/Src/ffb_timer.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/motor.cyclo ./Core/Src/motor.d ./Core/Src/motor.o ./Core/Src/motor.su ./Core/Src/serial_cmd.cyclo ./Core/Src/serial_cmd.d ./Core/Src/serial_cmd.o ./Core/Src/serial_cmd.su ./Core/Src/stm32f4xx_hal_msp.cyclo ./Core/Src/stm32f4xx_hal_msp.d ./Core/Src/stm32f4xx_hal_msp.o ./Core/Src/stm32f4xx_hal_msp.su ./Core/Src/stm32f4xx_it.cyclo ./Core/Src/stm32f4xx_it.d ./Core/Src/stm32f4xx_it.o ./Core/Src/stm32f4xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f4xx.cyclo ./Core/Src/system_stm32f4xx.d ./Core/Src/system_stm32f4xx.o ./Core/Src/system_stm32f4xx.su ./Core/Src/usb_hid_desc.cyclo ./Core/Src/usb_hid_desc.d ./Core/Src/usb_hid_desc.o ./Core/Src/usb_hid_desc.su

.PHONY: clean-Core-2f-Src

