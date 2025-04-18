// FASES DE LAVADO, FUNCION - TIEMPO en minutos
const FaseIndex programaLargo[] = {
  {LLENADO_PRE_LAVADO, 5}, {LLENADO, 5}, {LAVADO, 8}, {VACIADO, 1},
  {LLENADO_PRE_LAVADO, 5}, {LLENADO, 5}, {LAVADO, 8}, {VACIADO, 1},
  {CENTRIFUGAR, 5}, {LLENADO_LAVADO, 5}, {LLENADO, 5}, {LAVADO, 8},
  {VACIADO, 1}, {LLENADO_SUAVIZANTE, 5}, {LLENADO, 5}, {LAVADO, 8},
  {VACIADO, 1}, {LLENADO_SUAVIZANTE, 5}, {LLENADO, 5}, {LAVADO, 8},
  {VACIADO, 1}, {CENTRIFUGAR, 10}, {ESPERA, 2}, {VACIADO, 1}, {CENTRIFUGAR, 10}
};

const FaseIndex programaCorto2[] = {
  {LLENADO_PRE_LAVADO, 5}, {LLENADO, 5}, {LAVADO, 15}, {VACIADO, 1},
  {CENTRIFUGAR, 5},{ESPERA, 2},{CENTRIFUGAR, 3}, {VACIADO, 1},
};

const FaseIndex programaCorto[] = {
  {LAVADO, 15}, {LLENADO, 1}, {LAVADO, 1}, {VACIADO, 1},
  {LLENADO_SUAVIZANTE, 1}, {LLENADO, 1}, {LAVADO, 1}, {VACIADO, 1},
  {LLENADO_SUAVIZANTE, 1}, {LLENADO, 1}, {LAVADO, 1}, {VACIADO, 1},
  {CENTRIFUGAR, 2}, {ESPERA, 2}, {VACIADO, 1}, {CENTRIFUGAR, 2}
};

const FaseIndex programaVaciado[] = {
  {VACIADO, 2},
};


const FaseIndex* getPrograma() {
  if (programa == 1) {
    return programaLargo;
  } else if (programa == 2) {
    return programaCorto;
  } else if (programa == 3) {
    return programaVaciado;
  } else if (programa == 4) {
    return programaCorto2;
  }else {
    return programaLargo; // Por defecto
  }
}