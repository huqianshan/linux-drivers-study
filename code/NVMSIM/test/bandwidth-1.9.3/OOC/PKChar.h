#ifndef _PK_CHAR
#define _PK_CHAR

typedef struct {
	short encoding;
	short height;
	short width;
	short xoffset;
	short yoffset;
	const char* pattern;
} PKChar;

#endif
