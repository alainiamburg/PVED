#!/usr/bin/python

# Bus Pirate script for SPI EEPROM read/write
# Tested on Bus Pirate v3.6
#
# Alain Iamburg
# 
# 
# References:
#  http://dangerousprototypes.com/docs/Bitbang
#  http://dangerousprototypes.com/docs/SPI_(binary)

import argparse, serial, struct, time, sys, os


	
def Bin_Enter(usb):
	entry_bytes = 20
	success = 0

	while (entry_bytes and not success):
		usb.write("\x00")
		entry_bytes = entry_bytes-1
		if "BBIO" in usb.read(10):
			success = 1
			
	return success
	
	
def Bin_Exit(usb):
	usb.write("\x00")    # Exit Binary Mode
	usb.write("\x0F")    # Reset
	
	
def SPI_Enter(usb):
	success = 0
	
	usb.write("\x01")
	if "SPI" in usb.read(10):
		success = 1
		
	return success


def SPI_Setup(usb):
	success = 1
	
	usb.write("\x49")           # 0100 1001 = Power ON, CS High
	if "\x00" in usb.read():
		success = 0
	usb.write("\x63")           # 0110 0011 = 1 MHz
	if "\x00" in usb.read():
		success = 0
	usb.write("\x8A")           # 1000 1010 = 3.3v, clock idle low, 
	if "\x00" in usb.read():    #             clock edge active-to-idle, sample middle
		success = 0
		
	return success


def EEPROM_Read(usb, addr, readbytes, outfile):
	success = 0
	
	# [COMMAND][NUMBER OF WRITE BYTES][NUMBER OF READ BYTES][BYTES TO WRITE]
	#  (1byte)        (2 bytes)			     (2 bytes)       (0-4096 bytes)
	usb.write("\x04")                         # Write-Then-Read cmd
	usb.write("\x00\x03")                     # Number of write bytes
	usb.write(struct.pack('>h', readbytes))   # Number of read bytes
	usb.write("\x03")                         # EEPROM Read command
	usb.write(struct.pack('>h', addr))        # Read from addr
	if "\x01" == usb.read():
		success = 1
	data = usb.read(readbytes)
	if args.x:
		dump(data, 16, addr)
		
	outfile.write(data)

	return success
	
	
def EEPROM_Write(usb, addr, writebytes, infile):
	success = 1
	
	# Bulk SPI Transfer command method
	usb.write("\x02")                       # 0000 0010 - Bring CS Low
	if "\x01" != usb.read():
		success = 0
	usb.write("\x10")                       # 0001 0000 Bulk SPI Transfer 1 byte
	if "\x01" != usb.read():   
		success = 0
	usb.write("\x06")                       # EEPROM Write Enable command
	usb.read()
	usb.write("\x03")                       # 0000 0011 - Bring CS High
	if "\x01" != usb.read():
		success = 0
	
	# Write-Then-Read command method (takes care of CS transitions)
	# [COMMAND][NUMBER OF WRITE BYTES][NUMBER OF READ BYTES][BYTES TO WRITE]
	#  (1byte)        (2 bytes)			     (2 bytes)       (0-4096 bytes)	
	usb.write("\x04")                           # Write-Then-Read cmd
	usb.write(struct.pack('>h', writebytes+3))  # Number of write bytes
	usb.write("\x00\x00")                       # Number of read bytes
	usb.write("\x02")						    # EEPROM Write command
	usb.write(struct.pack('>h', addr))		    # Write to addr

	for b in range(writebytes):                 # Write
		char = infile.read(1)
		usb.write(char)
	if "\x01" != usb.read():
		success = 0	
		
	return success


def bail(usb):
	try:
		usb.close()
		f.close()
		sys.exit(1)
	except:
		sys.exit(1)


# Thanks Stephen Chappell
# http://code.activestate.com/recipes/576945/
# modded by Alain Iamburg
def dump(data, length, addr):
	hex = lambda line: ' '.join('{:02x}'.format(b) for b in map(ord, line))
	str = lambda line: ''.join(31 < c < 127 and chr(c) or '.' for c in map(ord, line))
	
	for i in range(0, len(data), length):
		line = data[i:i+length]
		print('{:08x}   {:47}   {}'.format(addr+i, hex(line), str(line)))
	
	

# Program Start

parser = argparse.ArgumentParser(description="Read from or write to a SPI EEPROM using the Bus Pirate")
parser.add_argument("interface", help="Bus Pirate serial interface")
parser.add_argument("addr", type=int, help="EEPROM address from which to read/write")
parser.add_argument("mode", choices=['read', 'write'], help="read or write")
parser.add_argument("numbytes", type=int, help="number of bytes to read or write")
parser.add_argument("file", help="file from which to read or write")
parser.add_argument("-x", help="also dump to screen", action="store_true")
args = parser.parse_args()

if args.mode == "write" and args.numbytes > 64:
	print "TODO: Add support to write more than 64 bytes at a time.\r\n"
	sys.exit(1)
		
if not os.access(args.interface, os.W_OK):
	print "[-] Cannot access interface. Ensure correct interface and user permissions\r\n"
	sys.exit(1)

try:
	if args.mode == "read":
		f = open(args.file, 'wb')
	elif args.mode == "write":
		f = open(args.file, 'rb')
except:
	print "[-] Could not open {0}\r\n".format(args.file)
	sys.exit(1)

print "[*] Opening serial port..."
usb = serial.Serial(args.interface, 115200, timeout=.01)  # default 8-N-1
usb.write("#\r\n")
usb.read(8)
print usb.read(1024) + "\r\n"

print "[*] Entering Binary Mode...\r"
if not Bin_Enter(usb):
	print "[-] Could not enter Binary Mode\r\n"
	bail(usb)
	
print "[*] Entering SPI Mode...\r"
if not SPI_Enter(usb):
	print "[-] Could not enter SPI Mode\r\n"
	bail(usb)

print "[*] Setting up SPI...\r"
if not SPI_Setup(usb):
	print "[-] Could not set up SPI\r\n"
	bail(usb)

if args.mode == "read":
	print "[*] Reading {0} bytes from {1}...\r\n".format(args.numbytes, hex(args.addr))
	if not EEPROM_Read(usb, args.addr, args.numbytes, f):
		print "[-] Could not read from EEPROM\r\n"
		bail(usb)
	print "\r\n[+] Saved to {0}\r\n".format(f.name)
elif args.mode == "write":
	print "[*] Writing {0} bytes to {1}...\r\n".format(args.numbytes, hex(args.addr))
	if not EEPROM_Write(usb, args.addr, args.numbytes, f):
		print "[-] Could not write to EEPROM\r\n"
		bail(usb)
	print "\r\n[+] EEPROM written\r\n"
		

f.close()
Bin_Exit(usb)
usb.close()

