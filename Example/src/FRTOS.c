/*
===============================================================================
 Name        : FRTOS.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/
#include "chip.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

SemaphoreHandle_t Semaforo_1;
SemaphoreHandle_t Semaforo_2;

#include <cr_section_macros.h>

// TODO: insert other include files here

// TODO: insert other definitions and declarations here
#define PORT(x) 	((uint8_t) x)
#define PIN(x)		((uint8_t) x)

#define OUTPUT		((uint8_t) 1)
#define INPUT		((uint8_t) 0)


void uC_StartUp (void)
{
	Chip_GPIO_Init (LPC_GPIO);

	Chip_IOCON_PinMux (LPC_IOCON , PORT(0) , PIN(22), IOCON_MODE_INACT , IOCON_FUNC0);

	Chip_GPIO_SetDir (LPC_GPIO , PORT(0) , PIN(22) , OUTPUT);
}


/* LED1 toggle thread */
static void vTask1(void *pvParameters)
{
	while (1)
	{
		xSemaphoreTake(Semaforo_2 , portMAX_DELAY );

		Chip_GPIO_SetPinOutHigh (LPC_GPIO , PORT(0) , PIN(22));

		vTaskDelay( 500 / portTICK_PERIOD_MS );

		xSemaphoreGive(Semaforo_1 );
	}
}

/* LED1 toggle thread */
static void xTask2(void *pvParameters)
{
	while (1)
	{
		xSemaphoreTake(Semaforo_1 , portMAX_DELAY );

		Chip_GPIO_SetPinOutLow (LPC_GPIO , PORT(0) , PIN(22));

		vTaskDelay( 500 / portTICK_PERIOD_MS );

		xSemaphoreGive(Semaforo_2 );
	}
}


int main(void)
{
	uC_StartUp ();
	SystemCoreClockUpdate();

	vSemaphoreCreateBinary(Semaforo_1);
	vSemaphoreCreateBinary(Semaforo_2);

	xSemaphoreTake(Semaforo_1 , portMAX_DELAY );

	xTaskCreate(vTask1, (char *) "vTaskLed1",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
				(xTaskHandle *) NULL);

	xTaskCreate(xTask2, (char *) "vTaskLed2",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
				(xTaskHandle *) NULL);

	/* Start the scheduler */
	vTaskStartScheduler();

	/* Nunca debería arribar aquí */

    return 0;
}

