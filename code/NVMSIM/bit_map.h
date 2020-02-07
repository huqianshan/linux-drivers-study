#ifndef __BIT_MAP_H
#define __BIT_MAP_H


/**
 * A literal 1 is a regular, signed int, which raises two problems: 
 * it isn't guaranteed to be 32 bits, and 1 << 31 is undefined behavior 
 * (because of the sign bit). All literal 1s 
 * in the above code should be replaced with the
 *  unsigned 32-bit (word_t) 1.
 */
typedef uint32_t word_t;
int *BitMap;

/**
 *  Num is the total numbers of item. 
 */
int init_bitmap(int num);

/**
 * retur ncount of number of 1's in word
 */ 
int bitCount(int x);

/* The length of a "Bit",equal to int size*/

#define BIT_WIDTH_IN_BYTES (sizeof(int))
#define BIT_WIDTH_IN_BITS (BIT_WIDTH_IN_BYTES<<3)

/* the position in "Bit"*/
#define BIT_OFFSET(pos) (pos % (BIT_WIDTH_IN_BITS))

/* the position in "int" Bitmap */
#define INT_OFFSET(pos) (pos / (BIT_WIDTH_IN_BITS))

/* Set the pos in Bitmap to one */
#define SET_BITMAP(pos) \
    (BitMap[INT_OFFSET(pos)] |= ((word_t)1 << BIT_OFFSET(pos)))
/* Clear the pos in Bitmap to zero */
#define CLEAR_BIT_BITMAP(pos) (BitMap[INT_OFFSET(pos)] &= (~((word_t)1 << BIT_OFFSET(pos)))
/* find the pos postion if is equal to One*/
#define FIND_POS_EQUL_ONE_BITMAP(pos) (BitMap[INT_OFFSET(pos)] & ((word_t)1 << BIT_OFFSET(pos)))
#define BOOL_ONE(pos) (BitMap[INT_OFFSET(pos)]&((word_t)1<<BIT_OFFSET(pos)))
/**
 *  print first len bits of the bitmap 
 */
void printb(int len);

#endif