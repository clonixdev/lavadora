#include "programas_lavadora.h"

const FaseIndex programaLargo[] = {
  {LLENADO_PRE_LAVADO, 3}, {LLENADO, 3}, {LAVADO, 6}, {VACIADO, 1},
  {CENTRIFUGAR, 3}, {LLENADO_LAVADO, 3}, {LLENADO, 3}, {LAVADO, 7}, {VACIADO, 1},
  {LLENADO_SUAVIZANTE, 3}, {LLENADO, 3}, {LAVADO, 9}, {VACIADO, 1},
  {CENTRIFUGAR, 7}, {ESPERA, 1}, {VACIADO, 1}, {CENTRIFUGAR, 7}
};

const FaseIndex programaCorto2[] = {
  {LLENADO_PRE_LAVADO, 3}, {LLENADO, 3}, {LAVADO, 12}, {VACIADO, 1},
  {CENTRIFUGAR, 3}, {ESPERA, 2}, {CENTRIFUGAR, 2}, {VACIADO, 1},
};

const FaseIndex programaCorto[] = {
  {LLENADO_PRE_LAVADO, 3}, {LLENADO, 3}, {LAVADO, 6}, {VACIADO, 1},
  {LLENADO_LAVADO, 3}, {LLENADO, 3}, {LAVADO, 7}, {VACIADO, 1},
  {LLENADO_SUAVIZANTE, 3}, {LLENADO, 3}, {LAVADO, 8}, {VACIADO, 1},
  {CENTRIFUGAR, 6}, {ESPERA, 1}, {VACIADO, 1}, {CENTRIFUGAR, 3}
};

const FaseIndex programaVaciado[] = {
  {VACIADO, 2},
};

const FaseIndex programaCentrifugar[] = {
  {CENTRIFUGAR, 3}, {VACIADO, 1},
};

static_assert(sizeof(programaLargo) / sizeof(FaseIndex) == LAV_FASES_LARGO, "LAV_FASES_LARGO");
static_assert(sizeof(programaCorto) / sizeof(FaseIndex) == LAV_FASES_CORTO, "LAV_FASES_CORTO");
static_assert(sizeof(programaVaciado) / sizeof(FaseIndex) == LAV_FASES_VACIADO, "LAV_FASES_VACIADO");
static_assert(sizeof(programaCorto2) / sizeof(FaseIndex) == LAV_FASES_CORTO2, "LAV_FASES_CORTO2");
static_assert(sizeof(programaCentrifugar) / sizeof(FaseIndex) == LAV_FASES_CENTRIF, "LAV_FASES_CENTRIF");
