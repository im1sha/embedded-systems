#include "main.h"
#include <stdbool.h>
#include <stdio.h> 
#include <string.h> 


void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);

//
// htim1
//

TIM_HandleTypeDef htim1;
const uint32_t HTIM1_PRESCALER = 3999;
const uint32_t HTIM1_PERIOD = 4;

// 
// display
//

const uint8_t DISPLAY_DIGITS[10] = 
{
	0x3F, /* 0 */
	0x06, /* 1 */
	0x5B, /* 2 */
	0x4F, /* 3 */
	0x66, /* 4 */
	0x6D, /* 5 */
	0x7D, /* 6 */
	0x07, /* 7 */
	0x7F, /* 8 */
	0x6F  /* 9 */
}; //display_table
const uint8_t DISPLAY_TOTAL_DIGITS = 4;
const uint8_t DISPLAY_TOTAL_DIGIT_PINS = DISPLAY_TOTAL_DIGITS;
const uint8_t DISPLAY_TOTAL_SEGMENT_PINS = 7; //pinsCount
const uint16_t DISPLAY_SEGMENT_PINS[DISPLAY_TOTAL_SEGMENT_PINS] =
{	
	GPIO_PIN_6,
	GPIO_PIN_5,
	GPIO_PIN_4,
	GPIO_PIN_3,
	GPIO_PIN_2,
	GPIO_PIN_1,
	GPIO_PIN_0
}; //seg_pins
const uint16_t DISPLAY_DIGIT_PINS[DISPLAY_TOTAL_DIGIT_PINS] =
{
	GPIO_PIN_8,
	GPIO_PIN_9,
	GPIO_PIN_10,
	GPIO_PIN_11
}; //digit_pins

//
// lock
//

uint8_t lockCurrentColumn; //current_col

const uint8_t LOCK_TOTAL_ROWS = 4; //row_count
const uint8_t LOCK_TOTAL_COLUMNS = 3; //col_count
const uint16_t LOCK_COLUMN_PINS[LOCK_TOTAL_COLUMNS] = 
{
	GPIO_PIN_5,
	GPIO_PIN_6,
	GPIO_PIN_7 
}; //col_pins
const uint16_t LOCK_ROW_PINS[LOCK_TOTAL_ROWS] =
{ 
	GPIO_PIN_0,
	GPIO_PIN_1,
	GPIO_PIN_2,
	GPIO_PIN_3 
};//row_pins

//
// passwords
//

const uint8_t PASSWORD_LENGTH = 4;
const uint8_t SERVICE_PASSWORD[PASSWORD_LENGTH] = { 5, 5, 9, 2 }; //servicePassword
uint8_t publicPassword[PASSWORD_LENGTH] = { 1, 4, 7, 6 };

int8_t currentInput[PASSWORD_LENGTH] = { -1,-1,-1,-1 }; //currEnter
uint8_t currentInputLength = 0; //currLength

bool isAsteriskPressed = false;
bool isHashPressed = false;

bool isNewPublicPasswordInput = false; //isNewPublicPassEnter

//
// leds
//

const uint32_t LED_DELAY = 400;//led_delay


void displayDigit(int32_t value, int32_t position)
{
	for (uint8_t i = 0; i < DISPLAY_TOTAL_DIGIT_PINS; i++)
	{
		HAL_GPIO_WritePin(GPIOA, DISPLAY_DIGIT_PINS[i], GPIO_PIN_SET);
	}

	for (uint8_t i = 0; i < DISPLAY_TOTAL_SEGMENT_PINS; i++)
	{
		GPIO_PinState currentPin = (DISPLAY_DIGITS[value] >> i) & 1 
			? GPIO_PIN_SET 
			: GPIO_PIN_RESET;
		HAL_GPIO_WritePin(GPIOA, DISPLAY_SEGMENT_PINS[i], currentPin);
	}

	HAL_GPIO_WritePin(GPIOA, DISPLAY_DIGIT_PINS[position], GPIO_PIN_RESET);
}


/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM1_Init();

	for (uint32_t i = 0; i < DISPLAY_TOTAL_DIGITS; i++)
	{
		HAL_GPIO_WritePin(GPIOA, DISPLAY_DIGIT_PINS[i], GPIO_PIN_SET);
	}

	while (1) 
	{
		for (uint32_t i = 0; i < 10; i++)
		{
			for (uint32_t j = 0; j < 4; j++)
			{
				displayDigit(i, j);
				HAL_Delay(500);
				HAL_GPIO_WritePin(GPIOA, DISPLAY_DIGIT_PINS[i], GPIO_PIN_SET);
				HAL_Delay(100);
			}
		}
	}
}

/**
	* @brief This function handles TIM1 update interrupt.
	*/
void TIM1_UP_IRQHandler(void)
{
	HAL_TIM_IRQHandler(&htim1);
}


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the CPU, AHB and APB busses clocks */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};


  htim1.Instance = TIM1;
	htim1.Init.Prescaler = HTIM1_PRESCALER;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim1.Init.Period = HTIM1_PERIOD;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3 
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_8 
                          |GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_13 
                          |GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA0 PA1 PA2 PA3 
                           PA4 PA5 PA6 PA8 
                           PA9 PA10 PA11 PA13 
                           PA14 PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3 
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_8 
                          |GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_13 
                          |GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB2 PB3 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB5 PB6 PB7 */
  GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI2_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI3_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{ 
}
#endif /* USE_FULL_ASSERT */

