#include "motor_service.h"

extern int tamborVacio;

static uint8_t cen_sm = 0;
static unsigned long cen_deadline = 0;

void motor_reset_service_state(void) {
  cen_sm = 0;
  cen_deadline = 0;
}

static void lavado_apply_steady_outputs(void) {
  switch (paso) {
    case 0:
      digitalWrite(motor, HIGH);
      digitalWrite(vel1, HIGH);
      digitalWrite(vel2, HIGH);
      digitalWrite(bomba, HIGH);
      break;
    case 1:
      digitalWrite(motor, HIGH);
      digitalWrite(giro, HIGH);
      break;
    case 2:
      digitalWrite(giro, HIGH);
      digitalWrite(motor, LOW);
      break;
    case 3:
      digitalWrite(motor, HIGH);
      digitalWrite(giro, LOW);
      break;
    case 4:
      digitalWrite(giro, LOW);
      digitalWrite(motor, LOW);
      break;
    default:
      break;
  }
}

static void vaciado_apply_steady_outputs(void) {
  digitalWrite(val1, HIGH);
  digitalWrite(giro, HIGH);
  digitalWrite(vel1, HIGH);
  digitalWrite(vel2, HIGH);
  digitalWrite(motor, HIGH);
  digitalWrite(bomba, LOW);
}

static void centrifugar_apply_nonblocking(void) {
  unsigned long now = millis();

  if (acelerado != 0) {
    digitalWrite(val1, HIGH);
    digitalWrite(giro, LOW);
    digitalWrite(vel1, LOW);
    digitalWrite(vel2, LOW);
    digitalWrite(bomba, LOW);
    digitalWrite(motor, LOW);
    return;
  }

  switch (cen_sm) {
    case 0:
      digitalWrite(val1, HIGH);
      digitalWrite(giro, LOW);
      digitalWrite(vel1, HIGH);
      digitalWrite(vel2, HIGH);
      cen_deadline = now + 1000UL;
      cen_sm = 1;
      break;
    case 1:
      if (now < cen_deadline)
        break;
      digitalWrite(motor, LOW);
      cen_deadline = now + 3000UL;
      cen_sm = 2;
      break;
    case 2:
      if (now < cen_deadline)
        break;
      digitalWrite(motor, HIGH);
      cen_deadline = now + 10UL;
      cen_sm = 3;
      break;
    case 3:
      if (now < cen_deadline)
        break;
      digitalWrite(vel1, LOW);
      digitalWrite(vel2, LOW);
      cen_deadline = now + 10UL;
      cen_sm = 4;
      break;
    case 4:
      if (now < cen_deadline)
        break;
      digitalWrite(giro, LOW);
      cen_deadline = now + 100UL;
      cen_sm = 5;
      break;
    case 5:
      if (now < cen_deadline)
        break;
      digitalWrite(motor, LOW);
      acelerado = 1;
      cen_sm = 6;
      break;
    default:
      digitalWrite(vel1, LOW);
      digitalWrite(vel2, LOW);
      digitalWrite(bomba, LOW);
      digitalWrite(motor, LOW);
      break;
  }
}

void motor_refresh_outputs(void) {
  if (!encendida || fases == nullptr || faseActual < 0 || faseActual >= totalFases)
    return;

  FaseIndex f = fases[faseActual];

  if (f.funcion != CENTRIFUGAR)
    cen_sm = 0;

  switch (f.funcion) {
    case LLENADO_LAVADO:
      llenado();
      /* Mientras el tambor no tiene agua, no conmutar el motor: evita picos en
         la alimentacion que hacen vibrar el servo de la jabonera. */
      if (tamborVacio == 0) {
        lavado_apply_steady_outputs();
      } else {
        digitalWrite(motor, HIGH);
        digitalWrite(giro, HIGH);
        digitalWrite(vel1, HIGH);
        digitalWrite(vel2, HIGH);
        digitalWrite(bomba, HIGH);
      }
      break;
    case LLENADO_PRE_LAVADO:
    case LLENADO:
    case LLENADO_SUAVIZANTE:
      llenado();
      break;
    case LAVADO:
      digitalWrite(val1, HIGH);
      lavado_apply_steady_outputs();
      break;
    case VACIADO:
      digitalWrite(val1, HIGH);
      vaciado_apply_steady_outputs();
      break;
    case CENTRIFUGAR:
      centrifugar_apply_nonblocking();
      break;
    default:
      break;
  }
}
