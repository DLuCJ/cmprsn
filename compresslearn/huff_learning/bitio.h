#ifndef BITIO_H
#define BITIO_H

#include <stdint.h>
#include <string.h>
#include <assert.h>

/* TODO: Error handling */

/*
  Bitio functions. 
  TODO: Basic docu, usage explanation
*/

typedef struct 
{
  size_t bit_buf;
  int bit_pos;

  uint8_t *start;
  uint8_t *ptr;
  uint8_t *end;
} BIO_Data; 

/******************************************* 
   bitio common init / deinit 
********************************************/
static inline int BIO_Init(BIO_Data *data, void *dst, size_t dst_size);
static inline int BIO_WriteCloseStatus(BIO_Data *data, size_t eob, size_t nbits);
static inline unsigned int BIO_ReadCloseStatus(BIO_Data *data);



/*******************************************
 bitio encode API 
********************************************/
static inline void BIO_WriteBits(BIO_Data *data, size_t val, size_t nbits);
static inline void BIO_FlushBits(BIO_Data *data);



/*******************************************
bitio decode API 
********************************************/
static inline void BIO_PeekBits(BIO_Data *data, size_t nbits);
static inline void BIO_ConsumeBits(BIO_Data *data, size_t nbits);
static inline size_t BIO_ReadBits(BIO_Data *data, size_t nbits);

/* Return codes for BIO_ReloadDataBuf */
typedef enum 
  { 
    BIO_Dec_Incomplete = 0,
    BIO_Dec_EndOfBuf = 1,
    BIO_Dec_Complete = 2 
  } BIO_Dec_Status;

static inline BIO_Dec_Status BIO_ReloadDataBuf(BIO_Data *data);



/********************************************************************************************/
static const size_t REGBITS = (sizeof(size_t) * 8);
static const unsigned BIO_mask[] = { 0, 1, 3, 7, 0xF, 0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF,
				     0x3FFF, 0x7FFF, 0xFFFF, 0x1FFFF, 0x3FFFF, 0x7FFFF, 0xFFFFF, 0x1FFFFF, 0x3FFFFF,
				     0x7FFFFF,  0xFFFFFF, 0x1FFFFFF, 0x3FFFFFF };   /* up to 26 bits */

/* TODO: Add decode option, so can prefill bit_buf on init? Right now have to first call a reload */
static inline int BIO_Init(BIO_Data *data, void *dst, size_t dst_size)
{
  data->bit_buf = 0;
  data->bit_pos = REGBITS;
  
  data->start = (uint8_t *) dst;
  data->ptr = data->start;
  data->end = data->start + dst_size - sizeof(data->ptr);

  if(dst_size < sizeof(data->ptr))
    return -1;

  return 0;
}

/*  
    Writes bits left-justified into the bit_buf.
    No overflow-checking is done. 
*/
static inline void BIO_WriteBits(BIO_Data *data, size_t val, size_t nbits)
{
  data->bit_pos -= nbits;
  data->bit_buf |= ((val & BIO_mask[nbits]) << data->bit_pos);
}

/*
  Flush bytes out to memory.
  Overflow-checking is done.
*/
static inline void BIO_FlushBits(BIO_Data *data)
{
  size_t nbits = REGBITS - bit_pos;
  size_t nbytes =  nbits >> 3;
  
  memmove(data->ptr, &data->bit_buf, nbytes);
  data->ptr += nbytes;
  
  if(data->ptr > data->end)
    data->ptr = data->end;
  
  /* Reset to leftover bits */
  
  data->bit_pos = REGBITS - (nbits % 8);
  data->bit_buf <<= (nbytes * 8);
}

/*
  Writes out end of buffer value to data buf
  Returns number of bits read / wrote.
  Returns 0 if bounds exceeded.
*/
static inline int BIO_WriteCloseStatus(BIO_Data *data, size_t eob, size_t nbits)
{ 
  BIO_WriteBits(data, eob, nbits);
  BIO_FlushBits(data);

  if(data->ptr >= data->end) 
    return 0;

  return (data->ptr - data->start) + (data->bit_pos > 0);
}

/*
  Returns true if all bits have been read from memory.
*/
static inline unsigned int BIO_ReadCloseStatus(BIO_Data *data)
{
  return ((data->ptr == data->end) && ((REGBITS - data->bit_pos) == sizeof(data->bit_buf) * 8));
}

static inline void BIO_PeekBits(BIO_Data *data, size_t nbits)
{
  return (data->bit_buf >> (REGBITS - (data->bit_pos + nbits))) & BIO_mask[nbits]; 
  // return (bit_buf << (REGBITS- data->bit_pos)) >> (REGBITS - nbits);
}

static inline void BIO_ConsumeBits(BIO_Data *data, size_t nbits)
{
  data->bit_pos -= nbits;
}

/*
  Read nbits number of bits from bit_buf.
*/
static inline size_t BIO_ReadBits(BIO_Data *data, size_t nbits)
{
  size_t const val = BIO_PeekBits(data, nbits);
  BIO_ConsumeBits(data, nbits);
  return val;
}

/*
  Refills bit_buf from data buffer.  
  Will not read past end of data buffer.
  Returns status code as specified.
*/
static inline BIO_Dec_Status BIO_ReloadDataBuf(BIO_Data *data)
{  
  assert(data->bit_pos >= 0);
  
  size_t nbits = REGBITS - bit_pos;
  size_t nbytes = nbits >> 3;

  if (data->ptr <= data->end - sizeof(data->bit_buf)) {
    data->ptr += nbytes;		    
    data->bit_pos = REGBITS - (nbits % 8);
    
    size_t val;
    memmove(&val, data->ptr, sizeof(val));
    data->bit_buf = val;
    return BIO_Dec_Incomplete;
  }

  if (data->ptr == data->end) {
    if (data->bit_pos < REGBITS)
      return BIO_Dec_EndOfBuf;
    return BIO_Dec_Complete;
  }
    
  /* Handle last REGBITS */

  BIO_Dec_status stat = BIO_Dec_Incomplete;

  if (data->ptr + nbytes > data->end) {
    nbytes = data->end - data->ptr;
    stat = BIO_Dec_EndOfBuf;
  }

  data->ptr += nbytes;		    
  data->bit_pos += (nbytes * 8);
  
  size_t val;
  memmove(&val, data->ptr, sizeof(val));
  data->bit_buf = val;
  
  return stat;
} 

/*
  write bit str: 10110011100011110000...repeat 6 times
  end val: 0xFF 
*/
void BIO_Validate()
{
  BIO_Data bd;
  
  size_t td_size = 256;
  uint8_t test_data[td_size];
  memset(test_data, 0, td_size);
  
  BIO_Init(&bd, test_data, td_size);
  
  for (int i = 0; i < 6; ++i) {    
    BIO_WriteBits(&bd, 2, 2);
    BIO_WriteBits(&bd, 12, 4);
    
    if ((i % 2) == 0)
      BIO_WriteBits(&bd, 56, 6);
    else {
      BIO_WriteBits(&bd, 7, 3);
      BIO_WriteBits(&bd, 0, 3);
    }
    
    BIO_WriteBits(&bd, 240, 8);
    BIO_FlushBits(&bd);
  }
  
  size_t wcs = BIO_WriteCloseStatus(&bd, 255, 8);
  assert(wcs > 0);
  
  BIO_data bdr;
  BIO_Init(&bdr, bd->start, wcs);
  
  for (int i = 0; i < 6; ++i) {    
    assert(BIO_ReadBits(&bdr, 2) == 2);
    assert(BIO_ReadBits(&bdr, 4) == 12);
    
    if ((i % 2) == 0)
      assert(BIO_ReadBits(&bdr, 6) == 56);
    else {
      assert(BIO_ReadBits(&bdr, 3) == 7);
      assert(BIO_ReadBits(&bdr, 3) == 0);
    }
    
    assert(BIO_ReadBits(&bdr, 8) == 240);
    BIO_ReloadDataBuf(&bdr);
  }
  
  assert(BIO_ReadBits(&bdr, 8) == 255);
  assert(BIO_ReadCloseStatus(&bdr));
  
  /* TODO: step through, maybe add in a few more reloads/flushes afterwards  */
  /* TODO: exercise end of buffer functionality */
  /* TODO: make sure reload returns expected values */     
}

#endif //BITIO_H