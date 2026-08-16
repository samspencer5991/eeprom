#ifndef EEPROM_H_
#define EEPROM_H_

// Framework and platform specific libraries
#if FRAMEWORK_STM32CUBE
#ifdef STM32G4xx
#include "stm32g4xx_hal.h"
#elif defined(STM32H5xx)
#include "stm32h5xx_hal.h"
#elif defined(STM32G0xx)
#include "stm32g0xx_hal.h"
#elif defined(STM32H7xx)
#include "stm32h7xx_hal.h"
#endif
#elif FRAMEWORK_ARDUINO
#include "Arduino.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// SUPPORTED EEPROM CHIPS //
//#define M95M04
//#define M95M01

#if defined(M95M04) || defined(M95M01) || defined(M95P32)
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

#if defined(M95P32)
// Identification page geometry
#define EEPROM_ID_PAGE_SIZE				512			// Each of the two identification pages is 512 bytes
#define EEPROM_ID_DEVICE_PAGE_ADDR		0x000		// First ID page: ST manufacturer/density codes + UID (read-only content)
#define EEPROM_ID_USER_PAGE_ADDR		0x200		// Second ID page: delivered erased, free for application data, lockable

// Configuration register bit positions (read with eeprom_ReadConfigRegisters)
#define EEPROM_CONFIG_LID_BIT			0			// Identification page lock. 1 = permanently locked in read-only mode
#define EEPROM_CONFIG_DRV0_BIT			5			// Output driver strength (01 = medium, 10 = low)
#define EEPROM_CONFIG_DRV1_BIT			6

// Safety register bit positions (read with eeprom_ReadConfigRegisters, cleared with eeprom_ClearSafetyFlags)
#define EEPROM_SAFETY_ECC3DS_BIT		0			// ECC triple-bit error detected (sticky)
#define EEPROM_SAFETY_ECC3D_BIT			1			// ECC triple-bit error detected
#define EEPROM_SAFETY_ECC2C_BIT			2			// ECC double-bit error corrected
#define EEPROM_SAFETY_ECC1C_BIT			3			// ECC single-bit error corrected
#define EEPROM_SAFETY_PRF_BIT			4			// Program fail
#define EEPROM_SAFETY_ERF_BIT			5			// Erase fail
#define EEPROM_SAFETY_PUF_BIT			6			// Power-up fail
#define EEPROM_SAFETY_PAMAF_BIT			7			// Protected array modify attempt

// Volatile register bit positions (eeprom_ReadVolatileRegister/eeprom_WriteVolatileRegister)
#define EEPROM_VOLATILE_BUFLD_BIT		0			// Buffer load status (1 = buffer full)
#define EEPROM_VOLATILE_BUFEN_BIT		1			// Buffer mode enable
#endif


//-------------------- PUBLIC FUNCTIONS PROTOTYPES --------------------//
EepromErrorState eeprom_Init(Eeprom* eeprom);
EepromErrorState eeprom_Write(Eeprom* eeprom, uint8_t *pData, uint32_t len, uint32_t dataAddr);
EepromErrorState eeprom_Read(Eeprom* eeprom, uint8_t *pData, uint32_t len, uint32_t dataAddr);
EepromErrorState eeprom_EraseAll(Eeprom* eeprom);

#if defined(M95P32)
// Erase operations. Addresses may be anywhere within the page/sector/block to be erased.
EepromErrorState eeprom_ErasePage(Eeprom* eeprom, uint32_t dataAddr);
EepromErrorState eeprom_EraseSector(Eeprom* eeprom, uint32_t dataAddr);
EepromErrorState eeprom_EraseBlock(Eeprom* eeprom, uint32_t dataAddr);
EepromErrorState eeprom_EraseChip(Eeprom* eeprom);

// Identification pages. Addresses are relative to the start of the ID area:
// 0x000-0x1FF = device ID page, 0x200-0x3FF = user ID page.
EepromErrorState eeprom_ReadIdPage(Eeprom* eeprom, uint8_t *pData, uint32_t len, uint32_t dataAddr);
EepromErrorState eeprom_WriteIdPage(Eeprom* eeprom, uint8_t *pData, uint32_t len, uint32_t dataAddr);
EepromErrorState eeprom_LockIdPage(Eeprom* eeprom);
EepromErrorState eeprom_IdPageLocked(Eeprom* eeprom, uint8_t* locked);

// Configuration, safety, and volatile registers
EepromErrorState eeprom_ReadConfigRegisters(Eeprom* eeprom, uint8_t* configReg, uint8_t* safetyReg);
EepromErrorState eeprom_ClearSafetyFlags(Eeprom* eeprom);
EepromErrorState eeprom_ReadVolatileRegister(Eeprom* eeprom, uint8_t* data);
EepromErrorState eeprom_WriteVolatileRegister(Eeprom* eeprom, uint8_t data);

// Block write protection. bpLevel 0-7 maps to the BP2:BP0 bits (0 = unprotected,
// 1-6 = upper/lower 1/64 to 1/2 of the array, 7 = whole array).
// protectBottom sets the TB bit: 0 = protect from the top, 1 = protect from the bottom.
EepromErrorState eeprom_SetBlockProtection(Eeprom* eeprom, uint8_t bpLevel, uint8_t protectBottom);
#endif

#ifdef __cplusplus
}
#endif

#endif /* EEPROM_H_ */
