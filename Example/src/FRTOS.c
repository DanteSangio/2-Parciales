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

#define ON		((uint8_t) 1)
#define OFF		((uint8_t) 0)


#define SENS_TEMP	ADC_CH5

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

#define	RGBB		PORT(2),PIN(1)
#define	RGBG		PORT(2),PIN(2)
#define	RGBR		PORT(2),PIN(3)

#define PULS_ONOFF	PORT(0),PIN(9)
#define PUERTA		PORT(1),PIN(26)
#define	MOTOR		PORT(2),PIN(0)
#define	VENTILADOR	PORT(0),PIN(23)


/*
	Rango:
	-5°C -> 0V
	10°C -> 3.3V

	3.3V / 15 = 0.22V
*/
#define LIMITE_INF_TEMP	1638	//1.32V es 1°C que son 1638 cuentas
#define LIMITE_SUP_TEMP	2730	//2.2V  es 5°C que son 2730 cuentas


//*********************************************************************************************************************
//DECLARACIONES
//*********************************************************************************************************************

SemaphoreHandle_t Semaforo_Ventilador;

QueueHandle_t ColaADCTemp;
QueueHandle_t ColaPuerta;

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

	if(Chip_ADC_ReadValue(LPC_ADC, SENS_TEMP, &dataADC))//si da 1 significa que la conversión estaba realizada
	{
		xQueueSendToBackFromISR(ColaADCTemp, &dataADC, &testigo);
		portYIELD_FROM_ISR(testigo);
	}
}


//*********************************************************************************************************************

static void ADC_Config(void *pvParameters)
{
	Chip_IOCON_PinMux(LPC_IOCON, 1, 31, IOCON_MODE_INACT, IOCON_FUNC3); // Entrada analogica 0 (Infotronik) ch5
	Chip_IOCON_PinMux(LPC_IOCON, 1, 30, IOCON_MODE_INACT, IOCON_FUNC3); // Entrada analogica 0 (Infotronik) ch5

	Chip_ADC_Init(LPC_ADC, &ADCSetup);
	Chip_ADC_EnableChannel(LPC_ADC, SENS_TEMP, ENABLE);

	Chip_ADC_SetSampleRate(LPC_ADC, &ADCSetup, 50000);
	Chip_ADC_Int_SetChannelCmd(LPC_ADC, SENS_TEMP, ENABLE);

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
	uint16_t datoTemp, datoPuerta;
	unsigned char Datos_Tx [] = { (W_ADDRESS & 0xFF00) >> 8 , W_ADDRESS & 0x00FF, DATO};

	while(1)
	{
		xQueueReceive(ColaADCTemp, &datoTemp, portMAX_DELAY);
		xQueueReceive(ColaPuerta, &datoPuerta, portMAX_DELAY);

		if(datoTemp > LIMITE_SUP_TEMP)
		{
			//Motor
			Chip_GPIO_SetPinOutHigh(LPC_GPIO, MOTOR);	//Enciendo el motor

			//Ventilador
			if(datoPuerta==ON)			//Si la puerta esta abierta
			{
				Chip_GPIO_SetPinOutLow(LPC_GPIO, VENTILADOR);	//Apago ventilador
			}
			else if(datoPuerta==OFF)	//Si la puerta esta cerrada
			{
				Chip_GPIO_SetPinOutHigh(LPC_GPIO, VENTILADOR);	//Enciendo ventilador
			}
		}

		if(datoTemp < LIMITE_INF_TEMP)
		{
			//Motor
			Chip_GPIO_SetPinOutLow(LPC_GPIO, MOTOR);		//Apago el motor

			//Ventilador
			Chip_GPIO_SetPinOutLow(LPC_GPIO, VENTILADOR);	//Apago el ventilador
		}

		/*
		if(datoTemp[i] > LIMITE_MAX_TEMP)
		{
			//Se debe apagar los aires y guardar en memoria EEPROM externa:
			Chip_GPIO_SetPinOutLow (LPC_GPIO , AIRE1);
			Chip_GPIO_SetPinOutLow (LPC_GPIO , AIRE2);
			Chip_GPIO_SetPinOutLow (LPC_GPIO , AIRE3);
			Chip_GPIO_SetPinOutLow (LPC_GPIO , AIRE4);
			NVIC_ClearPendingIRQ(ADC_IRQn);
			NVIC_DisableIRQ(ADC_IRQn);
			Chip_ADC_DeInit(LPC_ADC);
			Chip_I2C_MasterSend (I2C1, SLAVE_ADDRESS, Datos_Tx,3); //Selecciono el lugar y escribo el dato.
		}
		*/

		Chip_ADC_SetStartMode (LPC_ADC, ADC_START_NOW, ADC_TRIGGERMODE_RISING);
		vTaskDelay( 950 / portTICK_PERIOD_MS );//Delay de 950 mseg
	}
}


//*********************************************************************************************************************

static void vTaskPulsadores(void *pvParameters)
{
	uint8_t Send=OFF;

	while(1)
	{
		//PUERTA
		if(Chip_GPIO_GetPinState(LPC_GPIO, PUERTA)==ON)			//Si se abrio la puerta (contacto cerrado)
		{
			Chip_GPIO_SetPinOutHigh(LPC_GPIO, LED);				//Enciendo led de iluminacion
			Send=ON;											//Se envia un 1 para indicar que se abrio la puerta
			xQueueSendToBack(ColaPuerta, &Send, portMAX_DELAY);
			vTaskDelay(1000/portTICK_RATE_MS);					//Delay 1 seg
		}
		else if(Chip_GPIO_GetPinState(LPC_GPIO, PUERTA)==OFF)	//Si no se abrio la puerta (contacto abierto)
		{
			Chip_GPIO_SetPinOutLow(LPC_GPIO, LED);				//Apago led de iluminacion
			Send=OFF;											//Se envia un 0 para indicar que la puerta esta cerrada
			xQueueSendToBack(ColaPuerta, &Send, portMAX_DELAY);
			vTaskDelay(1000/portTICK_RATE_MS);					//Delay 1 seg
		}

		//PULSADOR ON-OFF
		if(Chip_GPIO_GetPinState(LPC_GPIO, PULS_ONOFF)==ON)
		{
			//APAGAR EQUIPO
		}
		else if(Chip_GPIO_GetPinState(LPC_GPIO, PULS_ONOFF)==OFF)
		{
			//ENCENDER EQUIPO
		}
	}
}


//*********************************************************************************************************************

static void vTaskVentilador(void *pvParameters)
{
	uint16_t datoPuerta;

	while(1)
	{
		xSemaphoreTake(Semaforo_Ventilador, portMAX_DELAY);

		xQueueReceive(ColaPuerta, &datoPuerta, portMAX_DELAY);

		if(datoPuerta==ON)			//Si la puerta se abrio
		{
			Chip_GPIO_SetPinOutLow(LPC_GPIO, VENTILADOR);	//Apago ventilador
		}
		else if(datoPuerta==OFF)	//Si la puerta esta cerrada
		{
			Chip_GPIO_SetPinOutHigh(LPC_GPIO, VENTILADOR);	//Enciendo ventilador
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

	Chip_GPIO_SetDir (LPC_GPIO, MOTOR, OUTPUT);
	Chip_IOCON_PinMux (LPC_IOCON, MOTOR, IOCON_MODE_INACT, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, VENTILADOR, OUTPUT);
	Chip_IOCON_PinMux (LPC_IOCON, VENTILADOR, IOCON_MODE_INACT, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, PUERTA, INPUT);
	Chip_IOCON_PinMux (LPC_IOCON, PUERTA, IOCON_MODE_PULLDOWN, IOCON_FUNC0);
	Chip_GPIO_SetDir (LPC_GPIO, PULS_ONOFF, INPUT);
	Chip_IOCON_PinMux (LPC_IOCON, PULS_ONOFF, IOCON_MODE_PULLDOWN, IOCON_FUNC0);


	//Salidas apagadas
	Chip_GPIO_SetPinOutLow(LPC_GPIO, LED_STICK);
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, BUZZER);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBR);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBG);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, RGBB);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, MOTOR);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, VENTILADOR);
}

int main(void)
{
	uC_StartUp ();

	SystemCoreClockUpdate();

	vSemaphoreCreateBinary(Semaforo_Sonoro);

	ColaADCTemp = xQueueCreate (1, sizeof(uint16_t));
	ColaPuerta = xQueueCreate (1, sizeof(uint16_t));

	xSemaphoreTake(Semaforo_Ventilador , portMAX_DELAY );

	xTaskCreate(taskAnalisis, (char *) "taskAnalisis",
					configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
					(xTaskHandle *) NULL);

	xTaskCreate(ADC_Config, (char *) "ADC_Config",
					configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 3UL),
					(xTaskHandle *) NULL);

	xTaskCreate(I2C_Config, (char *) "I2C_Config",
					configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 3UL),
					(xTaskHandle *) NULL);

	xTaskCreate(vTaskVentilador, (char *) "vTaskVentilador",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 2UL),
				(xTaskHandle *) NULL);

	xTaskCreate(vTaskPulsadores, (char *) "vTaskPulsadores",
				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 4UL),
				(xTaskHandle *) NULL);


	/* Start the scheduler */
	vTaskStartScheduler();

	/* Nunca debería arribar aquí */
    return 0;
}
