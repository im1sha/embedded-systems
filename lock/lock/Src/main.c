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

TIM_HandleTypeDef _htim1;
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
};
const uint8_t DISPLAY_TOTAL_DIGITS = 4;
const uint8_t DISPLAY_TOTAL_SEGMENT_PINS = 7;
const uint8_t DISPLAY_TOTAL_DIGIT_PINS = DISPLAY_TOTAL_DIGITS;

const uint16_t DISPLAY_SEGMENT_PINS[DISPLAY_TOTAL_SEGMENT_PINS] = { GPIO_PIN_6, GPIO_PIN_5, GPIO_PIN_4, GPIO_PIN_3, GPIO_PIN_2, GPIO_PIN_1, GPIO_PIN_0 };
const uint16_t DISPLAY_DIGIT_PINS[DISPLAY_TOTAL_SEGMENT_PINS] = { GPIO_PIN_8, GPIO_PIN_9, GPIO_PIN_10, GPIO_PIN_11 };

//
// lock
//

uint8_t _lockCurrentColumn;

const uint8_t LOCK_TOTAL_COLUMNS = 3;
const uint8_t LOCK_TOTAL_ROWS = 4;

const uint16_t LOCK_COLUMN_PINS[LOCK_TOTAL_COLUMNS] = { GPIO_PIN_5, GPIO_PIN_6, GPIO_PIN_7 };
const uint16_t LOCK_ROW_PINS[LOCK_TOTAL_ROWS] = { GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3 };

//
// leds
//

const uint32_t LED_DEFAULT_DELAY = 400;

const uint16_t LED_RED = GPIO_PIN_13;
const uint16_t LED_YELLOW = GPIO_PIN_14;
const uint16_t LED_GREEN = GPIO_PIN_15;

//
// input
//
const uint8_t NO_INPUT = 0xFFU;

const uint8_t MAX_NUMBER = 9;
const uint8_t STAR_CHAR = MAX_NUMBER + 1;
const uint8_t HASH_CHAR = MAX_NUMBER + 2;
const uint8_t CODE_SHIFT = 4;

const uint8_t PASSWORD_LENGTH = 4;
const uint8_t SERVICE_PASSWORD[PASSWORD_LENGTH] = { 4, 4, 9, 2 };

uint8_t _publicPassword[PASSWORD_LENGTH] = { 1, 8, 5, 2 };

uint8_t _currentInput[PASSWORD_LENGTH] = 
{
  NO_INPUT,
  NO_INPUT,
  NO_INPUT,
  NO_INPUT 
};
uint8_t _currentInputLength = 0;

bool _isNewPublicPasswordInput = false;
bool _isHashPressed = false;
bool _isAsteriskPressed= false;


const uint32_t DEFAULT_IT_DEBOUNCE = 100;
uint32_t _itDebounce = DEFAULT_IT_DEBOUNCE;

uint32_t _lastDigitTick = 0;

uint8_t _currentDigitToShow = 0;

bool CompareArrays(const uint8_t a[], const uint8_t b[], int32_t length)
{
  for(int32_t i = 0; i < length; i++)
  {
    if (a[i] != b[i])
    {
      return false;
    }
  }
  return true;
}

uint8_t ConvertCodeToDigitChar(uint8_t code)
{
  switch(code)
  {
    case 0x00:
      return 1;
    case 0x01:
      return 4;
    case 0x02:
      return 7;
    case 0x03:
      return STAR_CHAR;

    case 0x10:
      return 2;
    case 0x11:
      return 5;
    case 0x12:
      return 8;
    case 0x13:
      return 0;

    case 0x20:
      return 3;
    case 0x21:
      return 6;
    case 0x22:
      return 9;
    case 0x23:
      return HASH_CHAR;

    default:
      return 0xFF;    
  } 
}

//
// display logic
//

// Outputs passed value at exact display position
void DisplayDigit(int32_t value, int32_t position)
{
  for (int32_t i = 0; i < DISPLAY_TOTAL_DIGIT_PINS; i++)
  {
    HAL_GPIO_WritePin(GPIOA, DISPLAY_DIGIT_PINS[i], GPIO_PIN_SET);
  }

  for (int32_t i = 0; i < DISPLAY_TOTAL_SEGMENT_PINS; i++)
  {
    GPIO_PinState currentPin = (DISPLAY_DIGITS[value] >> i) & 1 
      ? GPIO_PIN_SET 
      : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(GPIOA, DISPLAY_SEGMENT_PINS[i], currentPin);        
  }
  
  HAL_GPIO_WritePin(GPIOA, DISPLAY_DIGIT_PINS[position], GPIO_PIN_RESET);
}

//
// lock logic
//

// Bypasses all lock columns: sets and resets signal on each column
void LoopLockColumns(uint8_t *lockCurrentColumn)
{
  for (uint32_t i = 0; i < LOCK_TOTAL_COLUMNS; i++)
  {
    *lockCurrentColumn = i;
    HAL_GPIO_WritePin(GPIOB, LOCK_COLUMN_PINS[i], GPIO_PIN_SET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(GPIOB, LOCK_COLUMN_PINS[i], GPIO_PIN_RESET);
  }
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_TIM1_Init();
  HAL_TIM_Base_Start_IT(&_htim1);

  for (int32_t i = 0; i < DISPLAY_TOTAL_DIGITS; i++) 
  {
    HAL_GPIO_WritePin(GPIOA, DISPLAY_DIGIT_PINS[i], GPIO_PIN_SET);
  }

  while (true)
  {
    LoopLockColumns(&_lockCurrentColumn);
  }
}

// Clears display, input, resets timer
void ReturnToInitialState(uint8_t currentInput[], uint8_t * currentInputLength, TIM_HandleTypeDef htim1)
{
  HAL_TIM_Base_Stop_IT(&htim1);

  *currentInputLength = 0;
  for (int32_t i = 0; i < PASSWORD_LENGTH; i++)
  {
    currentInput[i] = NO_INPUT;
  }  
 
  for (int32_t i = 0; i < DISPLAY_TOTAL_DIGITS; i++)
  {
    HAL_GPIO_WritePin(GPIOA, DISPLAY_DIGIT_PINS[i], GPIO_PIN_SET);
  }
  for (int32_t i = 0; i < DISPLAY_TOTAL_SEGMENT_PINS; i++)
  {
    HAL_GPIO_WritePin(GPIOA, DISPLAY_SEGMENT_PINS[i], GPIO_PIN_RESET);
  }

  HAL_TIM_Base_Start_IT(&htim1);
}
 
// Turns on led for <time> milliseconds
void SignalLed(uint16_t pin, uint32_t time)
{
  HAL_GPIO_WritePin(GPIOA, pin, GPIO_PIN_SET);
  HAL_Delay(time);
  HAL_GPIO_WritePin(GPIOA, pin, GPIO_PIN_RESET);
}
 
// Can affect isNewPublicPasswordInput
// Returns new state of hash 
bool HandleHashPressed(
  bool * isNewPublicPasswordInput, 
  uint8_t currentInput[], 
  uint8_t * currentInputLength,
  TIM_HandleTypeDef htim1)
{
  if (*isNewPublicPasswordInput)
  {
    SignalLed(LED_RED, LED_DEFAULT_DELAY);
    *isNewPublicPasswordInput = false;
  }
  else if(*currentInputLength > 0)
  {
    ReturnToInitialState(currentInput, currentInputLength, htim1);
    SignalLed(LED_YELLOW, LED_DEFAULT_DELAY);
  }
  return false;
}

// Returns new state of star 
bool HandleAsteriskPressed(
  bool *isNewPublicPasswordInput,
  uint8_t currentInput[], 
  uint8_t * currentInputLength,
  TIM_HandleTypeDef htim1)
{
  bool result = true;
  if (*isNewPublicPasswordInput)
  {
    *isNewPublicPasswordInput = false;

    ReturnToInitialState(currentInput, currentInputLength, htim1);

    result = false;

    HAL_GPIO_WritePin(GPIOA, LED_YELLOW, GPIO_PIN_RESET); 
    HAL_GPIO_WritePin(GPIOA, LED_GREEN, GPIO_PIN_RESET); 
    HAL_GPIO_WritePin(GPIOA, LED_RED, GPIO_PIN_RESET);    
  } 
  return result;
}

void HandlePasswordInput(
  uint8_t currentInput[],  
  uint8_t * currentInputLength,
  bool * isAsteriskPressed, 
  bool * isNewPublicPasswordInput, 
  uint8_t publicPassword[],
  TIM_HandleTypeDef htim1)
{ 
  if(*isAsteriskPressed)
  {
    if (CompareArrays(currentInput, publicPassword, PASSWORD_LENGTH))
    {   
      SignalLed(LED_GREEN, LED_DEFAULT_DELAY);
    } 
    else if (CompareArrays(currentInput, SERVICE_PASSWORD, PASSWORD_LENGTH))
    {
      *isNewPublicPasswordInput = true;

      HAL_GPIO_WritePin(GPIOA, LED_RED, GPIO_PIN_SET); 
      HAL_GPIO_WritePin(GPIOA, LED_YELLOW, GPIO_PIN_SET); 
      HAL_GPIO_WritePin(GPIOA, LED_GREEN, GPIO_PIN_SET); 
    }
    else
    {     
      SignalLed(LED_RED, LED_DEFAULT_DELAY);
    }
    *isAsteriskPressed = false;
  }
  else if (*isNewPublicPasswordInput)
  {
    HAL_GPIO_WritePin(GPIOA, LED_GREEN, GPIO_PIN_RESET); 
    HAL_GPIO_WritePin(GPIOA, LED_YELLOW, GPIO_PIN_RESET); 
    HAL_GPIO_WritePin(GPIOA, LED_RED, GPIO_PIN_RESET);  

    for(int32_t i = 0; i < PASSWORD_LENGTH; i++)
    {
      publicPassword[i] = currentInput[i];
    }   

    *isNewPublicPasswordInput = false;
    SignalLed(LED_GREEN, LED_DEFAULT_DELAY);
  }
  else 
  {
    SignalLed(LED_RED, LED_DEFAULT_DELAY);
  }      

  ReturnToInitialState(currentInput, currentInputLength, htim1);
}

void HandleHalGpioExti(
  uint16_t gpioPin,
  uint32_t * lastDigitTick,
  uint32_t * itDebounce,
  uint8_t lockCurrentColumn,
  uint8_t currentInput[],
  uint8_t *currentInputLength,
  bool *isAsteriskPressed,
  bool * isHashPressed,
  uint8_t publicPassword[],
  bool * isNewPublicPasswordInput,
  TIM_HandleTypeDef htim1)
{
  uint32_t currentTick = HAL_GetTick();

  if (currentTick - *lastDigitTick > *itDebounce)
  {
    *lastDigitTick = currentTick;
    *itDebounce = DEFAULT_IT_DEBOUNCE;

    uint8_t currentCode = lockCurrentColumn << CODE_SHIFT;

    for (uint32_t i = 0; i < LOCK_TOTAL_ROWS; i++)
    {
      if (gpioPin == LOCK_ROW_PINS[i])
      {
        currentCode |= i;
        break;
      }
    }

    int8_t newDigit = ConvertCodeToDigitChar(currentCode);

    if (newDigit <= MAX_NUMBER)
    {
      currentInput[*currentInputLength] = newDigit;
      (*currentInputLength)++;
    }
    else
    {
      if (newDigit == STAR_CHAR)
      {
        if (*currentInputLength == 0)
        {
          *isAsteriskPressed = true;
        }
        *isAsteriskPressed = HandleAsteriskPressed(isNewPublicPasswordInput, currentInput, currentInputLength, htim1);
      }
      else if (newDigit == HASH_CHAR)
      {
        *isHashPressed = true;
        *isHashPressed = HandleHashPressed(isNewPublicPasswordInput, currentInput, currentInputLength, htim1);
      }
    }
    if (*currentInputLength >= PASSWORD_LENGTH)
    {
      HandlePasswordInput(currentInput,
        currentInputLength,
        isAsteriskPressed,
        isNewPublicPasswordInput,
        publicPassword,
        htim1);
    }
  }
}

void HandleTim1Up(
  uint8_t currentInputLength,
  uint8_t currentInput[], 
  uint8_t * currentDigitToShow)
{
  if (currentInputLength != 0)
  {
    if (currentInput[*currentDigitToShow] != NO_INPUT)
    {
      DisplayDigit(currentInput[*currentDigitToShow], *currentDigitToShow);
    }
    *currentDigitToShow = (*currentDigitToShow + 1) % currentInputLength;
  }
}

void TIM1_UP_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&_htim1);  
  HandleTim1Up(_currentInputLength, _currentInput, &_currentDigitToShow);
}

void HAL_GPIO_EXTI_Callback(uint16_t gpioPin)  
{
  HandleHalGpioExti(gpioPin,
    &_lastDigitTick,
    &_itDebounce,
    _lockCurrentColumn,
    _currentInput,
    &_currentInputLength,
    &_isAsteriskPressed,
    &_isHashPressed, 
    _publicPassword,
    &_isNewPublicPasswordInput,
    _htim1);
}


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
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
  _htim1.Instance = TIM1;
  _htim1.Init.Prescaler = HTIM1_PRESCALER;
  _htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  _htim1.Init.Period = HTIM1_PERIOD;
  _htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  _htim1.Init.RepetitionCounter = 0;
  _htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&_htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&_htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&_htim1, &sMasterConfig) != HAL_OK)
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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3 
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_8 
                          |GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_13 
                          |GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

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
  HAL_NVIC_SetPriority(EXTI0_IRQn, 1, 1);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 1, 1);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI2_IRQn, 1, 1);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI3_IRQn, 1, 1);
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

