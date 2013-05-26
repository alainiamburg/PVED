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


#define F_CPU               8000000  //internal osc, remember to clear the CKDIV8 fuse bit in the uC ;)
#define BAUD         		9600
#define UBRR          		F_CPU/16/BAUD-1
#define SERIAL_BUFFER_SIZE  16  //TODO
#define STRING_BUFFER_SIZE  SERIAL_BUFFER_SIZE+1
#define STRING_HASH_SIZE    MD5_HASH_SIZE+1
#define MD5_HASH_SIZE		32  //md5 hash is 16 bytes, but 32 when stored as ascii hex chars
#define DEBUG  0
#define NUM_USERS  2

#define ECHO_OFF  0
#define ECHO_ON   1

#define STATE_IDLE      0
#define STATE_PREAUTH   1
#define STATE_POSTAUTH  2
#define STATE_TAMPER    3

//TODO make pretty and add interesting functions...
#define CMD_HELP         "help"
#define CMD_RESET		 "reset"
#define CMD_REVERT		 "revert"
#define CMD_STATUS		 "status"
#define CMD_LOGOUT		 "logout"
#define CMD_READBACK	 "readback"
#define CMD_ERASE		 "erase"
#define CMD_EEPROM		 "eeprom" //args
#define CMD_MD5			 "md5 "   //args

#define SPI_SS    PORTB2
#define SPI_MOSI  PORTB3
#define SPI_MISO  PORTB4 
#define SPI_CLK   PORTB5

#define EEPROM_WRSR     1
#define EEPROM_WRITE    2
#define EEPROM_READ     3
#define EEPROM_WRDI     4
#define EEPROM_RDSR     5
#define EEPROM_WREN     6
#define EEPROM_HOLD     PORTC4
#define EEPROM_WP       PORTC5
#define EEPROM_CHKFRUN    0x240
#define EEPROM_CRED_BASE  0x80
#define EEPROM_CRED_SIZE  sizeof(creds)/NUM_USERS
#define EEPROM_STATUS_WPEN (1<<7)
#define EEPROM_STATUS_RDY  (1<<0)
#define EEPROM_PAGE_SIZE 64
#define EEPROM_DELAY     10    //TODO use STATUS_RDY instead of delay

#define STRCMP(a, R, b)    (strcmp(a, b) R 0)
#define MEMCMP(a, R, b, c) (memcmp(a, b, c) R 0)


typedef unsigned char BYTE;
typedef unsigned int  WORD;
typedef unsigned long DWORD;

struct CRED {
	char user[STRING_BUFFER_SIZE];
	char hash[STRING_HASH_SIZE];
	BYTE priv;
};


int first_run(void);

void shell(void);
BYTE shell_command(void);
void login(void);

void md5_test(void);
void md5_hash(BYTE *input, char *hash, BYTE len);

void usart_init(void);
void usart_putchar(char c, FILE *stream);
char usart_getchar(FILE *stream);

void serial_init(void);
void serial_getLine(char *buffer, BYTE echo);

void io_init(void);

void idle(void);

void eeprom_init(void);
void eeprom_read(WORD address, BYTE *buffer, unsigned int len);
void eeprom_read_hilo(BYTE hiaddr, BYTE loaddr, BYTE *buffer, unsigned int len);
void eeprom_read_page(BYTE page, BYTE *buffer);
void eeprom_write(WORD address, BYTE *buffer, unsigned int len);
void eeprom_write_enable(void);
void eeprom_erase(void);

void readback(void);

void spi_init(void);
BYTE spi_write(BYTE data);

void wdt_init(void);
