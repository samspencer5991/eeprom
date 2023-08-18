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

uint8_t erasePacket[PAGE_WIDTH];

EepromErrorState eeprom_readCurrent(uint8_t *pData);
EepromErrorState eeprom_readRandom(Eeprom* eeprom, uint8_t *pData, uint32_t addr);
EepromErrorState eeprom_readSequential(Eeprom* eeprom, uint8_t *pData, uint32_t addr, uint32_t size);

EepromErrorState eeprom_writeByte(Eeprom* eeprom, uint8_t data, uint32_t dataAddr);
EepromErrorState eeprom_writeSequential(Eeprom* eeprom, uint8_t *data, uint32_t dataAddr, uint32_t size);

// Private Function Prototypes
EepromErrorState eeprom_writeMultiplePages(Eeprom* eeprom, uint8_t *pData, uint32_t len, uint32_t dataAddr);


//------------------------ PUBLIC FUNCTIONS ------------------------//
void eeprom_Init(Eeprom* eeprom)
{
	// Configure the erase buffer (just a buffer full of 1's)
	for(int i=0; i<PAGE_WIDTH; i++)
	{
		erasePacket[i] = 0xff;
	}
	eeprom->writeInProgress = FALSE;
	eeprom->writeCompleted = TRUE;
}

EepromErrorState eeprom_Write(Eeprom* eeprom, uint8_t *pData, uint32_t len, uint32_t dataAddr)
{
   EepromErrorState status = eeprom_writeMultiplePages(eeprom, pData, len, dataAddr);
   return
}

EepromErrorState eeprom_Read(Eeprom* eeprom, uint8_t *pData, uint32_t len, uint32_t dataAddr)
{
	EepromErrorState status = EepromOk;
	status = eeprom_readSequential(eeprom, pData, dataAddr, len);
	return status;
}



// Returns 0 if successful, otherwise returns the number of EEPROM device that is not ready
EepromErrorState eeprom_CheckReady(Eeprom* eeprom)
{
	uint8_t txBuf = RDSR_CMD;
	uint8_t rxBuf;
	uint8_t checkSuccess = FALSE;
   for(uint8_t j=0; j<NUM_READY_CHECK_ATTEMPTS; j++)
   {
      while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
      HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
      if(HAL_SPI_Transmit(eeprom->hspi, &txBuf, 1, 100) != HAL_OK)
      {
         return EepromHalError;
      }
      if(HAL_SPI_Receive(eeprom->hspi, &rxBuf, 1, 10) != HAL_OK)
      {
         return EepromHalError;
      }
      HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);

      // If the WIP bit is set, device is not ready to respond to new WRITE commands
      if(((rxBuf>>WIP_BIT) & 1) == 0)
      {
         checkSuccess = TRUE;
         break;
      }
   }
   if(checkSuccess == FALSE)
   {
      return EepromDeviceError;
   }

	return EepromOk;
}



EepromErrorState eeprom_EraseAll(Eeprom* eeprom)
{
	EepromErrorState status;
	if(!eeprom->writeCompleted)
	{
		return EepromBusy;
	}
	for(uint16_t i=0; i<NUM_EEPROM_PAGES; i++)
	{
		status = eeprom_writeSequential(eeprom, erasePacket, i*PAGE_WIDTH, PAGE_WIDTH);
		if(status != EepromOk)
		{
			return status;
		}
	}
	return status;
}

void eeprom_TimerHandler(Eeprom* eeprom)
{
	HAL_TIM_Base_Stop_IT(eeprom->writeTim);
	// If an ongoing write required a new page to be sent
	if(eeprom->writeInProgress)
	{
		// No valid parameters are required, as ongoing data is stored as static in the function
		eeprom_writeMultiplePages(eeprom, NULL, 0, 0);
	}
	// If a multi-page write has been completed
	else if(eeprom->writeCompleted == FALSE)
	{
		eeprom->writeCompleted = TRUE;
	}
	else if(eeprom->readCompleted == FALSE)
	{
		eeprom->readCompleted = TRUE;
	}
}

void eeprom_RxHandler(Eeprom* eeprom)
{
	//eeprom->readCompleted = TRUE;
}

uint8_t eeprom_ReadCompleted(Eeprom* eeprom)
{
	return eeprom->readCompleted;
}

//------------------------ PRIVATE FUNCTIONS ------------------------//
EepromErrorState eeprom_writeMultiplePages(Eeprom* eeprom, uint8_t *pData, uint32_t len, uint32_t dataAddr)
{
	EepromErrorState status;
	static uint32_t numRemaining;
	static uint32_t currentDataAddr;
	static uint8_t *currentData;

	// First data packet
	if(!eeprom->writeInProgress)
	{
		/* Because each page becomes row locked, if a write reaches the end of a page boundary,
		 * The address counter in the eeprom will reset to the beginning of the page.
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
		eeprom->writeInProgress = TRUE;
		// Update the static variables to those passed to the function
		currentData = pData;
		currentDataAddr = dataAddr;

		if(len > currentPageBytes)
		{
			status = eeprom_writeSequential(eeprom, currentData, currentDataAddr, currentPageBytes);

			// Update the static data variables
			numRemaining = (len - currentPageBytes);
			currentData += currentPageBytes;
			currentDataAddr += currentPageBytes;
		}
		else
		{
			status = eeprom_writeSequential(eeprom, currentData, currentDataAddr, len);

			// Set the flag so that no future writes take place
			eeprom->writeInProgress = FALSE;
		}
	}

	// For consecutive writes after the initial page write
	else if(numRemaining > 0)
	{
		// If this is the last sequential write required
		if(numRemaining <= PAGE_WIDTH)
		{
			status = eeprom_writeSequential(eeprom, currentData, currentDataAddr, numRemaining);
			eeprom->writeInProgress = FALSE;
		}
		else
		{
			status = eeprom_writeSequential(eeprom, currentData, currentDataAddr, PAGE_WIDTH);
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

//------------------------ CHIP SPECIFIC FUNCTIONS ------------------------//
/* These functions are the raw implementation of the eeprom datasheet.
 * Generally, they should not be used by the application, but go through the API calls.
 */
/**
  * @brief	Reads a byte from any memory address in the eeprom memory
  * @param	*pData pointer to store the received byte
  * 			addr	 address to read from
  * @retval	Errorstate
  */
EepromErrorState eeprom_readRandom(Eeprom* eeprom, uint8_t *pData, uint32_t dataAddr)
{
	if(!eeprom->writeCompleted)
	{
		return EepromBusy;
	}
	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
	eeprom->readCompleted = FALSE;

	uint8_t txPacket[4];
	txPacket[0] = READ_CMD;
	txPacket[1] = (uint8_t)((dataAddr >> 16) & 0xff);
	txPacket[2] = (uint8_t)((dataAddr >> 8) & 0xff);
	txPacket[3] = (uint8_t)(dataAddr & 0xff);
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, txPacket, 4, HAL_MAX_DELAY))
	{
		return EepromHalError;
	}
	if(HAL_SPI_Receive(eeprom->hspi, pData, 1, HAL_MAX_DELAY))
	{
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
	HAL_TIM_Base_Start_IT(eeprom->writeTim);
	return EepromOk;
}

/**
  * @brief	Reads 'size' number of bytes to the *data pointer from the eeprom
  * @param	*pData data to write
  * 			addr   address to begin write to
  * 			size   number of bytes to write
  * @retval	Errorstate
  */
EepromErrorState eeprom_readSequential(Eeprom* eeprom, uint8_t *pData, uint32_t dataAddr, uint32_t size)
{
	if(!eeprom->writeCompleted)
	{
		return EepromBusy;
	}
	// Check to make sure the device is ready
	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
	if(!eeprom_CheckReady(eeprom))
	{
		return EepromDeviceError;
	}
	eeprom->readCompleted = FALSE;

	uint8_t txPacket[4];
	txPacket[0] = READ_CMD;
	txPacket[1] = (uint8_t)((dataAddr >> 16) & 0xff);
	txPacket[2] = (uint8_t)((dataAddr >> 8) & 0xff);
	txPacket[3] = (uint8_t)(dataAddr & 0xff);
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, txPacket, 4, HAL_MAX_DELAY))
	{
		return EepromHalError;
	}
	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
	if(HAL_SPI_Receive(eeprom->hspi, pData, size, HAL_MAX_DELAY))
	{
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
	HAL_TIM_Base_Start_IT(eeprom->writeTim);

	return EepromOk;
}

/* WRITE FUNCTIONS */

/**
  * @brief	Writes a single byte to the eeprom memory
  * @param	data byte to write
  * 			addr target address for the write
  * @retval	Errorstate
  */
EepromErrorState eeprom_writeByte(Eeprom* eeprom, uint8_t data, uint32_t dataAddr)
{
	eeprom->writeCompleted = FALSE;
	// Check to make sure the device is ready
	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
	if(!eeprom_CheckReady(eeprom))
	{
		return EepromDeviceError;
	}

	// Send the write enmable (WREN) instruction
	uint8_t wrenPacket = WREN_CMD;
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(eeprom->hspi, &wrenPacket, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);

	uint8_t txPacket[5];
	txPacket[0] = WRITE_CMD;
	txPacket[1] = (uint8_t)((dataAddr >> 16) & 0xff);
	txPacket[2] = (uint8_t)((dataAddr >> 8) & 0xff);
	txPacket[3] = (uint8_t)(dataAddr & 0xff);
	txPacket[4] = data;
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, txPacket, 5, HAL_MAX_DELAY))
	{
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);

	eeprom->writeCompleted = FALSE;
	HAL_TIM_Base_Start_IT(eeprom->writeTim);
	return EepromOk;
}

/**
  * @brief 	Writes 'size' number of bytes from the *data pointer to the eeprom
  * 			Note that sizes of more than the PAGE_WIDTH cannot be written with one call.
  * 			The application must call this function multiple times for multiple data pages.
  * @param 	*data Pointer for the data to write
  * 			size Number of bytes to be written
  * @retval	error state
  */

EepromErrorState eeprom_writeSequential(Eeprom* eeprom, uint8_t *data, uint32_t dataAddr, uint32_t size)
{
	// Check to make sure the device is ready
	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);

	// Send the write enmable (WREN) instruction
	uint8_t wrenPacket = WREN_CMD;
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(eeprom->hspi, &wrenPacket, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);

	uint8_t txPacket[4 + size];
	txPacket[0] = WRITE_CMD;
	txPacket[1] = (uint8_t)((dataAddr >> 16) & 0xff);
	txPacket[2] = (uint8_t)((dataAddr >> 8) & 0xff);
	txPacket[3] = (uint8_t)(dataAddr & 0xff);
	for(uint32_t i=0; i<size; i++)
	{
		txPacket[4+i] = data[i];
	}

	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, txPacket, size + 4, HAL_MAX_DELAY) != HAL_OK)
	{
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);


	eeprom->writeCompleted = FALSE;
 	HAL_TIM_Base_Start_IT(eeprom->writeTim);
	return EepromOk;
}
