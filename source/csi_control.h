/*
 * csi_control.h - ON/OFF control policy and the control-UART JSON message.
 *
 * Pure C (no RTOS/SDK calls) so it can be unit-tested on a PC. Owned by the
 * output task in csi_uart_out.c; not thread-safe on its own.
 *
 * Policy (per class, from CSI_CLASS_ACTION[] in the generated csi_model_config.h):
 *   ON   e.g. approaching            -> state ON
 *   OFF  e.g. leaving, empty         -> state OFF
 *   HOLD e.g. idle-nearby, passing-by -> state unchanged (never turns ON by itself)
 * and fail-safe OFF for: startup (no classification yet), stale (no successful
 * classification for stale_ms), and faults (invalid model output; self-test or
 * init failure latches OFF until reboot).
 *
 * Message (schemas/presense-control.v1.schema.json), one line, '\n' terminated:
 *   {"version":1,"event":"control","state":"on","reason":"prediction",
 *    "class":"approaching","confidence":0.87,"seq":42,"up_ms":123456,"crc":51234}
 * "class"/"confidence" are null unless reason is "prediction". "crc" is
 * CRC-16/CCITT-FALSE over every byte before ',"crc"'.
 */
#ifndef CSI_CONTROL_H_
#define CSI_CONTROL_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CSI_CTRL_ACTION_HOLD 0U
#define CSI_CTRL_ACTION_ON   1U
#define CSI_CTRL_ACTION_OFF  2U

typedef enum
{
    CSI_CTRL_REASON_STARTUP    = 0,
    CSI_CTRL_REASON_PREDICTION = 1,
    CSI_CTRL_REASON_STALE      = 2,
    CSI_CTRL_REASON_FAULT      = 3
} csi_ctrl_reason_t;

typedef struct
{
    uint8_t on;            /* 1 = ON */
    uint8_t reason;        /* csi_ctrl_reason_t */
    uint8_t fault_latched; /* 1 = stays OFF until reboot */
    uint8_t conf_pct;      /* confidence of the latest prediction, 0..100 */
    const char *cls_name;  /* latest predicted class; valid only for reason PREDICTION */
    uint32_t last_pred_ms; /* time of the latest successful classification */
    uint32_t boot_ms;
} csi_ctrl_t;

/* Starts OFF with reason "startup". */
void csi_ctrl_init(csi_ctrl_t *c, uint32_t now_ms);

/* Applies one successful classification. Returns true if ON/OFF changed. */
bool csi_ctrl_on_prediction(csi_ctrl_t *c, uint8_t action, const char *cls_name, uint8_t conf_pct,
                            uint32_t now_ms);

/* Forces OFF with reason "fault". latch = stay OFF until reboot.
 * Returns true if ON/OFF changed. */
bool csi_ctrl_on_fault(csi_ctrl_t *c, bool latch);

/* Deadline checks; call at least every few hundred ms. A prediction older than
 * stale_ms forces OFF ("stale"); with no prediction since boot the reason turns
 * from "startup" to "stale" after startup_grace_ms. Returns true if ON/OFF changed. */
bool csi_ctrl_poll(csi_ctrl_t *c, uint32_t now_ms, uint32_t stale_ms, uint32_t startup_grace_ms);

/* Writes one complete message including '\n' and a terminating NUL.
 * Returns its length (without NUL), or -1 if it does not fit. */
int csi_ctrl_format(const csi_ctrl_t *c, uint32_t seq, uint32_t up_ms, bool with_crc, char *buf, size_t size);

/* CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no final xor. */
uint16_t csi_crc16_ccitt(const uint8_t *d, size_t n);

#endif /* CSI_CONTROL_H_ */
