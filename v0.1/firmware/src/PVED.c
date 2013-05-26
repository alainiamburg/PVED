/*
* Copyright 2013 Alain Iamburg
*
* This file is part of Purposely Vulnerable Embedded Device (PVED)
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2, or (at your option)
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; see the file COPYING. If not, write to
* the Free Software Foundation, Inc., 51 Franklin Street,
* Boston, MA 02110-1301, USA.
*/


#include <avr/io.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "PVED.h"
#include "md5.h"


FILE iostream = FDEV_SETUP_STREAM(usart_putchar, usart_getchar, _FDEV_SETUP_RW);
const char *banner = "PVED v0.1\r\n\n";
const char *hostname = "PVED";
char usernameBuffer[STRING_BUFFER_SIZE] = { 0 };
char commandBuffer[STRING_BUFFER_SIZE] = { 0 };
BYTE serialBuffer[SERIAL_BUFFER_SIZE] = { 0 };
BYTE system_state = STATE_IDLE;
BYTE is_admin = 0;
BYTE *serialBufferTail;
const struct CRED creds[NUM_USERS] = 
{
	{"admin",    "615c114c332a69aa9b31d28d05b1be2c", 1},
	{"acidburn", "2ac9cb7dc02b3c0083eb70898e549b63", 0}
};




/*
**
** Main function loop
**
**
**
*/
int main(void)
{
	wdt_init();
	io_init();
	serial_init();
	eeprom_init();
	first_run();


	while (1) 
	{
		switch(system_state)
		{
			case STATE_IDLE:
				idle();
				break;

			case STATE_PREAUTH:
				login();
				break;
			
			case STATE_POSTAUTH:
				shell();
				break;

			case STATE_TAMPER:
				puts("!!! TILT !!!");
				while (1)
				{
					// system lock down!
					// release the hounds!
				}
		}
	}
}


/*
**
** First run EEPROM programming
**
**
**
*/
int first_run(void)
{
	BYTE ok[2] = { 'o', 'k' };
	BYTE buffer[2];


	eeprom_read(EEPROM_CHKFRUN, buffer, 2);
	if MEMCMP(buffer, ==, ok, sizeof(ok))
	{
		return 0;
	}
	else
	{
		puts("This is the first run! Burning EEPROM...\r\n");

		for (int i = 0; i < NUM_USERS; i++)
		{
			eeprom_write(EEPROM_CRED_BASE + (i*EEPROM_PAGE_SIZE), (BYTE*)&creds[i], EEPROM_CRED_SIZE);
			_delay_ms(EEPROM_DELAY);
		}

		eeprom_write(EEPROM_CHKFRUN, ok, 2);
		//_delay_ms(EEPROM_DELAY);

		return 1;
	}
}


/*
**
** Idle State
**
**
**
*/
void idle(void)
{
	printf("\r\n");

	serial_getLine(commandBuffer, ECHO_OFF);
	printf("%s\r\n", banner);
	
	system_state = STATE_PREAUTH;
}



/*
**
** Pre Auth State
**
**
**
*/
void login(void)
{
	char pass_attempt[STRING_BUFFER_SIZE] = { 0 };
	struct CRED cred_attempt, cred_actual;


	printf("Username: ");
	serial_getLine(cred_attempt.user, ECHO_ON);

	printf("Password: ");
	serial_getLine(pass_attempt, ECHO_OFF);


	md5_hash((BYTE*)pass_attempt, cred_attempt.hash, strlen(pass_attempt));

	for (int i = 0; i < NUM_USERS; i++)
	{
		eeprom_read(EEPROM_CRED_BASE + (EEPROM_PAGE_SIZE*i), (BYTE*)&cred_actual, sizeof(cred_actual));

		if (STRCMP(cred_attempt.user, ==, cred_actual.user) &&
		    STRCMP(cred_attempt.hash, ==, cred_actual.hash))
		{
			strcpy(usernameBuffer, cred_actual.user);
			is_admin = cred_actual.priv;
			system_state = STATE_POSTAUTH;
		}
	}

	printf("\r\n");
}



/*
**
** Post Auth State
**
**
**
*/
void shell(void)
{
	printf("\r[%s@%s]%s", usernameBuffer, hostname, is_admin ? "# " : "$ ");
	serial_getLine(commandBuffer, ECHO_ON);

	shell_command();
}



/*
**
** Execute Shell Command
**
**
**
*/
BYTE shell_command(void)
{
	char hash[STRING_HASH_SIZE] = { 0 };
	char arg[10];
	BYTE nulls[2] = { 0x00, 0x00 };
	BYTE retVal = 1;
	int i, j, bytes, addr;

	if (STRCMP(commandBuffer, ==, CMD_STATUS))
	{
		if (is_admin)
		{
			printf("PORTB: ");
			for (i = 7; i >= 0; i--)
			{
				(PORTB & 1<<i) ? printf("1") : printf("0");
			}
			printf("\r\n");

			printf("PORTC: ");
			for (i = 7; i >= 0; i--)
			{
				(PORTC & 1<<i) ? printf("1") : printf("0");
			}
			printf("\r\n");

			printf("PORTD: ");
			for (i = 7; i >= 0; i--)
			{
				(PORTD & 1<<i) ? printf("1") : printf("0");
			}
			printf("\r\n\r\n");
		}
		else
		{
			printf("\r\nAccess Denied.\r\n\r\n");
		}
	}
	else if (STRCMP(commandBuffer, ==, CMD_RESET))
	{
		wdt_enable(WDTO_15MS);  //reset in 15ms
		while (1) { }  //bye bye
	}
	else if (STRCMP(commandBuffer, ==, CMD_ERASE))
	{
		eeprom_erase();
	}
	else if (STRCMP(commandBuffer, ==, CMD_READBACK))
	{
		readback();
	}
	else if (strstr(commandBuffer, CMD_MD5))
	{
		sscanf(commandBuffer, CMD_MD5 "%s\r", arg);
		
		md5_hash((BYTE*)arg, hash, strlen(arg));
	
		printf("\r\nmd5(%s) = %s\r\n\r\n", arg, hash);
	}
	else if (strstr(commandBuffer, CMD_EEPROM))
	{
		BYTE eeprom_buffer[512];

		if (sscanf(commandBuffer, CMD_EEPROM "%d %d\r", &addr, &bytes) > 0) //If we got a valid arg
		{
			//eeprom_read_page(page, eeprom_buffer);
			if (bytes > 512)
				bytes = 512;

			eeprom_read(addr, eeprom_buffer, bytes);

			for (i=0; i < bytes; i+=16)
			{
				if (i % 16 == 0)
				{						 
					//printf("%04x   ", i+(page*EEPROM_PAGE_SIZE));
					printf("%04x   ", addr+i);
				}

				for (j=0; j < 16; j++)
					printf("%02x ", eeprom_buffer[i+j]);
	
				printf("  ");

				for (j=i; j<i+16; j++)
				{
					if (isprint(eeprom_buffer[j]))
						printf("%c", eeprom_buffer[j]);
					else
						printf(".");
				}

				printf("\r\n");
			}
		}
		else
		{
			printf("eeprom [addr] [bytes]\r\n\r\n");
		}
	}
	else if (STRCMP(commandBuffer, ==, CMD_REVERT))
	{
		eeprom_write(EEPROM_CHKFRUN, nulls, sizeof(nulls));
		wdt_enable(WDTO_15MS);  //reset in 15ms
		while (1) { }	//bye bye
	}
	else if (STRCMP(commandBuffer, ==, CMD_LOGOUT))
	{
		is_admin = 0;
		system_state = STATE_PREAUTH;

		printf("\r\n");
	}
	else if (STRCMP(commandBuffer, ==, CMD_HELP))
	{
		printf("\r\nThe following commands are accepted:\r\n\r\n");
		printf("help\r\nreset\r\nrevert\r\nstatus\r\nlogout\r\nreadback\r\nerase\r\neeprom\r\nmd5\r\n");
		printf("\r\n");
	}
	else
	{
		retVal = 0;
	}


	return retVal;
}



void readback(void)
{
	char c = 0;

	while (c != 'q')
	{
		printf("0x%x\r\n", c = getchar());
	}
}


/*
**
** Get line from USB
** 
**
**
*/
void serial_getLine(char *buffer, BYTE echo)
{
	char newChar = '\0';
	memset(buffer, '\0', sizeof(SERIAL_BUFFER_SIZE));

	while (newChar != '\r')    // Handle input until end of line
	{
		newChar = getchar();


		if (newChar == '\b')	// Backspace, remove last char from console
		{
			if ((serialBufferTail - serialBuffer) > 0)
			{
				*serialBufferTail-- = '\0';
				printf("\b \b");
			}
		}
		/*
		else if ((newChar >= 0x41) && (newChar <= 0x44))
		{
			//TODO arrows, but also blocks A thru D...
		}
		*/
		else
		{
			if ((serialBufferTail - serialBuffer) == SERIAL_BUFFER_SIZE)
				serialBufferTail = serialBuffer;

			if (newChar == '\r')
			{
				printf("\r\n");
				*serialBufferTail = '\0';
			}
			else
			{	
				*serialBufferTail = newChar;

				if (echo)
					putchar(newChar);
			}
		
			serialBufferTail++;
		}
	}

	memcpy(buffer, serialBuffer, (serialBufferTail-serialBuffer));
	serialBufferTail = serialBuffer;
}


/*
**
** Perform MD5 hash
** 
**
**
*/
void md5_hash(BYTE *input, char *hash, BYTE len)
{
	BYTE result[16], i;
	MD5_CTX ctx;
	

	MD5_Init(&ctx);
	MD5_Update(&ctx, input, len);
	MD5_Final(result, &ctx);

	for (i = 0; i < sizeof(result); i++)
	{
		sprintf(hash + i*2, "%02x", result[i]);
	}
}


/*
**
** Initialize Watchdog Timer for reset
** 
**
**
*/
void wdt_init(void)
{
	/* Clear watchdog reset flag */
	MCUSR &= ~(1<<WDRF);

	WDTCSR |= (1<<WDCE) | (1<<WDE); //do I need this
	WDTCSR = 0x00;
}



/*
**
** Initialize Data Direction Registers for I/O
** 
**
**
*/
void io_init(void)
{
	/* Set output pins as such */
	DDRB = (1 << SPI_SS) | (1 << SPI_CLK) | (1 << SPI_MOSI);
	DDRC = 1;
}


/*
**
** Initialize SPI Bus
** 
**
**
*/
void spi_init()
{
	/* Bring SS High */
	PORTB |= (1<<SPI_SS);

	/* Set Master Mode, Fosc/4 speed, Enable SPI */
	SPCR = (1<<SPE) | (1<<MSTR);
	//SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPI2X) | (1<<SPR0);
}


/*
**
** Initialize USART
** 
**
**
*/
void usart_init(void)
{
	/* Set baud rate */
	UBRR0H = (BYTE)((UBRR)>>8);
	UBRR0L = (BYTE)(UBRR);

	/* Set framing to 8-N-1 */
	//UCSR0C = (3<<UCSZ00);
	UCSR0C = (1<<UCSZ00) | (1<<UCSZ01);

	/* Enable Rx/Tx */
	UCSR0B = (1<<RXEN0) | (1<<TXEN0);
}


/*
**
** Initialize serial communications
** 
**
**
*/
void serial_init(void)
{
	usart_init();

	/* Allow usage of string.h functions for usart */
	stdin =  &iostream;
	stdout = &iostream;

	/* Reset serial buffer */
	serialBufferTail = serialBuffer;
}


/*
**
** Initialize EEPROM
** 
**
**
*/
void eeprom_init(void)
{
	spi_init();
	
	PORTC |= (1<<EEPROM_WP) | (1<<EEPROM_HOLD);
}


/*
**
** Read from EEPROM
** 
**
**
*/
void eeprom_read(WORD address, BYTE *buffer, unsigned int len)
{		
	PORTB &= ~(1<<SPI_SS);		// Bring down SS (active low)

	spi_write(EEPROM_READ);			// Send Read command
	//_delay_us(1);
	//_delay_ms(EEPROM_DELAY);
	spi_write((BYTE)(address>>8));	// Address to read (high byte)
	//_delay_us(1);
	//_delay_ms(EEPROM_DELAY);
	spi_write((BYTE)address);		// Address to read (low byte)

	if (DEBUG)
		printf("\r\nReading %d bytes from address %x\r\n\n", len, address);

	for (int i = 0; i < len; i++)
	{
		*buffer++ = spi_write(0xFF);	// We read a byte for each FF sent
	}

	// We are finished with you, slave
	PORTB |= (1<<SPI_SS);
}



void eeprom_read_page(BYTE page, BYTE *buffer)
{
	eeprom_read(page*EEPROM_PAGE_SIZE, buffer, EEPROM_PAGE_SIZE);
}



void eeprom_erase(void)
{
	BYTE nullpage[EEPROM_PAGE_SIZE];
	int i;

	memset(&nullpage, 0xFF, sizeof(nullpage));
	
	for (i=0; i < 10; i++)
	{
		eeprom_write(i*EEPROM_PAGE_SIZE, nullpage, sizeof(nullpage));
		_delay_ms(EEPROM_DELAY);
	}
}


/*
**
** Write to EEPROM
** 
**
**
*/
void eeprom_write(WORD address, BYTE *buffer, unsigned int len)
{
	eeprom_write_enable();	// Enable Writing
	
	PORTB &= ~(1<<SPI_SS);		// Bring down SS (active low)

	spi_write(EEPROM_WRITE);		// Send Write command
	_delay_ms(EEPROM_DELAY);
	spi_write((BYTE)(address>>8));	// Address to write (high byte)
	_delay_ms(EEPROM_DELAY);
  	spi_write((BYTE)address);		// Address to write (low byte)
  
  	if (DEBUG)
		printf("Writing %d bytes to address %x\r\n\n", len, address);

  	for (int i = 0; i < len; i++)
	{
	    spi_write(*(buffer + i));	// Write, one byte at a time
	}

	// We are finished with you, slave
	PORTB |= (1<<SPI_SS);
}


/*
**
** Enable EEPROM writing
** 
**
**
*/
void eeprom_write_enable(void)
{
	PORTB &= ~(1<<SPI_SS);

	spi_write(EEPROM_WREN);		// Send Write Enable comand

	PORTB |= (1<<SPI_SS);
}


/*
**
** Write a byte to the SPI bus
** 
**
**
*/
BYTE spi_write(BYTE data)
{
	SPDR = data;
	while (!(SPSR & (1<<SPIF)))
	{
		//wait
	}

	return SPDR;
}


/*
**
** Write a byte to the USART
** 
**
**
*/
void usart_putchar(char c, FILE *stream)
{
	while ((STATE_TAMPER != system_state) && (!(UCSR0A & (1<<UDRE0))))
	{
		//wait until ready
	}

	UDR0 = c;
}


/*
**
** Get a char from the USART
** 
**
**
*/
char usart_getchar(FILE *stream)
{
	while((STATE_TAMPER != system_state) && (!(UCSR0A & (1<<RXC0))))
	{
		//wait until ready
	}

	return UDR0;
}
