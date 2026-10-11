#ifndef CSI_RECEIVER_HPP
#define CSI_RECEIVER_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_wifi.h>
#include <stdint.h>
#include <stdbool.h>

class CsiReceiver {
public:
    CsiReceiver() = default;
    ~CsiReceiver();

    void begin();
    void setPrintMode(uint8_t mode);

    void setThreshold(uint8_t standing, uint8_t lying) {}
    void setLyingParams(int delta_threshold, int confirm_ms, int exit_ms) {}
    bool isLying() { return false; }
    bool isPersonPresent() { return false; }
    bool isFallDetected() { return false; }

private:
    static void s_csi_cb(void *ctx, wifi_csi_info_t *info);
    static void s_process_task(void *arg);
    static void s_stats_task(void *arg);

    void init_nvs();
    void init_wifi();

    static CsiReceiver *s_instance;
    QueueHandle_t event_queue_ = nullptr;
    uint32_t pkt_count_ = 0;
    uint8_t print_mode_ = 2;
};

#endif
