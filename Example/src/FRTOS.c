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

SemaphoreHandle_t Semaforo_Valvula;
SemaphoreHandle_t Semaforo_2;

QueueHandle_t	  ColaADCPre,ColaADCTemp;

#include <cr_section_macros.h>

// TODO: insert other include files here

// TODO: insert other definitions and declarations here
#define PORT(x) 	((uint8_t) x)
#define PIN(x)		((uint8_t) x)

#define OUTPUT		((uint8_t) 1)
#define INPUT		((uint8_t) 0)

#define SEN_TEMP	ADC_CH5
#define SEN_PRE		ADC_CH4


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

#define VALVULA		PORT(0),PIN(8)
#define PUL_EMER	PORT(0),PIN(9)

#define LIMITE_INF_PRE	512 	//0.4125V es 69BAR que son 512 cuentas
#define LIMITE_SUP_PRE	3584	//2.8875V es 75BAR que son 3584 cuentas
#define LIMITE_SUP_TEMP	3840	//3.09375V es 65°C que son 3840 cuentas

static ADC_CLOCK_SETUP_T ADCSetup;

//*********************************************************************************************************************

void SendRS485 (LPC_USART_T *pUART, const void *data, int numBytes)
{

}

//*********************************************************************************************************************

/* Apertura de valvula y deshabilito ADC */
static void xValvula(void *pvParameters)
{
	while (1)
	{
		xSemaphoreTake(Semaforo_Valvula , portMAX_DELAY );

		Chip_GPIO_SetPinOutHigh (LPC_GPIO , VALVULA);

		NVIC_DisableIRQ(ADC_IRQn);

		NVIC_ClearPendingIRQ(ADC_IRQn);
	}
}

//*********************************************************************************************************************

void ADC_IRQHandler(void)
{
	uint16_t dataADC;
	BaseType_t testigo = pdFALSE;

	if(Chip_ADC_ReadValue(LPC_ADC, SEN_TEMP, &dataADC))//si da 1 significa que la conversión estaba realizada
	{
		xQueueSendToBackFromISR(ColaADCTemp, &dataADC, &testigo);
		portYIELD_FROM_ISR(testigo);
	}

	if(Chip_ADC_ReadValue(LPC_ADC, SEN_PRE, &dataADC))//si da 1 significa que la conversión estaba realizada
	{
		xQueueSendToBackFromISR(ColaADCPre, &dataADC, &testigo);
		portYIELD_FROM_ISR(testigo);
	}

}

//*********************************************************************************************************************

static void ADC_Config(void *pvParameters)
{
	Chip_IOCON_PinMux(LPC_IOCON, 1, 31, IOCON_MODE_INACT, IOCON_FUNC3); // Entrada analogica 0 (Infotronik) ch5
	Chip_IOCON_PinMux(LPC_IOCON, 1, 30, IOCON_MODE_INACT, IOCON_FUNC3); // Entrada analogica 0 (Infotronik) ch5

	//Chip_ADC_ReadStatus(_LPC_ADC_ID, _ADC_CHANNLE, ADC_DR_DONE_STAT)

	Chip_ADC_Init(LPC_ADC, &ADCSetup);
	Chip_ADC_EnableChannel(LPC_ADC, SEN_TEMP, ENABLE);
	Chip_ADC_EnableChannel(LPC_ADC, SEN_PRE, ENABLE);

	Chip_ADC_SetSampleRate(LPC_ADC, &ADCSetup, 20000);
	Chip_ADC_Int_SetChannelCmd(LPC_ADC, SEN_TEMP, ENABLE);
	Chip_ADC_Int_SetChannelCmd(LPC_ADC, SEN_PRE, ENABLE);
	Chip_ADC_SetBurstCmd(LPC_ADC, DISABLE);

	NVIC_ClearPendingIRQ(ADC_IRQn);
	NVIC_EnableIRQ(ADC_IRQn);

	Chip_ADC_SetStartMode (LPC_ADC, ADC_START_NOW, ADC_TRIGGERMODE_RISING);

	vTaskDelete(NULL);

}

//*********************************************************************************************************************

static void taskAnalisis(void *pvParameters)
{
	uint16_t datoTemp[4],datoPre[4],i=0,totalPre,totalTemp;
	char EMerPre[2]={0xAA,0xF1},EMerTemp[2]={0xAA,0xF5};
	while(1)
	{

		if(uxQueueMessagesWaiting(ColaADCPre) == 4)
		{
			for(i=0;i<4;i++)
			xQueueReceive( ColaADCPre, &datoPre[i], portMAX_DELAY );
			totalPre = (datoPre[0]+datoPre[1]+datoPre[2]+datoPre[3])/4;
			if(totalPre > LIMITE_SUP_PRE || totalPre < LIMITE_INF_PRE)
			{
				xSemaphoreGive(Semaforo_Valvula );
				SendRS485(LPC_UART0,(void*)EMerPre,2);
			}
		}


		if(uxQueueMessagesWaiting(ColaADCTemp) == 4)
		{
			for(i=0;i<4;i++)
			xQueueReceive( ColaADCTemp, &datoTemp[i], portMAX_DELAY );

			totalTemp = (datoTemp[0]+datoTemp[1]+datoTemp[2]+datoTemp[3])/4;

			if(totalTemp > LIMITE_SUP_TEMP)
			{
				xSemaphoreGive(Semaforo_Valvula );
				SendRS485(LPC_UART0,(void*)EMerTemp,2);
			}
		}

		Chip_ADC_SetStartMode (LPC_ADC, ADC_START_NOW, ADC_TRIGGERMODE_RISING);
		vTaskDelay( 250 / portTICK_PERIOD_MS );//ESTO SIGNIFICA QUE CONVIERTO CADA 250ms

	}
}

//*********************************************************************************************************************
/* ENTRADAS */
static void vTaskPulsadores(void *pvParameters)
{
	uint32_t EstadoValvula = FALSE;
	char EMerPuls[2]={0xAA,0xFA};


	while (1)
	{
		vTaskDelay( 50 / portTICK_PERIOD_MS );//Muestreo cada 50 mseg

		//Control de stop
		if(!Chip_GPIO_GetPinState(LPC_GPIO, PUL_EMER) && EstadoValvula == FALSE)
		{
				xSemaphoreGive(Semaforo_Valvula );
				SendRS485(LPC_UART0,(void*)EMerPuls,2);
				EstadoValvula = TRUE;
		}

		if(Chip_GPIO_GetPinState(LPC_GPIO, PUL_EMER) && EstadoValvula == TRUE)//significa que volvio a la normalidad
		{
			Chip_GPIO_SetPinOutLow (LPC_GPIO , VALVULA);
			NVIC_ClearPendingIRQ(ADC_IRQn);
			NVIC_EnableIRQ(ADC_IRQn);
			Chip_ADC_SetStartMode (LPC_ADC, ADC_START_NOW, ADC_TRIGGERMODE_RISING);
			EstadoValvula = FALSE;
		}
	}
}
//*********************************************************************************************************************
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

	Chip_GPIO_SetDir (LPC_GPIO, PUL_EMER, INPUT);
	Chip_IOCON_PinMux (LPC_IOCON, PUL_EMER, IOCON_MODE_PULLDOWN, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, VALVULA, OUTPUT);
	Chip_IOCON_PinMux (LPC_IOCON, VALVULA, IOCON_MODE_PULLDOWN, IOCON_FUNC0);


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

	vSemaphoreCreateBinary(Semaforo_Valvula);
	vSemaphoreCreateBinary(Semaforo_2);

	ColaADCPre = xQueueCreate (4, sizeof(uint16_t));
	ColaADCTemp = xQueueCreate (4, sizeof(uint16_t));


	xSemaphoreTake(Semaforo_Valvula , portMAX_DELAY );

	xTaskCreate(taskAnalisis, (char *) "taskAnalisis",
					configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
					(xTaskHandle *) NULL);

	xTaskCreate(ADC_Config, (char *) "ADC_Config",
					configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 3UL),
					(xTaskHandle *) NULL);

	xTaskCreate(vTaskPulsadores, (char *) "vTaskPulsadores",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 2UL),
				(xTaskHandle *) NULL);

	xTaskCreate(xValvula, (char *) "xValvula",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 2UL),
				(xTaskHandle *) NULL);

	/* Start the scheduler */
	vTaskStartScheduler();

	/* Nunca debería arribar aquí */

    return 0;
}
