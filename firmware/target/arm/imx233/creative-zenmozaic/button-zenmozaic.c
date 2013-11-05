/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2013 by Amaury Pouly
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include "button-target.h"
#include "system.h"
#include "system-target.h"
#include "pinctrl-imx233.h"
#include "power-imx233.h"
#include "button-lradc-imx233.h"

struct imx233_button_lradc_mapping_t imx233_button_lradc_mapping[] =
{
    {0, IMX233_BUTTON_LRADC_HOLD},
    {200, BUTTON_MENU},
    {445, BUTTON_SHORTCUT},
    {645, BUTTON_UP},
    {860, BUTTON_LEFT},
    {1060, BUTTON_RIGHT},
    {1260, BUTTON_DOWN},
    {1480, BUTTON_SELECT},
    {2700, BUTTON_BACK},
    {2945, BUTTON_PLAYPAUSE},
    {3400, 0},
    {0, IMX233_BUTTON_LRADC_END},
};

void button_init_device(void)
{
    imx233_button_lradc_init();

    imx233_pinctrl_acquire(2, 8, "jack_detect");
    imx233_pinctrl_set_function(2, 8, PINCTRL_FUNCTION_GPIO);
    imx233_pinctrl_enable_gpio(2, 8, false);
}

bool headphones_inserted(void)
{
    return imx233_pinctrl_get_gpio(2, 8);
}

bool button_hold(void)
{
    return imx233_button_lradc_hold();
}

int button_read_device(void)
{
    int btn = 0;
    if(BF_RD(POWER_STS, PSWITCH) == 1)
        btn |= BUTTON_POWER;
    return imx233_button_lradc_read(btn);
}
