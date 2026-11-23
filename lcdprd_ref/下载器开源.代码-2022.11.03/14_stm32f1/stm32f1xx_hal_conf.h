/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32F1xx_HAL_CONF_H
#define __STM32F1xx_HAL_CONF_H

#ifdef __cplusplus
 extern "C" {
#endif

#define HAL_PCD_MODULE_ENABLED

#ifdef HAL_PCD_MODULE_ENABLED
 #include "stm32f1xx_hal_pcd.h"
#endif /* HAL_PCD_MODULE_ENABLED */
  
#ifdef __cplusplus
}
#endif

#endif /* __STM32F1xx_HAL_CONF_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
