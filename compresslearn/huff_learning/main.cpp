#include "platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "bitio.h"

#define DEBUG

static void panic(const char *fmt, ...)
{
  va_list arg;
  
  va_start(arg, fmt);
  fputs("Error: ", stderr);
  vfprintf(stderr, fmt, arg);
  va_end(arg);
  fputs("\n", stderr);
  
  exit(1);
}

static uint8_t* read_file(char const* filename, size_t* out_size)
{
  FILE* f = fopen(filename, "rb");
  if (!f)
    panic("file not found: %s\n", filename);
  
  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  fseek(f, 0, SEEK_SET);
  
  uint8_t* buf = new uint8_t[size];
  if (fread(buf, size, 1, f) != 1)
    panic("read failed\n");
  
  fclose(f);
  if (out_size)
    *out_size = size;
  
  return buf;
}

struct SymbolStats
{
  uint32_t freqs[256];
  uint32_t normfreqs[256];
  void count_freqs(uint8_t const* in, size_t nbytes);
  void normalize_freqs(uint8_t target_total);
};

void SymbolStats::count_freqs(uint8_t const* in, size_t nbytes)
{
  for (int i=0; i < 256; i++)
    freqs[i] = 0;

  for (size_t i=0; i < nbytes; i++)
    freqs[in[i]]++;
}

void SymbolStats::normalize_freqs(uint8_t max_val)
{    
  for (int i=0; i < 256; i++)
    normfreqs[i] = 0;

  uint32_t max_freq = 0;
  int max_idx = 0;
  for (int i = 0; i < 256; i++) {
    if (freqs[i] > max_freq) {
      max_freq = freqs[i];
      max_idx = i;
    }
  }

  //scale counts to range [0..max_val]
  if (max_freq > max_val) {
    for (int i = 0; i < 256; i++)
      normfreqs[i] = ((uint64_t)max_val * freqs[i])/max_freq;
  }

  //guarantee that a count > 0 won't be scaled to 0
  for (int i=0; i < 256; i++) {
    if (freqs[i] && !normfreqs[i]) {
      // symbol i was set to zero freq

      // find best symbol to steal frequency from (try to steal from low-freq ones)
      uint32_t best_freq = ~0u;
      int best_steal = -1;
      for (int j=0; j < 256; j++) {
	uint32_t freq = normfreqs[j];
	if (freq > 1 && freq < best_freq) {
	  best_freq = freq;
	  best_steal = j;
	}
      }
      assert(best_steal != -1);

      normfreqs[best_steal]--;
      normfreqs[i]++;
    }
  }
  
  for (int i = 0; i < 256; i++) {
    if (freqs[i]) assert(normfreqs[i]);
  }

  assert(normfreqs[max_idx] == max_val); 
}

int main(void)
{

#ifdef DEBUG
  BIO_Validate();
#endif

  size_t src_size;
  uint8_t *src_buf = read_file("../../book1", &src_size);
  
  const uint8_t scalefreq = 255;

  SymbolStats stats;
  stats.count_freqs(src_buf, src_size);
  stats.normalize_freqs(scalefreq);
  
  uint8_t* enc_buf = new uint8_t[src_size + src_size / 4];
  uint8_t* dec_buf = new uint8_t[src_size];

  memset(dec_buf, 0xcc, src_size);

  BIO_Data bd_enc;
  BIO_Init(&bd_enc, (void *)enc_buf, src_size + src_size / 4, ENCODE); 

  printf("huff_learn encode:\n");
  for (int run=0; run < 5; run++) {
    double start_time = timer();
    uint64_t enc_start_time = __rdtsc();
    
    //TODO:
   
    uint64_t enc_clocks = __rdtsc() - enc_start_time;
    double enc_time = timer() - start_time;
    printf("%"PRIu64" clocks, %.1f clocks/symbol (%5.1fMiB/s)\n", enc_clocks, 1.0 * enc_clocks / src_size, 1.0 * src_size / (enc_time * 1048576.0));
  }

  printf("MTHUFF: %d bytes\n", (int)(bd_enc->ptr - bd_enc->start) + 1);
  
  printf("\nhuff_learn decode:\n");
  for (int run=0; run < 5; run++) {
    double start_time = timer();
    uint64_t dec_start_time = __rdtsc();
    
    //TODO:
    
    uint64_t dec_clocks = __rdtsc() - dec_start_time;
    double dec_time = timer() - start_time;
    printf("%"PRIu64" clocks, %.1f clocks/symbol (%5.1fMiB/s)\n", dec_clocks, 1.0 * dec_clocks / src_size, 1.0 * src_size / (dec_time * 1048576.0));
  }
  
  // check decode results
  if (memcmp(src_buf, dec_buf, src_size) == 0)
    printf("\ndecode ok!\n");
  else
    printf("\nERROR: bad decoder!\n");
  
  delete[] enc_buf;
  delete[] dec_buf;
  delete[] src_buf;
  
  return 0;
}
 
