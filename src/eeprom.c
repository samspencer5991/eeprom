/*
 * eeprom_storage.c
 *
 *  Created on: 9Nov.,2019
 *      Author: Sam Work
 */

#include "eeprom.h"
#include "stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TRUE	1
#define FALSE	0

// M95 EEPROM Devices
#ifdef EEPROM_M95
#define PAGE_WIDTH 				512			// Maximum number of bytes in a page write

#if defined(M95M04)
#define MAX_WRITE_CYCLES		4000000	// Maximum number of writes allowed per cell
#define DEVICE_SIZE 			512000	// EEPROM storage size in bytes
#define NUM_EEPROM_PAGES		1000		// Equal to the device size / page size

#define WRITE_CYCLE_TIME		5				// Required time for the device to complete an internal  write operation (mS)
#define READ_CYCLE_TIME			5				// Required time for the device to complete an internal  read operation (mS)
#define READY_CHECK_TIMEOUT		15

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
#define LID_CMD		0b10000010		// Locks the Identification page in read-only mode

// Status register bit positions
#define WIP_BIT		0
#define WEL_BIT 	1
#define BP0_BIT		2
#define BP1_BIT		3
#define SRWD_BIT	4
#endif

#if defined (M95P32)
#define MAX_WRITE_CYCLES		5000000		// Maximum number of writes allowed per cell
#define DEVICE_SIZE 			4194304		// EEPROM storage size in bytes (32 Mbit = 4 MB)
#define NUM_EEPROM_PAGES		8192		// Equal to the device size / page size (512 bytes/page)
#define SECTOR_SIZE				4096		// Sector size in bytes (4 Kbytes)
#define NUM_SECTORS				1024		// Equal to the device size / sector size
#define BLOCK_SIZE				65536		// Block size in bytes (64 Kbytes)
#define NUM_BLOCKS				64			// Equal to the device size / block size

#define WRITE_CYCLE_TIME		5				// Required time for the device to complete a page write (erase+program) cycle (mS). Datasheet tPW max is 4.5mS
#define READ_CYCLE_TIME			5				// Required time for the device to complete an internal read operation (mS)
#define READY_CHECK_TIMEOUT		15			// Page write/program/erase poll timeout (tPW/tPE max 4.5mS)
#define SECTOR_ERASE_TIMEOUT	15			// Sector erase poll timeout (tSE max 5mS)
#define BLOCK_ERASE_TIMEOUT		25			// Block erase poll timeout (tBE max 8mS)
#define CHIP_ERASE_TIMEOUT		75			// Chip erase poll timeout (tCE max 25mS)
#define WRSR_TIMEOUT			30			// Write status/configuration registers poll timeout (tWSCR max 9mS)

// Command bytes
#define WREN_CMD	0b00000110		// Write enable
#define WRDI_CMD	0b00000100		// Write disable
#define RDSR_CMD	0b00000101		// Read status register
#define WRSR_CMD	0b00000001		// Write status and configuration registers
#define READ_CMD	0b00000011		// Read data single output from memory array
#define FREAD_CMD	0b00001011		// Fast read single output with one dummy byte
#define FDREAD_CMD	0b00111011		// Fast read dual output with one dummy byte
#define FQREAD_CMD	0b01101011		// Fast read quad output with one dummy byte
#define WRITE_CMD	0b00000010		// Page write: self-timed erase + program (PGWR), used for generic byte-alterable writes
#define PGPR_CMD	0b00001010		// Page program: programs a pre-erased page only (PGPR)
#define PGER_CMD	0b11011011		// Page erase (512 bytes)
#define SCER_CMD	0b00100000		// Sector erase (4 Kbytes)
#define BKER_CMD	0b11011000		// Block erase (64 Kbytes)
#define CHER_CMD	0b11000111		// Chip erase
#define RDID_CMD	0b10000011		// Read identification page
#define FRDID_CMD	0b10001011		// Fast read identification page with one dummy byte
#define WRID_CMD	0b10000010		// Write identification page
#define DPD_CMD		0b10111001		// Deep power-down enter
#define RDPD_CMD	0b10101011		// Deep power-down release
#define JEDID_CMD	0b10011111		// JEDEC identification
#define RDCR_CMD	0b00010101		// Read configuration and safety registers
#define RDVR_CMD	0b10000101		// Read volatile register
#define WRVR_CMD	0b10000001		// Write volatile register
#define CLRSF_CMD	0b01010000		// Clear safety register sticky flags
#define RDSFDP_CMD	0b01011010		// Read SFDP register
#define RSTEN_CMD	0b01100110		// Enable reset
#define RESET_CMD	0b10011001		// Software reset

// Status register bit positions
#define WIP_BIT		0
#define WEL_BIT		1
#define BP0_BIT		2
#define BP1_BIT		3
#define BP2_BIT		4
#define TB_BIT		6
#define SRWD_BIT	7
#endif

//-------------------- Private Function Prototypes --------------------//
EepromErrorState m95_Read(Eeprom* eeprom, uint8_t *pData, uint32_t dataAddr, uint32_t size);
EepromErrorState m95_Write(Eeprom* eeprom, uint8_t *data, uint32_t dataAddr, uint32_t size);
EepromErrorState m95_PollReady(Eeprom* eeprom, uint32_t timeoutMs);
EepromErrorState m95_WriteEnable(Eeprom* eeprom);
EepromErrorState m95_WriteDisable(Eeprom* eeprom);
EepromErrorState m95_ReadStatusRegister(Eeprom* eeprom, uint8_t* data);
#if defined(M95P32)
EepromErrorState m95p32_SendCommand(Eeprom* eeprom, uint8_t cmd);
EepromErrorState m95p32_Erase(Eeprom* eeprom, uint8_t cmd, uint32_t dataAddr, uint8_t hasAddress, uint32_t timeoutMs);
EepromErrorState m95p32_WriteStatusConfigRegisters(Eeprom* eeprom, uint8_t statusReg, uint8_t configReg, uint8_t writeConfig);
#endif
#endif

#if !defined(M95P32)
uint8_t erasePacket[PAGE_WIDTH];
#endif

/**
  * @brief 	Initialises the eeprom struct
  * @param 	eeprom eeprom struct
  * @retval	error state
  */
EepromErrorState eeprom_Init(Eeprom* eeprom)
{
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
		status = m95_Write(eeprom, currentData, currentPageBytes, currentDataAddr);
		#endif
		// Update the static data variables
		numRemaining = (len - currentPageBytes);
		currentData += currentPageBytes;
		currentDataAddr += currentPageBytes;
	}
	else
	{
		#ifdef EEPROM_M95
		status = m95_Write(eeprom, currentData, len, currentDataAddr);
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
			status = m95_Write(eeprom, currentData, numRemaining, currentDataAddr);
			#endif
			return status;
		}
		else
		{
			#ifdef EEPROM_M95
			status = m95_Write(eeprom, currentData, PAGE_WIDTH, currentDataAddr);
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
	return m95_Read(eeprom, pData, len, dataAddr);
#endif
}

/**
  * @brief 	Erases the entire eeprom chip, setting every address to 0xff.
  * @param eeprom eeprom struct
  * @retval	error state
  */
EepromErrorState eeprom_EraseAll(Eeprom* eeprom)
{
#if defined(M95P32)
	// The M95P32 has a dedicated single-instruction chip erase
	return eeprom_EraseChip(eeprom);
#else
	EepromErrorState status;

	for(uint16_t i=0; i<PAGE_WIDTH; i++)
	{
		erasePacket[i] = 0xff;
	}
	for(uint16_t i=0; i<NUM_EEPROM_PAGES; i++)
	{
		#ifdef EEPROM_M95
		status = m95_Write(eeprom, erasePacket, PAGE_WIDTH, i*PAGE_WIDTH);
		#endif
		if(status != EepromOk)
		{
			return status;
		}
	}
	return status;
#endif
}

#if defined(M95P32)
/**
  * @brief 	Erases the 512 byte page containing dataAddr (sets all bytes to 0xff).
  * @param	eeprom eeprom struct
  * @param	dataAddr Any address within the page to be erased
  * @retval	error state
  */
EepromErrorState eeprom_ErasePage(Eeprom* eeprom, uint32_t dataAddr)
{
	if(dataAddr >= DEVICE_SIZE)
	{
		return EepromStorageError;
	}
	return m95p32_Erase(eeprom, PGER_CMD, dataAddr, TRUE, READY_CHECK_TIMEOUT);
}

/**
  * @brief 	Erases the 4 Kbyte sector containing dataAddr (sets all bytes to 0xff).
  * @param	eeprom eeprom struct
  * @param	dataAddr Any address within the sector to be erased
  * @retval	error state
  */
EepromErrorState eeprom_EraseSector(Eeprom* eeprom, uint32_t dataAddr)
{
	if(dataAddr >= DEVICE_SIZE)
	{
		return EepromStorageError;
	}
	return m95p32_Erase(eeprom, SCER_CMD, dataAddr, TRUE, SECTOR_ERASE_TIMEOUT);
}

/**
  * @brief 	Erases the 64 Kbyte block containing dataAddr (sets all bytes to 0xff).
  * @param	eeprom eeprom struct
  * @param	dataAddr Any address within the block to be erased
  * @retval	error state
  */
EepromErrorState eeprom_EraseBlock(Eeprom* eeprom, uint32_t dataAddr)
{
	if(dataAddr >= DEVICE_SIZE)
	{
		return EepromStorageError;
	}
	return m95p32_Erase(eeprom, BKER_CMD, dataAddr, TRUE, BLOCK_ERASE_TIMEOUT);
}

/**
  * @brief 	Erases the entire memory array (sets all bytes to 0xff).
  * Blocks for the duration of the erase cycle (up to 25mS).
  * @param	eeprom eeprom struct
  * @retval	error state
  */
EepromErrorState eeprom_EraseChip(Eeprom* eeprom)
{
	return m95p32_Erase(eeprom, CHER_CMD, 0, FALSE, CHIP_ERASE_TIMEOUT);
}

/**
  * @brief 	Reads from the two 512 byte identification pages.
  * Addresses 0x000-0x1FF are the device ID page (ST manufacturer/density codes and UID),
  * 0x200-0x3FF are the user ID page. Reads may span both pages; the internal address
  * counter rolls over to 0 after the end of the second page.
  * @param	eeprom eeprom struct
  * @param 	pData Pointer for the data to read to
  * @param	len Number of bytes to be read
  * @param	dataAddr Address to begin reading from (0x000-0x3FF)
  * @retval	error state
  */
EepromErrorState eeprom_ReadIdPage(Eeprom* eeprom, uint8_t *pData, uint32_t len, uint32_t dataAddr)
{
	if(len == 0 || (dataAddr + len) > (2 * EEPROM_ID_PAGE_SIZE))
	{
		return EepromStorageError;
	}
	// Check to make sure the device is ready
	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);

	// Prepare the command + address header
	uint8_t txPacket[4];
	txPacket[0] = RDID_CMD;
	txPacket[1] = (uint8_t)((dataAddr >> 16) & 0xff);
	txPacket[2] = (uint8_t)((dataAddr >> 8) & 0xff);
	txPacket[3] = (uint8_t)(dataAddr & 0xff);

	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, txPacket, 4, HAL_MAX_DELAY) != HAL_OK)
	{
		HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
		return EepromHalError;
	}
	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
	if(HAL_SPI_Receive(eeprom->hspi, pData, len, HAL_MAX_DELAY) != HAL_OK)
	{
		HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
	return EepromOk;
}

/**
  * @brief 	Writes to an identification page.
  * The write must stay within a single 512 byte ID page: the internal address counter
  * rolls over at the page boundary, so a longer write would wrap and overwrite the
  * beginning of the same page. Application data belongs in the user ID page
  * (EEPROM_ID_USER_PAGE_ADDR); the device ID page content is factory programmed.
  * Fails once the ID pages have been locked (check with eeprom_IdPageLocked).
  * @param	eeprom eeprom struct
  * @param 	pData Pointer for the data to write
  * @param	len Number of bytes to be written (1-512)
  * @param	dataAddr Address to begin writing to (0x000-0x3FF)
  * @retval	error state
  */
EepromErrorState eeprom_WriteIdPage(Eeprom* eeprom, uint8_t *pData, uint32_t len, uint32_t dataAddr)
{
	if(len == 0 || len > EEPROM_ID_PAGE_SIZE || dataAddr >= (2 * EEPROM_ID_PAGE_SIZE))
	{
		return EepromStorageError;
	}
	// Reject writes that would wrap within the page
	if((dataAddr % EEPROM_ID_PAGE_SIZE) + len > EEPROM_ID_PAGE_SIZE)
	{
		return EepromStorageError;
	}
	// Check to make sure the device is ready
	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);

	// Send the write enable (WREN) instruction
	m95_WriteEnable(eeprom);

	// Prepare the command + address header
	uint8_t txPacket[4 + len];
	txPacket[0] = WRID_CMD;
	txPacket[1] = (uint8_t)((dataAddr >> 16) & 0xff);
	txPacket[2] = (uint8_t)((dataAddr >> 8) & 0xff);
	txPacket[3] = (uint8_t)(dataAddr & 0xff);
	for(uint32_t i=0; i<len; i++)
	{
		txPacket[4+i] = pData[i];
	}

	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, txPacket, len + 4, HAL_MAX_DELAY) != HAL_OK)
	{
		HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
	return m95_PollReady(eeprom, READY_CHECK_TIMEOUT);
}

/**
  * @brief 	Permanently locks the identification pages in read-only mode.
  * WARNING: this is irreversible. The lock is the non-volatile LID bit in the
  * configuration register, set by writing both the status and configuration registers
  * (the current values of all other non-volatile bits are preserved).
  * @param	eeprom eeprom struct
  * @retval	error state
  */
EepromErrorState eeprom_LockIdPage(Eeprom* eeprom)
{
	uint8_t statusReg, configReg, safetyReg;
	EepromErrorState status;

	status = m95_ReadStatusRegister(eeprom, &statusReg);
	if(status != EepromOk)
	{
		return status;
	}
	status = eeprom_ReadConfigRegisters(eeprom, &configReg, &safetyReg);
	if(status != EepromOk)
	{
		return status;
	}
	if((configReg >> EEPROM_CONFIG_LID_BIT) & 1)
	{
		// Already locked
		return EepromOk;
	}
	configReg |= (1 << EEPROM_CONFIG_LID_BIT);
	return m95p32_WriteStatusConfigRegisters(eeprom, statusReg, configReg, TRUE);
}

/**
  * @brief 	Reads the identification page lock status from the configuration register.
  * @param	eeprom eeprom struct
  * @param	locked Set to 1 if the ID pages are permanently locked, 0 otherwise
  * @retval	error state
  */
EepromErrorState eeprom_IdPageLocked(Eeprom* eeprom, uint8_t* locked)
{
	uint8_t configReg, safetyReg;
	EepromErrorState status = eeprom_ReadConfigRegisters(eeprom, &configReg, &safetyReg);
	if(status != EepromOk)
	{
		return status;
	}
	*locked = (configReg >> EEPROM_CONFIG_LID_BIT) & 1;
	return EepromOk;
}

/**
  * @brief 	Reads the configuration and safety registers with a single RDCR instruction.
  * Note: byte order (configuration first, then safety) follows the datasheet section
  * ordering — verify against hardware (delivered-state configuration reads 0x20:
  * DRV = 01 medium strength, LID = 0).
  * @param	eeprom eeprom struct
  * @param	configReg Pointer for the configuration register value (DRV1/DRV0, LID)
  * @param	safetyReg Pointer for the safety register value (PAMAF/PUF/ERF/PRF/ECC flags)
  * @retval	error state
  */
EepromErrorState eeprom_ReadConfigRegisters(Eeprom* eeprom, uint8_t* configReg, uint8_t* safetyReg)
{
	uint8_t txBuf[3] = {RDCR_CMD, 0, 0};
	uint8_t rxBuf[3];

	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_TransmitReceive(eeprom->hspi, txBuf, rxBuf, 3, HAL_MAX_DELAY) != HAL_OK)
	{
		HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);

	*configReg = rxBuf[1];
	*safetyReg = rxBuf[2];
	return EepromOk;
}

/**
  * @brief 	Clears the sticky flags in the safety register (PAMAF, ERF, PRF, ECC flags).
  * @param	eeprom eeprom struct
  * @retval	error state
  */
EepromErrorState eeprom_ClearSafetyFlags(Eeprom* eeprom)
{
	return m95p32_SendCommand(eeprom, CLRSF_CMD);
}

/**
  * @brief 	Reads the volatile register (BUFEN/BUFLD buffer mode bits).
  * @param	eeprom eeprom struct
  * @param	data Pointer for the register data to be read into
  * @retval	error state
  */
EepromErrorState eeprom_ReadVolatileRegister(Eeprom* eeprom, uint8_t* data)
{
	uint8_t txBuf[2] = {RDVR_CMD, 0};
	uint8_t rxBuf[2];

	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_TransmitReceive(eeprom->hspi, txBuf, rxBuf, 2, HAL_MAX_DELAY) != HAL_OK)
	{
		HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);

	*data = rxBuf[1];
	return EepromOk;
}

/**
  * @brief 	Writes the volatile register. Setting BUFEN enables buffered page program
  * (a page program instruction can be loaded while the previous one is executing).
  * @param	eeprom eeprom struct
  * @param	data Register value to write
  * @retval	error state
  */
EepromErrorState eeprom_WriteVolatileRegister(Eeprom* eeprom, uint8_t data)
{
	// Send the write enable (WREN) instruction
	m95_WriteEnable(eeprom);

	uint8_t txPacket[2] = {WRVR_CMD, data};
	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, txPacket, 2, HAL_MAX_DELAY) != HAL_OK)
	{
		HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
	return m95_PollReady(eeprom, READY_CHECK_TIMEOUT);
}

/**
  * @brief 	Sets the block write protection level via the status register.
  * The protected region is rejected by program/erase instructions and flagged in the
  * safety register (PAMAF). Note that erase instructions are only accepted at all
  * when the array is fully unprotected (bpLevel 0).
  * @param	eeprom eeprom struct
  * @param	bpLevel BP2:BP0 value, 0-7. 0 = unprotected, 1-6 = 1/64 to 1/2 of the
  * 		array, 7 = whole array
  * @param	protectBottom TB bit. 0 = protect from the top of the array, 1 = from the bottom
  * @retval	error state
  */
EepromErrorState eeprom_SetBlockProtection(Eeprom* eeprom, uint8_t bpLevel, uint8_t protectBottom)
{
	if(bpLevel > 7)
	{
		return EepromStorageError;
	}
	uint8_t statusReg;
	EepromErrorState status = m95_ReadStatusRegister(eeprom, &statusReg);
	if(status != EepromOk)
	{
		return status;
	}
	// Clear and set the BP2:BP0 and TB bits, preserving SRWD
	statusReg &= ~((1 << BP2_BIT) | (1 << BP1_BIT) | (1 << BP0_BIT) | (1 << TB_BIT));
	statusReg |= (bpLevel << BP0_BIT);
	if(protectBottom)
	{
		statusReg |= (1 << TB_BIT);
	}
	return m95p32_WriteStatusConfigRegisters(eeprom, statusReg, 0, FALSE);
}
#endif


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
EepromErrorState m95_Read(Eeprom* eeprom, uint8_t *pData, uint32_t size, uint32_t dataAddr)
{
	// Check to make sure the device is ready
	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);

	// Prepare the command + address header
	uint8_t txPacket[4];
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
EepromErrorState m95_Write(Eeprom* eeprom, uint8_t *data, uint32_t size, uint32_t dataAddr)
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
	EepromErrorState status = m95_PollReady(eeprom, READY_CHECK_TIMEOUT);
	if(status != EepromOk)
	{
		return status;
	}
	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, txPacket, size + 4, HAL_MAX_DELAY) != HAL_OK)
	{
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
	status = m95_PollReady(eeprom, READY_CHECK_TIMEOUT);
	if(status != EepromOk)
	{
		return status;
	}
	return status;
}

/**
  * @brief	Continuously reads the status register, checking for the WIP bit to be reset.
  * A timeout is added for device lockup, sized to the operation being waited on
  * (page writes complete in ~5mS, but block/chip erases can take up to 25mS).
  * @param	eeprom eeprom struct
  * @param	timeoutMs Maximum time to poll before returning EepromBusy
  * @retval	Error state. EepromOk if the device is ready, EepromBusy if the device is not ready
  */
EepromErrorState m95_PollReady(Eeprom* eeprom, uint32_t timeoutMs)
{
	uint8_t txBuf = RDSR_CMD;
	uint8_t rxBuf;
	uint8_t deviceBusy = TRUE;

	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, &txBuf, 1, HAL_MAX_DELAY) != HAL_OK)
	{
		return EepromHalError;
	}
	uint32_t startMs, timeMs = 0;
	#if FRAMEWORK_STM32CUBE
	startMs = HAL_GetTick();
	#elif FRAMEWORK_ARDUINO
	startMs = millis();
	#endif
	timeMs = startMs;
	while((timeMs - startMs) < timeoutMs)
	{
		// Read the status register contents
		while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
		if(HAL_SPI_Receive(eeprom->hspi, &rxBuf, 1, HAL_MAX_DELAY) != HAL_OK)
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
		timeMs = HAL_GetTick();
		#elif FRAMEWORK_ARDUINO
		timeMs = millis();
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
	uint8_t wrenPacket = WREN_CMD;
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, &wrenPacket, 1, HAL_MAX_DELAY) != HAL_OK)
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
	uint8_t wrdiPacket = WRDI_CMD;
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, &wrdiPacket, 1, HAL_MAX_DELAY) != HAL_OK)
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
{
	uint8_t txBuf[2] = {RDSR_CMD, 0};
	uint8_t rxBuf[2];

	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_TransmitReceive(eeprom->hspi, txBuf, rxBuf, 2, HAL_MAX_DELAY) != HAL_OK)
	{
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);

	*data = rxBuf[1];
	return EepromOk;
}

#if defined(M95P32)
/**
  * @brief	Sends a single-byte instruction with no address or data.
  * @param	eeprom eeprom struct
  * @param	cmd Instruction byte
  * @retval	Error state
  */
EepromErrorState m95p32_SendCommand(Eeprom* eeprom, uint8_t cmd)
{
	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, &cmd, 1, HAL_MAX_DELAY) != HAL_OK)
	{
		HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
	return EepromOk;
}

/**
  * @brief	Executes a self-timed erase instruction and waits for it to complete.
  * @param	eeprom eeprom struct
  * @param	cmd Erase instruction byte (PGER/SCER/BKER/CHER)
  * @param	dataAddr Address within the region to erase (ignored if hasAddress is FALSE)
  * @param	hasAddress TRUE if the instruction takes a 24-bit address (all except chip erase)
  * @param	timeoutMs Poll timeout matched to the erase cycle time of the operation
  * @retval	Error state
  */
EepromErrorState m95p32_Erase(Eeprom* eeprom, uint8_t cmd, uint32_t dataAddr, uint8_t hasAddress, uint32_t timeoutMs)
{
	// Check to make sure the device is ready
	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);

	// Send the write enable (WREN) instruction
	m95_WriteEnable(eeprom);

	uint8_t txPacket[4];
	uint16_t packetLen = 1;
	txPacket[0] = cmd;
	if(hasAddress)
	{
		txPacket[1] = (uint8_t)((dataAddr >> 16) & 0xff);
		txPacket[2] = (uint8_t)((dataAddr >> 8) & 0xff);
		txPacket[3] = (uint8_t)(dataAddr & 0xff);
		packetLen = 4;
	}

	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, txPacket, packetLen, HAL_MAX_DELAY) != HAL_OK)
	{
		HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
	return m95_PollReady(eeprom, timeoutMs);
}

/**
  * @brief	Writes the status register, and optionally the configuration register,
  * with a WRSR instruction (one or two data bytes).
  * @param	eeprom eeprom struct
  * @param	statusReg Status register value (non-volatile bits SRWD/TB/BP2/BP1/BP0)
  * @param	configReg Configuration register value (DRV1/DRV0/LID), sent only if writeConfig is TRUE
  * @param	writeConfig TRUE to send the configuration register as a second data byte
  * @retval	Error state
  */
EepromErrorState m95p32_WriteStatusConfigRegisters(Eeprom* eeprom, uint8_t statusReg, uint8_t configReg, uint8_t writeConfig)
{
	// Send the write enable (WREN) instruction
	m95_WriteEnable(eeprom);

	uint8_t txPacket[3] = {WRSR_CMD, statusReg, configReg};
	uint16_t packetLen = writeConfig ? 3 : 2;

	while(HAL_SPI_GetState(eeprom->hspi) != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_RESET);
	if(HAL_SPI_Transmit(eeprom->hspi, txPacket, packetLen, HAL_MAX_DELAY) != HAL_OK)
	{
		HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
		return EepromHalError;
	}
	HAL_GPIO_WritePin(eeprom->csPort, eeprom->csPin, GPIO_PIN_SET);
	return m95_PollReady(eeprom, WRSR_TIMEOUT);
}
#endif
#endif

#ifdef __cplusplus
}
#endif