#ifndef MOTOR_SERVICE_H
#define MOTOR_SERVICE_H

#include <Arduino.h>
#include "programas_lavadora.h"

extern bool encendida;
extern const FaseIndex* fases;
extern int faseActual;
extern int totalFases;
extern int paso;
extern int acelerado;
extern int val1, giro, vel1, vel2, motor, bomba;

void setJabonera(void);
void llenado(void);

void motor_reset_service_state(void);
void motor_refresh_outputs(void);

#endif
