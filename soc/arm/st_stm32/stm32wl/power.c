/*
 * Copyright (c) 2021 Fabio Baltieri
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include <pm/pm.h>
#include <soc.h>
#include <init.h>

#include <stm32wlxx_ll_utils.h>
#include <stm32wlxx_ll_bus.h>
#include <stm32wlxx_ll_cortex.h>
#include <stm32wlxx_ll_pwr.h>
#include <stm32wlxx_ll_rcc.h>
#include <stm32wlxx_ll_system.h>
#include <clock_control/clock_stm32_ll_common.h>
#include <drivers/clock_control/stm32_clock_control.h>

#include <logging/log.h>
LOG_MODULE_DECLARE(soc, CONFIG_SOC_LOG_LEVEL);

/* select MSI as wake-up system clock if configured, HSI otherwise */
#if STM32_SYSCLK_SRC_MSI
#define RCC_STOP_WAKEUPCLOCK_SELECTED LL_RCC_STOP_WAKEUPCLOCK_MSI
#else
#define RCC_STOP_WAKEUPCLOCK_SELECTED LL_RCC_STOP_WAKEUPCLOCK_HSI
#endif

/* Invoke Low Power/System Off specific Tasks */
__weak void pm_power_state_set(struct pm_state_info info)
{
	switch (info.state) {
	case PM_STATE_SUSPEND_TO_IDLE:
		LL_RCC_SetClkAfterWakeFromStop(RCC_STOP_WAKEUPCLOCK_SELECTED);
		LL_PWR_ClearFlag_WU();
		switch (info.substate_id) {
		case 1:
			LL_PWR_SetPowerMode(LL_PWR_MODE_STOP0);
			break;
		case 2:
			LL_PWR_SetPowerMode(LL_PWR_MODE_STOP1);
			break;
		case 3:
			LL_PWR_SetPowerMode(LL_PWR_MODE_STOP2);
			break;
		default:
			LOG_DBG("Unsupported power substate-id %u", info.substate_id);
			return;
		}
		LL_LPM_EnableDeepSleep();
		k_cpu_idle();
		break;
	case PM_STATE_SOFT_OFF:
		LL_PWR_ClearFlag_WU();
		switch (info.substate_id) {
		case 0:
			LL_PWR_SetPowerMode(LL_PWR_MODE_STANDBY);
			break;
		case 1:
			LL_PWR_SetPowerMode(LL_PWR_MODE_SHUTDOWN);
			break;
		default:
			LOG_DBG("Unsupported power substate-id %u", info.substate_id);
			return;
		}
		LL_LPM_EnableDeepSleep();
		k_cpu_idle();
		break;
	default:
		LOG_DBG("Unsupported power state %u", info.state);
		break;
	}
}

/* Handle SOC specific activity after Low Power Mode Exit */
__weak void pm_power_state_exit_post_ops(struct pm_state_info info)
{
	switch (info.state) {
	case PM_STATE_SUSPEND_TO_IDLE:
		LL_LPM_DisableSleepOnExit();
		LL_LPM_EnableSleep();
		/* need to restore the clock */
		stm32_clock_control_init(NULL);
		break;
	case PM_STATE_SOFT_OFF:
		/* Nothing to do. */
		break;
	default:
		LOG_DBG("Unsupported power substate-id %u", info.state);
		break;
	}

	/*
	 * System is now in active mode.
	 * Reenable interrupts which were disabled
	 * when OS started idling code.
	 */
	irq_unlock(0);
}

/* Initialize STM32 Power */
static int stm32_power_init(const struct device *dev)
{
	ARG_UNUSED(dev);

#ifdef CONFIG_DEBUG
	/* Enable the Debug Module during STOP mode */
	LL_DBGMCU_EnableDBGStopMode();
#endif /* CONFIG_DEBUG */

	return 0;
}

SYS_INIT(stm32_power_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
