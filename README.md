# Temperature Control and Heating System

## Overview

The Temperature Control and Heating System is designed to manage and control the heating for both driver and passenger seats in a vehicle. It uses FreeRTOS to handle multiple tasks that involve setting desired temperatures, controlling heating intensity, and displaying status updates through UART.

## Components

1. **vTempSettingTask**: Manages the desired temperature settings based on interrupt events for the driver and passenger. It adjusts the temperature settings and measures task execution time.

2. **vHeaterControllerTask**: Controls the heating intensity based on the current and desired temperatures. It sends heating intensity values to appropriate queues and updates the display.

3. **vHeaterLedsControllerTask**: Controls the LEDs that indicate the current heating intensity. It updates the LED states based on the heating intensity values received from the queue.

4. **vDisplayTask**: Sends temperature and heating status information to a UART interface for both the driver and passenger. It updates the display with current temperature, desired heat level, and heater status.

5. **vRunTimeMeasurementsTask**: Measures and displays the CPU load and task execution times. It periodically calculates the CPU load and sends the information to the UART interface.

## Getting Started

### Prerequisites

- FreeRTOS library
- UART communication setup
- Hardware with GPIO and PWM capabilities
- Timer support for task scheduling

### Setup

1. **Include FreeRTOS**: Ensure you have the FreeRTOS library integrated into your project.

2. **Define Constants**: Configure necessary constants and bit values such as `mainSW1_INTERRUPT_BIT`, `mainSW2_INTERRUPT_BIT`, `OFF`, `LOW`, `MEDIUM`, `HIGH`, etc.

3. **Initialize Resources**: Set up semaphores, mutexes, and queues used in the tasks:
   - `DesiredTempMutexDriver`
   - `DesiredTempMutexPassenger`
   - `CurrentTempMutexDriver`
   - `CurrentTempMutexPassenger`
   - `Controller_HeatingDriver`
   - `Controller_HeatingPassenger`
   - `Controller_DisplayDriver`
   - `Controller_DisplayPassenger`
   - `Reading_DisplayDriver`
   - `Reading_DisplayPassenger`
   - `UARTMutex`

4. **Configure GPIO**: Define GPIO functions to control LEDs and other hardware components:
   - `GPIO_RedLed1On()`, `GPIO_BlueLed1On()`, `GPIO_GreenLed1On()`
   - `GPIO_RedLed1Off()`, `GPIO_BlueLed1Off()`, `GPIO_GreenLed1Off()`
   - Similar functions for passenger LEDs

5. **Define UART Functions**: Implement UART functions for sending strings and bytes:
   - `UART0_SendString()`
   - `UART0_SendByte()`
   - `UART0_SendInteger()`

6. **Implement Tasks**: Write the tasks as provided and add them to the FreeRTOS scheduler.

### Running the System

1. **Compile and Upload**: Build your project and upload it to the hardware.

2. **Monitor UART Output**: Use a terminal program to view the UART output from the `vDisplayTask` and `vRunTimeMeasurementsTask`.

3. **Test Temperature Control**: Verify that temperature adjustments, heating intensity control, and LED indicators function correctly based on the defined logic.

## Task Timing and Performance

- `vTempSettingTask`: Measures the time taken to set desired temperatures and adjusts the settings.
- `vHeaterControllerTask`: Calculates heating intensity and updates the queues for driver and passenger.
- `vHeaterLedsControllerTask`: Manages LED indicators based on heating intensity.
- `vDisplayTask`: Sends UART messages with temperature and heating status.
- `vRunTimeMeasurementsTask`: Measures CPU load and task execution times for performance monitoring.

## Troubleshooting

- **LEDs Not Working**: Ensure GPIO pins are correctly configured and the LED functions are properly defined.
- **UART Communication Issues**: Verify UART setup and connections. Check for correct baud rate and settings.
