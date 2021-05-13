/*
* wiznetweb.c
*
* Created: 20/10/2016 12:20:29
* Author : HILLAN
*/

/*****************************************************************************
//  File Name    : wiznetweb.c
//  Version      : 1.0
//  Description  : AVR Mega328p and Wiznet W5100 Web Server
//  Author       : RWB
//  Target       : Mega328p Board
//  Compiler     : AVR-GCC 4.3.2; avr-libc 1.6.6 (WinAVR 20090313)
//  IDE          : Atmel AVR Studio 7.8
//  Programmer   : AVRJazz Mega328 STK500 v2.0 Bootloader
//               : AVR Visual Studio 4.17, STK500 programmer
//  Last Updated : 28/10/2016
*****************************************************************************/

#define F_CPU 16000000UL
#define BAUD_RATE 57600

// Debugging Mode, 0 - Debug OFF, 1 - Debug ON
#define _DEBUG_MODE      1

#include <avr/io.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "uart/uart.h"
#include "ethw5100/eth.h"
#include "SD/SD.h"
#include "FAT16/fat16.h"


#define SPI_DDR          DDRB
#define SPI_PORT         PORTB
#define SPI_MOSI         (1<<PORTB3)
#define SPI_MISO         (1<<PORTB4)
#define SPI_SCK          (1<<PORTB5)


// Define W5100 Socket Register and Variables Used
uint8_t sockreg;
uint8_t ETHBUFFER[MAX_BUF];
uint8_t FIRSTRESPONSEETHBUFFER[60];
uint8_t EthFlood = 0;
int     tempvalue;
char    temp[10];

void EEPROM_write(uint8_t address, uint8_t data)
{
	/* Wait for completion of previous write */
	while(EECR & (1<<EEPE))
	;
	/* Set up address and Data Registers */
	EEAR = address;
	EEDR = data;
	/* Write logical one to EEMPE */
	EECR |= (1<<EEMPE);
	/* Start eeprom write by setting EEPE */
	EECR |= (1<<EEPE);
	
}


uint8_t EEPROM_read(uint8_t address)
{
	/* Wait for completion of previous write */
	while(EECR & (1<<EEPE))
	;
	/* Set up address register */
	EEAR = address;
	/* Start eeprom read by writing EERE */
	EECR |= (1<<EERE);
	/* Return data from Data Register */
	return EEDR;
	
}



void SPI_init() {
	
	
	// Initial the AVR ATMega328p SPI Peripheral
	// Set MOSI (PORTB3),SCK (PORTB5) and PORTB2 (SS) as output, others as input
	DDRB = (1<<PORTB3)|(1<<PORTB5)|(1<<PORTB2);
	
	//Pino SS HIGH
	//É necessario colocar o pin SS como saida e nivel lógico alto
	//para poder controlar outros disposivos via interface SPI
	PORTB |= (1<<PORTB2); //Set SS as Output
	
	//Pino CS SPI do SD_CARD
	DDRD   |= (1<<PORTD4); // SD card circuit select as output
	PORTD |= (1<<PORTD4); // SD card HIGH
	
	// Enable SPI, master, set clock rate fck/16
	SPCR = (1<<SPE) | (1<<MSTR) |  (1<<SPR0);
	
}


//Put SPI SCLK to 8MHZ
void SPI_speed(){ 
	
	SPCR &= ~(1<<SPE);
	_delay_ms(1);
	SPCR &= ~(1<<SPR0);
	SPCR |=  (1<<SPE);
	SPSR |=  (1<<SPI2X);
	_delay_ms(1);
	
}

void ADC_init(){
	
	//ADEN: Ativa a conversão AD do microcontrolador, por defalt fica desligado.
	//ADPS2: ADPS2, ADPS1 e ADPS0 configura o prescaler do conversor 16000000MHz/128 = 125KHz. Frequencia ideal conforme datasheet
	ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);
	
}

void TIMERs_init(){
	
	// Initial ATMega368 Timer/Counter0 Peripheral
	TCCR0A=0x00;                  // Normal Timer0 Operation
	TCCR0B=(1<<CS02)|(1<<CS00);   // Use maximum prescaller: Clk/1024
	TCNT0=0x94;                   // Start counter from 0x94, overflow at 10 mSec
	TIMSK0=(1<<TOIE0);            // Enable Counter Overflow Interrupt
	sei();                        // Enable Interrupt
	
}


ISR(TIMER0_OVF_vect) //Interrupção por estouro to TIMER0
{
	static unsigned char tenms=1;

	tenms++;                  // Read ADC every 20 x 10ms = 200 milisecond
	if (tenms >= 20) {
		cli();                // Disable Interupt


		// Set ADMUX Channel for LM35DZ Input
		// Leitura do pino PC0/ADC0  (ARDUINO UNO - A0)
		ADMUX=0x40;

		// Start conversion by setting ADSC on ADCSRA Register
		ADCSRA |= (1<<ADSC);

		// wait until convertion complete ADSC=0 -> Complete
		while (ADCSRA & (1<<ADSC));

		// Get the ADC Result
		tempvalue = ADCW;

		// ADC = (Vin x 1024) / Vref, Vref = 1 Volt, LM35DZ Out = 10mv/C
		tempvalue = (int)(tempvalue) / 10.24;

		tenms=1;

		sei();                            // Enable Interupt
	}

	// Start counter from 0x94, overflow at 10 mSec
	TCNT0=0x94;
}


#if _DEBUG_MODE
// Assign I/O stream to UART
FILE uart_str = FDEV_SETUP_STREAM(uart_putch, uart_getch, _FDEV_SETUP_RW);
#endif

void ETH_SEND_FILE(char *filename, char *ext){
	unsigned int pointer;
	fat16_init();
	fat16_open_file(filename,ext);
	
	while(fat16_state.file_left) {
		pointer = fat16_read_file(FAT16_BUFFER_SIZE);
		Eth_Send(sockreg,fat16_buffer,pointer);
	}
	
};

void mainpageswitch(int id){
	
	switch (id){
		
		case 0:
		ETH_SEND_FILE("LOGIN   ", "HTM");
		
		break;
		
		case 1:
		ETH_SEND_FILE("INDEX   ", "HTM");
		
		break;
	}
	
	
	
}


void save_stdnet_eeprom(){
	
	uint8_t  ip_addr[]   = {192,168,000,200};
	uint8_t  sub_mask[]  = {255,255,255,000};
	uint8_t  gtw_addr[]  = {192,168,000,001};
	uint8_t  dft_user[9]	 = "admin";
    uint8_t  dft_password[9] = "admin";		
	
	//salva ip nos endereços       0x00 - 0x03
	EEPROM_write(0x00,ip_addr[0]);
	EEPROM_write(0x01,ip_addr[1]);
	EEPROM_write(0x02,ip_addr[2]);
	EEPROM_write(0x03,ip_addr[3]);
	
	//salva gateway nos endereços  0x04 - 0x07
	EEPROM_write(0x04,gtw_addr[0]);
	EEPROM_write(0x05,gtw_addr[1]);
	EEPROM_write(0x06,gtw_addr[2]);
	EEPROM_write(0x07,gtw_addr[3]);
	
	//salva sub-mask nos endereços 0x08 - 0x0B
	EEPROM_write(0x08,sub_mask[0]);
	EEPROM_write(0x09,sub_mask[1]);
	EEPROM_write(0x0A,sub_mask[2]);
	EEPROM_write(0x0B,sub_mask[3]);
	
	//salva User nos endereços 0x0C - 0x14
	EEPROM_write(0x0C,dft_user[0]);
	EEPROM_write(0x0D,dft_user[1]);
	EEPROM_write(0x0E,dft_user[2]);
	EEPROM_write(0x0F,dft_user[3]);
	EEPROM_write(0x10,dft_user[4]);
	EEPROM_write(0x11,dft_user[5]);
	EEPROM_write(0x12,dft_user[6]);
	EEPROM_write(0x13,dft_user[7]);
	EEPROM_write(0x14,dft_user[8]);
	
	//salva Password nos endereços 0x15 - 0x1D
	EEPROM_write(0x15,dft_password[0]);
	EEPROM_write(0x16,dft_password[1]);
	EEPROM_write(0x17,dft_password[2]);
	EEPROM_write(0x18,dft_password[3]);
	EEPROM_write(0x19,dft_password[4]);
	EEPROM_write(0x1A,dft_password[5]);
	EEPROM_write(0x1B,dft_password[6]);
	EEPROM_write(0x1C,dft_password[7]);
	EEPROM_write(0x1D,dft_password[8]);
	
}

char *get_user_addr()
{
	
	char *get_user_ptr  = malloc(sizeof(char) * 9);
	
	for (uint8_t i = 0x00; i <= 8; i++ )
	{
		*(get_user_ptr+i) =  EEPROM_read(0x0C+i);
		
		#if _DEBUG_MODE
		printf("User %c\n", *(get_user_ptr+i));
		#endif
	}

	return (char *)get_user_ptr;
	
}

char *get_password_addr()
{
	
	char *get_password_ptr  = malloc(sizeof(char) * 9);
	
	#if _DEBUG_MODE
	printf("\n");
	#endif
	
	for (uint8_t i = 0x00; i <= 8; i++ )
	{
		*(get_password_ptr+i) =  EEPROM_read(0x15+i);
		
		#if _DEBUG_MODE
		printf("Password %c\n", *(get_password_ptr+i));
		#endif	
		
	}

	return (char *)get_password_ptr;
}

uint8_t *get_ip_addr()
{
	
	uint8_t *get_ip_ptr  = malloc(sizeof(uint8_t) * 4);
	
	for (uint8_t i = 0x00; i <= 3; i++ )
	{
		*(get_ip_ptr+i) =  EEPROM_read(i);
		
		#if _DEBUG_MODE
		printf("\n IP %d", *(get_ip_ptr+i));
		#endif
		
	}

	return (uint8_t *)get_ip_ptr;
	
}

uint8_t *get_gtw_addr()
{
	
	uint8_t *get_gtw_ptr  = malloc(sizeof(uint8_t) * 4);
	
	for (uint8_t i = 0x00; i <= 3; i++ )
	{
		*(get_gtw_ptr+i) =  EEPROM_read(0x04+i);
		
		#if _DEBUG_MODE
		printf("\n GTW %d", *(get_gtw_ptr+i));
		#endif
		
	}

	return (uint8_t *)get_gtw_ptr;
	
}

uint8_t *get_mask_addr()
{
	
	uint8_t *get_mask_ptr  = malloc(sizeof(uint8_t) * 4);
	
	for (uint8_t i = 0x00; i <= 3; i++ )
	{
		*(get_mask_ptr+i) =  EEPROM_read(0x08+i);
		
		#if _DEBUG_MODE
		printf("\n MASK %d", *(get_mask_ptr+i));
		#endif
		
	}

	return (uint8_t *)get_mask_ptr;
	
}


char *getSet(char *s1, char *s2,char *s3){
	
	uint16_t i,n;
	
	n = strlen(s2);
	
	for(i=0; *(strstr(s1,s2)+i+n) != '&' ; i++) {
		
		*(s3+i) = *(strstr(s1,s2)+i+n);
		
	}
	
	*(s3+i) = '\0';
	
	return (char *)s3;
	
}

int main(void){
	//delay para estabilizar 
	_delay_ms(10);
	
	//disable global interrupt
	cli();
	
	// Initial variable used
	sockreg = 0;
	tempvalue = 0;
	uint8_t sockstat;
	uint16_t rsize;
	int getidx,postidx;
	unsigned int i, ret;
	static uint8_t keyid = 0;
	static char login[18];
	
	DDRC  = 1<<PORTC1 | 1<<PORTC2 | 1<<PORTC3;     // Set PORTC1, PORTC2 and PORTC3 as Output
	PORTC &= ~(1<<PORTC1); //set PORTC1 LOW
	PORTC &= ~(1<<PORTC2); //set PORTC2 LOW
	PORTC &= ~(1<<PORTC3); //set PORTC3 LOW
	
	#if _DEBUG_MODE
	// Initial UART Peripheral
	uart_init();
	
	// Assign I/O stream to UART
	stdout = stdin = &uart_str;

	// Clear Screen
	ansi_me();
	ansi_cl();
	ansi_me();
	ansi_cl();
	uart_flush();
	#endif

	// Initial ATMega328p ADC Peripheral
	ADC_init();
	_delay_us(10);
	
	// Initial the AVR ATMega328p SPI Peripheral
	SPI_init();
	_delay_us(10);
	
	// salva na eeprom as configurações de rede default
	if (EEPROM_read(0x00) == 0xFF)
	{
	save_stdnet_eeprom();
	}
	
	
	//assing login on variable
	sprintf((char *)login,"%s%s",get_user_addr(),get_password_addr());
	
	// Initial the W5100 Ethernet
	W5100_Init(get_ip_addr(),get_gtw_addr(),get_mask_addr());
   
    
    _delay_us(10);

	if((ret = SD_init())) {
		
		#if _DEBUG_MODE
		printf("\r\nSD err: ");
		printf("%#X",ret);
		#endif
	}

	SPI_speed();

	//while(fat16_state.file_left) {
	//	ret = fat16_read_file(FAT16_BUFFER_SIZE);
	//	for(i=0; i<ret; i++)
	//	printf("%c",fat16_buffer[i]);
	//}



	#if _DEBUG_MODE
	printf("WEB Server Debug Mode\n\n");
	#endif

    // Initial ATMega368 Timer/Counter0 Peripheral
    TIMERs_init();


    //ativa as interrupçoes globais
    sei();
     
	// Loop forever
	while(1){
		
		// S0_CR = 0x0401 // Socket 0: Comand Register Address
		sockstat=SPI_Eth_Read(S0_SR);
		
		
		switch(sockstat) {
			//SOCK_CLOSED 0x00 //Closed
			case SOCK_CLOSED:
			//socket function() configura protocolo TCP, PORTA TCP e ABRE O SOCKET
			//MR_TCP = 0x01 //TCP
			//TCP_PORT = 80 //TCP/ip Port
			if (Eth_socket(sockreg,MR_TCP,TCP_PORT) > 0) {
				// Listen to Socket 0
				
				if (Eth_Listen(sockreg) < 0) //mudança de <= para < 0;
				_delay_ms(1);
				#if _DEBUG_MODE
				printf_P(PSTR("SOCKET LISTEN!\n"));
				#endif
			}
			break;

			// SOCK_ESTABILISHED = 0x17 //Sucess to connect
			case SOCK_ESTABLISHED:
			
			
			// Get the client request size
			// recv_size function() return(S0_RX_RSR = Socket 0:RX Received Size Pointer Register: 0x0425 to 0x0427)
			rsize=Eth_Recvr_size();
			#if _DEBUG_MODE
			printf("Size: %d\n",rsize);
			#endif
			if (rsize > 0) {
				// Now read the client Request
				// ETHBUFFER = array buf[MAX_BUF] and MAX_BUF = 512 ou seja array com 512 endereços
				// on recv function() ETHBUFFER recive data from client (http data)
				if (Eth_Recvr(sockreg,ETHBUFFER,rsize) <= 0) break;
				#if _DEBUG_MODE
				printf("Content:\n%s\n",ETHBUFFER);
				#endif
				// Check the Request Header
				
				getidx=Eth_cmp_Str_index((char *)ETHBUFFER,"GET /");
				postidx=Eth_cmp_Str_index((char *)ETHBUFFER,"POST /");

				if (getidx >= 0 || postidx >= 0) {
					
					//checa o cookie de login que fica salvo como cookie e é enviado tmmbém no header
					if (Eth_cmp_Str_index((char *)ETHBUFFER, strcpy("keyid=",login)) > 0){
						keyid=1;
					}else{
						keyid=0;
					}
					
					#if _DEBUG_MODE	
				    printf("Request\n");
					printf("getidx: %d\n",getidx);
					printf("postidx: %d\n",postidx);
					#endif
					
					if (postidx >= 0 && keyid == 1) {
						
						
						if (Eth_cmp_Str_index((char *)ETHBUFFER, "SETCONFIG") > 0){
							
						cli();	
							
						char* setBuff = malloc(sizeof(char) * 10);	
							
						
						#if _DEBUG_MODE
						printf_P(PSTR("\nIP1 %d"),atoi(getSet((char *)ETHBUFFER, "ip_1=",setBuff)));
						#endif	
							
					    EEPROM_write(0x00,atoi(getSet((char *)ETHBUFFER, "ip_1=",setBuff)));
						
						#if _DEBUG_MODE
						printf_P(PSTR("\nIP2 %d"),atoi(getSet((char *)ETHBUFFER, "ip_2=",setBuff)));
						#endif
						EEPROM_write(0x01,atoi(getSet((char *)ETHBUFFER, "ip_2=",setBuff)));
						
						#if _DEBUG_MODE
						printf_P(PSTR("\nIP3 %d"),atoi(getSet((char *)ETHBUFFER, "ip_3=",setBuff)));
						#endif
					    EEPROM_write(0x02,atoi(getSet((char *)ETHBUFFER, "ip_3=",setBuff)));
					    
						#if _DEBUG_MODE
						printf_P(PSTR("\nIP4 %d"),atoi(getSet((char *)ETHBUFFER, "ip_4=",setBuff)));
						#endif
						EEPROM_write(0x03,atoi(getSet((char *)ETHBUFFER, "ip_4=",setBuff)));
						
						#if _DEBUG_MODE
					    printf_P(PSTR("\ngateway1 %d"),atoi(getSet((char *)ETHBUFFER, "gateway_1=",setBuff)));
						#endif
						EEPROM_write(0x04,atoi(getSet((char *)ETHBUFFER, "gateway_1=",setBuff)));
						
						#if _DEBUG_MODE
				        printf_P(PSTR("\ngateway2 %d"),atoi(getSet((char *)ETHBUFFER, "gateway_2=",setBuff)));
						#endif
						EEPROM_write(0x05,atoi(getSet((char *)ETHBUFFER, "gateway_2=",setBuff)));
						
						#if _DEBUG_MODE
				        printf_P(PSTR("\ngateway3 %d"),atoi(getSet((char *)ETHBUFFER, "gateway_3=",setBuff)));
						#endif
						EEPROM_write(0x06,atoi(getSet((char *)ETHBUFFER, "gateway_3=",setBuff)));
						
						#if _DEBUG_MODE
				        printf_P(PSTR("\ngateway4 %d"),atoi(getSet((char *)ETHBUFFER, "gateway_4=",setBuff)));
						#endif
						EEPROM_write(0x07,atoi(getSet((char *)ETHBUFFER, "gateway_4=",setBuff)));
						
						#if _DEBUG_MODE
				        printf_P(PSTR("\nsubmask1 %d"),atoi(getSet((char *)ETHBUFFER, "submask_1=",setBuff)));
						#endif
						EEPROM_write(0x08,atoi(getSet((char *)ETHBUFFER, "submask_1=",setBuff)));
						
						#if _DEBUG_MODE
					    printf_P(PSTR("\nsubmask2 %d"),atoi(getSet((char *)ETHBUFFER, "submask_2=",setBuff)));
						#endif
						EEPROM_write(0x09,atoi(getSet((char *)ETHBUFFER, "submask_2=",setBuff)));
						
						#if _DEBUG_MODE
					    printf_P(PSTR("\nsubmask3 %d"),atoi(getSet((char *)ETHBUFFER, "submask_3=",setBuff)));
						#endif
						EEPROM_write(0x0A,atoi(getSet((char *)ETHBUFFER, "submask_3=",setBuff)));
						
						#if _DEBUG_MODE
					    printf_P(PSTR("\nsubmask4 %d"),atoi(getSet((char *)ETHBUFFER, "submask_4=",setBuff)));
						#endif
						EEPROM_write(0x0B,atoi(getSet((char *)ETHBUFFER, "submask_4=",setBuff)));
						
						
						#if _DEBUG_MODE
					    printf_P(PSTR("\nuser %s"),getSet((char *)ETHBUFFER, "user=",setBuff));
						#endif
						sprintf((char *)temp,"%s",getSet((char *)ETHBUFFER, "user=",setBuff));
						
						for (i = 0; i <= strlen(temp); i++)
						{
							
						EEPROM_write(0x0C+i,*(temp+i));	
							
						}
						
						
						#if _DEBUG_MODE
					    printf_P(PSTR("\npassword %s \n"),getSet((char *)ETHBUFFER, "password=",setBuff));
						#endif
						sprintf((char *)temp,"%s",getSet((char *)ETHBUFFER, "password=",setBuff));
						
						for (i = 0; i <= strlen(temp); i++)
						{
							
							EEPROM_write(0x15+i,*(temp+i));
							
						}
						
						free(setBuff);
						
						//assign login on variable
						sprintf((char *)login,"%s%s",get_user_addr(),get_password_addr());
						
						W5100_Init(get_ip_addr(),get_gtw_addr(),get_mask_addr());
						
						sei();
							
						}else{
						
						if (Eth_cmp_Str_index((char *)ETHBUFFER,"system1=ON") > 0){
							
							PORTC |= (1<<PORTC1);
							
						}else if(Eth_cmp_Str_index((char *)ETHBUFFER,"system1=OFF") > 0){
							
							PORTC &= ~(1<<PORTC1);
	
						}else if (Eth_cmp_Str_index((char *)ETHBUFFER,"system2=ON") > 0){
							
							PORTC |= (1<<PORTC2);
							
						}else if(Eth_cmp_Str_index((char *)ETHBUFFER,"system2=OFF") > 0){
							
							PORTC &= ~(1<<PORTC2);
							
						}						else if(Eth_cmp_Str_index((char *)ETHBUFFER,"system3=ON") > 0){
							
							PORTC |= (1<<PORTC3);
							
						}else if(Eth_cmp_Str_index((char *)ETHBUFFER,"system3=OFF") > 0){
							
							PORTC &= ~(1<<PORTC3);
							
						}
					}
					
					
					
					    strcpy_P((char *)ETHBUFFER,PSTR("HTTP/1.1 204 No Content \r\nConnection: keep-alive \r\n\r\n"));
					    Eth_Send(sockreg,ETHBUFFER,strlen((char *)ETHBUFFER));
						
						// Disconnect the socket
						Eth_disconnect(sockreg);
					    break;
						
						}
					
				//	#if _DEBUG_MODE
				//	printf("Req. Send!\n");
				//	printf("get radio=0: %d\n",Eth_cmp_Str_index((char *)ETHBUFFER,"radio=0"));
				//	printf("get radio=1: %d\n",Eth_cmp_Str_index((char *)ETHBUFFER,"radio=1"));
				//	#endif
					
					//HTTP RESPONSE SD FILES
					
					//Imediate Response
					strcpy_P((char *)FIRSTRESPONSEETHBUFFER,PSTR("HTTP/1.1 200 OK \r\nConnection: keep-alive, close \r\n\r\n"));
					Eth_Send(sockreg,FIRSTRESPONSEETHBUFFER,strlen((char *)FIRSTRESPONSEETHBUFFER));
					
					if (Eth_cmp_Str_index((char *)ETHBUFFER,"AVR.JPG") > 0){
						ETH_SEND_FILE("AVR     ", "JPG");
						}else if (Eth_cmp_Str_index((char *)ETHBUFFER,"BOOTSTRP.CSS") > 0){
						ETH_SEND_FILE("BOOTSTRP", "CSS");
						}else if (Eth_cmp_Str_index((char *)ETHBUFFER,"JQUERY.JS") > 0){
						ETH_SEND_FILE("JQUERY  ", "JS ");
						
						}else if (Eth_cmp_Str_index((char *)ETHBUFFER,"SYSTATUS.JSON") > 0 && keyid == 1){
						strcpy_P((char *)ETHBUFFER,PSTR("{\"systens\":["));
						if (PINC & (1<<PINC1)){strcat_P((char *)ETHBUFFER,PSTR("{\"system\":\"ON\"},"));
						}else{strcat_P((char *)ETHBUFFER,PSTR("{\"system\":\"OFF\"},"));
						}
						if (PINC & (1<<PINC2)){strcat_P((char *)ETHBUFFER,PSTR("{\"system\":\"ON\"},"));
						}else{strcat_P((char *)ETHBUFFER,PSTR("{\"system\":\"OFF\"},"));
						}
						if (PINC & (1<<PINC3)){strcat_P((char *)ETHBUFFER,PSTR("{\"system\":\"ON\"},"));
					    }else{strcat_P((char *)ETHBUFFER,PSTR("{\"system\":\"OFF\"},"));
						}	
						strcat_P((char *)ETHBUFFER,PSTR("{\"temperature\":\""));
						sprintf((char *)temp,"%d",tempvalue);
						strcat((char *)ETHBUFFER,temp);
						strcat_P((char *)ETHBUFFER,PSTR("\"}"));
					    strcat_P((char *)ETHBUFFER,PSTR("]}"));
						
						Eth_Send(sockreg,ETHBUFFER,strlen((char *)ETHBUFFER));
						
						
						}else if (Eth_cmp_Str_index((char *)ETHBUFFER,"SETTINGS.JSON") > 0 && keyid == 1){
						
						strcpy_P((char *)ETHBUFFER,PSTR("{\"settings\":["));
							
						uint8_t *ptr = get_ip_addr();	
							
						strcat_P((char *)ETHBUFFER,PSTR("{\"ip_1\":\""));
						sprintf((char *)temp,"%d",*(ptr+0));
						strcat((char *)ETHBUFFER,temp);	
					    strcat_P((char *)ETHBUFFER,PSTR("\","));
						   	
						strcat_P((char *)ETHBUFFER,PSTR("\"ip_2\":\""));
						sprintf((char *)temp,"%d",*(ptr+1));
						strcat((char *)ETHBUFFER,temp);
						strcat_P((char *)ETHBUFFER,PSTR("\","));
								
						strcat_P((char *)ETHBUFFER,PSTR("\"ip_3\":\""));
						sprintf((char *)temp,"%d",*(ptr+2));
						strcat((char *)ETHBUFFER,temp);
						strcat_P((char *)ETHBUFFER,PSTR("\","));
							
						strcat_P((char *)ETHBUFFER,PSTR("\"ip_4\":\""));
						sprintf((char *)temp,"%d",*(ptr+3));
						strcat((char *)ETHBUFFER,temp);
						strcat_P((char *)ETHBUFFER,PSTR("\","));	
						
	                    ptr = get_gtw_addr();
	  
	                    strcat_P((char *)ETHBUFFER,PSTR("\"gateway_1\":\""));
		                sprintf((char *)temp,"%d",*(ptr+0));
		                strcat((char *)ETHBUFFER,temp);
	                    strcat_P((char *)ETHBUFFER,PSTR("\","));
	                    
	                    strcat_P((char *)ETHBUFFER,PSTR("\"gateway_2\":\""));
		                sprintf((char *)temp,"%d",*(ptr+1));
		                strcat((char *)ETHBUFFER,temp);
	                    strcat_P((char *)ETHBUFFER,PSTR("\","));
	                    
	                    strcat_P((char *)ETHBUFFER,PSTR("\"gateway_3\":\""));
		                sprintf((char *)temp,"%d",*(ptr+2));
		                strcat((char *)ETHBUFFER,temp);
	                    strcat_P((char *)ETHBUFFER,PSTR("\","));
	                    
	                    strcat_P((char *)ETHBUFFER,PSTR("\"gateway_4\":\""));
		                sprintf((char *)temp,"%d",*(ptr+3));
		                strcat((char *)ETHBUFFER,temp);
	                    strcat_P((char *)ETHBUFFER,PSTR("\","));
	  
	                    ptr = get_mask_addr();
						
						
						strcat_P((char *)ETHBUFFER,PSTR("\"submask_1\":\""));
						sprintf((char *)temp,"%d",*(ptr+0));
						strcat((char *)ETHBUFFER,temp);
						strcat_P((char *)ETHBUFFER,PSTR("\","));
						
						strcat_P((char *)ETHBUFFER,PSTR("\"submask_2\":\""));
						sprintf((char *)temp,"%d",*(ptr+1));
						strcat((char *)ETHBUFFER,temp);
						strcat_P((char *)ETHBUFFER,PSTR("\","));
						
						strcat_P((char *)ETHBUFFER,PSTR("\"submask_3\":\""));
						sprintf((char *)temp,"%d",*(ptr+2));
						strcat((char *)ETHBUFFER,temp);
						strcat_P((char *)ETHBUFFER,PSTR("\","));
						
						strcat_P((char *)ETHBUFFER,PSTR("\"submask_4\":\""));
						sprintf((char *)temp,"%d",*(ptr+3));
						strcat((char *)ETHBUFFER,temp);
						strcat_P((char *)ETHBUFFER,PSTR("\","));
						
						strcat_P((char *)ETHBUFFER,PSTR("\"user\":\""));
						sprintf((char *)temp,"%s",get_user_addr());
						strcat((char *)ETHBUFFER,temp);
						strcat_P((char *)ETHBUFFER,PSTR("\","));
						
						strcat_P((char *)ETHBUFFER,PSTR("\"password\":\""));
						sprintf((char *)temp,"%s",get_password_addr());
						strcat((char *)ETHBUFFER,temp);
						strcat_P((char *)ETHBUFFER,PSTR("\""));
									
			            strcat_P((char *)ETHBUFFER,PSTR("}]}"));
			
			            Eth_Send(sockreg,ETHBUFFER,strlen((char *)ETHBUFFER));
					
						}else{
						 mainpageswitch(keyid);
						}
					
				}
				
				// Disconnect the socket
				Eth_disconnect(sockreg);
				
				break;
				
			} else{
				
		        //Wait response 
		        _delay_us(10); 
				break;
			}
			
			
		    //SOCK_LISTEN 0x14
			case SOCK_LISTEN:
			_delay_us(10);
			break;
			
			//SOCK_FIN_WAIT 0x18
			case SOCK_FIN_WAIT:
			#if _DEBUG_MODE
			printf_P(PSTR("SOCK_FIN_WAIT\n"));
			#endif
			Eth_Close(sockreg);
			_delay_us(10);
			break;
			
			//SOCK_CLOSING 0x1A
			case SOCK_CLOSING:
			#if _DEBUG_MODE
			printf_P(PSTR("SOCK_CLOSING\n"));
			#endif
			_delay_us(10);
			break;
			
			//SOCK_TIIME_WAIT 0x1B
			case SOCK_TIME_WAIT:
			#if _DEBUG_MODE
			printf_P(PSTR("SOCK_TIME_WAIT\n"));
			#endif
			Eth_Close(sockreg);
			_delay_us(10);
			break;
	
			//SOCK_CLOSE_WAIT 0x1C
			case SOCK_CLOSE_WAIT:
			#if _DEBUG_MODE
			printf_P(PSTR("SOCK_CLOSE_WAIT\n"));
			#endif
			Eth_Close(sockreg);
			_delay_us(10);
			break;
			
			//SOCK_LAST_ACK 0x1D
			case SOCK_LAST_ACK:
			#if _DEBUG_MODE
			printf_P(PSTR("SOCK_LAST_ACK\n"));
			// Disconnect the socket
			Eth_disconnect(sockreg);
			#endif
			_delay_us(10);
			break;
			
		}
	}
	return 0;
}

/* EOF: wiznetweb.c */