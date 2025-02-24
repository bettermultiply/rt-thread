/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Email: opensource_embedded@phytium.com.cn
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-10-18     zhangyan     first version
 *
 */
#include "rtconfig.h"
#ifdef BSP_USING_PWM
#define LOG_TAG      "pwm_drv"
#include "drv_log.h"
#include "drv_pwm.h"
#include "fio_mux.h"
#include "fpwm.h"
#ifdef RT_USING_SMART
    #include <ioremap.h>
#endif
struct phytium_pwm
{
    const char *name;
    FPwmCtrl pwm_handle;
    struct rt_device_pwm device; /* inherit from can device */
};

static struct phytium_pwm pwm_dev[FPWM_NUM] =
{
#if defined(TARGET_E2000)
    {
        .name = "PWM0",
        .pwm_handle.config.instance_id = 0,
    },
    {
        .name = "PWM1",
        .pwm_handle.config.instance_id = 1,
    },
    {
        .name = "PWM2",
        .pwm_handle.config.instance_id = 2,
    },
    {
        .name = "PWM3",
        .pwm_handle.config.instance_id = 3,
    },
    {
        .name = "PWM4",
        .pwm_handle.config.instance_id = 4,
    },
    {
        .name = "PWM5",
        .pwm_handle.config.instance_id = 5,
    },
    {
        .name = "PWM6",
        .pwm_handle.config.instance_id = 6,
    },
    {
        .name = "PWM7",
        .pwm_handle.config.instance_id = 7,
    },
#elif defined(TARGET_PHYTIUMPI)
    {
        .name = "PWM2",
        .pwm_handle.config.instance_id = 2,
    },
#endif
};

static rt_err_t drv_pwm_config(struct phytium_pwm *pwm_dev)
{
    RT_ASSERT(pwm_dev);
    u32 ret;
    FPwmConfig *config = NULL;
    FPwmCtrl *pwm_handle = &pwm_dev->pwm_handle;
    FIOPadSetPwmMux(pwm_handle->config.instance_id, 0);
    FIOPadSetPwmMux(pwm_handle->config.instance_id, 1);
    config = FPwmLookupConfig(pwm_handle->config.instance_id);
#ifdef RT_USING_SMART
    config->pwm_base_addr = (uintptr)rt_ioremap((void *)config->pwm_base_addr, 0x1000);
    config->db_base_addr = (uintptr)rt_ioremap((void *)config->db_base_addr, 0x100);
#endif
    ret = FPwmCfgInitialize(pwm_handle, config);
    if (ret != FPWM_SUCCESS)
    {
        LOG_E("Pwm config init failed.\n");

        return RT_ERROR;
    }

    return RT_EOK;
}

static rt_err_t drv_pwm_enable(struct phytium_pwm *pwm_dev, struct rt_pwm_configuration *configuration, boolean enable_pwm)
{
    RT_ASSERT(pwm_dev);
    RT_ASSERT(configuration);
    u32 channel = configuration->channel;

    if (enable_pwm == RT_TRUE)
    {
        FPwmEnable(&pwm_dev->pwm_handle, channel);
    }
    else
    {
        FPwmDisable(&pwm_dev->pwm_handle, channel);
    }

    return RT_EOK;
}

static rt_err_t drv_pwm_set(struct phytium_pwm *pwm_dev, int cmd, struct rt_pwm_configuration *configuration)
{
    RT_ASSERT(pwm_dev);
    RT_ASSERT(configuration);
    u32 ret;
    FPwmVariableConfig pwm_cfg;
    u32 channel = configuration->channel;

    memset(&pwm_cfg, 0, sizeof(pwm_cfg));
    pwm_cfg.tim_ctrl_mode = FPWM_MODULO;
    pwm_cfg.tim_ctrl_div = 50 - 1;
    /* Precision set to microseconds */
    switch (cmd)
    {
        case PWM_CMD_SET:
            pwm_cfg.pwm_period = configuration->period / 1000;
            pwm_cfg.pwm_pulse = configuration->pulse / 1000;
            break;
        case PWM_CMD_SET_PERIOD:
            pwm_cfg.pwm_period = configuration->period / 1000;
            break;
        case PWM_CMD_SET_PULSE:
            pwm_cfg.pwm_pulse = configuration->pulse / 1000;
            break;
    }
    /* Can be modified according to function */
    pwm_cfg.pwm_mode = FPWM_OUTPUT_COMPARE;
    pwm_cfg.pwm_polarity = FPWM_POLARITY_NORMAL;
    pwm_cfg.pwm_duty_source_mode = FPWM_DUTY_CCR;

    FPwmDisable(&pwm_dev->pwm_handle, channel);

    ret = FPwmVariableSet(&pwm_dev->pwm_handle, channel, &pwm_cfg);
    if (ret != FPWM_SUCCESS)
    {
        LOG_E("Pwm variable set failed.\n");

        return RT_ERROR;
    }

    FPwmEnable(&pwm_dev->pwm_handle, channel);

    return RT_EOK;
}

static rt_err_t drv_pwm_get(struct phytium_pwm *pwm_dev, struct rt_pwm_configuration *configuration)
{
    RT_ASSERT(pwm_dev);
    RT_ASSERT(configuration);
    u32 ret;
    FPwmVariableConfig pwm_cfg;
    u32 channel = configuration->channel;

    memset(&pwm_cfg, 0, sizeof(pwm_cfg));
    ret = FPwmVariableGet(&pwm_dev->pwm_handle, channel, &pwm_cfg);
    if (ret != FPWM_SUCCESS)
    {
        LOG_E("Pwm variable get failed.\n");

        return RT_ERROR;
    }

    configuration->period = pwm_cfg.pwm_period * 1000;
    configuration->pulse = pwm_cfg.pwm_pulse * 1000;

    LOG_D("period = %d\n, pulse = %d\n", configuration->period, configuration->pulse);

    return RT_EOK;
}

static rt_err_t drv_pwm_set_dead_time(struct phytium_pwm *pwm_dev, struct rt_pwm_configuration *configuration)
{
    RT_ASSERT(pwm_dev);
    RT_ASSERT(configuration);
    u32 ret;
    FPwmDbVariableConfig db_cfg;
    u32 channel = configuration->channel;

    memset(&db_cfg, 0, sizeof(db_cfg));
    db_cfg.db_rise_cycle = configuration->dead_time / 1000;
    db_cfg.db_fall_cycle = configuration->dead_time / 1000;
    db_cfg.db_polarity_sel = FPWM_DB_AH;
    db_cfg.db_in_mode = FPWM_DB_IN_MODE_PWM0;
    db_cfg.db_out_mode = FPWM_DB_OUT_MODE_ENABLE_RISE_FALL;

    FPwmDisable(&pwm_dev->pwm_handle, channel);
    ret = FPwmDbVariableSet(&pwm_dev->pwm_handle, &db_cfg);
    if (ret != FPWM_SUCCESS)
    {
        LOG_E("FPwmDbVariableSet failed.");

        return RT_ERROR;
    }
    FPwmEnable(&pwm_dev->pwm_handle, channel);

    return RT_EOK;
}

static rt_err_t _pwm_control(struct rt_device_pwm *device, int cmd, void *arg)
{
    struct rt_pwm_configuration *configuration = (struct rt_pwm_configuration *)arg;
    struct phytium_pwm *pwm_dev;
    pwm_dev = (struct phytium_pwm *)(device->parent.user_data);

    switch (cmd)
    {
        case PWM_CMD_ENABLE:
            return drv_pwm_enable(pwm_dev, configuration, RT_TRUE);
        case PWM_CMD_DISABLE:
            return drv_pwm_enable(pwm_dev, configuration, RT_FALSE);
        case PWM_CMD_SET:
            return drv_pwm_set(pwm_dev, PWM_CMD_SET, configuration);
        case PWM_CMD_GET:
            return drv_pwm_get(pwm_dev, configuration);
        case PWM_CMD_SET_DEAD_TIME:
            return drv_pwm_set_dead_time(pwm_dev, configuration);
        case PWM_CMD_SET_PERIOD:
            return drv_pwm_set(pwm_dev, PWM_CMD_SET_PERIOD, configuration);
        case PWM_CMD_SET_PULSE:
            return drv_pwm_set(pwm_dev, PWM_CMD_SET_PULSE, configuration);
        default:
            return -RT_EINVAL;
    }
}

static const struct rt_pwm_ops _pwm_ops =
{
    _pwm_control,
};

static rt_err_t pwm_controller_init(u32 pwm_id)
{
    u32 ret = RT_EOK;
    ret = drv_pwm_config(&pwm_dev[pwm_id]);
    if (ret != FPWM_SUCCESS)
    {
        LOG_E("Pwm config failed.\n");

        return RT_ERROR;
    }
    ret = rt_device_pwm_register(&pwm_dev[pwm_id].device,
                                 pwm_dev[pwm_id].name,
                                 &_pwm_ops,
                                 &pwm_dev[pwm_id]);
    RT_ASSERT(ret == RT_EOK);

    return ret;
}

int rt_hw_pwm_init(void)
{
#if defined(TARGET_E2000)

#if defined(RT_USING_PWM0)
    pwm_controller_init(FPWM0_ID);
#endif
#if defined(RT_USING_PWM1)
    pwm_controller_init(FPWM1_ID);
#endif
#if defined(RT_USING_PWM2)
    pwm_controller_init(FPWM2_ID);
#endif
#if defined(RT_USING_PWM3)
    pwm_controller_init(FPWM3_ID);
#endif
#if defined(RT_USING_PWM4)
    pwm_controller_init(FPWM4_ID);
#endif
#if defined(RT_USING_PWM5)
    pwm_controller_init(FPWM5_ID);
#endif
#if defined(RT_USING_PWM6)
    pwm_controller_init(FPWM6_ID);
#endif
#if defined(RT_USING_PWM7)
    pwm_controller_init(FPWM7_ID);
#endif

#elif defined(TARGET_PHYTIUMPI)

#if defined(RT_USING_PWM2)
    pwm_controller_init(FPWM2_ID);
#endif

#endif
    return 0;
}
INIT_DEVICE_EXPORT(rt_hw_pwm_init);
#endif