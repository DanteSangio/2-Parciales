/*
TECNICAS DIGITALES II - 2do PARCIAL TEMA 84 - Parte practica:

El sistema estará encargado de monitorear una vez por segundo a 04 (cuatro) sensores de temperatura, ubicados en
distintos puntos de una cámara frigorífica, y, en función de los valores medidos, se encargará del accionamiento de 04
(cuatro) equipos de acondicionamiento de temperatura.
Siendo que los sensores se encontrarán en áreas susceptibles a inducciones de ruido, se utilizarán lazos 4-20 mA para
interconectarlos con la placa central.
Las especificaciones técnicas de los sensores son:
			Temperatura[°C] 	Corriente
			-40°C 				04 mA
			 10°C 				20 mA
Se solicita:
a) Desarrollar toda la electrónica necesaria, incluido el amplificador de transresistencia para convertir los 4-20mA
a 1 - 2,5 Volts de forma que pueda ser leído por el conversor ADC del microcontrolador. Realice el/los diagrama/s
esquemático/s correspondiente/s.
b) Deberá desarrollar el sistema de control del tipo ON/OFF de la cámara frigorífica. La variable a controlar será la
Temperatura. Para garantizar el correcto mantenimiento de los alimentos, la temperatura deberá encontrarse en todo
momento entre -25°C y -10°C en los 04 sectores monitoreados de la cámara. Para esto, deberá:
• Si Tx < -25°C, apague acondicionador de temperatura correspondiente (Acx)
• Si Tx > -10°C, encienda acondicionador de temperatura correspondiente (Acx)
c) Si en algún momento, la temperatura supera los -5 °C en cualquiera de los 04 (cuatro) sensores de
temperatura, el sistema deberá accionar una alarma sonora de 220V por 30 segundos.
d) Si en algún momento, la temperatura supera los 0 °C en cualquiera de los 04 (cuatro) sensores de temperatura.
El sistema deberá guardar en la posición 0x10 de una memoria EEPROM externa (conectada al módulo I2C1 bajo
direccionamiento físico 0x00) el valor 0xAA y apagar el sistema siendo que los alimentos deberán ser descartados. Este
parámetro será auditado ante algún control bromatológico.
Reglas de diseño:
• Se encuentra prohibida la utilización de variables globales.
• El sistema deberá ser desarrollado íntegramente bajo FreeRTOS utilizando los mecanismos de sincronización
provistos por el sistema operativo.
• Las temporizaciones deberán generarse con los Timers provistos por el microcontrolador.
• El acceso al hardware deberá ser desarrollado utilizando LPCOpen.
*/


//*********************************************************************************************************************
//INCLUDES
//*********************************************************************************************************************

#include "chip.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include <cr_section_macros.h>


//*********************************************************************************************************************
//DEFINES
//*********************************************************************************************************************

#define PORT(x) 	((uint8_t) x)
#define PIN(x)		((uint8_t) x)

#define OUTPUT		((uint8_t) 1)
#define INPUT		((uint8_t) 0)

#define SEN_1		ADC_CH5
#define SEN_2		ADC_CH4
#define SEN_3		ADC_CH6
#define SEN_4		ADC_CH7

//I2C
#define PUERTO_0  	 		0
#define I2C1				1
#define PIN_SDA1 	   	   19
#define PIN_SCL1  	   	   20
#define NEITHER				2
#define I2C_FUNC     		3
#define SLAVE_ADDRESS		0x50 	// 1010 bits x default de la memorio 0000 los primeros 3 son el direccionamiento fisico
#define W_ADDRESS			0x0010 	// posicion donde voy a escribir
#define DATO				0xAA

//Placa Infotronic
#define LED_STICK	PORT(0),PIN(22)
#define	BUZZER		PORT(0),PIN(28)
#define	SW1			PORT(2),PIN(10)
#define SW2			PORT(0),PIN(18)
#define	SW3			PORT(0),PIN(11)
#define SW4			PORT(2),PIN(13)
#define SW5			PORT(1),PIN(26)
#define	AIRE1		PORT(2),PIN(0)
#define	AIRE2		PORT(0),PIN(23)
#define	AIRE3		PORT(0),PIN(21)
#define	AIRE4		PORT(0),PIN(27)
#define	RGBB		PORT(2),PIN(1)
#define	RGBG		PORT(2),PIN(2)
#define	RGBR		PORT(2),PIN(3)

#define SONORA		PORT(0),PIN(8)
#define PUL_EMER	PORT(0),PIN(9)

#define LIMITE_INF_TEMP	1800	//1.45V es -25°C que son 1800 cuentas
#define LIMITE_SUP_TEMP	2358	//1.9V  es -10°C que son 2358 cuentas
#define LIMITE_SON_TEMP	2544	//2.05V es -5°C que son 2544 cuentas
#define LIMITE_MAX_TEMP	2730	//2.2V  es 0°C que son 2730 cuentas

//*********************************************************************************************************************
//DECLARACIONES
//*********************************************************************************************************************

SemaphoreHandle_t Semaforo_Sonoro;
SemaphoreHandle_t Semaforo_2;

QueueHandle_t	  ColaADCZona1,ColaADCZona2,ColaADCZona3,ColaADCZona4;

static ADC_CLOCK_SETUP_T ADCSetup;


//*********************************************************************************************************************

void I2C1_IRQHandler(void)
{
	Chip_I2C_MasterStateHandler (I2C1);
}


//*********************************************************************************************************************

void ADC_IRQHandler(void)
{
	BaseType_t testigo = pdFALSE;
	uint16_t dataADC;

	if(Chip_ADC_ReadValue(LPC_ADC, SEN_1, &dataADC))//si da 1 significa que la conversión estaba realizada
	{
		xQueueSendToBackFromISR(ColaADCZona1, &dataADC, &testigo);
		portYIELD_FROM_ISR(testigo);
	}

	if(Chip_ADC_ReadValue(LPC_ADC, SEN_2, &dataADC))//si da 1 significa que la conversión estaba realizada
	{
		xQueueSendToBackFromISR(ColaADCZona2, &dataADC, &testigo);
		portYIELD_FROM_ISR(testigo);
	}

	if(Chip_ADC_ReadValue(LPC_ADC, SEN_3, &dataADC))//si da 1 significa que la conversión estaba realizada
	{
		xQueueSendToBackFromISR(ColaADCZona3, &dataADC, &testigo);
		portYIELD_FROM_ISR(testigo);
	}

	if(Chip_ADC_ReadValue(LPC_ADC, SEN_4, &dataADC))//si da 1 significa que la conversión estaba realizada
	{
		xQueueSendToBackFromISR(ColaADCZona4, &dataADC, &testigo);
		portYIELD_FROM_ISR(testigo);
	}
}


//*********************************************************************************************************************

static void ADC_Config(void *pvParameters)
{
	Chip_IOCON_PinMux(LPC_IOCON, 1, 31, IOCON_MODE_INACT, IOCON_FUNC3); // Entrada analogica 0 (Infotronik) ch5
	Chip_IOCON_PinMux(LPC_IOCON, 1, 30, IOCON_MODE_INACT, IOCON_FUNC3); // Entrada analogica 0 (Infotronik) ch5

	Chip_ADC_Init(LPC_ADC, &ADCSetup);
	Chip_ADC_EnableChannel(LPC_ADC, SEN_1, ENABLE);
	Chip_ADC_EnableChannel(LPC_ADC, SEN_2, ENABLE);
	Chip_ADC_EnableChannel(LPC_ADC, SEN_3, ENABLE);
	Chip_ADC_EnableChannel(LPC_ADC, SEN_4, ENABLE);

	Chip_ADC_SetSampleRate(LPC_ADC, &ADCSetup, 50000);
	Chip_ADC_Int_SetChannelCmd(LPC_ADC, SEN_1, ENABLE);
	Chip_ADC_Int_SetChannelCmd(LPC_ADC, SEN_2, ENABLE);
	Chip_ADC_Int_SetChannelCmd(LPC_ADC, SEN_3, ENABLE);
	Chip_ADC_Int_SetChannelCmd(LPC_ADC, SEN_4, ENABLE);
	Chip_ADC_SetBurstCmd(LPC_ADC, DISABLE);

	NVIC_ClearPendingIRQ(ADC_IRQn);
	NVIC_EnableIRQ(ADC_IRQn);

	Chip_ADC_SetStartMode (LPC_ADC, ADC_START_NOW, ADC_TRIGGERMODE_RISING);

	vTaskDelete(NULL);
}

//*********************************************************************************************************************

static void I2C_Config(void *pvParameters)
{
	Chip_IOCON_PinMux (LPC_IOCON, PUERTO_0, PIN_SDA1 , NEITHER , I2C_FUNC );
	Chip_IOCON_PinMux (LPC_IOCON, PUERTO_0, PIN_SCL1 , NEITHER , I2C_FUNC );
	Chip_IOCON_EnableOD (LPC_IOCON, PUERTO_0, PIN_SDA1);
	Chip_IOCON_EnableOD (LPC_IOCON, PUERTO_0, PIN_SCL1);

	Chip_I2C_Init (I2C1);
	Chip_I2C_SetClockRate (I2C1, 100000);

	Chip_I2C_SetMasterEventHandler (I2C1, Chip_I2C_EventHandler);
	NVIC_EnableIRQ(I2C1_IRQn);

	vTaskDelete(NULL);
}

//*********************************************************************************************************************

static void taskAnalisis(void *pvParameters)
{
	uint16_t datoTemp[4],i=0;
	unsigned char Datos_Tx [] = { (W_ADDRESS & 0xFF00) >> 8 , W_ADDRESS & 0x00FF, DATO};

	while(1)
	{
		Chip_ADC_SetStartMode (LPC_ADC, ADC_START_NOW, ADC_TRIGGERMODE_RISING);

		//Me aseguro que los datos ya esten en las colas:
		xQueueReceive( ColaADCZona1, &datoTemp[0], portMAX_DELAY );
		xQueueReceive( ColaADCZona2, &datoTemp[1], portMAX_DELAY );
		xQueueReceive( ColaADCZona3, &datoTemp[2], portMAX_DELAY );
		xQueueReceive( ColaADCZona4, &datoTemp[3], portMAX_DELAY );

		for(i=0;i<4;i++)
		{
			if(datoTemp[i] > LIMITE_SUP_TEMP)
			{
				//Se debe encender el aire correspondiente  a la zona i:
				switch(i)
				{
					case 1:
						Chip_GPIO_SetPinOutHigh (LPC_GPIO , AIRE1);
					break;
					case 2:
						Chip_GPIO_SetPinOutHigh (LPC_GPIO , AIRE2);
					break;
					case 3:
						Chip_GPIO_SetPinOutHigh (LPC_GPIO , AIRE3);
					break;
					case 4:
						Chip_GPIO_SetPinOutHigh (LPC_GPIO , AIRE4);
					break;
				}
			}

			if(datoTemp[i] < LIMITE_INF_TEMP)
			{
				//Se debe apagar el aire correspondiente  a la zona i:
				switch(i)
				{
					case 1:
						Chip_GPIO_SetPinOutLow (LPC_GPIO , AIRE1);
					break;
					case 2:
						Chip_GPIO_SetPinOutLow (LPC_GPIO , AIRE2);
					break;
					case 3:
						Chip_GPIO_SetPinOutLow (LPC_GPIO , AIRE3);
					break;
					case 4:
						Chip_GPIO_SetPinOutLow (LPC_GPIO , AIRE4);
					break;
				}
			}

			if(datoTemp[i] > LIMITE_SON_TEMP)
			{
				//Se debe activar la alarma sonora:
				xSemaphoreGive(Semaforo_Sonoro);
			}

			if(datoTemp[i] > LIMITE_MAX_TEMP)
			{
				//Se debe apagar los aires y guardar en memoria EEPROM externa:
				Chip_GPIO_SetPinOutLow (LPC_GPIO , AIRE1);
				Chip_GPIO_SetPinOutLow (LPC_GPIO , AIRE2);
				Chip_GPIO_SetPinOutLow (LPC_GPIO , AIRE3);
				Chip_GPIO_SetPinOutLow (LPC_GPIO , AIRE4);
				NVIC_ClearPendingIRQ(ADC_IRQn);
				NVIC_DisableIRQ(ADC_IRQn);
			    Chip_I2C_MasterSend (I2C1, SLAVE_ADDRESS, Datos_Tx,3); //Selecciono el lugar y escribo el dato
			}
		}
	}
}


//*********************************************************************************************************************

static void vTaskSonora(void *pvParameters)
{
	while(1)
	{
		xSemaphoreTake(Semaforo_Sonoro , portMAX_DELAY );
		Chip_GPIO_SetPinOutHigh (LPC_GPIO , SONORA);
		vTaskDelay( 30000 / portTICK_PERIOD_MS );		//Delay de 30 seg
		Chip_GPIO_SetPinOutLow (LPC_GPIO , SONORA);
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
	Chip_GPIO_SetDir (LPC_GPIO, AIRE1, OUTPUT);
	Chip_IOCON_PinMux (LPC_IOCON, AIRE1, IOCON_MODE_INACT, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, AIRE2, OUTPUT);
	Chip_IOCON_PinMux (LPC_IOCON, AIRE2, IOCON_MODE_INACT, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, AIRE3, OUTPUT);
	Chip_IOCON_PinMux (LPC_IOCON, AIRE3, IOCON_MODE_INACT, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, AIRE4, OUTPUT);
	Chip_IOCON_PinMux (LPC_IOCON, AIRE4, IOCON_MODE_INACT, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, SW1, INPUT);
	Chip_IOCON_PinMux (LPC_IOCON, SW1, IOCON_MODE_PULLDOWN, IOCON_FUNC0);

	Chip_GPIO_SetDir (LPC_GPIO, PUL_EMER, INPUT);
	Chip_IOCON_PinMux (LPC_IOCON, PUL_EMER, IOCON_MODE_PULLDOWN, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, SONORA, OUTPUT);
	Chip_IOCON_PinMux (LPC_IOCON, SONORA, IOCON_MODE_PULLDOWN, IOCON_FUNC0);


	//Salidas apagadas
	Chip_GPIO_SetPinOutLow(LPC_GPIO, LED_STICK);
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, BUZZER);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBR);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBG);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBB);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, AIRE1);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, AIRE2);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, AIRE3);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, AIRE4);
}

int main(void)
{
	uC_StartUp ();

	SystemCoreClockUpdate();

	vSemaphoreCreateBinary(Semaforo_Sonoro);
	vSemaphoreCreateBinary(Semaforo_2);

	ColaADCZona1 = xQueueCreate (1, sizeof(uint16_t));
	ColaADCZona2 = xQueueCreate (1, sizeof(uint16_t));
	ColaADCZona3 = xQueueCreate (1, sizeof(uint16_t));
	ColaADCZona4 = xQueueCreate (1, sizeof(uint16_t));

	xSemaphoreTake(Semaforo_Sonoro , portMAX_DELAY );

	xTaskCreate(taskAnalisis, (char *) "taskAnalisis",
					configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
					(xTaskHandle *) NULL);

	xTaskCreate(ADC_Config, (char *) "ADC_Config",
					configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 3UL),
					(xTaskHandle *) NULL);

	xTaskCreate(I2C_Config, (char *) "I2C_Config",
					configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 3UL),
					(xTaskHandle *) NULL);

	xTaskCreate(vTaskSonora, (char *) "vTaskSonora",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 2UL),
				(xTaskHandle *) NULL);

	/* Start the scheduler */
	vTaskStartScheduler();

	/* Nunca debería arribar aquí */
    return 0;
}
