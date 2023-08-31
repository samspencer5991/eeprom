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