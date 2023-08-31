/*
 * eeprom_storage.c
 *
 *  Created on: 9Nov.,2019
 *      Author: Sam Work
 */

#include "eeprom.h"
#include "stdlib.h"

#define TRUE	1
#define FALSE	0

// M95 EEPROM Devices
#ifdef EEPROM_M95
#define MAX_WRITE_CYCLES	4000000	// Maximum number of writes allowed per cell
#define PAGE_WIDTH 			512			// Maximum number of bytes in a page write

#if defined(M95M04)
#define DEVICE_SIZE 			512000	// EEPROM storage size in bytes
#define NUM_EEPROM_PAGES	1000		// Equal to the device size / page size

#define WRITE_CYCLE_TIME	      5				// Required time for the device to complete an internal  write operation (mS)
#define READ_CYCLE_TIME		      5				// Required time for the device to complete an internal  read operation (mS)
#define READY_CHECK_TIMEOUT	   15

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
#define WIP_BIT	0
#define WEL_BIT 	1
#define BP0_BIT	2
#define BP1_BIT	3
#define SRWD_BIT	4
#endif

//-------------------- Private Function Prototypes --------------------//
EepromErrorState m95_Read(Eeprom* eeprom, uint8_t *pData, uint32_t dataAddr, uint32_t size);
EepromErrorState m95_Write(Eeprom* eeprom, uint8_t *data, uint32_t dataAddr, uint32_t size);
EepromErrorState m95_PollReady(Eeprom* eeprom);
EepromErrorState m95_WriteEnable(Eeprom* eeprom);
EepromErrorState m95_WriteDisable(Eeprom* eeprom);
EepromErrorState m95_ReadStatusRegister(Eeprom* eeprom, uint8_t* data);
#endif

/**
  * @brief 	Writes 'size' number of bytes from the data pointer to the eeprom
  * 			Note that sizes of more than the PAGE_WIDTH cannot be written with one call.
  * 			The application must call this function multiple times for multiple data pages.
  * @param	eeprom eeprom struct
  * @param 	data Pointer for the data to write
  * @param	dataAddr Address to begin writing to
  * @param	size Number of bytes to be written
  * @retval	error state
  */
EepromErrorState eeprom_Write(Eeprom* eeprom, uint8_t *pData, uint32_t len, uint32_t dataAddr)
{
	EepromErrorState status;
	static uint32_t numRemaining;
	static uint32_t currentDataAddr;
	static uint8_t *currentData;

	/* 
	* Because each page becomes row locked, if a write reaches the end of a page boundary,
	* the address counter in the eeprom will reset to the beginning of the page.
	* Therefore, the buffer address must be checked against a multiple of the page width,
	* With that many bytes being written, then the rest of the data can be written to consecutive pages.
	* If this is not done, other data in the page which may not be part of the buffer can be overwritten.
	*
	* The formula to work out how many bits can be written before overflowing the current page is:
	* currentPageBytes = PAGE_WIDTH - (addres % PAGE_WIDTH)
	*
	* currentPageBytes number of bytes are written to the page that the data address exists in,
	* then full pages can be written to as normal.
	*/

	// Calculate how many bytes exist in the current page that need to be written to
	uint16_t currentPageBytes = PAGE_WIDTH - (dataAddr % PAGE_WIDTH);
	
	// Update the static variables to those passed to the function
	currentData = pData;
	currentDataAddr = dataAddr;

	if(len > currentPageBytes)
	{		
		#ifdef EEPROM_M95
		status = m95_Write(eeprom, currentData, currentDataAddr, currentPageBytes);
		#endif
		// Update the static data variables
		numRemaining = (len - currentPageBytes);
		currentData += currentPageBytes;
		currentDataAddr += currentPageBytes;
	}
	else
	{
		#ifdef EEPROM_M95
		status = m95_Write(eeprom, currentData, currentDataAddr, len);
		#endif
		return status;
	}

	// For consecutive writes after the initial page write
	while(numRemaining > 0)
	{
		// If this is the last sequential write required
		if(numRemaining <= PAGE_WIDTH)
		{
			#ifdef EEPROM_M95
			status = m95_Write(eeprom, currentData, currentDataAddr, numRemaining);
			#endif
			return status;
		}
		else
		{
			#ifdef EEPROM_M95
			status = m95_Write(eeprom, currentData, currentDataAddr, PAGE_WIDTH);
			#endif
			numRemaining -= PAGE_WIDTH;
			currentData += PAGE_WIDTH;
			currentDataAddr += PAGE_WIDTH;
		}
		if(status != EepromOk)
		{
			return status;
		}
	}
	return EepromOk;
}

/**
  * @brief 	Reads 'size' number of bytes from the eeprom to the data pointer.
  * @param	eeprom eeprom struct
  * @param 	data Pointer for the data to read to
  * @param	dataAddr Address to begin reading from
  * @param	size Number of bytes to be read
  * @retval	error state
  */
EepromErrorState eeprom_Read(Eeprom* eeprom, uint8_t *pData, uint32_t len, uint32_t dataAddr)
{
#ifdef EEPROM_M95
	return m95_Read(eeprom, pData, dataAddr, len);
#endif
}

/**
  * @brief 	Erases the entire eeprom chip by writing 0xff to every address.
  * @param eeprom eeprom struct
  * @retval	error state
  */
EepromErrorState eeprom_EraseAll(Eeprom* eeprom)
{
	EepromErrorState status;
	uint8_t erasePacket[PAGE_WIDTH];
	for(uint16_t i=0; i<PAGE_WIDTH; i++)
	{
		erasePacket[i] = 0xff;
	}
	for(uint16_t i=0; i<NUM_EEPROM_PAGES; i++)
	{
		#ifdef EEPROM_M95
		status = m95_Write(eeprom, erasePacket, i*PAGE_WIDTH, PAGE_WIDTH);
		#endif
		if(status != EepromOk)
		{
			return status;
		}
	}
	return status;
}


//-------------------- Private Device Functions --------------------//
#ifdef EEPROM_M95
/**
  * @brief 	Reads 'size' number of bytes from the eeprom to the data pointer.
  * 
  * @param	eeprom eeprom struct
  * @param 	data Pointer for the data to read to
  * @param	dataAddr Address to begin reading from
  * @param	size Number of bytes to be read
  * @retval	error state
  */
EepromErrorState m95_Read(Eeprom* eeprom, uint8_t *pData, uint32_t dataAddr, uint32_t size)
{
	// Check to make sure the device is ready
	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);

	// Prepare the command + address header
	uint8_t txPacket[4 + size];
	txPacket[0] = READ_CMD;
	txPacket[1] = (uint8_t)((dataAddr >> 16) & 0xff);
	txPacket[2] = (uint8_t)((dataAddr >> 8) & 0xff);
	txPacket[3] = (uint8_t)(dataAddr & 0xff);

	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, txPacket, 4, HAL_MAX_DELAY) != HAL_OK)
	{
		return EepromHalError;
	}
	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
	if(HAL_SPI_Receive(eeprom->hspi, pData, size, HAL_MAX_DELAY) != HAL_OK)
	{
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
	return EepromOk;
}

/**
  * @brief 	Writes 'size' number of bytes from the data pointer to the eeprom
  * 			Note that sizes of more than the PAGE_WIDTH cannot be written with one call.
  * 			The application must call this function multiple times for multiple data pages.
  * @param	eeprom eeprom struct
  * @param 	data Pointer for the data to write
  * @param	dataAddr Address to begin writing to
  * @param	size Number of bytes to be written
  * @retval	error state
  */
EepromErrorState m95_Write(Eeprom* eeprom, uint8_t *data, uint32_t dataAddr, uint32_t size)
{
	if(size > PAGE_WIDTH)
	{
		return EepromStorageError;
	}
	// Check to make sure the device is ready
	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);

	// Send the write enmable (WREN) instruction
	m95_WriteEnable(eeprom);

	// Prepare the command + address header
	uint8_t txPacket[4 + size];
	txPacket[0] = WRITE_CMD;
	txPacket[1] = (uint8_t)((dataAddr >> 16) & 0xff);
	txPacket[2] = (uint8_t)((dataAddr >> 8) & 0xff);
	txPacket[3] = (uint8_t)(dataAddr & 0xff);
	// Copy the data to be written into the transmit buffer
	for(uint32_t i=0; i<size; i++)
	{
		txPacket[4+i] = data[i];
	}

	// Wait until the device is ready
	// On a HAL error or device timeout, return the error condition
	EepromErrorState status = m95_PollReady(eeprom);
	if(status != EepromOk)
	{
		return status;
	}

	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, txPacket, size + 4, HAL_MAX_DELAY) != HAL_OK)
	{
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
	return EepromOk;
}

/**
  * @brief	Continuously reads the status register, checking for the WIP bit to be reset.
  * The typical write cycle time is 5ms, however a timeout is added for device lockup. 
  * @param	eeprom eeprom struct
  * @retval	Error state. EepromOk if the device is ready, EepromBusy if the device is not ready
  */
EepromErrorState m95_PollReady(Eeprom* eeprom)
{
	uint8_t txBuf = RDSR_CMD;
	uint8_t rxBuf;
	uint8_t deviceBusy = TRUE;

	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, &txBuf, 1, 5) != HAL_OK)
	{
		return EepromHalError;
	}
	uint32_t startMs, timeMs;
	#if FRAMEWORK_STM32CUBE
	startMs = HAL_GetTick();
	#elif FRAMEWORK_ARDUINO
	startMs = millis();
	#endif
	while((timeMs - startMs) < READY_CHECK_TIMEOUT)
	{
		// Read the status register contents
		while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
		if(HAL_SPI_Receive(eeprom->hspi, &rxBuf, 1, 5) != HAL_OK)
		{
			HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
			return EepromHalError;
		}
		// Check the WIP bit
		deviceBusy = (rxBuf>>WIP_BIT) & 1;
		if(!deviceBusy)
		{
			break;
		}
		// Get the current polling time. This is used to check for a timeout condition
		#if FRAMEWORK_STM32CUBE
		uint32_t timeMs = HAL_GetTick();
		#elif FRAMEWORK_ARDUINO
		uint32_t timeMs = millis();
		#endif
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
	if(deviceBusy)
	{
		return EepromBusy;
	}
	return EepromOk;
}

/**
  * @brief	Sends a write enable instruction to the device.
  * This sets the Write Enable Latch (WEL) bit in the status register.
  * This must be set prior to each WRITE and WRSR instruction.
  * @param	eeprom eeprom struct
  * @retval	Error state
  */
EepromErrorState m95_WriteEnable(Eeprom* eeprom)
{
	// Send the write enmable (WREN) instruction
	uint8_t wrdiPacket = WRDI_CMD;
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, &wrdiPacket, 1, 10) != HAL_OK)
	{
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
	return EepromOk;
}

/**
  * @brief	Sends a write disable instruction to the device.
  * This resets the Write Enable Latch (WEL) bit in the status register.
  * Note that the WEL bit becomes reset after any of the following events:
  * - Power-up
  * - WRDI instruction execution
  * - WRSR instruction completion
  * - WRITE instruction completion
  * @param	eeprom eeprom struct
  * @retval	Error state
  */
EepromErrorState m95_WriteDisable(Eeprom* eeprom)
{
	// Send the write enmable (WREN) instruction
	uint8_t wrenPacket = WREN_CMD;
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, &wrenPacket, 1, 10) != HAL_OK)
	{
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
	return EepromOk;
}

/**
  * @brief	Reads the status register of the device. The status register may be read at any time,
  * even while a write or write status register cycle is in progress.
  * @param	eeprom eeprom struct
  * @param	data pointer for the register data to be read into
  * @retval	Error state
  */
EepromErrorState m95_ReadStatusRegister(Eeprom* eeprom, uint8_t* data)
#endif