#include<stdio.h>
#include<stdlib.h>
#include <stdint.h>   /* for uint32_t */


#include "bit_map.h"


int init_bitmap(int num){
    int size = (num / BIT_WIDTH_IN_BITS) + 1;
    int *tem = calloc(size,BIT_WIDTH_IN_BYTES);
    if(tem==NULL){
        printf("Init BitMap Failed;Malloc return Null\n");
        return -1;
    }
    BitMap = tem;
    printf("%x Bitmap address size: %d (int) %d(bytes)\n", BitMap, size,size<<2);
    return 0;
}

/*
 * bitCount - returns count of number of 1's in word
 *   Examples: bitCount(5) = 2, bitCount(7) = 3
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 40
 *   Rating: 4
 */
int bitCount(int x) {
  /* So So difficult
  This method is based on Divide and Conquer
  Is also known as haming weight "popcount" or "sideways addition"
  'variable-precision SWAR algorithm'
  References 1.https://stackoverflow.com/questions/3815165/how-to-implement-bitcount-using-only-bitwise-operators
             2.http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
             3.https://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer
  */
  int c=0;
  int v=x;
  c = (v & 0x55555555) + ((v >> 1) & 0x55555555);
  c = (c & 0x33333333) + ((c >> 2) & 0x33333333);
  c = (c & 0x0F0F0F0F) + ((c >> 4) & 0x0F0F0F0F);
  c = (c & 0x00FF00FF) + ((c >> 8) & 0x00FF00FF);
  c = (c & 0x0000FFFF) + ((c >> 16)& 0x0000FFFF);
  return c;
}


/*
 * ilog2 - return floor(log base 2 of x), where x > 0
 *   Example: ilog2(16) = 4
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 90
 *   Rating: 4
 */
int ilog2(int x) {
  //This program takes refrence to the Internet
	//First consider the First 16 bit of x,if x>0xffff then the last 16 bit is useless so we can do right shift
	//After the right shift,what is left is the original First 16 bits
	//t records the answer
	//use (!!x) as a representation of (x!=0)
	//use bit-or to do add operation
  //return 2;
  return 2;
}

void printb(int len){
  int i;
  int tem;
  printf("\nBitMap Information\n");
  for (i = 0; i < len; i++)
  {
    tem = BOOL_ONE(i);
    printf("%d", tem!=0);
    if((i+1)%4==0){
      printf(" ");
    }else if ((i+1)%8==0){
      printf("   ");
    }
    else if((i+1)%(BIT_WIDTH_IN_BITS)==0){
      printf("\n");
    }
  }
  printf("\n");
}

int main(){
    int size;
    scanf("%d", &size);
    printf("%d\n", size);
    init_bitmap(size);
    int code;
    while (1)
    {
      scanf("%d", &code);
      SET_BITMAP(code);
      printb(size);
      
    }
    if(BitMap){
        free(BitMap);
    }
}