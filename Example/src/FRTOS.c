/*
===============================================================================
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
#include "queue.h"

SemaphoreHandle_t Semaforo_1;
SemaphoreHandle_t Semaforo_2;

QueueHandle_t	  ColaADC;

#include <cr_section_macros.h>

// TODO: insert other include files here

// TODO: insert other definitions and declarations here
#define PORT(x) 	((uint8_t) x)
#define PIN(x)		((uint8_t) x)

#define OUTPUT		((uint8_t) 1)
#define INPUT		((uint8_t) 0)


//Placa Infotronic
#define LED_STICK	PORT(0),PIN(22)
#define	BUZZER		PORT(0),PIN(28)
#define	SW1			PORT(2),PIN(10)
#define SW2			PORT(0),PIN(18)
#define	SW3			PORT(0),PIN(11)
#define SW4			PORT(2),PIN(13)
#define SW5			PORT(1),PIN(26)
#define	LED1		PORT(2),PIN(0)
#define	LED2		PORT(0),PIN(23)
#define	LED3		PORT(0),PIN(21)
#define	LED4		PORT(0),PIN(27)
#define	RGBB		PORT(2),PIN(1)
#define	RGBG		PORT(2),PIN(2)
#define	RGBR		PORT(2),PIN(3)

#define LIMITE_A	0
#define LIMITE_B	1300
#define LIMITE_C	2600

static ADC_CLOCK_SETUP_T ADCSetup;

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

void ADC_IRQHandler(void)
{
	uint16_t dataADC;
	BaseType_t testigo = pdFALSE;

	if(Chip_ADC_ReadValue(LPC_ADC, ADC_CH5, &dataADC))//si da 1 significa que la conversión estaba realizada
	{
		xQueueSendToBackFromISR(ColaADC, &dataADC, &testigo);
		portYIELD_FROM_ISR(testigo);
	}

}

static void ADC_Config(void *pvParameters)
{
	Chip_IOCON_PinMux(LPC_IOCON, 1, 31, IOCON_MODE_INACT, IOCON_FUNC3); // Entrada analogica 0 (Infotronik)

	//Chip_ADC_ReadStatus(_LPC_ADC_ID, _ADC_CHANNLE, ADC_DR_DONE_STAT)

	Chip_ADC_Init(LPC_ADC, &ADCSetup);
	Chip_ADC_EnableChannel(LPC_ADC, ADC_CH5, ENABLE);
	Chip_ADC_SetSampleRate(LPC_ADC, &ADCSetup, 50000);
	Chip_ADC_Int_SetChannelCmd(LPC_ADC, ADC_CH5, ENABLE);
	Chip_ADC_SetBurstCmd(LPC_ADC, DISABLE);

	NVIC_ClearPendingIRQ(ADC_IRQn);
	NVIC_EnableIRQ(ADC_IRQn);

	Chip_ADC_SetStartMode (LPC_ADC, ADC_START_NOW, ADC_TRIGGERMODE_RISING);

	vTaskDelete(NULL);

}

static void taskAnalisis(void *pvParameters)
{
	uint16_t dato;

	while(1)
	{
		xQueueReceive( ColaADC, &dato, portMAX_DELAY );

		if ( dato >= LIMITE_A && dato <= LIMITE_B )
		{
			Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBG);
			Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBB);
			Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBR);

			Chip_GPIO_SetPinOutHigh(LPC_GPIO, RGBG);
		}
		else if ( dato > LIMITE_B && dato <= LIMITE_C )
		{
			Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBG);
			Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBB);
			Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBR);

			Chip_GPIO_SetPinOutHigh(LPC_GPIO, RGBG);
			Chip_GPIO_SetPinOutHigh(LPC_GPIO, RGBR);

		}
		else
		{
			Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBG);
			Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBB);
			Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBR);

			Chip_GPIO_SetPinOutHigh(LPC_GPIO, RGBG);
			Chip_GPIO_SetPinOutHigh(LPC_GPIO, RGBR);
			Chip_GPIO_SetPinOutHigh(LPC_GPIO, RGBB);
		}

		Chip_ADC_SetStartMode (LPC_ADC, ADC_START_NOW, ADC_TRIGGERMODE_RISING);
	}
}

void uC_StartUp (void)
{
	Chip_GPIO_Init (LPC_GPIO);
	Chip_GPIO_SetDir (LPC_GPIO, LED_STICK, OUTPUT);
	Chip_IOCON_PinMux (LPC_IOCON, LED_STICK, IOCON_MODE_INACT, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, BUZZER, OUTPUT);
	Chip_IOCON_PinMux (LPC_IOCON, BUZZER, IOCON_MODE_INACT, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, RGBB, OUTPUT);
	Chip_IOCON_PinMux (LPC_IOCON, RGBB, IOCON_MODE_INACT, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, RGBG, OUTPUT);
	Chip_IOCON_PinMux (LPC_IOCON, RGBG, IOCON_MODE_INACT, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, RGBR, OUTPUT);
	Chip_IOCON_PinMux (LPC_IOCON, RGBR, IOCON_MODE_INACT, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, LED1, OUTPUT);
	Chip_IOCON_PinMux (LPC_IOCON, LED1, IOCON_MODE_INACT, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, LED2, OUTPUT);
	Chip_IOCON_PinMux (LPC_IOCON, LED2, IOCON_MODE_INACT, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, LED3, OUTPUT);
	Chip_IOCON_PinMux (LPC_IOCON, LED3, IOCON_MODE_INACT, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, LED4, OUTPUT);
	Chip_IOCON_PinMux (LPC_IOCON, LED4, IOCON_MODE_INACT, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, SW1, INPUT);
	Chip_IOCON_PinMux (LPC_IOCON, SW1, IOCON_MODE_PULLDOWN, IOCON_FUNC0);

	//Salidas apagadas
	Chip_GPIO_SetPinOutLow(LPC_GPIO, LED_STICK);
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, BUZZER);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBR);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBG);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBB);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, LED1);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, LED2);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, LED3);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, LED4);
}

int main(void)
{
	uC_StartUp ();
	SystemCoreClockUpdate();

	vSemaphoreCreateBinary(Semaforo_1);
	vSemaphoreCreateBinary(Semaforo_2);

	ColaADC = xQueueCreate (1, sizeof(uint16_t));

	xSemaphoreTake(Semaforo_1 , portMAX_DELAY );

	xTaskCreate(taskAnalisis, (char *) "taskAnalisis",
					configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
					(xTaskHandle *) NULL);

	xTaskCreate(ADC_Config, (char *) "ADC_Config",
					configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 2UL),
					(xTaskHandle *) NULL);
/*
	xTaskCreate(vTask1, (char *) "vTaskLed1",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
				(xTaskHandle *) NULL);

	xTaskCreate(xTask2, (char *) "vTaskLed2",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
				(xTaskHandle *) NULL);
*/
	/* Start the scheduler */
	vTaskStartScheduler();

	/* Nunca debería arribar aquí */

    return 0;
}
