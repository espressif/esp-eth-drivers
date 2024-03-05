/*
 * SPDX-FileCopyrightText: 2024 Sergey Kharenko
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2024 Sergey Kharenko
 * SPDX-FileContributor: 2024 Espressif Systems (Shanghai) CO LTD
 */
#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"

void basic_init(esp_eth_handle_t *handle);
