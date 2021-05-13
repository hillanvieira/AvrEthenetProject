/*
 * uart.h
 *
 * Created: 28/10/2016 02:19:33
 *  Author: HILLAN
 */ 
#define F_CPU 16000000UL
#define BAUD_RATE 57600

#ifndef UART_H_
#define UART_H_

//Settings

#ifndef F_CPU
/* prevent compiler error by supplying a default */
# warning "F_CPU not defined for <uart/uart.h>"
# define F_CPU 16000000UL
#endif

#ifndef BAUD_RATE
#define BAUD_RATE 19200
#endif

//Functions

extern void uart_init(void);
extern void uart_flush(void);
extern int  uart_putch(char ch,FILE *stream);
extern int  uart_getch(FILE *stream);
extern void ansi_cl(void);
extern void ansi_me(void);

#endif /* UART_H_ */