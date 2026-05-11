#ifndef RECOVERY_EEPROM_H
#define RECOVERY_EEPROM_H

#include <Arduino.h>
#include <EEPROM.h>
#include <stddef.h>
#include <string.h>

static const uint16_t EEPROM_CP_MAGIC = 0x574C;
static const uint8_t EEPROM_CP_VERSION = 1;
static const int EEPROM_CP_ADDR = 0;

enum RecoveryStateE : uint8_t {
  REC_NONE = 0,
  REC_RUNNING = 1,
  REC_ERROR = 2,
};

enum ErrorCodeE : uint8_t {
  ERR_NONE = 0,
  ERR_FILL_TIMEOUT = 1,
  ERR_LLENADO = 2,
};

struct WashCheckpoint {
  uint16_t magic;
  uint8_t version;
  uint8_t recovery_state;
  uint8_t programa;
  uint8_t totalFases_cp;
  uint8_t faseActual;
  uint8_t minuto;
  uint8_t segundos;
  uint8_t paso;
  uint8_t acelerado;
  uint8_t sttone;
  uint16_t tiempoTranscurrido;
  uint8_t last_error_code;
  uint8_t tambor_saved;
  uint8_t reserved[2];
  uint16_t crc16;
} __attribute__((packed));

static uint16_t crc16_modbus_update(uint16_t crc, uint8_t data) {
  crc ^= data;
  for (uint8_t i = 0; i < 8; i++)
    crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
  return crc;
}

static uint16_t checkpoint_compute_crc(const WashCheckpoint* c) {
  const uint8_t* p = (const uint8_t*)c;
  const size_t n = offsetof(WashCheckpoint, crc16);
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < n; i++)
    crc = crc16_modbus_update(crc, p[i]);
  return crc;
}

inline void checkpoint_clear(void) {
  for (unsigned i = 0; i < sizeof(WashCheckpoint); i++)
    EEPROM.update(EEPROM_CP_ADDR + i, 0);
}

inline bool checkpoint_read(WashCheckpoint* out) {
  EEPROM.get(EEPROM_CP_ADDR, *out);
  if (out->magic != EEPROM_CP_MAGIC || out->version != EEPROM_CP_VERSION)
    return false;
  uint16_t stored = out->crc16;
  out->crc16 = 0;
  uint16_t calc = checkpoint_compute_crc(out);
  out->crc16 = stored;
  return calc == stored;
}

inline void checkpoint_write(const WashCheckpoint* cp_in) {
  WashCheckpoint cp = *cp_in;
  cp.magic = EEPROM_CP_MAGIC;
  cp.version = EEPROM_CP_VERSION;
  cp.crc16 = 0;
  cp.crc16 = checkpoint_compute_crc(&cp);
  EEPROM.put(EEPROM_CP_ADDR, cp);
}

#endif
