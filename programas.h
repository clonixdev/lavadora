#ifndef PROGRAMAS_H
#define PROGRAMAS_H

#include "programas_lavadora.h"

extern int programa;

static inline const FaseIndex* getPrograma(void) {
  if (programa == 1)
    return programaLargo;
  if (programa == 2)
    return programaCorto;
  if (programa == 3)
    return programaVaciado;
  if (programa == 4)
    return programaCorto2;
  if (programa == 5)
    return programaCentrifugar;
  if (programa == 6)
    return programaCargaAgua;
  if (programa == 7)
    return programaSoloLavado;
  return programaLargo;
}

#endif
