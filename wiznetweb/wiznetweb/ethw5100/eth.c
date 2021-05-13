/*
 * eth.c
 *
 * Created: 28/10/2016 03:20:35
 *  Author: HILLAN
 */ 

// Debugging Mode, 0 - Debug OFF, 1 - Debug ON
#define _DEBUG_MODE      0

#define F_CPU 16000000UL

#include <avr/io.h>
#include <string.h>
#include <stdio.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include "eth.h"

void SPI_Eth_Write(uint16_t addr,uint8_t data)
{
	// Activate the CS pin
	ETH_CS_ENABLE();

	// Start Wiznet W5100 Write OpCode transmission //When SPDR is write the MCU send signal SPI
	// WIZNET_WRITE_OPCODE = OxF0
	
	SPDR = WIZNET_WRITE_OPCODE;

	// Wait for transmission complete
	while(!(SPSR & (1<<SPIF)));

	// Start Wiznet W5100 Address High Bytes transmission
	SPDR = (addr & 0xFF00) >> 8;

	// Wait for transmission complete
	while(!(SPSR & (1<<SPIF)));

	// Start Wiznet W5100 Address Low Bytes transmission
	SPDR = addr & 0x00FF;

	// Wait for transmission complete
	while(!(SPSR & (1<<SPIF)));

	// Start Data transmission
	SPDR = data;

	// Wait for transmission complete
	while(!(SPSR & (1<<SPIF)));

	// CS pin is not active
	ETH_CS_DISABLE();
}

unsigned char SPI_Eth_Read(uint16_t addr)
{
	// Activate the CS pin
	ETH_CS_ENABLE();

	// Start Wiznet W5100 Read OpCode transmission
	// WIZNET_READ_OPCODE = 0x0F
	SPDR = WIZNET_READ_OPCODE;

	// Wait for transmission complete
	while(!(SPSR & (1<<SPIF)));

	// Start Wiznet W5100 Address High Bytes transmission
	SPDR = (addr & 0xFF00) >> 8;

	// Wait for transmission complete
	while(!(SPSR & (1<<SPIF)));

	// Start Wiznet W5100 Address Low Bytes transmission
	SPDR = addr & 0x00FF;

	// Wait for transmission complete
	while(!(SPSR & (1<<SPIF)));

	// Send Dummy transmission for reading the data
	SPDR = 0x00;

	// Wait for transmission complete
	while(!(SPSR & (1<<SPIF)));

	// CS pin is not active
	ETH_CS_DISABLE();

	return(SPDR);
}

void W5100_Init(uint8_t *ip_ptr, uint8_t *gtw_ptr, uint8_t *mask_ptr)
{
	// Ethernet Setup
	//unsigned char mac_addr[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
	unsigned char mac_addr[] = {0x00, 0x08, 0xDC, 0x01, 0x03, 0x03};
	unsigned char ip_addr[4];
	unsigned char sub_mask[4];
	unsigned char gtw_addr[4];

    ip_addr[0] = *(ip_ptr+0);
    ip_addr[1] = *(ip_ptr+1);
    ip_addr[2] = *(ip_ptr+2);
    ip_addr[3] = *(ip_ptr+3);

    gtw_addr[0] = *(gtw_ptr+0);
    gtw_addr[1] = *(gtw_ptr+1);
    gtw_addr[2] = *(gtw_ptr+2);
    gtw_addr[3] = *(gtw_ptr+3);

    sub_mask[0] = *(mask_ptr+0);
	sub_mask[1] = *(mask_ptr+1);
	sub_mask[2] = *(mask_ptr+2);
	sub_mask[3] = *(mask_ptr+3);

	// Setting the Wiznet W5100 Mode Register: 0x0000
	SPI_Eth_Write(MR,0x80);           // MR = 0b10000000;

	// Setting the Wiznet W5100 Gateway Address (GAR): 0x0001 to 0x0004
	SPI_Eth_Write(GAR + 0,gtw_addr[0]);
	SPI_Eth_Write(GAR + 1,gtw_addr[1]);
	SPI_Eth_Write(GAR + 2,gtw_addr[2]);
	SPI_Eth_Write(GAR + 3,gtw_addr[3]);

	// Setting the Wiznet W5100 Source Address Register (SAR): 0x0009 to 0x000E
	SPI_Eth_Write(SAR + 0,mac_addr[0]);
	SPI_Eth_Write(SAR + 1,mac_addr[1]);
	SPI_Eth_Write(SAR + 2,mac_addr[2]);
	SPI_Eth_Write(SAR + 3,mac_addr[3]);
	SPI_Eth_Write(SAR + 4,mac_addr[4]);
	SPI_Eth_Write(SAR + 5,mac_addr[5]);

	// Setting the Wiznet W5100 Sub Mask Address (SUBR): 0x0005 to 0x0008
	SPI_Eth_Write(SUBR + 0,sub_mask[0]);
	SPI_Eth_Write(SUBR + 1,sub_mask[1]);
	SPI_Eth_Write(SUBR + 2,sub_mask[2]);
	SPI_Eth_Write(SUBR + 3,sub_mask[3]);

	// Setting the Wiznet W5100 IP Address (SIPR): 0x000F to 0x0012
	SPI_Eth_Write(SIPR + 0,ip_addr[0]);
	SPI_Eth_Write(SIPR + 1,ip_addr[1]);
	SPI_Eth_Write(SIPR + 2,ip_addr[2]);
	SPI_Eth_Write(SIPR + 3,ip_addr[3]);

	// Setting the Wiznet W5100 RX and TX Memory Size (2KB),
	SPI_Eth_Write(RMSR,NET_MEMALLOC);
	SPI_Eth_Write(TMSR,NET_MEMALLOC);
}

void Eth_Close(uint8_t sock)
{
	if (sock != 0) return;

	// Send Close Command
	//S0_CR = 0x0401 // Socket 0: Comand Register Address
	//CR_CLOSE = 0x10 // Close socket
	SPI_Eth_Write(S0_CR,CR_CLOSE);

	// Waiting until the S0_CR is clear
	//S0_CR = 0x0401 // Socket 0: Comand Register Address
	while(SPI_Eth_Read(S0_CR));
}

void Eth_disconnect(uint8_t sock)
{
	if (sock != 0) return;
	// Send Disconnect Command
	//S0_CR = 0x0401 // Socket 0: Comand Register Address
	//CR_DISCON = 0x08 // Send closing request in tcp mode
	SPI_Eth_Write(S0_CR,CR_DISCON);
	
	//S0_CR = 0x0401 // Socket 0: Comand Register Address
	// Wait for Disconecting Process
	while(SPI_Eth_Read(S0_CR));
}

uint8_t Eth_socket(uint8_t sock,uint8_t eth_protocol,uint16_t tcp_port)
{
	uint8_t retval=0;

	if (sock != 0) return retval;

	// Make sure we close the socket first
	// S0_CR = 0x0401 // Socket 0: Comand Register Address
	// SOCK_CLOSED = 0x00
	if (SPI_Eth_Read(S0_SR) == SOCK_CLOSED) {
		Eth_Close(sock);
	}

	// Assigned Socket 0 Mode Register
	// S0_MR 0x0400 //Socket 0: Mode Register Adress
	SPI_Eth_Write(S0_MR,eth_protocol);

	// Now open the Socket 0
	// S0_PORT = 0x0404 //Socket0: Source Port:0x0404 to 0x0405
	// S0_CR = 0x0401 // Socket 0: Comand Register Address
	// CR_OPEN = 0x01 //Initialize or open socket
	SPI_Eth_Write(S0_PORT,((tcp_port & 0xFF00) >> 8 ));
	SPI_Eth_Write(S0_PORT + 1,(tcp_port & 0x00FF));
	SPI_Eth_Write(S0_CR,CR_OPEN);                   // Open Socket

	// Wait for Opening Process
	// S0_CR = 0x0401 // Socket 0: Comand Register Address
	while(SPI_Eth_Read(S0_CR));

	// Check for Init Status
	// S0_CR = 0x0401 // Socket 0: Comand Register Address
	// SOCK_INIT = 0x13 //Init state
	if (SPI_Eth_Read(S0_SR) == SOCK_INIT)
	retval=1;
	else
	Eth_Close(sock);

	return retval;
}

uint8_t Eth_Listen(uint8_t sock)
{
	uint8_t retval = 0;

	if (sock != 0) return retval;

	// S0_CR = 0x0401 // Socket 0: Comand Register Address
	// SOCK_INIT = 0x13 //Init state
	if (SPI_Eth_Read(S0_SR) == SOCK_INIT) {
		// Send the LISTEN Command
		// S0_CR = 0x0401 // Socket 0: Comand Register Address
		// CR_LISTEN = 0x02 //wait connection request in tcp mode(Server mode)
		SPI_Eth_Write(S0_CR,CR_LISTEN);

		// Wait for Listening Process
		// S0_CR = 0x0401 // Socket 0: Comand Register Address
		while(SPI_Eth_Read(S0_CR));

		// Check for Listen Status
		// S0_CR = 0x0401 // Socket 0: Comand Register Address
		// SOCK_LISTEN = 0x14
		if (SPI_Eth_Read(S0_SR) == SOCK_LISTEN)
		retval=1;
		else
		Eth_Close(sock);
	}
	return retval;
}

uint16_t Eth_Send(uint8_t sock,const uint8_t *ethbuffer,uint16_t buffleng)
{
	uint16_t ptr,offsetaddrs,realaddrs,txsize,timeout;

	if (buffleng <= 0 || sock != 0) return 0;

	#if _DEBUG_MODE
	printf("Send Size: %d\n",buffleng);
	#endif

	// Make sure the TX Free Size Register is available
	// SO_TX_FSR 0x0420 //Socket 0:Tx Free Size Register: 0x0420 to 0x0421
	txsize=SPI_Eth_Read(SO_TX_FSR);
	txsize=(((txsize & 0x00FF) << 8 ) + SPI_Eth_Read(SO_TX_FSR + 1));

	#if _DEBUG_MODE
	printf("TX Free Size: %d\n",txsize);
	#endif

	timeout=0;
	while (txsize < buffleng) {
		SPI_Eth_Write(S0_TX_WR,(TXBASEADDR & 0xFF00) >> 8 );
		SPI_Eth_Write(S0_TX_WR + 1,(TXBASEADDR & 0x00FF));
		_delay_ms(1);
		// SO_TX_FSR 0x0420 //Socket 0:Tx Free Size Register: 0x0420 to 0x0421
		txsize=SPI_Eth_Read(SO_TX_FSR);
		txsize=(((txsize & 0x00FF) << 8 ) + SPI_Eth_Read(SO_TX_FSR + 1));

		// Timeout for approx 1000 ms
		if (timeout++ > 1000) {
			#if _DEBUG_MODE
			printf("TX Free Size Error!\n");
			#endif
			// Disconnect the connection
			Eth_disconnect(sock);
			return 0;
		}
	}

	// Read the Tx Write Pointer
	// S0_TX_WR =  0x0424 // Socket 0:Write Pointer Register: 0x0424 to 0x0425
	ptr = SPI_Eth_Read(S0_TX_WR);
	offsetaddrs = (((ptr & 0x00FF) << 8 ) + SPI_Eth_Read(S0_TX_WR + 1));
	#if _DEBUG_MODE
	printf("TX Buffer: %x\n",offsetaddrs);
	#endif

	while(buffleng) {
		buffleng--;
		// Calculate the real W5100 physical Tx Buffer Address
		//TXBUFADDR = 0x4000 //W5100 Send Buffer Base Adress
		//TX_BUF_MASK = 0x07FF //Tx 2K Buffer Mask
		realaddrs = TXBASEADDR + (offsetaddrs & TX_BUF_MASK);

		// Copy the application data to the W5100 Tx Buffer
		SPI_Eth_Write(realaddrs,*ethbuffer);
		offsetaddrs++;
		ethbuffer++;
	}

	// Increase the S0_TX_WR value, so it point to the next transmit
	// S0_TX_WR =  0x0424 // Socket 0:Write Pointer Register: 0x0424 to 0x0425
	SPI_Eth_Write(S0_TX_WR,(offsetaddrs & 0xFF00) >> 8 );
	SPI_Eth_Write(S0_TX_WR + 1,(offsetaddrs & 0x00FF));

	// Now Send the SEND command
	// S0_CR = 0x0401 // Socket 0: Comand Register Address
	//CR_SEND = 0x20  //Update Tx Memory pointer and send data
	SPI_Eth_Write(S0_CR,CR_SEND);

	// Wait for Sending Process
	// S0_CR = 0x0401 // Socket 0: Comand Register Address
	while(SPI_Eth_Read(S0_CR));

	return 1;
}

uint16_t Eth_Recvr(uint8_t sock,uint8_t *ethbuffer,uint16_t buffleng)
{
	uint16_t ptr,offsetaddrs,realaddrs;

	if (buffleng <= 0 || sock != 0) return 1;

	// If the request size > MAX_BUF,just truncate it
	//MAX_BUF = 512
	if (buffleng > MAX_BUF)
	buffleng = MAX_BUF - 2;

	// Read the Rx Read Pointer
	//S0_RX_RD = 0x0428 //socket 0; RX Read pointer: 0x0428 to 0x0429
	ptr = SPI_Eth_Read(S0_RX_RD);
	offsetaddrs = (((ptr & 0x00FF) << 8 ) + SPI_Eth_Read(S0_RX_RD + 1));
	#if _DEBUG_MODE
	printf("RX Buffer: %x\n",offsetaddrs);
	#endif

	while(buffleng) {
		buffleng--;
		//RXBUFADDR 0x6000 //W5100 Read Buffer Base Adress
		//RX_BUF_MASK 0x07FF //RX 2K Buffer Mask
		realaddrs=RXBUFADDR + (offsetaddrs & RX_BUF_MASK);
		*ethbuffer = SPI_Eth_Read(realaddrs);
		offsetaddrs++;
		ethbuffer++;
	}
	*ethbuffer='\0';        // String terminated character

	// Increase the S0_RX_RD value, so it point to the next receive
	//S0_RX_RD = 0x0428 //socket 0; RX Read pointer: 0x048 to 0x0429
	SPI_Eth_Write(S0_RX_RD,(offsetaddrs & 0xFF00) >> 8 );
	SPI_Eth_Write(S0_RX_RD + 1,(offsetaddrs & 0x00FF));

	// Now Send the RECV command
	// S0_CR = 0x0401 // Socket 0: Comand Register Address
	//CR_RECV = 0x40 //Update RX memory buffer pointer and receive data
	SPI_Eth_Write(S0_CR,CR_RECV);
	_delay_us(5);    // Wait for Receive Process

	return 1;
}

uint16_t Eth_Recvr_size(void)
{
	// S0_RX_RSR = 0x0426 //Socket 0:RX Received Size Pointer Register: 0x0426 to 0x0427
	return ((SPI_Eth_Read(S0_RX_RSR) & 0x00FF) << 8 ) + SPI_Eth_Read(S0_RX_RSR + 1);
}


//Function to compare Eth index with a text;
//check reciver commands from web page
int Eth_cmp_Str_index(char *s,char *t)
{
	uint16_t i,n;

	//n = number at adress on t pointer,<string.h> strlen function() return object lenght
	n=strlen(t);
	
	// s[i] = *(s+i)
	for(i=0;*(s+i) != '\0' ; i++) {
		
		//<string.h> strncmp function() comprate two strings and return 0 if the contents of both strings are equal
		if (strncmp(s+i,t,n) == 0)
		return i;
	}
	return -1;
}