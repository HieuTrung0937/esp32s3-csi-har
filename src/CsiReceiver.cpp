#include "esp32s3-csi-har/CsiReceiver.hpp"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <nvs_flash.h>

extern int s_channel;
CsiReceiver *CsiReceiver::s_instance = nullptr;

static bool s_tx_mac_auto_detected = false;

typedef struct {
    uint32_t seq;
    int8_t   rssi;
    uint8_t  subcarrier_count;
    float    amplitudes[64];
} csi_clean_pkt_t;

void IRAM_ATTR CsiReceiver::s_csi_cb(void *ctx, wifi_csi_info_t *info) {
    if (!s_instance || !info || !info->buf) return;

    extern uint8_t s_tx_mac[6];

    bool has_custom_mac = false;
    for (int i = 0; i < 6; i++) {
        if (s_tx_mac[i] != 0x00 && s_tx_mac[i] != 0xFF) {
            has_custom_mac = true;
            break;
        }
    }

    if (!has_custom_mac && !s_tx_mac_auto_detected) {
        memcpy(s_tx_mac, info->mac, 6);
        s_tx_mac_auto_detected = true;
        ESP_DRAM_LOGI("CSI_RX", "Tu do tim thong tin TX MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                      s_tx_mac[0], s_tx_mac[1], s_tx_mac[2],
                      s_tx_mac[3], s_tx_mac[4], s_tx_mac[5]);
    }

    if (memcmp(info->mac, s_tx_mac, 6) != 0) return;

    csi_clean_pkt_t pkt;
    pkt.seq = s_instance->pkt_count_++;
    pkt.rssi = info->rx_ctrl.rssi;
    
    int8_t *raw_iq = (int8_t *)info->buf;
    int total_bytes = info->len;
    int num_subcarriers = total_bytes / 2;
    if (num_subcarriers > 64) num_subcarriers = 64;

    pkt.subcarrier_count = num_subcarriers;

    for (int i = 0; i < num_subcarriers; i++) {
        float i_val = (float)raw_iq[i * 2];
        float q_val = (float)raw_iq[i * 2 + 1];
        pkt.amplitudes[i] = sqrtf(i_val * i_val + q_val * q_val);
    }

    BaseType_t higher_priority_task_woken = pdFALSE;
    xQueueSendFromISR(s_instance->event_queue_, &pkt, &higher_priority_task_woken);
    if (higher_priority_task_woken) portYIELD_FROM_ISR();
}

void CsiReceiver::s_process_task(void *arg) {
    auto *self = static_cast<CsiReceiver*>(arg);
    csi_clean_pkt_t pkt;

    while (1) {
        if (xQueueReceive(self->event_queue_, &pkt, portMAX_DELAY) == pdTRUE) {
            if (self->print_mode_ & 2) {
                printf("CSI,%lu,%d,%d", (unsigned long)pkt.seq, pkt.rssi, pkt.subcarrier_count);
                for (int i = 0; i < pkt.subcarrier_count; i++) {
                    printf(",%.2f", pkt.amplitudes[i]);
                }
                printf("\n");
            }
        }
    }
}

void CsiReceiver::s_stats_task(void *arg) {
    auto *self = static_cast<CsiReceiver*>(arg);
    uint32_t last_count = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        uint32_t current_count = self->pkt_count_;
        uint32_t pps = (current_count - last_count) / 2;
        last_count = current_count;

        UBaseType_t free_queue = uxQueueSpacesAvailable(self->event_queue_);
        ESP_LOGI("CSI_RX", "Rates: %lu pkt/s | Free Queue: %u/128", pps, free_queue);
    }
}

void CsiReceiver::init_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void CsiReceiver::init_wifi() {
    esp_netif_init();
    esp_event_loop_create_default();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE);

    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));

    wifi_csi_config_t csi_config = {};
    csi_config.lltf_en = true;
    csi_config.htltf_en = true;
    csi_config.ltf_merge_en = true;
    csi_config.channel_filter_en = true;
    csi_config.manu_scale = false;

    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_config));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(s_csi_cb, this));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));

    ESP_LOGI("CSI_RX", "Native ESP32-S3 CSI Hardware Initialized");
}

void CsiReceiver::begin() {
    s_instance = this;
    event_queue_ = xQueueCreate(128, sizeof(csi_clean_pkt_t));

    init_nvs();
    init_wifi();

    xTaskCreatePinnedToCore(s_process_task, "csi_proc", 4096, this, 10, NULL, 0);
    xTaskCreatePinnedToCore(s_stats_task,   "csi_stat", 2048, this, 3, NULL, 1);
}

CsiReceiver::~CsiReceiver() {
    if (event_queue_) vQueueDelete(event_queue_);
}

void CsiReceiver::setPrintMode(uint8_t mode) { print_mode_ = mode; }
