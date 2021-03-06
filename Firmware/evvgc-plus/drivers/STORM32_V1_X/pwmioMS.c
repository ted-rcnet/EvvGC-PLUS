/*
    EvvGC-PLUS - Copyright (C) 2013-2015 Sarunas Vaitekonis

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/


#include "hal.h"
//#include "eicu.h"

#include "pwmio.h"

/* C libraries: */
#include <math.h>
#include <string.h>



#define PWM_CLK (72000000)
#define PERIOD_PWM (2000)

/**
 * PWM value for the 50 percent of the total power, given:
 * - PWM clock frequency = 72 MHz;
 * - PWM period = 1/18000 s;
 */
#define PWM_OUT_POWER_50PCT     ((PERIOD_PWM)/2)///0x000003E8
/**
 * PWM value for the half percent of the total power, given:
 * - PWM clock frequency = 72 MHz;
 * - PWM period = 1/18000 s;
 */
#define PWM_OUT_POWER_1PCT2     (PWM_OUT_POWER_50PCT/100)//0x0000000A

/**
 * ADC related constants.
 */
#define ADC_GRP_NUM_CHANNELS    0x02
#define ADC_GRP_BUF_DEPTH       0x20

/**
 * Separation angle between phases.
 */
#ifndef M_2PI_3
#define M_2PI_3         (2.0f * M_PI / 3.0f)
#endif

/**
 * Amplitude scaling factor for third harmonic injection PWM.
 */
#define THI_PWM_K       (2.0f / sqrtf(3.0f))

/**
 * Local macros for dead time calculation.
 */
#define constrainLeft(val,left)     ((val)<(left)?(left):(val))
#define constrainRight(val,right)   ((val)>(right)?(right):(val))

/**
 * Forward declarations:
 */
///* Callback function for ADC conversions. */
//static void adccb(ADCDriver *adcp, adcsample_t *buffer, size_t n);
///* Callback function for EICU width calculation. */
//static void eicuwidthcb(EICUDriver *eicup, eicuchannel_t channel, uint32_t w, uint32_t p);
/**
 * Default settings for PWM outputs.
 */
PWMOutputStruct g_pwmOutput[3] = {
  {0,                      /* Motor power;     */
   14,                     /* Number of poles; */
   0x00,                   /* Flags;           */
   PWM_OUT_CMD_DISABLED},
  {0,                      /* Motor power;     */
   14,                     /* Number of poles; */
   0x00,                   /* Flags;           */
   PWM_OUT_CMD_DISABLED},
  {0,                      /* Motor power;     */
   14,                     /* Number of poles; */
   0x00,                   /* Flags;           */
   PWM_OUT_CMD_DISABLED}
};

/**
 * Default settings for generic inputs.
 */
MixedInputStruct g_mixedInput[3] = {
  {1000,                   /* Min value;      */
   1500,                   /* Mid value;      */
   2000,                   /* Max value;      */
   INPUT_CHANNEL_DISABLED},/* Input channel#; */
  {1000,                   /* Min value;      */
   1500,                   /* Mid value;      */
   2000,                   /* Max value;      */
   INPUT_CHANNEL_DISABLED},/* Input channel#; */
  {1000,                   /* Min value;      */
   1500,                   /* Mid value;      */
   2000,                   /* Max value;      */
   INPUT_CHANNEL_DISABLED} /* Input channel#; */
};

/**
 * Values of the input channels.
 */
int16_t g_inputValues[5] = {0};

static void pwmpcb(PWMDriver *pwmp) {

  (void)pwmp;
  //palClearPad(GPIOB, GPIOB_LED_1);
  //palClearPad(GPIOB, GPIOB_LED_0);
}



static void pwmc1cb(PWMDriver *pwmp) {

  (void)pwmp;
  palSetPad(GPIOB, GPIOB_LED_1);

}

static void startPWD2(PWMDriver *pwmp) {

  (void)pwmp;
//  palClearPad(GPIOB, GPIOB_LED_1);
  //palClearPad(GPIOB, GPIOB_LED_0);
  //palClearPad(GPIOC, 0);
    //palClearPad(GPIOC, 1);
    //palClearPad(GPIOA, 0);
}


static void pwmc2cB(PWMDriver *pwmp) {

  (void)pwmp;
  //palSetPad(GPIOB, GPIOB_LED_0);
}


static void pwmc21cB(PWMDriver *pwmp) {

  (void)pwmp;
  //palSetPad(GPIOC, 0);

}

static void pwmc22cB(PWMDriver *pwmp) {

  (void)pwmp;
  //palSetPad(GPIOC, 1);

}

static void startPWD3(PWMDriver *pwmp) {

  (void)pwmp;
//  palClearPad(GPIOB, GPIOB_LED_1);
  //palClearPad(GPIOB, GPIOB_LED_0);
  palClearPad(GPIOB, 0);
  palClearPad(GPIOA, 1);
    //palClearPad(GPIOC, 1);
    //palClearPad(GPIOA, 0);
}


static void pwmc30cB(PWMDriver *pwmp) {

  (void)pwmp;
  palSetPad(GPIOB, 0);
}


static void pwmc31cB(PWMDriver *pwmp) {

  (void)pwmp;
  //palSetPad(GPIOC, 0);
  toggleSetPad(GPIOA, 1);

}

static void pwmc32cB(PWMDriver *pwmp) {

  (void)pwmp;
  //palSetPad(GPIOC, 1);

}

static void pwmc33cB(PWMDriver *pwmp) {

  (void)pwmp;
  //palSetPad(GPIOA, 0);
}



/**
 * PWM configuration structure for TIM1 and TIM8 output.
 */
static PWMConfig pwmcfg_d1_d8 = {
		10000,//PWM_CLK, /* PWM clock frequency (72 MHz). */
		10000, //2000,
		/* PWM period (1/18000 s) in ticks
               for center-aligned mode.      */
  NULL,     /* Callback disabled.            */
  //pwmpcb,
  {         /* PWM channel configuration:    */
		  {PWM_OUTPUT_ACTIVE_HIGH /*| PWM_COMPLEMENTARY_OUTPUT_ACTIVE_HIGH*/, NULL}, /* CH1 */
    //{PWM_OUTPUT_ACTIVE_HIGH /*| PWM_COMPLEMENTARY_OUTPUT_ACTIVE_HIGH*/, pwmc1cb}, /* CH1 */
	{PWM_OUTPUT_ACTIVE_HIGH /*| PWM_COMPLEMENTARY_OUTPUT_ACTIVE_HIGH*/, NULL}, /* CH1 */
    //{PWM_OUTPUT_ACTIVE_HIGH | PWM_COMPLEMENTARY_OUTPUT_ACTIVE_HIGH, NULL}, /* CH2 */
	//{PWM_OUTPUT_DISABLED,                                           NULL},  /* CH4 */
    //{PWM_OUTPUT_ACTIVE_HIGH | PWM_COMPLEMENTARY_OUTPUT_ACTIVE_HIGH, NULL}, /* CH3 */
	{PWM_OUTPUT_DISABLED,                                           NULL},  /* CH4 */
    {PWM_OUTPUT_DISABLED,                                           NULL}  /* CH4 */
  },
  0, /* CR2 register value. */
#if STM32_PWM_USE_ADVANCED
  /* BDTR register value: */
  STM32_TIM_BDTR_LOCK(1) | /* LOCK Level 1: DTG bits in TIMx_BDTR
                              register, OISx and OISxN bits in TIMx_CR2
                              register and BKE/BKP/AOE bits in TIMx_BDTR
                              register can no longer be written.        */
  STM32_TIM_BDTR_OSSI |    /* Off-state selection for Idle mode enable. */
  STM32_TIM_BDTR_OSSR |    /* Off-state selection for Run mode enable.  */
  STM32_TIM_BDTR_AOE,      /* Automatic output enable.                  */
#endif
  0  /* DIER register value. */
};

/**
 * PWM configuration structure for TIM2 output.
 */
static const PWMConfig pwmcfg_d2 = {
  10000,//PWM_CLK, /* PWM clock frequency (72 MHz). */
  10000,
  //PERIOD_PWM,
  /* PWM period (1/18000 s) in ticks
               for center-aligned mode.      */
  NULL,
  //startPWD2,//NULL,     /* Callback disabled.            */
  {         /* PWM channel configuration:    */
    //{PWM_OUTPUT_DISABLED, pwmc2cB},  // CH1: NC
		  {PWM_OUTPUT_ACTIVE_HIGH,
				  NULL},
				  //pwmc2cB},  // CH1: NC
    {PWM_OUTPUT_ACTIVE_HIGH,
    		NULL},
    		//pwmc21cB}, // CH1: M1-B1
    {PWM_OUTPUT_ACTIVE_HIGH,
    		NULL},
    		//pwmc22cB},  // CH1: M1-A1
    {PWM_OUTPUT_ACTIVE_HIGH,
    		NULL}
    		//pwmc23cB}  // CH1: M1-A0
  },
  0, /* CR2 register value. */
#if STM32_PWM_USE_ADVANCED
  0, /* BDTR register value. Not used for TIM5. */
#endif
  0  /* DIER register value. */
};/**
 * PWM configuration structure for TIM3 output.
 */
static const PWMConfig pwmcfg_d3 = {
  10000,//PWM_CLK, /* PWM clock frequency (72 MHz). */
  10000,PERIOD_PWM,     /* PWM period (1/18000 s) in ticks
               for center-aligned mode.      */
  NULL,//startPWD3,     /* Callback disabled.            */
  {         /* PWM channel configuration:    */
    {PWM_OUTPUT_ACTIVE_HIGH, NULL},
	//pwmc30cB}, // CH1: M0-A0
    {PWM_OUTPUT_ACTIVE_HIGH, NULL},
	//pwmc31cB}, 	// CH2: M0-A1
    {PWM_OUTPUT_ACTIVE_HIGH, NULL}, // CH3: M0-B0
    {PWM_OUTPUT_ACTIVE_HIGH,    NULL}// CH4: M0-B1
  },
  0, /* CR2 register value. */
#if STM32_PWM_USE_ADVANCED
  0, /* BDTR register value. Not used for TIM5. */
#endif
  0  /* DIER register value. */
};


/**
 * PWM configuration structure for TIM4 output.
 */
static const PWMConfig pwmcfg_d4 = {
  PWM_CLK, /* PWM clock frequency (72 MHz). */
  PERIOD_PWM,     /* PWM period (1/18000 s) in ticks
               for center-aligned mode.      */
  NULL,     /* Callback disabled.            */
  {         /* PWM channel configuration:    */
    {PWM_OUTPUT_DISABLED, NULL},  //CH1: NC
    {PWM_OUTPUT_DISABLED, NULL},  //CH2: NC
    {PWM_OUTPUT_ACTIVE_HIGH, NULL},  //CH3: NC - pin3-M3
    {PWM_OUTPUT_ACTIVE_HIGH,   NULL}   //CH4: M1-B0
  },
  0, /* CR2 register value. */
#if STM32_PWM_USE_ADVANCED
  0, /* BDTR register value. Not used for TIM4. */
#endif
  0  /* DIER register value. */
};

///**
// * ADC conversion group.
// * Mode:     Continuous, circular, 32 samples of 2 channels.
// * Channels: IN12, IN13.
// */
//static const ADCConversionGroup adcgrpcfg = {
//  TRUE,                 /* Circular buffer mode enabled.    */
//  ADC_GRP_NUM_CHANNELS, /* Number of channels in the group. */
//  adccb,                /* Callback function of the group.  */
//  NULL,                 /* Error callback function.         */
//  /* STM32F1xx dependent part: */
//  0,                                      /* CR1.   */
//  0,                                      /* CR2.   */
//  ADC_SMPR1_SMP_AN12(ADC_SAMPLE_239P5) |
//  ADC_SMPR1_SMP_AN13(ADC_SAMPLE_239P5),   /* SMPR1. */
//  0,                                      /* SMPR2. */
//  ADC_SQR1_NUM_CH(ADC_GRP_NUM_CHANNELS),  /* SQR1.  */
//  0,                                      /* SQR2.  */
//  ADC_SQR3_SQ2_N(ADC_CHANNEL_IN13) |
//  ADC_SQR3_SQ1_N(ADC_CHANNEL_IN12)        /* SQR3.  */
//};

///**
// * Input capture configuration for EICU2 driver.
// *
// * @note EICU drivers used in the firmware are modified ChibiOS
// *       drivers for extended input capture functionality.
// */
//static const EICUChannelConfig eicuset2 = {
//    EICU_INPUT_ACTIVE_HIGH,
//    EICU_INPUT_BOTH,
//    eicuwidthcb
//};
//
//static const EICUConfig eicucfg2 = {
//  1000000,    /* EICU clock frequency in Hz.*/
//  {
//    NULL,
//    &eicuset2,
//    NULL,
//    NULL,
//  },
//  0
//};

///**
// * Input capture configuration for EICU3 driver.
// *
// * @note EICU drivers used in the firmware are modified ChibiOS
// *       drivers for extended input capture functionality.
// */
//static const EICUChannelConfig eicuset3 = {
//    EICU_INPUT_ACTIVE_HIGH,
//    EICU_INPUT_PULSE,
//    eicuwidthcb
//};
//
//static const EICUConfig eicucfg3 = {
//  1000000,    /* EICU clock frequency in Hz.*/
//  {
//    &eicuset3,
//    &eicuset3,
//    NULL,
//    NULL,
//  },
//  0
//};


/**
 * Local PWM output values for three phase BLDC motor driver.
 */
static uint32_t pwm3PhaseDrv[3];

/**
 * Local buffer for ADC conversions.
 */
static adcsample_t adcBuf[ADC_GRP_NUM_CHANNELS * ADC_GRP_BUF_DEPTH];

/**
 * @brief  ADC streaming callback.
 * @param  adcp - pointer to the ADCDriver object triggering the callback.
 * @param  buffer - pointer to the most recent samples data.
 * @param  n - number of buffer rows available starting from buffer.
 * @return none.
 */
static void adccb(ADCDriver *adcp, adcsample_t *buffer, size_t n) {
  (void)adcp;
  uint32_t sum1 = 0;
  uint32_t sum2 = 0;
  size_t i;
  for(i = 0; i < n; i++) {
    sum1 += *buffer++;
    sum2 += *buffer++;
  }
  g_inputValues[INPUT_CHANNEL_AUX1] = (adcsample_t)(sum1 / n);
  g_inputValues[INPUT_CHANNEL_AUX2] = (adcsample_t)(sum2 / n);
}

///**
// * @brief  Callback function for input capture unit.
// * @param  eicup - pointer to the input capture driver.
// * @param  channel - input capture channel triggering the callback.
// * @return none.
// */
//static void eicuwidthcb(EICUDriver *eicup, eicuchannel_t channel, uint32_t w, uint32_t p) {
//  (void)p;
//  if (&EICUD2 == eicup) {
//    g_inputValues[INPUT_CHANNEL_AUX3] = w;
//  } else {
//    g_inputValues[INPUT_CHANNEL_AUX4 + channel] = w;
//  }
//}

/**
 * @brief  Callback function for input capture unit.
 * @param  eicup - pointer to the input capture driver.
 * @param  channel - input capture channel triggering the callback.
 * @return none.
 */


/**
 * @brief  Disables PWM on roll driver.
 * @return none.
 */
static void pwmOutputDisableRoll(void) {
  /* Disable update event. */
  PWMD2.tim->CR1 |= STM32_TIM_CR1_UDIS;
  PWMD3.tim->CR1 |= STM32_TIM_CR1_UDIS;

  PWMD3.tim->CCR[0] = 0;
  PWMD2.tim->CCR[3] = 0;
  PWMD2.tim->CCR[2] = 0;

  /* Enable update event. */
  PWMD2.tim->CR1 &= ~STM32_TIM_CR1_UDIS;
  PWMD3.tim->CR1 &= ~STM32_TIM_CR1_UDIS;
}

/**
 * @brief  Disables PWM on pitch driver.
 * @return none.
 */
static void pwmOutputDisablePitch(void) {
  /* Disable update event. */
  PWMD3.tim->CR1 |= STM32_TIM_CR1_UDIS;

  PWMD3.tim->CCR[3] = 0;
  PWMD3.tim->CCR[2] = 0;
  PWMD3.tim->CCR[1] = 0;

  /* Enable update event. */
  PWMD3.tim->CR1 &= ~STM32_TIM_CR1_UDIS;
}

/**
 * @brief  Disables PWM on yaw driver.
 * @return none.
 */
static void pwmOutputDisableYaw(void) {
  chSysLock();
  /* Disable update event. */
  PWMD2.tim->CR1 |= STM32_TIM_CR1_UDIS;
  PWMD4.tim->CR1 |= STM32_TIM_CR1_UDIS;
  chSysUnlock();

  PWMD4.tim->CCR[3] = 0;
  PWMD2.tim->CCR[1] = 0;
  PWMD4.tim->CCR[2] = 0;

  chSysLock();
  /* Enable update event. */
  PWMD2.tim->CR1 &= ~STM32_TIM_CR1_UDIS;
  PWMD4.tim->CR1 &= ~STM32_TIM_CR1_UDIS;
  chSysUnlock();
}

/**
 * @brief  Converts motor command to biased three phase motor driving PWM signal.
 * @param  cmd - new position of the motor.
 * @param  power - power of the motor in percent.
 * @param  thi - third harmonic injection enable flag.
 * @return none.
 */
static void pwmOutputCmdTo3PhasePWM(float cmd, uint8_t power, uint8_t thi) {
  float halfPower = power * PWM_OUT_POWER_1PCT2;
  if (thi) {
    halfPower *= THI_PWM_K;
    float thirdHarmonic = sinf(cmd * 3.0f) / 6.0f;
    pwm3PhaseDrv[0] = PWM_OUT_POWER_50PCT + (sinf(cmd) + thirdHarmonic)*halfPower;
    pwm3PhaseDrv[1] = PWM_OUT_POWER_50PCT + (sinf(cmd + M_2PI_3) + thirdHarmonic)*halfPower;
    pwm3PhaseDrv[2] = PWM_OUT_POWER_50PCT + (sinf(cmd - M_2PI_3) + thirdHarmonic)*halfPower;
  } else {
    pwm3PhaseDrv[0] = PWM_OUT_POWER_50PCT + sinf(cmd)*halfPower;
    pwm3PhaseDrv[1] = PWM_OUT_POWER_50PCT + sinf(cmd + M_2PI_3)*halfPower;
    pwm3PhaseDrv[2] = PWM_OUT_POWER_50PCT + sinf(cmd - M_2PI_3)*halfPower;
  }
}

/**
 *
 */
static void pwmOutputUpdateRoll(void) {
  /* Disable update event. */
  PWMD2.tim->CR1 |= STM32_TIM_CR1_UDIS;
  PWMD3.tim->CR1 |= STM32_TIM_CR1_UDIS;

  /* Check if motor direction is reversed. */
  if (g_pwmOutput[PWM_OUT_ROLL].flags & PWM_OUT_REV_FLAG) {
	PWMD3.tim->CCR[0] = pwm3PhaseDrv[1];
	PWMD2.tim->CCR[3] = pwm3PhaseDrv[0];
  } else {
	PWMD3.tim->CCR[0] = pwm3PhaseDrv[0];
	PWMD2.tim->CCR[3] = pwm3PhaseDrv[1];
  }
  PWMD2.tim->CCR[2] = pwm3PhaseDrv[2];

  /* Enable update event. */
  PWMD2.tim->CR1 &= ~STM32_TIM_CR1_UDIS;
  PWMD3.tim->CR1 &= ~STM32_TIM_CR1_UDIS;
}

/**
 *
 */
static void pwmOutputUpdatePitch(void) {
  /* Disable update event. */
  PWMD3.tim->CR1 |= STM32_TIM_CR1_UDIS;

  /* Check if motor direction is reversed. */
  if (g_pwmOutput[PWM_OUT_PITCH].flags & PWM_OUT_REV_FLAG) {
    PWMD3.tim->CCR[3] = pwm3PhaseDrv[1];
    PWMD3.tim->CCR[2] = pwm3PhaseDrv[0];
  } else {
    PWMD3.tim->CCR[3] = pwm3PhaseDrv[0];
    PWMD3.tim->CCR[2] = pwm3PhaseDrv[1];
  }
  PWMD3.tim->CCR[1] = pwm3PhaseDrv[2];

  /* Enable update event. */
  PWMD3.tim->CR1 &= ~STM32_TIM_CR1_UDIS;
}

/**
 *
 */
static void pwmOutputUpdateYaw(void) {
  chSysLock();
  /* Disable update event. */
  PWMD2.tim->CR1 |= STM32_TIM_CR1_UDIS;
  PWMD4.tim->CR1 |= STM32_TIM_CR1_UDIS;
  chSysUnlock();

  /**
   * Update shadow registers.
   * Apply dead-time to Yaw PWM:
   */
  /* Check if motor direction is reversed. */

  /* Check if motor direction is reversed. */
  if (g_pwmOutput[PWM_OUT_YAW].flags & PWM_OUT_REV_FLAG) {
	  PWMD4.tim->CCR[3] = pwm3PhaseDrv[1];
	  PWMD2.tim->CCR[1] = pwm3PhaseDrv[0];
  } else {
	  PWMD4.tim->CCR[3] = pwm3PhaseDrv[0];
	  PWMD2.tim->CCR[1] = pwm3PhaseDrv[1];
  }
  PWMD4.tim->CCR[2] = pwm3PhaseDrv[2];

  chSysLock();
  /* Enable update event. */
  PWMD2.tim->CR1 &= ~STM32_TIM_CR1_UDIS;
  PWMD4.tim->CR1 &= ~STM32_TIM_CR1_UDIS;
  chSysUnlock();
}

/**
 * @brief  Starts the PWM output.
 * @note   The pwmStart() function used in this code is not
 *         the original ChibiOS HAL function, but modified
 *         one with STM32_TIM_CR1_CEN flag removed.
 * @return none.
 */
void pwmOutputStart(void) {

#if 1
  pwmcfg_d1_d8.cr2 = STM32_TIM_CR2_MMS(1); // Master mode set to Enable;

  /* Configure TIM1 as master timer: */
  pwmStart(&PWMD1, &pwmcfg_d1_d8);
  pwmEnablePeriodicNotification(&PWMD1);

  palSetPadMode(GPIOB, 8, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  palSetPadMode(GPIOA, 1, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  palSetPadMode(GPIOB, 0, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  palSetPadMode(GPIOB, 1, PAL_MODE_STM32_ALTERNATE_PUSHPULL);

  //return;

  PWMD1.tim->CR1 &= ~STM32_TIM_CR1_CEN;

  //pwmEnableChannel(&PWMD1, 0, 5000);
  pwmEnableChannel(&PWMD1, 0, PWM_PERCENTAGE_TO_WIDTH(&PWMD1, 1200));
  pwmEnableChannelNotification(&PWMD1, 0);
  pwmEnableChannel(&PWMD1, 1, PWM_PERCENTAGE_TO_WIDTH(&PWMD1, 5000));
  pwmEnableChannel(&PWMD1, 2, PWM_PERCENTAGE_TO_WIDTH(&PWMD1, 7000));
  pwmEnableChannel(&PWMD1, 3, PWM_PERCENTAGE_TO_WIDTH(&PWMD1, 1200));
    pwmEnableChannelNotification(&PWMD1, 1);
  //pwmEnableChannel(&PWMD1, 1, 5000);
  //pwmEnableChannel(&PWMD1, 2, 5000);

  while(TRUE){

  	      chThdSleepMilliseconds(5000);

    }

  while (TRUE) {
     chThdSleepMilliseconds(1000);
     pwmEnableChannel(&PWMD1, 0, PWM_PERCENTAGE_TO_WIDTH(&PWMD1, 5000));
     chThdSleepMilliseconds(1000);
     pwmEnableChannel(&PWMD1, 0, PWM_PERCENTAGE_TO_WIDTH(&PWMD1, 2500));
     chThdSleepMilliseconds(1000);
     pwmEnableChannel(&PWMD1, 0, PWM_PERCENTAGE_TO_WIDTH(&PWMD1, 0000));
     chThdSleepMilliseconds(1000);
     pwmEnableChannel(&PWMD1, 0, PWM_PERCENTAGE_TO_WIDTH(&PWMD1, 9900));
   }



#endif

#if 0
  pwmStart(&PWMD2, &pwmcfg_d2);
#if 1
  pwmEnablePeriodicNotification(&PWMD2);
  pwmEnableChannelNotification(&PWMD2, 0);
  pwmEnableChannelNotification(&PWMD2, 1);
  pwmEnableChannelNotification(&PWMD2, 2);
  pwmEnableChannelNotification(&PWMD2, 3);
  //palSetPadMode(GPIOB, GPIOB_LED_1, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
//  palSetPadMode(GPIOB, GPIOB_LED_0, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
#endif

  // tu jest problem!!
#if 1
  //pwmEnableChannel(&PWMD2, 0, PWM_PERCENTAGE_TO_WIDTH(&PWMD2, 5700));
  // to wlacza YawM
  //pwmEnableChannel(&PWMD2, 1, PWM_PERCENTAGE_TO_WIDTH(&PWMD2, 5000));
  //pwmEnableChannel(&PWMD2, 2, PWM_PERCENTAGE_TO_WIDTH(&PWMD2, 5000));
  // a to RollM
  //pwmEnableChannel(&PWMD2, 3, PWM_PERCENTAGE_TO_WIDTH(&PWMD2, 5000));
#endif
#endif
  palSetPadMode(GPIOA, 1, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  palSetPadMode(GPIOA, 9, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
#if 1
  pwmStart(&PWMD3, &pwmcfg_d3);
#if 1
  pwmEnablePeriodicNotification(&PWMD3);
  pwmEnableChannelNotification(&PWMD3, 0);
  pwmEnableChannelNotification(&PWMD3, 1);
  //pwmEnableChannelNotification(&PWMD3, 2);
  //pwmEnableChannelNotification(&PWMD3, 3);
  //palSetPadMode(GPIOB, GPIOB_LED_1, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
//  palSetPadMode(GPIOB, GPIOB_LED_0, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
#endif


  // tu jest problem!!
#if 0
  pwmEnableChannel(&PWMD3, 0, PWM_PERCENTAGE_TO_WIDTH(&PWMD3, 5700));
  // to wlacza YawM
  pwmEnableChannel(&PWMD3, 1, PWM_PERCENTAGE_TO_WIDTH(&PWMD3, 5000));
  //pwmEnableChannel(&PWMD2, 2, PWM_PERCENTAGE_TO_WIDTH(&PWMD3, 5000));
  // a to RollM
  //pwmEnableChannel(&PWMD2, 3, PWM_PERCENTAGE_TO_WIDTH(&PWMD3, 5000));
#endif
#endif

  //pwmEnableChannel(&PWMD2, 0, PWM_PERCENTAGE_TO_WIDTH(&PWMD2, 6700));

#if 0
  pwmStart(&PWMD3, &pwmcfg_d3);
  pwmStart(&PWMD4, &pwmcfg_d4);

  // Timers started, but not in sync. So we stop them and reset.
  PWMD2.tim->CR1 &= ~STM32_TIM_CR1_CEN;
  PWMD3.tim->CR1 &= ~STM32_TIM_CR1_CEN;
  PWMD4.tim->CR1 &= ~STM32_TIM_CR1_CEN;
  PWMD2.tim->CNT  = 0;
  PWMD3.tim->CNT  = 0;
  PWMD4.tim->CNT  = 0;

  /* Configure TIM2 as slave timer: */
  PWMD2.tim->SMCR =
    STM32_TIM_SMCR_SMS(6) | // Trigger Mode;
    STM32_TIM_SMCR_TS(0);   // Trigger event comes from TIM1;

  /* Configure TIM3 as slave timer: */
  PWMD3.tim->SMCR =
    STM32_TIM_SMCR_SMS(6) | // Trigger Mode;
    STM32_TIM_SMCR_TS(0);   // Trigger event comes from TIM1;

  /* Configure TIM4 as slave timer: */
  PWMD4.tim->SMCR =
    STM32_TIM_SMCR_SMS(6) | // Trigger Mode;
    STM32_TIM_SMCR_TS(0);   // Trigger event comes from TIM1;

  /* Switch to center-aligned mode 1 and start timers. */
  PWMD2.tim->CR1 |= (STM32_TIM_CR1_CMS(1)); // This is a slave timer - do not start;
  PWMD3.tim->CR1 |= (STM32_TIM_CR1_CMS(1)); // This is a slave timer - do not start;
  PWMD4.tim->CR1 |= (STM32_TIM_CR1_CMS(1)); // This is a slave timer - do not start;

  PWMD1.tim->CR1 |= (STM32_TIM_CR1_CMS(1) | STM32_TIM_CR1_CEN);;
#endif
}

/**
 * @brief  Stops the PWM output.
 * @return none.
 */
void pwmOutputStop(void) {
  pwmOutputDisableAll();
  pwmStop(&PWMD1);
  //pwmStop(&PWMD8);
  pwmStop(&PWMD2);
  pwmStop(&PWMD3);
  pwmStop(&PWMD4);
}

/**
 * @brief  Updates specified PWM output channel driver state
 *         according to the given command.
 * @param  channel_id - PWM output channel ID.
 * @param  cmd - new command to the motor driver.
 * @return none.
 */
void pwmOutputUpdate(const uint8_t channel_id, float cmd) {
  switch (channel_id) {
  case PWM_OUT_PITCH:
    if ((g_pwmOutput[PWM_OUT_PITCH].dt_cmd_id & PWM_OUT_CMD_ID_MASK) == PWM_OUT_CMD_DISABLED) {
      pwmOutputDisablePitch();
    } else {
      pwmOutputCmdTo3PhasePWM(cmd, g_pwmOutput[PWM_OUT_PITCH].power,
        g_pwmOutput[PWM_OUT_PITCH].flags & PWM_OUT_THI_FLAG);
      pwmOutputUpdatePitch();
    }
    break;
  case PWM_OUT_ROLL:
    if ((g_pwmOutput[PWM_OUT_ROLL].dt_cmd_id & PWM_OUT_CMD_ID_MASK) == PWM_OUT_CMD_DISABLED) {
      pwmOutputDisableRoll();
    } else {
      pwmOutputCmdTo3PhasePWM(cmd, g_pwmOutput[PWM_OUT_ROLL].power,
        g_pwmOutput[PWM_OUT_ROLL].flags & PWM_OUT_THI_FLAG);
      pwmOutputUpdateRoll();
    }
    break;
  case PWM_OUT_YAW:
    if ((g_pwmOutput[PWM_OUT_YAW].dt_cmd_id & PWM_OUT_CMD_ID_MASK) == PWM_OUT_CMD_DISABLED) {
      pwmOutputDisableYaw();
    } else {
      pwmOutputCmdTo3PhasePWM(cmd, g_pwmOutput[PWM_OUT_YAW].power,
        g_pwmOutput[PWM_OUT_YAW].flags & PWM_OUT_THI_FLAG);
      pwmOutputUpdateYaw();
    }
    break;
  default:;
  }
}

/**
 * @brief  Disables all PWM output channels.
 * @return none.
 */
void pwmOutputDisableAll(void) {
  pwmOutputUpdatePitch();
  pwmOutputUpdateRoll();
  pwmOutputUpdateYaw();
}

/**
 *
 */
void pwmOutputSettingsUpdate(const PPWMOutputStruct pNewSettings) {
  memcpy((void *)&g_pwmOutput, (void *)pNewSettings, sizeof(g_pwmOutput));
}

///**
// * @brief  Starts the ADC and EICU input drivers.
// * @note   EICU drivers used in the firmware are modified ChibiOS
// *         drivers for extended input capture functionality.
// * @return none.
// */
void mixedInputStart(void) {
//  /* Activates the EICU2 and EICU3 drivers. */
//  eicuStart(&EICUD2, &eicucfg2);
//  eicuStart(&EICUD3, &eicucfg3);
//  /* Starts continuous pulse width measurements. */
//  eicuEnable(&EICUD2);
//  eicuEnable(&EICUD3);
//
//  /* Activates the ADC1 driver. */
//  adcStart(&ADCD1, NULL);
//  /* Starts an ADC continuous conversion. */
//  adcStartConversion(&ADCD1, &adcgrpcfg, adcBuf, ADC_GRP_BUF_DEPTH);
}
//
///**
// * @brief  Stops the ADC and EICU input drivers.
// * @return none.
// */
void mixedInputStop(void) {
//  eicuStop(&EICUD2);
//  eicuStop(&EICUD3);
//  adcStop(&ADCD1);
}
//
///**
// *
// */
void mixedInputSettingsUpdate(const PMixedInputStruct pNewSettings) {
//  memcpy((void *)&g_mixedInput, (void *)pNewSettings, sizeof(g_mixedInput));
}
