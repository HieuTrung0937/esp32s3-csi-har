#ifndef ESP32S3_CSI_HAR_H
#define ESP32S3_CSI_HAR_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CSI_MODE_TX = 0,
    CSI_MODE_RX = 1
} csi_mode_t;

typedef enum {
    CSI_PRINT_OFF       = 0,
    CSI_PRINT_RSSI      = 1,
    CSI_PRINT_AMPLITUDE = 2
} csi_print_mode_t;

#define MODE_TX           CSI_MODE_TX
#define MODE_RX           CSI_MODE_RX
#define PRINT_MODE_CSI    CSI_PRINT_AMPLITUDE
#define PRINT_MODE_AMPLITUDE CSI_PRINT_AMPLITUDE

#ifdef __cplusplus
extern "C" {
#endif

void csi_har_set_mac(const uint8_t *tx_mac, const uint8_t *rx_mac);
void csi_har_begin(csi_mode_t mode, int channel);
void csi_har_set_print_mode(csi_print_mode_t mode);

void CSIFallGuard_set_tx_mac(const uint8_t *mac);
void CSIFallGuard_set_rx_mac(const uint8_t *mac);
void CSIFallGuard_set_channel(int ch);
void CSIFallGuard_begin(int mode, int ch);
void CSIFallGuard_set_print_mode(uint8_t mode);
void CSIFallGuard_update(void);

#ifdef __cplusplus
}
#endif

#endif
