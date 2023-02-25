#Author: Geoffrey Jensen
#ECEA 5305 Assignment 2
#Date: 1/24/2023

CC=gcc
CROSS_CC=aarch64-none-linux-gnu-
SRC=aesdsocket.c
OBJ=aesdsocket
CROSS_COMPILE=none

default: $(OBJ)
ifeq ($(CROSS_COMPILE), $(CROSS_CC))
	$(CROSS_CC)$(CC) -o $(OBJ) $(SRC) 
else
	$(CC) -o $(OBJ) $(SRC)
endif

clean:
	rm -f $(OBJ)
	rm -f *.o *.elf
