/*
 * SD.h
 *
 * Created: 28/10/2016 15:32:12
 *  Author: HILLAN
 */ 


#ifndef SD_H_
#define SD_H_

#define SD_CS_DDR DDRD
#define SD_CS (1<<PORTD4)
#define SD_CS_ENABLE() (PORTD &= ~SD_CS)
#define SD_CS_DISABLE() (PORTD |= SD_CS)


//Functions
extern unsigned char SPI_WR_RD(unsigned char ch);
extern unsigned char SD_command(unsigned char cmd, unsigned long arg, unsigned char crc, unsigned char read);
extern char          SD_init();
extern void          SD_read(unsigned long sector, unsigned short offset, unsigned char * buffer, unsigned short len);
extern void          fat16_seek(unsigned long offset);
extern char          fat16_read(unsigned char bytes);

#endif /* SD_H_ */