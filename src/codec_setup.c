#include "stm32f4xx.h"
#include "codec_setup.h" 


/* double this size because we read from half of the array while writing to the other half */
int16_t i2sDataIn[I2S_DMA_BUF_SIZE * 2]; 
int16_t i2sDataOut[I2S_DMA_BUF_SIZE * 2];
int16_t *curI2sDataIn, *curI2sDataOut;

/* Set up the internal workings of the CODEC over SPI */
static void codec_setup_options(void)
{
    /* enable the chip by writing 1 to CE */
    GPIO_SetBits(GPIOE, GPIO_Pin_4);
    /* wait for port to be free */
    while (SPI_I2S_GetFlagStatus(SPI4, SPI_FLAG_TXE) == RESET);
    /* reset registers to defaults */
    SPI_I2S_SendData(SPI4, (0x27 << 9));
    while (SPI_I2S_GetFlagStatus(SPI4, SPI_FLAG_TXE) == RESET);
    /* latch the datum in by pulsing CE low */
    GPIO_ResetBits(GPIOE, GPIO_Pin_4);
    GPIO_SetBits(GPIOE, GPIO_Pin_4);
    GPIO_SetBits(GPIOE, GPIO_Pin_4);
    /* set 16 bit datum size for output (DAC)*/
    SPI_I2S_SendData(SPI4, (0x0a << 9));
    while (SPI_I2S_GetFlagStatus(SPI4, SPI_FLAG_TXE) == RESET);
    /* latch the datum in by pulsing CE low */
    GPIO_ResetBits(GPIOE, GPIO_Pin_4);
    GPIO_SetBits(GPIOE, GPIO_Pin_4);
    /* set 16 bit datum size for input (ADC)*/
    SPI_I2S_SendData(SPI4, (0x0b << 9));
    while (SPI_I2S_GetFlagStatus(SPI4, SPI_FLAG_TXE) == RESET);
    /* latch the datum in by pulsing CE low */
    GPIO_ResetBits(GPIOE, GPIO_Pin_4);
    GPIO_SetBits(GPIOE, GPIO_Pin_4);
}


/* delay to wait for codec to be fully powered on */
static void codec_init_delay(void)
{
    /* assuming 180MHz, wait 9000000 cycles for 0.5 secs */
    int i = 9000000;
    while (i--);
}

/* Setup SPI to communicate commands to codec */
static void codec_spi_setup(void)
{
    GPIO_InitTypeDef portEInitStruct;
    SPI_InitTypeDef  spi4InitStruct;
    /* Setup up SPI4 peripheral clock (for some reason you use SPI3) */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI4, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource2 | GPIO_PinSource6,
            GPIO_AF_SPI4);
    portEInitStruct.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_6;
    portEInitStruct.GPIO_Mode = GPIO_Mode_AF;
    portEInitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    portEInitStruct.GPIO_OType = GPIO_OType_PP;
    portEInitStruct.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOE, &portEInitStruct);
    /* init the "chip enable" pin */
    portEInitStruct.GPIO_Mode = GPIO_Mode_OUT;
    portEInitStruct.GPIO_Pin = GPIO_Pin_4;
    GPIO_Init(GPIOE, &portEInitStruct);
    /* Init SPI struct with defaults */
    SPI_StructInit(&spi4InitStruct);
    spi4InitStruct.SPI_Direction = SPI_Direction_Tx;
    spi4InitStruct.SPI_Mode = SPI_Mode_Master;
    spi4InitStruct.SPI_DataSize = SPI_DataSize_16b;
    /* The rest state of the clock signal is LOW */
    spi4InitStruct.SPI_CPOL = SPI_CPOL_Low; 
    /* clocks in datum on rising edge of clock (first edge) */
    spi4InitStruct.SPI_CPHA = SPI_CPHA_1Edge;
    spi4InitStruct.SPI_NSS = SPI_NSS_Soft; /* we control chip select ourselves */
    /* Assuming a clock speed of 180 MHz and a APB1 prescaler of 4, SPI clock is
     * then 180Mhz/4/16 = 5.625 MHz. According to WM8778 the max speed would be
     * 12.5 MHz */
    spi4InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;
    /* send the most significant bit first */
    spi4InitStruct.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_Init(SPI4,&spi4InitStruct);
    SPI_Cmd(SPI4, ENABLE);
}

/* Set up I2S to communicate audio data to codec. PLL should already be
 * configured to drive I2S clock (codec_i2s_clocks_setup()) */
static void codec_i2s_setup(void)
{
    GPIO_InitTypeDef portA_CInitStruct;
    I2S_InitTypeDef i2s3InitStruct;
    /* Enable I2S/SPI3 clock */
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_SPI3, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    portA_CInitStruct.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_10 | GPIO_Pin_7 | GPIO_Pin_11;
    portA_CInitStruct.GPIO_Mode = GPIO_Mode_AF;
    portA_CInitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    portA_CInitStruct.GPIO_OType = GPIO_OType_PP;
    portA_CInitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOC, &portA_CInitStruct);
    portA_CInitStruct.GPIO_Pin = GPIO_Pin_4;
    GPIO_Init(GPIOA, &portA_CInitStruct);

    GPIO_PinAFConfig(GPIOC, GPIO_PinSource12 | GPIO_PinSource10 | GPIO_PinSource7,
            GPIO_AF_SPI3);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_SPI3);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource4, GPIO_AF_SPI3);

    i2s3InitStruct.I2S_Mode = I2S_Mode_MasterTx;
    i2s3InitStruct.I2S_Standard = I2S_Standard_Phillips;
    i2s3InitStruct.I2S_DataFormat = I2S_DataFormat_16b; /* set up codec to accept this size! */
    i2s3InitStruct.I2S_MCLKOutput = I2S_MCLKOutput_Enable;
    i2s3InitStruct.I2S_AudioFreq = I2S_AudioFreq_44k;
    i2s3InitStruct.I2S_CPOL = I2S_CPOL_Low;

    I2S_Init(SPI3, &i2s3InitStruct);

    /* the extension block is doing rx (receiving), so change that part of struct */
//    i2s3InitStruct.I2S_Mode = I2S_Mode_MasterRx;
    I2S_FullDuplexConfig(I2S3ext, &i2s3InitStruct);
    I2S_Cmd(SPI3, ENABLE);
    I2S_Cmd(I2S3ext, ENABLE);
}

static void codec_i2s_clocks_setup(void)
{
    RCC_I2SCLKConfig(RCC_I2S2CLKSource_PLLI2S);
    RCC_PLLI2SCmd(ENABLE);
    /* wait until clock ready */
    while (RCC_GetFlagStatus(RCC_FLAG_PLLI2SRDY) == RESET);
}

static void codec_i2s_dma_setup(void)
{
    DMA_InitTypeDef DMA_I2STxInitStruct;
    DMA_InitTypeDef DMA_I2SRxInitStruct;
    /* Enable DMA clock */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
    /* Setup DMA for SPI_TX */
    DMA_ITConfig(DMA1_Stream5, DMA_IT_HT | DMA_IT_TC, ENABLE); 
    NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    DMA_DeInit(DMA1_Stream5);
    DMA_StructInit(&DMA_I2STxInitStruct);
    DMA_I2STxInitStruct.DMA_Channel = DMA_Channel_0;
    DMA_I2STxInitStruct.DMA_PeripheralBaseAddr = (uint32_t)(&(SPI3->DR));
    DMA_I2STxInitStruct.DMA_Memory0BaseAddr = (uint32_t)i2sDataOut;
    DMA_I2STxInitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    DMA_I2STxInitStruct.DMA_BufferSize = I2S_DMA_BUF_SIZE * 2;
    DMA_I2STxInitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_I2STxInitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_I2STxInitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_I2STxInitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_I2STxInitStruct.DMA_Mode = DMA_Mode_Circular;
    DMA_I2STxInitStruct.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_I2STxInitStruct.DMA_FIFOMode = DMA_FIFOMode_Disable;
    /* DMA_FIFOThreshold ... N/A */
    DMA_I2STxInitStruct.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_I2STxInitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA1_Stream5, &DMA_I2STxInitStruct);
    /* Enable DMA stream */
    DMA_Cmd(DMA1_Stream5, ENABLE);

    /* Setup DMA for I2S3_EXT_RX because the EXT (extension block) pin is
     * connected to ADCDOUT pin of codec */
    DMA_DeInit(DMA1_Stream0);
    /* Trigger on half complete and fully complete */
    DMA_ITConfig(DMA1_Stream0, DMA_IT_HT | DMA_IT_TC, ENABLE); 
    NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    DMA_StructInit(&DMA_I2SRxInitStruct);
    DMA_I2SRxInitStruct.DMA_Channel = DMA_Channel_3;
    DMA_I2SRxInitStruct.DMA_PeripheralBaseAddr = (uint32_t)(&(I2S3ext->DR));
    DMA_I2SRxInitStruct.DMA_Memory0BaseAddr = (uint32_t)i2sDataIn;
    DMA_I2SRxInitStruct.DMA_DIR = DMA_DIR_PeripheralToMemory;
    DMA_I2SRxInitStruct.DMA_BufferSize = I2S_DMA_BUF_SIZE * 2;
    DMA_I2SRxInitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_I2SRxInitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_I2SRxInitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_I2SRxInitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_I2SRxInitStruct.DMA_Mode = DMA_Mode_Circular;
    DMA_I2SRxInitStruct.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_I2SRxInitStruct.DMA_FIFOMode = DMA_FIFOMode_Disable;
    /* DMA_FIFOThreshold ... N/A */
    DMA_I2SRxInitStruct.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_I2SRxInitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA1_Stream0, &DMA_I2SRxInitStruct);
    /* Enable DMA stream */
    DMA_Cmd(DMA1_Stream0, ENABLE);

    SPI_I2S_DMACmd(SPI3, SPI_I2S_DMAReq_Tx, ENABLE);
    SPI_I2S_DMACmd(I2S3ext, SPI_I2S_DMAReq_Rx, ENABLE);

    /* wait for DMA to be ready */
    while (DMA_GetCmdStatus(DMA1_Stream5) != ENABLE);
    while (DMA_GetCmdStatus(DMA1_Stream0) != ENABLE);
}

/* DMA interrupt handler for I2S Rx */
void DMA1_Stream0_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_Stream0, DMA_IT_TCIF0)) {
        DMA_ClearITPendingBit(DMA1_Stream0, DMA_IT_TCIF0); 
        curI2sDataIn = i2sDataIn + I2S_DMA_BUF_SIZE;
    }
    if (DMA_GetITStatus(DMA1_Stream0, DMA_IT_HTIF0)) {
        DMA_ClearITPendingBit(DMA1_Stream0, DMA_IT_HTIF0); 
        curI2sDataIn = i2sDataIn;
    }
    NVIC_ClearPendingIRQ(DMA1_Stream0_IRQn);
}

/* DMA interrupt handler for I2S Tx */
void DMA1_Stream5_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_Stream5, DMA_IT_TCIF0)) {
        DMA_ClearITPendingBit(DMA1_Stream5, DMA_IT_TCIF0); 
        curI2sDataOut = i2sDataOut + I2S_DMA_BUF_SIZE;
    }
    if (DMA_GetITStatus(DMA1_Stream5, DMA_IT_HTIF0)) {
        DMA_ClearITPendingBit(DMA1_Stream5, DMA_IT_HTIF0); 
        curI2sDataOut = i2sDataOut;
    }
    NVIC_ClearPendingIRQ(DMA1_Stream5_IRQn);
}

void codec_setup(void)
{
    /* wait for codec to get powered up */
    codec_init_delay();
    /* initialize SPI to communicate commands */
    codec_spi_setup();
    /* set up codec options (like datum size, gain etc.) */
    codec_setup_options();
    /* setup I2S */
    codec_i2s_setup();
    /* setup I2S clocks */
    codec_i2s_clocks_setup();
    /* set up DMA for i2s communications */
    codec_i2s_dma_setup();
}
