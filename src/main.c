/* Includes ------------------------------------------------------------------*/
#include <stdlib.h> 
#include "stm32f4xx_conf.h" 
#include "timerdo.h" 

TIM_TimeBaseInitTypeDef TIM_TimerInitStruct;
GPIO_InitTypeDef        GPIO_LEDs_InitStruct;

void Delay(__IO uint32_t nCount);
void Timer_Setup(void);

int main(void)
{
    timerdo_setup();
    Timer_Setup();
  /*!< At this stage the microcontroller clock setting is already configured, 
       this is done through SystemInit() function which is called from startup
       file (startup_stm32f4xx.s) before to branch to application main.
       To reconfigure the default setting of SystemInit() function, refer to
        system_stm32f4xx.c file
     */
    
  while (1)
  {
  }
}

/**
  * @brief  Delay Function.
  * @param  nCount:specifies the Delay time length.
  * @retval None
  */
void Delay(__IO uint32_t nCount)
{
  while(nCount--)
  {
  }
}

void Timer_Setup(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimerInitStruct.TIM_Prescaler = 0;
    TIM_TimerInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimerInitStruct.TIM_Period = 42000; /* Tick once per millisecond */ 
    TIM_TimerInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;

    TIM_TimeBaseInit(TIM2, &TIM_TimerInitStruct);

    NVIC_EnableIRQ(TIM2_IRQn);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
  * @}
  */ 

/**
  * @}
  */ 
