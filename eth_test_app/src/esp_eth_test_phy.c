/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "sdkconfig.h"
#include "esp_eth_test_utils.h"
#include "esp_eth_phy_802_3.h"

#if CONFIG_ETH_TEST_PHY_DEFAULTS
TEST_CASE("ethernet PHY reset timing defaults", "[ethernet],[skip_setup_teardown]")
{
    TEST_ASSERT_MESSAGE(CONFIG_ETH_TEST_PHY_RESET_ASSERTION_US != 0 || CONFIG_ETH_TEST_PHY_POST_RESET_DELAY_MS != 0,
                        "Configure at least one expected reset timing");
    eth_phy_config_t config = ETH_PHY_DEFAULT_CONFIG();
    esp_eth_phy_t *phy = esp_eth_test_phy_new(&config);
    TEST_ASSERT_NOT_NULL(phy);
    const phy_802_3_t *phy_802_3 = esp_eth_phy_into_phy_802_3(phy);
    const int32_t assertion_us = phy_802_3->hw_reset_assert_time_us;
    const int32_t delay_ms = phy_802_3->post_hw_reset_delay_ms;
    TEST_ASSERT_EQUAL(ESP_OK, phy->del(phy));

    if (CONFIG_ETH_TEST_PHY_RESET_ASSERTION_US != 0) {
        TEST_ASSERT_EQUAL_INT32(CONFIG_ETH_TEST_PHY_RESET_ASSERTION_US, assertion_us);
    }
    if (CONFIG_ETH_TEST_PHY_POST_RESET_DELAY_MS != 0) {
        TEST_ASSERT_EQUAL_INT32(CONFIG_ETH_TEST_PHY_POST_RESET_DELAY_MS, delay_ms);
    }
}

TEST_CASE("ethernet PHY preserves reset timing overrides", "[ethernet],[skip_setup_teardown]")
{
    const int32_t assertion_us = 250;
    const int32_t delays_ms[] = {50, -1};

    for (size_t i = 0; i < sizeof(delays_ms) / sizeof(delays_ms[0]); i++) {
        eth_phy_config_t config = ETH_PHY_DEFAULT_CONFIG();
        config.hw_reset_assert_time_us = assertion_us;
        config.post_hw_reset_delay_ms = delays_ms[i];
        eth_phy_config_t original;
        memcpy(&original, &config, sizeof(config));

        esp_eth_phy_t *phy = esp_eth_test_phy_new(&config);
        TEST_ASSERT_NOT_NULL(phy);
        const phy_802_3_t *phy_802_3 = esp_eth_phy_into_phy_802_3(phy);
        TEST_ASSERT_EQUAL_INT32(assertion_us, phy_802_3->hw_reset_assert_time_us);
        TEST_ASSERT_EQUAL_INT32(delays_ms[i], phy_802_3->post_hw_reset_delay_ms);
        TEST_ASSERT_EQUAL_MEMORY(&original, &config, sizeof(config));
        TEST_ASSERT_EQUAL(ESP_OK, phy->del(phy));
    }
}
#endif
