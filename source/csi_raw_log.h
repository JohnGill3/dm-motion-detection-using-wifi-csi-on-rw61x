/*
 * csi_raw_log.h - log complete raw CSI records from the RW612 Wi-Fi driver.
 *
 * Usage (before enabling CSI):
 *     csi_raw_log_init();
 *     wlan_register_csi_user_callback(csi_raw_log_cb);
 */
#ifndef CSI_RAW_LOG_H_
#define CSI_RAW_LOG_H_

#include <stddef.h>
#include <stdint.h>

/* Largest record (header 48 B + CSI data + tail 4 B) that will be logged.
 * 512 covers 1x1 at 20/40 MHz. Use 1024 for 2x2 or 80 MHz. Bigger records
 * are counted as dropped. */
#ifndef CSI_LOG_MAX_RECORD_BYTES
#define CSI_LOG_MAX_RECORD_BYTES 512U
#endif

/* Number of records buffered between the Wi-Fi driver and the UART (<= 255). */
#ifndef CSI_LOG_QUEUE_DEPTH
#define CSI_LOG_QUEUE_DEPTH 16U
#endif

/* 1 = also print a decoded header line (CSIH,...) after every record. */
#ifndef CSI_LOG_PRINT_SUMMARY
#define CSI_LOG_PRINT_SUMMARY 0
#endif

/* Creates the buffers and the low-priority logger task. Returns 0 on success. */
int csi_raw_log_init(void);

/* Pass this to wlan_register_csi_user_callback(). Only copies the record. */
int csi_raw_log_cb(void *buffer, size_t len);

#endif /* CSI_RAW_LOG_H_ */
