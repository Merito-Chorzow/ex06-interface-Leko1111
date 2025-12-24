#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// 1. Stałe protokołu
#define PROTO_STX       0xAA    // Start of Transmission (magiczna liczba)
#define PROTO_MAX_LEN   32      // Maksymalna długość payloadu
#define PROTO_TIMEOUT_MS 50     // Timeout dla pojedynczego bajtu
#define PROTO_FRAME_TIMEOUT_MS 200 // Timeout dla całej ramki

// 2. Lista komend (CMD) - zgodnie ze sprawozdaniem
typedef enum {
    CMD_SET_SPEED   = 0x10, // Param: uint8_t speed (0-100)
    CMD_MODE        = 0x20, // Param: uint8_t mode (0=OPEN, 1=CLOSED)
    CMD_STOP        = 0x30, // Brak parametrów
    CMD_GET_STAT    = 0x40, // Brak parametrów -> odsyła strukturę STAT
    
    // Odpowiedzi
    CMD_ACK         = 0xAA,
    CMD_NACK        = 0xFF
} CmdType;

// 3. Tryby pracy (do komendy CMD_MODE)
typedef enum {
    MODE_OPEN   = 0,
    MODE_CLOSED = 1
} SysMode;

// 4. Powody błędu (do NACK)
typedef enum {
    NACK_UNKNOWN_CMD = 1,
    NACK_BAD_PARAM   = 2,
    NACK_CRC_ERROR   = 3,
    NACK_TIMEOUT     = 4
} NackReason;

// 5. Struktura statystyk (Telemetria) - to co ma wracać w GET STAT
typedef struct {
    uint32_t rx_dropped;      // Zgubione bajty (z ringbufa)
    uint32_t broken_frames;   // Ramki z błędem formatu/CRC
    uint32_t crc_errors;      // Konkretnie błędy CRC
    uint32_t last_cmd_latency;// Czas reakcji (ms)
    uint32_t ticks;           // Licznik pętli
} Telemetry;

// 6. Struktura ramki (do łatwiejszej obsługi w kodzie)
typedef struct {
    uint8_t len;
    uint8_t cmd;
    uint8_t payload[PROTO_MAX_LEN];
    uint8_t crc;
} Frame;

// --- Funkcje API (deklaracje) ---

// Inicjalizacja drivera
void proto_init(void);

// Maszyna stanów (wywoływana w pętli głównej) - odbiera bajty z RB i składa ramki
// Zwraca 1 jeśli odebrano pełną, poprawną ramkę, 0 w przeciwnym razie.
int proto_poll(Frame *out_frame);

// Funkcja do wysłania ramki (pakuje dane, liczy CRC i wrzuca do RB TX)
void proto_send(uint8_t cmd, const void *payload, uint8_t len);

// Pomocnicza do odsyłania ACK/NACK
void proto_send_ack(void);
void proto_send_nack(uint8_t reason);

#endif // PROTOCOL_H