#include "main.h"
#include "can.h"
#include "dma.h"
#include "tim.h"
#include "gpio.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <time.h>
#include <stdio.h>
// DEAD CODE (old, no longer built): lighting_demos.hpp pulled in the old LightingController demo harness;
// unused here and no longer part of the active build (see .cproject).
// #include <lighting_demos.hpp>
#include <string.h>
#include "can_manager.hpp"
#include "pinecan_handlers.h"
// NEW CHANGE: new lighting pipeline entry points (led_init/Select_Pattern/Generate_Leds/Push_Leds)
#include "new_lighting_controller.hpp"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// #define ROTATE_LED
// #define CYCLE_ONE_LED_ON
// #define CONSTANT_COLOR
//  DEAD CODE (old, no longer built): superseded by new_lighting_controller.hpp (included above). The old
//  LightingController class/board object is no longer part of the active
//  build -- Lighting/Src/lighting_controller.cpp is excluded in .cproject.
//  #include "lighting_controller.hpp"
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern TIM_HandleTypeDef htim6;
static uint32_t node_id;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief  Return a unique ID made out of the 96-bit STM32 UID
 * @param  id an array of size 16 to fill with the unique ID
 * @retval None
 */
void getUniqueID(uint8_t id[16])
{
	uint32_t HALUniqueIDs[4];
	// Make Unique ID out of the 96-bit STM32 UID
	memset(id, 0, 16);
	HALUniqueIDs[0] = HAL_GetUIDw0();
	HALUniqueIDs[1] = HAL_GetUIDw1();
	HALUniqueIDs[2] = HAL_GetUIDw2();
	HALUniqueIDs[3] = HAL_GetUIDw1(); // repeating UIDw1 for this, no specific reason I chose this..
	memcpy(id, HALUniqueIDs, 16);
}

void initializeNodeId()
{
	uint8_t buffer[16];
	getUniqueID(buffer);
	uint32_t *parts = (uint32_t *)buffer;
	node_id = parts[0] ^ parts[1] ^ parts[2];
}

// DEAD CODE (old, no longer built): superseded by new_lighting_controller.cpp: GROUND's CD_BEACON breathing is
// now driven by new_pattern_table.cpp's ANIM_BREATHE row + update_animations()
// (reproduces this exact ramp -- 0..50, +-1 per ~20ms). The old `board`
// object below is no longer part of the active build (see .cproject), so
// this can no longer link -- kept as comments for reference only.
//
// extern LightingController board;
//
// void groundStateBreathe(uint8_t state) {
// 	if (state == TRANSITION_GROUND) {
// 		static uint8_t brightness = 0;
// 		static uint8_t brightness_direction = 1;
// 		uint8_t brightness_max = 50;
//
// 		if (brightness <= 0) {
// 			brightness = 0;
// 			brightness_direction = 1;
// 		} else if (brightness >= brightness_max) {
// 			brightness = brightness_max;
// 			brightness_direction = -1;
// 		}
// 		board.set_domain_brightness(CD_BEACON, brightness);
// 		board.activate_domain(CD_BEACON);
// 		brightness += brightness_direction;
// 	}
// }

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_CAN1_Init();
	MX_TIM1_Init();
	MX_TIM6_Init();
	MX_TIM7_Init();
	MX_TIM2_Init();
	// here

	/* USER CODE BEGIN 2 */

	HAL_TIM_Base_Start_IT(&htim6);
	HAL_TIM_Base_Start_IT(&htim2);

	// DEAD CODE (old, no longer built): superseded by led_init() below -- board/domain setup, colours, and
	// brightness now live in Lighting/Src/new_pattern_table.cpp (the single
	// file to edit for colour/brightness/animation changes). The old
	// `board` object is no longer part of the active build (see .cproject).
	//
	// board.start_lighting_control(); //start lighting
	// uint8_t all_domains_enabled = (1 << 7);
	// board.configure_allowed_domains(all_domains_enabled);
	//
	// //set up the domain colours and brightness
	// board.set_domain_colour_and_brightness(CD_MAIN, PURPLE, 5);
	// board.set_domain_colour_and_brightness(CD_TAXI, WHITE, 99);
	// board.set_domain_colour_and_brightness(CD_LANDING, WHITE, 99);
	// board.set_domain_colour_and_brightness(CD_NAV, BLUE, 99);
	// board.set_domain_colour_and_brightness(CD_BEACON, RED, 99);
	// board.set_domain_colour_and_brightness(CD_STROBE, ORANGE, 99);
	// board.set_domain_colour_and_brightness(CD_BRAKE, ORANGE, 99);
	// board.set_domain_colour_and_brightness(CD_SEARCH, WHITE, 99);
	//
	// board.configure_active_domains(255);

	initializeNodeId();

	// Initialize PineCAN — owned by can.c (initCAN), mirroring the
	// single-servo driver reference. Handler registration is compile-time
	// via RX_HANDLER_LIST in pinecan_handlers.h.
	if (initCAN() != PINECAN_OK)
	{
		Error_Handler();
	}

	// NEW CHANGE: initialise the LED PWM/DMA output (bank/DMA buffers, HAL_TIM_PWM_Start_DMA)
	// and load the GROUND pattern as the boot default, so the board shows a
	// valid pattern before the first CAN message arrives.
	led_init();

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */

	uint64_t raw_vehicle_state = 0;
	bool new_data = false;
	bool stay_in_loop = true;
	uint8_t flight_state = 0;

	while (1)
	{
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */

		while (stay_in_loop)
		{
			// NEW CHANGE: rate-gate the lighting pipeline to ~50Hz (20ms) rather than running
			// it flat-out every loop iteration -- matches legacy's ~20ms cadence
			// without blocking (no HAL_Delay, so CAN servicing below is never
			// starved). `tick` feeds the animation engine (update_animations() in
			// new_lighting_controller.cpp) so BREATHE/STROBE advance at a known,
			// fixed rate regardless of how fast this outer loop otherwise spins.
			static uint32_t lastLedTick = 0;
			static uint32_t tick = 0;
			if (HAL_GetTick() - lastLedTick >= 20)
			{
				lastLedTick = HAL_GetTick();
				Generate_Leds(tick); // advance animations, expand active zones into colour_buffer
				Push_Leds();		 // bit-encode colour_buffer into the DMA-streamed bank buffer
				tick++;
			}

			// Service PineCAN housekeeping (1ms tick-gated pinecan1ms call)
			CANManager::service();

			// Re-read the cache each inner iteration so new CAN data is visible
			raw_vehicle_state = CANManager::getLatestVehicleState();

			// Step 2: check if data changed since last call
			new_data = CANManager::vehicleStateChanged(raw_vehicle_state);

			if (new_data)
			{
				stay_in_loop = false;
			}

			// every once in a while: inactivity check here
			// TODO: inactivity check placeholder
		}

		// read cache and decode — gives us the current flight state
		flight_state = interpretVehicleState(raw_vehicle_state);

		// NEW CHANGE: load the new state's pattern row (colour/brightness/animation per zone,
		// from new_pattern_table.cpp) into current_appearance. Only runs here, on
		// state change -- per-tick animation and LED output happen continuously
		// in the inner loop above, independent of how often this outer step runs.
		Select_Pattern(flight_state);

		new_data = false;
		stay_in_loop = true;
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Configure the main internal regulator output voltage
	 */
	if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;
	RCC_OscInitStruct.MSICalibrationValue = 0;
	RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_11;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
	{
		Error_Handler();
	}
}
/* USER CODE BEGIN 4 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	// canService() (Core/Src/can.c) self-gates on HAL_GetTick() now, so no
	// flag needs to be set here for PineCAN servicing.
	(void)htim;
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 */
void assert_failed(uint8_t *file, uint32_t line)
{
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	   ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
