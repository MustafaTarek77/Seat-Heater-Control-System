/***************** Kernel includes. *****************/
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "event_groups.h"

/***************** MCAL includes. *****************/
#include "adc.h"
#include "gpio.h"
#include "uart0.h"
#include "GPTM.h"
#include "tm4c123gh6pm_registers.h"

/***************** Definitions *******************/
#define MAXVOLTAGEADC 3.3f  //ADC
#define MAXTEMPERATURE 45  //ADC
#define ISDRIVER 0
#define ISPASSENGER 1
#define NUMBER_OF_ITERATIONS_PER_ONE_MILI_SECOND 369  //DELAY
#define mainSW1_INTERRUPT_BIT ( 1UL << 0UL )
#define mainSW2_INTERRUPT_BIT ( 1UL << 1UL )
#define RUNTIME_MEASUREMENTS_TASK_PERIODICITY (1000U)

/***************** FreeRTOS tasks *****************/
void vTempSettingTask(void *pvParameters);
void vTempReadingTask(void *pvParameters); //Sensor Task
void vHeaterControllerTask(void *pvParameters);
void vHeaterLedsControllerTask(void *pvParameters);
void vDisplayTask(void *pvParameters);
void vRunTimeMeasurementsTask(void *pvParameters);

/***************** Task Handles *****************/
TaskHandle_t vTemperatureSetTaskDrivertHandle;
TaskHandle_t vTemperatureSetTaskPassengertHandle;
TaskHandle_t vTemperatureReadTaskDriverHandle;
TaskHandle_t vTemperatureReadTaskPassengerHandle;
TaskHandle_t vHeaterControllerTaskDriverHandle;
TaskHandle_t vHeaterControllerTaskPassengerHandle;
TaskHandle_t vHeaterLedsControllerTaskDriverHandle;
TaskHandle_t vHeaterLedsControllerTaskPassengerHandle;
TaskHandle_t vDisplayTaskDriverHandle;
TaskHandle_t vDisplayTaskPassengerHandle;
TaskHandle_t vRunTimeMeasurementsTaskHandle;

/*************************** Variables ***************************/
/* General Variables */
uint8 DriverState = 0;
uint8 PassengerState = 0;
UserHeatInput DesiredTempDriver; /* Carries Desired Value entered by user */
UserHeatInput DesiredTempPassenger;
float32 CurrentTempDriver; /* Carries Value generated by ADC */
float32 CurrentTempPassenger;
HeatIntensity heatIntensity;

/* Semaphores & Mutexes */
xSemaphoreHandle CurrentTempMutexDriver;
xSemaphoreHandle CurrentTempMutexPassenger;
xSemaphoreHandle DesiredTempMutexDriver;
xSemaphoreHandle DesiredTempMutexPassenger;
xSemaphoreHandle UARTMutex;

/* Queues */
QueueHandle_t Reading_DisplayDriver;
QueueHandle_t Reading_DisplayPassenger;
QueueHandle_t Controller_HeatingDriver;
QueueHandle_t Controller_HeatingPassenger;
QueueHandle_t Controller_DisplayDriver;
QueueHandle_t Controller_DisplayPassenger;

/* Events */
EventGroupHandle_t eventTempSet;

/* Runtime measurements */
uint32 ullTasksOutTime[13];
uint32 ullTasksInTime[13];
uint32 ullTasksExecutionTime[13];

/* LockTime variables per task for each resource */
TickType_t CurrentTempReadingTaskDriverLT = 0;
TickType_t CurrentTempReadingTaskPassengerLT = 0;

TickType_t DesiredTempSettingTaskDriverLT = 0;
TickType_t DesiredTempSettingTaskPassengerLT = 0;

TickType_t CurrentTempControllerTaskDriverLT = 0;
TickType_t CurrentTempControllerTaskPassengerLT = 0;

TickType_t DesiredTempControllerTaskDriverLT = 0;
TickType_t DesiredTempControllerTaskPassengerLT = 0;

TickType_t DesiredTempDisplayTaskDriverLT = 0;
TickType_t DesiredTempDisplayTaskPassengerLT = 0;

TickType_t UARTDisplayTaskDriverLT = 0;
TickType_t UARTDisplayTaskPassengerLT = 0;

TickType_t UARTRunTimeMeasurementsTaskLT = 0;
TickType_t UARTOneTimeRunTimeMeasurementsTaskLT = 0;

/***************** The HW setup function *****************/
static void prvSetupHardware(void)
{
    /* Place here any needed HW initialization such as GPIO, UART, etc.  */
    GPIO_BuiltinButtonsLedsInit();
    GPIO_SW1EdgeTriggeredInterruptInit();
    GPIO_SW2EdgeTriggeredInterruptInit();
    GPIO_SW3EdgeTriggeredInterruptInit();
    GPTM_WTimer0Init();
    UART0_Init();
    ADC_Init();
}

/******************* Extra Methods *******************/
void Delay_MS(unsigned long long n)
{
    volatile unsigned long long count = 0;
    while (count++ < (NUMBER_OF_ITERATIONS_PER_ONE_MILI_SECOND * n))
        ;
}

/*----------------------------- Main --------------------------------*/
int main()
{
    /* Setup the hardware for use with the Tiva C board. */
    prvSetupHardware();

    /* MUTEX CREATION */
    CurrentTempMutexDriver = xSemaphoreCreateMutex();
    CurrentTempMutexPassenger = xSemaphoreCreateMutex();
    DesiredTempMutexDriver = xSemaphoreCreateMutex();
    DesiredTempMutexPassenger = xSemaphoreCreateMutex();
    UARTMutex = xSemaphoreCreateMutex();

    /* QUEUE CREATION */
    Reading_DisplayDriver = xQueueCreate(1, sizeof(uint8));
    Reading_DisplayPassenger = xQueueCreate(1, sizeof(uint8));
    Controller_HeatingDriver = xQueueCreate(1, sizeof(uint8));
    Controller_HeatingPassenger = xQueueCreate(1, sizeof(uint8));
    Controller_DisplayDriver = xQueueCreate(1, sizeof(uint8));
    Controller_DisplayPassenger = xQueueCreate(1, sizeof(uint8));

    /* EVENT CREATION */
    eventTempSet = xEventGroupCreate();

    /* Tasks Creation */
    xTaskCreate(vTempSettingTask, /* Pointer to the function that implements the task. */
                "SetTempForDriver", /* Text name for the task.  This is to facilitate debugging only. */
                256, /* Stack depth - most small micro controllers will use much less stack than this. */
                NULL, /* pvParameters */
                (configMAX_PRIORITIES - 1), /* This task will run at priority 4. */
                &vTemperatureSetTaskDrivertHandle); /* We are using the task handle. */
    xTaskCreate(vTempSettingTask, "SetTempForPassenger", 256, NULL,
                (configMAX_PRIORITIES - 1),
                &vTemperatureSetTaskPassengertHandle);

    xTaskCreate(vTempReadingTask, "ReadTempForDriver", 256, ((void*) 0), 3,
                &vTemperatureReadTaskDriverHandle); // 0 => Driver
    xTaskCreate(vTempReadingTask, "ReadTempForPassenger", 256, ((void*) 1), 3,
                &vTemperatureReadTaskPassengerHandle); // 1 => Passenger

    xTaskCreate(vHeaterControllerTask, "ControlTempForDriver", 256, ((void*) 0),
                2, &vHeaterControllerTaskDriverHandle);
    xTaskCreate(vHeaterControllerTask, "ControlTempForPassenger", 256,
                ((void*) 1), 2, &vHeaterControllerTaskPassengerHandle);

    xTaskCreate(vHeaterLedsControllerTask, "ControlLedsForDriver", 256,
                ((void*) 0), 2, &vHeaterLedsControllerTaskDriverHandle);
    xTaskCreate(vHeaterLedsControllerTask, "ControlLedsForPassenger", 256,
                ((void*) 1), 2, &vHeaterLedsControllerTaskPassengerHandle);

    xTaskCreate(vDisplayTask, "DisplayForDriver", 256, ((void*) 0), 2,
                &vDisplayTaskDriverHandle);
    xTaskCreate(vDisplayTask, "DisplayForPassenger", 256, ((void*) 1), 2,
                &vDisplayTaskPassengerHandle);

    xTaskCreate(vRunTimeMeasurementsTask, "RunTimeMeasurements", 256, NULL, 1,
                &vRunTimeMeasurementsTaskHandle);

    /* Tasks' Tags  */
    vTaskSetApplicationTaskTag(vTemperatureSetTaskDrivertHandle,
                               (TaskHookFunction_t) 1);
    vTaskSetApplicationTaskTag(vTemperatureSetTaskPassengertHandle,
                               (TaskHookFunction_t) 2);
    vTaskSetApplicationTaskTag(vTemperatureReadTaskDriverHandle,
                               (TaskHookFunction_t) 3);
    vTaskSetApplicationTaskTag(vTemperatureReadTaskPassengerHandle,
                               (TaskHookFunction_t) 4);
    vTaskSetApplicationTaskTag(vHeaterControllerTaskDriverHandle,
                               (TaskHookFunction_t) 5);
    vTaskSetApplicationTaskTag(vHeaterControllerTaskPassengerHandle,
                               (TaskHookFunction_t) 6);
    vTaskSetApplicationTaskTag(vHeaterLedsControllerTaskDriverHandle,
                               (TaskHookFunction_t) 7);
    vTaskSetApplicationTaskTag(vHeaterLedsControllerTaskPassengerHandle,
                               (TaskHookFunction_t) 8);
    vTaskSetApplicationTaskTag(vDisplayTaskDriverHandle,
                               (TaskHookFunction_t) 9);
    vTaskSetApplicationTaskTag(vDisplayTaskPassengerHandle,
                               (TaskHookFunction_t) 10);
    vTaskSetApplicationTaskTag(vRunTimeMeasurementsTaskHandle,
                               (TaskHookFunction_t) 11);

    vTaskStartScheduler();

    /* Should never reach here!  If you do then there was not enough heap
     available for the idle task to be created. */
    for (;;)
        ;
}

/*------------------------ Handler Functions -------------------------*/
void GPIOPortF_Handler(void)
{
    BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
    if (GPIO_PORTF_RIS_REG & (1 << 0))
    { /* PF0 handler code for the DRIVER SEAT  */
        xEventGroupSetBitsFromISR(eventTempSet, mainSW1_INTERRUPT_BIT,
                                  &pxHigherPriorityTaskWoken);
        DriverState++;
        DriverState %= 4;
        GPIO_PORTF_ICR_REG |= (1 << 0); /* Clear Trigger flag for PF0 (Interrupt Flag) */
    }
    else if (GPIO_PORTF_RIS_REG & (1 << 4))
    { /* PF4 handler code for the PASSENGER SEAT */
        xEventGroupSetBitsFromISR(eventTempSet, mainSW2_INTERRUPT_BIT,
                                  &pxHigherPriorityTaskWoken);
        PassengerState++;
        PassengerState %= 4;
        GPIO_PORTF_ICR_REG |= (1 << 4); /* Clear Trigger flag for PF4 (Interrupt Flag) */
    }
}

void GPIOPortB_Handler(void)
{
    BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
    if (GPIO_PORTB_RIS_REG & (1 << 0))
    { /* PB0 handler code for the DRIVER SEAT  */
        xEventGroupSetBitsFromISR(eventTempSet, mainSW1_INTERRUPT_BIT,
                                  &pxHigherPriorityTaskWoken);
        DriverState++;
        DriverState %= 4;
        GPIO_PORTB_ICR_REG |= (1 << 0); /* Clear Trigger flag for PB0 (Interrupt Flag) */
    }
}

/*---------------------------- Functions -------------------------------*/

//Sensor Reading Function
void vTempReadingTask(void *pvParameters)
{
    const TickType_t xDelay = pdMS_TO_TICKS(200UL);

    uint8 SeatSelect = *((uint8*) pvParameters);
    float32 adc_value;

    TickType_t xStartTime, xEndTime;
    for (;;)
    {
        adc_value = ((float32) ADC0_readChannel())
                * ((float) MAXTEMPERATURE / MAXVOLTAGEADC);

        xStartTime = xTaskGetTickCount();
        if (SeatSelect == ISDRIVER)
        {
            if (xSemaphoreTake(CurrentTempMutexDriver, portMAX_DELAY) == pdTRUE)
            {
                CurrentTempDriver = adc_value;
                xSemaphoreGive(CurrentTempMutexDriver); /* Release the resource */
            }
            xEndTime = xTaskGetTickCount();
            CurrentTempReadingTaskDriverLT += xEndTime - xStartTime;

            xQueueSend(Reading_DisplayDriver, &adc_value, portMAX_DELAY);
        }
        else if (SeatSelect == ISPASSENGER)
        {
            if (xSemaphoreTake(CurrentTempMutexPassenger,
                    portMAX_DELAY) == pdTRUE)
            {
                CurrentTempPassenger = adc_value;
                xSemaphoreGive(CurrentTempMutexPassenger); /* Release the resource */
            }
            xEndTime = xTaskGetTickCount();
            CurrentTempReadingTaskPassengerLT += xEndTime - xStartTime;

            xQueueSend(Reading_DisplayPassenger, &adc_value, portMAX_DELAY);
        }

        vTaskDelay(xDelay);
    }
}

void vTempSettingTask(void *pvParameters)
{
    EventBits_t xEventGroupValue;
    const EventBits_t xBitsToWaitFor = ( mainSW1_INTERRUPT_BIT
            | mainSW2_INTERRUPT_BIT);

    TickType_t xStartTime, xEndTime;
    for (;;)
    {
        /* Block to wait for event bits to become set within the event group. */
        xEventGroupValue = xEventGroupWaitBits(eventTempSet, /* The event group to read. */
                                               xBitsToWaitFor, /* Bits to test. */
                                               pdTRUE, /* Clear bits on exit if the unblock condition is met. */
                                               pdFALSE, /* Dont't Wait for all bits. */
                                               portMAX_DELAY); /* Don't time out. */

        if ((xEventGroupValue & mainSW1_INTERRUPT_BIT) != 0)
        { /*Driver*/
            xStartTime = xTaskGetTickCount();
            if (xSemaphoreTake(DesiredTempMutexDriver, portMAX_DELAY) == pdTRUE)
            {
                switch (DriverState)
                { /* Heat Level */
                case 0:
                    DesiredTempDriver = OFF;
                    break;
                case 1:
                    DesiredTempDriver = LOW;
                    break;
                case 2:
                    DesiredTempDriver = MEDIUM;
                    break;
                case 3:
                    DesiredTempDriver = HIGH;
                    break;
                }
                xSemaphoreGive(DesiredTempMutexDriver); /* Release the resource */
            }
            xEndTime = xTaskGetTickCount();
            DesiredTempSettingTaskDriverLT += xEndTime - xStartTime;
        }

        if ((xEventGroupValue & mainSW2_INTERRUPT_BIT) != 0)
        { /*Passenger*/
            xStartTime = xTaskGetTickCount();
            if (xSemaphoreTake(DesiredTempMutexPassenger,
                    portMAX_DELAY) == pdTRUE)
            {
                switch (PassengerState)
                {
                case 0:
                    DesiredTempPassenger = OFF;
                    break;
                case 1:
                    DesiredTempPassenger = LOW;
                    break;
                case 2:
                    DesiredTempPassenger = MEDIUM;
                    break;
                case 3:
                    DesiredTempPassenger = HIGH;
                    break;
                }
                xSemaphoreGive(DesiredTempMutexPassenger); /* Release the resource */
            }
            xEndTime = xTaskGetTickCount();
            DesiredTempSettingTaskPassengerLT += xEndTime - xStartTime;
        }
    }
}

void vHeaterControllerTask(void *pvParameters)
{
    const TickType_t xDelay = pdMS_TO_TICKS(200UL);

    float32 CurrentTemp = 0;
    UserHeatInput DesiredTemp = OFF;

    uint8 SeatSelect = *((uint8*) pvParameters);

    TickType_t xStartTime, xEndTime;
    for (;;)
    {
        if (SeatSelect == ISDRIVER)
        {
            xStartTime = xTaskGetTickCount();
            if (xSemaphoreTake(CurrentTempMutexDriver, portMAX_DELAY) == pdTRUE)
            {
                CurrentTemp = CurrentTempDriver;
                xSemaphoreGive(CurrentTempMutexDriver); /* Release the resource */
            }
            xEndTime = xTaskGetTickCount();
            CurrentTempControllerTaskDriverLT += xEndTime - xStartTime;

            xStartTime = xTaskGetTickCount();
            if (xSemaphoreTake(DesiredTempMutexDriver, portMAX_DELAY) == pdTRUE)
            {
                DesiredTemp = DesiredTempDriver;
                xSemaphoreGive(DesiredTempMutexDriver); /* Release the resource */
            }
            xEndTime = xTaskGetTickCount();
            DesiredTempControllerTaskDriverLT += xEndTime - xStartTime;

            if (CurrentTemp < 5 || CurrentTemp > 40)
            {
                heatIntensity = ERROR;
            }
            else if ((DesiredTemp - CurrentTemp) >= 10)
            {
                heatIntensity = HIGHINTENSITY;
            }
            else if ((DesiredTemp - CurrentTemp) >= 5
                    && (DesiredTemp - CurrentTemp) < 10)
            {
                heatIntensity = MEDIUMINTENSITY;
            }
            else if ((DesiredTemp - CurrentTemp) >= 2
                    && (DesiredTemp - CurrentTemp) < 5)
            {
                heatIntensity = LOWINTENSITY;
            }
            else
            {
                heatIntensity = INTENSITYOFF;
            }

            xQueueSend(Controller_HeatingDriver, &heatIntensity, portMAX_DELAY);
            xQueueSend(Controller_DisplayDriver, &heatIntensity, portMAX_DELAY); /* Send Heat State to Display */
        }
        else if (SeatSelect == ISPASSENGER)
        {
            xStartTime = xTaskGetTickCount();
            if (xSemaphoreTake(CurrentTempMutexPassenger,
                    portMAX_DELAY) == pdTRUE)
            {
                CurrentTemp = CurrentTempPassenger;
                xSemaphoreGive(CurrentTempMutexPassenger); /* Release the resource */
            }
            xEndTime = xTaskGetTickCount();
            CurrentTempControllerTaskPassengerLT += xEndTime - xStartTime;

            xStartTime = xTaskGetTickCount();
            if (xSemaphoreTake(DesiredTempMutexPassenger,
                    portMAX_DELAY) == pdTRUE)
            {
                DesiredTemp = DesiredTempPassenger;
                xSemaphoreGive(DesiredTempMutexPassenger); /* Release the resource */
            }
            xEndTime = xTaskGetTickCount();
            DesiredTempControllerTaskPassengerLT += xEndTime - xStartTime;

            if (CurrentTemp < 5 || CurrentTemp > 40)
            {
                heatIntensity = ERROR;
                //EEPROMWrite((uint32)Reading, 1, 1) ;
            }
            else if ((DesiredTemp - CurrentTemp) >= 10)
            {
                heatIntensity = HIGHINTENSITY;
            }
            else if ((DesiredTemp - CurrentTemp) >= 5
                    && (DesiredTemp - CurrentTemp) < 10)
            {
                heatIntensity = MEDIUMINTENSITY;
            }
            else if ((DesiredTemp - CurrentTemp) >= 2
                    && (DesiredTemp - CurrentTemp) < 5)
            {
                heatIntensity = LOWINTENSITY;
            }
            else
            {
                heatIntensity = INTENSITYOFF;
            }

            xQueueSend(Controller_HeatingPassenger, &heatIntensity,
                       portMAX_DELAY);
            xQueueSend(Controller_DisplayPassenger, &heatIntensity,
                       portMAX_DELAY); /* Send Heat State to Display */
        }

        vTaskDelay(xDelay);
    }
}

void vHeaterLedsControllerTask(void *pvParameters)
{

    uint8 SeatSelect = *((uint8*) pvParameters);
    HeatIntensity selectedHeatingIntensity;

    for (;;)
    {
        if (SeatSelect == ISDRIVER)
        {
            xQueueReceive(Controller_HeatingDriver, &selectedHeatingIntensity,
            portMAX_DELAY);

            switch (selectedHeatingIntensity)
            {
            case ERROR:
                GPIO_RedLed1On();
                GPIO_BlueLed1Off();
                GPIO_GreenLed1Off();
                break;

            case INTENSITYOFF:
                GPIO_RedLed1Off();
                GPIO_BlueLed1Off();
                GPIO_GreenLed1Off();
                break;

            case LOWINTENSITY:
                GPIO_RedLed1Off();
                GPIO_BlueLed1Off();
                GPIO_GreenLed1On();
                break;

            case MEDIUMINTENSITY:
                GPIO_RedLed1Off();
                GPIO_BlueLed1On();
                GPIO_GreenLed1Off();
                break;

            case HIGHINTENSITY:
                GPIO_RedLed1Off();
                GPIO_BlueLed1On();
                GPIO_GreenLed1On();
                break;
            }
        }
        else if (SeatSelect == ISPASSENGER)
        {
            xQueueReceive(Controller_HeatingPassenger,
                          &selectedHeatingIntensity, portMAX_DELAY);

            switch (selectedHeatingIntensity)
            {
            case ERROR:
                GPIO_RedLed2On();
                GPIO_BlueLed2Off();
                GPIO_GreenLed2Off();
                break;

            case INTENSITYOFF:
                GPIO_RedLed2Off();
                GPIO_BlueLed2Off();
                GPIO_GreenLed2Off();
                break;

            case LOWINTENSITY:
                GPIO_RedLed2Off();
                GPIO_BlueLed2Off();
                GPIO_GreenLed2On();
                break;

            case MEDIUMINTENSITY:
                GPIO_RedLed2Off();
                GPIO_BlueLed2On();
                GPIO_GreenLed2Off();
                break;

            case HIGHINTENSITY:
                GPIO_RedLed2Off();
                GPIO_BlueLed2On();
                GPIO_GreenLed2On();
                break;
            }
        }
    }
}

void vDisplayTask(void *pvParameters)
{

    uint8 SeatSelect = *((uint8*) pvParameters);
    HeatIntensity DriverHeatState, PassengerHeatState;
    uint8 DriverCurrentTemp, PassengerCurrentTemp;
    UserHeatInput DriverHeatLevel, PassengerHeatLevel;

    TickType_t xStartTime, xEndTime;
    for (;;)
    {
        if (SeatSelect == ISDRIVER)
        {
            if (xQueueReceive(Reading_DisplayDriver, &DriverCurrentTemp,
            portMAX_DELAY) == pdTRUE)
            { /* Receive Current Temp to Display */
                if (xQueueReceive(Controller_DisplayDriver, &DriverHeatState,
                portMAX_DELAY) == pdTRUE)
                { /* Receive Heat State to Display */
                    xStartTime = xTaskGetTickCount();
                    if (xSemaphoreTake(DesiredTempMutexDriver,
                            portMAX_DELAY) == pdTRUE)
                    { /* Receive Heat Level to Display */
                        DriverHeatLevel = DesiredTempDriver;
                        xSemaphoreGive(DesiredTempMutexDriver); /* Release the resource */
                    }
                    xEndTime = xTaskGetTickCount();
                    DesiredTempDisplayTaskDriverLT += xEndTime - xStartTime;

                    /********* DISPLAY ON SCREEN USING UART ********/
                    xStartTime = xTaskGetTickCount();
                    if (xSemaphoreTake(UARTMutex, portMAX_DELAY) == pdTRUE)
                    { /* Receive Heat Level to Display */
                        UART0_SendString("Driver:");

                        UART0_SendString("\nCurrent Temperature = ");
                        UART0_SendByte(DriverCurrentTemp);

                        UART0_SendString("\nRequired Heat Level = ");
                        UART0_SendByte(DriverHeatLevel);

                        UART0_SendString("\nThe Heater is Working with ");
                        switch (DriverHeatState)
                        {
                        case (ERROR):
                            UART0_SendString("NO Intensity due to error");
                            break;
                        case (INTENSITYOFF):
                            UART0_SendString("NO Intensity");
                            break;
                        case (LOWINTENSITY):
                            UART0_SendString("LOW Intensity");
                            break;
                        case (MEDIUMINTENSITY):
                            UART0_SendString("MEDIUM Intensity");
                            break;
                        case (HIGHINTENSITY):
                            UART0_SendString("HIGH Intensity");
                            break;
                        }

                        xSemaphoreGive(UARTMutex); /* Release the resource */
                        xEndTime = xTaskGetTickCount();
                        UARTDisplayTaskDriverLT += xEndTime - xStartTime;
                    }
                }
            }
        }
        else if (SeatSelect == ISPASSENGER)
        {
            if (xQueueReceive(Reading_DisplayPassenger, &PassengerCurrentTemp,
            portMAX_DELAY) == pdTRUE)
            { /* Receive Current Temp to Display */
                if (xQueueReceive(Controller_DisplayPassenger,
                                  &PassengerHeatState, portMAX_DELAY) == pdTRUE)
                { /* Receive Heat State to Display */
                    xStartTime = xTaskGetTickCount();
                    if (xSemaphoreTake(DesiredTempMutexPassenger,
                            portMAX_DELAY) == pdTRUE)
                    { /* Receive Heat Level to Display */
                        PassengerHeatLevel = DesiredTempPassenger;
                        xSemaphoreGive(DesiredTempMutexPassenger); /* Release the resource */
                    }
                    xEndTime = xTaskGetTickCount();
                    DesiredTempDisplayTaskPassengerLT += xEndTime - xStartTime;

                    /********* DISPLAY ON SCREEN USING UART ********/
                    xStartTime = xTaskGetTickCount();
                    if (xSemaphoreTake(UARTMutex, portMAX_DELAY) == pdTRUE)
                    { /* Receive Heat Level to Display */
                        UART0_SendString("Passenger:");

                        UART0_SendString("\nCurrent Temperature = ");
                        UART0_SendByte(PassengerCurrentTemp);

                        UART0_SendString("\nRequired Heat Level = ");
                        UART0_SendByte(PassengerHeatLevel);

                        UART0_SendString("\nThe Heater is Working with ");
                        switch (PassengerHeatState)
                        {
                        case (ERROR):
                            UART0_SendString("NO Intensity due to error");
                            break;
                        case (INTENSITYOFF):
                            UART0_SendString("NO Intensity");
                            break;
                        case (LOWINTENSITY):
                            UART0_SendString("LOW Intensity");
                            break;
                        case (MEDIUMINTENSITY):
                            UART0_SendString("MEDIUM Intensity");
                            break;
                        case (HIGHINTENSITY):
                            UART0_SendString("HIGH Intensity");
                            break;
                        }

                        xSemaphoreGive(UARTMutex); /* Release the resource */
                        xEndTime = xTaskGetTickCount();
                        UARTDisplayTaskPassengerLT += xEndTime - xStartTime;
                    }
                }
            }
        }
    }
}

void vRunTimeMeasurementsTask(void *pvParameters)
{
    const TickType_t xDelay = pdMS_TO_TICKS(200UL);
    vTaskDelay(xDelay);

    uint8 ucCounter, ucCPU_Load;
    uint32 ullTotalTasksTime = 0;

    TickType_t xStartTime, xEndTime;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    for (;;)
    {
        ullTotalTasksTime = 0;
        vTaskDelayUntil(&xLastWakeTime, RUNTIME_MEASUREMENTS_TASK_PERIODICITY);
        for (ucCounter = 1; ucCounter <= 11; ucCounter++)
        {
            ullTotalTasksTime += ullTasksExecutionTime[ucCounter];
        }
        ucCPU_Load = (ullTotalTasksTime * 100) / GPTM_WTimer0Read();

        xStartTime = xTaskGetTickCount();
        if (xSemaphoreTake(UARTMutex, portMAX_DELAY) == pdTRUE)
        {
            UART0_SendString("CPU Load is ");
            UART0_SendInteger(ucCPU_Load);
            UART0_SendString("% \r\n");
            /* Release the peripheral */
            xSemaphoreGive(UARTMutex);
        }
        xEndTime = xTaskGetTickCount();
        UARTRunTimeMeasurementsTaskLT = xEndTime - xStartTime;
    }
}
