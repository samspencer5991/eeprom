#ifndef EEPROM_H_
#define EEPROM_H_

#include "main.h"

// SUPPORTED EEPROM CHIPS //
// uncomment all chip types that will be used (multiple supported)
#define M95M04
//#define M95M01

#if defined(M95M04) || defined(M95M01)
#include "spi.h"

#define SPI_EEPROM
#define WRITE_CYCLE_TIME	      5				// Required time for the device to complete an internal  write operation (mS)
#define READ_CYCLE_TIME		      5				// Required time for the device to complete an internal  read operation (mS)
#define READY_CHECK_TIMEOUT	   10
#define NUM_READY_CHECK_ATTEMPTS	50

// Command bytes
#define WREN_CMD	0b00000110		// Write enable
#define WRDI_CMD	0b00000100		// Write disable
#define RDSR_CMD	0b00000101		// Read Status register
#define WRSR_CMD	0b00000001		// Write Status register
#define READ_CMD	0b00000011		// Read from Memory array
#define WRITE_CMD	0b00000010		// Write to Memory array
#define RDID_CMD	0b10000011		// Read Identification page
#define WRID_CMD	0b10000010		// Write Identification page
#define RDLS_CMD	0b10000011		// Reads the Identification page lock status
#define LID_CMD	0b10000010		// Locks the Identification page in read-only mode

// Status register bit positions
#define WIP_BIT		0
#define WEL_BIT 	1
#define BP0_BIT		2
#define BP1_BIT		3
#define SRWD_BIT	4

#define MAX_WRITE_CYCLES	4000000	// Maximum number of writes allowed per cell
#define PAGE_WIDTH 				512			// Maximum number of bytes in a page write

#endif

#ifdef M95M04
#define DEVICE_SIZE 			512000	// EEPROM storage size in bytes
#define NUM_EEPROM_PAGES	1000		// Equal to the device size / page size
#endif


#ifdef M95M01
#define DEVICE_SIZE 			512000	// EEPROM storage size in bytes
#define NUM_EEPROM_PAGES	1000		// Equal to the device size / page size
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
	TIM_HandleTypeDef *writeTim;

	// Privately used
	volatile uint8_t writeInProgress;	// Whether a multi-page write is taking place or not
	volatile uint8_t writeCompleted;		// Whether a new write can begin
	volatile uint8_t readCompleted;						// Whether the last read event has been completed
} Eeprom;


//-------------------- PUBLIC FUNCTIONS PROTOTYPES --------------------//
EepromErrorState eeprom_Init(Eeprom* eeprom);
EepromErrorState eeprom_Write(Eeprom* eeprom, uint8_t *pData, uint32_t len, uint32_t dataAddr);
EepromErrorState eeprom_Read(Eeprom* eeprom, uint8_t *pData, uint32_t len, uint32_t dataAddr);

EepromErrorState eeprom_EraseAll();
uint8_t eeprom_ReadCompleted();
EepromErrorState eeprom_CheckReady(Eeprom* eeprom);
void eeprom_TimerHandler();
void eeprom_RxHandler(Eeprom* eeprom);


#endif /* EEPROM_H_ */
