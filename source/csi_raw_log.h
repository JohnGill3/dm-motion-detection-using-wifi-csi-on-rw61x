/*
 * csi_raw_log.h - Wi-Fi driver CSI callback -> record pool -> worker task.
 *
 * Register csi_raw_log_cb() with wlan_register_csi_user_callback() only AFTER
 * csi_raw_log_init() has returned 0. The callback validates and copies records
 * (it never blocks); the worker task then
 *   - prints them as hex lines for csi_logger.py   (CSI_LOG_RAW=1)
 *   - feeds csi_presence_feed() for classification (CSI_PRESENCE_ENABLE=1)
 */
#ifndef CSI_RAW_LOG_H_
#define CSI_RAW_LOG_H_

#include <stddef.h>
#include <stdint.h>

/* 1 = print every record as CSI,<seq>,<tick_ms>,<bytes>,<hex> (data collection).
 * Turn off while classifying: at ~10 records/s it floods the console. */
#ifndef CSI_LOG_RAW
#define CSI_LOG_RAW 1
#endif

/* 1 = feed records to the classifier (csi_presence.c, needs the generated headers). */
#ifndef CSI_PRESENCE_ENABLE
#define CSI_PRESENCE_ENABLE 0
#endif

/* 1 = also print a decoded header line (CSIH,...) after each raw record. */
#ifndef CSI_LOG_PRINT_SUMMARY
#define CSI_LOG_PRINT_SUMMARY 0
#endif

/* Largest record copied: 48-byte header + CSI data + 4-byte tail. 20 MHz HT
 * records are 156 bytes (104 data bytes); 512 also fits 40 MHz/VHT records.
 * Larger records are counted as drops. */
#ifndef CSI_LOG_MAX_RECORD_BYTES
#define CSI_LOG_MAX_RECORD_BYTES 512U
#endif

/* Pool entries between the callback and the worker task (max 255). 16 x 512 B
 * of .bss; this depth is proven for raw logging at the collection rate. */
#ifndef CSI_LOG_QUEUE_DEPTH
#define CSI_LOG_QUEUE_DEPTH 16U
#endif

#if CSI_LOG_QUEUE_DEPTH > 255U
#error "CSI_LOG_QUEUE_DEPTH must fit in uint8_t"
#endif

/* Creates the pool, queues and worker task; with CSI_PRESENCE_ENABLE also
 * initialises the classifier and output task. Returns 0 on success; on failure
 * do not register the callback (the control output stays OFF / silent, and the
 * receiver's watchdog must treat silence as OFF). */
int csi_raw_log_init(void);

/* CSI callback for wlan_register_csi_user_callback(). Task context only. */
int csi_raw_log_cb(void *buffer, size_t len);

#endif /* CSI_RAW_LOG_H_ */
