#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "ringbuf.h"
#include "protocol.h"

// --- FIX DLA QEMU (To naprawia mruganie i brak tekstu) ---
extern void initialise_monitor_handles(void);

// --- Globalne bufory ---
rb_t rb_rx;
rb_t rb_tx;

// Symulacja stanu urządzenia
static struct {
    uint8_t speed;      // 0-100
    SysMode mode;       // OPEN/CLOSED
    uint32_t ticks;     // Czas systemowy
} device = {0, MODE_OPEN, 0};

// --- Funkcje testowe (udają PC) ---

// Wstrzykuje ramkę do bufora RX (jakby przyszła po kablu)
void sim_inject_cmd(uint8_t cmd, const uint8_t *payload, uint8_t len) {
    uint8_t stx = PROTO_STX;
    uint8_t crc = 0;
    
    // Liczymy CRC ręcznie
    crc ^= len;
    crc ^= cmd;
    for(int i=0; i<len; i++) crc ^= payload[i];

    // Wrzucamy do bufora RX
    rb_put(&rb_rx, stx);
    rb_put(&rb_rx, len);
    rb_put(&rb_rx, cmd);
    for(int i=0; i<len; i++) rb_put(&rb_rx, payload[i]);
    rb_put(&rb_rx, crc);

    printf("[TEST] PC wysyla: CMD=0x%02X LEN=%d\n", cmd, len);
}

// Czyta odpowiedź urządzenia z bufora TX
void sim_read_response() {
    uint8_t val;
    printf("[TEST] Odpowiedz urzadzenia (TX): ");
    int count = 0;
    while (rb_get(&rb_tx, &val)) {
        printf("%02X ", val);
        count++;
    }
    if (count == 0) printf("(brak danych)");
    printf("\n");
}

// --- Główna logika (Obsługa ramek) ---

void process_frame(Frame *f) {
    printf("[APP] Przyszla ramka: CMD=0x%02X LEN=%d\n", f->cmd, f->len);

    switch (f->cmd) {
        case CMD_SET_SPEED:
            if (f->len != 1) {
                proto_send_nack(NACK_BAD_PARAM);
                printf("[APP] Blad: zla dlugosc parametru\n");
            } else {
                uint8_t new_speed = f->payload[0];
                if (new_speed > 100) {
                    proto_send_nack(NACK_BAD_PARAM);
                    printf("[APP] Blad: predkosc > 100\n");
                } else {
                    device.speed = new_speed;
                    proto_send_ack();
                    printf("[APP] Sukces: Predkosc ustawiona na %d\n", device.speed);
                }
            }
            break;

        case CMD_MODE:
            if (f->len != 1) {
                proto_send_nack(NACK_BAD_PARAM);
            } else {
                uint8_t new_mode = f->payload[0];
                if (new_mode > 1) {
                    proto_send_nack(NACK_BAD_PARAM);
                } else {
                    device.mode = (SysMode)new_mode;
                    proto_send_ack();
                    printf("[APP] Sukces: Tryb zmieniony na %s\n", new_mode ? "CLOSED" : "OPEN");
                }
            }
            break;

        case CMD_STOP:
            device.speed = 0;
            device.mode = MODE_OPEN; 
            proto_send_ack();
            printf("[APP] ZATRZYMANIE AWARYJNE (STOP)\n");
            break;

        case CMD_GET_STAT: {
            Telemetry t;
            extern void proto_get_stat(Telemetry *out); 
            proto_get_stat(&t);
            t.ticks = device.ticks;

            proto_send(CMD_GET_STAT, &t, sizeof(t));
            printf("[APP] Wyslano telemetrie (Dropped=%ld, CRC_Err=%ld)\n", t.rx_dropped, t.crc_errors);
            break;
        }

        default:
            proto_send_nack(NACK_UNKNOWN_CMD);
            printf("[APP] Blad: Nieznana komenda\n");
            break;
    }
}

int main(void) {
    // 1. ODPALENIE KONSOLI (Kluczowe dla QEMU)
    initialise_monitor_handles(); 
    setbuf(stdout, NULL); // Wyłączamy buforowanie, żeby tekst był od razu

    // 2. Inicjalizacja buforów i protokołu
    rb_init(&rb_rx);
    rb_init(&rb_tx);
    proto_init();

    printf("=== START SYSTEMU ===\n");

    // --- TEST 1: Ustawienie prędkości ---
    printf("\n--- TEST 1: SET SPEED (Ok) ---\n");
    uint8_t speed_val = 80;
    sim_inject_cmd(CMD_SET_SPEED, &speed_val, 1);
    
    Frame frame;
    if (proto_poll(&frame)) {
        process_frame(&frame);
    }
    sim_read_response(); 

    // --- TEST 2: Nieznana komenda ---
    printf("\n--- TEST 2: Nieznana Komenda ---\n");
    sim_inject_cmd(0x99, NULL, 0); 
    
    if (proto_poll(&frame)) {
        process_frame(&frame);
    }
    sim_read_response(); 

    // --- TEST 3: Błędy CRC ---
    printf("\n--- TEST 3: Zle CRC ---\n");
    rb_put(&rb_rx, PROTO_STX);
    rb_put(&rb_rx, 0); 
    rb_put(&rb_rx, CMD_STOP);
    rb_put(&rb_rx, 0x00); // Błędne CRC
    
    if (proto_poll(&frame)) {
        printf("BLAD: Ramka powinna zostac odrzucona!\n");
    } else {
        printf("[SYSTEM] Ramka odrzucona prawidlowo (blad sumy kontrolnej)\n");
    }

    // --- TEST 4: Pobranie statystyk ---
    printf("\n--- TEST 4: GET STAT ---\n");
    sim_inject_cmd(CMD_GET_STAT, NULL, 0);
    
    if (proto_poll(&frame)) {
        process_frame(&frame);
    }
    sim_read_response(); 

    return 0;
}