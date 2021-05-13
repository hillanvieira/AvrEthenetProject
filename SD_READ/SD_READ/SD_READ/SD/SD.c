/*
 * SD.c
 *
 * Created: 28/10/2016 15:32:35
 *  Author: HILLAN
 */ 

#define F_CPU 16000000UL

#include "../FAT16/fat16.h"
#include "SD.h"
#include <stdio.h>
#include <util/delay.h>
#include <avr/io.h>


unsigned char SPI_WR_RD(unsigned char ch) {
	SPDR = ch;
	while(!(SPSR & (1<<SPIF))) {}
	return SPDR;
}

unsigned char SD_command(unsigned char cmd, unsigned long arg, unsigned char crc, unsigned char read) {
	unsigned char i, buffer[32], ret = 0xFF;
	
	#if _DEBUG_MODE
	unsigned char cmd_nb = cmd&0x3F;
	printf("SD CMD%d: ",cmd_nb);
	#endif
	
	SD_CS_ENABLE();
	SPI_WR_RD(cmd);
	SPI_WR_RD(arg>>24);
	SPI_WR_RD(arg>>16);
	SPI_WR_RD(arg>>8);
	SPI_WR_RD(arg);
	SPI_WR_RD(crc);
	
	for(i=0; i<read; i++)
	buffer[i] = SPI_WR_RD(0xFF);
	
	SD_CS_DISABLE();
	
	for(i=0; i<read; i++) {
		
		#if _DEBUG_MODE
		
		printf("%02X",buffer[i]);
		
		#endif
		
		if(buffer[i] != 0xFF)
		ret = buffer[i];
	}
	
	printf("\n");
	
	return ret;
}

unsigned long sd_sector;
unsigned short sd_pos;

char SD_init() {
	unsigned int i;
	
	// ]r:10
	SD_CS_DISABLE();
	for(i=0; i<10; i++) // idle for 1 bytes / 80 clocks
	SPI_WR_RD(0xFF);
	
	// [0x40 0x00 0x00 0x00 0x00 0x95 r:8] until we get "1"
	for(i=0; i<64 && SD_command(0x40, 0x00000000, 0x95, 8) != 1; i++)
	_delay_ms(10);
	
	if(i == 10) // card did not respond to initialization
	return -1;
	
	// CMD1 until card comes out of idle, but maximum of 10 times
	for(i=0; i<10 && SD_command(0x41, 0x00000000, 0xFF, 8) != 0; i++)
	_delay_ms(10);

	if(i == 10) // card did not come out of idle
	return -2;
	
	// SET_BLOCKLEN to 512
	SD_command(0x50, 0x00000200, 0xFF, 8);
	
	sd_sector = sd_pos = 0;
	
	_delay_ms(10);
	
	return 0;
}

// TODO: This function will not exit gracefully if SD card does not do what it should
void SD_read(unsigned long sector, unsigned short offset, unsigned char * buffer, unsigned short len) {
	unsigned int i, pos = 0;
	
	SD_CS_ENABLE();
	SPI_WR_RD(0x51);
	SPI_WR_RD(sector>>15); // sector*512 >> 24
	SPI_WR_RD(sector>>7);  // sector*512 >> 16
	SPI_WR_RD(sector<<1);  // sector*512 >> 8
	SPI_WR_RD(0);          // sector*512
	SPI_WR_RD(0xFF);
	
	for(i=0; i<100 && !(SPI_WR_RD(0xFF) == 0x00); i++) {} // wait for 0
	
	for(i=0; i<100 && !(SPI_WR_RD(0xFF) == 0xFE); i++) {} // wait for data start
	
	for(i=0; i<offset; i++) // "skip" bytes
	SPI_WR_RD(0xFF);
	
	for(i=0; i<len; i++) // read len bytes
	buffer[i] = SPI_WR_RD(0xFF);
	
	for(i+=offset; i<512; i++) // "skip" again
	SPI_WR_RD(0xFF);
	
	// skip checksum
	SPI_WR_RD(0xFF);
	SPI_WR_RD(0xFF);
	SPI_WR_RD(0xFF);
	
	SD_CS_DISABLE();
}

void fat16_seek(unsigned long offset) {
	sd_sector = offset >> 9;
	sd_pos = offset & 0x1FF;
}

char fat16_read(unsigned char bytes) {
	SD_read(sd_sector, sd_pos, fat16_buffer, bytes);
	sd_pos+=(unsigned short)bytes;
	
	if(sd_pos == 512) {
		sd_pos = 0;
		sd_sector++;
	}
	
	return bytes;
}