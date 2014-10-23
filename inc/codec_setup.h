#ifndef CODEC_SETUP_H
#define CODEC_SETUP_H 

#include <stdint.h> 

#define I2S_DMA_BUF_SIZE 128 

extern int16_t *curI2sDataIn;
extern int16_t *curI2sDataOut;

void codec_setup(void);

#endif /* CODEC_SETUP_H */
