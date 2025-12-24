#include "protocol.h"
#include "ringbuf.h"
#include <stddef.h>
#include <string.h>

// POPRAWKA: Używamy rb_t zamiast ringbuf_t (zgodnie z Twoim ringbuf.h)
extern rb_t rb_rx;
extern rb_t rb_tx;

// --- Zmienne prywatne (stan FSM) ---

typedef enum {
    STATE_WAIT_STX,
    STATE_WAIT_LEN,
    STATE_WAIT_CMD,
    STATE_WAIT_DATA,
    STATE_WAIT_CRC
} ProtoState;

static ProtoState rx_state = STATE_WAIT_STX;
static uint8_t rx_buf[PROTO_MAX_LEN];
static uint8_t rx_len_expected = 0;
static uint8_t rx_idx = 0;
static uint8_t rx_cmd_tmp = 0;

Telemetry telem = {0};

// --- Funkcje pomocnicze ---

static uint8_t calc_crc(uint8_t len, uint8_t cmd, const uint8_t *data) {
    uint8_t crc = 0;
    crc ^= len;
    crc ^= cmd;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
    }
    return crc;
}

static void fsm_reset(void) {
    rx_state = STATE_WAIT_STX;
    rx_idx = 0;
}

// --- Implementacja API ---

void proto_init(void) {
    fsm_reset();
    memset(&telem, 0, sizeof(telem));
}

void proto_send(uint8_t cmd, const void *payload, uint8_t len) {
    if (len > PROTO_MAX_LEN) return;

    const uint8_t *data = (const uint8_t*)payload;
    uint8_t crc = calc_crc(len, cmd, data);

    // Sekwencja wysyłania: STX | LEN | CMD | DATA... | CRC
    uint8_t stx = PROTO_STX;
    
    rb_put(&rb_tx, stx);
    rb_put(&rb_tx, len);
    rb_put(&rb_tx, cmd);
    
    for (int i = 0; i < len; i++) {
        rb_put(&rb_tx, data[i]);
    }
    
    rb_put(&rb_tx, crc);
}

void proto_send_ack(void) {
    proto_send(CMD_ACK, NULL, 0);
}

void proto_send_nack(uint8_t reason) {
    proto_send(CMD_NACK, &reason, 1);
}

int proto_poll(Frame *out_frame) {
    // POPRAWKA: Zmienna musi być uint8_t, bo tego oczekuje rb_get
    uint8_t byte_in;

    // Aktualizuj statystykę dropped (zakładamy, że rb_t ma pole dropped)
    telem.rx_dropped = rb_rx.dropped; 

    // Przekazujemy adres &byte_in (który teraz jest uint8_t*)
    while (rb_get(&rb_rx, &byte_in)) {
        uint8_t b = byte_in;

        switch (rx_state) {
            case STATE_WAIT_STX:
                if (b == PROTO_STX) {
                    rx_state = STATE_WAIT_LEN;
                }
                break;

            case STATE_WAIT_LEN:
                if (b > PROTO_MAX_LEN) {
                    telem.broken_frames++;
                    fsm_reset(); 
                } else {
                    rx_len_expected = b;
                    rx_state = STATE_WAIT_CMD;
                }
                break;

            case STATE_WAIT_CMD:
                rx_cmd_tmp = b;
                rx_idx = 0;
                if (rx_len_expected > 0) {
                    rx_state = STATE_WAIT_DATA;
                } else {
                    rx_state = STATE_WAIT_CRC;
                }
                break;

            case STATE_WAIT_DATA:
                rx_buf[rx_idx++] = b;
                if (rx_idx >= rx_len_expected) {
                    rx_state = STATE_WAIT_CRC;
                }
                break;

            case STATE_WAIT_CRC:
                {
                    uint8_t expected_crc = calc_crc(rx_len_expected, rx_cmd_tmp, rx_buf);
                    
                    if (b == expected_crc) {
                        if (out_frame) {
                            out_frame->len = rx_len_expected;
                            out_frame->cmd = rx_cmd_tmp;
                            out_frame->crc = b;
                            memcpy(out_frame->payload, rx_buf, rx_len_expected);
                        }
                        fsm_reset();
                        return 1;
                    } else {
                        telem.crc_errors++;
                        fsm_reset();
                    }
                }
                break;
        }
    }
    
    return 0;
}

void proto_get_stat(Telemetry *out) {
    if (out) {
        *out = telem;
    }
}