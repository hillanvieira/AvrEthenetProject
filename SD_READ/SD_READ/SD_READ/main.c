#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define F_CPU 16000000UL
#include <util/delay.h>

#include "FAT16/fat16.h"
#include "SD/SD.h"


#define SPI_DDR DDRB
#define SPI_PORT PORTB
#define SPI_MOSI (1<<PORTB3)
#define SPI_MISO (1<<PORTB4)
#define SPI_SCK  (1<<PORTB5)


// Debugging Mode, 0 - Debug OFF, 1 - Debug ON
#define _DEBUG_MODE      1
#define DEBUG            1

#if _DEBUG_MODE
#define BAUD_RATE 57600
#endif


void SPI_init() {
	
	DDRB |= (1<<PORTB2); // Set SS as Output
	PORTB |= (1<<PORTB2); //Set SS as Output
	SD_CS_DDR |= SD_CS; // SD card circuit select as output
	SPI_DDR |= SPI_MOSI + SPI_SCK; // MOSI and SCK as outputs
	SPI_PORT |= SPI_MISO; // pullup in MISO, might not be needed
	
	// Enable SPI, master, set clock rate fck/16
	SPCR = (1<<SPE) | (1<<MSTR) |  (1<<SPR0);

}


//UART Config
#if _DEBUG_MODE

void uart_init(void)
{
	UBRR0H = (((F_CPU/BAUD_RATE)/16)-1)>>8;		// set baud rate
	UBRR0L = (((F_CPU/BAUD_RATE)/16)-1);
	UCSR0B = (1<<RXEN0)|(1<<TXEN0); 			// enable Rx & Tx
	UCSR0C=  (1<<UCSZ01)|(1<<UCSZ00);  	        // config USART; 8N1
}

void uart_flush(void)
{
	unsigned char dummy;

	while (UCSR0A & (1<<RXC0)) dummy = UDR0;
}

int uart_putch(char ch,FILE *stream)
{
	if (ch == '\n')
	uart_putch('\r', stream);

	while (!(UCSR0A & (1<<UDRE0)));
	UDR0=ch;

	return 0;
}

int uart_getch(FILE *stream)
{
	unsigned char ch;

	while (!(UCSR0A & (1<<RXC0)));
	ch=UDR0;

	/* Echo the Output Back to terminal */
	uart_putch(ch,stream);

	return ch;
}

void ansi_cl(void)
{
	// ANSI clear screen: cl=\E[H\E[J
	putchar(27);
	putchar('[');
	putchar('H');
	putchar(27);
	putchar('[');
	putchar('J');
}

void ansi_me(void)
{
	// ANSI turn off all attribute: me=\E[0m
	putchar(27);
	putchar('[');
	putchar('0');
	putchar('m');
}
#endif


#if _DEBUG_MODE
// Assign I/O stream to UART
FILE uart_str = FDEV_SETUP_STREAM(uart_putch, uart_getch, _FDEV_SETUP_RW);
#endif

int main(int argc, char *argv[]) {
	unsigned int i, ret;
	//short offset = 0x1BE;

#if _DEBUG_MODE
    // Define Output/Input Stream
    stdout = stdin = &uart_str;
    // Initial UART Peripheral
    uart_init();
    // Clear Screen
    ansi_me();
    ansi_cl();
    ansi_me();
    ansi_cl();
    uart_flush();
#endif

#if _DEBUG_MODE
#endif

	SPI_init();
	
	   printf("\nStart\r\n");

	if((ret = SD_init())) {
		
		printf("\r\nSD err: ");
		printf("%#X",ret);
		return -1;
	}
	
	if((ret = fat16_init())) {
		printf("\r\nFAT err: ");
		printf("%#X",ret);
		return -1;
	}
	
	if((ret = fat16_open_file("HAMLET  ", "TXT"))) {
		printf("\r\nOpen error: ");
		printf("%#X",ret);
		return -1;
	}
	
	while(fat16_state.file_left) {
		ret = fat16_read_file(FAT16_BUFFER_SIZE);
		for(i=0; i<ret; i++)
		printf("%c",fat16_buffer[i]);
	}
	
	return 0;
}

