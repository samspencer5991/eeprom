#ifndef EEPROM_H_
#define EEPROM_H_

// Framework and platform specific libraries
#if FRAMEWORK_STM32CUBE
#ifdef STM32G4xx
#include "stm32g4xx_hal.h"
#endif
#elif FRAMEWORK_ARDUINO
#include "Arduino.h"
#endif

// SUPPORTED EEPROM CHIPS //
#define M95M04
//#define M95M01

#if defined(M95M04) || defined(M95M01)
#include "m95.h"
#define SPI_EEPROM
#define EEPROM_M95
#endif

typedef enum
{
	EepromHalError,					// Low level device HAL error
	EepromDeviceError,			// Eeprom communication or hardware error
	EepromStorageError,			// Eeprom storage allocation error
	EepromBusy,							// Returned when an existing eeprom process is underway
	EepromOk								// API is ok
} EepromErrorState;

typedef struct
{
	// Application assigned
#ifdef SPI_EEPROM
	SPI_HandleTypeDef *hspi;
	GPIO_TypeDef* csPort;
	uint32_t csPin;
#endif
} Eeprom;


//-------------------- PUBLIC FUNCTIONS PROTOTYPES --------------------//
void eeprom_Init(Eeprom* eeprom);
EepromErrorState eeprom_Write(Eeprom* eeprom, uint8_t *pData, uint32_t len, uint32_t dataAddr);
EepromErrorState eeprom_Read(Eeprom* eeprom, uint8_t *pData, uint32_t len, uint32_t dataAddr);
EepromErrorState eeprom_EraseAll();

#endif /* EEPROM_H_ */
