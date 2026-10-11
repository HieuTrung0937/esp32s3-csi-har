#include "esp32s3-csi-har.h"
#include "esp32s3-csi-har/CsiReceiver.hpp"
#include "esp32s3-csi-har/EspNowTransmitter.hpp"
#include <string.h>

extern uint8_t s_tx_mac[6];
extern uint8_t s_rx_mac[6];
extern int s_channel;

static EspNowTransmitter s_tx;
static CsiReceiver s_rx;

void csi_har_set_mac(const uint8_t *tx_mac, const uint8_t *rx_mac) {
    if (tx_mac) memcpy(s_tx_mac, tx_mac, 6);
    if (rx_mac) memcpy(s_rx_mac, rx_mac, 6);
}

void csi_har_begin(csi_mode_t mode, int channel) {
    s_channel = channel;
    if (mode == CSI_MODE_TX) {
        s_tx.begin();
    } else {
        s_rx.begin();
    }
}

void csi_har_set_print_mode(csi_print_mode_t mode) {
    s_rx.setPrintMode((uint8_t)mode);
}

extern "C" {
    void CSIFallGuard_set_tx_mac(const uint8_t *mac) { csi_har_set_mac(mac, NULL); }
    void CSIFallGuard_set_rx_mac(const uint8_t *mac) { csi_har_set_mac(NULL, mac); }
    void CSIFallGuard_set_channel(int ch) { s_channel = ch; }
    void CSIFallGuard_begin(int mode, int ch) { csi_har_begin((csi_mode_t)mode, ch); }
    void CSIFallGuard_set_print_mode(uint8_t mode) { csi_har_set_print_mode((csi_print_mode_t)mode); }
    void CSIFallGuard_update(void) {}
}
