#============================================================================
# Object Oriented C, an object-oriented programming facility for C.
# Copyright (C) by Zack T Smith.
# This is covered by the Lesser GNU Public License version 3.
#============================================================================

CFLAGS= -g -Wno-conditional-type-mismatch
CC=gcc -Wall
SRC=main.c 
OBJ=main.o
LIB= 
OOC_SRC= ObjectOriented.c Object.c Image.c MutableImage.c Double.c Int.c Array.c MutableArray.c SimpleGraphing.c String.c MutableString.c Font.c FontBuiltin.c Console.c 
OOC_OBJ= ObjectOriented.o Object.o Image.o MutableImage.o Double.o Int.o Array.o MutableArray.o SimpleGraphing.o String.o MutableString.o Font.o FontBuiltin.o Console.o 

# Note: Mac 32-bit linking no longer supported on Mac OS as of 10.14.

OOC64.a: ${OOC_SRC}
	${CC} -m64 ${CFLAGS} -c ${OOC_SRC}
	ar rvs OOC64.a ${OOC_OBJ}

OOC32.a: ${OOC_SRC}
	${CC} -m32 ${CFLAGS} -c ${OOC_SRC}
	ar rvs OOC32.a ${OOC_OBJ}

clean:
	rm -f ${OOC_OBJ} OOC32.a OOC64.a

