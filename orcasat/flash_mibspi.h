/*
 * flash_mibspi.h
 *
 *  Created on: Aug 24, 2017
 *      Author: Richard
 *
 *      Flash driver for SST26 flash memory.
 *      In HALCoGEN, 4 transfer groups were created. This is somewhat efficient, as we don't waste time sending a ton of
 *      dummy bytes when we just need to send a single byte command, as we would with only 1 large transfer group.
 *
 *      TG number	Number of bytes
 *      0			6
 *      1			1
 *      2			2
 *      3			20
 *      4			4
 *
 *      Keep in mind that ERASED flash is all 1's. So don't fill space with 0's if you will want to use it later, since erasing
 *      is slow.
 *
 *      Flash addresses are always the BYTE number. Address 0 = byte 0. Address 349 = byte 349. Sectors are 4 kbytes, so bytes (addresses) 0-4095 are on the same sector.
 *      Byte 4096 is on a different sector.
 */

#ifndef SFUSAT_FLASH_MIBSPI_H_
#define SFUSAT_FLASH_MIBSPI_H_

#include "mibspi.h"
#include "FreeRTOS.h"
#include "obc_hardwaredefs.h"
#include "rtos_semphr.h"

// flags for complete transfers
extern uint8_t TG0_IS_Complete;
extern uint8_t TG1_IS_Complete;
extern uint8_t TG2_IS_Complete;
extern uint8_t TG3_IS_Complete;
extern uint8_t TG4_IS_Complete;

// Flash Specific
void flash_mibspi_init();
void flash_erase_chip();
void flash_set_burst_64();
void flash_erase_sector(uint32_t address);
uint16_t flash_status();
void flash_busy_erasing_chip();
void flash_read_16(uint32_t address, uint16_t *inBuffer);
//void flash_read_16_rtos(uint32_t address, uint16_t *inBuffer);
//void flash_write_16_rtos(uint32_t address, uint16_t *inBuffer);
uint32_t getEmptySector(); // finds the first section with 16 1's in it


// tests
boolean flash_test_JEDEC(void); // reads and confirms JEDEC ID
boolean rw16_test(uint32_t address); // reads and writes 16 bytes to the specified address

// Data construction and send
void construct_send_packet_6(uint16_t command, uint32_t address, uint16_t * packet, uint16_t databytes);
void construct_send_packet_16(uint16_t command, uint32_t address, uint16_t * packet);

// For SPIFFS
void flash_write_arbitrary(uint32_t address, uint32_t size, uint8_t *src); // write an arbitrary data buffer to flash
void flash_read_arbitrary(uint32_t address, uint32_t size, uint8_t *dest);

// SPI drivers
void mibspi_send(uint8_t transfer_group, uint16_t * TX_DATA);
void mibspi_receive(uint8_t transfer_group,uint16_t * RX_DATA);
void mibspi_write_byte(uint16_t toWrite);
void mibspi_write_two(uint16_t arg1, uint16_t arg2);

// Flash Commands
#define FLASH_READ 0x0003
#define FLASH_RDID 0xAB00 // flash read product ID + 1 dummy byte
#define DUMMY2 0x0000 // since we're using 16-bit words, align the commands to the left edge of the word. Then have trailing zeros (see above). Send in 4 bytes (2 words) as {command, DUMMY2}

// instructions
#define WRITE_ENABLE 0x0006 // send this before flash_write
#define FLASH_WRITE 0x0002 // program page command - must come after write enable
#define READ_REG_STATUS 0x0005 // read status register (bit 0 = WIP)
#define NORD 0x0003 // normal read
#define RMDID 0x0090 // read device manufacturer or something
#define RDJDID 0x009F
#define ULBPR 0x0098 // global write unlock
#define CHIP_ERASE 0x00c7
#define SECTOR_ERASE 0x00D7

// status register
#define STATUS_WIP 0x01 // WIP bit of status register
#define STATUS_WEL 0x02 // 1 = write enabled

#endif /* SFUSAT_FLASH_MIBSPI_H_ */
