#ifndef PROGRAMAS_LAVADORA_H
#define PROGRAMAS_LAVADORA_H

#include <Arduino.h>
#include <stdint.h>

enum FaseNombreIndex {
  LLENADO_PRE_LAVADO = 0,
  LLENADO_LAVADO,
  LLENADO_SUAVIZANTE,
  LLENADO,
  LAVADO,
  VACIADO,
  CENTRIFUGAR,
  ESPERA
};

struct FaseIndex {
  uint8_t funcion;
  uint8_t tiempo;
};

extern const FaseIndex programaLargo[];
extern const FaseIndex programaCorto2[];
extern const FaseIndex programaCorto[];
extern const FaseIndex programaVaciado[];
extern const FaseIndex programaCentrifugar[];

#define LAV_FASES_LARGO 17
#define LAV_FASES_CORTO 16
#define LAV_FASES_VACIADO 1
#define LAV_FASES_CORTO2 8
#define LAV_FASES_CENTRIF 2

#endif
