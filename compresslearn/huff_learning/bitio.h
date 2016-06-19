#ifndef BITIO_H
#define BITIO_H

#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "platform.h"

/* TODO: Error handling */

#if defined (__cplusplus)
extern "C" {
#endif

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
#define DECODE 1
#define ENCODE 0
  static inline int BIO_Init(BIO_Data *data, void *dst, size_t dst_size, unsigned is_decode);

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
  static inline size_t BIO_PeekBits(BIO_Data *data, size_t nbits);
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

  static inline unsigned BIO_Is32bits(void) { return sizeof(size_t)==4; }
  static inline unsigned BIO_Is64bits(void) { return sizeof(size_t)==8; }

  static inline size_t BIO_MemSwap(size_t val)
  {
    if (BIO_Is32bits()) {
#if defined(_MSC_VER)     /* Visual Studio */
      return _byteswap_ulong((unsigned long)val);
#elif defined (__GNUC__)
      return __builtin_bswap32((unsigned long)val);
#else
      return  ((val << 24) & 0xff000000) |
	((val << 8) & 0x00ff0000) |
	((val >> 8) & 0x0000ff00) |
	((val >> 24) & 0x000000ff);
#endif
    }
    else {
#if defined(_MSC_VER)     /* Visual Studio */
      return _byteswap_uint64((uint64_t)val);
#elif defined (__GNUC__)
      return __builtin_bswap64((uint64_t)val);
#else
      return  ((val << 56) & 0xff00000000000000ULL) |
	((val << 40) & 0x00ff000000000000ULL) |
	((val << 24) & 0x0000ff0000000000ULL) |
	((val << 8) & 0x000000ff00000000ULL) |
	((val >> 8) & 0x00000000ff000000ULL) |
	((val >> 24) & 0x0000000000ff0000ULL) |
	((val >> 40) & 0x000000000000ff00ULL) |
	((val >> 56) & 0x00000000000000ffULL);
#endif
    }
  }

  static inline int BIO_Init(BIO_Data *data, void *dst, size_t dst_size, unsigned is_decode)
  {
    data->bit_buf = 0;
    data->bit_pos = REGBITS;
  
    data->start = (uint8_t *) dst;
    data->ptr = data->start;
    data->end = data->start + dst_size - sizeof(data->ptr);

    if (is_decode) {
      size_t val;
      memmove(&val, data->ptr, sizeof(val));
      data->bit_buf = BIO_MemSwap(val);
    }

    if(dst_size <= sizeof(data->ptr))
      return -1;

    return 0;
  }

  /*  
      Writes bits left-justified into the bit_buf.
      Flushes out to mem if necessary.
  */
  static inline void BIO_WriteBits(BIO_Data *data, size_t val, size_t nbits)
  {
    if (data->bit_pos < nbits)
      BIO_FlushBits(data);
    data->bit_pos -= (int)nbits;
    data->bit_buf |= ((val & BIO_mask[nbits]) << data->bit_pos);
  }

  /*
    Flush bytes out to memory.
    Overflow-checking is done.
  */
  static inline void BIO_FlushBits(BIO_Data *data)
  {
    size_t nbits = REGBITS - data->bit_pos;
    size_t nbytes =  nbits >> 3;
  
    size_t val = BIO_MemSwap(data->bit_buf);
    memcpy(data->ptr, &val, sizeof(val));
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

  static inline size_t BIO_PeekBits(BIO_Data *data, size_t nbits)
  {
    return (data->bit_buf >> (data->bit_pos - nbits)) & BIO_mask[nbits]; 
    // return (bit_buf << (REGBITS- data->bit_pos)) >> (REGBITS - nbits);
  }

  static inline void BIO_ConsumeBits(BIO_Data *data, size_t nbits)
  {
    data->bit_pos -= (int)nbits;
  }

  /*
    Read nbits number of bits from bit_buf.
  */
  static inline size_t BIO_ReadBits(BIO_Data *data, size_t nbits)
  {
    if (data->bit_pos < nbits)
      BIO_ReloadDataBuf(data);
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
  
    size_t nbits = REGBITS - data->bit_pos;
    size_t nbytes = nbits >> 3;

    if (data->ptr <= data->end - sizeof(data->bit_buf)) {
      data->ptr += nbytes;		    
      data->bit_pos = REGBITS - (nbits % 8);
    
      size_t val;
      memmove(&val, data->ptr, sizeof(val));
      data->bit_buf = BIO_MemSwap(val);
      return BIO_Dec_Incomplete;
    }

    if (data->ptr == data->end) {
      if (data->bit_pos < REGBITS)
	return BIO_Dec_EndOfBuf;
      return BIO_Dec_Complete;
    }
    
    /* Handle last REGBITS */

    BIO_Dec_Status stat = BIO_Dec_Incomplete;

    if (data->ptr + nbytes > data->end) {
      nbytes = data->end - data->ptr;
      stat = BIO_Dec_EndOfBuf;
    }

    data->ptr += nbytes;		    
    data->bit_pos += (int)(nbytes * 8);
  
    size_t val;
    memmove(&val, data->ptr, sizeof(val));
    data->bit_buf = BIO_MemSwap(val);
  
    return stat;
  } 

  /*
    write bit str: 10110011100011110000...repeat 6 times
    end val: 0xFF 
  */
  void BIO_Validate()
  {
    BIO_Data bd;
  
    size_t const td_size = 256;
    uint8_t test_data[td_size];
    memset(test_data, 0, td_size);
  
    BIO_Init(&bd, test_data, td_size, ENCODE);
  
    for (int i = 0; i < 6; ++i) {    
      BIO_WriteBits(&bd, 2, 2);
      BIO_WriteBits(&bd, 12, 4);
      BIO_WriteBits(&bd, 56, 6);
      BIO_WriteBits(&bd, 240, 8);
      BIO_FlushBits(&bd);
    }
  
    size_t wcs = BIO_WriteCloseStatus(&bd, 255, 8);
    assert(wcs > 0);
  
    BIO_Data bdr;
    BIO_Init(&bdr, bd.start, wcs, DECODE);
  
    for (int i = 0; i < 6; ++i) {    
      //size_t val1 = BIO_ReadBits(&bdr, 2);
      assert(BIO_ReadBits(&bdr, 2) == 2);
      //size_t val2 = BIO_ReadBits(&bdr, 4);
      assert(BIO_ReadBits(&bdr, 4) == 12);
      //size_t val3 = BIO_ReadBits(&bdr, 6);
      assert(BIO_ReadBits(&bdr, 6) == 56);
      //size_t val4 = BIO_ReadBits(&bdr, 8);
      assert(BIO_ReadBits(&bdr, 8) == 240);
      BIO_ReloadDataBuf(&bdr);
    }
  
    assert(BIO_ReadBits(&bdr, 8) == 255);
    //assert(BIO_ReadCloseStatus(&bdr));

    /* TODO: step through, maybe add in a few more reloads/flushes afterwards  */
    /* TODO: exercise differently sized read/writes */
    /* TODO: exercise end of buffer functionality */
    /* TODO: make sure reload returns expected values */     
    /* TODO: flush maybe wrong when not on byte boundary */
    /* TODO: rethink ReadCloseStatus.  not what i want to be checking. */
  }


#if defined (__cplusplus)
}
#endif

#endif //BITIO_H
