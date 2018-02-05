/*
 * flash_mibspi.c
 *
 *  Created on: Aug 24, 2017
 *      Author: Richard
 *
 *      Notes:
 *      Reading and writing address command is the BYTE to read/write. Since only pages can be erased, be careful with this.
 *
 *      Pages: 256 bytes (the most you can WRITE at once)
 *      Sectors: 4096 bytes
 *
 *      Therefore, 16 sectors per block
 */

#include "flash_mibspi.h"
#include "sfu_utils.h"


void mibspi_write_byte(uint16_t toWrite){
    while (TG1_IS_Complete != 0xA5){} // wait for other transfers to complete
    mibspi_send(FLASH_SINGLE_TRANSFER, &toWrite);
}

void mibspi_write_two(uint16_t arg1, uint16_t arg2){
    uint16_t dataIn[2] = {arg1, arg2};
    mibspi_send(FLASH_DOUBLE_TRANSFER, dataIn);
}

void flash_erase_chip(){
    mibspi_write_byte(WRITE_ENABLE);
    mibspi_write_byte(CHIP_ERASE);

    flash_busy_erasing_chip(); // only returns once chip is erased
}

void flash_busy_erasing_chip(){
    TG1_RX[0] = 0;
    TG1_RX[1] = 0;

    mibspi_write_two(READ_REG_STATUS,0x0000);
    mibspi_receive(FLASH_DOUBLE_TRANSFER,TG1_RX);

    while(TG1_RX[1] == 0x83){ // reports 131 while still erasing. Takes ~45ms to do a chip erase
        mibspi_write_two(READ_REG_STATUS,0x0000);
        mibspi_receive(FLASH_DOUBLE_TRANSFER,TG1_RX);
    }
}

void flash_set_burst_64(){
    mibspi_write_two(0x00c0, 0x0003);
    uint16 TG1_RX[1] = {0x0000};
    mibspi_receive(FLASH_DOUBLE_TRANSFER,TG1_RX);
}

void flash_write_enable(){
    mibspi_write_byte(WRITE_ENABLE);
}


void construct_send_packet_6(uint16_t command, uint32_t address, uint16_t *packet, uint16_t databytes){
    uint16_t sendOut[6] = { 0 };
    uint16_t index;

    // packet size = data bytes + command (1) + address (3) bytes
    // address is 24 bit, so fudge the bits around such that the SPI 3 bytes are in the correct order (MSB first)
    sendOut[0] = command;
    sendOut[1] = (address & 0xFF0000) >> 16;
    sendOut[2] = (address & 0xFF00) >> 8;
    sendOut[3] = (address) & 0xFF;

    for(index = 4; index < 4 + databytes; index++){
        sendOut[index] = packet[index - 4];
    }

    if(command == FLASH_WRITE){ // before write, need to write enable
        mibspi_write_byte(WRITE_ENABLE);
    }

    mibspi_send(FLASH0_TRANSFER_GROUP, sendOut);
}

void flash_read_16(uint32_t address, uint16_t *outBuffer){
    construct_send_packet_16(FLASH_READ, address, dummyBytes_16);
    mibspi_receive(FLASH_TWENTY,TG3_RX);

    // strip off the first 4 bytes of the receive, since those are the responses to the command and address
    int i = 0;
    for(i = 0; i < 16; i++){
        outBuffer[i] = TG3_RX[i + 4];
    }
}

void flash_read_16_rtos(uint32_t address, uint16_t *outBuffer){
	// in the RTOS so use a mutex
	xSemaphoreTake( xFlashMutex, pdMS_TO_TICKS(60) );
	{
		construct_send_packet_16(FLASH_READ, address, dummyBytes_16);
		mibspi_receive(FLASH_TWENTY,TG3_RX);

		// strip off the first 4 bytes of the receive, since those are the responses to the command and address
		int i = 0;
		for(i = 0; i < 16; i++){
			outBuffer[i] = TG3_RX[i + 4];
		}

	}
	xSemaphoreGive( xFlashMutex );
}

void flash_write_16_rtos(uint32_t address, uint16_t *outBuffer){
	xSemaphoreTake( xFlashMutex, pdMS_TO_TICKS(60) );
	{
		construct_send_packet_16(FLASH_WRITE, address, outBuffer);

	}
	xSemaphoreGive( xFlashMutex );
}

boolean rw16_test(uint32_t address){
    // this function writes 16 test bytes to the specified address. Chip should be erased first.
    // uses the transfer group with 20 byte size

    uint16_t test_bytes_16[16] = {0x0001, 0x0001, 0x0000, 0x0007, 0x0003, 0x0005, 0x000F, 0x0004, 0x0007, 0x000B, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
    construct_send_packet_16(FLASH_WRITE, address, test_bytes_16);

    while(flash_status() != 0){ // wait for the write to complete
    }

    // read them back
    uint16_t readBuf[16];
    flash_read_16(address, readBuf);

    // check that read is the same as written
    int i;
    for(i = 0; i < 16; i++ ){
        if(readBuf[i] != test_bytes_16[i]){
            return(FALSE);
        }
    }
    return(TRUE);
}

uint16_t flash_status(){
    mibspi_write_two(READ_REG_STATUS,0x0000);
    mibspi_receive(FLASH_DOUBLE_TRANSFER,TG1_RX);

    return TG1_RX[1];
}

void construct_send_packet_16(uint16_t command, uint32_t address, uint16_t *packet){
    uint16_t sendOut[20] = { 0 };
    uint16_t index;

    // packet size = data bytes + command (1) + address (3) bytes
    // address is 24 bit, so fudge the bits around such that the SPI 3 bytes are in the correct order (MSB out first)
    sendOut[0] = command;
    sendOut[1] = (address & 0xFF0000) >> 16;
    sendOut[2] = (address & 0xFF00) >> 8;
    sendOut[3] = (address) & 0xFF;

    for(index = 4; index < 4 + 16; index++){
        sendOut[index] = packet[index - 4];
    }

    if(command == FLASH_WRITE){ // before write, need to write enable
        mibspi_write_byte(WRITE_ENABLE);
    }
    mibspi_send(FLASH_TWENTY, sendOut);
}

void flash_write_arbitrary(uint32_t address, uint32_t size, uint8_t *src){
	// TODO: since this all works off 16-bit words, need to ensure we're ok with SPIFFss 8-bit src pointers
	// TODO: make RTOS-safe version.
	// for size, every 16 bytes, construct a packet and send it out.

	uint16_t sendOut[16] = { 0 };
	uint32_t outIndex; // the place in each output frame
	uint32_t inIndex; // the place in the input data buffer
	uint32_t numDummy;

	outIndex = 0; // the place in each frame
	for(inIndex = 0; inIndex < size; inIndex ++){
		sendOut[outIndex] = src[inIndex]; // put the next char into the send buffer
		outIndex++;
		if(outIndex == 16){ // (16 not 15 since we increment it right above) - every 16, send out and increment address for the next one
			construct_send_packet_16(FLASH_WRITE, address, sendOut);
			outIndex = 0;
			address += 16; // I think this is right.
		}
	}

	// Handle packing in dummy bytes for cases where data isn't a multiple of 16 bytes
	if(outIndex != 16){
		for(numDummy = 16 - outIndex; numDummy == 0; numDummy --){
			outIndex++;
			sendOut[outIndex] = 0xff; // empty or unprogrammed value for flash is 1
		}
		address += 16;
		construct_send_packet_16(FLASH_WRITE, address, sendOut);
	}
}

boolean flash_test_JEDEC(void){
    // reads the JEDEC ID. It's 3 bytes long and is 0xBF, 0x26, 0x42 for the SST26
    uint16_t rmdid[6] = {0x009F,0x0000,0x0000,0x0000,0x0000,0x0000};
    uint16_t TG0_RX[6] = {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};

    mibspi_send(FLASH0_TRANSFER_GROUP, rmdid);
    mibspi_receive(FLASH0_TRANSFER_GROUP,TG0_RX);

#if FLASH_CHIP_TYPE == 1 // 1 = IS25LP016D
    if(TG0_RX[1] == 157 && TG0_RX[2] == 96 && TG0_RX[3] == 21){
        return TRUE;
    }
    return FALSE;
#endif

#if FLASH_CHIP_TYPE == 0 // 0 = SST26
    if(TG0_RX[1] == 191 && TG0_RX[2] == 0x26 && TG0_RX[3] == 0x42){
        return TRUE;
    }
    return FALSE;
#endif
}

uint32_t getEmptySector(){
	uint16_t flashData[16] = {0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF};
	uint32_t tempAddress;
	uint32_t i;
	uint8_t emptyBytes;

	for(tempAddress = 0; tempAddress < 115200; tempAddress += 16){ // only check the first 450 pages so we're not here forever (that's about two hours of recording)
		emptyBytes = 16;
		simpleWatchdog();
		 flash_read_16(tempAddress, flashData);
		 for(i = 0; i < 16; i++){
			 if(flashData[i] != 0x00FF){
				 emptyBytes--;
			 }
		 }
		 if(emptyBytes == 16){ // if we read a section with all 1's
			 break;
		 }
	}
	return tempAddress;
}

void flash_mibspi_init(){
    // NOTE: call _enable_interrupt_(); before this function
    mibspiInit();
    mibspiEnableGroupNotification(FLASH_MIBSPI_REG,FLASH0_TRANSFER_GROUP,FLASH_DATA_FORMAT);
    mibspiEnableGroupNotification(FLASH_MIBSPI_REG,FLASH_SINGLE_TRANSFER,FLASH_DATA_FORMAT);
    mibspiEnableGroupNotification(FLASH_MIBSPI_REG,FLASH_DOUBLE_TRANSFER,FLASH_DATA_FORMAT);
    mibspiEnableGroupNotification(FLASH_MIBSPI_REG,FLASH_TWENTY,FLASH_DATA_FORMAT);

    TG0_IS_Complete = 0xA5; // start as complete
    TG1_IS_Complete = 0xA5; // start as complete
    TG2_IS_Complete = 0xA5; // start as complete
    TG3_IS_Complete = 0xA5; // start as complete

    // Init by write enable and global unlock
    flash_write_enable();
    mibspi_write_byte(ULBPR);
}

void mibspi_send(uint8_t transfer_group, uint16_t * TX_DATA){
    switch (transfer_group){
    case 0:
        TG0_IS_Complete = 0x0000;
        break;
    case 1:
        TG1_IS_Complete = 0x0000;
        break;
    case 2:
        TG2_IS_Complete = 0x0000;
        break;
    case 3:
        TG3_IS_Complete = 0x0000;
        break;
    }


    mibspiSetData(FLASH_MIBSPI_REG,transfer_group, TX_DATA);
    mibspiTransfer(FLASH_MIBSPI_REG,transfer_group);

    // wait for the transfer to complete. This is the same as mibspiIsTransferComplete, except it actually works

    // Commented this out for the RTOS version ... not sure why this is now apparently required
//    while(!((((FLASH_MIBSPI_REG->TGINTFLG & 0xFFFF0000U) >> 16U)>> (uint32)transfer_group) & 1U) == 1U){}
//    FLASH_MIBSPI_REG->TGINTFLG = (FLASH_MIBSPI_REG->TGINTFLG & 0x0000FFFFU) | ((uint32)((uint32)1U << (uint32)transfer_group) << 16U);

}

void mibspi_receive(uint8_t transfer_group,uint16_t * RX_DATA){
    switch(transfer_group){
    case 0:
        while(TG0_IS_Complete != 0xA5){
            // wait for the transfer to finish up
        }
        break;
    case 1:
        while(TG1_IS_Complete != 0xA5){
            // wait for the transfer to finish up
        }
        break;
    case 2:
        while(TG2_IS_Complete != 0xA5){
            // wait for the transfer to finish up
        }
        break;
    case 3:
        while(TG3_IS_Complete != 0xA5){
            // wait for the transfer to finish up
        }
        break;
    }
    mibspiGetData(FLASH_MIBSPI_REG,transfer_group,RX_DATA);
}

void mibspiGroupNotification(mibspiBASE_t *mibspi, uint32 group){
    /* This is the callback from the ISR. We use it to signal that a transfer has completed.
     *
     */

    switch (group){
    case 0 :
        /* Enable TG1 to start, SW Trigger */
        //            mibspiTransfer(mibspiREG1,1);
        TG0_IS_Complete = 0xA5;
        break;
    case 1:
        TG1_IS_Complete = 0xA5;
        break;
    case 2:
        TG2_IS_Complete = 0xA5;
        break;
    case 3:
        TG3_IS_Complete = 0xA5;
        break;
    default :
        while(1);
        break;
    }
}
