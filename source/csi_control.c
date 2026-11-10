/*
 * csi_control.c - ON/OFF policy and control-message formatting (see csi_control.h).
 */
#include "csi_control.h"

#include <stdio.h>

static const char *const s_reason_name[] = {"startup", "prediction", "stale", "fault"};

void csi_ctrl_init(csi_ctrl_t *c, uint32_t now_ms)
{
    c->on            = 0U;
    c->reason        = (uint8_t)CSI_CTRL_REASON_STARTUP;
    c->fault_latched = 0U;
    c->conf_pct      = 0U;
    c->cls_name      = NULL;
    c->last_pred_ms  = now_ms;
    c->boot_ms       = now_ms;
}

static bool set_off(csi_ctrl_t *c, csi_ctrl_reason_t reason)
{
    bool changed = (c->on != 0U);
    c->on        = 0U;
    c->reason    = (uint8_t)reason;
    c->cls_name  = NULL;
    c->conf_pct  = 0U;
    return changed;
}

bool csi_ctrl_on_prediction(csi_ctrl_t *c, uint8_t action, const char *cls_name, uint8_t conf_pct,
                            uint32_t now_ms)
{
    if ((c->fault_latched != 0U) || (cls_name == NULL))
    {
        return false;
    }
    uint8_t was     = c->on;
    c->reason       = (uint8_t)CSI_CTRL_REASON_PREDICTION;
    c->cls_name     = cls_name;
    c->conf_pct     = (conf_pct > 100U) ? 100U : conf_pct;
    c->last_pred_ms = now_ms;
    if (action == CSI_CTRL_ACTION_ON)
    {
        c->on = 1U;
    }
    else if (action == CSI_CTRL_ACTION_OFF)
    {
        c->on = 0U;
    }
    /* CSI_CTRL_ACTION_HOLD (and anything unknown) keeps the current state */
    return c->on != was;
}

bool csi_ctrl_on_fault(csi_ctrl_t *c, bool latch)
{
    if (latch)
    {
        c->fault_latched = 1U;
    }
    return set_off(c, CSI_CTRL_REASON_FAULT);
}

bool csi_ctrl_poll(csi_ctrl_t *c, uint32_t now_ms, uint32_t stale_ms, uint32_t startup_grace_ms)
{
    if (c->fault_latched != 0U)
    {
        return set_off(c, CSI_CTRL_REASON_FAULT);
    }
    if ((c->reason == (uint8_t)CSI_CTRL_REASON_PREDICTION) && ((uint32_t)(now_ms - c->last_pred_ms) > stale_ms))
    {
        return set_off(c, CSI_CTRL_REASON_STALE);
    }
    if ((c->reason == (uint8_t)CSI_CTRL_REASON_STARTUP) && ((uint32_t)(now_ms - c->boot_ms) > startup_grace_ms))
    {
        return set_off(c, CSI_CTRL_REASON_STALE);
    }
    return false;
}

uint16_t csi_crc16_ccitt(const uint8_t *d, size_t n)
{
    uint16_t crc = 0xFFFFU;
    while (n-- > 0U)
    {
        crc ^= (uint16_t)((uint16_t)(*d++) << 8);
        for (int i = 0; i < 8; i++)
        {
            crc = ((crc & 0x8000U) != 0U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

int csi_ctrl_format(const csi_ctrl_t *c, uint32_t seq, uint32_t up_ms, bool with_crc, char *buf, size_t size)
{
    bool pred = (c->reason == (uint8_t)CSI_CTRL_REASON_PREDICTION) && (c->cls_name != NULL);
    /* Only a prediction may hold ON; anything else is reported (and is) OFF. */
    const char *state  = (pred && (c->on != 0U)) ? "on" : "off";
    const char *reason = (c->reason < 4U) ? s_reason_name[c->reason] : "fault";
    int n;

    if (pred)
    {
        n = snprintf(buf, size,
                     "{\"version\":1,\"event\":\"control\",\"state\":\"%s\",\"reason\":\"%s\",\"class\":\"%s\","
                     "\"confidence\":%u.%02u,\"seq\":%lu,\"up_ms\":%lu",
                     state, reason, c->cls_name, (unsigned)(c->conf_pct / 100U), (unsigned)(c->conf_pct % 100U),
                     (unsigned long)seq, (unsigned long)up_ms);
    }
    else
    {
        n = snprintf(buf, size,
                     "{\"version\":1,\"event\":\"control\",\"state\":\"off\",\"reason\":\"%s\",\"class\":null,"
                     "\"confidence\":null,\"seq\":%lu,\"up_ms\":%lu",
                     reason, (unsigned long)seq, (unsigned long)up_ms);
    }
    if ((n <= 0) || ((size_t)n >= size))
    {
        return -1;
    }
    int m;
    if (with_crc)
    {
        uint16_t crc = csi_crc16_ccitt((const uint8_t *)buf, (size_t)n);
        m            = snprintf(&buf[n], size - (size_t)n, ",\"crc\":%u}\n", (unsigned)crc);
    }
    else
    {
        m = snprintf(&buf[n], size - (size_t)n, "}\n");
    }
    if ((m <= 0) || ((size_t)(n + m) >= size))
    {
        return -1;
    }
    return n + m;
}
