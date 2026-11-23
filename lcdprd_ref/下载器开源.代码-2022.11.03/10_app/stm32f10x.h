/**
  ******************************************************************************
  * @file    stm32f10x.h
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    11-March-2011
  * @brief   CMSIS Cortex-M3 Device Peripheral Access Layer Header File. 
  *          This file contains all the peripheral register's definitions, bits 
  *          definitions and memory mapping for STM32F10x Connectivity line, 
  *          High density, High density value line, Medium density, 
  *          Medium density Value line, Low density, Low density Value line 
  *          and XL-density devices.
  *
  *          The file is the unique include file that the application programmer
  *          is using in the C source code, usually in main.c. This file contains:
  *           - Configuration section that allows to select:
  *              - The device used in the target application
  *              - To use or not the peripheral抯 drivers in application code(i.e. 
  *                code will be based on direct access to peripheral抯 registers 
  *                rather than drivers API), this option is controlled by 
  *                "#define USE_STDPERIPH_DRIVER"
  *              - To change few application-specific parameters such as the HSE 
  *                crystal frequency
  *           - Data structures and the address mapping for all peripherals
  *           - Peripheral's registers declarations and bits definition
  *           - Macros to access peripheral抯 registers hardware
  *
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */


/*
//-------- <<< Use Configuration Wizard in Context Menu >>> ------------------
*/
///  <o0.0> 关于防止破解中的打开和关闭SWD引脚的功能
//               <0=> 打开调试引脚
//               <1=> 关闭调试引脚
//
#define CLOSE_SWDPIN  0
///  <o0.0> 启用软件发布
//   <i> 因为发布版本存在不一样，而且会造成程序的不能运行，所以用这个开关进行编辑
//   <i> 比如：检查AES加密，BOOT违法验证，还有升级标志，还有中断向量等等。
//               <0=> 关闭发布版本
//               <1=> 启用发布版本
//
#define ENABLE_APP_ISSUE  0

#define HARDWARE_VERSION  1

///  <o0.0> 强制版本型号调试(关闭发布版本下启用)
//   <i> 旧版本的主时钟，统一使用内部倍频，但是发布版本判断方法为：根据BOOT来的。
//   <i> 于是在调试模式下，需要进行强制版本。
//   <i> 这个的作用，只能在：关闭发布版本中使用。其它情况下不使用。
//               <0=> 强制新版本
//               <1=> 强制旧版本
//
#define FORCE_VERSION  0


/*
//-------- <<< end of configuration section >>> ------------------------------
*/


#define MINI_RESET_2  GPIOB->ODR.OutputData1_RW

#define MINI_VERSION  0
#define LCD_VERSION   1

#define LED_MINI_1 GPIOC->ODR.OutputData3_RW
#define LED_MINI_2 GPIOC->ODR.OutputData7_RW

#define LCD_NULL_DEBUG_EN 255,255,255,RED   // 由于LCD作为LCD显示的时候，同时也显示DEBUG信息，用这个宏定义，则表示，只用来进行DEUG信息显示。

#define L_ENGLISH_VER  HighDensity_FlashPage(255)->IndexLen16[0]  
#define L_LIGNT_LV     HighDensity_FlashPage(255)->IndexLen16[1]  // 0-9:表示高度：10%-100%，禁止息屏，
#define L_BEEP_ENABLE  HighDensity_FlashPage(255)->IndexLen16[2]  // 打开与关闭蜂鸣器，mini版本的：使用：五击，来回切换。

/** @addtogroup CMSIS
  * @{
  */

/** @addtogroup stm32f10x
  * @{
  */

int usbhidCom(void);
extern unsigned char isHIDVersion;
    
#ifndef __STM32F10x_H
#define __STM32F10x_H

#ifdef __cplusplus
 extern "C" {
#endif 
  
/** @addtogroup Library_configuration_section
  * @{
  */
  
/* Uncomment the line below according to the target STM32 device used in your
   application 
  */


/**
  * @}
  */

/** @addtogroup Configuration_section_for_CMSIS
  * @{
  */

/**
 * @brief Configuration of the Cortex-M3 Processor and Core Peripherals 
 */
#ifdef STM32F10X_XL
 #define __MPU_PRESENT             1 /*!< STM32 XL-density devices provide an MPU */
#else
 #define __MPU_PRESENT             0 /*!< Other STM32 devices does not provide an MPU */
#endif /* STM32F10X_XL */
#define __NVIC_PRIO_BITS          4 /*!< STM32 uses 4 Bits for the Priority Levels    */
#define __Vendor_SysTickConfig    0 /*!< Set to 1 if different SysTick Config is used */

/**
 * @brief STM32F10x Interrupt Number Definition, according to the selected device 
 *        in @ref Library_configuration_section 
 */
typedef enum IRQn
{
/******  Cortex-M3 Processor Exceptions Numbers ***************************************************/
  NonMaskableInt_IRQn         = -14,    /*!< 2 Non Maskable Interrupt                             */
  MemoryManagement_IRQn       = -12,    /*!< 4 Cortex-M3 Memory Management Interrupt              */
  BusFault_IRQn               = -11,    /*!< 5 Cortex-M3 Bus Fault Interrupt                      */
  UsageFault_IRQn             = -10,    /*!< 6 Cortex-M3 Usage Fault Interrupt                    */
  SVCall_IRQn                 = -5,     /*!< 11 Cortex-M3 SV Call Interrupt                       */
  DebugMonitor_IRQn           = -4,     /*!< 12 Cortex-M3 Debug Monitor Interrupt                 */
  PendSV_IRQn                 = -2,     /*!< 14 Cortex-M3 Pend SV Interrupt                       */
  SysTick_IRQn                = -1,     /*!< 15 Cortex-M3 System Tick Interrupt                   */

/******  STM32 specific Interrupt Numbers *********************************************************/
  WWDG_IRQn                   = 0,      /*!< Window WatchDog Interrupt                            */
  PVD_IRQn                    = 1,      /*!< PVD through EXTI Line detection Interrupt            */
  TAMPER_IRQn                 = 2,      /*!< Tamper Interrupt                                     */
  RTC_IRQn                    = 3,      /*!< RTC global Interrupt                                 */
  FLASH_IRQn                  = 4,      /*!< FLASH global Interrupt                               */
  RCC_IRQn                    = 5,      /*!< RCC global Interrupt                                 */
  EXTI0_IRQn                  = 6,      /*!< EXTI Line0 Interrupt                                 */
  EXTI1_IRQn                  = 7,      /*!< EXTI Line1 Interrupt                                 */
  EXTI2_IRQn                  = 8,      /*!< EXTI Line2 Interrupt                                 */
  EXTI3_IRQn                  = 9,      /*!< EXTI Line3 Interrupt                                 */
  EXTI4_IRQn                  = 10,     /*!< EXTI Line4 Interrupt                                 */
  DMA1_Channel1_IRQn          = 11,     /*!< DMA1 Channel 1 global Interrupt                      */
  DMA1_Channel2_IRQn          = 12,     /*!< DMA1 Channel 2 global Interrupt                      */
  DMA1_Channel3_IRQn          = 13,     /*!< DMA1 Channel 3 global Interrupt                      */
  DMA1_Channel4_IRQn          = 14,     /*!< DMA1 Channel 4 global Interrupt                      */
  DMA1_Channel5_IRQn          = 15,     /*!< DMA1 Channel 5 global Interrupt                      */
  DMA1_Channel6_IRQn          = 16,     /*!< DMA1 Channel 6 global Interrupt                      */
  DMA1_Channel7_IRQn          = 17,     /*!< DMA1 Channel 7 global Interrupt                      */

#ifdef STM32F10X_LD
  ADC1_2_IRQn                 = 18,     /*!< ADC1 and ADC2 global Interrupt                       */
  USB_HP_CAN1_TX_IRQn         = 19,     /*!< USB Device High Priority or CAN1 TX Interrupts       */
  USB_LP_CAN1_RX0_IRQn        = 20,     /*!< USB Device Low Priority or CAN1 RX0 Interrupts       */
  CAN1_RX1_IRQn               = 21,     /*!< CAN1 RX1 Interrupt                                   */
  CAN1_SCE_IRQn               = 22,     /*!< CAN1 SCE Interrupt                                   */
  EXTI9_5_IRQn                = 23,     /*!< External Line[9:5] Interrupts                        */
  TIM1_BRK_IRQn               = 24,     /*!< TIM1 Break Interrupt                                 */
  TIM1_UP_IRQn                = 25,     /*!< TIM1 Update Interrupt                                */
  TIM1_TRG_COM_IRQn           = 26,     /*!< TIM1 Trigger and Commutation Interrupt               */
  TIM1_CC_IRQn                = 27,     /*!< TIM1 Capture Compare Interrupt                       */
  TIM2_IRQn                   = 28,     /*!< TIM2 global Interrupt                                */
  TIM3_IRQn                   = 29,     /*!< TIM3 global Interrupt                                */
  I2C1_EV_IRQn                = 31,     /*!< I2C1 Event Interrupt                                 */
  I2C1_ER_IRQn                = 32,     /*!< I2C1 Error Interrupt                                 */
  SPI1_IRQn                   = 35,     /*!< SPI1 global Interrupt                                */
  USART1_IRQn                 = 37,     /*!< USART1 global Interrupt                              */
  USART2_IRQn                 = 38,     /*!< USART2 global Interrupt                              */
  EXTI15_10_IRQn              = 40,     /*!< External Line[15:10] Interrupts                      */
  RTCAlarm_IRQn               = 41,     /*!< RTC Alarm through EXTI Line Interrupt                */
  USBWakeUp_IRQn              = 42      /*!< USB Device WakeUp from suspend through EXTI Line Interrupt */    
#endif /* STM32F10X_LD */  

#ifdef STM32F10X_LD_VL
  ADC1_IRQn                   = 18,     /*!< ADC1 global Interrupt                                */
  EXTI9_5_IRQn                = 23,     /*!< External Line[9:5] Interrupts                        */
  TIM1_BRK_TIM15_IRQn         = 24,     /*!< TIM1 Break and TIM15 Interrupts                      */
  TIM1_UP_TIM16_IRQn          = 25,     /*!< TIM1 Update and TIM16 Interrupts                     */
  TIM1_TRG_COM_TIM17_IRQn     = 26,     /*!< TIM1 Trigger and Commutation and TIM17 Interrupt     */
  TIM1_CC_IRQn                = 27,     /*!< TIM1 Capture Compare Interrupt                       */
  TIM2_IRQn                   = 28,     /*!< TIM2 global Interrupt                                */
  TIM3_IRQn                   = 29,     /*!< TIM3 global Interrupt                                */
  I2C1_EV_IRQn                = 31,     /*!< I2C1 Event Interrupt                                 */
  I2C1_ER_IRQn                = 32,     /*!< I2C1 Error Interrupt                                 */
  SPI1_IRQn                   = 35,     /*!< SPI1 global Interrupt                                */
  USART1_IRQn                 = 37,     /*!< USART1 global Interrupt                              */
  USART2_IRQn                 = 38,     /*!< USART2 global Interrupt                              */
  EXTI15_10_IRQn              = 40,     /*!< External Line[15:10] Interrupts                      */
  RTCAlarm_IRQn               = 41,     /*!< RTC Alarm through EXTI Line Interrupt                */
  CEC_IRQn                    = 42,     /*!< HDMI-CEC Interrupt                                   */
  TIM6_DAC_IRQn               = 54,     /*!< TIM6 and DAC underrun Interrupt                      */
  TIM7_IRQn                   = 55      /*!< TIM7 Interrupt                                       */       
#endif /* STM32F10X_LD_VL */

#ifdef STM32F10X_MD
  ADC1_2_IRQn                 = 18,     /*!< ADC1 and ADC2 global Interrupt                       */
  USB_HP_CAN1_TX_IRQn         = 19,     /*!< USB Device High Priority or CAN1 TX Interrupts       */
  USB_LP_CAN1_RX0_IRQn        = 20,     /*!< USB Device Low Priority or CAN1 RX0 Interrupts       */
  CAN1_RX1_IRQn               = 21,     /*!< CAN1 RX1 Interrupt                                   */
  CAN1_SCE_IRQn               = 22,     /*!< CAN1 SCE Interrupt                                   */
  EXTI9_5_IRQn                = 23,     /*!< External Line[9:5] Interrupts                        */
  TIM1_BRK_IRQn               = 24,     /*!< TIM1 Break Interrupt                                 */
  TIM1_UP_IRQn                = 25,     /*!< TIM1 Update Interrupt                                */
  TIM1_TRG_COM_IRQn           = 26,     /*!< TIM1 Trigger and Commutation Interrupt               */
  TIM1_CC_IRQn                = 27,     /*!< TIM1 Capture Compare Interrupt                       */
  TIM2_IRQn                   = 28,     /*!< TIM2 global Interrupt                                */
  TIM3_IRQn                   = 29,     /*!< TIM3 global Interrupt                                */
  TIM4_IRQn                   = 30,     /*!< TIM4 global Interrupt                                */
  I2C1_EV_IRQn                = 31,     /*!< I2C1 Event Interrupt                                 */
  I2C1_ER_IRQn                = 32,     /*!< I2C1 Error Interrupt                                 */
  I2C2_EV_IRQn                = 33,     /*!< I2C2 Event Interrupt                                 */
  I2C2_ER_IRQn                = 34,     /*!< I2C2 Error Interrupt                                 */
  SPI1_IRQn                   = 35,     /*!< SPI1 global Interrupt                                */
  SPI2_IRQn                   = 36,     /*!< SPI2 global Interrupt                                */
  USART1_IRQn                 = 37,     /*!< USART1 global Interrupt                              */
  USART2_IRQn                 = 38,     /*!< USART2 global Interrupt                              */
  USART3_IRQn                 = 39,     /*!< USART3 global Interrupt                              */
  EXTI15_10_IRQn              = 40,     /*!< External Line[15:10] Interrupts                      */
  RTCAlarm_IRQn               = 41,     /*!< RTC Alarm through EXTI Line Interrupt                */
  USBWakeUp_IRQn              = 42      /*!< USB Device WakeUp from suspend through EXTI Line Interrupt */  
#endif /* STM32F10X_MD */  

#ifdef STM32F10X_MD_VL
  ADC1_IRQn                   = 18,     /*!< ADC1 global Interrupt                                */
  EXTI9_5_IRQn                = 23,     /*!< External Line[9:5] Interrupts                        */
  TIM1_BRK_TIM15_IRQn         = 24,     /*!< TIM1 Break and TIM15 Interrupts                      */
  TIM1_UP_TIM16_IRQn          = 25,     /*!< TIM1 Update and TIM16 Interrupts                     */
  TIM1_TRG_COM_TIM17_IRQn     = 26,     /*!< TIM1 Trigger and Commutation and TIM17 Interrupt     */
  TIM1_CC_IRQn                = 27,     /*!< TIM1 Capture Compare Interrupt                       */
  TIM2_IRQn                   = 28,     /*!< TIM2 global Interrupt                                */
  TIM3_IRQn                   = 29,     /*!< TIM3 global Interrupt                                */
  TIM4_IRQn                   = 30,     /*!< TIM4 global Interrupt                                */
  I2C1_EV_IRQn                = 31,     /*!< I2C1 Event Interrupt                                 */
  I2C1_ER_IRQn                = 32,     /*!< I2C1 Error Interrupt                                 */
  I2C2_EV_IRQn                = 33,     /*!< I2C2 Event Interrupt                                 */
  I2C2_ER_IRQn                = 34,     /*!< I2C2 Error Interrupt                                 */
  SPI1_IRQn                   = 35,     /*!< SPI1 global Interrupt                                */
  SPI2_IRQn                   = 36,     /*!< SPI2 global Interrupt                                */
  USART1_IRQn                 = 37,     /*!< USART1 global Interrupt                              */
  USART2_IRQn                 = 38,     /*!< USART2 global Interrupt                              */
  USART3_IRQn                 = 39,     /*!< USART3 global Interrupt                              */
  EXTI15_10_IRQn              = 40,     /*!< External Line[15:10] Interrupts                      */
  RTCAlarm_IRQn               = 41,     /*!< RTC Alarm through EXTI Line Interrupt                */
  CEC_IRQn                    = 42,     /*!< HDMI-CEC Interrupt                                   */
  TIM6_DAC_IRQn               = 54,     /*!< TIM6 and DAC underrun Interrupt                      */
  TIM7_IRQn                   = 55      /*!< TIM7 Interrupt                                       */       
#endif /* STM32F10X_MD_VL */

#ifdef STM32F10X_HD
  ADC1_2_IRQn                 = 18,     /*!< ADC1 and ADC2 global Interrupt                       */
  USB_HP_CAN1_TX_IRQn         = 19,     /*!< USB Device High Priority or CAN1 TX Interrupts       */
  USB_LP_CAN1_RX0_IRQn        = 20,     /*!< USB Device Low Priority or CAN1 RX0 Interrupts       */
  CAN1_RX1_IRQn               = 21,     /*!< CAN1 RX1 Interrupt                                   */
  CAN1_SCE_IRQn               = 22,     /*!< CAN1 SCE Interrupt                                   */
  EXTI9_5_IRQn                = 23,     /*!< External Line[9:5] Interrupts                        */
  TIM1_BRK_IRQn               = 24,     /*!< TIM1 Break Interrupt                                 */
  TIM1_UP_IRQn                = 25,     /*!< TIM1 Update Interrupt                                */
  TIM1_TRG_COM_IRQn           = 26,     /*!< TIM1 Trigger and Commutation Interrupt               */
  TIM1_IRQn                   = 27,     /*!< TIM1 Capture Compare Interrupt                       */
  TIM2_IRQn                   = 28,     /*!< TIM2 global Interrupt                                */
  TIM3_IRQn                   = 29,     /*!< TIM3 global Interrupt                                */
  TIM4_IRQn                   = 30,     /*!< TIM4 global Interrupt                                */
  I2C1_EV_IRQn                = 31,     /*!< I2C1 Event Interrupt                                 */
  I2C1_ER_IRQn                = 32,     /*!< I2C1 Error Interrupt                                 */
  I2C2_EV_IRQn                = 33,     /*!< I2C2 Event Interrupt                                 */
  I2C2_ER_IRQn                = 34,     /*!< I2C2 Error Interrupt                                 */
  SPI1_IRQn                   = 35,     /*!< SPI1 global Interrupt                                */
  SPI2_IRQn                   = 36,     /*!< SPI2 global Interrupt                                */
  USART1_IRQn                 = 37,     /*!< USART1 global Interrupt                              */
  USART2_IRQn                 = 38,     /*!< USART2 global Interrupt                              */
  USART3_IRQn                 = 39,     /*!< USART3 global Interrupt                              */
  EXTI15_10_IRQn              = 40,     /*!< External Line[15:10] Interrupts                      */
  RTCAlarm_IRQn               = 41,     /*!< RTC Alarm through EXTI Line Interrupt                */
  USBWakeUp_IRQn              = 42,     /*!< USB Device WakeUp from suspend through EXTI Line Interrupt */
  TIM8_BRK_IRQn               = 43,     /*!< TIM8 Break Interrupt                                 */
  TIM8_UP_IRQn                = 44,     /*!< TIM8 Update Interrupt                                */
  TIM8_TRG_COM_IRQn           = 45,     /*!< TIM8 Trigger and Commutation Interrupt               */
  TIM8_CC_IRQn                = 46,     /*!< TIM8 Capture Compare Interrupt                       */
  ADC3_IRQn                   = 47,     /*!< ADC3 global Interrupt                                */
  FSMC_IRQn                   = 48,     /*!< FSMC global Interrupt                                */
  SDIO_IRQn                   = 49,     /*!< SDIO global Interrupt                                */
  TIM5_IRQn                   = 50,     /*!< TIM5 global Interrupt                                */
  SPI3_IRQn                   = 51,     /*!< SPI3 global Interrupt                                */
  UART4_IRQn                  = 52,     /*!< UART4 global Interrupt                               */
  UART5_IRQn                  = 53,     /*!< UART5 global Interrupt                               */
  TIM6_IRQn                   = 54,     /*!< TIM6 global Interrupt                                */
  TIM7_IRQn                   = 55,     /*!< TIM7 global Interrupt                                */
  DMA2_Channel1_IRQn          = 56,     /*!< DMA2 Channel 1 global Interrupt                      */
  DMA2_Channel2_IRQn          = 57,     /*!< DMA2 Channel 2 global Interrupt                      */
  DMA2_Channel3_IRQn          = 58,     /*!< DMA2 Channel 3 global Interrupt                      */
  DMA2_Channel4_5_IRQn        = 59      /*!< DMA2 Channel 4 and Channel 5 global Interrupt        */
#endif /* STM32F10X_HD */  

#ifdef STM32F10X_HD_VL
  ADC1_IRQn                   = 18,     /*!< ADC1 global Interrupt                                */
  EXTI9_5_IRQn                = 23,     /*!< External Line[9:5] Interrupts                        */
  TIM1_BRK_TIM15_IRQn         = 24,     /*!< TIM1 Break and TIM15 Interrupts                      */
  TIM1_UP_TIM16_IRQn          = 25,     /*!< TIM1 Update and TIM16 Interrupts                     */
  TIM1_TRG_COM_TIM17_IRQn     = 26,     /*!< TIM1 Trigger and Commutation and TIM17 Interrupt     */
  TIM1_CC_IRQn                = 27,     /*!< TIM1 Capture Compare Interrupt                       */
  TIM2_IRQn                   = 28,     /*!< TIM2 global Interrupt                                */
  TIM3_IRQn                   = 29,     /*!< TIM3 global Interrupt                                */
  TIM4_IRQn                   = 30,     /*!< TIM4 global Interrupt                                */
  I2C1_EV_IRQn                = 31,     /*!< I2C1 Event Interrupt                                 */
  I2C1_ER_IRQn                = 32,     /*!< I2C1 Error Interrupt                                 */
  I2C2_EV_IRQn                = 33,     /*!< I2C2 Event Interrupt                                 */
  I2C2_ER_IRQn                = 34,     /*!< I2C2 Error Interrupt                                 */
  SPI1_IRQn                   = 35,     /*!< SPI1 global Interrupt                                */
  SPI2_IRQn                   = 36,     /*!< SPI2 global Interrupt                                */
  USART1_IRQn                 = 37,     /*!< USART1 global Interrupt                              */
  USART2_IRQn                 = 38,     /*!< USART2 global Interrupt                              */
  USART3_IRQn                 = 39,     /*!< USART3 global Interrupt                              */
  EXTI15_10_IRQn              = 40,     /*!< External Line[15:10] Interrupts                      */
  RTCAlarm_IRQn               = 41,     /*!< RTC Alarm through EXTI Line Interrupt                */
  CEC_IRQn                    = 42,     /*!< HDMI-CEC Interrupt                                   */
  TIM12_IRQn                  = 43,     /*!< TIM12 global Interrupt                               */
  TIM13_IRQn                  = 44,     /*!< TIM13 global Interrupt                               */
  TIM14_IRQn                  = 45,     /*!< TIM14 global Interrupt                               */
  TIM5_IRQn                   = 50,     /*!< TIM5 global Interrupt                                */
  SPI3_IRQn                   = 51,     /*!< SPI3 global Interrupt                                */
  UART4_IRQn                  = 52,     /*!< UART4 global Interrupt                               */
  UART5_IRQn                  = 53,     /*!< UART5 global Interrupt                               */  
  TIM6_DAC_IRQn               = 54,     /*!< TIM6 and DAC underrun Interrupt                      */
  TIM7_IRQn                   = 55,     /*!< TIM7 Interrupt                                       */  
  DMA2_Channel1_IRQn          = 56,     /*!< DMA2 Channel 1 global Interrupt                      */
  DMA2_Channel2_IRQn          = 57,     /*!< DMA2 Channel 2 global Interrupt                      */
  DMA2_Channel3_IRQn          = 58,     /*!< DMA2 Channel 3 global Interrupt                      */
  DMA2_Channel4_5_IRQn        = 59,     /*!< DMA2 Channel 4 and Channel 5 global Interrupt        */
  DMA2_Channel5_IRQn          = 60      /*!< DMA2 Channel 5 global Interrupt (DMA2 Channel 5 is 
                                             mapped at position 60 only if the MISC_REMAP bit in 
                                             the AFIO_MAPR2 register is set)                      */       
#endif /* STM32F10X_HD_VL */

#ifdef STM32F10X_XL
  ADC1_2_IRQn                 = 18,     /*!< ADC1 and ADC2 global Interrupt                       */
  USB_HP_CAN1_TX_IRQn         = 19,     /*!< USB Device High Priority or CAN1 TX Interrupts       */
  USB_LP_CAN1_RX0_IRQn        = 20,     /*!< USB Device Low Priority or CAN1 RX0 Interrupts       */
  CAN1_RX1_IRQn               = 21,     /*!< CAN1 RX1 Interrupt                                   */
  CAN1_SCE_IRQn               = 22,     /*!< CAN1 SCE Interrupt                                   */
  EXTI9_5_IRQn                = 23,     /*!< External Line[9:5] Interrupts                        */
  TIM1_BRK_TIM9_IRQn          = 24,     /*!< TIM1 Break Interrupt and TIM9 global Interrupt       */
  TIM1_UP_TIM10_IRQn          = 25,     /*!< TIM1 Update Interrupt and TIM10 global Interrupt     */
  TIM1_TRG_COM_TIM11_IRQn     = 26,     /*!< TIM1 Trigger and Commutation Interrupt and TIM11 global interrupt */
  TIM1_CC_IRQn                = 27,     /*!< TIM1 Capture Compare Interrupt                       */
  TIM2_IRQn                   = 28,     /*!< TIM2 global Interrupt                                */
  TIM3_IRQn                   = 29,     /*!< TIM3 global Interrupt                                */
  TIM4_IRQn                   = 30,     /*!< TIM4 global Interrupt                                */
  I2C1_EV_IRQn                = 31,     /*!< I2C1 Event Interrupt                                 */
  I2C1_ER_IRQn                = 32,     /*!< I2C1 Error Interrupt                                 */
  I2C2_EV_IRQn                = 33,     /*!< I2C2 Event Interrupt                                 */
  I2C2_ER_IRQn                = 34,     /*!< I2C2 Error Interrupt                                 */
  SPI1_IRQn                   = 35,     /*!< SPI1 global Interrupt                                */
  SPI2_IRQn                   = 36,     /*!< SPI2 global Interrupt                                */
  USART1_IRQn                 = 37,     /*!< USART1 global Interrupt                              */
  USART2_IRQn                 = 38,     /*!< USART2 global Interrupt                              */
  USART3_IRQn                 = 39,     /*!< USART3 global Interrupt                              */
  EXTI15_10_IRQn              = 40,     /*!< External Line[15:10] Interrupts                      */
  RTCAlarm_IRQn               = 41,     /*!< RTC Alarm through EXTI Line Interrupt                */
  USBWakeUp_IRQn              = 42,     /*!< USB Device WakeUp from suspend through EXTI Line Interrupt */
  TIM8_BRK_TIM12_IRQn         = 43,     /*!< TIM8 Break Interrupt and TIM12 global Interrupt      */
  TIM8_UP_TIM13_IRQn          = 44,     /*!< TIM8 Update Interrupt and TIM13 global Interrupt     */
  TIM8_TRG_COM_TIM14_IRQn     = 45,     /*!< TIM8 Trigger and Commutation Interrupt and TIM14 global interrupt */
  TIM8_CC_IRQn                = 46,     /*!< TIM8 Capture Compare Interrupt                       */
  ADC3_IRQn                   = 47,     /*!< ADC3 global Interrupt                                */
  FSMC_IRQn                   = 48,     /*!< FSMC global Interrupt                                */
  SDIO_IRQn                   = 49,     /*!< SDIO global Interrupt                                */
  TIM5_IRQn                   = 50,     /*!< TIM5 global Interrupt                                */
  SPI3_IRQn                   = 51,     /*!< SPI3 global Interrupt                                */
  UART4_IRQn                  = 52,     /*!< UART4 global Interrupt                               */
  UART5_IRQn                  = 53,     /*!< UART5 global Interrupt                               */
  TIM6_IRQn                   = 54,     /*!< TIM6 global Interrupt                                */
  TIM7_IRQn                   = 55,     /*!< TIM7 global Interrupt                                */
  DMA2_Channel1_IRQn          = 56,     /*!< DMA2 Channel 1 global Interrupt                      */
  DMA2_Channel2_IRQn          = 57,     /*!< DMA2 Channel 2 global Interrupt                      */
  DMA2_Channel3_IRQn          = 58,     /*!< DMA2 Channel 3 global Interrupt                      */
  DMA2_Channel4_5_IRQn        = 59      /*!< DMA2 Channel 4 and Channel 5 global Interrupt        */
#endif /* STM32F10X_XL */  

#ifdef STM32F10X_CL
  ADC1_2_IRQn                 = 18,     /*!< ADC1 and ADC2 global Interrupt                       */
  CAN1_TX_IRQn                = 19,     /*!< USB Device High Priority or CAN1 TX Interrupts       */
  CAN1_RX0_IRQn               = 20,     /*!< USB Device Low Priority or CAN1 RX0 Interrupts       */
  CAN1_RX1_IRQn               = 21,     /*!< CAN1 RX1 Interrupt                                   */
  CAN1_SCE_IRQn               = 22,     /*!< CAN1 SCE Interrupt                                   */
  EXTI9_5_IRQn                = 23,     /*!< External Line[9:5] Interrupts                        */
  TIM1_BRK_IRQn               = 24,     /*!< TIM1 Break Interrupt                                 */
  TIM1_UP_IRQn                = 25,     /*!< TIM1 Update Interrupt                                */
  TIM1_TRG_COM_IRQn           = 26,     /*!< TIM1 Trigger and Commutation Interrupt               */
  TIM1_CC_IRQn                = 27,     /*!< TIM1 Capture Compare Interrupt                       */
  TIM2_IRQn                   = 28,     /*!< TIM2 global Interrupt                                */
  TIM3_IRQn                   = 29,     /*!< TIM3 global Interrupt                                */
  TIM4_IRQn                   = 30,     /*!< TIM4 global Interrupt                                */
  I2C1_EV_IRQn                = 31,     /*!< I2C1 Event Interrupt                                 */
  I2C1_ER_IRQn                = 32,     /*!< I2C1 Error Interrupt                                 */
  I2C2_EV_IRQn                = 33,     /*!< I2C2 Event Interrupt                                 */
  I2C2_ER_IRQn                = 34,     /*!< I2C2 Error Interrupt                                 */
  SPI1_IRQn                   = 35,     /*!< SPI1 global Interrupt                                */
  SPI2_IRQn                   = 36,     /*!< SPI2 global Interrupt                                */
  USART1_IRQn                 = 37,     /*!< USART1 global Interrupt                              */
  USART2_IRQn                 = 38,     /*!< USART2 global Interrupt                              */
  USART3_IRQn                 = 39,     /*!< USART3 global Interrupt                              */
  EXTI15_10_IRQn              = 40,     /*!< External Line[15:10] Interrupts                      */
  RTCAlarm_IRQn               = 41,     /*!< RTC Alarm through EXTI Line Interrupt                */
  OTG_FS_WKUP_IRQn            = 42,     /*!< USB OTG FS WakeUp from suspend through EXTI Line Interrupt */
  TIM5_IRQn                   = 50,     /*!< TIM5 global Interrupt                                */
  SPI3_IRQn                   = 51,     /*!< SPI3 global Interrupt                                */
  UART4_IRQn                  = 52,     /*!< UART4 global Interrupt                               */
  UART5_IRQn                  = 53,     /*!< UART5 global Interrupt                               */
  TIM6_IRQn                   = 54,     /*!< TIM6 global Interrupt                                */
  TIM7_IRQn                   = 55,     /*!< TIM7 global Interrupt                                */
  DMA2_Channel1_IRQn          = 56,     /*!< DMA2 Channel 1 global Interrupt                      */
  DMA2_Channel2_IRQn          = 57,     /*!< DMA2 Channel 2 global Interrupt                      */
  DMA2_Channel3_IRQn          = 58,     /*!< DMA2 Channel 3 global Interrupt                      */
  DMA2_Channel4_IRQn          = 59,     /*!< DMA2 Channel 4 global Interrupt                      */
  DMA2_Channel5_IRQn          = 60,     /*!< DMA2 Channel 5 global Interrupt                      */
  ETH_IRQn                    = 61,     /*!< Ethernet global Interrupt                            */
  ETH_WKUP_IRQn               = 62,     /*!< Ethernet Wakeup through EXTI line Interrupt          */
  CAN2_TX_IRQn                = 63,     /*!< CAN2 TX Interrupt                                    */
  CAN2_RX0_IRQn               = 64,     /*!< CAN2 RX0 Interrupt                                   */
  CAN2_RX1_IRQn               = 65,     /*!< CAN2 RX1 Interrupt                                   */
  CAN2_SCE_IRQn               = 66,     /*!< CAN2 SCE Interrupt                                   */
  OTG_FS_IRQn                 = 67      /*!< USB OTG FS global Interrupt                          */
#endif /* STM32F10X_CL */     
} IRQn_Type;

/**
  * @}
  */

#include "core_cm3.h"
/**
* @brief 不需要这个，被我备注掉了 #include "system_stm32f10x.h" 
*/

#include <stdint.h>

#pragma anon_unions  // CRJ ADD :允许结构体和联合体中的匿名变量

/** @addtogroup Exported_types
  * @{
  */  

/*!< STM32F10x Standard Peripheral Library old types (maintained for legacy purpose) */
typedef int32_t  s32;
typedef int16_t s16;
typedef int8_t  s8;

typedef const int32_t sc32;  /*!< Read Only */
typedef const int16_t sc16;  /*!< Read Only */
typedef const int8_t sc8;   /*!< Read Only */

typedef __IO int32_t  vs32;
typedef __IO int16_t  vs16;
typedef __IO int8_t   vs8;

typedef __I int32_t vsc32;  /*!< Read Only */
typedef __I int16_t vsc16;  /*!< Read Only */
typedef __I int8_t vsc8;   /*!< Read Only */

typedef uint32_t  u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef const uint32_t uc32;  /*!< Read Only */
typedef const uint16_t uc16;  /*!< Read Only */
typedef const uint8_t uc8;   /*!< Read Only */

typedef __IO uint32_t  vu32;
typedef __IO uint16_t vu16;
typedef __IO uint8_t  vu8;

typedef __I uint32_t vuc32;  /*!< Read Only */
typedef __I uint16_t vuc16;  /*!< Read Only */
typedef __I uint8_t vuc8;   /*!< Read Only */

typedef enum {RESET = 0, SET = !RESET} FlagStatus, ITStatus;

typedef enum {DISABLE = 0, ENABLE = !DISABLE} FunctionalState;
#define IS_FUNCTIONAL_STATE(STATE) (((STATE) == DISABLE) || ((STATE) == ENABLE))

typedef enum {ERROR = 0, SUCCESS = !ERROR} ErrorStatus;

/**
* @Brief 包含头文件，主要是为了方便调用时钟，还有定义一些宏变量
* @Author CRJ QQ:43058655 WeChat:TC4300
*/

#include "binary.h"
#include "ClockConfigurationByCRJ.h"
#define SUCCESS  1
#define FAIL     0
#define TRUE     1
#define FALSE    0
#define ENABLE   1
#define DISABLE  0
#define PAI 3.1415926535897932384626
//#define FULL     1
//#define EMPTY    0

typedef enum 
{ // 中断使能仅仅也有两种状态
	DisableIrq = 0,
	EnableIrq
}IRQEnableType;

typedef enum  // 用枚举类型定义几个布尔变量，最大限度防止出错。
{
	bool_False = 0,
	bool_True = 1,
	bool_Fail = 0,
	bool_Success = 1,
	bool_Disable = 0,
	bool_Enable = 1,
}_bool;  // 不能直接用bool

/*!< STM32F10x Standard Peripheral Library old definitions (maintained for legacy purpose) */
#define HSEStartUp_TimeOut   HSE_STARTUP_TIMEOUT
#define HSE_Value            HSE_VALUE
#define HSI_Value            HSI_VALUE
/**
  * @}
  */

/** @addtogroup Peripheral_registers_structures
  * @{
  */   

/** 
  * @brief Analog to Digital Converter  
  * @Date 2016-7-22 by CRJ
  */
typedef union
{
	struct 
	{
		u32 AnalogWatchdogFlag_RCW0:1;
		u32 EndOfConversion_RCW0:1;
		u32 InjectedChannelEndOfConversion_RCW0:1;
		u32 InjectedChannelStartFlag_RCW0:1;
		u32 RegularChannelStartFlag_RCW0:1;
		u32 Reserved0:27;
	};
	u32 All;
}_ADC_Status;

typedef union
{
	struct 
	{
		u32 AnalogWatchdogChannelSelection_RW:5;
		u32 EOCInterruptEnable_RW:1;
		u32 AnalogWatchdogInterruptEnable_RW:1;
		u32 InjectedChannelsInterruptEnable_RW:1;
		u32 ScanModeEnable_RW:1;
		u32 WatchdogOnASingleChannelInScanMode_RW:1;
		u32 AutomaticInjectedGroupConversion_RW:1;
		u32 RegularChannelsDiscontinuousModeEnable_RW:1;
		u32 InjectedChannelsDiscontinuousModeEnable_RW:1;
		u32 DiscontinuousModeChannelCount_RW:3;
		u32 DualModeSelection_RW:4;
		u32 Reserved0:2;
		u32 InjectedChannelsAnalogWatchdogEnable_RW:1;
		u32 RegularChannelsAnalogWatchdogEnable_RW:1;
		u32 Reserved1:8;
	};
	u32 All;
}_ADC_Control_1;

typedef union
{
	struct 
	{
		u32 ADONEnable_RW:1;
		u32 ContinuousConversionEnable_RW:1;
		u32 ADCCalibration_RW:1;
		u32 ResetCalibrationEnable_RW:1;
		u32 Reserved0:4;
		u32 DMAModeEnable_RW:1;
		u32 Reserved1:2;
		u32 DataAlignmentLeftEnable_RW:1;
		u32 InjectedGroupExternnalEventSelect_RW:3;
		u32 InjectedChannelsExternalTriggerConversionMode_RW:1;
		u32 Reserved2:1;
		u32 RegularGroupExternalEventSelection_RW:3;
		u32 RegularChannelsExternalTriggerConversionMode_RW:1;
		u32 InjectedChannelsStartConversion_RW:1;
		u32 RegularChannelsStartConversion_RW:1;
		u32 TemperatureSensorAndVREFINTEnable_RW:1;
		u32 Reserved3:8;
	};
	u32 All;
}_ADC_Control_2;

typedef union
{
	struct 
	{
		u32 SampleTime10_RW:3;
		u32 SampleTime11_RW:3;
		u32 SampleTime12_RW:3;
		u32 SampleTime13_RW:3;
		u32 SampleTime14_RW:3;
		u32 SampleTime15_RW:3;
		u32 SampleTime16_RW:3;
		u32 SampleTime17_RW:3;
		u32 Reserved0:8;
	};
	u32 All;
}_ADC_SampleTime_1;

typedef union
{
	struct 
	{
		u32 SampleTime0_RW:3;
		u32 SampleTime1_RW:3;
		u32 SampleTime2_RW:3;		
		u32 SampleTime3_RW:3;
		u32 SampleTime4_RW:3;
		u32 SampleTime5_RW:3;
		u32 SampleTime6_RW:3;
		u32 SampleTime7_RW:3;
		u32 SampleTime8_RW:3;
		u32 SampleTime9_RW:3;
		u32 Reserved0:2;
	};
	u32 All;
}_ADC_SampleTime_2;

typedef union
{
	struct 
	{
		u32 InjectedChannelDataOffset_RW:12;
		u32 Reserved0:20;
	};
	u32 All;
}_ADC_InjectedChannelDataOffset;


typedef union
{
	struct 
	{
		u32 InjectedChannelDataOffset_RW:12;
		u32 Reserved0:20;
	};
	u32 All;
}_ADC_InjectedChannelDataOffset_4;


typedef union
{
	struct 
	{
		u32 AnalogWatchdogHighThreshold_RW:12;
		u32 Reserved0:20;
	};
	u32 All;
}_ADC_WatchdogHighThresHold;


typedef union
{
	struct 
	{
		u32 AnalogWatchdogLowThreshold_RW:12;
		u32 Reserved0:20;
	};
	u32 All;
}_ADC_WatchdogLowThresHold;


typedef union
{
	struct 
	{
		u32 RegularSequence13_RW:5;
		u32 RegularSequence14_RW:5;
		u32 RegularSequence15_RW:5;
		u32 RegularSequence16_RW:5;
		u32 RegularChannelSequenceLengthMinusOne_RW:4;
		u32 Reserved0:8;
	};
	u32 All;
}_ADC_RegularSequence_1;


typedef union
{
	struct 
	{
		u32 RegularSequence7_RW:5;
		u32 RegularSequence8_RW:5;
		u32 RegularSequence9_RW:5;
		u32 RegularSequence10_RW:5;
		u32 RegularSequence11_RW:5;
		u32 RegularSequence12_RW:5;
		u32 Reserved0:2;
	};
	u32 All;
}_ADC_RegularSequence_2;

typedef union
{
	struct 
	{
		u32 RegularSequence1_RW:5;
		u32 RegularSequence2_RW:5;
		u32 RegularSequence3_RW:5;
		u32 RegularSequence4_RW:5;
		u32 RegularSequence5_RW:5;
		u32 RegularSequence6_RW:5;
		u32 Reserved0:2;
	};
	u32 All;
}_ADC_RegularSequence_3;

typedef union
{
	struct 
	{
		u32 InjectedSequence1_RW:5;
		u32 InjectedSequence2_RW:5;
		u32 InjectedSequence3_RW:5;
		u32 InjectedSequence4_RW:5;
		u32 InjectedSequenceLength_RW:2;
		u32 Reserved0:10;
	};
	u32 All;
}_ADC_InjectedSequence;

typedef union
{
	struct 
	{
		u32 InjectedData_R:16;
		u32 Reserved0:16;
	};
	u32 All;
}_ADC_InjectedData;

typedef union
{
	struct 
	{
		u32 RegularData_R:16;
		u32 ADC2Data_R:16;
	};
	u32 All;
}_ADC_Data;


typedef struct
{
  __IO _ADC_Status SR;
  __IO _ADC_Control_1 CR1;
  __IO _ADC_Control_2 CR2;
  __IO _ADC_SampleTime_1 SMPR1;
  __IO _ADC_SampleTime_2 SMPR2;
  __IO _ADC_InjectedChannelDataOffset JOFR1;
  __IO _ADC_InjectedChannelDataOffset JOFR2;
  __IO _ADC_InjectedChannelDataOffset JOFR3;
  __IO _ADC_InjectedChannelDataOffset JOFR4;
  __IO _ADC_WatchdogHighThresHold HTR;
  __IO _ADC_WatchdogLowThresHold LTR;
  __IO _ADC_RegularSequence_1 SQR1;
  __IO _ADC_RegularSequence_2 SQR2;
  __IO _ADC_RegularSequence_3 SQR3;
  __IO _ADC_InjectedSequence JSQR;
  __IO _ADC_InjectedData JDR1;
  __IO _ADC_InjectedData JDR2;
  __IO _ADC_InjectedData JDR3;
  __IO _ADC_InjectedData JDR4;
  __IO _ADC_Data DR;
} ADC_TypeDef;

/** 
  * @brief Backup Registers  
  * @attention 为了使程序通用性高,一定要带上all。即使存在相同功能的变量
  */
typedef union  
{
	u16 BackupData_RW;
}_BKP_Data;

typedef union
{
	struct 
	{
		u16 CalibrationValue_RW:7;
		u16 CalibrationClockOutput_RW:1;
		u16 AlarmOrSecondOutputEnable_RW:1;
		u16 AlarmOrSecondOutputSelection_RW:1;
		u16 Reserved0:6;
	};
	u16 All;
}_BKP_RTCControl;

typedef union
{
	struct 
	{
		u16 TAMPERPinEanble_RW:1;
		u16 TAMPERPinActiveLevel_RW:1;
		u16 Reserved0:14;
	};
	u16 All;
}_BKP_Control;

typedef union
{
	struct 
	{
		u16 ClearTamperEvent_RW:1;
		u16 ClearTamperInterrupt_RW:1;
		u16 TAMPERPinInterruptEnable_RW:1;
		u16 Reserved0:5;
		u16 TamperEventFlag_R:1;
		u16 TamperInterruptFlag_R:1;
		u16 Reserved1:6;
	};
	u16 All;
}_BKP_ControlAndStatus;

typedef struct
{
  u32  RESERVED0;
  __IO _BKP_Data DR1;
  u16  RESERVED1;
  __IO _BKP_Data DR2;
  u16  RESERVED2;
  __IO _BKP_Data DR3;
  u16  RESERVED3;
  __IO _BKP_Data DR4;
  u16  RESERVED4;
  __IO _BKP_Data DR5;
  u16  RESERVED5;
  __IO _BKP_Data DR6;
  u16  RESERVED6;
  __IO _BKP_Data DR7;
  u16  RESERVED7;
  __IO _BKP_Data DR8;
  u16  RESERVED8;
  __IO _BKP_Data DR9;
  u16  RESERVED9;
  __IO _BKP_Data DR10;
  u16  RESERVED10; 
  __IO _BKP_RTCControl RTCCR;
  u16  RESERVED11;
  __IO _BKP_Control CR;
  u16  RESERVED12;
  __IO _BKP_ControlAndStatus CSR;
  u16  RESERVED13[5];
  __IO _BKP_Data DR11;
  u16  RESERVED14;
  __IO _BKP_Data DR12;
  u16  RESERVED15;
  __IO _BKP_Data DR13;
  u16  RESERVED16;
  __IO _BKP_Data DR14;
  u16  RESERVED17;
  __IO _BKP_Data DR15;
  u16  RESERVED18;
  __IO _BKP_Data DR16;
  u16  RESERVED19;
  __IO _BKP_Data DR17;
  u16  RESERVED20;
  __IO _BKP_Data DR18;
  u16  RESERVED21;
  __IO _BKP_Data DR19;
  u16  RESERVED22;
  __IO _BKP_Data DR20;
  u16  RESERVED23;
  __IO _BKP_Data DR21;
  u16  RESERVED24;
  __IO _BKP_Data DR22;
  u16  RESERVED25;
  __IO _BKP_Data DR23;
  u16  RESERVED26;
  __IO _BKP_Data DR24;
  u16  RESERVED27;
  __IO _BKP_Data DR25;
  u16  RESERVED28;
  __IO _BKP_Data DR26;
  u16  RESERVED29;
  __IO _BKP_Data DR27;
  u16  RESERVED30;
  __IO _BKP_Data DR28;
  u16  RESERVED31;
  __IO _BKP_Data DR29;
  u16  RESERVED32;
  __IO _BKP_Data DR30;
  u16  RESERVED33; 
  __IO _BKP_Data DR31;
  u16  RESERVED34;
  __IO _BKP_Data DR32;
  u16  RESERVED35;
  __IO _BKP_Data DR33;
  u16  RESERVED36;
  __IO _BKP_Data DR34;
  u16  RESERVED37;
  __IO _BKP_Data DR35;
  u16  RESERVED38;
  __IO _BKP_Data DR36;
  u16  RESERVED39;
  __IO _BKP_Data DR37;
  u16  RESERVED40;
  __IO _BKP_Data DR38;
  u16  RESERVED41;
  __IO _BKP_Data DR39;
  u16  RESERVED42;
  __IO _BKP_Data DR40;
  u16  RESERVED43;
  __IO _BKP_Data DR41;
  u16  RESERVED44;
  __IO _BKP_Data DR42;
  u16  RESERVED45;    
} BKP_TypeDef;
  
/** 
  * @brief Controller Area Network TxMailBox 
  */
typedef union
{
	struct
	{
		u32 TransmitMailboxRequest_RW:1;
		u32 RemoteTransmissionRequest_RW:1;
		u32 ExtendedIdentifierFormatEnable_RW:1;
		u32 Reserved0:29;
	};
	u32 All;
}_CAN_TX_MailboxIdentifier;

typedef union
{
	struct
	{
		u32 TransmitMailboxRequest_RW:1;
		u32 RemoteTransmissionRequest_RW:1;     // 0:数据帧 1：远程帧
		u32 ExtendedIdentifierFormatEnable_RW:1;
		u32 Reserved0:18;
		u32 StandIdentifier_RW:11;
	};
	u32 All;
}_CAN_TX_MailboxIdentifier_Stand;

typedef union
{
	struct
	{
		u32 TransmitMailboxRequest_RW:1;
		u32 RemoteTransmissionRequest_RW:1;
		u32 ExtendedIdentifierFormatEnable_RW:1;
		u32 ExtendedIdentifier_RW:29;
	};
	u32 All;
}_CAN_TX_MailboxIdentifier_Extend;


typedef union
{
	struct
	{
		u32 DataLength_RW:4;
		u32 Reserved0:1;
		u32 TransmitGlobalTime_RW:1;
		u32 Reserved1:7;
		u32 MessageTimeStamp_RW:16;
	};
	u32 All;
}_CAN_MailboxDataLength_Control_And_TimeStamp;

typedef union
{
	struct
	{
		u32 DataByte0:8;
		u32 DataByte1:8;
		u32 DataByte2:8;
		u32 DataByte3:8;
	};
	u32 All;
}_CAN_Mailbox_Data_Low;

typedef union
{
	struct
	{
		u32 DataByte4:8;
		u32 DataByte5:8;
		u32 DataByte6:8;
		u32 DataByte7:8;
	};
	u32 All;
}_CAN_Mailbox_Data_High;

typedef struct
{
	union {    // CRJ ADD 下面两个联合体，主要作用就是可快速写入标准帧或扩展帧地址
  __IO _CAN_TX_MailboxIdentifier_Stand  TIR_Stand;   // 用于标准帧的控制
	__IO _CAN_TX_MailboxIdentifier_Extend TIR_Extend;  // 用于扩展帧的控制
	__IO _CAN_TX_MailboxIdentifier        TIR;         // 用于防止下面两个名字的误导！
		};
  __IO _CAN_MailboxDataLength_Control_And_TimeStamp TDTR;
  __IO _CAN_Mailbox_Data_Low TDLR;
  __IO _CAN_Mailbox_Data_High TDHR;
} CAN_TxMailBox_TypeDef;

/** 
  * @brief Controller Area Network FIFOMailBox 
  */
  

typedef union
{
	struct
	{
		u32 Reserved0:1;
		u32 RemoteTransmissionRequest_R:1;  // 0:数据帧    1:远程帧
		u32 ExtendedIdentifierFormat_R:1;   // 0:标准标识  1:扩展标识
		u32 Reserved1:18;
		u32 StandardIdentifier_R:11;
	};
	u32 All;
}_CAN_RX_Identifier_Stand;

typedef union
{
	struct
	{
		u32 Reserved0:1;
		u32 RemoteTransmissionRequest_R:1;
		u32 ExtendedIdentifierFormat_R:1;
		u32 ExtendedIdentifier_R:29;
	};
	u32 All;
}_CAN_RX_Identifier_Extend;

typedef union
{
	struct
	{
		u32 Reserved0:1;
		u32 RemoteTransmissionRequest_R:1;
		u32 ExtendedIdentifierFormat_R:1;
		u32 Reserved1:29;
	};
	u32 All;
}_CAN_RX_Identifier;

typedef union
{
	struct
	{
		u32 DataLength_R:4;
		u32 Resered0:4;
		u32 FilterMatchIndex_R:8;
		u32 MessageTimeStamp_R:16;
	};
	u32 All;
}_CAN_RX_MailboxDataLength_Control_And_TimeStamp;

typedef union
{
	struct
	{
		u32 DataByte0:8;
		u32 DataByte1:8;
		u32 DataByte2:8;
		u32 DataByte3:8;
	};
	u32 All;
}_CAN_RX_Mailbox_Data_Low;

typedef union
{
	struct
	{
		u32 DataByte4:8;
		u32 DataByte5:8;
		u32 DataByte6:8;
		u32 DataByte7:8;
	};
	u32 All;
}_CAN_RX_Mailbox_Data_High;

typedef struct
{
	union {  // 用联合体方式方便管理
  __IO _CAN_RX_Identifier_Stand RIR_Stand;
	__IO _CAN_RX_Identifier_Extend RIR_Extend;
	__IO _CAN_RX_Identifier RIR;
	};
  __IO _CAN_RX_MailboxDataLength_Control_And_TimeStamp RDTR;
  __IO _CAN_RX_Mailbox_Data_Low RDLR;
  __IO _CAN_RX_Mailbox_Data_High RDHR;
} CAN_FIFOMailBox_TypeDef;

/** 
  * @brief Controller Area Network FilterRegister 
  */
  
typedef union
{
	struct
	{
		u32 FilterBit0_RW:1;
		u32 FilterBit1_RW:1;
		u32 FilterBit2_RW:1;
		u32 FilterBit3_RW:1;
		u32 FilterBit4_RW:1;
		u32 FilterBit5_RW:1;
		u32 FilterBit6_RW:1;
		u32 FilterBit7_RW:1;
		u32 FilterBit8_RW:1;
		u32 FilterBit9_RW:1;
		u32 FilterBit10_RW:1;
		u32 FilterBit11_RW:1;
		u32 FilterBit12_RW:1;
		u32 FilterBit13_RW:1;
		u32 FilterBit14_RW:1;
		u32 FilterBit15_RW:1;
		u32 FilterBit16_RW:1;
		u32 FilterBit17_RW:1;
		u32 FilterBit18_RW:1;
		u32 FilterBit19_RW:1;
		u32 FilterBit20_RW:1;
		u32 FilterBit21_RW:1;
		u32 FilterBit22_RW:1;
		u32 FilterBit23_RW:1;
		u32 FilterBit24_RW:1;
		u32 FilterBit25_RW:1;
		u32 FilterBit26_RW:1;
		u32 FilterBit27_RW:1;
		u32 FilterBit28_RW:1;
		u32 FilterBit29_RW:1;
		u32 FilterBit30_RW:1;
		u32 FilterBit31_RW:1;
	};
	u32 All;
}_CAN_Filterbank;

typedef struct
{
  __IO _CAN_Filterbank FR1;
  __IO _CAN_Filterbank FR2;
} CAN_FilterRegister_TypeDef;

/** 
  * @brief Controller Area Network 
  */
  
typedef union
{
	struct
	{
		u32 InitRequsetEnable_RW:1;   // 0: 1:请求进入初始化模式
		u32 SleepModeEnable_RW:1;    
		u32 TransmitFIFOPriorityMode_RW:1;  // 发送顺序  0:标识符 1:请求顺序
		u32 RecvFIFOLockedMode_RW:1;        // 接收溢出处理  0:覆盖报文  1:丢弃
		u32 ForbidAutomaticRetransmission_RW:1;  
		u32 AutomaticWakeupMode_RW:1;   
		u32 AutomaticBusOffManagement_RW:1;  //0:对INRQ写1后清零  1:只要检测到128次11位连续隐性位
		u32 TimeTriggerCommunicationModeEnable_RW:1;
		u32 Reserved0:7;
		u32 SoftwareMasterReset_RS:1;  // 1: 强行复位CAN总线，后自动清零进入睡眠模式
		u32 DebugFreezeEnable:1;  // 1:调试时进入冻结模式
		u32 Reserved1:15;
	};
	u32 All;
}_CAN_Master_Control;

typedef union
{
	struct
	{ 
		u32 InitAcknowledge_R:1;    // 初始化模式确认
		u32 SleepAcknowledge_R:1;   // 睡眠模式确认
		u32 ErrorInterrupt_RCW1:1;  // 错误中断
		u32 WakeupInterrupt_RCW1:1;
		u32 SleepAcknowledgeInterrupt_RCW1:1;  // 睡眠确认
		u32 Reserved0:3;
		u32 TransmitMode_R:1;
		u32 RecvMode_R:1;
		u32 LastSamplePoint_R:1;
		u32 RxSignal_R:1;
		u32 Reserved1:20;
	};
	u32 All;
}_CAN_Master_Status;

typedef union
{
	struct
	{
		u32 RequestCompletedOfMailbox0_RCW1:1;  // 邮箱请求完成(发送或中止)
		u32 TransmissionOKOfMailbox0_RCW1:1;    // 1:上次发送成功
		u32 ArbitrationLostOfMailbox0_RCW1:1;   // 1:因为丢失仲裁导致发送失败
		u32 TransmissionErrorOfMailbox0_RCW1:1; // 1:发送出错导致失败
		u32 Reserved0:3;
		u32 AbortRequestOfMailbox0_RS:1;        // 1:中止报文发送
		
		u32 RequestCompletedOfMailbox1_RCW1:1;
		u32 TransmissionOKOfMailbox1_RCW1:1;
		u32 ArbitrationLostOfMailbox1_RCW1:1;
		u32 TransmissionErrorOfMailbox1_RCW1:1;
		u32 Reserved1:3;
		u32 AbortRequestOfMailbox1_RS:1;
		
		u32 RequestCompletedOfMailbox2_RCW1:1;
		u32 TransmissionOKOfMailbox2_RCW1:1;
		u32 ArbitrationLostOfMailbox2_RCW1:1;
		u32 TransmissionErrorOfMailbox2_RCW1:1;
		u32 Reserved2:3;
		u32 AbortRequestOfMailbox2_RS:1;
		
		u32 MailboxEmptyIndex_R:2;
		u32 TransmitMailboxEmpty0_R:1;
		u32 TransmitMailboxEmpty1_R:1;
		u32 TransmitMailboxEmpty2_R:1;
		u32 LowestPriorityOfMailbox0_R:1;
		u32 LowestPriorityOfMailbox1_R:1;
		u32 LowestPriorityOfMailbox2_R:1;
	};
	u32 All;
}_CAN_Transmit_Status;

typedef union
{
	struct
	{
		u32 FIFOMessagePendingNumbers_R:2; // 当前FIFO接收报文数量
		u32 Reserved0:1;
		u32 FIFOFull_RCW1:1;               // 当前FIFO有三个报文时,置位
		u32 FIFOOverrun_RCW1_RCW1:1;       //
		u32 RelesaeFIFOOutputMailbox_RS:1; // 每次只能释放一个
		u32 Reserved1:26;
	};
	u32 All;
}_CAN_Recv_FIFO_Control;

typedef union
{
	struct
	{
		u32 TransmitMailboxEmptyInterruptEnable_RW:1;  // 发送邮箱空中断
		u32 FIFOMessagePendingInterruptEnable0_RW:1;   // FIFO0FMP不为0时中断，接收中断
		u32 FIFOFullInterruptEnable0_RW:1;             // FIFO0满中断使能
		u32 FIFOOverrunInterruptEnable0_RW:1;          // FIFO1溢出中断使能
		u32 FIFOMessagePendingInterruptEnable1_RW:1;
		u32 FIFOFullInterruptEnable1_RW:1;
		u32 FIFOOverrunInterruptEnable1_RW:1;
		u32 Reserved0:1;
		u32 ErrorWarningInterruptEnable_RW:1;          // 当EWGF位置位时,设置ERRI位产生中断
		u32 ErrorPassiveInterruptEnable_RW:1;          // EPVF
		u32 BusOffInterruptEnable_RW:1;                // BOFF
		u32 LastErrorCodeInterruptEnable_RW:1;         // LEC
		u32 Reserved1:3;
		u32 ErrorInterruptEnable_RW:1;                 //当CAN_ESR有错误挂号时,产生中断
		u32 WakeupInterruptEnable_RW:1;   // 当WUKI置位时,产生中断
		u32 SleepInterruptEnable_RW:1;    // 当SLAKI置位时,产生中断
		u32 Reserved2:14;
	};
	u32 All;
}_CAN_Interrupt_Enable;

typedef union
{
	struct
	{
		u32 ErrorWarningFlag_R:1;  // 接收或发送错误计数器>=96
		u32 ErrorPassiveFlag_R:1;  // 接收或发送错误计数器>=127
		u32 BusOffFlag_R:1;        // 当发送错误计数器>=255时,进入离线状态
		u32 Reserved0:1;
		u32 LastErrorCode_RW:3;    // 上次错误代码
		u32 Reserved1:9;
		u32 LowEightOfNineBitTransmitErrorCount_R:8; // 发送错误计数器
		u32 RecvErrorCounter_R:8;  // 接收错误计数器
	};
	u32 All;
}_CAN_Error_Status;


typedef union
{
	struct
	{
		u32 BaudratePrescale_RW:10;
		u32 Reserved0:6;
		u32 TimeSegment1_RW:4;
		u32 TimeSegment2_RW:3;
		u32 Reserved1:1;
		u32 ResynchronizationJumpWidth_RW:2;
		u32 Reserved2:4;
		u32 LoopBackModeForDebugEnable_RW:1;
		u32 SilentModeForDebugEnable_RW:1;
	};
	u32 All;
}_CAN_BitTimeing;

typedef union
{
	struct
	{
		u32 FilterInitModeEnable_RW:1;  // 0:工作在正常模式  1:工作在初始化模式
		u32 Reserved0:7;
		u32 CAN2StartBank_RW:6;
		u32 Reserved1:18;
	};
	u32 All;
}_CAN_Filter_Master;

typedef union
{
	struct
	{
		u32 FilterMode0_RW:1; // 0:屏蔽位模式  1:标识符模式
		u32 FilterMode1_RW:1;
		u32 FilterMode2_RW:1;
		u32 FilterMode3_RW:1;
		u32 FilterMode4_RW:1;
		u32 FilterMode5_RW:1;
		u32 FilterMode6_RW:1;
		u32 FilterMode7_RW:1;
		u32 FilterMode8_RW:1;
		u32 FilterMode9_RW:1;
		u32 FilterMode10_RW:1;
		u32 FilterMode11_RW:1;
		u32 FilterMode12_RW:1;
		u32 FilterMode13_RW:1;
		u32 FilterMode14_RW:1;
		u32 FilterMode15_RW:1;
		u32 FilterMode16_RW:1;
		u32 FilterMode17_RW:1;
		u32 FilterMode18_RW:1;
		u32 FilterMode19_RW:1;
		u32 FilterMode20_RW:1;
		u32 FilterMode21_RW:1;
		u32 FilterMode22_RW:1;
		u32 FilterMode23_RW:1;
		u32 FilterMode24_RW:1;
		u32 FilterMode25_RW:1;
		u32 FilterMode26_RW:1;
		u32 FilterMode27_RW:1;
		u32 Reserved0:4;
	};
	u32 All;
}_CAN_FilterMode;

typedef union
{
	struct
	{
		u32 FilterLength0_RW:1;  // 0:两个16位宽  1:单个32位宽
		u32 FilterLength1_RW:1;
		u32 FilterLength2_RW:1;
		u32 FilterLength3_RW:1;
		u32 FilterLength4_RW:1;
		u32 FilterLength5_RW:1;
		u32 FilterLength6_RW:1;
		u32 FilterLength7_RW:1;
		u32 FilterLength8_RW:1;
		u32 FilterLength9_RW:1;
		u32 FilterLength10_RW:1;
		u32 FilterLength11_RW:1;
		u32 FilterLength12_RW:1;
		u32 FilterLength13_RW:1;
		u32 FilterLength14_RW:1;
		u32 FilterLength15_RW:1;
		u32 FilterLength16_RW:1;
		u32 FilterLength17_RW:1;
		u32 FilterLength18_RW:1;
		u32 FilterLength19_RW:1;
		u32 FilterLength20_RW:1;
		u32 FilterLength21_RW:1;
		u32 FilterLength22_RW:1;
		u32 FilterLength23_RW:1;
		u32 FilterLength24_RW:1;
		u32 FilterLength25_RW:1;
		u32 FilterLength26_RW:1;
		u32 FilterLength27_RW:1;
		u32 Reserved0:4;
	};
	u32 All;
}_CAN_Filter_Scale;

typedef union
{
	struct
	{
		u32 FilterFIFOAssigment0_RW:1;  // 0: 过滤器关联到FIFO0 1:FIFO1
		u32 FilterFIFOAssigment1_RW:1;
		u32 FilterFIFOAssigment2_RW:1;
		u32 FilterFIFOAssigment3_RW:1;
		u32 FilterFIFOAssigment4_RW:1;
		u32 FilterFIFOAssigment5_RW:1;
		u32 FilterFIFOAssigment6_RW:1;
		u32 FilterFIFOAssigment7_RW:1;
		u32 FilterFIFOAssigment8_RW:1;
		u32 FilterFIFOAssigment9_RW:1;
		u32 FilterFIFOAssigment10_RW:1;
		u32 FilterFIFOAssigment11_RW:1;
		u32 FilterFIFOAssigment12_RW:1;
		u32 FilterFIFOAssigment13_RW:1;
		u32 FilterFIFOAssigment14_RW:1;
		u32 FilterFIFOAssigment15_RW:1;
		u32 FilterFIFOAssigment16_RW:1;
		u32 FilterFIFOAssigment17_RW:1;
		u32 FilterFIFOAssigment18_RW:1;
		u32 FilterFIFOAssigment19_RW:1;
		u32 FilterFIFOAssigment20_RW:1;
		u32 FilterFIFOAssigment21_RW:1;
		u32 FilterFIFOAssigment22_RW:1;
		u32 FilterFIFOAssigment23_RW:1;
		u32 FilterFIFOAssigment24_RW:1;
		u32 FilterFIFOAssigment25_RW:1;
		u32 FilterFIFOAssigment26_RW:1;
		u32 FilterFIFOAssigment27_RW:1;
		u32 Reserved0:4;
	};
	u32 All;
}_CAN_Filter_FIFO_Assignment;

typedef union
{
	struct
	{
		u32 FilterActiveEnable0_RW:1;  
		u32 FilterActiveEnable1_RW:1;
		u32 FilterActiveEnable2_RW:1;
		u32 FilterActiveEnable3_RW:1;
		u32 FilterActiveEnable4_RW:1;
		u32 FilterActiveEnable5_RW:1;
		u32 FilterActiveEnable6_RW:1;
		u32 FilterActiveEnable7_RW:1;
		u32 FilterActiveEnable8_RW:1;
		u32 FilterActiveEnable9_RW:1;
		u32 FilterActiveEnable10_RW:1;
		u32 FilterActiveEnable11_RW:1;
		u32 FilterActiveEnable12_RW:1;
		u32 FilterActiveEnable13_RW:1;
		u32 FilterActiveEnable14_RW:1;
		u32 FilterActiveEnable15_RW:1;
		u32 FilterActiveEnable16_RW:1;
		u32 FilterActiveEnable17_RW:1;
		u32 FilterActiveEnable18_RW:1;
		u32 FilterActiveEnable19_RW:1;
		u32 FilterActiveEnable20_RW:1;
		u32 FilterActiveEnable21_RW:1;
		u32 FilterActiveEnable22_RW:1;
		u32 FilterActiveEnable23_RW:1;
		u32 FilterActiveEnable24_RW:1;
		u32 FilterActiveEnable25_RW:1;
		u32 FilterActiveEnable26_RW:1;
		u32 FilterActiveEnable27_RW:1;
		u32 Reserved0:4;
	};
	u32 All;
}_CAN_Filter_Activation;


typedef struct
{
  __IO _CAN_Master_Control MCR;
  __IO _CAN_Master_Status MSR;
  __IO _CAN_Transmit_Status TSR;
  __IO _CAN_Recv_FIFO_Control RF0R;
  __IO _CAN_Recv_FIFO_Control RF1R;
  __IO _CAN_Interrupt_Enable IER;
  __IO _CAN_Error_Status ESR;
  __IO _CAN_BitTimeing BTR;
  u32  RESERVED0[88];
  CAN_TxMailBox_TypeDef sTxMailBox[3];
  CAN_FIFOMailBox_TypeDef sFIFOMailBox[2];
  u32  RESERVED1[12];
  __IO _CAN_Filter_Master FMR;
  __IO _CAN_FilterMode FM1R;
  u32  RESERVED2;
  __IO _CAN_Filter_Scale FS1R;
  u32  RESERVED3;
  __IO _CAN_Filter_FIFO_Assignment FFA1R;
  u32  RESERVED4;
  __IO _CAN_Filter_Activation FA1R;
  u32  RESERVED5[8];
#ifndef STM32F10X_CL
  CAN_FilterRegister_TypeDef sFilterRegister[14];
#else
  CAN_FilterRegister_TypeDef sFilterRegister[28];
#endif /* STM32F10X_CL */  
} CAN_TypeDef;

/** 
  * @brief Consumer Electronics Control (CEC)
  */
typedef struct
{
  __IO uint32_t CFGR;
  __IO uint32_t OAR;
  __IO uint32_t PRES;
  __IO uint32_t ESR;
  __IO uint32_t CSR;
  __IO uint32_t TXD;
  __IO uint32_t RXD;  
} CEC_TypeDef;

/** 
  * @brief CRC calculation unit 
  * @data 26 July 2016
  */
typedef union
{
	u32 Data_RW;
}_CRC_Data;

typedef union
{
	u8 IndependentData_RW;
}_CRC_IndependentData;

typedef union
{
	struct 
	{
		u32 Reset_W:1;
		u32 Reserved0:31;
	};
	u32 All;
}_CRC_Control;

typedef struct
{
  __IO _CRC_Data DR;
  __IO _CRC_IndependentData  IDR;
  u8   RESERVED0;
  u16  RESERVED1;
  __IO _CRC_Control CR;
} CRC_TypeDef;

/** 
  * @brief Digital to Analog Converter
  * @Data 2016-7-22 By CRJ
  */

typedef union
{
	struct 
	{
		u32 DACEnable1_RW:1;
		u32 OutputBufferDisable1_RW:1;
		u32 TriggerEnable1_RW:1;
		u32 TriggerSelecion1_RW:3;
		u32 NoiseTriangleWaveGeneration1_RW:2;
		u32 MaskAmplitudeSelector1_RW:4;
		u32 DMAEnable1_RW:1;
		u32 Reserved0:3;
		u32 DACEnable2_RW:1;
		u32 OutputBufferDisable2_RW:1;
		u32 TriggerEnable2_RW:1;
		u32 TriggerSelecion2_RW:3;
		u32 NoiseTriangleWaveGeneration2_RW:2;
		u32 MaskAmplitudeSelector2_RW:4;
		u32 DMAEnable2_RW:1;
		u32 Reserved1:3;
	};
	u32 All;
}_DAC_Control;

typedef union
{
	struct 
	{
		u32 SoftwareTrigger1_RW:1;
		u32 SoftwareTrigger2_RW:1;
		u32 Reserved0:30;
	};
	u32 All;
}_DAC_SoftwareTrigger;


typedef union
{
	struct 
	{
		u32 TwelveBitsRightAlignedData_RW:12;
		u32 Reserved0:20;
	};
	u32 All;
}_DAC_TwelveBitsRightAlignedDataHolding;

typedef union
{
	struct 
	{
		u32 Reserved0:4;
		u32 TwelveBitsLeftAlignedData_RW:12;
		u32 Reserved1:16;
	};
	u32 All;
}_DAC_TwelveBitsLeftAlignedDataHolding;

typedef union
{
	struct 
	{
		u32 EightBitsRightAlignedData_RW:8;
		u32 Reserved0:24;
	};
	u32 All;
}_DAC_EightBitsRightAlignedDataHolding;

typedef union
{
	struct 
	{
		u32 TwelveBitsRightAlignedData1_RW:12;
		u32 Reserved0:4;
		u32 TwelveBitsRightAlignedData2_RW:12;
		u32 Reserved1:4;
	};
	u32 All;
}_DAC_TwelveBitsRightAlignedDataHolding_Double;


typedef union
{
	struct 
	{
		u32 Reserved0:4;
		u32 TwelveBitsLeftAlignedData1_RW:12;
		u32 Reserved1:4;
		u32 TwelveBitsLeftAlignedData2_RW:12;
	};
	u32 All;
}_DAC_TwelveBitsLeftAlignedDataHolding_Double;

typedef union
{
	struct 
	{
		u32 EightBitsRightAlignedData1_RW:8;
		u32 EightBitsRightAlignedData2_RW:8;
		u32 Reserved0:16;
	};
	u32 All;
}_DAC_EightBitsRightAlignedDataHolding_Double;

typedef union
{
	struct 
	{
		u32 DataOutput_RW:12;
		u32 Reserved0:20;
	};
	u32 All;
}_DAC_DataOutput;


typedef struct
{
  __IO _DAC_Control CR;
  __IO _DAC_SoftwareTrigger SWTRIGR;
  __IO _DAC_TwelveBitsRightAlignedDataHolding DHR12R1;
  __IO _DAC_TwelveBitsLeftAlignedDataHolding DHR12L1;
  __IO _DAC_EightBitsRightAlignedDataHolding DHR8R1;
  __IO _DAC_TwelveBitsRightAlignedDataHolding DHR12R2;
  __IO _DAC_TwelveBitsLeftAlignedDataHolding DHR12L2;
  __IO _DAC_TwelveBitsLeftAlignedDataHolding DHR8R2;
  __IO _DAC_TwelveBitsRightAlignedDataHolding_Double DHR12RD;
  __IO _DAC_TwelveBitsLeftAlignedDataHolding_Double DHR12LD;
  __IO _DAC_EightBitsRightAlignedDataHolding_Double DHR8RD;
  __IO _DAC_DataOutput DOR1;
  __IO _DAC_DataOutput DOR2;
#if defined (STM32F10X_LD_VL) || defined (STM32F10X_MD_VL) || defined (STM32F10X_HD_VL)
  __IO uint32_t SR;
#endif
} DAC_TypeDef;

/** 
  * @brief Debug MCU
  * @data 26 July 2016
  */

typedef union
{
	struct 
	{
		u32 RevisionID_R:16;
		u32 Reserved0:4;
		u32 DeviceID_R:12;
	};
	u32 All;
}_DEBUGMCU_IDCode;

typedef union
{
	struct 
	{
		u32	DebugSleep_RW:1;
		u32	DebugStop_RW:1;
		u32	DebugStandy_RW:1;
		u32 Reserved0:2;
		u32	TranceIOEnable_RW:1;
		u32	TraceMode_RW:2;
		u32	IndependentWatchdogStopEnable_RW:1;
		u32	WindowsWatchdogStopEnable_RW:1;
		u32	TimerStopEnable1_RW:1;
		u32	TimerStopEnable2_RW:1;
		u32	TimerStopEnable3_RW:1;
		u32	TimerStopEnable4_RW:1;
		u32	CANStopEnable1_RW:1;
		u32	I2C1_SMBUS_Timerout_RW:1;
		u32	I2C2_SMBUS_Timerout_RW:1;
		u32	TimerStopEnable5_RW:1;
		u32	TimerStopEnable6_RW:1;
		u32	TimerStopEnable7_RW:1;
		u32	TimerStopEnable8_RW:1;
		u32	CAN2StopEnable_RW:1;
		u32 Reserved1:10;
	};
	u32 All;
}_DEBUGMCU_Control;

typedef struct
{
  __IO _DEBUGMCU_IDCode IDCODE;
  __IO _DEBUGMCU_Control CR;	
}DBGMCU_TypeDef;

/** 
  * @brief DMA Controller
  */

#define DMA_CHANNEL_CFG_PRIORUTY_LEVEL_LOW        0
#define DMA_CHANNEL_CFG_PRIORUTY_LEVEL_MEDIUM     1
#define DMA_CHANNEL_CFG_PRIORUTY_LEVEL_HIGH       2
#define DMA_CHANNEL_CFG_PRIORUTY_LEVEL_VERY_HIGH  3

#define DMA_CHANNEL_CFG_MEMORY_PERIPHERAL_SIZE_8_BITS   0
#define DMA_CHANNEL_CFG_MEMORY_PERIPHERAL_SIZE_16_BITS  1
#define DMA_CHANNEL_CFG_MEMORY_PERIPHERAL_SIZE_32_BITS  2
typedef union
{
	struct
	{
		u32 ChannelEnable_RW:1;
		u32 TransferCompleteInterruptEnable_RW:1;
		u32 HalfTransferInterruptEnable_RW:1;
		u32 TransferErrorInterruptEnable_RW:1;
		u32 DataTransferDirection_RW:1;
		u32 CircularModeEnable_RW:1;
		u32 PeripheralIncrementModeEnable_RW:1;
		u32 MemoryIncrementModeEnable_RW:1;
		u32 PeripheralSize_RW:2;
		u32 MemorySize_RW:2;
		u32 PriorutyLevel_RW:2;
		u32 MemoryToMemoryModeEnable:1;
		u32 Reserved0:17;
	};
	u32 All;
}_DMA_Channel_Configuration;

typedef union
{
	struct
	{
		u32 NumberOfData_RW:16;
		u32 Reserved0:16;
	};
	u32 All;
}_DMA_Channel_Number_Of_Data;

typedef union
{
	struct   
	{
		u32 PeripheralAddress_RW:32; //当PSIZE为16位，不用第0位。为32位不用0/1位
	};
	u32 All;                       //只能在没有使能通道时写入
}_DMA_Channel_Peripheral_Address;

typedef union
{
	struct
	{
		u32 MemoryAddress_RW:32; //当MSIZE为16位，不用第0位。为32位不用0/1位
	};
	u32 All;                   //只能在没有使能通道时写入
}_DMA_Channel_Memory_Address;
typedef struct
{
  __IO _DMA_Channel_Configuration CCR;
  __IO _DMA_Channel_Number_Of_Data CNDTR;
  __IO _DMA_Channel_Peripheral_Address CPAR;
  __IO _DMA_Channel_Memory_Address CMAR;
} DMA_Channel_TypeDef;

typedef union
{
	struct
	{
		u32 GlobalInterruptFlag1_R:1;   // 1:产生了 TE HT TC 其中的事件
		u32 TransferCompleteFlag1_R:1;
		u32 HalfTransferFlag1_R:1;
		u32 TransferErrorFlag1_R:1;
		
		u32 GlobalInterruptFlag2_R:1;
		u32 TransferCompleteFlag2_R:1;
		u32 HalfTransferFlag2_R:1;
		u32 TransferErrorFlag2_R:1;
		
		u32 GlobalInterruptFlag3_R:1;
		u32 TransferCompleteFlag3_R:1;
		u32 HalfTransferFlag3_R:1;
		u32 TransferErrorFlag3_R:1;
		
		u32 GlobalInterruptFlag4_R:1;
		u32 TransferCompleteFlag4_R:1;
		u32 HalfTransferFlag4_R:1;
		u32 TransferErrorFlag4_R:1;
		
		u32 GlobalInterruptFlag5_R:1;
		u32 TransferCompleteFlag5_R:1;
		u32 HalfTransferFlag5_R:1;
		u32 TransferErrorFlag5_R:1;
		
		u32 GlobalInterruptFlag6_R:1;
		u32 TransferCompleteFlag6_R:1;
		u32 HalfTransferFlag6_R:1;
		u32 TransferErrorFlag6_R:1;
		
		u32 GlobalInterruptFlag7_R:1;
		u32 TransferCompleteFlag7_R:1;
		u32 HalfTransferFlag7_R:1;
		u32 TransferErrorFlag7_R:1;
		
		u32 Reserved0:4;
	};
	u32 All;
}_DMA_Interrupt_Status;

typedef union
{
	struct
	{
		u32 GlobalInterruptClear1_RW:1;   // 1:清除 GIF TE HT TC
		u32 TransferCompleteClear1_RW:1;
		u32 HalfTransferClear1_RW:1;
		u32 TransferErrorClear1_RW:1;
		
		u32 GlobalInterruptClear2_RW:1;   // 1:清除 GIF TE HT TC
		u32 TransferCompleteClear2_RW:1;
		u32 HalfTransferClear2_RW:1;
		u32 TransferErrorClear2_RW:1;
		
		u32 GlobalInterruptClear3_RW:1;   // 1:清除 GIF TE HT TC
		u32 TransferCompleteClear3_RW:1;
		u32 HalfTransferClear3_RW:1;
		u32 TransferErrorClear3_RW:1;
		
		u32 GlobalInterruptClear4_RW:1;   // 1:清除 GIF TE HT TC
		u32 TransferCompleteClear4_RW:1;
		u32 HalfTransferClear4_RW:1;
		u32 TransferErrorClear4_RW:1;
		
		u32 GlobalInterruptClear5_RW:1;   // 1:清除 GIF TE HT TC
		u32 TransferCompleteClear5_RW:1;
		u32 HalfTransferClear5_RW:1;
		u32 TransferErrorClear5_RW:1;
		
		u32 GlobalInterruptClear6_RW:1;   // 1:清除 GIF TE HT TC
		u32 TransferCompleteClear6_RW:1;
		u32 HalfTransferClear6_RW:1;
		u32 TransferErrorClear6_RW:1;
		
		u32 GlobalInterruptClear7_RW:1;   // 1:清除 GIF TE HT TC
		u32 TransferCompleteClear7_RW:1;
		u32 HalfTransferClear7_RW:1;
		u32 TransferErrorClear7_RW:1;
		
		u32 Reserved0:4;
	};
	u32 All;
}_DMA_Interrupt_Flag_Clear;

typedef struct
{
  __IO _DMA_Interrupt_Status ISR;
  __IO _DMA_Interrupt_Flag_Clear IFCR;
} DMA_TypeDef;

/** 
  * @brief Ethernet MAC
  */

typedef struct
{
  __IO uint32_t MACCR;
  __IO uint32_t MACFFR;
  __IO uint32_t MACHTHR;
  __IO uint32_t MACHTLR;
  __IO uint32_t MACMIIAR;
  __IO uint32_t MACMIIDR;
  __IO uint32_t MACFCR;
  __IO uint32_t MACVLANTR;             /*    8 */
       uint32_t RESERVED0[2];
  __IO uint32_t MACRWUFFR;             /*   11 */
  __IO uint32_t MACPMTCSR;
       uint32_t RESERVED1[2];
  __IO uint32_t MACSR;                 /*   15 */
  __IO uint32_t MACIMR;
  __IO uint32_t MACA0HR;
  __IO uint32_t MACA0LR;
  __IO uint32_t MACA1HR;
  __IO uint32_t MACA1LR;
  __IO uint32_t MACA2HR;
  __IO uint32_t MACA2LR;
  __IO uint32_t MACA3HR;
  __IO uint32_t MACA3LR;               /*   24 */
       uint32_t RESERVED2[40];
  __IO uint32_t MMCCR;                 /*   65 */
  __IO uint32_t MMCRIR;
  __IO uint32_t MMCTIR;
  __IO uint32_t MMCRIMR;
  __IO uint32_t MMCTIMR;               /*   69 */
       uint32_t RESERVED3[14];
  __IO uint32_t MMCTGFSCCR;            /*   84 */
  __IO uint32_t MMCTGFMSCCR;
       uint32_t RESERVED4[5];
  __IO uint32_t MMCTGFCR;
       uint32_t RESERVED5[10];
  __IO uint32_t MMCRFCECR;
  __IO uint32_t MMCRFAECR;
       uint32_t RESERVED6[10];
  __IO uint32_t MMCRGUFCR;
       uint32_t RESERVED7[334];
  __IO uint32_t PTPTSCR;
  __IO uint32_t PTPSSIR;
  __IO uint32_t PTPTSHR;
  __IO uint32_t PTPTSLR;
  __IO uint32_t PTPTSHUR;
  __IO uint32_t PTPTSLUR;
  __IO uint32_t PTPTSAR;
  __IO uint32_t PTPTTHR;
  __IO uint32_t PTPTTLR;
       uint32_t RESERVED8[567];
  __IO uint32_t DMABMR;
  __IO uint32_t DMATPDR;
  __IO uint32_t DMARPDR;
  __IO uint32_t DMARDLAR;
  __IO uint32_t DMATDLAR;
  __IO uint32_t DMASR;
  __IO uint32_t DMAOMR;
  __IO uint32_t DMAIER;
  __IO uint32_t DMAMFBOCR;
       uint32_t RESERVED9[9];
  __IO uint32_t DMACHTDR;
  __IO uint32_t DMACHRDR;
  __IO uint32_t DMACHTBAR;
  __IO uint32_t DMACHRBAR;
} ETH_TypeDef;

/** 
  * @brief External Interrupt/Event Controller
  * @data 26 July 2016 CRJ 
  */

typedef union
{
	struct 
	{
		u32 NotMaskInterruptOnLine0_RW:1;
		u32 NotMaskInterruptOnLine1_RW:1;
		u32 NotMaskInterruptOnLine2_RW:1;
		u32 NotMaskInterruptOnLine3_RW:1;
		u32 NotMaskInterruptOnLine4_RW:1;
		u32 NotMaskInterruptOnLine5_RW:1;
		u32 NotMaskInterruptOnLine6_RW:1;
		u32 NotMaskInterruptOnLine7_RW:1;
		u32 NotMaskInterruptOnLine8_RW:1;
		u32 NotMaskInterruptOnLine9_RW:1;
		u32 NotMaskInterruptOnLine10_RW:1;
		u32 NotMaskInterruptOnLine11_RW:1;
		u32 NotMaskInterruptOnLine12_RW:1;
		u32 NotMaskInterruptOnLine13_RW:1;
		u32 NotMaskInterruptOnLine14_RW:1;
		u32 NotMaskInterruptOnLine15_RW:1;
		u32 NotMaskInterruptOnLine16_RW:1;
		u32 NotMaskInterruptOnLine17_RW:1;
		u32 NotMaskInterruptOnLine18_RW:1;
		u32 NotMaskInterruptOnLine19_RW:1;
		u32 Reserved0:12;
	};
	u32 All;
}_EXTI_InterruptMask;

typedef union
{
	struct 
	{
		u32 NotMaskEventOnline0_RW:1;
		u32 NotMaskEventOnline1_RW:1;
		u32 NotMaskEventOnline2_RW:1;
		u32 NotMaskEventOnline3_RW:1;
		u32 NotMaskEventOnline4_RW:1;
		u32 NotMaskEventOnline5_RW:1;
		u32 NotMaskEventOnline6_RW:1;
		u32 NotMaskEventOnline7_RW:1;
		u32 NotMaskEventOnline8_RW:1;
		u32 NotMaskEventOnline9_RW:1;
		u32 NotMaskEventOnline10_RW:1;
		u32 NotMaskEventOnline11_RW:1;
		u32 NotMaskEventOnline12_RW:1;
		u32 NotMaskEventOnline13_RW:1;
		u32 NotMaskEventOnline14_RW:1;
		u32 NotMaskEventOnline15_RW:1;
		u32 NotMaskEventOnline16_RW:1;
		u32 NotMaskEventOnline17_RW:1;
		u32 NotMaskEventOnline18_RW:1;
		u32 NotMaskEventOnline19_RW:1;
		u32 Reserved0:12;
	};
	u32 All;
}_EXTI_EventMask;

typedef union
{
	struct 
	{
		u32 RisingTrrggerEnable0_RW:1;
		u32 RisingTrrggerEnable1_RW:1;
		u32 RisingTrrggerEnable2_RW:1;
		u32 RisingTrrggerEnable3_RW:1;
		u32 RisingTrrggerEnable4_RW:1;
		u32 RisingTrrggerEnable5_RW:1;
		u32 RisingTrrggerEnable6_RW:1;
		u32 RisingTrrggerEnable7_RW:1;
		u32 RisingTrrggerEnable8_RW:1;
		u32 RisingTrrggerEnable9_RW:1;
		u32 RisingTrrggerEnable10_RW:1;
		u32 RisingTrrggerEnable11_RW:1;
		u32 RisingTrrggerEnable12_RW:1;
		u32 RisingTrrggerEnable13_RW:1;
		u32 RisingTrrggerEnable14_RW:1;
		u32 RisingTrrggerEnable15_RW:1;
		u32 RisingTrrggerEnable16_RW:1;
		u32 RisingTrrggerEnable17_RW:1;
		u32 RisingTrrggerEnable18_RW:1;
		u32 RisingTrrggerEnable19_RW:1;
		u32 Reserved0:12;
	};
	u32 All;
}_EXTI_RisingTrrggerEnable;


typedef union
{
	struct 
	{
		u32 FallingTriggerEnable0_RW:1;
		u32 FallingTriggerEnable1_RW:1;
		u32 FallingTriggerEnable2_RW:1;
		u32 FallingTriggerEnable3_RW:1;
		u32 FallingTriggerEnable4_RW:1;
		u32 FallingTriggerEnable5_RW:1;
		u32 FallingTriggerEnable6_RW:1;
		u32 FallingTriggerEnable7_RW:1;
		u32 FallingTriggerEnable8_RW:1;
		u32 FallingTriggerEnable9_RW:1;
		u32 FallingTriggerEnable10_RW:1;
		u32 FallingTriggerEnable11_RW:1;
		u32 FallingTriggerEnable12_RW:1;
		u32 FallingTriggerEnable13_RW:1;
		u32 FallingTriggerEnable14_RW:1;
		u32 FallingTriggerEnable15_RW:1;
		u32 FallingTriggerEnable16_RW:1;
		u32 FallingTriggerEnable17_RW:1;
		u32 FallingTriggerEnable18_RW:1;
		u32 FallingTriggerEnable19_RW:1;
		u32 Reserved0:12;
	};
	u32 All;
}_EXTI_FallingTriggerEnable;

typedef union
{
	struct 
	{
		u32 SoftwareInterruptEvent0_RW:1;
		u32 SoftwareInterruptEvent1_RW:1;
		u32 SoftwareInterruptEvent2_RW:1;
		u32 SoftwareInterruptEvent3_RW:1;
		u32 SoftwareInterruptEvent4_RW:1;
		u32 SoftwareInterruptEvent5_RW:1;
		u32 SoftwareInterruptEvent6_RW:1;
		u32 SoftwareInterruptEvent7_RW:1;
		u32 SoftwareInterruptEvent8_RW:1;
		u32 SoftwareInterruptEvent9_RW:1;
		u32 SoftwareInterruptEvent10_RW:1;
		u32 SoftwareInterruptEvent11_RW:1;
		u32 SoftwareInterruptEvent12_RW:1;
		u32 SoftwareInterruptEvent13_RW:1;
		u32 SoftwareInterruptEvent14_RW:1;
		u32 SoftwareInterruptEvent15_RW:1;
		u32 SoftwareInterruptEvent16_RW:1;
		u32 SoftwareInterruptEvent17_RW:1;
		u32 SoftwareInterruptEvent18_RW:1;
		u32 SoftwareInterruptEvent19_RW:1;
		u32 Reserved0:12;
	};
	u32 All;
}_EXTI_SoftwareInterruptEvent;

typedef union
{
	struct 
	{
		u32 Pending0_RCW1:1;
		u32 Pending1_RCW1:1;
		u32 Pending2_RCW1:1;
		u32 Pending3_RCW1:1;
		u32 Pending4_RCW1:1;
		u32 Pending5_RCW1:1;
		u32 Pending6_RCW1:1;
		u32 Pending7_RCW1:1;
		u32 Pending8_RCW1:1;
		u32 Pending9_RCW1:1;
		u32 Pending10_RCW1:1;
		u32 Pending11_RCW1:1;
		u32 Pending12_RCW1:1;
		u32 Pending13_RCW1:1;
		u32 Pending14_RCW1:1;
		u32 Pending15_RCW1:1;
		u32 Pending16_RCW1:1;
		u32 Pending17_RCW1:1;
		u32 Pending18_RCW1:1;
		u32 Pending19_RCW1:1;
	};
	u32 All;
}_EXTI_Pending;
typedef struct
{
  __IO _EXTI_InterruptMask IMR;
  __IO _EXTI_EventMask EMR;
  __IO _EXTI_RisingTrrggerEnable RTSR;
  __IO _EXTI_FallingTriggerEnable FTSR;
  __IO _EXTI_SoftwareInterruptEvent SWIER;
  __IO _EXTI_Pending PR;
} EXTI_TypeDef;

/** 
  * @brief FLASH Registers
  */

#define FLASH_ACR_LATENCY_0_24MHZ   (0)
#define FLASH_ACR_LATENCY_24_48MHZ  (1)
#define FLASH_ACR_LATENCY_48_72MHZ  (2)
typedef union
{
	struct 
	{
		u32 Latency_RW:3;
		u32 FlashHalfCycleAccessEnable_RW:1;
		u32 PrefetchBufferEnable_RW:1;
		u32 PrefetchBufferStatus_R:1;
		u32 Reserved0:26;
	};
	u32 All;
}_Flash_Access_Control;


typedef struct
{
	u32 FlashProgramAndEraseControllerKey_W;
}_Flash_Program_And_Erase_Controller_Key;

typedef struct
{
	u32 OptionByteKey_W;
}_Flash_Option_Key;

typedef union
{
	struct
	{
		u32 Busy_R :1;
		u32 Reserved0:1;
		u32 ProgrammingError_RW:1;
		u32 Reserved1:1;
		u32 WriteProtectionError_RW:1;
		u32 EndOfOperation:1;
		u32 Reserved2:26;
	};
	u32 All;
}_Flash_Status;

typedef union
{
	struct
	{
		u32 Programming_RW :1;
		u32 PageErase_RW:1;
		u32 MassErase_RW:1;
		u32 Reserved0:1;
		u32 OptionByteProgramming_RW:1;
		u32 OptionByteErase_RW:1;
		u32 Strat:1;
		u32 LockOnlyCanWriteOne_RW:1;
		u32 Reserved1:1;
		u32 OptionByteWriteEnable_RW:1;
		u32 ErrorInterruptEnable_RW:1;
		u32 Reserved2:1;
		u32 EndOfOperationInterruptEnable_RW:1;
		u32 Reserved3:19;
	};
	u32 All;
}_Flash_Control;

typedef struct
{
	u32 Address_W;
}_Flash_Address;

typedef union
{
	struct
	{
		u32 OptionBytesError_R:1;
		u32 ReadProtection_R:1;
		u32 UserOptionBytes_WDG_SW_R:1;
		u32 UserOptionBytes_nRST_STOP_R:1;
		u32 UserOptionBytes_nRST_STDBY_R:1;
		u32 UserOptionBytes_Reserved_R:5;
		u32 Data0_R:8;
		u32 Data1_R:8;
		u32 Reserved0:6;
	};
	u32 All;
}_Flash_OptionByte;

typedef struct
{
	u32 WriteProtect_R;
}_Flash_WriteProtection;

typedef struct
{
  __IO _Flash_Access_Control ACR;
  __IO _Flash_Program_And_Erase_Controller_Key KEYR;
  __IO _Flash_Option_Key OPTKEYR;
  __IO _Flash_Status SR;
  __IO _Flash_Control CR;
  __IO _Flash_Address AR;
  __IO u32  RESERVED;
  __IO _Flash_OptionByte OBR;
  __IO _Flash_WriteProtection WRPR;
#ifdef STM32F10X_XL
  uint32_t RESERVED1[8]; 
  __IO uint32_t KEYR2;
  uint32_t RESERVED2;   
  __IO uint32_t SR2;
  __IO uint32_t CR2;
  __IO uint32_t AR2; 
#endif /* STM32F10X_XL */  
} FLASH_TypeDef;

/** 
  * @brief Option Bytes Registers
  * \Attention 下面的写操作，不允许使用8位的方式，就像Flash不能写8位一样，但是要注意：反码部分好像是自动的，也就是用户操作非反码部分是有效的。
  */
  
typedef struct
{
  __IO u16 RDP;
  __IO u16 USER;
  __IO u16 Data0;
  __IO u16 Data1;
  __IO u16 WRP0;
  __IO u16 WRP1;
  __IO u16 WRP2;
  __IO u16 WRP3;
} OB_TypeDef;

/** 
  * @brief Flexible Static Memory Controller
	* @Attention 官方库中只是给了一个数组，根据正点原子添加寄存器定义
  */

typedef union
{
	struct 
	{
		u32 MemoryBankEnable_RW:1;
		u32 AddressDataMultiplexingEnable_RW:1;
		u32 MemoryType_RW:2;   // 00:SRAM_ROM 01:PSRAM(CellularRAM) 10:NOR 11:-
		u32 MemoryDatabusWidth_RW:2; // 00:8 01:16 10/11:-
		u32 FlashAccessEnable_RW:1;
		u32 Reserved0:1;
		u32 BurstEnable_RW:1;
		u32 WaitSignalPolarity_RW:1;
		u32 WrappedBurstModeSupport_RW:1;
		u32 WaitTimingConfiguration_RW:1;
		u32 WriteEnable_RW:1;
		u32 WaitEnable_RW:1;
		u32 ExtendedModeEnable_RW:1;
		u32 Reserved1:4;
		u32 WriteBurstEnable_RW:1;
		u32 Reserved2:12;
	};
	u32 All;
}_FSMC_ChipSelectControl_BCR;

typedef union
{
	struct 
	{
		u32 AddressSetupPhaseDuration_RW:4;
		u32 AddressHoldPhaseDuration_RW:4;
		u32 DataPhaseDuration_RW:8;
		u32 BusTurnaroundPhaseDuration_RW:4;
		u32 ClockDivideRatio_RW:4;
		u32 DataLatecy_RW:4;
		u32 AccessMode_RW:2;
		u32 Reserved0:2;
	};
	u32 All;
}_FSMC_ChipSelectTiming_BTR;


typedef union
{
	struct
	{
	__IO _FSMC_ChipSelectControl_BCR BCR1;
	__IO _FSMC_ChipSelectTiming_BTR  BTR1;
	__IO _FSMC_ChipSelectControl_BCR BCR2;
	__IO _FSMC_ChipSelectTiming_BTR  BTR2;
	__IO _FSMC_ChipSelectControl_BCR BCR3;
	__IO _FSMC_ChipSelectTiming_BTR  BTR3;
	__IO _FSMC_ChipSelectControl_BCR BCR4;
	__IO _FSMC_ChipSelectTiming_BTR  BTR4;
	};
  __IO u32 BTCR[8];   
} FSMC_Bank1_TypeDef; 


/** 
  * @brief Flexible Static Memory Controller Bank1E.
  * @Attention 本来只有BWTR数组的,后来改为联合体
  */
typedef union
{
	struct 
	{
		u32 AddressSetupPhaseDuration_RW:4;
		u32 AddressHoldPhaseDuration_RW:4;
		u32 DataPhaseDuration_RW:8;
		u32 Reserved0:4;
		u32 ClockDivideRatio_RW:4;
		u32 DataLatecy_RW:4;
		u32 AccessMode_RW:2;
		u32 Reserved1:2;
	};
	u32 All;
}_FSMC_WriteTiming_BWTR;

typedef union
{
	struct
	{
	__IO _FSMC_WriteTiming_BWTR BWTR1;
	 u32 Reserved0;
	__IO _FSMC_WriteTiming_BWTR BWTR2;
	 u32 Reserved1;
	__IO _FSMC_WriteTiming_BWTR BWTR3;
	 u32 Reserved2;
	__IO _FSMC_WriteTiming_BWTR BWTR4;
	};
	__IO u32 BWTR[7];
} FSMC_Bank1E_TypeDef;

/** 
  * @brief Flexible Static Memory Controller Bank2
  */
typedef union
{
	struct 
	{
		u32 Reserved0:1;
		u32 WaitFeatureEnable_RW:1;
		u32 PCCardNandFlashMemoryBankEnable_RW:1;
		u32 MemoryType_RW:1;
		u32 DatabusWidth_RW:2;
		u32 ECCComputationLogicEnable_RW:1;
		u32 Reserved1:2;
		u32 CLEToREDelay_RW:4;
		u32 ALEToREDelay_RW:4;
		u32 ECCPageSize_RW:3;
		u32 Reserved2:12;
	};
	u32 All;
}_FSMC_NANDFlash_PCCard_Control;  

typedef union
{
	struct 
	{
		u32 InterruptRisingEdgeStatus_RW:1;
		u32 InterruptHighLevelStatus_RW:1;
		u32 InterruptFallingEdgeStatus_RW:1;
		u32 InterruptRisingEdgeDetectionEnable_RW:1;
		u32 InterruptHighLevelDetectionEnable_RW:1;
		u32 InterruptFallingEdgeDecectionEnable_RW:1;
		u32 FIFOEmptyFlag_R:1;
		u32 Reserved0:25;
	};
	u32 All;
}_FSMC_FIFOStatusAndInterrupt;

typedef union
{
	struct 
	{
		u32 CommonMemorySetupTime_RW:8;
		u32 CommonMemoryWaitTime_RW:8;
		u32 CommonMemoryHoldTime_RW:8;
		u32 CommonMemoryDatabusHiZTime_RW:8;
	};
	u32 All;
}_FSMC_CommonMemorySpaceTiming;

typedef union
{
	struct 
	{
		u32 AttributeMemorySetupTime_RW:8;
		u32 AttributeMemoryWaitTime_RW:8;
		u32 AttributeMemoryHoldTIme_RW:8;
		u32 AttributeMemoryDatabusHiZTime_RW:8;
	};
	u32 All;
}_FSMC_AttributeMemorySpaceTiming;

typedef union
{
	struct 
	{
		u32 IOSetupTime_RW:8;
		u32 IOWaitTime_RW:8;
		u32 IOHoldTime_RW:8;
		u32 IODatabusHiZTime_RW:8;
	};
	u32 All;
}_FSMC_IO_SpaceTiming;

typedef struct 
{
  u32 ECCValue;
}_FSMC_ECC;

typedef struct
{
  __IO _FSMC_NANDFlash_PCCard_Control PCR2;
  __IO _FSMC_FIFOStatusAndInterrupt SR2;
  __IO _FSMC_CommonMemorySpaceTiming PMEM2;
  __IO _FSMC_AttributeMemorySpaceTiming PATT2;
  u32  RESERVED0;   
  __IO _FSMC_ECC ECCR2; 
} FSMC_Bank2_TypeDef;  

/** 
  * @brief Flexible Static Memory Controller Bank3
  */
  
typedef struct
{
  __IO _FSMC_NANDFlash_PCCard_Control PCR3;
  __IO _FSMC_FIFOStatusAndInterrupt SR3;
  __IO _FSMC_CommonMemorySpaceTiming PMEM3;
  __IO _FSMC_AttributeMemorySpaceTiming PATT3;
  u32  RESERVED0;   
  __IO _FSMC_ECC ECCR3; 
} FSMC_Bank3_TypeDef; 

/** 
  * @brief Flexible Static Memory Controller Bank4
  */
  
typedef struct
{
  __IO _FSMC_NANDFlash_PCCard_Control PCR4;
  __IO _FSMC_FIFOStatusAndInterrupt SR4;
  __IO _FSMC_CommonMemorySpaceTiming PMEM4;
  __IO _FSMC_AttributeMemorySpaceTiming PATT4;
  __IO _FSMC_IO_SpaceTiming PIO4; 
} FSMC_Bank4_TypeDef; 

/** 
  * @brief General Purpose I/O
  */

/**
* @CRJ
* @brief Use Macro To Definition Register
*/
#define GPIO_CR_INOUNTMODE_INPUT         (0)
#define GPIO_CR_INOUNTMODE_OUTPUT_10MHZ  (1)
#define GPIO_CR_INOUNTMODE_OUTPUT_2MHZ   (2)
#define GPIO_CR_INOUNTMODE_OUTPUT_50MHZ  (3)

#define GPIO_CR_PINCONFG_IN_ANALOG          (0)
#define GPIO_CR_PINCONFG_IN_FLOATING        (1)
#define GPIO_CR_PINCONFG_IN_PULLUP_PULLDOWN (2)

#define GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_PUSHPULL   (0)
#define GPIO_CR_PINCONFG_OUT_GENERAL_PURPOSE_OPEN_DRAIN (1)
#define GPIO_CR_PINCONFG_OUT_ALTERNATE_PUSHPULL         (2)
#define GPIO_CR_PINCONFG_OUT_ALTERNATE_OPEN_DRAIN       (3)

typedef union 
{
	struct 
	{
		u32 InOutMode0_RW : 2;
		u32 PinConfig0_RW : 2;
		u32 InOutMode1_RW : 2;
		u32 PinConfig1_RW : 2;
		u32 InOutMode2_RW : 2;
		u32 PinConfig2_RW : 2;
		u32 InOutMode3_RW : 2;
		u32 PinConfig3_RW : 2;
		u32 InOutMode4_RW : 2;
		u32 PinConfig4_RW : 2;
		u32 InOutMode5_RW : 2;
		u32 PinConfig5_RW : 2;
		u32 InOutMode6_RW : 2;
		u32 PinConfig6_RW : 2;
		u32 InOutMode7_RW : 2;
		u32 PinConfig7_RW : 2;
	};
	u32 All;
}_GPIO_PortConfiguration_Low;

typedef union
{
	struct
	{
		u32 InOutMode8_RW : 2;
		u32 PinConfig8_RW : 2;
		u32 InOutMode9_RW : 2;
		u32 PinConfig9_RW : 2;
		u32 InOutMode10_RW : 2;
		u32 PinConfig10_RW : 2;
		u32 InOutMode11_RW : 2;
		u32 PinConfig11_RW : 2;
		u32 InOutMode12_RW : 2;
		u32 PinConfig12_RW : 2;
		u32 InOutMode13_RW : 2;
		u32 PinConfig13_RW : 2;
		u32 InOutMode14_RW : 2;
		u32 PinConfig14_RW : 2;
		u32 InOutMode15_RW : 2;
		u32 PinConfig15_RW : 2;
	};
	u32 All;
}_GPIO_PortConfiguration_High;

typedef union 
{
	struct  
	{
		u32 InputData0_R : 1;
		u32 InputData1_R : 1;
		u32 InputData2_R : 1;
		u32 InputData3_R : 1;
		u32 InputData4_R : 1;
		u32 InputData5_R : 1;
		u32 InputData6_R : 1;
		u32 InputData7_R : 1;
		u32 InputData8_R : 1;
		u32 InputData9_R : 1;
		u32 InputData10_R : 1;
		u32 InputData11_R : 1;
		u32 InputData12_R : 1;
		u32 InputData13_R : 1;
		u32 InputData14_R : 1;
		u32 InputData15_R : 1;
		u32 Reserved0 : 16;
	};
	u32 All;
}_GPIO_Input_Data;


typedef union
{
	struct
	{
		u32 OutputData0_RW : 1;
		u32 OutputData1_RW : 1;
		u32 OutputData2_RW : 1;
		u32 OutputData3_RW : 1;
		u32 OutputData4_RW : 1;
		u32 OutputData5_RW : 1;
		u32 OutputData6_RW : 1;
		u32 OutputData7_RW : 1;
		u32 OutputData8_RW : 1;
		u32 OutputData9_RW : 1;
		u32 OutputData10_RW : 1;
		u32 OutputData11_RW : 1;
		u32 OutputData12_RW : 1;
		u32 OutputData13_RW : 1;
		u32 OutputData14_RW : 1;
		u32 OutputData15_RW : 1;
		u32 Reserved0 : 16;
	};
	struct
	{
		u32 lowPort_RW:8;  // 为了使用高8位
		u32 highPort_RW:8;
		u32 Reserved1 : 16;
	};
	u32 All;
}_GPIO_Output_Data;

typedef union 
{
	struct  
	{
		u32 SetOutputHigh0_W : 1;
		u32 SetOutputHigh1_W : 1;
		u32 SetOutputHigh2_W : 1;
		u32 SetOutputHigh3_W : 1;
		u32 SetOutputHigh4_W : 1;
		u32 SetOutputHigh5_W : 1;
		u32 SetOutputHigh6_W : 1;
		u32 SetOutputHigh7_W : 1;
		u32 SetOutputHigh8_W : 1;
		u32 SetOutputHigh9_W : 1;
		u32 SetOutputHigh10_W : 1;
		u32 SetOutputHigh11_W : 1;
		u32 SetOutputHigh12_W : 1;
		u32 SetOutputHigh13_W : 1;
		u32 SetOutputHigh14_W : 1;
		u32 SetOutputHigh15_W : 1;

		u32 SetOutputLow0_W : 1;
		u32 SetOutputLow1_W : 1;
		u32 SetOutputLow2_W : 1;
		u32 SetOutputLow3_W : 1;
		u32 SetOutputLow4_W : 1;
		u32 SetOutputLow5_W : 1;
		u32 SetOutputLow6_W : 1;
		u32 SetOutputLow7_W : 1;
		u32 SetOutputLow8_W : 1;
		u32 SetOutputLow9_W : 1;
		u32 SetOutputLow10_W : 1;
		u32 SetOutputLow11_W : 1;
		u32 SetOutputLow12_W : 1;
		u32 SetOutputLow13_W : 1;
		u32 SetOutputLow14_W : 1;
		u32 SetOutputLow15_W : 1;
	};
	u32 All;
}_GPIO_Bit_Set_Reset;

typedef union 
{
	struct 
	{
		u32 SetOutputLow0_W : 1;
		u32 SetOutputLow1_W : 1;
		u32 SetOutputLow2_W : 1;
		u32 SetOutputLow3_W : 1;
		u32 SetOutputLow4_W : 1;
		u32 SetOutputLow5_W : 1;
		u32 SetOutputLow6_W : 1;
		u32 SetOutputLow7_W : 1;
		u32 SetOutputLow8_W : 1;
		u32 SetOutputLow9_W : 1;
		u32 SetOutputLow10_W : 1;
		u32 SetOutputLow11_W : 1;
		u32 SetOutputLow12_W : 1;
		u32 SetOutputLow13_W : 1;
		u32 SetOutputLow14_W : 1;
		u32 SetOutputLow15_W : 1;
		u32 Reserved0 : 16;
	};
	u32 All;
}_GPIO_Bit_Reset;

typedef union 
{
	struct 
	{
		u32 LockPin0_RW : 1;  // Write 1 can not lock pin configuration ,need to write LockEnableKey as special sequence
		u32 LockPin1_RW : 1;
		u32 LockPin2_RW : 1;
		u32 LockPin3_RW : 1;
		u32 LockPin4_RW : 1;
		u32 LockPin5_RW : 1;
		u32 LockPin6_RW : 1;
		u32 LockPin7_RW : 1;
		u32 LockPin8_RW : 1;
		u32 LockPin9_RW : 1;
		u32 LockPin10_RW : 1;
		u32 LockPin11_RW : 1;
		u32 LockPin12_RW : 1;
		u32 LockPin13_RW : 1;
		u32 LockPin14_RW : 1;
		u32 LockPin15_RW : 1;
		u32 LockEnableKey_RW : 1;  // Lock Sequence: Write 1 -> 0 -> 1 -> 0  Lock Active: Read as 1
	};
	u32 All;
}_GPIO_Port_Configuration_Lock;
typedef struct
{
	__IO _GPIO_PortConfiguration_Low CRL;
	__IO _GPIO_PortConfiguration_High CRH;
	__IO _GPIO_Input_Data IDR;
	__IO _GPIO_Output_Data ODR;
	__IO _GPIO_Bit_Set_Reset BSRR;
	__IO _GPIO_Bit_Reset BRR;
	__IO _GPIO_Port_Configuration_Lock LCKR;
	u32 Reserved0[249];  // CRJ Add 2016-7-5 STM32的寄存器为8位宽度
} GPIO_TypeDef;

/** 
  * @brief Alternate Function I/O
  */

typedef union
{
	struct
	{
		u32 CortexEventOutPinSel_RW : 4;
		u32 CortexEventOutPortSel_RW : 3;
		u32 CortexEventOutEnable_RW : 1;
		u32 Reserved0 : 24;
	};
	u32 All;
}_AFIO_CortexEvent_Control;

typedef union
{
	struct
	{
		u32 SPI1Remap_RW : 1;
		u32 I2CRemap_RW : 1;
		u32 USART1Remap_RW : 1;
		u32 USART2Remap_RW : 1;
		u32 USART3Remap_RW : 2;
		u32 TIM1Remap_RW : 2;
		u32 TIM2Remap_RW : 2;
		u32 TIM3Remap_RW : 2;
		u32 TIM4Remap_RW : 1;
		u32 CANRemap_RW : 2;
		u32 PD01Remap_RW : 1;
		u32 TIM5CH4InternalRemap_RW : 1;
		u32 ADC1ExternalTriggerInjectedConversionRema_RWp : 1;
		u32 ADC1ExternalTriggerRegularConversionRemap_RW : 1;
		u32 ADC2ExternalTriggerInjectedConversionRemap_RW : 1;
		u32 ADC2ExternalTriggerRegularConversionRemap_RW : 1;
		u32 Reserved0_RW : 3;
		u32 SerialWireJTAGConfiguration_W : 3;
		u32 Reserved1 : 5;
	};
	u32 All;
}_AFIO_Remap_And_DebugIOConfiguration;

/**
* @CRJ
* @Brief Use MRACO to Definition AFIO EXIT Pin
*/
#define AFIO_EXIT_PINSEL_PA (0)
#define AFIO_EXIT_PINSEL_PB (1)
#define AFIO_EXIT_PINSEL_PC (2)
#define AFIO_EXIT_PINSEL_PD (3)
#define AFIO_EXIT_PINSEL_PE (4)
#define AFIO_EXIT_PINSEL_PF (5)
#define AFIO_EXIT_PINSEL_PG (6)
typedef union
{
	struct
	{
		u32 EXITConfiguration0_RW : 4;
		u32 EXITConfiguration1_RW : 4;
		u32 EXITConfiguration2_RW : 4;
		u32 EXITConfiguration3_RW : 4;
		u32 Reserved0 : 16;
	};
	u32 All;
}_AFIO_External_Interrupt_Control1;
typedef union
{
	struct
	{
		u32 EXITConfiguration4_RW : 4;
		u32 EXITConfiguration5_RW : 4;
		u32 EXITConfiguration6_RW : 4;
		u32 EXITConfiguration7_RW : 4;
		u32 Reserved0 : 16;
	};
	u32 All;
}_AFIO_External_Interrupt_Control2;
typedef union
{
	struct
	{
		u32 EXITConfiguration8_RW : 4;
		u32 EXITConfiguration9_RW : 4;
		u32 EXITConfiguration10_RW : 4;
		u32 EXITConfiguration11_RW : 4;
		u32 Reserved0 : 16;
	};
	u32 All;
}_AFIO_External_Interrupt_Control3;
typedef union
{
	struct
	{
		u32 EXITConfiguration12_RW : 4;
		u32 EXITConfiguration13_RW : 4;
		u32 EXITConfiguration14_RW : 4;
		u32 EXITConfiguration15_RW : 4;
		u32 Reserved0 : 16;
	};
	u32 All;
}_AFIO_External_Interrupt_Control4;
typedef struct
{
	__IO _AFIO_CortexEvent_Control EVCR;
	__IO _AFIO_Remap_And_DebugIOConfiguration MAPR;
 // __IO uint32_t EXTICR[4];   // not use array Unfold By CRJ
	__IO _AFIO_External_Interrupt_Control1 EXTICR1;
	__IO _AFIO_External_Interrupt_Control2 EXTICR2;
	__IO _AFIO_External_Interrupt_Control3 EXTICR3;
	__IO _AFIO_External_Interrupt_Control4 EXTICR4;
  u32 RESERVED0;
  __IO u32 MAPR2;  
} AFIO_TypeDef;

/** 
  * @brief Inter Integrated Circuit Interface
  * @Date  2016-7-22
  */
typedef union
{
	struct 
	{
		u16 PeripheralEnable_RW:1;
		u16 SMBusEnable_RW:1;
		u16 Reserved0:1;
		u16 SMBustype_RW:1;
		u16 ARPEnable_RW:1;
		u16 PacketErrorCheckingEnable_RW:1;
		u16 GeneralcallEnable_RW:1;
		u16 ClockStretchingDisable_RW:1;
		u16 StartGeneration_RW:1;
		u16 StopGeneration_RW:1;
		u16 AcknowledgeEnable_RW:1;
		u16 AcknowledgeAndPacketErrorCheckingPosition_RW:1;
		u16 PacketErrorChecking_RW:1;
		u16 SMBusAlert_RW:1;
		u16 Reserved1:1;
		u16 SoftwareResetStateEnable_RW:1;
	};
	u16 All;
}_I2C_Control_1;

typedef union
{
	struct 
	{
		u16 PeripheralClockFrequency_RW:6;
		u16 Reserved0:2;
		u16 ErrorInterruptEnable_RW:1;
		u16 EventInterruptEnable_RW:1;
		u16 BufferInterruptEnable_RW:1;
		u16 DMARequsetEnable_RW:1;
		u16 DMALastTransfer_RW:1;
		u16 Reserved1:3;
	};
	u16 All;
}_I2C_Control_2;

typedef union
{
	struct 
	{
		u16 Reserved0:1;
		u16 SevenInterfaceAddress_RW:7;
		u16 Reserved1:2;
		u16 Reserved2:4;
		u16 SoftwareMustSetOne_RW:1;
		u16 AddressingMode_RW:1;
	};
	u16 All;
}_I2C_OwnAddress_SevenBit_1;

typedef union
{
	struct 
	{
		u16 TenInterfaceAddress_RW:10;
		u16 Reserved0:4;
		u16 SoftwareMustSetOne_RW:1;
		u16 AddressingMode_RW:1;
	};
	u16 All;
}_I2C_OwnAddress_TenBit_1;

typedef union
{
	struct 
	{
		u16 DualAddressingModeEnable_RW:1;
		u16 SevenBitsAddress2_RW:7;
		u16 Reserved0:8;
	};
	u16 All;
}_I2C_OwnAddress_2;

typedef union
{
	struct 
	{
		u16 Data_RW:8;
		u16 Reserved0:8;
	};
	u16 All;
}_I2C_Data;

typedef union
{
	struct 
	{
		u16 StartBit_R:1;  // 中断事件_起始位已发送(主)
		u16 AddressSentAddressMatched_R:1; // 中断事件_地址已发送(主)地址匹配(从)
		u16 ByteTransferFinished_R:1;  // 中断事件_数据字节传输完成(主从)
		u16 TenBitsHeaderSent_R:1; // 中断事件_10位头已发送(主)
		u16 StopDetection_R:1; // 中断事件_已收到停止(从)
		u16 Reserved0:1;
		u16 DataRegisterNotEmpty_R:1; // 接收缓冲区不空(主从)_中断事件中处理
		u16 DataRegisterEmpty_R:1;  // 发送缓冲区空(主从)_中断事件中处理
		u16 BusError_RCW0:1;  // 中断错误_总线错误(主从)
		u16 ArbitrationLost_RCW0:1; // 中断错误_仲裁丢失(主)
		u16 AcknowledgeFailure_RCW0:1; // 中断错误_应答失败(主从)
		u16 OverrunOrUnderrun_RCW0:1; // 中断错误_过载欠载(主从)
		u16 PacketErrorCheckingErrorInReception_RCW0:1; // 中断错误_PEC错误(主从)
		u16 Reserved1:1;
		u16 TimeoutOrTlowError_RCW0:1; // 中断错误_超时错误(主从)
		u16 SMBusAlert_RCW0:1; // 中断错误_SMBus提醒(主从)
	};
	u16 All;
}_I2C_Status_1;

typedef union
{
	struct 
	{
		u16 MaterSlaveMode_R:1;
		u16 BusBusy_R:1;
		u16 TransmitterOrReceiver_R:1;
		u16 Reserved0:1;
		u16 GeneralcallAddress_R:1;
		u16 SMBusDeviceDefaultAddress_R:1;
		u16 SMBusHostHeader_R:1;
		u16 DualFlag_R:1;
		u16 PacketErrorCheckingValue_R:8;
	};
	u16 All;
}_I2C_Status_2;

typedef union
{
	struct 
	{
		u16 ClockControl_RW:12;
		u16 Reserved0:2;
		u16 FastModeDutyCycle_RW:1;
		u16 I2CFastmodeEnable_RW:1;
	};
	u16 All;
}_I2C_ClockControl;

typedef union
{
	struct 
	{
		u16 MaximumRiseTime_RW:6;
		u16 Reserved0:10;
	};
	u16 All;
}_I2C_Trise;


typedef struct
{
  __IO _I2C_Control_1 CR1;
  u16  RESERVED0;
  __IO _I2C_Control_2 CR2;
  u16  RESERVED1;
	union {
  __IO _I2C_OwnAddress_SevenBit_1 SevenBit_OAR1;
	__IO _I2C_OwnAddress_TenBit_1   TenBit_OAR1;
	};
  u16  RESERVED2;
  __IO _I2C_OwnAddress_2 OAR2;
  u16  RESERVED3;
  __IO _I2C_Data DR;
  u16  RESERVED4;
  __IO _I2C_Status_1 SR1;
  u16  RESERVED5;
  __IO _I2C_Status_2 SR2;
  u16  RESERVED6;
  __IO _I2C_ClockControl CCR;
  u16  RESERVED7;
  __IO _I2C_Trise TRISE;
  u16  RESERVED8;
} I2C_TypeDef;

/** 
  * @brief Independent WATCHDOG
  */

#define IWDG_KR_FEED_DOG  0xAAAA
#define IWDG_KR_ALLOW_ACCESS_PR_RLR  0x5555
#define IWDG_KR_START_DOG 0xCCCC

typedef union
{
	struct
	{
		u32 KeyValue_W:16;
		u32 Reserved0:16;
	};
	u32 All;
}_IndependentWatchDog_Key;

typedef union
{
	struct
	{
		u32 PrescaleDivider_RW:3;
		u32 Reserved0:29;
	};
	u32 All;
}_IndependentWatchDog_Prescale;

typedef union
{
	struct
	{
		u32 ReloadValue_RW:12;
		u32 Reserved0:20;
	};
	u32 All;
}_IndependentWatchDog_Reload;

typedef union
{
	struct
	{
		u32 PrescaleValueUpdataStatus_R:1;
		u32 ReloadValueUpdataStatus_R:1;
		u32 Reserved0:30;
	};
	u32 All;
}_IndependentWatchDog_Status;

typedef struct
{
  __IO _IndependentWatchDog_Key KR;
  __IO _IndependentWatchDog_Prescale PR;
  __IO _IndependentWatchDog_Reload RLR;
  __IO _IndependentWatchDog_Status SR;
} IWDG_TypeDef;

/** 
  * @brief Power Control
  */
typedef union
{
	struct
	{
		u32 LowPowerDeepSleep_RW : 1;   // 0:PVD on at StopMode  1:PVD in lowpower at StopMode
		u32 PowerDownDeepSleep_RW : 1; 
		u32 ClearWakeupFlag_RCW1 : 1;
		u32 ClearStandbyFlag_RCW1 : 1;
		u32 PowerVoltageDetectorEnable_RW : 1;
		u32 PowerVoltageDetectorLevelSelection_RW : 3;
		u32 AllowAccessRTCAndBackupRegister_RW : 1;
		u32 Reserved0 : 23;
	};
	u32 All;
}_PWR_Control;

typedef union
{
	struct
	{
		u32 WakeupFlag_R : 1;
		u32 StandbyFalg_R : 1;
		u32 PowerVoltageDetectorIndicate_R : 1;
		u32 Reserved0 : 5;
		u32 WakeupPinEnable_RW : 1;
		u32 Reserved1 : 23;
	};
	u32 All;
}_PWR_Control_Status;

typedef struct
{
	__IO _PWR_Control CR;
	__IO _PWR_Control_Status CSR;
} PWR_TypeDef;

/** 
  * @brief Reset and Clock Control
  */
typedef union 
{
	struct 
	{
		u32 HSIEnable_RW : 1;
		u32 HSIReady_R : 1;
		u32 Reserved0 : 1;
		u32 HSITrimming_RW : 5;
		u32 HSICalibration_R : 8;
		u32 HSEEnable_RW : 1;
		u32 HSEReady_R : 1;
		u32 HSEBypass_RW : 1;
		u32 ClockSecuritySystemEnable_RW : 1;
		u32 Reserved1 : 4;
		u32 PLLEnable_RW : 1;
		u32 PLLReady_R : 1;
		u32 Reserved2 : 6;
	};
	u32 All;
}_RCC_Control;

/**
* @Author CRJ
* @brief Use Macro To Definition CFGR 
* @waring Redefiniton
*/
//#define RCC_CFGR_SW_HSI  (0)
//#define RCC_CFGR_SW_HSE  (1)
//#define RCC_CFGR_SW_PLL  (2)
//#define RCC_CFGR_SWS_HSI  (0)
//#define RCC_CFGR_SWS_HSE  (1)
//#define RCC_CFGR_SWS_PLL  (2)
//#define RCC_CFGR_HPRE_PRESCALE_1    (0)
//#define RCC_CFGR_HPRE_PRESCALE_2    (8)
//#define RCC_CFGR_HPRE_PRESCALE_4    (9)
//#define RCC_CFGR_HPRE_PRESCALE_8    (10)
//#define RCC_CFGR_HPRE_PRESCALE_16   (11)
//#define RCC_CFGR_HPRE_PRESCALE_64   (12)
//#define RCC_CFGR_HPRE_PRESCALE_128  (13)
//#define RCC_CFGR_HPRE_PRESCALE_256  (14)
//#define RCC_CFGR_HPRE_PRESCALE_512  (15)
//#define RCC_CFGR_PPRE1_PRESCALE_1  (0)
//#define RCC_CFGR_PPRE1_PRESCALE_2  (4)
//#define RCC_CFGR_PPRE1_PRESCALE_4  (5)
//#define RCC_CFGR_PPRE1_PRESCALE_8  (6)
//#define RCC_CFGR_PPRE1_PRESCALE_16 (7)
//#define RCC_CFGR_PPRE2_PRESCALE_1  (0)
//#define RCC_CFGR_PPRE2_PRESCALE_2  (4)
//#define RCC_CFGR_PPRE2_PRESCALE_4  (5)
//#define RCC_CFGR_PPRE2_PRESCALE_8  (6)
//#define RCC_CFGR_PPRE2_PRESCALE_16 (7)
//#define RCC_CFGR_ADCPRE_PRESCALE_2  (0)
//#define RCC_CFGR_ADCPRE_PRESCALE_4  (1)
//#define RCC_CFGR_ADCPRE_PRESCALE_8  (2)
//#define RCC_CFGR_ADCPRE_PRESCALE_16 (3)
//#define RCC_CFGR_PLLSRC_HSI (0)
//#define RCC_CFGR_PLLSRC_HSE (1)
//#define RCC_CFGR_PLLXTPRE_PRESCALE_1 (0)
//#define RCC_CFGR_PLLXTPRE_PRESCALE_2 (1)
//#define RCC_CFGR_PLLMUL_MUL_2   (0)
//#define RCC_CFGR_PLLMUL_MUL_3   (1)
//#define RCC_CFGR_PLLMUL_MUL_4   (2)
//#define RCC_CFGR_PLLMUL_MUL_5   (3)
//#define RCC_CFGR_PLLMUL_MUL_6   (4)
//#define RCC_CFGR_PLLMUL_MUL_7   (5)
//#define RCC_CFGR_PLLMUL_MUL_8   (6)
//#define RCC_CFGR_PLLMUL_MUL_9   (7)
//#define RCC_CFGR_PLLMUL_MUL_10  (8)
//#define RCC_CFGR_PLLMUL_MUL_11  (9)
//#define RCC_CFGR_PLLMUL_MUL_12  (10)
//#define RCC_CFGR_PLLMUL_MUL_13  (11)
//#define RCC_CFGR_PLLMUL_MUL_14  (12)
//#define RCC_CFGR_PLLMUL_MUL_15  (13)
//#define RCC_CFGR_PLLMUL_MUL_16  (14)
//#define RCC_CFGR_USBPRE_MUL_1_5 (0) 
//#define RCC_CFGR_USBPRE_MUL_1   (1)
//#define RCC_CFGR_MCO_NOCLOCK  (0)
//#define RCC_CFGR_MCO_SYSCLK   (4)
//#define RCC_CFGR_MCO_HSI      (5)
//#define RCC_CFGR_MCO_HSE      (6)
//#define RCC_CFGR_MCO_PLL      (7)
typedef union 
{
	struct 
	{
		u32 SystemClockSwithch_RW : 2;  // 00:HSI 01:HSE 10:PLL
		u32 SystemClockSwithchStatus_R : 2; // 00:HSI 01:HSE 10:PLL
		u32 AHBPrescale_RW : 4; // 1000-1111:2/4/6....512  Prescale
		u32 APB1Prescale_RW : 3;
		u32 APB2Prescale_RW : 3;
		u32 ADCPrescale_RW : 2;
		u32 PLLSourceClock_RW : 1;
		u32 HSEDivForPLLEntry_RW : 1;
		u32 PLLMultiplicationFactor_RW : 4;
		u32 USBPrescaler_RW : 1;
		u32 Reserved0_RW : 1;
		u32 MicroControllerClockOutputSel_RW : 3;
		u32 Reserved1_RW : 5;
	};
	u32 All;
}_RCC_ClockConfiguration;


typedef union
{
	struct
	{
		u32 LSIReadyInterruptFlag_R : 1;
		u32 LSEReadyInterruptFlag_R : 1;
		u32 HSIReadyInterruptFlag_R : 1;
		u32 HSEReadyInterruptFlag_R : 1;
		u32 PLLReadyInterruptFlag_R : 1;
		u32 Reserved0 : 2;
		u32 ClockSecuritySystemInterruptFlag_R : 1;
		u32 LSIReadyInterruptEnable_RW : 1;
		u32 LSEReadyInterruptEnable_RW : 1;
		u32 HSIReadyInterruptEnable_RW : 1;
		u32 HSEReadyInterruptEnable_RW : 1;
		u32 PLLReadyInterruptEnable_RW : 1;
		u32 Reserved1 : 3;
		u32 LSIReadyInterruptClear_W : 1;
		u32 LSEReadyInterruptClear_W : 1;
		u32 HSIReadyInterruptClear_W : 1;
		u32 HSEReadyInterruptClear_W : 1;
		u32 PLLReadyInterruptClear_W : 1;
		u32 Reserved2 : 2;
		u32 ClockSecuritySystemInterruptClear_W : 1;
		u32 Reserved3 : 8;
	};
	u32 All;
}_RCC_ClockInterrupt;

typedef union 
{
	struct 
	{
		u32 AFIOReset_RW : 1;
		u32 Reserved0 : 1;
		u32 IOPAReset_RW : 1;
		u32 IOPBReset_RW : 1;
		u32 IOPCReset_RW : 1;
		u32 IOPDReset_RW : 1;
		u32 IOPEReset_RW : 1;
		u32 IOPFReset_RW : 1;
		u32 IOPGReset_RW : 1;
		u32 ADC1Reset_RW : 1;
		u32 ADC2Reset_RW : 1;
		u32 TIM1Reset_RW : 1;
		u32 SPI1Reset_RW : 1;
		u32 TIM8Reset_RW : 1;
		u32 USART1Reset_RW : 1;
		u32 ADC3Reset_RW : 1;
		u32 Reserved1 : 16;
	};
	u32 All;
}_RCC_APB2_Peripheral_Reset;

typedef	union
{
	struct
	{
		u32 TIM2Reset_RW : 1;
		u32 TIM3Reset_RW : 1;
		u32 TIM4Reset_RW : 1;
		u32 TIM5Reset_RW : 1;
		u32 TIM6Reset_RW : 1;
		u32 TIM7Reset_RW : 1;
		u32 Reserved0 : 5;
		u32 WWDGReset_RW : 1;
		u32 Reserved1 : 2;
		u32 SPI2Reset_RW : 1;
		u32 SPI3Reset_RW : 1;
		u32 Reserved2 : 1;
		u32 USART2Reset_RW : 1;
		u32 USART3Reset_RW : 1;
		u32 UART4Reset_RW : 1;
		u32 UART5Reset_RW : 1;
		u32 I2C1Reset_RW : 1;
		u32 I2C2Reset_RW : 1;
		u32 USBReset_RW : 1;
		u32 Reserved3 : 1;
		u32 CANReset_RW : 1;
		u32 Reserved4 : 1;
		u32 BKPReset_RW : 1;
		u32 PWRReset_RW : 1;
		u32 DACReset_RW : 1;
		u32 Reserved5 : 2;
	};
	u32 All;
}_RCC_APB1_Peripheral_Reset;

typedef union 
{
	struct 
	{
		u32 DMA1ClockEnable_RW : 1;
		u32 DMA2ClockEnable_RW : 1;
		u32 SRAMClockEnable_RW : 1;
		u32 Reserved0 : 1;
		u32 FLITFClockEnable_RW : 1;
		u32 Reserved1 : 1;
		u32 CRCClockEnable_RW : 1;
		u32 Reserved2 : 1;
		u32 FSMCClockEnable_RW : 1;
		u32 Reserved3 : 1;
		u32 SDIOClockEnable_RW : 1;
		u32 Reserved4 : 21;
	};
	u32 All;
}_RCC_AHB_Peripheral_Clock_Enable;

typedef union 
{
	struct 
	{
		u32 AFIOClockEnable_RW : 1;
		u32 Reserved0 : 1;
		u32 IOPAClockEnable_RW : 1;
		u32 IOPBClockEnable_RW : 1;
		u32 IOPCClockEnable_RW : 1;
		u32 IOPDClockEnable_RW : 1;
		u32 IOPEClockEnable_RW : 1;
		u32 IOPFClockEnable_RW : 1;
		u32 IOPGClockEnable_RW : 1;
		u32 ADC1ClockEnable_RW : 1;
		u32 ADC2ClockEnable_RW : 1;
		u32 TIM1ClockEnable_RW : 1;
		u32 SPI1ClockEnable_RW : 1;
		u32 TIM8ClockEnable_RW : 1;
		u32 USART1ClockEnable_RW : 1;
		u32 ADC3ClockEnable_RW : 1;
		u32 Reserved1 : 16;
	};
	u32 All;
}_RCC_APB2_Peripheral_Clock_Enable;

typedef union
{
	struct
	{
		u32 TIM2ClockEnable_RW : 1;
		u32 TIM3ClockEnable_RW : 1;
		u32 TIM4ClockEnable_RW : 1;
		u32 TIM5ClockEnable_RW : 1;
		u32 TIM6ClockEnable_RW : 1;
		u32 TIM7ClockEnable_RW : 1;
		u32 Reserved0 : 5;
		u32 WWDGClockEnable_RW : 1;
		u32 Reserved1 : 2;
		u32 SPI2ClockEnable_RW : 1;
		u32 SPI3ClockEnable_RW : 1;
		u32 Reserved2 : 1;
		u32 USART2ClockEnable_RW : 1;
		u32 USART3ClockEnable_RW : 1;
		u32 UART4ClockEnable_RW : 1;
		u32 UART5ClockEnable_RW : 1;
		u32 I2C1ClockEnable_RW : 1;
		u32 I2C2ClockEnable_RW : 1;
		u32 USBClockEnable_RW : 1;
		u32 Reserved3 : 1;
		u32 CANClockEnable_RW : 1;
		u32 Reserved4 : 1;
		u32 BKPClockEnable_RW : 1;
		u32 PWRClockEnable_RW : 1;
		u32 DACClockEnable_RW : 1;
		u32 Reserved5 : 2;
	};
	u32 All;
}_RCC_APB1_Peripheral_Clock_Enable;

typedef union 
{
	struct 
	{
		u32 LSEEnable_RW : 1;
		u32 LSEReady_R : 1;
		u32 LSEBypass_RW : 1;
		u32 Reserved0 : 5;
		u32 RTCClockSourceSelection_RW : 2;
		u32 Reserved1 : 5;
		u32 RTCEnable_RW : 1;
		u32 BackupDomainReset_RW : 1;
		u32 Reserved2 : 15;
	};
	u32 All;
}_RCC_BackupDomain_Control;

typedef union 
{
	struct 
	{
		u32 LSIEnable_RW : 1;
		u32 LSIReady_R : 1;
		u32 Reserved0 : 22;
		u32 RemoveAllResetFlag_RW : 1;
		u32 Reserved1 : 1;
		u32 ResetPinResetFlag_RW : 1;
		u32 POR_PDRResetFlag_RW : 1;
		u32 SoftwareResetFlag_RW : 1;
		u32 IndependentWatchdogResetFlag_RW : 1;
		u32 WindowsWatchdodResetFlag_RW : 1;
		u32 LowPowerResetFlag_RW : 1;
	};
	u32 All;
}_RCC_ControlStatus;
typedef struct
{
	__IO _RCC_Control CR;
	__IO _RCC_ClockConfiguration CFGR;
	__IO _RCC_ClockInterrupt CIR;
	__IO _RCC_APB2_Peripheral_Reset APB2RSTR;
	__IO _RCC_APB1_Peripheral_Reset APB1RSTR;
	__IO _RCC_AHB_Peripheral_Clock_Enable AHBENR;
	__IO _RCC_APB2_Peripheral_Clock_Enable APB2ENR;
	__IO _RCC_APB1_Peripheral_Clock_Enable APB1ENR;
	__IO _RCC_BackupDomain_Control BDCR;
	__IO _RCC_ControlStatus CSR;

#ifdef STM32F10X_CL  
	__IO u32 AHBRSTR;
	__IO u32 CFGR2;
#endif /* STM32F10X_CL */ 

#if defined (STM32F10X_LD_VL) || defined (STM32F10X_MD_VL) || defined (STM32F10X_HD_VL)   
	u32 RESERVED0;
	__IO u32 CFGR2;
#endif /* STM32F10X_LD_VL || STM32F10X_MD_VL || STM32F10X_HD_VL */ 
} RCC_TypeDef;

/** 
  * @brief Real-Time Clock
  * @data 2016-7-23 By CRJ
  */
typedef union
{
	struct
	{
		u16 SecondInterruptEnable_RW:1;
		u16 AlarmInterruptEnable_RW:1;
		u16 OverflowInterruptEnable_RW:1;
		u16 Reserved0:13;
	};
	u16 All;
}_RTC_ControlHigh;

typedef union
{
	struct
	{
		u16 SecondFlag_RCW0:1;
		u16 AlarmFlag_RCW0:1;
		u16 OverflowFlag_RCW0:1;
		u16 RegisterSynchronizedFlag_RCW0:1;
		u16 ConfigurationFlag_RW:1;
		u16 RTCOperationOFF_R:1;
		u16 Reserved0:10;
	};
	u16 All;
}_RTC_ControlLow;

typedef union
{
	struct
	{
		u16 PrescalerLoadHigh_W:4;
		u16 Reserved0:12;
	};
	u16 All;
}_RTC_PrescalerLoadHigh;

typedef struct
{
	u16 PrescalerLoadLow_W;
}_RTC_PrescalerLoadLow;

typedef union
{
	struct
	{
		u16 PrescalerDividerHigh_R:4;
		u16 Reserved0:12;
	};
	u16 All;
}_RTC_PrescalerDividerHigh;

typedef struct
{
	u16 PrescalerDividerLow_R;
}_RTC_PrescalerDividerLow;

typedef struct
{
	u16 ConunterHigh_RW;
}_RTC_CounterHigh;

typedef struct
{
	u16 ConunterLow_RW;
}_RTC_CounterLow;

typedef struct
{
	u16 AlarmHigh_RW;
}_RTC_AlarmHigh;

typedef struct
{
	u16 AlarmLow_RW;
}_RTC_AlarmLow;

typedef struct
{
  __IO _RTC_ControlHigh CRH;
  u16  RESERVED0;
  __IO _RTC_ControlLow CRL;
  u16  RESERVED1;
  __IO _RTC_PrescalerLoadHigh PRLH;
  u16  RESERVED2;
  __IO _RTC_PrescalerLoadLow PRLL;
  u16  RESERVED3;
  __IO _RTC_PrescalerDividerHigh DIVH;
  u16  RESERVED4;
  __IO _RTC_PrescalerDividerLow DIVL;
  u16  RESERVED5;
  __IO _RTC_CounterHigh CNTH;
  u16  RESERVED6;
  __IO _RTC_CounterLow CNTL;
  u16  RESERVED7;
  __IO _RTC_AlarmHigh ALRH;
  u16  RESERVED8;
  __IO _RTC_AlarmLow ALRL;
  u16  RESERVED9;
} RTC_TypeDef;

/** 
  * @brief SD host Interface
  * @data 2016-7-23 By CRJ
  */

typedef union
{
	struct
	{
		u32 PowerControl_RW:2;
		u32 Reserved0:30;
	};
	u32 All;
}_SDIO_Power;

typedef union
{
	struct
	{
		u32 ClockDividerFactor_RW:8;
		u32 ClockEnable_RW:1;
		u32 PowerSavingConfiguration_RW:1;
		u32 ClockDividerBypassEnable_RW:1;
		u32 WidebusMode_RW:2;
		u32 SDIO_CKDephasingSelection_RW:1;
		u32 HardwareFlowControlEnable_RW:1;
		u32 Reserved0:17;
	};
	u32 All;
}_SDIO_CLKControl;

typedef struct
{
	u32 CommandArgument;
}_SDIO_Argument;

typedef union
{
	struct
	{
		u32 CommandIndex_RW:6;
		u32 ResponseWait_RW:2;
		u32 CommandPathStateMachineWaitInterruptRequest_RW:1;
		u32 CommandPathStateMachineWaitEndsOfDataTransfer_RW:1;
		u32 CommandPathStateMachineEnable_RW:1;
		u32 SDIOSuspendCommand_RW:1;
		u32 CMDCompletionEnable_RW:1;
		u32 NotInterruptEnable_RW:1;
		u32 CE_ATECommand_RW:1;
		u32 Reserved0:17;
	};
	u32 All;
}_SDIO_Command;

typedef union
{
	struct
	{
		u32 ResponseCommandIndex_R:6;
		u32 Reserved0:26;
	};
	u32 All;
}_SDIO_CommmandResponse;

typedef struct
{
	u32 CardStatus_R;
}_SDIO_Response;

typedef struct
{
	u32 DataTimeoutPeriod_RW;
}_SDIO_DataTimeout;

typedef struct
{
	u32 DataLength_RW;
}_SDIO_DataLength;

typedef union
{
	struct
	{
		u32 DataTransferEnable_RW:1;
		u32 DataTransferDirection_RW:1;
		u32 DataTransferMode_RW:1;
		u32 DMAEnable_RW:1;
		u32 DataBlockSize_RW:4;
		u32 ReadWaitStart_RW:1;
		u32 ReadWaitStop_RW:1;
		u32 ReadWaitMode_RW:1;
		u32 SDIOFunctionEnable_RW:1;
		u32 Reserved0:20;
	};
	u32 All;
}_SDIO_DataControl;

typedef union
{
	struct
	{
		u32 DataCount_RW:25;
		u32 Reserved0:7;
	};
	u32 All;
}_SDIO_DataCount;

typedef union
{
	struct
	{
		u32 CommandResponseReceivedWhenCRCError_R:1;
		u32 DataBlockSentOrReceivedWhenCRCError_R:1;
		u32 CommandResponseTimeout_R:1;
		u32 DataTimeout_R:1;
		u32 TransmitFIFOUnderrunError_R:1;
		u32 ReceiveFIFOOverrunError_R:1;
		u32 CommandResponseWhenCRCCorrect_R:1;
		u32 CommandSent_R:1;
		u32 DataEnd_R:1;
		u32 StartBitNotDetectedOnAllDataSignalsInWidebusMode_R:1;
		u32 DataBlockSentOrReceivedWhenCRCCorrect_R:1;
		u32 CommandTransferInprogress_R:1;
		u32 DataTransmitInprogress_R:1;
		u32 DataReceiveInProgress_R:1;
		u32 TrasnmitFIFOHalfEmpty_R:1;
		u32 ReceivedFIFOHalfFull_R:1;
		u32 TransmitFIFOFull_R:1;
		u32 ReceivFIFOFull_R:1;
		u32 TransmitFIFOEmpty_R:1;
		u32 ReceivedFIFOEmpty_R:1;
		u32 DataAvailableInTransmitFIFO_R:1;
		u32 DataAvailableInReceiveFIFO_R:1;
		u32 SDIOInterruptReceived_R:1;
		u32 CEATACommandCompletionSignalReceivedForCMD61_R:1;
		u32 Reserved0:8;
	};
	u32 All;
}_SDIO_Status;

typedef union
{
	struct
	{
		u32 CommandResponseReceivedWhenCRCError_FlagClear_RW:1;
		u32 DataBlockSentOrReceivedWhenCRCError_FlagClear_RW:1;
		u32 CommandResponseTimeout_FlagClear_RW:1;
		u32 DataTimeout_FlagClear_RW:1;
		u32 TransmitFIFOUnderrunError_FlagClear_RW:1;
		u32 ReceiveFIFOOverrunError_FlagClear_RW:1;
		u32 CommandResponseWhenCRCCorrect_FlagClear_RW:1;
		u32 CommandSent_FlagClear_RW:1;
		u32 DataEnd_FlagClear_RW:1;
		u32 StartBitNotDetectedOnAllDataSignalsInWidebusMode_FlagClear_RW:1;
		u32 DataBlockSentOrReceivedWhenCRCCorrect_FlagClear_RW:1;
		u32 Reserved0:11;
		u32 SDIOInterruptReceived_FlagClear_RW:1;
		u32 CEATACommandCompletionSignalReceivedForCMD61_FlagClear_RW:1;
		u32 Reserved1:8;
	};
	u32 All;
}_SDIO_InterruptClear;

typedef union
{
	struct
	{
		u32 CommandResponseReceivedWhenCRCError_InterrputEnable_RW:1;
		u32 DataBlockSentOrReceivedWhenCRCError_InterrputEnable_RW:1;
		u32 CommandResponseTimeout_InterrputEnable_RW:1;
		u32 DataTimeout_InterrputEnable_RW:1;
		u32 TransmitFIFOUnderrunError_InterrputEnable_RW:1;
		u32 ReceiveFIFOOverrunError_InterrputEnable_RW:1;
		u32 CommandResponseWhenCRCCorrect_InterrputEnable_RW:1;
		u32 CommandSent_InterrputEnable_RW:1;
		u32 DataEnd_InterrputEnable_RW:1;
		u32 StartBitNotDetectedOnAllDataSignalsInWidebusMode_InterrputEnable_RW:1;
		u32 DataBlockSentOrReceivedWhenCRCCorrect_InterrputEnable_RW:1;
		u32 CommandTransferInprogress_InterrputEnable_RW:1;
		u32 DataTransmitInprogress_InterrputEnable_RW:1;
		u32 DataReceiveInProgress_InterrputEnable_RW:1;
		u32 TrasnmitFIFOHalfEmpty_InterrputEnable_RW:1;
		u32 ReceivedFIFOHalfFull_InterrputEnable_RW:1;
		u32 TransmitFIFOFull_InterrputEnable_RW:1;
		u32 ReceivFIFOFull_InterrputEnable_RW:1;
		u32 TransmitFIFOEmpty_InterrputEnable_RW:1;
		u32 ReceivedFIFOEmpty_InterrputEnable_RW:1;
		u32 DataAvailableInTransmitFIFO_InterrputEnable_RW:1;
		u32 DataAvailableInReceiveFIFO_InterrputEnable_RW:1;
		u32 SDIOInterruptReceived_InterrputEnable_RW:1;
		u32 CEATACommandCompletionSignalReceivedForCMD61_InterrputEnable_RW:1;
		u32 Reserved0:8;
	};
	u32 All;
}_SDIO_InterruptMask;

typedef union
{
	struct
	{
		u32 FIFOCountValue_R:24;
		u32 Reserved0:8;
	};
	u32 All;
}_SDIO_FIFOCount;

typedef struct
{
	u32 ReceivedAndTransmitFIFOData;
}_SDIO_FIFOData;

typedef struct
{
  __IO _SDIO_Power POWER;
  __IO _SDIO_CLKControl CLKCR;
  __IO _SDIO_Argument ARG;
  __IO _SDIO_Command CMD;
  __I _SDIO_CommmandResponse RESPCMD;
  __I _SDIO_Response RESP1;
  __I _SDIO_Response RESP2;
  __I _SDIO_Response RESP3;
  __I _SDIO_Response RESP4;
  __IO _SDIO_DataLength DTIMER;
  __IO _SDIO_DataLength DLEN;
  __IO _SDIO_DataControl DCTRL;
  __I _SDIO_DataCount DCOUNT;
  __I _SDIO_Status STA;
  __IO _SDIO_InterruptClear ICR;
  __IO _SDIO_InterruptMask MASK;
  u32  RESERVED0[2];
  __I _SDIO_FIFOCount FIFOCNT;
  u32  RESERVED1[13];
  __IO _SDIO_FIFOData FIFO;
} SDIO_TypeDef;

/** 
  * @brief Serial Peripheral Interface
  */
typedef union
{
	struct 
	{
		u16 ClockPhase_RW:1;
		u16 ClockPolarity_RW:1;
		u16 MasterModeEnable_RW:1;
		u16 BaudratePrescale_RW:3;
		u16 SPIEnable_RW:1;
		u16 FrameFormatLSBSendFirstEnable_RW:1;
		u16 InternalSlaveSelect_RW:1;
		u16 SoftwareSlaveManagement_RW:1;
		u16 RecvOnly_RW:1;
		u16 DataLength_RW:1;
		u16 CRCTransmitNext_RW:1;
		u16 CRCCalculationEnable_RW:1;
		u16 DidirectionalModeOutputEnable_RW:1;
		u16 BidirectionDataModeEnable_RW:1;
	};
	u16 All;
}_SPI_Control_1;

typedef union
{
	struct 
	{
		u16 RxBufferDMAEnable_RW:1;
		u16 TxBufferDMAEnable_RW:1;
		u16 SSOutputEnable_RW:1;
		u16 Reserved0:2;
		u16 ErrorInterruptEnable_RW:1;
		u16 RxBufferNotEmptyInterruptEnable_RW:1;
		u16 TxBufferEmptyInterruptEnable_RW:1;
		u16 Reserved1:8;
	};
	u16 All;
}_SPI_Control_2;

typedef union
{
	struct 
	{
		u16 RecvBufferNotEmpty_R:1;
		u16 TransmitBufferEmpty_R:1;
		u16 ChannelSide_R:1;
		u16 UnderrunFlag_R:1;
		u16 CRCErrorFlag_RCW0:1;
		u16 ModeFault_R:1;
		u16 OverrunFlag_R:1;
		u16 BusyFlag_R:1;
		u16 Reserved0:8;
	};
	u16 All;
}_SPI_Status;

typedef struct
{
  u16 Data_RW;
}_SPI_Data;

typedef struct
{
  u16 CRCPolynomial_RW;
}_SPI_CRC_Polynomial;

typedef struct
{
  u16 RxCRC_R;
}_SPI_Rx_CRC;

typedef struct
{
  u16 TxCRC_R;
}_SPI_Tx_CRC;

typedef union
{
	struct 
	{
		u16 ChannelLength_RW:1;
		u16 I2SDatalengthSelect_RW:2;
		u16 SteadyStateClockPolarity_RW:1;
		u16 I2SStandradSelect_RW:2;
		u16 Reserved0:1;
		u16 PCMFrameSynchronization_RW:1;
		u16 I2SConfigurationMode_RW:2;
		u16 I2SEnable_RW:1;
		u16 I2SModeSelection_RW:1;
		u16 Reserved1:4;
	};
	u16 All;
}_SPI_I2S_Configuration;

typedef union
{
	struct 
	{
		u16 I2SLinerPrescale_RW:8;
		u16 OddFactorForThePrescale_1:1;
		u16 MasterClockOutputEnable_RW:1;
		u16 Reserved0:6;
	};
	u16 All;
}_SPI_I2S_Prescale;

typedef struct
{
  __IO _SPI_Control_1 CR1;
  u16  RESERVED0;
  __IO _SPI_Control_2 CR2;
  u16  RESERVED1;
  __IO _SPI_Status SR;
  u16  RESERVED2;
  __IO _SPI_Data DR;
  u16  RESERVED3;
  __IO _SPI_CRC_Polynomial CRCPR;
  u16  RESERVED4;
  __IO _SPI_Rx_CRC RXCRCR;
  u16  RESERVED5;
  __IO _SPI_Tx_CRC TXCRCR;
  u16  RESERVED6;
  __IO _SPI_I2S_Configuration I2SCFGR;
  u16  RESERVED7;
  __IO _SPI_I2S_Prescale I2SPR;
  u16  RESERVED8;  
} SPI_TypeDef;

/** 
  * @brief 下面只针对高级定时器TIM1/TIM8
  * @Attention 所用的结构体定义是用高级定时器来定义的
  */

typedef union
{
	struct 
	{
		u16 CountEnable_RW:1;
		u16 UpdateDisable_RW:1;
		u16 UpdateRequestSource_RW:1;
		u16 OnePulseMode_RW:1;
		u16 Direction_RW:1;
		u16 CenterAlignedSelection_RW:2;
		u16 AutoReloadPreloadEnable_RW:1;
		u16 CLKDivsion_RW:2;
		u16 Reserved0:6;
	};
	u16 All;
}_TIM_Control_1;

typedef union
{
	struct 
	{
		u16 CaptureComparePreloadedControl_RW:1;
		u16 Reserved0:1;
		u16 CaptureCompareControlUpdateSelection_RW:1;
		u16 CaptureCompareDMASelection_RW:1;
		u16 MasterModeSelection_RW:3;
		u16 TI1Selection_RW:1;
		u16 OutIdleState1_RW:1;
		u16 OutIdleStateN1_RW:1;
		u16 OutIdleState2_RW:1;
		u16 OutIdleStateN2_RW:1;
		u16 OutIdleState3_RW:1;
		u16 OutIdleStateN3_RW:1;
		u16 OutIdleState4_RW:1;
		u16 Reserved1:1;
	};
	u16 All;
}_TIM_Control_2;

typedef union
{
	struct 
	{
		u16 SlaveModeSelection_RW:3;
		u16 Reserved0:1;
		u16 TriggerSelection_RW:3;
		u16 MasterSlaveAction_RW:1;
		u16 ExternalTriggerFilterFrequency_RW:4;
		u16 ExternalTriggerPrescalerIndex_RW:2;
		u16 ExternalClockEnable_RW:1;
		u16 ExternalTriggerPolarity_RW:1;
	};
	u16 All;
}_TIM_SlaveModeControl;

typedef union
{
	struct 
	{
		u16 UpdateInterruptEnable_RW:1;
		u16 CaptureCompareInterruptEnable1_RW:1;
		u16 CaptureCompareInterruptEnable2_RW:1;
		u16 CaptureCompareInterruptEnable3_RW:1;
		u16 CaptureCompareInterruptEnable4_RW:1;
		u16 COMInterruptEnable_RW:1;
		u16 TriggerInterruptEnable_RW:1;
		u16 BreakInterruptEnable_RW:1;
		u16 UpdateDMARequestEnable_RW:1;
		u16 CaptureCompareDMARequestEnable1_RW:1;
		u16 CaptureCompareDMARequestEnable2_RW:1;
		u16 CaptureCompareDMARequestEnable3_RW:1;
		u16 CaptureCompareDMARequestEnable4_RW:1;
		u16 COMDMARequestEnable_RW:1;
		u16 TriggerDMARequestEnable_RW:1;
		u16 Reserved0:1;
	};
	u16 All;
}_TIM_DMA_Interrupt_Enable;

typedef union
{
	struct 
	{
		u16 UpdateInterruptFlag_RCW0:1;
		u16 CaptureCompareInterruptFlag1_RCW0:1;
		u16 CaptureCompareInterruptFlag2_RCW0:1;
		u16 CaptureCompareInterruptFlag3_RCW0:1;
		u16 CaptureCompareInterruptFlag4_RCW0:1;
		u16 COMInterruptFlag_RCW0:1;
		u16 TriggerInterruptFlag_RCW0:1;
		u16 BreakInterruptFlag_RCW0:1;
		u16 Reserved0:1;
		u16 CaptureCompareOvercaptureFlag1_RCW0:1;
		u16 CaptureCompareOvercaptureFlag2_RCW0:1;
		u16 CaptureCompareOvercaptureFlag3_RCW0:1;
		u16 CaptureCompareOvercaptureFlag4_RCW0:1;
		u16 Reserved1:3;
	};
	u16 All;
}_TIM_Status;

typedef union
{
	struct 
	{
		u16 UpdateGeneration_W:1;
		u16 CaptureCompareGeneration1_W:1;
		u16 CaptureCompareGeneration2_W:1;
		u16 CaptureCompareGeneration3_W:1;
		u16 CaptureCompareGeneration4_W:1;
		u16 CaptureCompareControlUpdateGeneration_W:1;
		u16 TriggerGeneration_W:1;
		u16 BreakGeneration_W:1;
		u16 Reserved0:8;
	};
	u16 All;
}_TIM_Event_Generation;

typedef union
{
	struct 
	{
		u16 CaptureCompareSelection1_RW:2;
		u16 OutputCompareFastModeEnable1_RW:1;
		u16 OutputComparePreloadEnable1_RW:1;
		u16 OutputCompareMode1_RW:3;
		u16 OutputCompareClearEnable1_RW:1;
		u16 CaptureCompareSelection2_RW:2;
		u16 OutputCompareFastModeEnable2_RW:1;
		u16 OutputComparePreloadEnable2_RW:1;
		u16 OutputCompareMode2_RW:3;
		u16 OutputCompareClearEnable2_RW:1;
	};
	u16 All;
}_TIM_Capture_Compare_Mode_For_OutputCompareMode_1;

typedef union
{
	struct 
	{
		u16 CaptureCompareSelection1_RW:2;
		u16 InputCapturePrescaler1_RW:2;
		u16 InputCaptureFilter1_RW:4;
		u16 CaptureCompareSelection2_RW:2;
		u16 InputCapturePrescaler2_RW:2;
		u16 InputCaptureFilter2_RW:4;
	};
	u16 All;
}_TIM_Capture_Compare_Mode_For_InputCompareMode_1;

typedef union
{
	struct 
	{
		u16 CaptureCompareSelection3_RW:2;
		u16 OutputCompareFastModeEnable3_RW:1;
		u16 OutputComparePreloadEnable3_RW:1;
		u16 OutputCompareMode3_RW:3;
		u16 OutputCompareClearEnable3_RW:1;
		u16 CaptureCompareSelection4_RW:2;
		u16 OutputCompareFastModeEnable4_RW:1;
		u16 OutputComparePreloadEnable4_RW:1;
		u16 OutputCompareMode4_RW:3;
		u16 OutputCompareClearEnable4_RW:1;
	};
	u16 All;
}_TIM_Capture_Compare_Mode_For_OutputCompareMode_2;

typedef union
{
	struct 
	{
		u16 CaptureCompareSelection3_RW:2;
		u16 InputCapturePrescaler3_RW:2;
		u16 InputCaptureFilter3_RW:4;
		u16 CaptureCompareSelection4_RW:2;
		u16 InputCapturePrescaler4_RW:2;
		u16 InputCaptureFilter4_RW:4;
	};
	u16 All;
}_TIM_Capture_Compare_Mode_For_InputCompareMode_2;

typedef union
{
	struct 
	{
		u16 CaptureCompareOutputEnable1_RW:1;
		u16 CaptureCompareOutputPolarity1_RW:1;
		u16 CaptureCompareComplementaryOutputEnable1:1;
		u16 CaptureCompareComplementaryOutputPolarity1_RW:1;
		u16 CaptureCompareOutputEnable2_RW:1;
		u16 CaptureCompareOutputPolarity2_RW:1;
		u16 CaptureCompareComplementaryOutputEnable2_RW:1;
		u16 CaptureCompareComplementaryOutputPolarity2_RW:1;
		u16 CaptureCompareOutputEnable3_RW:1;
		u16 CaptureCompareOutputPolarity3_RW:1;
		u16 CaptureCompareComplementaryOutputEnable3_RW:1;
		u16 CaptureCompareComplementaryOutputPolarity3_RW:1;
		u16 CaptureCompareOutputEnable4_RW:1;
		u16 CaptureCompareOutputPolarity4_RW:1;
		u16 Reserved0:2;
	};
	u16 All;
}_TIM_Capture_Compare_Enable;

typedef struct 
{
  u16 CurrentCountValue_RW;
}_TIM_Count;

typedef struct
{
	u16 PrescaleValue_RW;
}_TIM_Prescale;

typedef struct
{
	u16 AutoReloadValue_RW;
}_TIM_Atuo_Reload;

typedef union
{
	struct	{
		u16 RepetitionCountValue_RW:8;
		u16 Reserved0:8;
	};
	u16 All;
}_TIM_RepetitonCount;

typedef struct
{
	u16 CaptureCompareValue_RW;
}_TIM_Capture_Compare_Value;

//typedef struct
//{
//	u16 CaptureCompareValue2_RW;
//}_TIM_Capture_Compare_2;

//typedef struct
//{
//	u16 CaptureCompareValue3_RW;
//}_TIM_Capture_Compare_3;

//typedef struct
//{
//	u16 CaptureCompareValue4_RW;
//}_TIM_Capture_Compare_4;

typedef union
{
	struct 
	{
		u16 DeadtimeGeneratorSetup_RW:8;
		u16 LockConfiguration_RW:2;
		u16 OffStateSelectionForIdleMode_RW:1;
		u16 OffStateSelectionForRunMode_RW:1;
		u16 BreakEnable_RW:1;
		u16 BreakPolarity_RW:1;
		u16 AutomaticOutputEnable_RW:1;
		u16 MainOutputEnable_RW:1;
	};
	u16 All;
}_TIM_BreakAndDeadTime;

typedef union
{
	struct 
	{
		u16 DMABaseAddress_RW:5;
		u16 Reserved0:3;
		u16 DMABurstLength_RW:5;
		u16 Reserved1:3;
	};
	u16 All;
}_TIM_DMA_Control;

typedef struct 
{
	u16 DMABurstAccess_RW;
}_TIM_Address_For_Full_Transfer;


typedef struct
{
  __IO _TIM_Control_1 CR1;
  u16  RESERVED0;
  __IO _TIM_Control_2 CR2;
  u16  RESERVED1;
  __IO _TIM_SlaveModeControl SMCR;
  u16  RESERVED2;
  __IO _TIM_DMA_Interrupt_Enable DIER;
  u16  RESERVED3;
  __IO _TIM_Status SR;
  u16  RESERVED4;
  __IO _TIM_Event_Generation EGR;
  u16  RESERVED5;
	union {
  __IO _TIM_Capture_Compare_Mode_For_OutputCompareMode_1 Output_CCMR1;
	__IO _TIM_Capture_Compare_Mode_For_InputCompareMode_1 Input_CCMR1;
	};
  u16  RESERVED6;
	union {
  __IO _TIM_Capture_Compare_Mode_For_OutputCompareMode_2 Output_CCMR2;
	__IO _TIM_Capture_Compare_Mode_For_InputCompareMode_2 Input_CCMR2;
	};
  u16  RESERVED7;
  __IO _TIM_Capture_Compare_Enable CCER;
  u16  RESERVED8;
  __IO _TIM_Count CNT;
  u16  RESERVED9;
  __IO _TIM_Prescale PSC;
  u16  RESERVED10;
  __IO _TIM_Atuo_Reload ARR;
  u16  RESERVED11;
  __IO _TIM_RepetitonCount RCR;
  u16  RESERVED12;
  __IO _TIM_Capture_Compare_Value CCR1;
  u16  RESERVED13;
  __IO _TIM_Capture_Compare_Value CCR2;
  u16  RESERVED14;
  __IO _TIM_Capture_Compare_Value CCR3;
  u16  RESERVED15;
  __IO _TIM_Capture_Compare_Value CCR4;
  u16  RESERVED16;
  __IO _TIM_BreakAndDeadTime BDTR;
  u16  RESERVED17;
  __IO _TIM_DMA_Control DCR;
  u16  RESERVED18;
  __IO _TIM_Address_For_Full_Transfer DMAR;
  u16  RESERVED19;
} TIM_TypeDef;

/**
 * @brief 下面针对普通定时器TIM6/TIM7
 * @data 1 August 2016
 */

typedef union
{
	struct 
	{
		u16 CountEnable_RW:1;
		u16 UpdateDisable_RW:1;
		u16 UpdateRequestSource_RW:1;
		u16 OnePulseMode_RW:1;
		u16 Reserved0:3;
		u16 AutoReloadPreloadEnable_RW:1;
		u16 Reserved1:8;
	};
	u16 All;
}_TIM67_Control_1;

typedef union
{
	struct 
	{
		u16 Reserved0:4;
		u16 MasterModeSelection_RW:3;
		u16 Reserved1:9;
	};
	u16 All;
}_TIM67_Control_2;

typedef union
{
	struct 
	{
		u16 UpdateInterruptEnable_RW:1;
		u16 Reserved0:7;
		u16 UpdateDMARequestEnable_RW:1;
		u16 Reserved1:7;
	};
	u16 All;
}_TIM67_DMA_Interrupt_Enable;

typedef union
{
	struct 
	{
		u16 UpdateInterruptFlag_RCW0:1;
		u16 Reserved0:15;
	};
	u16 All;
}_TIM67_Status;

typedef union
{
	struct 
	{
		u16 UpdateGeneration_W:1;
		u16 Reserved0:15;
	};
	u16 All;
}_TIM67_Event_Generation;

typedef struct 
{
  u16 CurrentCountValue_RW;
}_TIM67_Count;

typedef struct
{
	u16 PrescaleValue_RW;
}_TIM67_Prescale;

typedef struct
{
	u16 AutoReloadValue_RW;
}_TIM67_Atuo_Reload;

typedef struct
{
  __IO _TIM67_Control_1 CR1;
  u16  RESERVED0;
  __IO _TIM67_Control_2 CR2;
  u16  RESERVED1[3];
  __IO _TIM67_DMA_Interrupt_Enable DIER;
  u16  RESERVED2;
  __IO _TIM67_Status SR;
  u16  RESERVED3;
  __IO _TIM67_Event_Generation EGR;
  u16  RESERVED4[7];
  __IO _TIM67_Count CNT;
  u16  RESERVED5;
  __IO _TIM67_Prescale PSC;
  u16  RESERVED6;
  __IO _TIM67_Atuo_Reload ARR;
} TIM67_TypeDef;


/** 
  * @brief 下面只针对高级定时器TIM_2345
  * @Attention 所用的结构体定义是用高级定时器来定义的
  */

typedef union
{
	struct 
	{
		u16 CountEnable_RW:1;
		u16 UpdateDisable_RW:1;
		u16 UpdateRequestSource_RW:1;
		u16 OnePulseMode_RW:1;
		u16 Direction_RW:1;
		u16 CenterAlignedSelection_RW:2;
		u16 AutoReloadPreloadEnable_RW:1;
		u16 CLKDivsion_RW:2;
		u16 Reserved0:6;
	};
	u16 All;
}_TIM2345_Control_1;

typedef union
{
	struct 
	{
		u16 Reserved0:3;
		u16 CaptureCompareDMASelection_RW:1;
		u16 MasterModeSelection_RW:3;
		u16 TI1Selection_RW:1;
		u16 Reserved1:8;
	};
	u16 All;
}_TIM2345_Control_2;

typedef union
{
	struct 
	{
		u16 SlaveModeSelection_RW:3;
		u16 Reserved0:1;
		u16 TriggerSelection_RW:3;
		u16 MasterSlaveAction_RW:1;
		u16 ExternalTriggerFilterFrequency_RW:4;
		u16 ExternalTriggerPrescalerIndex_RW:2;
		u16 ExternalClockEnable_RW:1;
		u16 ExternalTriggerPolarity_RW:1;
	};
	u16 All;
}_TIM2345_SlaveModeControl;

typedef union
{
	struct 
	{
		u16 UpdateInterruptEnable_RW:1;
		u16 CaptureCompareInterruptEnable1_RW:1;
		u16 CaptureCompareInterruptEnable2_RW:1;
		u16 CaptureCompareInterruptEnable3_RW:1;
		u16 CaptureCompareInterruptEnable4_RW:1;
		u16 Reserved0:1;
		u16 TriggerInterruptEnable_RW:1;
		u16 Reserved1:1;
		u16 UpdateDMARequestEnable_RW:1;
		u16 CaptureCompareDMARequestEnable1_RW:1;
		u16 CaptureCompareDMARequestEnable2_RW:1;
		u16 CaptureCompareDMARequestEnable3_RW:1;
		u16 CaptureCompareDMARequestEnable4_RW:1;
		u16 Reserved2:1;
		u16 TriggerDMARequestEnable_RW:1;
		u16 Reserved3:1;
	};
	u16 All;
}_TIM2345_DMA_Interrupt_Enable;

typedef union
{
	struct 
	{
		u16 UpdateInterruptFlag_RCW0:1;
		u16 CaptureCompareInterruptFlag1_RCW0:1;
		u16 CaptureCompareInterruptFlag2_RCW0:1;
		u16 CaptureCompareInterruptFlag3_RCW0:1;
		u16 CaptureCompareInterruptFlag4_RCW0:1;
		u16 Reserved0:1;
		u16 TriggerInterruptFlag_RCW0:1;
		u16 Reserved1:2;
		u16 CaptureCompareOvercaptureFlag1_RCW0:1;
		u16 CaptureCompareOvercaptureFlag2_RCW0:1;
		u16 CaptureCompareOvercaptureFlag3_RCW0:1;
		u16 CaptureCompareOvercaptureFlag4_RCW0:1;
		u16 Reserved2:3;
	};
	u16 All;
}_TIM2345_Status;

typedef union
{
	struct 
	{
		u16 UpdateGeneration_W:1;
		u16 CaptureCompareGeneration1_W:1;
		u16 CaptureCompareGeneration2_W:1;
		u16 CaptureCompareGeneration3_W:1;
		u16 CaptureCompareGeneration4_W:1;
		u16 Reserved0:1;
		u16 TriggerGeneration_W:1;
		u16 Reserved1:9;
	};
	u16 All;
}_TIM2345_Event_Generation;

typedef union
{
	struct 
	{
		u16 CaptureCompareSelection1_RW:2;
		u16 OutputCompareFastModeEnable1_RW:1;
		u16 OutputComparePreloadEnable1_RW:1;
		u16 OutputCompareMode1_RW:3;
		u16 OutputCompareClearEnable1_RW:1;
		u16 CaptureCompareSelection2_RW:2;
		u16 OutputCompareFastModeEnable2_RW:1;
		u16 OutputComparePreloadEnable2_RW:1;
		u16 OutputCompareMode2_RW:3;
		u16 OutputCompareClearEnable2_RW:1;
	};
	u16 All;
}_TIM2345_Capture_Compare_Mode_For_OutputCompareMode_1;

typedef union
{
	struct 
	{
		u16 CaptureCompareSelection1_RW:2;
		u16 InputCapturePrescaler1_RW:2;
		u16 InputCaptureFilter1_RW:4;
		u16 CaptureCompareSelection2_RW:2;
		u16 InputCapturePrescaler2_RW:2;
		u16 InputCaptureFilter2_RW:4;
	};
	u16 All;
}_TIM2345_Capture_Compare_Mode_For_InputCompareMode_1;

typedef union
{
	struct 
	{
		u16 CaptureCompareSelection3_RW:2;
		u16 OutputCompareFastModeEnable3_RW:1;
		u16 OutputComparePreloadEnable3_RW:1;
		u16 OutputCompareMode3_RW:3;
		u16 OutputCompareClearEnable3_RW:1;
		u16 CaptureCompareSelection4_RW:2;
		u16 OutputCompareFastModeEnable4_RW:1;
		u16 OutputComparePreloadEnable4_RW:1;
		u16 OutputCompareMode4_RW:3;
		u16 OutputCompareClearEnable4_RW:1;
	};
	u16 All;
}_TIM2345_Capture_Compare_Mode_For_OutputCompareMode_2;

typedef union
{
	struct 
	{
		u16 CaptureCompareSelection3_RW:2;
		u16 InputCapturePrescaler3_RW:2;
		u16 InputCaptureFilter3_RW:4;
		u16 CaptureCompareSelection4_RW:2;
		u16 InputCapturePrescaler4_RW:2;
		u16 InputCaptureFilter4_RW:4;
	};
	u16 All;
}_TIM2345_Capture_Compare_Mode_For_InputCompareMode_2;

typedef union
{
	struct 
	{
		u16 CaptureCompareOutputEnable1_RW:1;
		u16 CaptureCompareOutputPolarity1_RW:1;
		u16 Reserved0:2;
		u16 CaptureCompareOutputEnable2_RW:1;
		u16 CaptureCompareOutputPolarity2_RW:1;
		u16 Reserved1:2;
		u16 CaptureCompareOutputEnable3_RW:1;
		u16 CaptureCompareOutputPolarity3_RW:1;
		u16 Reserved2:2;
		u16 CaptureCompareOutputEnable4_RW:1;
		u16 CaptureCompareOutputPolarity4_RW:1;
		u16 Reserved3:2;
	};
	u16 All;
}_TIM2345_Capture_Compare_Enable;

typedef struct 
{
  u16 CurrentCountValue_RW;
}_TIM2345_Count;

typedef struct
{
	u16 PrescaleValue_RW;
}_TIM2345_Prescale;

typedef struct
{
	u16 AutoReloadValue_RW;
}_TIM2345_Atuo_Reload;

typedef struct
{
	u16 CaptureCompareValue_RW;
}_TIM2345_Capture_Compare_Value;

//typedef struct
//{
//	u16 CaptureCompareValue2_RW;
//}_TIM2345_Capture_Compare_2;

//typedef struct
//{
//	u16 CaptureCompareValue3_RW;
//}_TIM2345_Capture_Compare_3;

//typedef struct
//{
//	u16 CaptureCompareValue4_RW;
//}_TIM2345_Capture_Compare_4;

typedef union
{
	struct 
	{
		u16 DMABaseAddress_RW:5;
		u16 Reserved0:3;
		u16 DMABurstLength_RW:5;
		u16 Reserved1:3;
	};
	u16 All;
}_TIM2345_DMA_Control;

typedef struct 
{
	u16 DMABurstAccess_RW;
}_TIM2345_Address_For_Full_Transfer;


typedef struct
{
  __IO _TIM2345_Control_1 CR1;
  u16  RESERVED0;
  __IO _TIM2345_Control_2 CR2;
  u16  RESERVED1;
  __IO _TIM2345_SlaveModeControl SMCR;
  u16  RESERVED2;
  __IO _TIM2345_DMA_Interrupt_Enable DIER;
  u16  RESERVED3;
  __IO _TIM2345_Status SR;
  u16  RESERVED4;
  __IO _TIM2345_Event_Generation EGR;
  u16  RESERVED5;
	union {
  __IO _TIM2345_Capture_Compare_Mode_For_OutputCompareMode_1 Output_CCMR1;
	__IO _TIM2345_Capture_Compare_Mode_For_InputCompareMode_1 Input_CCMR1;
	};
  u16  RESERVED6;
	union {
  __IO _TIM2345_Capture_Compare_Mode_For_OutputCompareMode_2 Output_CCMR2;
	__IO _TIM2345_Capture_Compare_Mode_For_InputCompareMode_2 Input_CCMR2;
	};
  u16  RESERVED7;
  __IO _TIM2345_Capture_Compare_Enable CCER;
  u16  RESERVED8;
  __IO _TIM2345_Count CNT;
  u16  RESERVED9;
  __IO _TIM2345_Prescale PSC;
  u16  RESERVED10;
  __IO _TIM2345_Atuo_Reload ARR;
  u16  RESERVED11[3];
  __IO _TIM2345_Capture_Compare_Value CCR1;
  u16  RESERVED13;
  __IO _TIM2345_Capture_Compare_Value CCR2;
  u16  RESERVED14;
  __IO _TIM2345_Capture_Compare_Value CCR3;
  u16  RESERVED15;
  __IO _TIM2345_Capture_Compare_Value CCR4;
  u16  RESERVED16[3];
  __IO _TIM2345_DMA_Control DCR;
  u16  RESERVED18;
  __IO _TIM2345_Address_For_Full_Transfer DMAR;
  u16  RESERVED19;
} TIM2345_TypeDef;
/** 
  * @brief Universal Synchronous Asynchronous Receiver Transmitter
  */
/**
*@brief 特别注意：原来的结构体中用的都是u16来定义的，本人改为u32
*/
typedef union
{
	struct
	{
		u32 ParityError_R:1;
		u32 FramingError_R:1;
		u32 NoiseErrorFlag_R:1;
		u32 OverrunError_R:1;
		u32 IDLELineDetected_R:1;
		u32 ReadDataRegisterNotEmpty_RCW0:1;
		u32 TransmissionComplete_RCW0:1;
		u32 TransmitDataRegisterEmpty_R:1;
		u32 LINBreakDetectionFlag_RCW0:1;
		u32 CTSFlag_RCW0:1;
		u32 Reserved0:22;
	};
	u32 All;
}_USART_Status;


typedef union 
{
  struct
	{
		u32 Data_RW:9;
		u32 Reserved0:23;
	};
	u32 All;
}_USART_Data;

typedef union
{
	struct 
	{
		u32 DIV_Fraction_RW:4;
		u32 DIV_Mantissa_RW:12;
		u32 Reserved0:16;
	};
	u32 All;
}_USART_Buadrate;

typedef union
{
	struct 
	{
		u32 SendBreak_RW:1;
		u32 ReceiverWakeup_RW:1;
		u32 ReceiverEnable_RW:1;
		u32 TransmitterEnable_RW:1;
		u32 IDLEInterruptEnable_RW:1;
		u32 RecvDataRegNotEmptyInterruptEnable_RW:1;
		u32 TransmissionCompleteInterruptEnable_RW:1;
		u32 TransmitDataRegEmptyInterruptEnable_RW:1;
		u32 ParityErrorInterruptEnable_RW:1;
		u32 ParitySelection_RW:1;
		u32 ParityControlEnable_RW:1;
		u32 WakeupMethod_RW:1;
		u32 DataLenth_RW:1;
		u32 USARTEnable_RW:1;
		u32 Reserved0:18;
	};
	u32 All;
}_USART_Control_1;

typedef union
{
	struct 
	{
		u32 LocalAddress_RW:4;
		u32 Reserved0:1;
		u32 LINBreakDetectionLength_RW:1;
		u32 LINBreakDetectionInterruptEnable_RW:1;
		u32 Reserved1:1;
		u32 LastBitClockPulse_RW:1;
		u32 ClockPhase_RW:1;
		u32 ClockPolarity_RW:1;
		u32 ClockEnable_RW:1;
		u32 StopBits_RW:2;
		u32 LINModeEnable_RW:1;
		u32 Reserved2:17;
	};
	u32 All;
}_USART_Control_2;

typedef union
{
	struct 
	{
		u32 ErrorInterruptEnable_RW:1;
		u32 IrDAEnable_RW:1;
		u32 IrDALowPower_RW:1;
		u32 HalfDuplexSeletion_RW:1;
		u32 SmartcardNACKEnable:1;
		u32 SmartcardModeEnable_RW:1;
		u32 DMARecvEnable_RW:1;
		u32 DMATransmitterEnable_RW:1;
		u32 RTSEnable_RW:1;
		u32 CTSEnable_RW:1;
		u32 CTSInterruptEnable_RW:1;
		u32 Reserved0:21;
	};
	u32 All;
}_USART_Control_3;

typedef union
{
	struct 
	{
		u32 PrescalerValueAtIrDAOrSmartcardMode_RW:8;
		u32 GuardTimeValue_RW:8;
		u32 Reserved0:16;
	};
	u32 All;
}_USART_GuardTime_And_Prescaler;

typedef struct
{
  __IO _USART_Status SR;
  __IO _USART_Data DR;
  __IO _USART_Buadrate BRR;
  __IO _USART_Control_1 CR1;
  __IO _USART_Control_2 CR2;
  __IO _USART_Control_3 CR3;
  __IO _USART_GuardTime_And_Prescaler GTPR;
} USART_TypeDef;

/** 
  * @brief Window WATCHDOG
  */

typedef union
{
	struct
	{
		u32 SevenBitCount_RW:7;
		u32 WatchDogActiveEnable_RS:1;
		u32 Reserved0:24;
	};
	u32 All;
}_WWDG_Control;

typedef union
{
	struct
	{
		u32 SevenBitsWindowsValue_RW:7;
		u32 WindowsWatchdogTimeBase_RW:1;
		u32 EarlyWakeupInterruptEnable_RS:1;
		u32 Reserved0:22;
	};
	u32 All;
}_WWDG_Config;
typedef union
{
	struct
	{
		u32 EarlyWakeupInterruptFlag_RWC0:1;
		u32 Reserved0:31;
	};
	u32 All;
}_WWDG_Status;
typedef struct
{
  __IO _WWDG_Control CR;
  __IO _WWDG_Config CFR;
  __IO _WWDG_Status SR;
} WWDG_TypeDef;

typedef struct
{
  __IO uint16_t EP0R;                 /*!< USB Endpoint 0 register,                   Address offset: 0x00 */ 
  __IO uint16_t RESERVED0;            /*!< Reserved */     
  __IO uint16_t EP1R;                 /*!< USB Endpoint 1 register,                   Address offset: 0x04 */
  __IO uint16_t RESERVED1;            /*!< Reserved */       
  __IO uint16_t EP2R;                 /*!< USB Endpoint 2 register,                   Address offset: 0x08 */
  __IO uint16_t RESERVED2;            /*!< Reserved */       
  __IO uint16_t EP3R;                 /*!< USB Endpoint 3 register,                   Address offset: 0x0C */ 
  __IO uint16_t RESERVED3;            /*!< Reserved */       
  __IO uint16_t EP4R;                 /*!< USB Endpoint 4 register,                   Address offset: 0x10 */
  __IO uint16_t RESERVED4;            /*!< Reserved */       
  __IO uint16_t EP5R;                 /*!< USB Endpoint 5 register,                   Address offset: 0x14 */
  __IO uint16_t RESERVED5;            /*!< Reserved */       
  __IO uint16_t EP6R;                 /*!< USB Endpoint 6 register,                   Address offset: 0x18 */
  __IO uint16_t RESERVED6;            /*!< Reserved */       
  __IO uint16_t EP7R;                 /*!< USB Endpoint 7 register,                   Address offset: 0x1C */
  __IO uint16_t RESERVED7[17];        /*!< Reserved */     
  __IO uint16_t CNTR;                 /*!< Control register,                          Address offset: 0x40 */
  __IO uint16_t RESERVED8;            /*!< Reserved */       
  __IO uint16_t ISTR;                 /*!< Interrupt status register,                 Address offset: 0x44 */
  __IO uint16_t RESERVED9;            /*!< Reserved */       
  __IO uint16_t FNR;                  /*!< Frame number register,                     Address offset: 0x48 */
  __IO uint16_t RESERVEDA;            /*!< Reserved */       
  __IO uint16_t DADDR;                /*!< Device address register,                   Address offset: 0x4C */
  __IO uint16_t RESERVEDB;            /*!< Reserved */       
  __IO uint16_t BTABLE;               /*!< Buffer Table address register,             Address offset: 0x50 */
  __IO uint16_t RESERVEDC;            /*!< Reserved */       
} USB_TypeDef;


/**
  * @}
  */
  
#pragma anon_unions  // CRJ ADD :允许结构体和联合体中的匿名变量

/** @addtogroup Peripheral_memory_map
  * @{
  */


#define FLASH_BASE            ((uint32_t)0x08000000) /*!< FLASH base address in the alias region */
#define SRAM_BASE             ((uint32_t)0x20000000) /*!< SRAM base address in the alias region */
#define PERIPH_BASE           ((uint32_t)0x40000000) /*!< Peripheral base address in the alias region */

#define SRAM_BB_BASE          ((uint32_t)0x22000000) /*!< SRAM base address in the bit-band region */
#define PERIPH_BB_BASE        ((uint32_t)0x42000000) /*!< Peripheral base address in the bit-band region */

#define FSMC_R_BASE           ((uint32_t)0xA0000000) /*!< FSMC registers base address */

/*!< Peripheral memory map */
#define APB1PERIPH_BASE       PERIPH_BASE
#define APB2PERIPH_BASE       (PERIPH_BASE + 0x10000)
#define AHBPERIPH_BASE        (PERIPH_BASE + 0x20000)

#define TIM2_BASE             (APB1PERIPH_BASE + 0x0000)
#define TIM3_BASE             (APB1PERIPH_BASE + 0x0400)
#define TIM4_BASE             (APB1PERIPH_BASE + 0x0800)
#define TIM5_BASE             (APB1PERIPH_BASE + 0x0C00)
#define TIM6_BASE             (APB1PERIPH_BASE + 0x1000)
#define TIM7_BASE             (APB1PERIPH_BASE + 0x1400)
#define TIM12_BASE            (APB1PERIPH_BASE + 0x1800)
#define TIM13_BASE            (APB1PERIPH_BASE + 0x1C00)
#define TIM14_BASE            (APB1PERIPH_BASE + 0x2000)
#define RTC_BASE              (APB1PERIPH_BASE + 0x2800)
#define WWDG_BASE             (APB1PERIPH_BASE + 0x2C00)
#define IWDG_BASE             (APB1PERIPH_BASE + 0x3000)
#define SPI2_BASE             (APB1PERIPH_BASE + 0x3800)
#define SPI3_BASE             (APB1PERIPH_BASE + 0x3C00)
#define USART2_BASE           (APB1PERIPH_BASE + 0x4400)
#define USART3_BASE           (APB1PERIPH_BASE + 0x4800)
#define UART4_BASE            (APB1PERIPH_BASE + 0x4C00)
#define UART5_BASE            (APB1PERIPH_BASE + 0x5000)
#define I2C1_BASE             (APB1PERIPH_BASE + 0x5400)
#define I2C2_BASE             (APB1PERIPH_BASE + 0x5800)
#define CAN1_BASE             (APB1PERIPH_BASE + 0x6400)
#define CAN2_BASE             (APB1PERIPH_BASE + 0x6800)
#define BKP_BASE              (APB1PERIPH_BASE + 0x6C00)
#define PWR_BASE              (APB1PERIPH_BASE + 0x7000)
#define DAC_BASE              (APB1PERIPH_BASE + 0x7400)
#define CEC_BASE              (APB1PERIPH_BASE + 0x7800)

#define AFIO_BASE             (APB2PERIPH_BASE + 0x0000)
#define EXTI_BASE             (APB2PERIPH_BASE + 0x0400)
#define GPIOA_BASE            (APB2PERIPH_BASE + 0x0800)  // 每个GPIO地址差：0x0400
#define GPIOB_BASE            (APB2PERIPH_BASE + 0x0C00)
#define GPIOC_BASE            (APB2PERIPH_BASE + 0x1000)
#define GPIOD_BASE            (APB2PERIPH_BASE + 0x1400)
#define GPIOE_BASE            (APB2PERIPH_BASE + 0x1800)
#define GPIOF_BASE            (APB2PERIPH_BASE + 0x1C00)
#define GPIOG_BASE            (APB2PERIPH_BASE + 0x2000)
#define ADC1_BASE             (APB2PERIPH_BASE + 0x2400)
#define ADC2_BASE             (APB2PERIPH_BASE + 0x2800)
#define TIM1_BASE             (APB2PERIPH_BASE + 0x2C00)
#define SPI1_BASE             (APB2PERIPH_BASE + 0x3000)
#define TIM8_BASE             (APB2PERIPH_BASE + 0x3400)
#define USART1_BASE           (APB2PERIPH_BASE + 0x3800)
#define ADC3_BASE             (APB2PERIPH_BASE + 0x3C00)
#define TIM15_BASE            (APB2PERIPH_BASE + 0x4000)
#define TIM16_BASE            (APB2PERIPH_BASE + 0x4400)
#define TIM17_BASE            (APB2PERIPH_BASE + 0x4800)
#define TIM9_BASE             (APB2PERIPH_BASE + 0x4C00)
#define TIM10_BASE            (APB2PERIPH_BASE + 0x5000)
#define TIM11_BASE            (APB2PERIPH_BASE + 0x5400)

#define SDIO_BASE             (PERIPH_BASE + 0x18000)

#define DMA1_BASE             (AHBPERIPH_BASE + 0x0000)
#define DMA1_Channel1_BASE    (AHBPERIPH_BASE + 0x0008)
#define DMA1_Channel2_BASE    (AHBPERIPH_BASE + 0x001C)
#define DMA1_Channel3_BASE    (AHBPERIPH_BASE + 0x0030)
#define DMA1_Channel4_BASE    (AHBPERIPH_BASE + 0x0044)
#define DMA1_Channel5_BASE    (AHBPERIPH_BASE + 0x0058)
#define DMA1_Channel6_BASE    (AHBPERIPH_BASE + 0x006C)
#define DMA1_Channel7_BASE    (AHBPERIPH_BASE + 0x0080)
#define DMA2_BASE             (AHBPERIPH_BASE + 0x0400)
#define DMA2_Channel1_BASE    (AHBPERIPH_BASE + 0x0408)
#define DMA2_Channel2_BASE    (AHBPERIPH_BASE + 0x041C)
#define DMA2_Channel3_BASE    (AHBPERIPH_BASE + 0x0430)
#define DMA2_Channel4_BASE    (AHBPERIPH_BASE + 0x0444)
#define DMA2_Channel5_BASE    (AHBPERIPH_BASE + 0x0458)
#define RCC_BASE              (AHBPERIPH_BASE + 0x1000)
#define CRC_BASE              (AHBPERIPH_BASE + 0x3000)

#define FLASH_R_BASE          (AHBPERIPH_BASE + 0x2000) /*!< Flash registers base address */
#define OB_BASE               ((uint32_t)0x1FFFF800)    /*!< Flash Option Bytes base address */

#define ETH_BASE              (AHBPERIPH_BASE + 0x8000)
#define ETH_MAC_BASE          (ETH_BASE)
#define ETH_MMC_BASE          (ETH_BASE + 0x0100)
#define ETH_PTP_BASE          (ETH_BASE + 0x0700)
#define ETH_DMA_BASE          (ETH_BASE + 0x1000)

#define FSMC_Bank1_R_BASE     (FSMC_R_BASE + 0x0000) /*!< FSMC Bank1 registers base address */
#define FSMC_Bank1E_R_BASE    (FSMC_R_BASE + 0x0104) /*!< FSMC Bank1E registers base address */
#define FSMC_Bank2_R_BASE     (FSMC_R_BASE + 0x0060) /*!< FSMC Bank2 registers base address */
#define FSMC_Bank3_R_BASE     (FSMC_R_BASE + 0x0080) /*!< FSMC Bank3 registers base address */
#define FSMC_Bank4_R_BASE     (FSMC_R_BASE + 0x00A0) /*!< FSMC Bank4 registers base address */

#define DBGMCU_BASE          ((uint32_t)0xE0042000) /*!< Debug MCU registers base address */

/** @addtogroup Peripheral_declaration
  * @{
  */  

#define TIM2                ((TIM2345_TypeDef *) TIM2_BASE)
#define TIM3                ((TIM2345_TypeDef *) TIM3_BASE)
#define TIM4                ((TIM2345_TypeDef *) TIM4_BASE)
#define TIM5                ((TIM2345_TypeDef *) TIM5_BASE)
#define TIM6                ((TIM67_TypeDef *) TIM6_BASE)
#define TIM7                ((TIM67_TypeDef *) TIM7_BASE)
#define TIM12               ((TIM_TypeDef *) TIM12_BASE)
#define TIM13               ((TIM_TypeDef *) TIM13_BASE)
#define TIM14               ((TIM_TypeDef *) TIM14_BASE)
#define RTC                 ((RTC_TypeDef *) RTC_BASE)
#define WWDG                ((WWDG_TypeDef *) WWDG_BASE)
#define IWDG                ((IWDG_TypeDef *) IWDG_BASE)
#define SPI2                ((SPI_TypeDef *) SPI2_BASE)
#define SPI3                ((SPI_TypeDef *) SPI3_BASE)
#define USART2              ((USART_TypeDef *) USART2_BASE)
#define USART3              ((USART_TypeDef *) USART3_BASE)
#define UART4               ((USART_TypeDef *) UART4_BASE)
#define UART5               ((USART_TypeDef *) UART5_BASE)
#define I2C1                ((I2C_TypeDef *) I2C1_BASE)
#define I2C2                ((I2C_TypeDef *) I2C2_BASE)
#define CAN1                ((CAN_TypeDef *) CAN1_BASE)
#define CAN2                ((CAN_TypeDef *) CAN2_BASE)
#define BKP                 ((BKP_TypeDef *) BKP_BASE)
#define PWR                 ((PWR_TypeDef *) PWR_BASE)
#define DAC                 ((DAC_TypeDef *) DAC_BASE)
#define CEC                 ((CEC_TypeDef *) CEC_BASE)
#define AFIO                ((AFIO_TypeDef *) AFIO_BASE)
#define EXTI                ((EXTI_TypeDef *) EXTI_BASE)
#define GPIOA               ((GPIO_TypeDef *) GPIOA_BASE)
#define GPIOB               ((GPIO_TypeDef *) GPIOB_BASE)
#define GPIOC               ((GPIO_TypeDef *) GPIOC_BASE)
#define GPIOD               ((GPIO_TypeDef *) GPIOD_BASE)
#define GPIOE               ((GPIO_TypeDef *) GPIOE_BASE)
#define GPIOF               ((GPIO_TypeDef *) GPIOF_BASE)
#define GPIOG               ((GPIO_TypeDef *) GPIOG_BASE)
#define ADC1                ((ADC_TypeDef *) ADC1_BASE)
#define ADC2                ((ADC_TypeDef *) ADC2_BASE)
#define TIM1                ((TIM_TypeDef *) TIM1_BASE)
#define SPI1                ((SPI_TypeDef *) SPI1_BASE)
#define TIM8                ((TIM_TypeDef *) TIM8_BASE)
#define USART1              ((USART_TypeDef *) USART1_BASE)
#define ADC3                ((ADC_TypeDef *) ADC3_BASE)
#define TIM15               ((TIM_TypeDef *) TIM15_BASE)
#define TIM16               ((TIM_TypeDef *) TIM16_BASE)
#define TIM17               ((TIM_TypeDef *) TIM17_BASE)
#define TIM9                ((TIM_TypeDef *) TIM9_BASE)
#define TIM10               ((TIM_TypeDef *) TIM10_BASE)
#define TIM11               ((TIM_TypeDef *) TIM11_BASE)
#define SDIO                ((SDIO_TypeDef *) SDIO_BASE)
#define DMA1                ((DMA_TypeDef *) DMA1_BASE)
#define DMA2                ((DMA_TypeDef *) DMA2_BASE)
#define DMA1_Channel1       ((DMA_Channel_TypeDef *) DMA1_Channel1_BASE)
#define DMA1_Channel2       ((DMA_Channel_TypeDef *) DMA1_Channel2_BASE)
#define DMA1_Channel3       ((DMA_Channel_TypeDef *) DMA1_Channel3_BASE)
#define DMA1_Channel4       ((DMA_Channel_TypeDef *) DMA1_Channel4_BASE)
#define DMA1_Channel5       ((DMA_Channel_TypeDef *) DMA1_Channel5_BASE)
#define DMA1_Channel6       ((DMA_Channel_TypeDef *) DMA1_Channel6_BASE)
#define DMA1_Channel7       ((DMA_Channel_TypeDef *) DMA1_Channel7_BASE)
#define DMA2_Channel1       ((DMA_Channel_TypeDef *) DMA2_Channel1_BASE)
#define DMA2_Channel2       ((DMA_Channel_TypeDef *) DMA2_Channel2_BASE)
#define DMA2_Channel3       ((DMA_Channel_TypeDef *) DMA2_Channel3_BASE)
#define DMA2_Channel4       ((DMA_Channel_TypeDef *) DMA2_Channel4_BASE)
#define DMA2_Channel5       ((DMA_Channel_TypeDef *) DMA2_Channel5_BASE)
#define RCC                 ((RCC_TypeDef *) RCC_BASE)
#define CRC                 ((CRC_TypeDef *) CRC_BASE)
#define FLASH               ((FLASH_TypeDef *) FLASH_R_BASE)
#define OB                  ((OB_TypeDef *) OB_BASE) 
#define ETH                 ((ETH_TypeDef *) ETH_BASE)
#define FSMC_Bank1          ((FSMC_Bank1_TypeDef *) FSMC_Bank1_R_BASE)
#define FSMC_Bank1E         ((FSMC_Bank1E_TypeDef *) FSMC_Bank1E_R_BASE)
#define FSMC_Bank2          ((FSMC_Bank2_TypeDef *) FSMC_Bank2_R_BASE)
#define FSMC_Bank3          ((FSMC_Bank3_TypeDef *) FSMC_Bank3_R_BASE)
#define FSMC_Bank4          ((FSMC_Bank4_TypeDef *) FSMC_Bank4_R_BASE)
#define DBGMCU              ((DBGMCU_TypeDef *) DBGMCU_BASE)



#define USB_BASE              (APB1PERIPH_BASE + 0x00005C00U) /*!< USB_IP Peripheral Registers base address */
#define USB_PMAADDR           (APB1PERIPH_BASE + 0x00006000U) /*!< USB_IP Packet Memory Area base address */


#define USB                 ((USB_TypeDef *)USB_BASE)

/********************************↓下面定义的Flash的操作块↓**********************************/	
	#define RDP_Key        ((u16)0x00A5)
	#define FLASH_KEY1     ((u32)0x45670123)
	#define FLASH_KEY2     ((u32)0xCDEF89AB)

	typedef union // 用结构体寻址
	{
		u8 IndexLen8[1024];	// 可正常读出，不可写入
		u16 IndexLen16[512];   // 512个16位
	}_Flash_Low_Medium_Density_TypeDef;

	typedef union
	{
		u8 IndexLen8[2048]; // 可正常读出，不可写入
		u16 IndexLen16[1024];  // 1024个16位
	}_Flash_High_Density_TypeDef;

	/**
	 * @brief 只定义了几个Flash块,因为实际中肯定只用后面的存储块
	 * @data 10 August 2016
	 */
	/**
	 * @HighDensityAlgorithm  HighDensity_FlashPageX = (_Flash_High_Density_TypeDef *) 0x08000000+X*0x800) 
	 */

	#define HighDensity_FlashPage(x)    ((_Flash_High_Density_TypeDef *) (0x08000000+x*0x800))  // 通用算法

	/**
	 * @MediumLowDensityAlgorithm  Density_FlashPageX = (_Flash_Low_Medium_Density_TypeDef *) 0x08000000 + X*0x400) 
	 */

	#define LowMediumDensity_FlashPage(x)   ((_Flash_Low_Medium_Density_TypeDef *) (0x08000000 + x*0x400)) // 通用型算法

/********************************↑上面定义的Flash的操作块↑**********************************/	


/******************  Bit definition for RCC_APB2ENR register  *****************/
#define RCC_APB2ENR_AFIOEN_Pos               (0U)                              
#define RCC_APB2ENR_AFIOEN_Msk               (0x1U << RCC_APB2ENR_AFIOEN_Pos)  /*!< 0x00000001 */
#define RCC_APB2ENR_AFIOEN                   RCC_APB2ENR_AFIOEN_Msk            /*!< Alternate Function I/O clock enable */
#define RCC_APB2ENR_IOPAEN_Pos               (2U)                              
#define RCC_APB2ENR_IOPAEN_Msk               (0x1U << RCC_APB2ENR_IOPAEN_Pos)  /*!< 0x00000004 */
#define RCC_APB2ENR_IOPAEN                   RCC_APB2ENR_IOPAEN_Msk            /*!< I/O port A clock enable */
#define RCC_APB2ENR_IOPBEN_Pos               (3U)                              
#define RCC_APB2ENR_IOPBEN_Msk               (0x1U << RCC_APB2ENR_IOPBEN_Pos)  /*!< 0x00000008 */
#define RCC_APB2ENR_IOPBEN                   RCC_APB2ENR_IOPBEN_Msk            /*!< I/O port B clock enable */
#define RCC_APB2ENR_IOPCEN_Pos               (4U)                              
#define RCC_APB2ENR_IOPCEN_Msk               (0x1U << RCC_APB2ENR_IOPCEN_Pos)  /*!< 0x00000010 */
#define RCC_APB2ENR_IOPCEN                   RCC_APB2ENR_IOPCEN_Msk            /*!< I/O port C clock enable */
#define RCC_APB2ENR_IOPDEN_Pos               (5U)                              
#define RCC_APB2ENR_IOPDEN_Msk               (0x1U << RCC_APB2ENR_IOPDEN_Pos)  /*!< 0x00000020 */
#define RCC_APB2ENR_IOPDEN                   RCC_APB2ENR_IOPDEN_Msk            /*!< I/O port D clock enable */
#define RCC_APB2ENR_ADC1EN_Pos               (9U)                              
#define RCC_APB2ENR_ADC1EN_Msk               (0x1U << RCC_APB2ENR_ADC1EN_Pos)  /*!< 0x00000200 */
#define RCC_APB2ENR_ADC1EN                   RCC_APB2ENR_ADC1EN_Msk            /*!< ADC 1 interface clock enable */

#define RCC_APB2ENR_ADC2EN_Pos               (10U)                             
#define RCC_APB2ENR_ADC2EN_Msk               (0x1U << RCC_APB2ENR_ADC2EN_Pos)  /*!< 0x00000400 */
#define RCC_APB2ENR_ADC2EN                   RCC_APB2ENR_ADC2EN_Msk            /*!< ADC 2 interface clock enable */

#define RCC_APB2ENR_TIM1EN_Pos               (11U)                             
#define RCC_APB2ENR_TIM1EN_Msk               (0x1U << RCC_APB2ENR_TIM1EN_Pos)  /*!< 0x00000800 */
#define RCC_APB2ENR_TIM1EN                   RCC_APB2ENR_TIM1EN_Msk            /*!< TIM1 Timer clock enable */
#define RCC_APB2ENR_SPI1EN_Pos               (12U)                             
#define RCC_APB2ENR_SPI1EN_Msk               (0x1U << RCC_APB2ENR_SPI1EN_Pos)  /*!< 0x00001000 */
#define RCC_APB2ENR_SPI1EN                   RCC_APB2ENR_SPI1EN_Msk            /*!< SPI 1 clock enable */
#define RCC_APB2ENR_USART1EN_Pos             (14U)                             
#define RCC_APB2ENR_USART1EN_Msk             (0x1U << RCC_APB2ENR_USART1EN_Pos) /*!< 0x00004000 */
#define RCC_APB2ENR_USART1EN                 RCC_APB2ENR_USART1EN_Msk          /*!< USART1 clock enable */


#define RCC_APB2ENR_IOPEEN_Pos               (6U)                              
#define RCC_APB2ENR_IOPEEN_Msk               (0x1U << RCC_APB2ENR_IOPEEN_Pos)  /*!< 0x00000040 */
#define RCC_APB2ENR_IOPEEN                   RCC_APB2ENR_IOPEEN_Msk            /*!< I/O port E clock enable */




/*****************  Bit definition for RCC_APB1ENR register  ******************/
#define RCC_APB1ENR_TIM2EN_Pos               (0U)                              
#define RCC_APB1ENR_TIM2EN_Msk               (0x1U << RCC_APB1ENR_TIM2EN_Pos)  /*!< 0x00000001 */
#define RCC_APB1ENR_TIM2EN                   RCC_APB1ENR_TIM2EN_Msk            /*!< Timer 2 clock enabled*/
#define RCC_APB1ENR_TIM3EN_Pos               (1U)                              
#define RCC_APB1ENR_TIM3EN_Msk               (0x1U << RCC_APB1ENR_TIM3EN_Pos)  /*!< 0x00000002 */
#define RCC_APB1ENR_TIM3EN                   RCC_APB1ENR_TIM3EN_Msk            /*!< Timer 3 clock enable */
#define RCC_APB1ENR_WWDGEN_Pos               (11U)                             
#define RCC_APB1ENR_WWDGEN_Msk               (0x1U << RCC_APB1ENR_WWDGEN_Pos)  /*!< 0x00000800 */
#define RCC_APB1ENR_WWDGEN                   RCC_APB1ENR_WWDGEN_Msk            /*!< Window Watchdog clock enable */
#define RCC_APB1ENR_USART2EN_Pos             (17U)                             
#define RCC_APB1ENR_USART2EN_Msk             (0x1U << RCC_APB1ENR_USART2EN_Pos) /*!< 0x00020000 */
#define RCC_APB1ENR_USART2EN                 RCC_APB1ENR_USART2EN_Msk          /*!< USART 2 clock enable */
#define RCC_APB1ENR_I2C1EN_Pos               (21U)                             
#define RCC_APB1ENR_I2C1EN_Msk               (0x1U << RCC_APB1ENR_I2C1EN_Pos)  /*!< 0x00200000 */
#define RCC_APB1ENR_I2C1EN                   RCC_APB1ENR_I2C1EN_Msk            /*!< I2C 1 clock enable */

#define RCC_APB1ENR_CAN1EN_Pos               (25U)                             
#define RCC_APB1ENR_CAN1EN_Msk               (0x1U << RCC_APB1ENR_CAN1EN_Pos)  /*!< 0x02000000 */
#define RCC_APB1ENR_CAN1EN                   RCC_APB1ENR_CAN1EN_Msk            /*!< CAN1 clock enable */

#define RCC_APB1ENR_BKPEN_Pos                (27U)                             
#define RCC_APB1ENR_BKPEN_Msk                (0x1U << RCC_APB1ENR_BKPEN_Pos)   /*!< 0x08000000 */
#define RCC_APB1ENR_BKPEN                    RCC_APB1ENR_BKPEN_Msk             /*!< Backup interface clock enable */
#define RCC_APB1ENR_PWREN_Pos                (28U)                             
#define RCC_APB1ENR_PWREN_Msk                (0x1U << RCC_APB1ENR_PWREN_Pos)   /*!< 0x10000000 */
#define RCC_APB1ENR_PWREN                    RCC_APB1ENR_PWREN_Msk             /*!< Power interface clock enable */

#define RCC_APB1ENR_TIM4EN_Pos               (2U)                              
#define RCC_APB1ENR_TIM4EN_Msk               (0x1U << RCC_APB1ENR_TIM4EN_Pos)  /*!< 0x00000004 */
#define RCC_APB1ENR_TIM4EN                   RCC_APB1ENR_TIM4EN_Msk            /*!< Timer 4 clock enable */
#define RCC_APB1ENR_SPI2EN_Pos               (14U)                             
#define RCC_APB1ENR_SPI2EN_Msk               (0x1U << RCC_APB1ENR_SPI2EN_Pos)  /*!< 0x00004000 */
#define RCC_APB1ENR_SPI2EN                   RCC_APB1ENR_SPI2EN_Msk            /*!< SPI 2 clock enable */
#define RCC_APB1ENR_USART3EN_Pos             (18U)                             
#define RCC_APB1ENR_USART3EN_Msk             (0x1U << RCC_APB1ENR_USART3EN_Pos) /*!< 0x00040000 */
#define RCC_APB1ENR_USART3EN                 RCC_APB1ENR_USART3EN_Msk          /*!< USART 3 clock enable */
#define RCC_APB1ENR_I2C2EN_Pos               (22U)                             
#define RCC_APB1ENR_I2C2EN_Msk               (0x1U << RCC_APB1ENR_I2C2EN_Pos)  /*!< 0x00400000 */
#define RCC_APB1ENR_I2C2EN                   RCC_APB1ENR_I2C2EN_Msk            /*!< I2C 2 clock enable */

#define RCC_APB1ENR_USBEN_Pos                (23U)                             
#define RCC_APB1ENR_USBEN_Msk                (0x1U << RCC_APB1ENR_USBEN_Pos)   /*!< 0x00800000 */
#define RCC_APB1ENR_USBEN                    RCC_APB1ENR_USBEN_Msk             /*!< USB Device clock enable */






/*******************  Bit definition for RCC_BDCR register  *******************/
#define RCC_BDCR_LSEON_Pos                   (0U)                              
#define RCC_BDCR_LSEON_Msk                   (0x1U << RCC_BDCR_LSEON_Pos)      /*!< 0x00000001 */
#define RCC_BDCR_LSEON                       RCC_BDCR_LSEON_Msk                /*!< External Low Speed oscillator enable */
#define RCC_BDCR_LSERDY_Pos                  (1U)                              
#define RCC_BDCR_LSERDY_Msk                  (0x1U << RCC_BDCR_LSERDY_Pos)     /*!< 0x00000002 */
#define RCC_BDCR_LSERDY                      RCC_BDCR_LSERDY_Msk               /*!< External Low Speed oscillator Ready */
#define RCC_BDCR_LSEBYP_Pos                  (2U)                              
#define RCC_BDCR_LSEBYP_Msk                  (0x1U << RCC_BDCR_LSEBYP_Pos)     /*!< 0x00000004 */
#define RCC_BDCR_LSEBYP                      RCC_BDCR_LSEBYP_Msk               /*!< External Low Speed oscillator Bypass */

#define RCC_BDCR_RTCSEL_Pos                  (8U)                              
#define RCC_BDCR_RTCSEL_Msk                  (0x3U << RCC_BDCR_RTCSEL_Pos)     /*!< 0x00000300 */
#define RCC_BDCR_RTCSEL                      RCC_BDCR_RTCSEL_Msk               /*!< RTCSEL[1:0] bits (RTC clock source selection) */
#define RCC_BDCR_RTCSEL_0                    (0x1U << RCC_BDCR_RTCSEL_Pos)     /*!< 0x00000100 */
#define RCC_BDCR_RTCSEL_1                    (0x2U << RCC_BDCR_RTCSEL_Pos)     /*!< 0x00000200 */

/*!< RTC congiguration */
#define RCC_BDCR_RTCSEL_NOCLOCK              0x00000000U                       /*!< No clock */
#define RCC_BDCR_RTCSEL_LSE                  0x00000100U                       /*!< LSE oscillator clock used as RTC clock */
#define RCC_BDCR_RTCSEL_LSI                  0x00000200U                       /*!< LSI oscillator clock used as RTC clock */
#define RCC_BDCR_RTCSEL_HSE                  0x00000300U                       /*!< HSE oscillator clock divided by 128 used as RTC clock */

#define RCC_BDCR_RTCEN_Pos                   (15U)                             
#define RCC_BDCR_RTCEN_Msk                   (0x1U << RCC_BDCR_RTCEN_Pos)      /*!< 0x00008000 */
#define RCC_BDCR_RTCEN                       RCC_BDCR_RTCEN_Msk                /*!< RTC clock enable */
#define RCC_BDCR_BDRST_Pos                   (16U)                             
#define RCC_BDCR_BDRST_Msk                   (0x1U << RCC_BDCR_BDRST_Pos)      /*!< 0x00010000 */
#define RCC_BDCR_BDRST                       RCC_BDCR_BDRST_Msk                /*!< Backup domain software reset  */

/*******************  Bit definition for RCC_CSR register  ********************/  
#define RCC_CSR_LSION_Pos                    (0U)                              
#define RCC_CSR_LSION_Msk                    (0x1U << RCC_CSR_LSION_Pos)       /*!< 0x00000001 */
#define RCC_CSR_LSION                        RCC_CSR_LSION_Msk                 /*!< Internal Low Speed oscillator enable */
#define RCC_CSR_LSIRDY_Pos                   (1U)                              
#define RCC_CSR_LSIRDY_Msk                   (0x1U << RCC_CSR_LSIRDY_Pos)      /*!< 0x00000002 */
#define RCC_CSR_LSIRDY                       RCC_CSR_LSIRDY_Msk                /*!< Internal Low Speed oscillator Ready */
#define RCC_CSR_RMVF_Pos                     (24U)                             
#define RCC_CSR_RMVF_Msk                     (0x1U << RCC_CSR_RMVF_Pos)        /*!< 0x01000000 */
#define RCC_CSR_RMVF                         RCC_CSR_RMVF_Msk                  /*!< Remove reset flag */
#define RCC_CSR_PINRSTF_Pos                  (26U)                             
#define RCC_CSR_PINRSTF_Msk                  (0x1U << RCC_CSR_PINRSTF_Pos)     /*!< 0x04000000 */
#define RCC_CSR_PINRSTF                      RCC_CSR_PINRSTF_Msk               /*!< PIN reset flag */
#define RCC_CSR_PORRSTF_Pos                  (27U)                             
#define RCC_CSR_PORRSTF_Msk                  (0x1U << RCC_CSR_PORRSTF_Pos)     /*!< 0x08000000 */
#define RCC_CSR_PORRSTF                      RCC_CSR_PORRSTF_Msk               /*!< POR/PDR reset flag */
#define RCC_CSR_SFTRSTF_Pos                  (28U)                             
#define RCC_CSR_SFTRSTF_Msk                  (0x1U << RCC_CSR_SFTRSTF_Pos)     /*!< 0x10000000 */
#define RCC_CSR_SFTRSTF                      RCC_CSR_SFTRSTF_Msk               /*!< Software Reset flag */
#define RCC_CSR_IWDGRSTF_Pos                 (29U)                             
#define RCC_CSR_IWDGRSTF_Msk                 (0x1U << RCC_CSR_IWDGRSTF_Pos)    /*!< 0x20000000 */
#define RCC_CSR_IWDGRSTF                     RCC_CSR_IWDGRSTF_Msk              /*!< Independent Watchdog reset flag */
#define RCC_CSR_WWDGRSTF_Pos                 (30U)                             
#define RCC_CSR_WWDGRSTF_Msk                 (0x1U << RCC_CSR_WWDGRSTF_Pos)    /*!< 0x40000000 */
#define RCC_CSR_WWDGRSTF                     RCC_CSR_WWDGRSTF_Msk              /*!< Window watchdog reset flag */
#define RCC_CSR_LPWRRSTF_Pos                 (31U)                             
#define RCC_CSR_LPWRRSTF_Msk                 (0x1U << RCC_CSR_LPWRRSTF_Pos)    /*!< 0x80000000 */
#define RCC_CSR_LPWRRSTF                     RCC_CSR_LPWRRSTF_Msk              /*!< Low-Power reset flag */


 
/******************************************************************************/
/*                                                                            */
/*                General Purpose and Alternate Function I/O                  */
/*                                                                            */
/******************************************************************************/

/*******************  Bit definition for GPIO_CRL register  *******************/
#define GPIO_CRL_MODE_Pos                    (0U)                              
#define GPIO_CRL_MODE_Msk                    (0x33333333U << GPIO_CRL_MODE_Pos) /*!< 0x33333333 */
#define GPIO_CRL_MODE                        GPIO_CRL_MODE_Msk                 /*!< Port x mode bits */

#define GPIO_CRL_MODE0_Pos                   (0U)                              
#define GPIO_CRL_MODE0_Msk                   (0x3U << GPIO_CRL_MODE0_Pos)      /*!< 0x00000003 */
#define GPIO_CRL_MODE0                       GPIO_CRL_MODE0_Msk                /*!< MODE0[1:0] bits (Port x mode bits, pin 0) */
#define GPIO_CRL_MODE0_0                     (0x1U << GPIO_CRL_MODE0_Pos)      /*!< 0x00000001 */
#define GPIO_CRL_MODE0_1                     (0x2U << GPIO_CRL_MODE0_Pos)      /*!< 0x00000002 */

#define GPIO_CRL_MODE1_Pos                   (4U)                              
#define GPIO_CRL_MODE1_Msk                   (0x3U << GPIO_CRL_MODE1_Pos)      /*!< 0x00000030 */
#define GPIO_CRL_MODE1                       GPIO_CRL_MODE1_Msk                /*!< MODE1[1:0] bits (Port x mode bits, pin 1) */
#define GPIO_CRL_MODE1_0                     (0x1U << GPIO_CRL_MODE1_Pos)      /*!< 0x00000010 */
#define GPIO_CRL_MODE1_1                     (0x2U << GPIO_CRL_MODE1_Pos)      /*!< 0x00000020 */

#define GPIO_CRL_MODE2_Pos                   (8U)                              
#define GPIO_CRL_MODE2_Msk                   (0x3U << GPIO_CRL_MODE2_Pos)      /*!< 0x00000300 */
#define GPIO_CRL_MODE2                       GPIO_CRL_MODE2_Msk                /*!< MODE2[1:0] bits (Port x mode bits, pin 2) */
#define GPIO_CRL_MODE2_0                     (0x1U << GPIO_CRL_MODE2_Pos)      /*!< 0x00000100 */
#define GPIO_CRL_MODE2_1                     (0x2U << GPIO_CRL_MODE2_Pos)      /*!< 0x00000200 */

#define GPIO_CRL_MODE3_Pos                   (12U)                             
#define GPIO_CRL_MODE3_Msk                   (0x3U << GPIO_CRL_MODE3_Pos)      /*!< 0x00003000 */
#define GPIO_CRL_MODE3                       GPIO_CRL_MODE3_Msk                /*!< MODE3[1:0] bits (Port x mode bits, pin 3) */
#define GPIO_CRL_MODE3_0                     (0x1U << GPIO_CRL_MODE3_Pos)      /*!< 0x00001000 */
#define GPIO_CRL_MODE3_1                     (0x2U << GPIO_CRL_MODE3_Pos)      /*!< 0x00002000 */

#define GPIO_CRL_MODE4_Pos                   (16U)                             
#define GPIO_CRL_MODE4_Msk                   (0x3U << GPIO_CRL_MODE4_Pos)      /*!< 0x00030000 */
#define GPIO_CRL_MODE4                       GPIO_CRL_MODE4_Msk                /*!< MODE4[1:0] bits (Port x mode bits, pin 4) */
#define GPIO_CRL_MODE4_0                     (0x1U << GPIO_CRL_MODE4_Pos)      /*!< 0x00010000 */
#define GPIO_CRL_MODE4_1                     (0x2U << GPIO_CRL_MODE4_Pos)      /*!< 0x00020000 */

#define GPIO_CRL_MODE5_Pos                   (20U)                             
#define GPIO_CRL_MODE5_Msk                   (0x3U << GPIO_CRL_MODE5_Pos)      /*!< 0x00300000 */
#define GPIO_CRL_MODE5                       GPIO_CRL_MODE5_Msk                /*!< MODE5[1:0] bits (Port x mode bits, pin 5) */
#define GPIO_CRL_MODE5_0                     (0x1U << GPIO_CRL_MODE5_Pos)      /*!< 0x00100000 */
#define GPIO_CRL_MODE5_1                     (0x2U << GPIO_CRL_MODE5_Pos)      /*!< 0x00200000 */

#define GPIO_CRL_MODE6_Pos                   (24U)                             
#define GPIO_CRL_MODE6_Msk                   (0x3U << GPIO_CRL_MODE6_Pos)      /*!< 0x03000000 */
#define GPIO_CRL_MODE6                       GPIO_CRL_MODE6_Msk                /*!< MODE6[1:0] bits (Port x mode bits, pin 6) */
#define GPIO_CRL_MODE6_0                     (0x1U << GPIO_CRL_MODE6_Pos)      /*!< 0x01000000 */
#define GPIO_CRL_MODE6_1                     (0x2U << GPIO_CRL_MODE6_Pos)      /*!< 0x02000000 */

#define GPIO_CRL_MODE7_Pos                   (28U)                             
#define GPIO_CRL_MODE7_Msk                   (0x3U << GPIO_CRL_MODE7_Pos)      /*!< 0x30000000 */
#define GPIO_CRL_MODE7                       GPIO_CRL_MODE7_Msk                /*!< MODE7[1:0] bits (Port x mode bits, pin 7) */
#define GPIO_CRL_MODE7_0                     (0x1U << GPIO_CRL_MODE7_Pos)      /*!< 0x10000000 */
#define GPIO_CRL_MODE7_1                     (0x2U << GPIO_CRL_MODE7_Pos)      /*!< 0x20000000 */

#define GPIO_CRL_CNF_Pos                     (2U)                              
#define GPIO_CRL_CNF_Msk                     (0x33333333U << GPIO_CRL_CNF_Pos) /*!< 0xCCCCCCCC */
#define GPIO_CRL_CNF                         GPIO_CRL_CNF_Msk                  /*!< Port x configuration bits */

#define GPIO_CRL_CNF0_Pos                    (2U)                              
#define GPIO_CRL_CNF0_Msk                    (0x3U << GPIO_CRL_CNF0_Pos)       /*!< 0x0000000C */
#define GPIO_CRL_CNF0                        GPIO_CRL_CNF0_Msk                 /*!< CNF0[1:0] bits (Port x configuration bits, pin 0) */
#define GPIO_CRL_CNF0_0                      (0x1U << GPIO_CRL_CNF0_Pos)       /*!< 0x00000004 */
#define GPIO_CRL_CNF0_1                      (0x2U << GPIO_CRL_CNF0_Pos)       /*!< 0x00000008 */

#define GPIO_CRL_CNF1_Pos                    (6U)                              
#define GPIO_CRL_CNF1_Msk                    (0x3U << GPIO_CRL_CNF1_Pos)       /*!< 0x000000C0 */
#define GPIO_CRL_CNF1                        GPIO_CRL_CNF1_Msk                 /*!< CNF1[1:0] bits (Port x configuration bits, pin 1) */
#define GPIO_CRL_CNF1_0                      (0x1U << GPIO_CRL_CNF1_Pos)       /*!< 0x00000040 */
#define GPIO_CRL_CNF1_1                      (0x2U << GPIO_CRL_CNF1_Pos)       /*!< 0x00000080 */

#define GPIO_CRL_CNF2_Pos                    (10U)                             
#define GPIO_CRL_CNF2_Msk                    (0x3U << GPIO_CRL_CNF2_Pos)       /*!< 0x00000C00 */
#define GPIO_CRL_CNF2                        GPIO_CRL_CNF2_Msk                 /*!< CNF2[1:0] bits (Port x configuration bits, pin 2) */
#define GPIO_CRL_CNF2_0                      (0x1U << GPIO_CRL_CNF2_Pos)       /*!< 0x00000400 */
#define GPIO_CRL_CNF2_1                      (0x2U << GPIO_CRL_CNF2_Pos)       /*!< 0x00000800 */

#define GPIO_CRL_CNF3_Pos                    (14U)                             
#define GPIO_CRL_CNF3_Msk                    (0x3U << GPIO_CRL_CNF3_Pos)       /*!< 0x0000C000 */
#define GPIO_CRL_CNF3                        GPIO_CRL_CNF3_Msk                 /*!< CNF3[1:0] bits (Port x configuration bits, pin 3) */
#define GPIO_CRL_CNF3_0                      (0x1U << GPIO_CRL_CNF3_Pos)       /*!< 0x00004000 */
#define GPIO_CRL_CNF3_1                      (0x2U << GPIO_CRL_CNF3_Pos)       /*!< 0x00008000 */

#define GPIO_CRL_CNF4_Pos                    (18U)                             
#define GPIO_CRL_CNF4_Msk                    (0x3U << GPIO_CRL_CNF4_Pos)       /*!< 0x000C0000 */
#define GPIO_CRL_CNF4                        GPIO_CRL_CNF4_Msk                 /*!< CNF4[1:0] bits (Port x configuration bits, pin 4) */
#define GPIO_CRL_CNF4_0                      (0x1U << GPIO_CRL_CNF4_Pos)       /*!< 0x00040000 */
#define GPIO_CRL_CNF4_1                      (0x2U << GPIO_CRL_CNF4_Pos)       /*!< 0x00080000 */

#define GPIO_CRL_CNF5_Pos                    (22U)                             
#define GPIO_CRL_CNF5_Msk                    (0x3U << GPIO_CRL_CNF5_Pos)       /*!< 0x00C00000 */
#define GPIO_CRL_CNF5                        GPIO_CRL_CNF5_Msk                 /*!< CNF5[1:0] bits (Port x configuration bits, pin 5) */
#define GPIO_CRL_CNF5_0                      (0x1U << GPIO_CRL_CNF5_Pos)       /*!< 0x00400000 */
#define GPIO_CRL_CNF5_1                      (0x2U << GPIO_CRL_CNF5_Pos)       /*!< 0x00800000 */

#define GPIO_CRL_CNF6_Pos                    (26U)                             
#define GPIO_CRL_CNF6_Msk                    (0x3U << GPIO_CRL_CNF6_Pos)       /*!< 0x0C000000 */
#define GPIO_CRL_CNF6                        GPIO_CRL_CNF6_Msk                 /*!< CNF6[1:0] bits (Port x configuration bits, pin 6) */
#define GPIO_CRL_CNF6_0                      (0x1U << GPIO_CRL_CNF6_Pos)       /*!< 0x04000000 */
#define GPIO_CRL_CNF6_1                      (0x2U << GPIO_CRL_CNF6_Pos)       /*!< 0x08000000 */

#define GPIO_CRL_CNF7_Pos                    (30U)                             
#define GPIO_CRL_CNF7_Msk                    (0x3U << GPIO_CRL_CNF7_Pos)       /*!< 0xC0000000 */
#define GPIO_CRL_CNF7                        GPIO_CRL_CNF7_Msk                 /*!< CNF7[1:0] bits (Port x configuration bits, pin 7) */
#define GPIO_CRL_CNF7_0                      (0x1U << GPIO_CRL_CNF7_Pos)       /*!< 0x40000000 */
#define GPIO_CRL_CNF7_1                      (0x2U << GPIO_CRL_CNF7_Pos)       /*!< 0x80000000 */

/*******************  Bit definition for GPIO_CRH register  *******************/
#define GPIO_CRH_MODE_Pos                    (0U)                              
#define GPIO_CRH_MODE_Msk                    (0x33333333U << GPIO_CRH_MODE_Pos) /*!< 0x33333333 */
#define GPIO_CRH_MODE                        GPIO_CRH_MODE_Msk                 /*!< Port x mode bits */

#define GPIO_CRH_MODE8_Pos                   (0U)                              
#define GPIO_CRH_MODE8_Msk                   (0x3U << GPIO_CRH_MODE8_Pos)      /*!< 0x00000003 */
#define GPIO_CRH_MODE8                       GPIO_CRH_MODE8_Msk                /*!< MODE8[1:0] bits (Port x mode bits, pin 8) */
#define GPIO_CRH_MODE8_0                     (0x1U << GPIO_CRH_MODE8_Pos)      /*!< 0x00000001 */
#define GPIO_CRH_MODE8_1                     (0x2U << GPIO_CRH_MODE8_Pos)      /*!< 0x00000002 */

#define GPIO_CRH_MODE9_Pos                   (4U)                              
#define GPIO_CRH_MODE9_Msk                   (0x3U << GPIO_CRH_MODE9_Pos)      /*!< 0x00000030 */
#define GPIO_CRH_MODE9                       GPIO_CRH_MODE9_Msk                /*!< MODE9[1:0] bits (Port x mode bits, pin 9) */
#define GPIO_CRH_MODE9_0                     (0x1U << GPIO_CRH_MODE9_Pos)      /*!< 0x00000010 */
#define GPIO_CRH_MODE9_1                     (0x2U << GPIO_CRH_MODE9_Pos)      /*!< 0x00000020 */

#define GPIO_CRH_MODE10_Pos                  (8U)                              
#define GPIO_CRH_MODE10_Msk                  (0x3U << GPIO_CRH_MODE10_Pos)     /*!< 0x00000300 */
#define GPIO_CRH_MODE10                      GPIO_CRH_MODE10_Msk               /*!< MODE10[1:0] bits (Port x mode bits, pin 10) */
#define GPIO_CRH_MODE10_0                    (0x1U << GPIO_CRH_MODE10_Pos)     /*!< 0x00000100 */
#define GPIO_CRH_MODE10_1                    (0x2U << GPIO_CRH_MODE10_Pos)     /*!< 0x00000200 */

#define GPIO_CRH_MODE11_Pos                  (12U)                             
#define GPIO_CRH_MODE11_Msk                  (0x3U << GPIO_CRH_MODE11_Pos)     /*!< 0x00003000 */
#define GPIO_CRH_MODE11                      GPIO_CRH_MODE11_Msk               /*!< MODE11[1:0] bits (Port x mode bits, pin 11) */
#define GPIO_CRH_MODE11_0                    (0x1U << GPIO_CRH_MODE11_Pos)     /*!< 0x00001000 */
#define GPIO_CRH_MODE11_1                    (0x2U << GPIO_CRH_MODE11_Pos)     /*!< 0x00002000 */

#define GPIO_CRH_MODE12_Pos                  (16U)                             
#define GPIO_CRH_MODE12_Msk                  (0x3U << GPIO_CRH_MODE12_Pos)     /*!< 0x00030000 */
#define GPIO_CRH_MODE12                      GPIO_CRH_MODE12_Msk               /*!< MODE12[1:0] bits (Port x mode bits, pin 12) */
#define GPIO_CRH_MODE12_0                    (0x1U << GPIO_CRH_MODE12_Pos)     /*!< 0x00010000 */
#define GPIO_CRH_MODE12_1                    (0x2U << GPIO_CRH_MODE12_Pos)     /*!< 0x00020000 */

#define GPIO_CRH_MODE13_Pos                  (20U)                             
#define GPIO_CRH_MODE13_Msk                  (0x3U << GPIO_CRH_MODE13_Pos)     /*!< 0x00300000 */
#define GPIO_CRH_MODE13                      GPIO_CRH_MODE13_Msk               /*!< MODE13[1:0] bits (Port x mode bits, pin 13) */
#define GPIO_CRH_MODE13_0                    (0x1U << GPIO_CRH_MODE13_Pos)     /*!< 0x00100000 */
#define GPIO_CRH_MODE13_1                    (0x2U << GPIO_CRH_MODE13_Pos)     /*!< 0x00200000 */

#define GPIO_CRH_MODE14_Pos                  (24U)                             
#define GPIO_CRH_MODE14_Msk                  (0x3U << GPIO_CRH_MODE14_Pos)     /*!< 0x03000000 */
#define GPIO_CRH_MODE14                      GPIO_CRH_MODE14_Msk               /*!< MODE14[1:0] bits (Port x mode bits, pin 14) */
#define GPIO_CRH_MODE14_0                    (0x1U << GPIO_CRH_MODE14_Pos)     /*!< 0x01000000 */
#define GPIO_CRH_MODE14_1                    (0x2U << GPIO_CRH_MODE14_Pos)     /*!< 0x02000000 */

#define GPIO_CRH_MODE15_Pos                  (28U)                             
#define GPIO_CRH_MODE15_Msk                  (0x3U << GPIO_CRH_MODE15_Pos)     /*!< 0x30000000 */
#define GPIO_CRH_MODE15                      GPIO_CRH_MODE15_Msk               /*!< MODE15[1:0] bits (Port x mode bits, pin 15) */
#define GPIO_CRH_MODE15_0                    (0x1U << GPIO_CRH_MODE15_Pos)     /*!< 0x10000000 */
#define GPIO_CRH_MODE15_1                    (0x2U << GPIO_CRH_MODE15_Pos)     /*!< 0x20000000 */

#define GPIO_CRH_CNF_Pos                     (2U)                              
#define GPIO_CRH_CNF_Msk                     (0x33333333U << GPIO_CRH_CNF_Pos) /*!< 0xCCCCCCCC */
#define GPIO_CRH_CNF                         GPIO_CRH_CNF_Msk                  /*!< Port x configuration bits */

#define GPIO_CRH_CNF8_Pos                    (2U)                              
#define GPIO_CRH_CNF8_Msk                    (0x3U << GPIO_CRH_CNF8_Pos)       /*!< 0x0000000C */
#define GPIO_CRH_CNF8                        GPIO_CRH_CNF8_Msk                 /*!< CNF8[1:0] bits (Port x configuration bits, pin 8) */
#define GPIO_CRH_CNF8_0                      (0x1U << GPIO_CRH_CNF8_Pos)       /*!< 0x00000004 */
#define GPIO_CRH_CNF8_1                      (0x2U << GPIO_CRH_CNF8_Pos)       /*!< 0x00000008 */

#define GPIO_CRH_CNF9_Pos                    (6U)                              
#define GPIO_CRH_CNF9_Msk                    (0x3U << GPIO_CRH_CNF9_Pos)       /*!< 0x000000C0 */
#define GPIO_CRH_CNF9                        GPIO_CRH_CNF9_Msk                 /*!< CNF9[1:0] bits (Port x configuration bits, pin 9) */
#define GPIO_CRH_CNF9_0                      (0x1U << GPIO_CRH_CNF9_Pos)       /*!< 0x00000040 */
#define GPIO_CRH_CNF9_1                      (0x2U << GPIO_CRH_CNF9_Pos)       /*!< 0x00000080 */

#define GPIO_CRH_CNF10_Pos                   (10U)                             
#define GPIO_CRH_CNF10_Msk                   (0x3U << GPIO_CRH_CNF10_Pos)      /*!< 0x00000C00 */
#define GPIO_CRH_CNF10                       GPIO_CRH_CNF10_Msk                /*!< CNF10[1:0] bits (Port x configuration bits, pin 10) */
#define GPIO_CRH_CNF10_0                     (0x1U << GPIO_CRH_CNF10_Pos)      /*!< 0x00000400 */
#define GPIO_CRH_CNF10_1                     (0x2U << GPIO_CRH_CNF10_Pos)      /*!< 0x00000800 */

#define GPIO_CRH_CNF11_Pos                   (14U)                             
#define GPIO_CRH_CNF11_Msk                   (0x3U << GPIO_CRH_CNF11_Pos)      /*!< 0x0000C000 */
#define GPIO_CRH_CNF11                       GPIO_CRH_CNF11_Msk                /*!< CNF11[1:0] bits (Port x configuration bits, pin 11) */
#define GPIO_CRH_CNF11_0                     (0x1U << GPIO_CRH_CNF11_Pos)      /*!< 0x00004000 */
#define GPIO_CRH_CNF11_1                     (0x2U << GPIO_CRH_CNF11_Pos)      /*!< 0x00008000 */

#define GPIO_CRH_CNF12_Pos                   (18U)                             
#define GPIO_CRH_CNF12_Msk                   (0x3U << GPIO_CRH_CNF12_Pos)      /*!< 0x000C0000 */
#define GPIO_CRH_CNF12                       GPIO_CRH_CNF12_Msk                /*!< CNF12[1:0] bits (Port x configuration bits, pin 12) */
#define GPIO_CRH_CNF12_0                     (0x1U << GPIO_CRH_CNF12_Pos)      /*!< 0x00040000 */
#define GPIO_CRH_CNF12_1                     (0x2U << GPIO_CRH_CNF12_Pos)      /*!< 0x00080000 */

#define GPIO_CRH_CNF13_Pos                   (22U)                             
#define GPIO_CRH_CNF13_Msk                   (0x3U << GPIO_CRH_CNF13_Pos)      /*!< 0x00C00000 */
#define GPIO_CRH_CNF13                       GPIO_CRH_CNF13_Msk                /*!< CNF13[1:0] bits (Port x configuration bits, pin 13) */
#define GPIO_CRH_CNF13_0                     (0x1U << GPIO_CRH_CNF13_Pos)      /*!< 0x00400000 */
#define GPIO_CRH_CNF13_1                     (0x2U << GPIO_CRH_CNF13_Pos)      /*!< 0x00800000 */

#define GPIO_CRH_CNF14_Pos                   (26U)                             
#define GPIO_CRH_CNF14_Msk                   (0x3U << GPIO_CRH_CNF14_Pos)      /*!< 0x0C000000 */
#define GPIO_CRH_CNF14                       GPIO_CRH_CNF14_Msk                /*!< CNF14[1:0] bits (Port x configuration bits, pin 14) */
#define GPIO_CRH_CNF14_0                     (0x1U << GPIO_CRH_CNF14_Pos)      /*!< 0x04000000 */
#define GPIO_CRH_CNF14_1                     (0x2U << GPIO_CRH_CNF14_Pos)      /*!< 0x08000000 */

#define GPIO_CRH_CNF15_Pos                   (30U)                             
#define GPIO_CRH_CNF15_Msk                   (0x3U << GPIO_CRH_CNF15_Pos)      /*!< 0xC0000000 */
#define GPIO_CRH_CNF15                       GPIO_CRH_CNF15_Msk                /*!< CNF15[1:0] bits (Port x configuration bits, pin 15) */
#define GPIO_CRH_CNF15_0                     (0x1U << GPIO_CRH_CNF15_Pos)      /*!< 0x40000000 */
#define GPIO_CRH_CNF15_1                     (0x2U << GPIO_CRH_CNF15_Pos)      /*!< 0x80000000 */

/*!<******************  Bit definition for GPIO_IDR register  *******************/
#define GPIO_IDR_IDR0_Pos                    (0U)                              
#define GPIO_IDR_IDR0_Msk                    (0x1U << GPIO_IDR_IDR0_Pos)       /*!< 0x00000001 */
#define GPIO_IDR_IDR0                        GPIO_IDR_IDR0_Msk                 /*!< Port input data, bit 0 */
#define GPIO_IDR_IDR1_Pos                    (1U)                              
#define GPIO_IDR_IDR1_Msk                    (0x1U << GPIO_IDR_IDR1_Pos)       /*!< 0x00000002 */
#define GPIO_IDR_IDR1                        GPIO_IDR_IDR1_Msk                 /*!< Port input data, bit 1 */
#define GPIO_IDR_IDR2_Pos                    (2U)                              
#define GPIO_IDR_IDR2_Msk                    (0x1U << GPIO_IDR_IDR2_Pos)       /*!< 0x00000004 */
#define GPIO_IDR_IDR2                        GPIO_IDR_IDR2_Msk                 /*!< Port input data, bit 2 */
#define GPIO_IDR_IDR3_Pos                    (3U)                              
#define GPIO_IDR_IDR3_Msk                    (0x1U << GPIO_IDR_IDR3_Pos)       /*!< 0x00000008 */
#define GPIO_IDR_IDR3                        GPIO_IDR_IDR3_Msk                 /*!< Port input data, bit 3 */
#define GPIO_IDR_IDR4_Pos                    (4U)                              
#define GPIO_IDR_IDR4_Msk                    (0x1U << GPIO_IDR_IDR4_Pos)       /*!< 0x00000010 */
#define GPIO_IDR_IDR4                        GPIO_IDR_IDR4_Msk                 /*!< Port input data, bit 4 */
#define GPIO_IDR_IDR5_Pos                    (5U)                              
#define GPIO_IDR_IDR5_Msk                    (0x1U << GPIO_IDR_IDR5_Pos)       /*!< 0x00000020 */
#define GPIO_IDR_IDR5                        GPIO_IDR_IDR5_Msk                 /*!< Port input data, bit 5 */
#define GPIO_IDR_IDR6_Pos                    (6U)                              
#define GPIO_IDR_IDR6_Msk                    (0x1U << GPIO_IDR_IDR6_Pos)       /*!< 0x00000040 */
#define GPIO_IDR_IDR6                        GPIO_IDR_IDR6_Msk                 /*!< Port input data, bit 6 */
#define GPIO_IDR_IDR7_Pos                    (7U)                              
#define GPIO_IDR_IDR7_Msk                    (0x1U << GPIO_IDR_IDR7_Pos)       /*!< 0x00000080 */
#define GPIO_IDR_IDR7                        GPIO_IDR_IDR7_Msk                 /*!< Port input data, bit 7 */
#define GPIO_IDR_IDR8_Pos                    (8U)                              
#define GPIO_IDR_IDR8_Msk                    (0x1U << GPIO_IDR_IDR8_Pos)       /*!< 0x00000100 */
#define GPIO_IDR_IDR8                        GPIO_IDR_IDR8_Msk                 /*!< Port input data, bit 8 */
#define GPIO_IDR_IDR9_Pos                    (9U)                              
#define GPIO_IDR_IDR9_Msk                    (0x1U << GPIO_IDR_IDR9_Pos)       /*!< 0x00000200 */
#define GPIO_IDR_IDR9                        GPIO_IDR_IDR9_Msk                 /*!< Port input data, bit 9 */
#define GPIO_IDR_IDR10_Pos                   (10U)                             
#define GPIO_IDR_IDR10_Msk                   (0x1U << GPIO_IDR_IDR10_Pos)      /*!< 0x00000400 */
#define GPIO_IDR_IDR10                       GPIO_IDR_IDR10_Msk                /*!< Port input data, bit 10 */
#define GPIO_IDR_IDR11_Pos                   (11U)                             
#define GPIO_IDR_IDR11_Msk                   (0x1U << GPIO_IDR_IDR11_Pos)      /*!< 0x00000800 */
#define GPIO_IDR_IDR11                       GPIO_IDR_IDR11_Msk                /*!< Port input data, bit 11 */
#define GPIO_IDR_IDR12_Pos                   (12U)                             
#define GPIO_IDR_IDR12_Msk                   (0x1U << GPIO_IDR_IDR12_Pos)      /*!< 0x00001000 */
#define GPIO_IDR_IDR12                       GPIO_IDR_IDR12_Msk                /*!< Port input data, bit 12 */
#define GPIO_IDR_IDR13_Pos                   (13U)                             
#define GPIO_IDR_IDR13_Msk                   (0x1U << GPIO_IDR_IDR13_Pos)      /*!< 0x00002000 */
#define GPIO_IDR_IDR13                       GPIO_IDR_IDR13_Msk                /*!< Port input data, bit 13 */
#define GPIO_IDR_IDR14_Pos                   (14U)                             
#define GPIO_IDR_IDR14_Msk                   (0x1U << GPIO_IDR_IDR14_Pos)      /*!< 0x00004000 */
#define GPIO_IDR_IDR14                       GPIO_IDR_IDR14_Msk                /*!< Port input data, bit 14 */
#define GPIO_IDR_IDR15_Pos                   (15U)                             
#define GPIO_IDR_IDR15_Msk                   (0x1U << GPIO_IDR_IDR15_Pos)      /*!< 0x00008000 */
#define GPIO_IDR_IDR15                       GPIO_IDR_IDR15_Msk                /*!< Port input data, bit 15 */

/*******************  Bit definition for GPIO_ODR register  *******************/
#define GPIO_ODR_ODR0_Pos                    (0U)                              
#define GPIO_ODR_ODR0_Msk                    (0x1U << GPIO_ODR_ODR0_Pos)       /*!< 0x00000001 */
#define GPIO_ODR_ODR0                        GPIO_ODR_ODR0_Msk                 /*!< Port output data, bit 0 */
#define GPIO_ODR_ODR1_Pos                    (1U)                              
#define GPIO_ODR_ODR1_Msk                    (0x1U << GPIO_ODR_ODR1_Pos)       /*!< 0x00000002 */
#define GPIO_ODR_ODR1                        GPIO_ODR_ODR1_Msk                 /*!< Port output data, bit 1 */
#define GPIO_ODR_ODR2_Pos                    (2U)                              
#define GPIO_ODR_ODR2_Msk                    (0x1U << GPIO_ODR_ODR2_Pos)       /*!< 0x00000004 */
#define GPIO_ODR_ODR2                        GPIO_ODR_ODR2_Msk                 /*!< Port output data, bit 2 */
#define GPIO_ODR_ODR3_Pos                    (3U)                              
#define GPIO_ODR_ODR3_Msk                    (0x1U << GPIO_ODR_ODR3_Pos)       /*!< 0x00000008 */
#define GPIO_ODR_ODR3                        GPIO_ODR_ODR3_Msk                 /*!< Port output data, bit 3 */
#define GPIO_ODR_ODR4_Pos                    (4U)                              
#define GPIO_ODR_ODR4_Msk                    (0x1U << GPIO_ODR_ODR4_Pos)       /*!< 0x00000010 */
#define GPIO_ODR_ODR4                        GPIO_ODR_ODR4_Msk                 /*!< Port output data, bit 4 */
#define GPIO_ODR_ODR5_Pos                    (5U)                              
#define GPIO_ODR_ODR5_Msk                    (0x1U << GPIO_ODR_ODR5_Pos)       /*!< 0x00000020 */
#define GPIO_ODR_ODR5                        GPIO_ODR_ODR5_Msk                 /*!< Port output data, bit 5 */
#define GPIO_ODR_ODR6_Pos                    (6U)                              
#define GPIO_ODR_ODR6_Msk                    (0x1U << GPIO_ODR_ODR6_Pos)       /*!< 0x00000040 */
#define GPIO_ODR_ODR6                        GPIO_ODR_ODR6_Msk                 /*!< Port output data, bit 6 */
#define GPIO_ODR_ODR7_Pos                    (7U)                              
#define GPIO_ODR_ODR7_Msk                    (0x1U << GPIO_ODR_ODR7_Pos)       /*!< 0x00000080 */
#define GPIO_ODR_ODR7                        GPIO_ODR_ODR7_Msk                 /*!< Port output data, bit 7 */
#define GPIO_ODR_ODR8_Pos                    (8U)                              
#define GPIO_ODR_ODR8_Msk                    (0x1U << GPIO_ODR_ODR8_Pos)       /*!< 0x00000100 */
#define GPIO_ODR_ODR8                        GPIO_ODR_ODR8_Msk                 /*!< Port output data, bit 8 */
#define GPIO_ODR_ODR9_Pos                    (9U)                              
#define GPIO_ODR_ODR9_Msk                    (0x1U << GPIO_ODR_ODR9_Pos)       /*!< 0x00000200 */
#define GPIO_ODR_ODR9                        GPIO_ODR_ODR9_Msk                 /*!< Port output data, bit 9 */
#define GPIO_ODR_ODR10_Pos                   (10U)                             
#define GPIO_ODR_ODR10_Msk                   (0x1U << GPIO_ODR_ODR10_Pos)      /*!< 0x00000400 */
#define GPIO_ODR_ODR10                       GPIO_ODR_ODR10_Msk                /*!< Port output data, bit 10 */
#define GPIO_ODR_ODR11_Pos                   (11U)                             
#define GPIO_ODR_ODR11_Msk                   (0x1U << GPIO_ODR_ODR11_Pos)      /*!< 0x00000800 */
#define GPIO_ODR_ODR11                       GPIO_ODR_ODR11_Msk                /*!< Port output data, bit 11 */
#define GPIO_ODR_ODR12_Pos                   (12U)                             
#define GPIO_ODR_ODR12_Msk                   (0x1U << GPIO_ODR_ODR12_Pos)      /*!< 0x00001000 */
#define GPIO_ODR_ODR12                       GPIO_ODR_ODR12_Msk                /*!< Port output data, bit 12 */
#define GPIO_ODR_ODR13_Pos                   (13U)                             
#define GPIO_ODR_ODR13_Msk                   (0x1U << GPIO_ODR_ODR13_Pos)      /*!< 0x00002000 */
#define GPIO_ODR_ODR13                       GPIO_ODR_ODR13_Msk                /*!< Port output data, bit 13 */
#define GPIO_ODR_ODR14_Pos                   (14U)                             
#define GPIO_ODR_ODR14_Msk                   (0x1U << GPIO_ODR_ODR14_Pos)      /*!< 0x00004000 */
#define GPIO_ODR_ODR14                       GPIO_ODR_ODR14_Msk                /*!< Port output data, bit 14 */
#define GPIO_ODR_ODR15_Pos                   (15U)                             
#define GPIO_ODR_ODR15_Msk                   (0x1U << GPIO_ODR_ODR15_Pos)      /*!< 0x00008000 */
#define GPIO_ODR_ODR15                       GPIO_ODR_ODR15_Msk                /*!< Port output data, bit 15 */

/******************  Bit definition for GPIO_BSRR register  *******************/
#define GPIO_BSRR_BS0_Pos                    (0U)                              
#define GPIO_BSRR_BS0_Msk                    (0x1U << GPIO_BSRR_BS0_Pos)       /*!< 0x00000001 */
#define GPIO_BSRR_BS0                        GPIO_BSRR_BS0_Msk                 /*!< Port x Set bit 0 */
#define GPIO_BSRR_BS1_Pos                    (1U)                              
#define GPIO_BSRR_BS1_Msk                    (0x1U << GPIO_BSRR_BS1_Pos)       /*!< 0x00000002 */
#define GPIO_BSRR_BS1                        GPIO_BSRR_BS1_Msk                 /*!< Port x Set bit 1 */
#define GPIO_BSRR_BS2_Pos                    (2U)                              
#define GPIO_BSRR_BS2_Msk                    (0x1U << GPIO_BSRR_BS2_Pos)       /*!< 0x00000004 */
#define GPIO_BSRR_BS2                        GPIO_BSRR_BS2_Msk                 /*!< Port x Set bit 2 */
#define GPIO_BSRR_BS3_Pos                    (3U)                              
#define GPIO_BSRR_BS3_Msk                    (0x1U << GPIO_BSRR_BS3_Pos)       /*!< 0x00000008 */
#define GPIO_BSRR_BS3                        GPIO_BSRR_BS3_Msk                 /*!< Port x Set bit 3 */
#define GPIO_BSRR_BS4_Pos                    (4U)                              
#define GPIO_BSRR_BS4_Msk                    (0x1U << GPIO_BSRR_BS4_Pos)       /*!< 0x00000010 */
#define GPIO_BSRR_BS4                        GPIO_BSRR_BS4_Msk                 /*!< Port x Set bit 4 */
#define GPIO_BSRR_BS5_Pos                    (5U)                              
#define GPIO_BSRR_BS5_Msk                    (0x1U << GPIO_BSRR_BS5_Pos)       /*!< 0x00000020 */
#define GPIO_BSRR_BS5                        GPIO_BSRR_BS5_Msk                 /*!< Port x Set bit 5 */
#define GPIO_BSRR_BS6_Pos                    (6U)                              
#define GPIO_BSRR_BS6_Msk                    (0x1U << GPIO_BSRR_BS6_Pos)       /*!< 0x00000040 */
#define GPIO_BSRR_BS6                        GPIO_BSRR_BS6_Msk                 /*!< Port x Set bit 6 */
#define GPIO_BSRR_BS7_Pos                    (7U)                              
#define GPIO_BSRR_BS7_Msk                    (0x1U << GPIO_BSRR_BS7_Pos)       /*!< 0x00000080 */
#define GPIO_BSRR_BS7                        GPIO_BSRR_BS7_Msk                 /*!< Port x Set bit 7 */
#define GPIO_BSRR_BS8_Pos                    (8U)                              
#define GPIO_BSRR_BS8_Msk                    (0x1U << GPIO_BSRR_BS8_Pos)       /*!< 0x00000100 */
#define GPIO_BSRR_BS8                        GPIO_BSRR_BS8_Msk                 /*!< Port x Set bit 8 */
#define GPIO_BSRR_BS9_Pos                    (9U)                              
#define GPIO_BSRR_BS9_Msk                    (0x1U << GPIO_BSRR_BS9_Pos)       /*!< 0x00000200 */
#define GPIO_BSRR_BS9                        GPIO_BSRR_BS9_Msk                 /*!< Port x Set bit 9 */
#define GPIO_BSRR_BS10_Pos                   (10U)                             
#define GPIO_BSRR_BS10_Msk                   (0x1U << GPIO_BSRR_BS10_Pos)      /*!< 0x00000400 */
#define GPIO_BSRR_BS10                       GPIO_BSRR_BS10_Msk                /*!< Port x Set bit 10 */
#define GPIO_BSRR_BS11_Pos                   (11U)                             
#define GPIO_BSRR_BS11_Msk                   (0x1U << GPIO_BSRR_BS11_Pos)      /*!< 0x00000800 */
#define GPIO_BSRR_BS11                       GPIO_BSRR_BS11_Msk                /*!< Port x Set bit 11 */
#define GPIO_BSRR_BS12_Pos                   (12U)                             
#define GPIO_BSRR_BS12_Msk                   (0x1U << GPIO_BSRR_BS12_Pos)      /*!< 0x00001000 */
#define GPIO_BSRR_BS12                       GPIO_BSRR_BS12_Msk                /*!< Port x Set bit 12 */
#define GPIO_BSRR_BS13_Pos                   (13U)                             
#define GPIO_BSRR_BS13_Msk                   (0x1U << GPIO_BSRR_BS13_Pos)      /*!< 0x00002000 */
#define GPIO_BSRR_BS13                       GPIO_BSRR_BS13_Msk                /*!< Port x Set bit 13 */
#define GPIO_BSRR_BS14_Pos                   (14U)                             
#define GPIO_BSRR_BS14_Msk                   (0x1U << GPIO_BSRR_BS14_Pos)      /*!< 0x00004000 */
#define GPIO_BSRR_BS14                       GPIO_BSRR_BS14_Msk                /*!< Port x Set bit 14 */
#define GPIO_BSRR_BS15_Pos                   (15U)                             
#define GPIO_BSRR_BS15_Msk                   (0x1U << GPIO_BSRR_BS15_Pos)      /*!< 0x00008000 */
#define GPIO_BSRR_BS15                       GPIO_BSRR_BS15_Msk                /*!< Port x Set bit 15 */

#define GPIO_BSRR_BR0_Pos                    (16U)                             
#define GPIO_BSRR_BR0_Msk                    (0x1U << GPIO_BSRR_BR0_Pos)       /*!< 0x00010000 */
#define GPIO_BSRR_BR0                        GPIO_BSRR_BR0_Msk                 /*!< Port x Reset bit 0 */
#define GPIO_BSRR_BR1_Pos                    (17U)                             
#define GPIO_BSRR_BR1_Msk                    (0x1U << GPIO_BSRR_BR1_Pos)       /*!< 0x00020000 */
#define GPIO_BSRR_BR1                        GPIO_BSRR_BR1_Msk                 /*!< Port x Reset bit 1 */
#define GPIO_BSRR_BR2_Pos                    (18U)                             
#define GPIO_BSRR_BR2_Msk                    (0x1U << GPIO_BSRR_BR2_Pos)       /*!< 0x00040000 */
#define GPIO_BSRR_BR2                        GPIO_BSRR_BR2_Msk                 /*!< Port x Reset bit 2 */
#define GPIO_BSRR_BR3_Pos                    (19U)                             
#define GPIO_BSRR_BR3_Msk                    (0x1U << GPIO_BSRR_BR3_Pos)       /*!< 0x00080000 */
#define GPIO_BSRR_BR3                        GPIO_BSRR_BR3_Msk                 /*!< Port x Reset bit 3 */
#define GPIO_BSRR_BR4_Pos                    (20U)                             
#define GPIO_BSRR_BR4_Msk                    (0x1U << GPIO_BSRR_BR4_Pos)       /*!< 0x00100000 */
#define GPIO_BSRR_BR4                        GPIO_BSRR_BR4_Msk                 /*!< Port x Reset bit 4 */
#define GPIO_BSRR_BR5_Pos                    (21U)                             
#define GPIO_BSRR_BR5_Msk                    (0x1U << GPIO_BSRR_BR5_Pos)       /*!< 0x00200000 */
#define GPIO_BSRR_BR5                        GPIO_BSRR_BR5_Msk                 /*!< Port x Reset bit 5 */
#define GPIO_BSRR_BR6_Pos                    (22U)                             
#define GPIO_BSRR_BR6_Msk                    (0x1U << GPIO_BSRR_BR6_Pos)       /*!< 0x00400000 */
#define GPIO_BSRR_BR6                        GPIO_BSRR_BR6_Msk                 /*!< Port x Reset bit 6 */
#define GPIO_BSRR_BR7_Pos                    (23U)                             
#define GPIO_BSRR_BR7_Msk                    (0x1U << GPIO_BSRR_BR7_Pos)       /*!< 0x00800000 */
#define GPIO_BSRR_BR7                        GPIO_BSRR_BR7_Msk                 /*!< Port x Reset bit 7 */
#define GPIO_BSRR_BR8_Pos                    (24U)                             
#define GPIO_BSRR_BR8_Msk                    (0x1U << GPIO_BSRR_BR8_Pos)       /*!< 0x01000000 */
#define GPIO_BSRR_BR8                        GPIO_BSRR_BR8_Msk                 /*!< Port x Reset bit 8 */
#define GPIO_BSRR_BR9_Pos                    (25U)                             
#define GPIO_BSRR_BR9_Msk                    (0x1U << GPIO_BSRR_BR9_Pos)       /*!< 0x02000000 */
#define GPIO_BSRR_BR9                        GPIO_BSRR_BR9_Msk                 /*!< Port x Reset bit 9 */
#define GPIO_BSRR_BR10_Pos                   (26U)                             
#define GPIO_BSRR_BR10_Msk                   (0x1U << GPIO_BSRR_BR10_Pos)      /*!< 0x04000000 */
#define GPIO_BSRR_BR10                       GPIO_BSRR_BR10_Msk                /*!< Port x Reset bit 10 */
#define GPIO_BSRR_BR11_Pos                   (27U)                             
#define GPIO_BSRR_BR11_Msk                   (0x1U << GPIO_BSRR_BR11_Pos)      /*!< 0x08000000 */
#define GPIO_BSRR_BR11                       GPIO_BSRR_BR11_Msk                /*!< Port x Reset bit 11 */
#define GPIO_BSRR_BR12_Pos                   (28U)                             
#define GPIO_BSRR_BR12_Msk                   (0x1U << GPIO_BSRR_BR12_Pos)      /*!< 0x10000000 */
#define GPIO_BSRR_BR12                       GPIO_BSRR_BR12_Msk                /*!< Port x Reset bit 12 */
#define GPIO_BSRR_BR13_Pos                   (29U)                             
#define GPIO_BSRR_BR13_Msk                   (0x1U << GPIO_BSRR_BR13_Pos)      /*!< 0x20000000 */
#define GPIO_BSRR_BR13                       GPIO_BSRR_BR13_Msk                /*!< Port x Reset bit 13 */
#define GPIO_BSRR_BR14_Pos                   (30U)                             
#define GPIO_BSRR_BR14_Msk                   (0x1U << GPIO_BSRR_BR14_Pos)      /*!< 0x40000000 */
#define GPIO_BSRR_BR14                       GPIO_BSRR_BR14_Msk                /*!< Port x Reset bit 14 */
#define GPIO_BSRR_BR15_Pos                   (31U)                             
#define GPIO_BSRR_BR15_Msk                   (0x1U << GPIO_BSRR_BR15_Pos)      /*!< 0x80000000 */
#define GPIO_BSRR_BR15                       GPIO_BSRR_BR15_Msk                /*!< Port x Reset bit 15 */

/*******************  Bit definition for GPIO_BRR register  *******************/
#define GPIO_BRR_BR0_Pos                     (0U)                              
#define GPIO_BRR_BR0_Msk                     (0x1U << GPIO_BRR_BR0_Pos)        /*!< 0x00000001 */
#define GPIO_BRR_BR0                         GPIO_BRR_BR0_Msk                  /*!< Port x Reset bit 0 */
#define GPIO_BRR_BR1_Pos                     (1U)                              
#define GPIO_BRR_BR1_Msk                     (0x1U << GPIO_BRR_BR1_Pos)        /*!< 0x00000002 */
#define GPIO_BRR_BR1                         GPIO_BRR_BR1_Msk                  /*!< Port x Reset bit 1 */
#define GPIO_BRR_BR2_Pos                     (2U)                              
#define GPIO_BRR_BR2_Msk                     (0x1U << GPIO_BRR_BR2_Pos)        /*!< 0x00000004 */
#define GPIO_BRR_BR2                         GPIO_BRR_BR2_Msk                  /*!< Port x Reset bit 2 */
#define GPIO_BRR_BR3_Pos                     (3U)                              
#define GPIO_BRR_BR3_Msk                     (0x1U << GPIO_BRR_BR3_Pos)        /*!< 0x00000008 */
#define GPIO_BRR_BR3                         GPIO_BRR_BR3_Msk                  /*!< Port x Reset bit 3 */
#define GPIO_BRR_BR4_Pos                     (4U)                              
#define GPIO_BRR_BR4_Msk                     (0x1U << GPIO_BRR_BR4_Pos)        /*!< 0x00000010 */
#define GPIO_BRR_BR4                         GPIO_BRR_BR4_Msk                  /*!< Port x Reset bit 4 */
#define GPIO_BRR_BR5_Pos                     (5U)                              
#define GPIO_BRR_BR5_Msk                     (0x1U << GPIO_BRR_BR5_Pos)        /*!< 0x00000020 */
#define GPIO_BRR_BR5                         GPIO_BRR_BR5_Msk                  /*!< Port x Reset bit 5 */
#define GPIO_BRR_BR6_Pos                     (6U)                              
#define GPIO_BRR_BR6_Msk                     (0x1U << GPIO_BRR_BR6_Pos)        /*!< 0x00000040 */
#define GPIO_BRR_BR6                         GPIO_BRR_BR6_Msk                  /*!< Port x Reset bit 6 */
#define GPIO_BRR_BR7_Pos                     (7U)                              
#define GPIO_BRR_BR7_Msk                     (0x1U << GPIO_BRR_BR7_Pos)        /*!< 0x00000080 */
#define GPIO_BRR_BR7                         GPIO_BRR_BR7_Msk                  /*!< Port x Reset bit 7 */
#define GPIO_BRR_BR8_Pos                     (8U)                              
#define GPIO_BRR_BR8_Msk                     (0x1U << GPIO_BRR_BR8_Pos)        /*!< 0x00000100 */
#define GPIO_BRR_BR8                         GPIO_BRR_BR8_Msk                  /*!< Port x Reset bit 8 */
#define GPIO_BRR_BR9_Pos                     (9U)                              
#define GPIO_BRR_BR9_Msk                     (0x1U << GPIO_BRR_BR9_Pos)        /*!< 0x00000200 */
#define GPIO_BRR_BR9                         GPIO_BRR_BR9_Msk                  /*!< Port x Reset bit 9 */
#define GPIO_BRR_BR10_Pos                    (10U)                             
#define GPIO_BRR_BR10_Msk                    (0x1U << GPIO_BRR_BR10_Pos)       /*!< 0x00000400 */
#define GPIO_BRR_BR10                        GPIO_BRR_BR10_Msk                 /*!< Port x Reset bit 10 */
#define GPIO_BRR_BR11_Pos                    (11U)                             
#define GPIO_BRR_BR11_Msk                    (0x1U << GPIO_BRR_BR11_Pos)       /*!< 0x00000800 */
#define GPIO_BRR_BR11                        GPIO_BRR_BR11_Msk                 /*!< Port x Reset bit 11 */
#define GPIO_BRR_BR12_Pos                    (12U)                             
#define GPIO_BRR_BR12_Msk                    (0x1U << GPIO_BRR_BR12_Pos)       /*!< 0x00001000 */
#define GPIO_BRR_BR12                        GPIO_BRR_BR12_Msk                 /*!< Port x Reset bit 12 */
#define GPIO_BRR_BR13_Pos                    (13U)                             
#define GPIO_BRR_BR13_Msk                    (0x1U << GPIO_BRR_BR13_Pos)       /*!< 0x00002000 */
#define GPIO_BRR_BR13                        GPIO_BRR_BR13_Msk                 /*!< Port x Reset bit 13 */
#define GPIO_BRR_BR14_Pos                    (14U)                             
#define GPIO_BRR_BR14_Msk                    (0x1U << GPIO_BRR_BR14_Pos)       /*!< 0x00004000 */
#define GPIO_BRR_BR14                        GPIO_BRR_BR14_Msk                 /*!< Port x Reset bit 14 */
#define GPIO_BRR_BR15_Pos                    (15U)                             
#define GPIO_BRR_BR15_Msk                    (0x1U << GPIO_BRR_BR15_Pos)       /*!< 0x00008000 */
#define GPIO_BRR_BR15                        GPIO_BRR_BR15_Msk                 /*!< Port x Reset bit 15 */

/******************  Bit definition for GPIO_LCKR register  *******************/
#define GPIO_LCKR_LCK0_Pos                   (0U)                              
#define GPIO_LCKR_LCK0_Msk                   (0x1U << GPIO_LCKR_LCK0_Pos)      /*!< 0x00000001 */
#define GPIO_LCKR_LCK0                       GPIO_LCKR_LCK0_Msk                /*!< Port x Lock bit 0 */
#define GPIO_LCKR_LCK1_Pos                   (1U)                              
#define GPIO_LCKR_LCK1_Msk                   (0x1U << GPIO_LCKR_LCK1_Pos)      /*!< 0x00000002 */
#define GPIO_LCKR_LCK1                       GPIO_LCKR_LCK1_Msk                /*!< Port x Lock bit 1 */
#define GPIO_LCKR_LCK2_Pos                   (2U)                              
#define GPIO_LCKR_LCK2_Msk                   (0x1U << GPIO_LCKR_LCK2_Pos)      /*!< 0x00000004 */
#define GPIO_LCKR_LCK2                       GPIO_LCKR_LCK2_Msk                /*!< Port x Lock bit 2 */
#define GPIO_LCKR_LCK3_Pos                   (3U)                              
#define GPIO_LCKR_LCK3_Msk                   (0x1U << GPIO_LCKR_LCK3_Pos)      /*!< 0x00000008 */
#define GPIO_LCKR_LCK3                       GPIO_LCKR_LCK3_Msk                /*!< Port x Lock bit 3 */
#define GPIO_LCKR_LCK4_Pos                   (4U)                              
#define GPIO_LCKR_LCK4_Msk                   (0x1U << GPIO_LCKR_LCK4_Pos)      /*!< 0x00000010 */
#define GPIO_LCKR_LCK4                       GPIO_LCKR_LCK4_Msk                /*!< Port x Lock bit 4 */
#define GPIO_LCKR_LCK5_Pos                   (5U)                              
#define GPIO_LCKR_LCK5_Msk                   (0x1U << GPIO_LCKR_LCK5_Pos)      /*!< 0x00000020 */
#define GPIO_LCKR_LCK5                       GPIO_LCKR_LCK5_Msk                /*!< Port x Lock bit 5 */
#define GPIO_LCKR_LCK6_Pos                   (6U)                              
#define GPIO_LCKR_LCK6_Msk                   (0x1U << GPIO_LCKR_LCK6_Pos)      /*!< 0x00000040 */
#define GPIO_LCKR_LCK6                       GPIO_LCKR_LCK6_Msk                /*!< Port x Lock bit 6 */
#define GPIO_LCKR_LCK7_Pos                   (7U)                              
#define GPIO_LCKR_LCK7_Msk                   (0x1U << GPIO_LCKR_LCK7_Pos)      /*!< 0x00000080 */
#define GPIO_LCKR_LCK7                       GPIO_LCKR_LCK7_Msk                /*!< Port x Lock bit 7 */
#define GPIO_LCKR_LCK8_Pos                   (8U)                              
#define GPIO_LCKR_LCK8_Msk                   (0x1U << GPIO_LCKR_LCK8_Pos)      /*!< 0x00000100 */
#define GPIO_LCKR_LCK8                       GPIO_LCKR_LCK8_Msk                /*!< Port x Lock bit 8 */
#define GPIO_LCKR_LCK9_Pos                   (9U)                              
#define GPIO_LCKR_LCK9_Msk                   (0x1U << GPIO_LCKR_LCK9_Pos)      /*!< 0x00000200 */
#define GPIO_LCKR_LCK9                       GPIO_LCKR_LCK9_Msk                /*!< Port x Lock bit 9 */
#define GPIO_LCKR_LCK10_Pos                  (10U)                             
#define GPIO_LCKR_LCK10_Msk                  (0x1U << GPIO_LCKR_LCK10_Pos)     /*!< 0x00000400 */
#define GPIO_LCKR_LCK10                      GPIO_LCKR_LCK10_Msk               /*!< Port x Lock bit 10 */
#define GPIO_LCKR_LCK11_Pos                  (11U)                             
#define GPIO_LCKR_LCK11_Msk                  (0x1U << GPIO_LCKR_LCK11_Pos)     /*!< 0x00000800 */
#define GPIO_LCKR_LCK11                      GPIO_LCKR_LCK11_Msk               /*!< Port x Lock bit 11 */
#define GPIO_LCKR_LCK12_Pos                  (12U)                             
#define GPIO_LCKR_LCK12_Msk                  (0x1U << GPIO_LCKR_LCK12_Pos)     /*!< 0x00001000 */
#define GPIO_LCKR_LCK12                      GPIO_LCKR_LCK12_Msk               /*!< Port x Lock bit 12 */
#define GPIO_LCKR_LCK13_Pos                  (13U)                             
#define GPIO_LCKR_LCK13_Msk                  (0x1U << GPIO_LCKR_LCK13_Pos)     /*!< 0x00002000 */
#define GPIO_LCKR_LCK13                      GPIO_LCKR_LCK13_Msk               /*!< Port x Lock bit 13 */
#define GPIO_LCKR_LCK14_Pos                  (14U)                             
#define GPIO_LCKR_LCK14_Msk                  (0x1U << GPIO_LCKR_LCK14_Pos)     /*!< 0x00004000 */
#define GPIO_LCKR_LCK14                      GPIO_LCKR_LCK14_Msk               /*!< Port x Lock bit 14 */
#define GPIO_LCKR_LCK15_Pos                  (15U)                             
#define GPIO_LCKR_LCK15_Msk                  (0x1U << GPIO_LCKR_LCK15_Pos)     /*!< 0x00008000 */
#define GPIO_LCKR_LCK15                      GPIO_LCKR_LCK15_Msk               /*!< Port x Lock bit 15 */
#define GPIO_LCKR_LCKK_Pos                   (16U)                             
#define GPIO_LCKR_LCKK_Msk                   (0x1U << GPIO_LCKR_LCKK_Pos)      /*!< 0x00010000 */
#define GPIO_LCKR_LCKK                       GPIO_LCKR_LCKK_Msk                /*!< Lock key */

/*----------------------------------------------------------------------------*/
/*                                                                            */
/*                                   USB Device FS                            */
/*                                                                            */
/******************************************************************************/


/* bit positions */ 
#define USB_EP_CTR_RX_Pos                       (15U)                          
#define USB_EP_CTR_RX_Msk                       (0x1U << USB_EP_CTR_RX_Pos)    /*!< 0x00008000 */
#define USB_EP_CTR_RX                           USB_EP_CTR_RX_Msk              /*!< EndPoint Correct TRansfer RX */
#define USB_EP_DTOG_RX_Pos                      (14U)                          
#define USB_EP_DTOG_RX_Msk                      (0x1U << USB_EP_DTOG_RX_Pos)   /*!< 0x00004000 */
#define USB_EP_DTOG_RX                          USB_EP_DTOG_RX_Msk             /*!< EndPoint Data TOGGLE RX */
#define USB_EPRX_STAT_Pos                       (12U)                          
#define USB_EPRX_STAT_Msk                       (0x3U << USB_EPRX_STAT_Pos)    /*!< 0x00003000 */
#define USB_EPRX_STAT                           USB_EPRX_STAT_Msk              /*!< EndPoint RX STATus bit field */
#define USB_EP_SETUP_Pos                        (11U)                          
#define USB_EP_SETUP_Msk                        (0x1U << USB_EP_SETUP_Pos)     /*!< 0x00000800 */
#define USB_EP_SETUP                            USB_EP_SETUP_Msk               /*!< EndPoint SETUP */
#define USB_EP_T_FIELD_Pos                      (9U)                           
#define USB_EP_T_FIELD_Msk                      (0x3U << USB_EP_T_FIELD_Pos)   /*!< 0x00000600 */
#define USB_EP_T_FIELD                          USB_EP_T_FIELD_Msk             /*!< EndPoint TYPE */
#define USB_EP_KIND_Pos                         (8U)                           
#define USB_EP_KIND_Msk                         (0x1U << USB_EP_KIND_Pos)      /*!< 0x00000100 */
#define USB_EP_KIND                             USB_EP_KIND_Msk                /*!< EndPoint KIND */
#define USB_EP_CTR_TX_Pos                       (7U)                           
#define USB_EP_CTR_TX_Msk                       (0x1U << USB_EP_CTR_TX_Pos)    /*!< 0x00000080 */
#define USB_EP_CTR_TX                           USB_EP_CTR_TX_Msk              /*!< EndPoint Correct TRansfer TX */
#define USB_EP_DTOG_TX_Pos                      (6U)                           
#define USB_EP_DTOG_TX_Msk                      (0x1U << USB_EP_DTOG_TX_Pos)   /*!< 0x00000040 */
#define USB_EP_DTOG_TX                          USB_EP_DTOG_TX_Msk             /*!< EndPoint Data TOGGLE TX */
#define USB_EPTX_STAT_Pos                       (4U)                           
#define USB_EPTX_STAT_Msk                       (0x3U << USB_EPTX_STAT_Pos)    /*!< 0x00000030 */
#define USB_EPTX_STAT                           USB_EPTX_STAT_Msk              /*!< EndPoint TX STATus bit field */
#define USB_EPADDR_FIELD_Pos                    (0U)                           
#define USB_EPADDR_FIELD_Msk                    (0xFU << USB_EPADDR_FIELD_Pos) /*!< 0x0000000F */
#define USB_EPADDR_FIELD                        USB_EPADDR_FIELD_Msk           /*!< EndPoint ADDRess FIELD */

/* EndPoint REGister MASK (no toggle fields) */
#define  USB_EPREG_MASK                      (USB_EP_CTR_RX|USB_EP_SETUP|USB_EP_T_FIELD|USB_EP_KIND|USB_EP_CTR_TX|USB_EPADDR_FIELD)
                                                                           /*!< EP_TYPE[1:0] EndPoint TYPE */
#define USB_EP_TYPE_MASK_Pos                    (9U)                           
#define USB_EP_TYPE_MASK_Msk                    (0x3U << USB_EP_TYPE_MASK_Pos) /*!< 0x00000600 */
#define USB_EP_TYPE_MASK                        USB_EP_TYPE_MASK_Msk           /*!< EndPoint TYPE Mask */
#define USB_EP_BULK                             0x00000000U                    /*!< EndPoint BULK */
#define USB_EP_CONTROL                          0x00000200U                    /*!< EndPoint CONTROL */
#define USB_EP_ISOCHRONOUS                      0x00000400U                    /*!< EndPoint ISOCHRONOUS */
#define USB_EP_INTERRUPT                        0x00000600U                    /*!< EndPoint INTERRUPT */
#define  USB_EP_T_MASK                          (~USB_EP_T_FIELD & USB_EPREG_MASK)

#define  USB_EPKIND_MASK                        (~USB_EP_KIND & USB_EPREG_MASK)  /*!< EP_KIND EndPoint KIND */
                                                                               /*!< STAT_TX[1:0] STATus for TX transfer */
#define USB_EP_TX_DIS                           0x00000000U                    /*!< EndPoint TX DISabled */
#define USB_EP_TX_STALL                         0x00000010U                    /*!< EndPoint TX STALLed */
#define USB_EP_TX_NAK                           0x00000020U                    /*!< EndPoint TX NAKed */
#define USB_EP_TX_VALID                         0x00000030U                    /*!< EndPoint TX VALID */
#define USB_EPTX_DTOG1                          0x00000010U                    /*!< EndPoint TX Data TOGgle bit1 */
#define USB_EPTX_DTOG2                          0x00000020U                    /*!< EndPoint TX Data TOGgle bit2 */
#define  USB_EPTX_DTOGMASK  (USB_EPTX_STAT|USB_EPREG_MASK)
                                                                               /*!< STAT_RX[1:0] STATus for RX transfer */
#define USB_EP_RX_DIS                           0x00000000U                    /*!< EndPoint RX DISabled */
#define USB_EP_RX_STALL                         0x00001000U                    /*!< EndPoint RX STALLed */
#define USB_EP_RX_NAK                           0x00002000U                    /*!< EndPoint RX NAKed */
#define USB_EP_RX_VALID                         0x00003000U                    /*!< EndPoint RX VALID */
#define USB_EPRX_DTOG1                          0x00001000U                    /*!< EndPoint RX Data TOGgle bit1 */
#define USB_EPRX_DTOG2                          0x00002000U                    /*!< EndPoint RX Data TOGgle bit1 */
#define  USB_EPRX_DTOGMASK  (USB_EPRX_STAT|USB_EPREG_MASK)

/*******************  Bit definition for USB_EP0R register  *******************/
#define USB_EP0R_EA_Pos                         (0U)                           
#define USB_EP0R_EA_Msk                         (0xFU << USB_EP0R_EA_Pos)      /*!< 0x0000000F */
#define USB_EP0R_EA                             USB_EP0R_EA_Msk                /*!< Endpoint Address */

#define USB_EP0R_STAT_TX_Pos                    (4U)                           
#define USB_EP0R_STAT_TX_Msk                    (0x3U << USB_EP0R_STAT_TX_Pos) /*!< 0x00000030 */
#define USB_EP0R_STAT_TX                        USB_EP0R_STAT_TX_Msk           /*!< STAT_TX[1:0] bits (Status bits, for transmission transfers) */
#define USB_EP0R_STAT_TX_0                      (0x1U << USB_EP0R_STAT_TX_Pos) /*!< 0x00000010 */
#define USB_EP0R_STAT_TX_1                      (0x2U << USB_EP0R_STAT_TX_Pos) /*!< 0x00000020 */

#define USB_EP0R_DTOG_TX_Pos                    (6U)                           
#define USB_EP0R_DTOG_TX_Msk                    (0x1U << USB_EP0R_DTOG_TX_Pos) /*!< 0x00000040 */
#define USB_EP0R_DTOG_TX                        USB_EP0R_DTOG_TX_Msk           /*!< Data Toggle, for transmission transfers */
#define USB_EP0R_CTR_TX_Pos                     (7U)                           
#define USB_EP0R_CTR_TX_Msk                     (0x1U << USB_EP0R_CTR_TX_Pos)  /*!< 0x00000080 */
#define USB_EP0R_CTR_TX                         USB_EP0R_CTR_TX_Msk            /*!< Correct Transfer for transmission */
#define USB_EP0R_EP_KIND_Pos                    (8U)                           
#define USB_EP0R_EP_KIND_Msk                    (0x1U << USB_EP0R_EP_KIND_Pos) /*!< 0x00000100 */
#define USB_EP0R_EP_KIND                        USB_EP0R_EP_KIND_Msk           /*!< Endpoint Kind */
                                                                           
#define USB_EP0R_EP_TYPE_Pos                    (9U)                           
#define USB_EP0R_EP_TYPE_Msk                    (0x3U << USB_EP0R_EP_TYPE_Pos) /*!< 0x00000600 */
#define USB_EP0R_EP_TYPE                        USB_EP0R_EP_TYPE_Msk           /*!< EP_TYPE[1:0] bits (Endpoint type) */
#define USB_EP0R_EP_TYPE_0                      (0x1U << USB_EP0R_EP_TYPE_Pos) /*!< 0x00000200 */
#define USB_EP0R_EP_TYPE_1                      (0x2U << USB_EP0R_EP_TYPE_Pos) /*!< 0x00000400 */

#define USB_EP0R_SETUP_Pos                      (11U)                          
#define USB_EP0R_SETUP_Msk                      (0x1U << USB_EP0R_SETUP_Pos)   /*!< 0x00000800 */
#define USB_EP0R_SETUP                          USB_EP0R_SETUP_Msk             /*!< Setup transaction completed */

#define USB_EP0R_STAT_RX_Pos                    (12U)                          
#define USB_EP0R_STAT_RX_Msk                    (0x3U << USB_EP0R_STAT_RX_Pos) /*!< 0x00003000 */
#define USB_EP0R_STAT_RX                        USB_EP0R_STAT_RX_Msk           /*!< STAT_RX[1:0] bits (Status bits, for reception transfers) */
#define USB_EP0R_STAT_RX_0                      (0x1U << USB_EP0R_STAT_RX_Pos) /*!< 0x00001000 */
#define USB_EP0R_STAT_RX_1                      (0x2U << USB_EP0R_STAT_RX_Pos) /*!< 0x00002000 */

#define USB_EP0R_DTOG_RX_Pos                    (14U)                          
#define USB_EP0R_DTOG_RX_Msk                    (0x1U << USB_EP0R_DTOG_RX_Pos) /*!< 0x00004000 */
#define USB_EP0R_DTOG_RX                        USB_EP0R_DTOG_RX_Msk           /*!< Data Toggle, for reception transfers */
#define USB_EP0R_CTR_RX_Pos                     (15U)                          
#define USB_EP0R_CTR_RX_Msk                     (0x1U << USB_EP0R_CTR_RX_Pos)  /*!< 0x00008000 */
#define USB_EP0R_CTR_RX                         USB_EP0R_CTR_RX_Msk            /*!< Correct Transfer for reception */

/*******************  Bit definition for USB_EP1R register  *******************/
#define USB_EP1R_EA_Pos                         (0U)                           
#define USB_EP1R_EA_Msk                         (0xFU << USB_EP1R_EA_Pos)      /*!< 0x0000000F */
#define USB_EP1R_EA                             USB_EP1R_EA_Msk                /*!< Endpoint Address */
                                                                          
#define USB_EP1R_STAT_TX_Pos                    (4U)                           
#define USB_EP1R_STAT_TX_Msk                    (0x3U << USB_EP1R_STAT_TX_Pos) /*!< 0x00000030 */
#define USB_EP1R_STAT_TX                        USB_EP1R_STAT_TX_Msk           /*!< STAT_TX[1:0] bits (Status bits, for transmission transfers) */
#define USB_EP1R_STAT_TX_0                      (0x1U << USB_EP1R_STAT_TX_Pos) /*!< 0x00000010 */
#define USB_EP1R_STAT_TX_1                      (0x2U << USB_EP1R_STAT_TX_Pos) /*!< 0x00000020 */

#define USB_EP1R_DTOG_TX_Pos                    (6U)                           
#define USB_EP1R_DTOG_TX_Msk                    (0x1U << USB_EP1R_DTOG_TX_Pos) /*!< 0x00000040 */
#define USB_EP1R_DTOG_TX                        USB_EP1R_DTOG_TX_Msk           /*!< Data Toggle, for transmission transfers */
#define USB_EP1R_CTR_TX_Pos                     (7U)                           
#define USB_EP1R_CTR_TX_Msk                     (0x1U << USB_EP1R_CTR_TX_Pos)  /*!< 0x00000080 */
#define USB_EP1R_CTR_TX                         USB_EP1R_CTR_TX_Msk            /*!< Correct Transfer for transmission */
#define USB_EP1R_EP_KIND_Pos                    (8U)                           
#define USB_EP1R_EP_KIND_Msk                    (0x1U << USB_EP1R_EP_KIND_Pos) /*!< 0x00000100 */
#define USB_EP1R_EP_KIND                        USB_EP1R_EP_KIND_Msk           /*!< Endpoint Kind */

#define USB_EP1R_EP_TYPE_Pos                    (9U)                           
#define USB_EP1R_EP_TYPE_Msk                    (0x3U << USB_EP1R_EP_TYPE_Pos) /*!< 0x00000600 */
#define USB_EP1R_EP_TYPE                        USB_EP1R_EP_TYPE_Msk           /*!< EP_TYPE[1:0] bits (Endpoint type) */
#define USB_EP1R_EP_TYPE_0                      (0x1U << USB_EP1R_EP_TYPE_Pos) /*!< 0x00000200 */
#define USB_EP1R_EP_TYPE_1                      (0x2U << USB_EP1R_EP_TYPE_Pos) /*!< 0x00000400 */

#define USB_EP1R_SETUP_Pos                      (11U)                          
#define USB_EP1R_SETUP_Msk                      (0x1U << USB_EP1R_SETUP_Pos)   /*!< 0x00000800 */
#define USB_EP1R_SETUP                          USB_EP1R_SETUP_Msk             /*!< Setup transaction completed */
                                                                           
#define USB_EP1R_STAT_RX_Pos                    (12U)                          
#define USB_EP1R_STAT_RX_Msk                    (0x3U << USB_EP1R_STAT_RX_Pos) /*!< 0x00003000 */
#define USB_EP1R_STAT_RX                        USB_EP1R_STAT_RX_Msk           /*!< STAT_RX[1:0] bits (Status bits, for reception transfers) */
#define USB_EP1R_STAT_RX_0                      (0x1U << USB_EP1R_STAT_RX_Pos) /*!< 0x00001000 */
#define USB_EP1R_STAT_RX_1                      (0x2U << USB_EP1R_STAT_RX_Pos) /*!< 0x00002000 */

#define USB_EP1R_DTOG_RX_Pos                    (14U)                          
#define USB_EP1R_DTOG_RX_Msk                    (0x1U << USB_EP1R_DTOG_RX_Pos) /*!< 0x00004000 */
#define USB_EP1R_DTOG_RX                        USB_EP1R_DTOG_RX_Msk           /*!< Data Toggle, for reception transfers */
#define USB_EP1R_CTR_RX_Pos                     (15U)                          
#define USB_EP1R_CTR_RX_Msk                     (0x1U << USB_EP1R_CTR_RX_Pos)  /*!< 0x00008000 */
#define USB_EP1R_CTR_RX                         USB_EP1R_CTR_RX_Msk            /*!< Correct Transfer for reception */

/*******************  Bit definition for USB_EP2R register  *******************/
#define USB_EP2R_EA_Pos                         (0U)                           
#define USB_EP2R_EA_Msk                         (0xFU << USB_EP2R_EA_Pos)      /*!< 0x0000000F */
#define USB_EP2R_EA                             USB_EP2R_EA_Msk                /*!< Endpoint Address */

#define USB_EP2R_STAT_TX_Pos                    (4U)                           
#define USB_EP2R_STAT_TX_Msk                    (0x3U << USB_EP2R_STAT_TX_Pos) /*!< 0x00000030 */
#define USB_EP2R_STAT_TX                        USB_EP2R_STAT_TX_Msk           /*!< STAT_TX[1:0] bits (Status bits, for transmission transfers) */
#define USB_EP2R_STAT_TX_0                      (0x1U << USB_EP2R_STAT_TX_Pos) /*!< 0x00000010 */
#define USB_EP2R_STAT_TX_1                      (0x2U << USB_EP2R_STAT_TX_Pos) /*!< 0x00000020 */

#define USB_EP2R_DTOG_TX_Pos                    (6U)                           
#define USB_EP2R_DTOG_TX_Msk                    (0x1U << USB_EP2R_DTOG_TX_Pos) /*!< 0x00000040 */
#define USB_EP2R_DTOG_TX                        USB_EP2R_DTOG_TX_Msk           /*!< Data Toggle, for transmission transfers */
#define USB_EP2R_CTR_TX_Pos                     (7U)                           
#define USB_EP2R_CTR_TX_Msk                     (0x1U << USB_EP2R_CTR_TX_Pos)  /*!< 0x00000080 */
#define USB_EP2R_CTR_TX                         USB_EP2R_CTR_TX_Msk            /*!< Correct Transfer for transmission */
#define USB_EP2R_EP_KIND_Pos                    (8U)                           
#define USB_EP2R_EP_KIND_Msk                    (0x1U << USB_EP2R_EP_KIND_Pos) /*!< 0x00000100 */
#define USB_EP2R_EP_KIND                        USB_EP2R_EP_KIND_Msk           /*!< Endpoint Kind */

#define USB_EP2R_EP_TYPE_Pos                    (9U)                           
#define USB_EP2R_EP_TYPE_Msk                    (0x3U << USB_EP2R_EP_TYPE_Pos) /*!< 0x00000600 */
#define USB_EP2R_EP_TYPE                        USB_EP2R_EP_TYPE_Msk           /*!< EP_TYPE[1:0] bits (Endpoint type) */
#define USB_EP2R_EP_TYPE_0                      (0x1U << USB_EP2R_EP_TYPE_Pos) /*!< 0x00000200 */
#define USB_EP2R_EP_TYPE_1                      (0x2U << USB_EP2R_EP_TYPE_Pos) /*!< 0x00000400 */

#define USB_EP2R_SETUP_Pos                      (11U)                          
#define USB_EP2R_SETUP_Msk                      (0x1U << USB_EP2R_SETUP_Pos)   /*!< 0x00000800 */
#define USB_EP2R_SETUP                          USB_EP2R_SETUP_Msk             /*!< Setup transaction completed */

#define USB_EP2R_STAT_RX_Pos                    (12U)                          
#define USB_EP2R_STAT_RX_Msk                    (0x3U << USB_EP2R_STAT_RX_Pos) /*!< 0x00003000 */
#define USB_EP2R_STAT_RX                        USB_EP2R_STAT_RX_Msk           /*!< STAT_RX[1:0] bits (Status bits, for reception transfers) */
#define USB_EP2R_STAT_RX_0                      (0x1U << USB_EP2R_STAT_RX_Pos) /*!< 0x00001000 */
#define USB_EP2R_STAT_RX_1                      (0x2U << USB_EP2R_STAT_RX_Pos) /*!< 0x00002000 */

#define USB_EP2R_DTOG_RX_Pos                    (14U)                          
#define USB_EP2R_DTOG_RX_Msk                    (0x1U << USB_EP2R_DTOG_RX_Pos) /*!< 0x00004000 */
#define USB_EP2R_DTOG_RX                        USB_EP2R_DTOG_RX_Msk           /*!< Data Toggle, for reception transfers */
#define USB_EP2R_CTR_RX_Pos                     (15U)                          
#define USB_EP2R_CTR_RX_Msk                     (0x1U << USB_EP2R_CTR_RX_Pos)  /*!< 0x00008000 */
#define USB_EP2R_CTR_RX                         USB_EP2R_CTR_RX_Msk            /*!< Correct Transfer for reception */

/*******************  Bit definition for USB_EP3R register  *******************/
#define USB_EP3R_EA_Pos                         (0U)                           
#define USB_EP3R_EA_Msk                         (0xFU << USB_EP3R_EA_Pos)      /*!< 0x0000000F */
#define USB_EP3R_EA                             USB_EP3R_EA_Msk                /*!< Endpoint Address */

#define USB_EP3R_STAT_TX_Pos                    (4U)                           
#define USB_EP3R_STAT_TX_Msk                    (0x3U << USB_EP3R_STAT_TX_Pos) /*!< 0x00000030 */
#define USB_EP3R_STAT_TX                        USB_EP3R_STAT_TX_Msk           /*!< STAT_TX[1:0] bits (Status bits, for transmission transfers) */
#define USB_EP3R_STAT_TX_0                      (0x1U << USB_EP3R_STAT_TX_Pos) /*!< 0x00000010 */
#define USB_EP3R_STAT_TX_1                      (0x2U << USB_EP3R_STAT_TX_Pos) /*!< 0x00000020 */

#define USB_EP3R_DTOG_TX_Pos                    (6U)                           
#define USB_EP3R_DTOG_TX_Msk                    (0x1U << USB_EP3R_DTOG_TX_Pos) /*!< 0x00000040 */
#define USB_EP3R_DTOG_TX                        USB_EP3R_DTOG_TX_Msk           /*!< Data Toggle, for transmission transfers */
#define USB_EP3R_CTR_TX_Pos                     (7U)                           
#define USB_EP3R_CTR_TX_Msk                     (0x1U << USB_EP3R_CTR_TX_Pos)  /*!< 0x00000080 */
#define USB_EP3R_CTR_TX                         USB_EP3R_CTR_TX_Msk            /*!< Correct Transfer for transmission */
#define USB_EP3R_EP_KIND_Pos                    (8U)                           
#define USB_EP3R_EP_KIND_Msk                    (0x1U << USB_EP3R_EP_KIND_Pos) /*!< 0x00000100 */
#define USB_EP3R_EP_KIND                        USB_EP3R_EP_KIND_Msk           /*!< Endpoint Kind */

#define USB_EP3R_EP_TYPE_Pos                    (9U)                           
#define USB_EP3R_EP_TYPE_Msk                    (0x3U << USB_EP3R_EP_TYPE_Pos) /*!< 0x00000600 */
#define USB_EP3R_EP_TYPE                        USB_EP3R_EP_TYPE_Msk           /*!< EP_TYPE[1:0] bits (Endpoint type) */
#define USB_EP3R_EP_TYPE_0                      (0x1U << USB_EP3R_EP_TYPE_Pos) /*!< 0x00000200 */
#define USB_EP3R_EP_TYPE_1                      (0x2U << USB_EP3R_EP_TYPE_Pos) /*!< 0x00000400 */

#define USB_EP3R_SETUP_Pos                      (11U)                          
#define USB_EP3R_SETUP_Msk                      (0x1U << USB_EP3R_SETUP_Pos)   /*!< 0x00000800 */
#define USB_EP3R_SETUP                          USB_EP3R_SETUP_Msk             /*!< Setup transaction completed */

#define USB_EP3R_STAT_RX_Pos                    (12U)                          
#define USB_EP3R_STAT_RX_Msk                    (0x3U << USB_EP3R_STAT_RX_Pos) /*!< 0x00003000 */
#define USB_EP3R_STAT_RX                        USB_EP3R_STAT_RX_Msk           /*!< STAT_RX[1:0] bits (Status bits, for reception transfers) */
#define USB_EP3R_STAT_RX_0                      (0x1U << USB_EP3R_STAT_RX_Pos) /*!< 0x00001000 */
#define USB_EP3R_STAT_RX_1                      (0x2U << USB_EP3R_STAT_RX_Pos) /*!< 0x00002000 */

#define USB_EP3R_DTOG_RX_Pos                    (14U)                          
#define USB_EP3R_DTOG_RX_Msk                    (0x1U << USB_EP3R_DTOG_RX_Pos) /*!< 0x00004000 */
#define USB_EP3R_DTOG_RX                        USB_EP3R_DTOG_RX_Msk           /*!< Data Toggle, for reception transfers */
#define USB_EP3R_CTR_RX_Pos                     (15U)                          
#define USB_EP3R_CTR_RX_Msk                     (0x1U << USB_EP3R_CTR_RX_Pos)  /*!< 0x00008000 */
#define USB_EP3R_CTR_RX                         USB_EP3R_CTR_RX_Msk            /*!< Correct Transfer for reception */

/*******************  Bit definition for USB_EP4R register  *******************/
#define USB_EP4R_EA_Pos                         (0U)                           
#define USB_EP4R_EA_Msk                         (0xFU << USB_EP4R_EA_Pos)      /*!< 0x0000000F */
#define USB_EP4R_EA                             USB_EP4R_EA_Msk                /*!< Endpoint Address */

#define USB_EP4R_STAT_TX_Pos                    (4U)                           
#define USB_EP4R_STAT_TX_Msk                    (0x3U << USB_EP4R_STAT_TX_Pos) /*!< 0x00000030 */
#define USB_EP4R_STAT_TX                        USB_EP4R_STAT_TX_Msk           /*!< STAT_TX[1:0] bits (Status bits, for transmission transfers) */
#define USB_EP4R_STAT_TX_0                      (0x1U << USB_EP4R_STAT_TX_Pos) /*!< 0x00000010 */
#define USB_EP4R_STAT_TX_1                      (0x2U << USB_EP4R_STAT_TX_Pos) /*!< 0x00000020 */

#define USB_EP4R_DTOG_TX_Pos                    (6U)                           
#define USB_EP4R_DTOG_TX_Msk                    (0x1U << USB_EP4R_DTOG_TX_Pos) /*!< 0x00000040 */
#define USB_EP4R_DTOG_TX                        USB_EP4R_DTOG_TX_Msk           /*!< Data Toggle, for transmission transfers */
#define USB_EP4R_CTR_TX_Pos                     (7U)                           
#define USB_EP4R_CTR_TX_Msk                     (0x1U << USB_EP4R_CTR_TX_Pos)  /*!< 0x00000080 */
#define USB_EP4R_CTR_TX                         USB_EP4R_CTR_TX_Msk            /*!< Correct Transfer for transmission */
#define USB_EP4R_EP_KIND_Pos                    (8U)                           
#define USB_EP4R_EP_KIND_Msk                    (0x1U << USB_EP4R_EP_KIND_Pos) /*!< 0x00000100 */
#define USB_EP4R_EP_KIND                        USB_EP4R_EP_KIND_Msk           /*!< Endpoint Kind */

#define USB_EP4R_EP_TYPE_Pos                    (9U)                           
#define USB_EP4R_EP_TYPE_Msk                    (0x3U << USB_EP4R_EP_TYPE_Pos) /*!< 0x00000600 */
#define USB_EP4R_EP_TYPE                        USB_EP4R_EP_TYPE_Msk           /*!< EP_TYPE[1:0] bits (Endpoint type) */
#define USB_EP4R_EP_TYPE_0                      (0x1U << USB_EP4R_EP_TYPE_Pos) /*!< 0x00000200 */
#define USB_EP4R_EP_TYPE_1                      (0x2U << USB_EP4R_EP_TYPE_Pos) /*!< 0x00000400 */

#define USB_EP4R_SETUP_Pos                      (11U)                          
#define USB_EP4R_SETUP_Msk                      (0x1U << USB_EP4R_SETUP_Pos)   /*!< 0x00000800 */
#define USB_EP4R_SETUP                          USB_EP4R_SETUP_Msk             /*!< Setup transaction completed */

#define USB_EP4R_STAT_RX_Pos                    (12U)                          
#define USB_EP4R_STAT_RX_Msk                    (0x3U << USB_EP4R_STAT_RX_Pos) /*!< 0x00003000 */
#define USB_EP4R_STAT_RX                        USB_EP4R_STAT_RX_Msk           /*!< STAT_RX[1:0] bits (Status bits, for reception transfers) */
#define USB_EP4R_STAT_RX_0                      (0x1U << USB_EP4R_STAT_RX_Pos) /*!< 0x00001000 */
#define USB_EP4R_STAT_RX_1                      (0x2U << USB_EP4R_STAT_RX_Pos) /*!< 0x00002000 */

#define USB_EP4R_DTOG_RX_Pos                    (14U)                          
#define USB_EP4R_DTOG_RX_Msk                    (0x1U << USB_EP4R_DTOG_RX_Pos) /*!< 0x00004000 */
#define USB_EP4R_DTOG_RX                        USB_EP4R_DTOG_RX_Msk           /*!< Data Toggle, for reception transfers */
#define USB_EP4R_CTR_RX_Pos                     (15U)                          
#define USB_EP4R_CTR_RX_Msk                     (0x1U << USB_EP4R_CTR_RX_Pos)  /*!< 0x00008000 */
#define USB_EP4R_CTR_RX                         USB_EP4R_CTR_RX_Msk            /*!< Correct Transfer for reception */

/*******************  Bit definition for USB_EP5R register  *******************/
#define USB_EP5R_EA_Pos                         (0U)                           
#define USB_EP5R_EA_Msk                         (0xFU << USB_EP5R_EA_Pos)      /*!< 0x0000000F */
#define USB_EP5R_EA                             USB_EP5R_EA_Msk                /*!< Endpoint Address */

#define USB_EP5R_STAT_TX_Pos                    (4U)                           
#define USB_EP5R_STAT_TX_Msk                    (0x3U << USB_EP5R_STAT_TX_Pos) /*!< 0x00000030 */
#define USB_EP5R_STAT_TX                        USB_EP5R_STAT_TX_Msk           /*!< STAT_TX[1:0] bits (Status bits, for transmission transfers) */
#define USB_EP5R_STAT_TX_0                      (0x1U << USB_EP5R_STAT_TX_Pos) /*!< 0x00000010 */
#define USB_EP5R_STAT_TX_1                      (0x2U << USB_EP5R_STAT_TX_Pos) /*!< 0x00000020 */

#define USB_EP5R_DTOG_TX_Pos                    (6U)                           
#define USB_EP5R_DTOG_TX_Msk                    (0x1U << USB_EP5R_DTOG_TX_Pos) /*!< 0x00000040 */
#define USB_EP5R_DTOG_TX                        USB_EP5R_DTOG_TX_Msk           /*!< Data Toggle, for transmission transfers */
#define USB_EP5R_CTR_TX_Pos                     (7U)                           
#define USB_EP5R_CTR_TX_Msk                     (0x1U << USB_EP5R_CTR_TX_Pos)  /*!< 0x00000080 */
#define USB_EP5R_CTR_TX                         USB_EP5R_CTR_TX_Msk            /*!< Correct Transfer for transmission */
#define USB_EP5R_EP_KIND_Pos                    (8U)                           
#define USB_EP5R_EP_KIND_Msk                    (0x1U << USB_EP5R_EP_KIND_Pos) /*!< 0x00000100 */
#define USB_EP5R_EP_KIND                        USB_EP5R_EP_KIND_Msk           /*!< Endpoint Kind */

#define USB_EP5R_EP_TYPE_Pos                    (9U)                           
#define USB_EP5R_EP_TYPE_Msk                    (0x3U << USB_EP5R_EP_TYPE_Pos) /*!< 0x00000600 */
#define USB_EP5R_EP_TYPE                        USB_EP5R_EP_TYPE_Msk           /*!< EP_TYPE[1:0] bits (Endpoint type) */
#define USB_EP5R_EP_TYPE_0                      (0x1U << USB_EP5R_EP_TYPE_Pos) /*!< 0x00000200 */
#define USB_EP5R_EP_TYPE_1                      (0x2U << USB_EP5R_EP_TYPE_Pos) /*!< 0x00000400 */

#define USB_EP5R_SETUP_Pos                      (11U)                          
#define USB_EP5R_SETUP_Msk                      (0x1U << USB_EP5R_SETUP_Pos)   /*!< 0x00000800 */
#define USB_EP5R_SETUP                          USB_EP5R_SETUP_Msk             /*!< Setup transaction completed */

#define USB_EP5R_STAT_RX_Pos                    (12U)                          
#define USB_EP5R_STAT_RX_Msk                    (0x3U << USB_EP5R_STAT_RX_Pos) /*!< 0x00003000 */
#define USB_EP5R_STAT_RX                        USB_EP5R_STAT_RX_Msk           /*!< STAT_RX[1:0] bits (Status bits, for reception transfers) */
#define USB_EP5R_STAT_RX_0                      (0x1U << USB_EP5R_STAT_RX_Pos) /*!< 0x00001000 */
#define USB_EP5R_STAT_RX_1                      (0x2U << USB_EP5R_STAT_RX_Pos) /*!< 0x00002000 */

#define USB_EP5R_DTOG_RX_Pos                    (14U)                          
#define USB_EP5R_DTOG_RX_Msk                    (0x1U << USB_EP5R_DTOG_RX_Pos) /*!< 0x00004000 */
#define USB_EP5R_DTOG_RX                        USB_EP5R_DTOG_RX_Msk           /*!< Data Toggle, for reception transfers */
#define USB_EP5R_CTR_RX_Pos                     (15U)                          
#define USB_EP5R_CTR_RX_Msk                     (0x1U << USB_EP5R_CTR_RX_Pos)  /*!< 0x00008000 */
#define USB_EP5R_CTR_RX                         USB_EP5R_CTR_RX_Msk            /*!< Correct Transfer for reception */

/*******************  Bit definition for USB_EP6R register  *******************/
#define USB_EP6R_EA_Pos                         (0U)                           
#define USB_EP6R_EA_Msk                         (0xFU << USB_EP6R_EA_Pos)      /*!< 0x0000000F */
#define USB_EP6R_EA                             USB_EP6R_EA_Msk                /*!< Endpoint Address */

#define USB_EP6R_STAT_TX_Pos                    (4U)                           
#define USB_EP6R_STAT_TX_Msk                    (0x3U << USB_EP6R_STAT_TX_Pos) /*!< 0x00000030 */
#define USB_EP6R_STAT_TX                        USB_EP6R_STAT_TX_Msk           /*!< STAT_TX[1:0] bits (Status bits, for transmission transfers) */
#define USB_EP6R_STAT_TX_0                      (0x1U << USB_EP6R_STAT_TX_Pos) /*!< 0x00000010 */
#define USB_EP6R_STAT_TX_1                      (0x2U << USB_EP6R_STAT_TX_Pos) /*!< 0x00000020 */

#define USB_EP6R_DTOG_TX_Pos                    (6U)                           
#define USB_EP6R_DTOG_TX_Msk                    (0x1U << USB_EP6R_DTOG_TX_Pos) /*!< 0x00000040 */
#define USB_EP6R_DTOG_TX                        USB_EP6R_DTOG_TX_Msk           /*!< Data Toggle, for transmission transfers */
#define USB_EP6R_CTR_TX_Pos                     (7U)                           
#define USB_EP6R_CTR_TX_Msk                     (0x1U << USB_EP6R_CTR_TX_Pos)  /*!< 0x00000080 */
#define USB_EP6R_CTR_TX                         USB_EP6R_CTR_TX_Msk            /*!< Correct Transfer for transmission */
#define USB_EP6R_EP_KIND_Pos                    (8U)                           
#define USB_EP6R_EP_KIND_Msk                    (0x1U << USB_EP6R_EP_KIND_Pos) /*!< 0x00000100 */
#define USB_EP6R_EP_KIND                        USB_EP6R_EP_KIND_Msk           /*!< Endpoint Kind */

#define USB_EP6R_EP_TYPE_Pos                    (9U)                           
#define USB_EP6R_EP_TYPE_Msk                    (0x3U << USB_EP6R_EP_TYPE_Pos) /*!< 0x00000600 */
#define USB_EP6R_EP_TYPE                        USB_EP6R_EP_TYPE_Msk           /*!< EP_TYPE[1:0] bits (Endpoint type) */
#define USB_EP6R_EP_TYPE_0                      (0x1U << USB_EP6R_EP_TYPE_Pos) /*!< 0x00000200 */
#define USB_EP6R_EP_TYPE_1                      (0x2U << USB_EP6R_EP_TYPE_Pos) /*!< 0x00000400 */

#define USB_EP6R_SETUP_Pos                      (11U)                          
#define USB_EP6R_SETUP_Msk                      (0x1U << USB_EP6R_SETUP_Pos)   /*!< 0x00000800 */
#define USB_EP6R_SETUP                          USB_EP6R_SETUP_Msk             /*!< Setup transaction completed */

#define USB_EP6R_STAT_RX_Pos                    (12U)                          
#define USB_EP6R_STAT_RX_Msk                    (0x3U << USB_EP6R_STAT_RX_Pos) /*!< 0x00003000 */
#define USB_EP6R_STAT_RX                        USB_EP6R_STAT_RX_Msk           /*!< STAT_RX[1:0] bits (Status bits, for reception transfers) */
#define USB_EP6R_STAT_RX_0                      (0x1U << USB_EP6R_STAT_RX_Pos) /*!< 0x00001000 */
#define USB_EP6R_STAT_RX_1                      (0x2U << USB_EP6R_STAT_RX_Pos) /*!< 0x00002000 */

#define USB_EP6R_DTOG_RX_Pos                    (14U)                          
#define USB_EP6R_DTOG_RX_Msk                    (0x1U << USB_EP6R_DTOG_RX_Pos) /*!< 0x00004000 */
#define USB_EP6R_DTOG_RX                        USB_EP6R_DTOG_RX_Msk           /*!< Data Toggle, for reception transfers */
#define USB_EP6R_CTR_RX_Pos                     (15U)                          
#define USB_EP6R_CTR_RX_Msk                     (0x1U << USB_EP6R_CTR_RX_Pos)  /*!< 0x00008000 */
#define USB_EP6R_CTR_RX                         USB_EP6R_CTR_RX_Msk            /*!< Correct Transfer for reception */

/*******************  Bit definition for USB_EP7R register  *******************/
#define USB_EP7R_EA_Pos                         (0U)                           
#define USB_EP7R_EA_Msk                         (0xFU << USB_EP7R_EA_Pos)      /*!< 0x0000000F */
#define USB_EP7R_EA                             USB_EP7R_EA_Msk                /*!< Endpoint Address */

#define USB_EP7R_STAT_TX_Pos                    (4U)                           
#define USB_EP7R_STAT_TX_Msk                    (0x3U << USB_EP7R_STAT_TX_Pos) /*!< 0x00000030 */
#define USB_EP7R_STAT_TX                        USB_EP7R_STAT_TX_Msk           /*!< STAT_TX[1:0] bits (Status bits, for transmission transfers) */
#define USB_EP7R_STAT_TX_0                      (0x1U << USB_EP7R_STAT_TX_Pos) /*!< 0x00000010 */
#define USB_EP7R_STAT_TX_1                      (0x2U << USB_EP7R_STAT_TX_Pos) /*!< 0x00000020 */

#define USB_EP7R_DTOG_TX_Pos                    (6U)                           
#define USB_EP7R_DTOG_TX_Msk                    (0x1U << USB_EP7R_DTOG_TX_Pos) /*!< 0x00000040 */
#define USB_EP7R_DTOG_TX                        USB_EP7R_DTOG_TX_Msk           /*!< Data Toggle, for transmission transfers */
#define USB_EP7R_CTR_TX_Pos                     (7U)                           
#define USB_EP7R_CTR_TX_Msk                     (0x1U << USB_EP7R_CTR_TX_Pos)  /*!< 0x00000080 */
#define USB_EP7R_CTR_TX                         USB_EP7R_CTR_TX_Msk            /*!< Correct Transfer for transmission */
#define USB_EP7R_EP_KIND_Pos                    (8U)                           
#define USB_EP7R_EP_KIND_Msk                    (0x1U << USB_EP7R_EP_KIND_Pos) /*!< 0x00000100 */
#define USB_EP7R_EP_KIND                        USB_EP7R_EP_KIND_Msk           /*!< Endpoint Kind */

#define USB_EP7R_EP_TYPE_Pos                    (9U)                           
#define USB_EP7R_EP_TYPE_Msk                    (0x3U << USB_EP7R_EP_TYPE_Pos) /*!< 0x00000600 */
#define USB_EP7R_EP_TYPE                        USB_EP7R_EP_TYPE_Msk           /*!< EP_TYPE[1:0] bits (Endpoint type) */
#define USB_EP7R_EP_TYPE_0                      (0x1U << USB_EP7R_EP_TYPE_Pos) /*!< 0x00000200 */
#define USB_EP7R_EP_TYPE_1                      (0x2U << USB_EP7R_EP_TYPE_Pos) /*!< 0x00000400 */

#define USB_EP7R_SETUP_Pos                      (11U)                          
#define USB_EP7R_SETUP_Msk                      (0x1U << USB_EP7R_SETUP_Pos)   /*!< 0x00000800 */
#define USB_EP7R_SETUP                          USB_EP7R_SETUP_Msk             /*!< Setup transaction completed */

#define USB_EP7R_STAT_RX_Pos                    (12U)                          
#define USB_EP7R_STAT_RX_Msk                    (0x3U << USB_EP7R_STAT_RX_Pos) /*!< 0x00003000 */
#define USB_EP7R_STAT_RX                        USB_EP7R_STAT_RX_Msk           /*!< STAT_RX[1:0] bits (Status bits, for reception transfers) */
#define USB_EP7R_STAT_RX_0                      (0x1U << USB_EP7R_STAT_RX_Pos) /*!< 0x00001000 */
#define USB_EP7R_STAT_RX_1                      (0x2U << USB_EP7R_STAT_RX_Pos) /*!< 0x00002000 */

#define USB_EP7R_DTOG_RX_Pos                    (14U)                          
#define USB_EP7R_DTOG_RX_Msk                    (0x1U << USB_EP7R_DTOG_RX_Pos) /*!< 0x00004000 */
#define USB_EP7R_DTOG_RX                        USB_EP7R_DTOG_RX_Msk           /*!< Data Toggle, for reception transfers */
#define USB_EP7R_CTR_RX_Pos                     (15U)                          
#define USB_EP7R_CTR_RX_Msk                     (0x1U << USB_EP7R_CTR_RX_Pos)  /*!< 0x00008000 */
#define USB_EP7R_CTR_RX                         USB_EP7R_CTR_RX_Msk            /*!< Correct Transfer for reception */

/*!< Common registers */
/*******************  Bit definition for USB_CNTR register  *******************/
#define USB_CNTR_FRES_Pos                       (0U)                           
#define USB_CNTR_FRES_Msk                       (0x1U << USB_CNTR_FRES_Pos)    /*!< 0x00000001 */
#define USB_CNTR_FRES                           USB_CNTR_FRES_Msk              /*!< Force USB Reset */
#define USB_CNTR_PDWN_Pos                       (1U)                           
#define USB_CNTR_PDWN_Msk                       (0x1U << USB_CNTR_PDWN_Pos)    /*!< 0x00000002 */
#define USB_CNTR_PDWN                           USB_CNTR_PDWN_Msk              /*!< Power down */
#define USB_CNTR_LP_MODE_Pos                    (2U)                           
#define USB_CNTR_LP_MODE_Msk                    (0x1U << USB_CNTR_LP_MODE_Pos) /*!< 0x00000004 */
#define USB_CNTR_LP_MODE                        USB_CNTR_LP_MODE_Msk           /*!< Low-power mode */
#define USB_CNTR_FSUSP_Pos                      (3U)                           
#define USB_CNTR_FSUSP_Msk                      (0x1U << USB_CNTR_FSUSP_Pos)   /*!< 0x00000008 */
#define USB_CNTR_FSUSP                          USB_CNTR_FSUSP_Msk             /*!< Force suspend */
#define USB_CNTR_RESUME_Pos                     (4U)                           
#define USB_CNTR_RESUME_Msk                     (0x1U << USB_CNTR_RESUME_Pos)  /*!< 0x00000010 */
#define USB_CNTR_RESUME                         USB_CNTR_RESUME_Msk            /*!< Resume request */
#define USB_CNTR_ESOFM_Pos                      (8U)                           
#define USB_CNTR_ESOFM_Msk                      (0x1U << USB_CNTR_ESOFM_Pos)   /*!< 0x00000100 */
#define USB_CNTR_ESOFM                          USB_CNTR_ESOFM_Msk             /*!< Expected Start Of Frame Interrupt Mask */
#define USB_CNTR_SOFM_Pos                       (9U)                           
#define USB_CNTR_SOFM_Msk                       (0x1U << USB_CNTR_SOFM_Pos)    /*!< 0x00000200 */
#define USB_CNTR_SOFM                           USB_CNTR_SOFM_Msk              /*!< Start Of Frame Interrupt Mask */
#define USB_CNTR_RESETM_Pos                     (10U)                          
#define USB_CNTR_RESETM_Msk                     (0x1U << USB_CNTR_RESETM_Pos)  /*!< 0x00000400 */
#define USB_CNTR_RESETM                         USB_CNTR_RESETM_Msk            /*!< RESET Interrupt Mask */
#define USB_CNTR_SUSPM_Pos                      (11U)                          
#define USB_CNTR_SUSPM_Msk                      (0x1U << USB_CNTR_SUSPM_Pos)   /*!< 0x00000800 */
#define USB_CNTR_SUSPM                          USB_CNTR_SUSPM_Msk             /*!< Suspend mode Interrupt Mask */
#define USB_CNTR_WKUPM_Pos                      (12U)                          
#define USB_CNTR_WKUPM_Msk                      (0x1U << USB_CNTR_WKUPM_Pos)   /*!< 0x00001000 */
#define USB_CNTR_WKUPM                          USB_CNTR_WKUPM_Msk             /*!< Wakeup Interrupt Mask */
#define USB_CNTR_ERRM_Pos                       (13U)                          
#define USB_CNTR_ERRM_Msk                       (0x1U << USB_CNTR_ERRM_Pos)    /*!< 0x00002000 */
#define USB_CNTR_ERRM                           USB_CNTR_ERRM_Msk              /*!< Error Interrupt Mask */
#define USB_CNTR_PMAOVRM_Pos                    (14U)                          
#define USB_CNTR_PMAOVRM_Msk                    (0x1U << USB_CNTR_PMAOVRM_Pos) /*!< 0x00004000 */
#define USB_CNTR_PMAOVRM                        USB_CNTR_PMAOVRM_Msk           /*!< Packet Memory Area Over / Underrun Interrupt Mask */
#define USB_CNTR_CTRM_Pos                       (15U)                          
#define USB_CNTR_CTRM_Msk                       (0x1U << USB_CNTR_CTRM_Pos)    /*!< 0x00008000 */
#define USB_CNTR_CTRM                           USB_CNTR_CTRM_Msk              /*!< Correct Transfer Interrupt Mask */

/*******************  Bit definition for USB_ISTR register  *******************/
#define USB_ISTR_EP_ID_Pos                      (0U)                           
#define USB_ISTR_EP_ID_Msk                      (0xFU << USB_ISTR_EP_ID_Pos)   /*!< 0x0000000F */
#define USB_ISTR_EP_ID                          USB_ISTR_EP_ID_Msk             /*!< Endpoint Identifier */
#define USB_ISTR_DIR_Pos                        (4U)                           
#define USB_ISTR_DIR_Msk                        (0x1U << USB_ISTR_DIR_Pos)     /*!< 0x00000010 */
#define USB_ISTR_DIR                            USB_ISTR_DIR_Msk               /*!< Direction of transaction */
#define USB_ISTR_ESOF_Pos                       (8U)                           
#define USB_ISTR_ESOF_Msk                       (0x1U << USB_ISTR_ESOF_Pos)    /*!< 0x00000100 */
#define USB_ISTR_ESOF                           USB_ISTR_ESOF_Msk              /*!< Expected Start Of Frame */
#define USB_ISTR_SOF_Pos                        (9U)                           
#define USB_ISTR_SOF_Msk                        (0x1U << USB_ISTR_SOF_Pos)     /*!< 0x00000200 */
#define USB_ISTR_SOF                            USB_ISTR_SOF_Msk               /*!< Start Of Frame */
#define USB_ISTR_RESET_Pos                      (10U)                          
#define USB_ISTR_RESET_Msk                      (0x1U << USB_ISTR_RESET_Pos)   /*!< 0x00000400 */
#define USB_ISTR_RESET                          USB_ISTR_RESET_Msk             /*!< USB RESET request */
#define USB_ISTR_SUSP_Pos                       (11U)                          
#define USB_ISTR_SUSP_Msk                       (0x1U << USB_ISTR_SUSP_Pos)    /*!< 0x00000800 */
#define USB_ISTR_SUSP                           USB_ISTR_SUSP_Msk              /*!< Suspend mode request */
#define USB_ISTR_WKUP_Pos                       (12U)                          
#define USB_ISTR_WKUP_Msk                       (0x1U << USB_ISTR_WKUP_Pos)    /*!< 0x00001000 */
#define USB_ISTR_WKUP                           USB_ISTR_WKUP_Msk              /*!< Wake up */
#define USB_ISTR_ERR_Pos                        (13U)                          
#define USB_ISTR_ERR_Msk                        (0x1U << USB_ISTR_ERR_Pos)     /*!< 0x00002000 */
#define USB_ISTR_ERR                            USB_ISTR_ERR_Msk               /*!< Error */
#define USB_ISTR_PMAOVR_Pos                     (14U)                          
#define USB_ISTR_PMAOVR_Msk                     (0x1U << USB_ISTR_PMAOVR_Pos)  /*!< 0x00004000 */
#define USB_ISTR_PMAOVR                         USB_ISTR_PMAOVR_Msk            /*!< Packet Memory Area Over / Underrun */
#define USB_ISTR_CTR_Pos                        (15U)                          
#define USB_ISTR_CTR_Msk                        (0x1U << USB_ISTR_CTR_Pos)     /*!< 0x00008000 */
#define USB_ISTR_CTR                            USB_ISTR_CTR_Msk               /*!< Correct Transfer */

/*******************  Bit definition for USB_FNR register  ********************/
#define USB_FNR_FN_Pos                          (0U)                           
#define USB_FNR_FN_Msk                          (0x7FFU << USB_FNR_FN_Pos)     /*!< 0x000007FF */
#define USB_FNR_FN                              USB_FNR_FN_Msk                 /*!< Frame Number */
#define USB_FNR_LSOF_Pos                        (11U)                          
#define USB_FNR_LSOF_Msk                        (0x3U << USB_FNR_LSOF_Pos)     /*!< 0x00001800 */
#define USB_FNR_LSOF                            USB_FNR_LSOF_Msk               /*!< Lost SOF */
#define USB_FNR_LCK_Pos                         (13U)                          
#define USB_FNR_LCK_Msk                         (0x1U << USB_FNR_LCK_Pos)      /*!< 0x00002000 */
#define USB_FNR_LCK                             USB_FNR_LCK_Msk                /*!< Locked */
#define USB_FNR_RXDM_Pos                        (14U)                          
#define USB_FNR_RXDM_Msk                        (0x1U << USB_FNR_RXDM_Pos)     /*!< 0x00004000 */
#define USB_FNR_RXDM                            USB_FNR_RXDM_Msk               /*!< Receive Data - Line Status */
#define USB_FNR_RXDP_Pos                        (15U)                          
#define USB_FNR_RXDP_Msk                        (0x1U << USB_FNR_RXDP_Pos)     /*!< 0x00008000 */
#define USB_FNR_RXDP                            USB_FNR_RXDP_Msk               /*!< Receive Data + Line Status */

/******************  Bit definition for USB_DADDR register  *******************/
#define USB_DADDR_ADD_Pos                       (0U)                           
#define USB_DADDR_ADD_Msk                       (0x7FU << USB_DADDR_ADD_Pos)   /*!< 0x0000007F */
#define USB_DADDR_ADD                           USB_DADDR_ADD_Msk              /*!< ADD[6:0] bits (Device Address) */
#define USB_DADDR_ADD0_Pos                      (0U)                           
#define USB_DADDR_ADD0_Msk                      (0x1U << USB_DADDR_ADD0_Pos)   /*!< 0x00000001 */
#define USB_DADDR_ADD0                          USB_DADDR_ADD0_Msk             /*!< Bit 0 */
#define USB_DADDR_ADD1_Pos                      (1U)                           
#define USB_DADDR_ADD1_Msk                      (0x1U << USB_DADDR_ADD1_Pos)   /*!< 0x00000002 */
#define USB_DADDR_ADD1                          USB_DADDR_ADD1_Msk             /*!< Bit 1 */
#define USB_DADDR_ADD2_Pos                      (2U)                           
#define USB_DADDR_ADD2_Msk                      (0x1U << USB_DADDR_ADD2_Pos)   /*!< 0x00000004 */
#define USB_DADDR_ADD2                          USB_DADDR_ADD2_Msk             /*!< Bit 2 */
#define USB_DADDR_ADD3_Pos                      (3U)                           
#define USB_DADDR_ADD3_Msk                      (0x1U << USB_DADDR_ADD3_Pos)   /*!< 0x00000008 */
#define USB_DADDR_ADD3                          USB_DADDR_ADD3_Msk             /*!< Bit 3 */
#define USB_DADDR_ADD4_Pos                      (4U)                           
#define USB_DADDR_ADD4_Msk                      (0x1U << USB_DADDR_ADD4_Pos)   /*!< 0x00000010 */
#define USB_DADDR_ADD4                          USB_DADDR_ADD4_Msk             /*!< Bit 4 */
#define USB_DADDR_ADD5_Pos                      (5U)                           
#define USB_DADDR_ADD5_Msk                      (0x1U << USB_DADDR_ADD5_Pos)   /*!< 0x00000020 */
#define USB_DADDR_ADD5                          USB_DADDR_ADD5_Msk             /*!< Bit 5 */
#define USB_DADDR_ADD6_Pos                      (6U)                           
#define USB_DADDR_ADD6_Msk                      (0x1U << USB_DADDR_ADD6_Pos)   /*!< 0x00000040 */
#define USB_DADDR_ADD6                          USB_DADDR_ADD6_Msk             /*!< Bit 6 */

#define USB_DADDR_EF_Pos                        (7U)                           
#define USB_DADDR_EF_Msk                        (0x1U << USB_DADDR_EF_Pos)     /*!< 0x00000080 */
#define USB_DADDR_EF                            USB_DADDR_EF_Msk               /*!< Enable Function */



	
	
	

#ifdef __cplusplus
}
#endif

#endif /* __STM32F10x_H */

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
