#include "esp32s3-csi-har/EspNowTransmitter.hpp"
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <string.h>

extern uint8_t s_rx_mac[6];
extern int s_channel;

EspNowTransmitter *EspNowTransmitter::s_instance = nullptr;

void EspNowTransmitter::s_send_cb(const wifi_tx_info_t *info, esp_now_send_status_t status) {
    if (!s_instance) return;
    if (status == ESP_NOW_SEND_SUCCESS) {
        s_instance->send_count_++;
    }
}

void EspNowTransmitter::s_send_task(void *arg) {
    auto *self = static_cast<EspNowTransmitter *>(arg);
    const TickType_t interval = pdMS_TO_TICKS(50);
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        self->tx_data_.packet_seq++;
        self->tx_data_.timestamp_ms = esp_timer_get_time() / 1000;
        esp_now_send(s_rx_mac, (uint8_t*)&self->tx_data_, sizeof(esp_now_payload_t));
        vTaskDelayUntil(&last_wake, interval);
    }
}

void EspNowTransmitter::init_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void EspNowTransmitter::init_wifi() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(20));

    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));
}

void EspNowTransmitter::init_espnow() {
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(s_send_cb));

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, s_rx_mac, 6);
    peer.channel = s_channel;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    esp_now_rate_config_t rate_cfg = {
        .phymode = WIFI_PHY_MODE_HT20,
        .rate = WIFI_PHY_RATE_MCS0_LGI,
        .ersu = false,
        .dcm = false
    };
    esp_now_set_peer_rate_config(s_rx_mac, &rate_cfg);
}

void EspNowTransmitter::begin() {
    s_instance = this;
    send_count_ = 0;

    init_nvs();
    init_wifi();
    init_espnow();

    xTaskCreatePinnedToCore(s_send_task, "tx_send", 3072, this, 5, nullptr, 1);
    ESP_LOGI("TX", "Transmitter Started - HT20 MCS0, Channel %d", s_channel);
}
