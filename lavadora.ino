#ifndef USE_JABONERA_SERVO
#define USE_JABONERA_SERVO 1
#endif
#if USE_JABONERA_SERVO
#include <Servo.h>
#endif
#include <NeoSWSerial.h>
#include <stdio.h>
#include <string.h>
#include <avr/wdt.h>
/*
 * UART wire (sin JSON), lineas terminadas en \n:
 * Comandos ESP->Arduino (prefijo '>'):
 *   >START,L|C|2|V|X|A|W|M  largo|corto|corto2|vaciado|centrifugar|carga_agua|solo_lavado|mini
 *   >STOP   >DISCARD   >RESUME,0|1   >JABON   >J1|>J2|>J3   >PING (respuesta OK PONG; pitido corto en alarma)
 *   Linea UART por ESP con subcadena "OLAF" (p. ej. basura RX): pitido grave adicional (diagnostico).
 *   Parser: primer '>' en la linea (basura delante); cola solo espacios/control; >PING sin distinguir mayus/minus.
 * Estado Arduino->ESP (prefijo S|): 21 campos separados por |
 *   S|Enc|Fa|Tf|Tv|Mn|Se|Pa|Tt|Tr|Pid|Fcode|Rp|Rcode|Ec|E0|E1|E2|Rx|Besp|InAgua|InBloc
 *   InAgua/InBloc: lectura digital pines feedback (A0/A3); 1=HIGH 0=LOW (INPUT_PULLUP: contacto a GND = 0).
 *   Besp: bytes leidos desde NeoSWSerial; sube con cualquier trafico en RX (comandos, eco S|, ruido).
 *   [diag] USB: tambien cuenta lineas completas S| vistas solo por espSerial (ver g_esp_s_pipe_rx_lines).
 *   Rx: lineas '>' recibidas por UART (cable ESP GPIO1/TX -> Arduino A1=D15, NeoSWSerial RX)
 *   Fcode: 0 idle 1 llenado_pre .. 9 desconocido (ver nombre_fase_actual_code)
 *   Rcode recuperacion UI: 0 ninguno 1 error 2 power_loss
 * Boot pendiente: R|rec|rcode|prog|fa|tf|mn|se|ec|ts  (rec=estado EEPROM)
 *
 * LLENADO_PRE_LAVADO y LLENADO_LAVADO: solo llenado hasta presostato; con agua, lavado por paso.
 *   Si se pierde el agua durante el agitado, nuevo llenado (timeout reiniciado), paso=0; REFILL LLPRL / LLLAV.
 * Depuracion monitor USB: DEBUG_UART_USB_LINES (1 por defecto) imprime cada linea \n recibida
 * por espSerial como [ESP RX] ... y por Serial como [USB RX] ... (no se reenvia al ESP).
 *
 * Jabonera servo: USE_JABONERA_SERVO 0 desactiva Servo.h, attach y movimientos (prueba UART/timers).
 *   Rehabilitar: definir USE_JABONERA_SERVO 1 antes de compilar.
 */
#include "recovery_eeprom.h"
#include "programas_lavadora.h"
#include "motor_service.h"
					
// Cable: salida del pad TX del modulo ESP (GPIO1) -> A1 (RX). Pad RX del ESP (GPIO3) <- A2 (TX). GND comun.
// No conectar el pad RX del ESP al A1: ahi solo llegarian datos si el Arduino transmitiera por error a GPIO3.
// Enlace ESP<->Arduino 9600 baud (debe coincidir con uart: baud_rate en ESPHome; ambos firmwares a la vez).
// Serial USB del IDE sigue a 9600.
#ifndef DEBUG_UART_USB_LINES
#define DEBUG_UART_USB_LINES 0
#endif

NeoSWSerial espSerial(A1, A2);
bool led = true;
bool encendida = false;
bool hasError = false;
unsigned long hora = 0;
const int intervalo = 1000;
int segundos = 0;
int minuto = 0;
/** Subciclo agitado: 0=giroA 5s, 1=pausa 3s, 2=giroB 5s, 3=pausa 3s (ver LAVADO_PASO_DUR_SEC). */
int paso = 0;
#define LAVADO_PASOS 4
static const uint8_t LAVADO_PASO_DUR_SEC[LAVADO_PASOS] = {5, 3, 5, 3};
uint8_t paso_seg = 0;
int sttone = 0; // TONO INICIAL
#if USE_JABONERA_SERVO
Servo jabservo;
#endif
int totalFases = 0;
int tiempoTranscurrido = 0; 
int ultimoSegundoEnviado = -1;
int ultimoSegundoLavadora = -1;
int presostato = 17;
/** Feedback hardware: carga/valvula entrada de agua encendida (optico, contacto aux. rele, etc.). */
constexpr int pinSensorCargaAgua = A0;
/** Feedback hardware: bloqueo de puerta energizado/enganchado. */
constexpr int pinSensorBloqueoPuerta = A3;
int val1 = 8;     // VALVULA DE ENTRADA DE AGUA
int giro = 5;     // GIRO DEL MOTOR
int vel1 = 6;     // VELOCIDAD DE MOTOR
int vel2 = 7;     // VELOCIDAD DE MOTOR
int motor = 4;    // ENCENDER MOTOR
int bomba = 9;    // BOMBA DE AGUA
int bloqueo = 10; // BLOQUEO DE PUERTA
int alarma = 2;   // ALARMA BUZZER PARA FIN DE LAVADO
int acelerado = 0;
int jabonera = 3;
int jabPosPreLavado = 750;
int jabPosLavado = 900;
int jabPosSuavizante = 1100;
int jabPosLavandina = 1350;
static int last_jab_servo_angle = -1;

int tamborVacio = 0;
/** Ultima lectura digital de los sensores de feedback (0/1). */
int g_sensor_carga_agua = 0;
int g_sensor_bloqueo_puerta = 0;
int tiempoTotal = 0;
int tiempoStart = 0;
int tiempoEnd = 0;
int faseActual = 0;
int llenadoError = 0;
int programa = 1;

const FaseIndex* fases = nullptr;

static const unsigned long FILL_TIMEOUT_MS = 10UL * 60UL * 1000UL;
static const unsigned long EEPROM_SAVE_INTERVAL_MS = 30000UL;

bool g_recovery_ui_pending = false;
char g_recovery_reason[16] = "";
uint8_t g_last_error_code = 0;
unsigned long g_last_eeprom_save_ms = 0;
unsigned long g_fill_phase_start_ms = 0;
bool g_fill_has_seen_full = false;
int g_prev_fase_for_fill = -1;
/** Ultimo tamborVacio en LLENADO_PRE_LAVADO / LLENADO_LAVADO (-1 = sin muestra). Flanco agua->vacio reabre llenado. */
static int g_ll_fillwash_prev_tambor = -1;

struct ErrorRingEntry {
  unsigned long t_ms;
  uint8_t code;
  uint8_t fase;
};
static ErrorRingEntry g_err_ring[3];
static uint8_t g_err_ring_pos = 0;
/** Incrementa al recibir una linea no vacia que empieza por '>' (comandos desde ESP/PC). */
static uint8_t g_uart_cmd_rx_count = 0;
/** Bytes leidos de espSerial (cada read()). */
static uint32_t g_esp_soft_rx_bytes = 0;
/** Lineas completas (\\n) por espSerial cuyo texto tras espacios empieza por "S|" (telemetria; no son comandos '>'). */
static uint16_t g_esp_s_pipe_rx_lines = 0;

void logMessage(const char* msg);
bool startLavadora(const char* programa);
void errorbuzzerPWM(void);
void setProgramaLargo(void);
void setProgramaCorto(void);
void setProgramaVaciado(void);
void setProgramaCorto2(void);
void setProgramaCentrifugar(void);
void setProgramaCargaAgua(void);
void setProgramaSoloLavado(void);
void setProgramaMini(void);
void calcTiempoTotal(void);
static void processCommandLine(const char* line);
static void processCommandLineFromEsp(const char* line);
static void processCommandLineFromPc(const char* line);

static void push_error_ring(uint8_t code, uint8_t fase) {
  g_err_ring[g_err_ring_pos].t_ms = millis();
  g_err_ring[g_err_ring_pos].code = code;
  g_err_ring[g_err_ring_pos].fase = fase;
  g_err_ring_pos = (uint8_t)((g_err_ring_pos + 1) % 3);
}

static bool fase_es_llenado(uint8_t fn) {
  return fn == LLENADO_PRE_LAVADO || fn == LLENADO_LAVADO || fn == LLENADO_SUAVIZANTE || fn == LLENADO;
}

static void reset_fill_watch_for_phase_change(void) {
  g_fill_phase_start_ms = millis();
  g_fill_has_seen_full = false;
  g_ll_fillwash_prev_tambor = -1;
}

static void save_checkpoint_runtime(uint8_t rec_state, uint8_t err_code) {
  WashCheckpoint cp;
  memset(&cp, 0, sizeof(cp));
  cp.recovery_state = rec_state;
  cp.programa = (uint8_t)programa;
  cp.totalFases_cp = (uint8_t)totalFases;
  cp.faseActual = (uint8_t)faseActual;
  if (minuto < 0) minuto = 0;
  if (minuto > 255) minuto = 255;
  if (segundos < 0) segundos = 0;
  if (segundos > 255) segundos = 255;
  if (paso < 0) paso = 0;
  if (paso >= LAVADO_PASOS) paso = 0;
  if (paso > 255) paso = 255;
  cp.minuto = (uint8_t)minuto;
  cp.segundos = (uint8_t)segundos;
  cp.paso = (uint8_t)paso;
  cp.acelerado = (uint8_t)(acelerado ? 1 : 0);
  cp.sttone = (uint8_t)(sttone ? 1 : 0);
  if (tiempoTranscurrido < 0) tiempoTranscurrido = 0;
  if (tiempoTranscurrido > 65535) tiempoTranscurrido = 65535;
  cp.tiempoTranscurrido = (uint16_t)tiempoTranscurrido;
  cp.last_error_code = err_code;
  cp.tambor_saved = (uint8_t)(tamborVacio ? 1 : 0);
  checkpoint_write(&cp);
  g_last_eeprom_save_ms = millis();
}

static void maybe_throttle_save_running(void) {
  if (!encendida || fases == nullptr || totalFases <= 0)
    return;
  unsigned long now = millis();
  if (now - g_last_eeprom_save_ms < EEPROM_SAVE_INTERVAL_MS)
    return;
  save_checkpoint_runtime(REC_RUNNING, ERR_NONE);
}

static uint8_t total_fases_for_programa_id(int p) {
  switch (p) {
    case 1: return (uint8_t)LAV_FASES_LARGO;
    case 2: return (uint8_t)LAV_FASES_CORTO;
    case 3: return (uint8_t)LAV_FASES_VACIADO;
    case 4: return (uint8_t)LAV_FASES_CORTO2;
    case 5: return (uint8_t)LAV_FASES_CENTRIF;
    case 6: return (uint8_t)LAV_FASES_CARGA_AGUA;
    case 7: return (uint8_t)LAV_FASES_SOLO_LAVADO;
    case 8: return (uint8_t)LAV_FASES_MINI;
    default: return 0;
  }
}

static bool checkpoint_matches_program(const WashCheckpoint* cp) {
  return cp->totalFases_cp == total_fases_for_programa_id(cp->programa);
}

static void send_boot_recovery_json(const WashCheckpoint* cp) {
  uint8_t rcode = (cp->recovery_state == REC_ERROR) ? 1u : 2u;
  char buf[96];
  snprintf(buf, sizeof(buf), "R|%u|%u|%u|%u|%u|%u|%u|%u|%u\n",
           (unsigned)cp->recovery_state, (unsigned)rcode, (unsigned)cp->programa,
           (unsigned)cp->faseActual, (unsigned)cp->totalFases_cp, (unsigned)cp->minuto,
           (unsigned)cp->segundos, (unsigned)cp->last_error_code, (unsigned)cp->tambor_saved);
  Serial.println(buf);
  espSerial.println(buf);
}

static void trigger_error_checkpoint(uint8_t err_code, const char* json_line) {
  if (llenadoError)
    return;
  save_checkpoint_runtime(REC_ERROR, err_code);
  g_last_error_code = err_code;
  push_error_ring(err_code, (uint8_t)faseActual);
  llenadoError = 1;
  encendida = false;
  hasError = true;
  paso = 0;
  paso_seg = 0;
  errorbuzzerPWM();
  logMessage(json_line);
}

void discard_recovery_state(void) {
  checkpoint_clear();
  g_recovery_ui_pending = false;
  g_recovery_reason[0] = '\0';
  llenadoError = 0;
  g_last_error_code = 0;
  g_prev_fase_for_fill = -1;
  motor_reset_service_state();
}

static bool restore_from_checkpoint(const WashCheckpoint* cp, bool ack_centrifuge) {
  if (!checkpoint_matches_program(cp))
    return false;
  if (cp->faseActual >= cp->totalFases_cp)
    return false;

  if (cp->programa == 1)
    setProgramaLargo();
  else if (cp->programa == 2)
    setProgramaCorto();
  else if (cp->programa == 3)
    setProgramaVaciado();
  else if (cp->programa == 4)
    setProgramaCorto2();
  else if (cp->programa == 5)
    setProgramaCentrifugar();
  else if (cp->programa == 6)
    setProgramaCargaAgua();
  else if (cp->programa == 7)
    setProgramaSoloLavado();
  else if (cp->programa == 8)
    setProgramaMini();
  else
    return false;

  calcTiempoTotal();

  FaseIndex f0 = fases[cp->faseActual];
  if (f0.funcion == CENTRIFUGAR && !ack_centrifuge)
    return false;

  faseActual = cp->faseActual;
  minuto = cp->minuto;
  segundos = cp->segundos;
  paso = cp->paso;
  if (paso < 0 || paso >= LAVADO_PASOS)
    paso = 0;
  paso_seg = 0;
  acelerado = cp->acelerado ? 1 : 0;
  sttone = cp->sttone ? 1 : 0;
  tiempoTranscurrido = (int)cp->tiempoTranscurrido;
  llenadoError = 0;
  hora = millis();
  ultimoSegundoLavadora = -1;
  ultimoSegundoEnviado = -1;

  g_prev_fase_for_fill = -1;
  g_recovery_ui_pending = false;
  g_recovery_reason[0] = '\0';
  g_last_error_code = 0;
  motor_reset_service_state();

  encendida = true;
  save_checkpoint_runtime(REC_RUNNING, ERR_NONE);
  setJabonera();
  return true;
}

static uint8_t nombre_fase_actual_code(void) {
  if (!encendida || fases == nullptr || faseActual < 0 || faseActual >= totalFases)
    return 0;
  switch (fases[faseActual].funcion) {
    case LLENADO_PRE_LAVADO: return 1;
    case LLENADO_LAVADO: return 2;
    case LLENADO_SUAVIZANTE: return 3;
    case LLENADO: return 4;
    case LAVADO: return 5;
    case VACIADO: return 6;
    case CENTRIFUGAR: return 7;
    case ESPERA: return 8;
    default: return 9;
  }
}

static uint8_t recovery_reason_wire_code(void) {
  if (g_recovery_reason[0] == '\0')
    return 0;
  if (!strncmp(g_recovery_reason, "error", 5))
    return 1;
  return 2;
}

// CONFIGURACION DE PINES
void setup()
{

  Serial.begin(9600);
  espSerial.begin(9600);
  delay(50);
  while (espSerial.available())
    (void)espSerial.read();

  // CONFIGURAMOS LOS PINES DE SALIDA NECESARIOS PARA NUESTRA LAVADORA
  pinMode(presostato, INPUT);
  pinMode(pinSensorCargaAgua, INPUT_PULLUP);
  pinMode(pinSensorBloqueoPuerta, INPUT_PULLUP);
  pinMode(val1, OUTPUT);
  pinMode(giro, OUTPUT);
  pinMode(vel1, OUTPUT);
  pinMode(vel2, OUTPUT);
  pinMode(motor, OUTPUT);
  pinMode(bomba, OUTPUT);
  pinMode(bloqueo, OUTPUT);
  pinMode(alarma, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT); // LED INDICATIVO DE TRANCURSO DEL TIEMPO

  // CONFIGURACION INICIAL DE LOS PINES EN ALTO, YA QUE LOS RELES ENCIENDEN CUANDO PONEMOS EN BAJO EL PIN
  // CONFIGURAMOIS EN ALTO LOS PINES PARA QUE LOS RELES ESTEN APAGADOS AL INICIO DEL LOOP
  digitalWrite(val1, HIGH);
  digitalWrite(giro, HIGH);
  digitalWrite(vel1, HIGH);
  digitalWrite(vel2, HIGH);
  digitalWrite(motor, HIGH);
  digitalWrite(bomba, HIGH);
  digitalWrite(bloqueo, HIGH); // BLOQUEO DE PUERTA
#if USE_JABONERA_SERVO
  /* ServoTimer2 (Timer2) suele irregular con NeoSWSerial y otras ISRs; Servo.h
     (Timer1) da pulsos mas estables con un solo servo en pin digital. */
  jabservo.attach(jabonera, 750, 2250);
  delay(50);
  jabservo.writeMicroseconds(jabPosPreLavado);
  last_jab_servo_angle = jabPosPreLavado;
#else
  pinMode(jabonera, INPUT); /* sin servo: pin 3 libre (no PWM al actuador) */
#endif
  powerOnbuzzerPWM();

  wdt_disable();
  delay(10);
  wdt_enable(WDTO_8S);

  WashCheckpoint cpBoot;
  if (checkpoint_read(&cpBoot) && (cpBoot.recovery_state == REC_RUNNING || cpBoot.recovery_state == REC_ERROR)) {
    g_recovery_ui_pending = true;
    if (cpBoot.recovery_state == REC_ERROR) {
      strncpy(g_recovery_reason, "error", sizeof(g_recovery_reason) - 1);
      g_recovery_reason[sizeof(g_recovery_reason) - 1] = '\0';
      g_last_error_code = cpBoot.last_error_code;
    } else {
      strncpy(g_recovery_reason, "power_loss", sizeof(g_recovery_reason) - 1);
      g_recovery_reason[sizeof(g_recovery_reason) - 1] = '\0';
    }
    send_boot_recovery_json(&cpBoot);
  }

  logMessage("SETUP END");
}

void buzzerPWM(int pin, int freq, int duration) {
  int period = 1000000 / freq; // periodo en microsegundos
  int halfPeriod = period / 2;
  long cycles = ((long)duration * 1000L) / period;

  for (long i = 0; i < cycles; i++) {
    digitalWrite(pin, HIGH);
    delayMicroseconds(halfPeriod);
    digitalWrite(pin, LOW);
    delayMicroseconds(halfPeriod);
  }
}

void nobuzzerPWM(int pin) {
  digitalWrite(pin, LOW); // Asegura que el pin esté en bajo
}

void calcTiempoTotal()
{
  int length = 0;
  switch (programa) {
    case 1: length = LAV_FASES_LARGO; break;
    case 2: length = LAV_FASES_CORTO; break;
    case 3: length = LAV_FASES_VACIADO; break;
    case 4: length = LAV_FASES_CORTO2; break;
    case 5: length = LAV_FASES_CENTRIF; break;
    case 6: length = LAV_FASES_CARGA_AGUA; break;
    case 7: length = LAV_FASES_SOLO_LAVADO; break;
    case 8: length = LAV_FASES_MINI; break;
  }

  tiempoTotal = 0;
  for (int i = 0; i < length; i++) {
    tiempoTotal += fases[i].tiempo;
  }
}

// FUNCION DE LLENADO
void llenado()
{
  if (tamborVacio == 1)
  {
    digitalWrite(bomba, HIGH); // APAGAMOS LA BOMBA DE DESAGOTE
    digitalWrite(val1, LOW);   // ENCENDEMOS LA VALVULA PARA QUE PUEDA ENTRAR AGUA
  }
  else
  {
    digitalWrite(val1, HIGH); // ENCENDEMOS LA VALVULA PARA QUE PUEDA ENTRAR AGUA
    digitalWrite(bomba, HIGH);
  }
}

void apagar()
{
  motor_reset_service_state();

  digitalWrite(val1, HIGH);
  digitalWrite(giro, HIGH);
  digitalWrite(vel1, HIGH);
  digitalWrite(vel2, HIGH);
  digitalWrite(bomba, HIGH);
  digitalWrite(motor, HIGH);
  digitalWrite(bloqueo, HIGH);
}

void bloqueoPuerta(){
  digitalWrite(bloqueo, HIGH);
}

void buzzerEnd()
{
  wdt_disable();
  buzzerPWM(alarma, 880, 100);
  delay(600);
  nobuzzerPWM(alarma);
  buzzerPWM(alarma, 880, 100);
  delay(600);
  nobuzzerPWM(alarma);
  buzzerPWM(alarma, 880, 100);
  delay(600);
  nobuzzerPWM(alarma);
  buzzerPWM(alarma, 880, 100);
  delay(600);
  nobuzzerPWM(alarma);
  buzzerPWM(alarma, 880, 100);
  nobuzzerPWM(alarma);
  wdt_enable(WDTO_8S);
}

// TONO INICIO
void startbuzzerPWM()
{
  wdt_disable();
  buzzerPWM(alarma, 880, 200);
  delay(500);
  buzzerPWM(alarma, 1000, 200);
  delay(500);
  nobuzzerPWM(alarma);
  wdt_enable(WDTO_8S);
}

void powerOnbuzzerPWM()
{
  buzzerPWM(alarma, 880, 200);
  /*delay(1000);
  nobuzzerPWM(alarma);*/
}

void errorbuzzerPWM()
{
  wdt_disable();
  buzzerPWM(alarma, 440, 500);
  delay(1000);
  buzzerPWM(alarma, 440, 500);
  delay(1000);
  nobuzzerPWM(alarma);
  wdt_enable(WDTO_8S);
}

/** Subcadena "OLAF" sin distinguir mayusculas (p. ej. trama corrupta tipo "OLAF Q ASHE"). */
static bool uart_line_has_olaf_ci(const char* s)
{
  for (; *s; ++s) {
    const char* p = s;
    static const char olaf[] = "OLAF";
    unsigned i;
    for (i = 0; i < sizeof(olaf) - 1u && *p; ++i, ++p) {
      char c = *p;
      if (c >= 'a' && c <= 'z')
        c = (char)(c - ('a' - 'A'));
      if (c != olaf[i])
        break;
    }
    if (i == sizeof(olaf) - 1u)
      return true;
  }
  return false;
}

static void uart_buzzer_ping_ack(void)
{
  buzzerPWM(alarma, 1568, 90);
  nobuzzerPWM(alarma);
}

static void uart_buzzer_olaf_siniestro(void)
{
  wdt_disable();
  buzzerPWM(alarma, 196, 550);
  delay(100);
  buzzerPWM(alarma, 147, 700);
  delay(120);
  buzzerPWM(alarma, 98, 900);
  nobuzzerPWM(alarma);
  wdt_enable(WDTO_8S);
}

void loopTimer()
{
  if (millis() - hora >= intervalo)
  {
    hora = millis();
    digitalWrite(LED_BUILTIN, led);
    led = !led;

    if (!encendida)
      return;

    segundos = segundos + 1;

    if (segundos == 60)
    {
      minuto = minuto + 1;
      segundos = 0;
      tiempoTranscurrido++; 
    }

    if (paso < 0 || paso >= LAVADO_PASOS)
      paso = 0;

    paso_seg++;
    if (paso_seg >= LAVADO_PASO_DUR_SEC[paso]) {
      paso_seg = 0;
      paso++;
      if (paso >= LAVADO_PASOS)
        paso = 0;
    }
  }
}

void calibrarJabonera(int value)
{
#if USE_JABONERA_SERVO
  jabservo.writeMicroseconds(value);
  last_jab_servo_angle = value;
#else
  (void)value;
#endif
}

void calibrarJaboneraTest()
{
#if USE_JABONERA_SERVO
  wdt_disable();
  jabservo.writeMicroseconds(jabPosPreLavado);
  last_jab_servo_angle = jabPosPreLavado;
  delay(3000);
  jabservo.writeMicroseconds(jabPosLavado);
  last_jab_servo_angle = jabPosLavado;
  delay(3000);
  jabservo.writeMicroseconds(jabPosSuavizante);
  last_jab_servo_angle = jabPosSuavizante;
  delay(3000);
  jabservo.writeMicroseconds(jabPosLavandina);
  last_jab_servo_angle = jabPosLavandina;
  wdt_enable(WDTO_8S);
#endif
}

void setJabonera()
{
#if USE_JABONERA_SERVO
  if (!encendida || fases == nullptr || faseActual < 0 || faseActual >= totalFases)
    return;

  FaseIndex fase = fases[faseActual];
  int target;
  switch (fase.funcion) {
    case LLENADO_PRE_LAVADO:
      target = jabPosPreLavado;
      break;
    case LLENADO_LAVADO:
      target = jabPosLavado;
      break;
    case LLENADO_SUAVIZANTE:
      target = jabPosSuavizante;
      break;
    case LLENADO:
      target = jabPosPreLavado;
      break;
    default:
      return;
  }

  if (target != last_jab_servo_angle) {
    jabservo.writeMicroseconds(target);
    last_jab_servo_angle = target;
  }
#endif
}

void loop()
{
  processCommand();

  tamborVacio = digitalRead(presostato);
  g_sensor_carga_agua = digitalRead(pinSensorCargaAgua) ? 1 : 0;
  g_sensor_bloqueo_puerta = digitalRead(pinSensorBloqueoPuerta) ? 1 : 0;

  loopTimer();

  if (encendida)
  {
    if (segundos != ultimoSegundoLavadora)
    {
      loopLavadora();
      ultimoSegundoLavadora = segundos;
    }
    motor_refresh_outputs();
    maybe_throttle_save_running();
  }

  if (segundos % 5 == 0 && segundos != ultimoSegundoEnviado)
  {
    serialSendStatus();
    ultimoSegundoEnviado = segundos;
  }

  wdt_reset();
}

void logMessage(const char* msg) {
  Serial.println(msg);
  espSerial.println(msg);
}

void serialSendStatus()
{
  int tiempoRestante = tiempoTotal - tiempoTranscurrido;
  char buf[220];
  snprintf(buf, sizeof(buf),
           "S|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%u|%d|%u|%u|%u|%u|%u|%u|%lu|%d|%d\n",
           encendida ? 1 : 0, faseActual, totalFases, tamborVacio, minuto, segundos, paso,
           tiempoTotal, tiempoRestante, programa, (unsigned)nombre_fase_actual_code(),
           g_recovery_ui_pending ? 1 : 0, (unsigned)recovery_reason_wire_code(),
           (unsigned)g_last_error_code, (unsigned)g_err_ring[0].code,
           (unsigned)g_err_ring[1].code, (unsigned)g_err_ring[2].code,
           (unsigned)g_uart_cmd_rx_count, (unsigned long)g_esp_soft_rx_bytes,
           g_sensor_carga_agua, g_sensor_bloqueo_puerta);
  Serial.println(buf);
  espSerial.println(buf);
#if DEBUG_UART_USB_LINES
  Serial.print(F("[diag] esp bytes="));
  Serial.print((unsigned long)g_esp_soft_rx_bytes);
  Serial.print(F(" S|lines="));
  Serial.print((unsigned)g_esp_s_pipe_rx_lines);
  Serial.print(F(" cmd>'="));
  Serial.println((unsigned)g_uart_cmd_rx_count);
#endif
}

void loopLavadora()
{

  if (sttone == 0)
  {
    startbuzzerPWM();
    bloqueoPuerta();
    sttone = 1;
  }

  if (llenadoError)
  {
    apagar();
    return;
  }

  if (faseActual >= totalFases) {
    logMessage("FINAL");
    encendida = false;
    paso = 0;
    paso_seg = 0;
    checkpoint_clear();
    g_recovery_ui_pending = false;
    g_recovery_reason[0] = '\0';
    buzzerEnd();
    apagar();
    return;
  }

  if (faseActual != g_prev_fase_for_fill) {
    g_prev_fase_for_fill = faseActual;
    reset_fill_watch_for_phase_change();
  }

  FaseIndex fase = fases[faseActual];
  setJabonera();

  if (fase_es_llenado(fase.funcion) && !g_fill_has_seen_full) {
    if (millis() - g_fill_phase_start_ms > FILL_TIMEOUT_MS) {
      trigger_error_checkpoint(ERR_FILL_TIMEOUT, "!E|Timeout llenado");
      return;
    }
  }

  switch (fase.funcion) {
    case LLENADO_LAVADO:
      acelerado = 0;
      if (tamborVacio == 0) {
        g_fill_has_seen_full = true;
      } else if (g_fill_has_seen_full && g_ll_fillwash_prev_tambor == 0) {
        /* Se perdio el agua durante lavado en esta fase: nuevo llenado, parar ciclo de motor. */
        g_fill_has_seen_full = false;
        g_fill_phase_start_ms = millis();
        paso = 0;
        paso_seg = 0;
        motor_reset_service_state();
        logMessage("REFILL LLLAV");
      }
      g_ll_fillwash_prev_tambor = tamborVacio;
      break;
    case LLENADO_PRE_LAVADO:
      acelerado = 0;
      if (tamborVacio == 0) {
        g_fill_has_seen_full = true;
        minuto = minuto + 1;
        segundos = 0;
        logMessage("AVANCE TAMBOR LLENO");
      } else if (g_fill_has_seen_full && g_ll_fillwash_prev_tambor == 0) {
        g_fill_has_seen_full = false;
        g_fill_phase_start_ms = millis();
        paso = 0;
        paso_seg = 0;
        motor_reset_service_state();
        logMessage("REFILL LLPRL");
      }
      g_ll_fillwash_prev_tambor = tamborVacio;
      break;
    case LLENADO:
    case LLENADO_SUAVIZANTE:
      acelerado = 0;
      if (tamborVacio == 0)
      {
        g_fill_has_seen_full = true;
        minuto = minuto + 1;
        segundos = 0;
        logMessage("AVANCE TAMBOR LLENO");
      }
      break;
    case LAVADO:
      acelerado = 0;
      break;
    case VACIADO:
      acelerado = 0;
      break;
    case CENTRIFUGAR:
      break;
    case ESPERA:
      break;
    default:
     Serial.println("FUNCION NO RECONOCIDA");
  }

  if (minuto >= fase.tiempo) {
    save_checkpoint_runtime(REC_RUNNING, ERR_NONE);
    minuto = 0;
    faseActual++;
    paso = 0;
    paso_seg = 0;
    segundos = 0;
    if (faseActual < totalFases)
      setJabonera();
  }
}

/* Comandos UART + \n caben en ~100 B; dos buffers pequeños para no saturar RAM del UNO (2 KiB). */
static const size_t UART_CMD_CAP = 112;
static char s_uart_line_esp[UART_CMD_CAP];
static char s_uart_line_pc[UART_CMD_CAP];
static size_t s_uart_len_esp;
static size_t s_uart_len_pc;

static void uart_drain_stream(Stream& s, char* acc, size_t& acc_len, size_t acc_cap,
                               void (*on_line)(const char*), size_t max_read, uint32_t* rx_byte_count) {
  size_t nread = 0;
  while (s.available() && nread < max_read) {
    int r = s.read();
    if (r < 0)
      break;
    nread++;
    if (rx_byte_count != nullptr)
      (*rx_byte_count)++;
    unsigned char uc = (unsigned char)r;
    if (uc == '\r')
      continue;
    if (uc == '\n') {
      if (acc_len < acc_cap)
        acc[acc_len] = '\0';
      else
        acc[acc_cap - 1] = '\0';
      if (acc_len > 0)
        on_line(acc);
      acc_len = 0;
      continue;
    }
    if (acc_len >= acc_cap - 1) {
      acc_len = 0;
      break;
    }
    acc[acc_len++] = (char)uc;
  }
}

void processCommand()
{
  /* Mismo espSerial que en rama wifi (JSON). Drenar con chunk grande y varias pasadas para no
   * perder bytes por buffer interno pequeño de NeoSWSerial (UART corrupto si se llena). */
  const size_t chunk = 255;
  for (uint8_t pass = 0; pass < 6; ++pass) {
    uart_drain_stream(espSerial, s_uart_line_esp, s_uart_len_esp, UART_CMD_CAP, processCommandLineFromEsp, chunk,
                      &g_esp_soft_rx_bytes);
    uart_drain_stream(Serial, s_uart_line_pc, s_uart_len_pc, UART_CMD_CAP, processCommandLineFromPc, chunk, nullptr);
  }
}

/** Tras el cuerpo fijo del comando, solo ASCII <= 32 (espacio, tab, CR, LF). */
static bool uart_cmd_tail_ws_only(const char* t)
{
  for (; *t; ++t) {
    unsigned char c = (unsigned char)*t;
    if (c > 32u)
      return false;
  }
  return true;
}

/** ">PING" con PING en mayusculas o minusculas. */
static bool uart_cmd_is_ping_ci(const char* p)
{
  if (p[0] != '>')
    return false;
  static const char ref[] = "PING";
  for (uint8_t i = 0; i < 4; i++) {
    char c = p[1 + i];
    if (c >= 'a' && c <= 'z')
      c = (char)(c - ('a' - 'A'));
    if (c != ref[i])
      return false;
  }
  return true;
}

static void processCommandLine(const char* line)
{
  static char buf[UART_CMD_CAP];
  strncpy(buf, line, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  size_t n = strlen(buf);
  while (n > 0 && (unsigned char)buf[n - 1] <= 32)
    buf[--n] = '\0';
  char* p = buf;
  while (*p == ' ' || *p == '\t')
    p++;
  if (*p == '\0')
    return;
  {
    char* gt = strchr(p, '>');
    if (gt != nullptr)
      p = gt;
  }
  if (p[0] != '>')
    return;
  g_uart_cmd_rx_count++;

  if (!strncmp(p, ">STOP", 5) && uart_cmd_tail_ws_only(p + 5)) {
    stopLavadora();
    logMessage("OK STOP");
    return;
  }
  if (!strncmp(p, ">DISCARD", 8) && uart_cmd_tail_ws_only(p + 8)) {
    discard_recovery_state();
    logMessage("OK DISCARD");
    return;
  }
  if (!strncmp(p, ">RESUME,", 8) && (p[8] == '0' || p[8] == '1') && uart_cmd_tail_ws_only(p + 9)) {
    bool ackCent = (p[8] == '1');
    WashCheckpoint cp;
    if (!checkpoint_read(&cp)) {
      logMessage("!E|no_checkpoint");
      return;
    }
    if (cp.recovery_state != REC_RUNNING && cp.recovery_state != REC_ERROR) {
      logMessage("!E|no_recovery");
      return;
    }
    if (!restore_from_checkpoint(&cp, ackCent)) {
      logMessage("!E|resume_fail");
      return;
    }
    logMessage("OK RESUME");
    return;
  }
  if (!strncmp(p, ">START,", 7) && p[7] != '\0' && uart_cmd_tail_ws_only(p + 8)) {
    const char* prog = nullptr;
    switch (p[7]) {
      case 'L': prog = "largo"; break;
      case 'C': prog = "corto"; break;
      case '2': prog = "corto2"; break;
      case 'V': prog = "vaciado"; break;
      case 'X': prog = "centrifugar"; break;
      case 'A': prog = "carga_agua"; break;
      case 'W': prog = "solo_lavado"; break;
      case 'M': prog = "mini"; break;
      default:
        logMessage("!E|badprog");
        return;
    }
    if (g_recovery_ui_pending)
      discard_recovery_state();
    if (startLavadora(prog))
      logMessage("OK START");
    else
      logMessage("!E|start_failed");
    return;
  }
  if (!strncmp(p, ">JABON", 6) && uart_cmd_tail_ws_only(p + 6)) {
    calibrarJaboneraTest();
    return;
  }
  if (!strncmp(p, ">J1", 3) && uart_cmd_tail_ws_only(p + 3)) {
    calibrarJabonera(jabPosPreLavado);
    return;
  }
  if (!strncmp(p, ">J2", 3) && uart_cmd_tail_ws_only(p + 3)) {
    calibrarJabonera(jabPosLavado);
    return;
  }
  if (!strncmp(p, ">J3", 3) && uart_cmd_tail_ws_only(p + 3)) {
    calibrarJabonera(jabPosSuavizante);
    return;
  }
  if (uart_cmd_is_ping_ci(p) && uart_cmd_tail_ws_only(p + 5)) {
    logMessage("OK PONG");
    uart_buzzer_ping_ack();
    return;
  }
  logMessage("!E|unknown");
}

static const char* skip_uart_ws(const char* s)
{
  while (*s == ' ' || *s == '\t')
    s++;
  return s;
}

static void processCommandLineFromEsp(const char* line)
{
  const char* q = skip_uart_ws(line);
  if (q[0] == 'S' && q[1] == '|') {
    if (g_esp_s_pipe_rx_lines != 0xFFFFu)
      g_esp_s_pipe_rx_lines++;
  }
#if DEBUG_UART_USB_LINES
  Serial.print(F("[ESP RX] "));
  Serial.println(line);
#endif
  processCommandLine(line);
  if (uart_line_has_olaf_ci(line))
    uart_buzzer_olaf_siniestro();
}

static void processCommandLineFromPc(const char* line)
{
#if DEBUG_UART_USB_LINES
  Serial.print(F("[USB RX] "));
  Serial.println(line);
#endif
  processCommandLine(line);
}

bool startLavadora(const char* programa)
{
  if (g_recovery_ui_pending) {
    logMessage("!E|recovery_pending");
    return false;
  }

  if (strcmp(programa, "corto") == 0)
  {
    setProgramaCorto();
  }else if (strcmp(programa, "corto2") == 0)
  {
    setProgramaCorto2();
  }
  else if (strcmp(programa, "vaciado") == 0)
  {
    setProgramaVaciado();
  }
  else if (strcmp(programa, "centrifugar") == 0)
  {
    setProgramaCentrifugar();
  }
  else if (strcmp(programa, "carga_agua") == 0)
  {
    setProgramaCargaAgua();
  }
  else if (strcmp(programa, "solo_lavado") == 0)
  {
    setProgramaSoloLavado();
  }
  else if (strcmp(programa, "mini") == 0)
  {
    setProgramaMini();
  }
  else
  {
    setProgramaLargo();
  }

  calcTiempoTotal();
  resetTimer();
  sttone = 0;
  encendida = 1;
  g_prev_fase_for_fill = -1;
  save_checkpoint_runtime(REC_RUNNING, ERR_NONE);
  setJabonera();
  return true;
}

void stopLavadora()
{
  encendida = 0;
  paso = 0;
  paso_seg = 0;
  checkpoint_clear();
  g_recovery_ui_pending = false;
  g_recovery_reason[0] = '\0';
  apagar();
}

void resetTimer()
{

  paso_seg = 0;
  segundos = 0;
  minuto = 0;
  hora = 0;
  paso = 0;
  tiempoTranscurrido = 0;
  faseActual = 0;
  g_prev_fase_for_fill = -1;
  motor_reset_service_state();
}

void setProgramaLargo()
{
   programa = 1;
   fases = programaLargo;
   totalFases = LAV_FASES_LARGO;
}

void setProgramaCorto()
{
   programa = 2;
   fases = programaCorto;
   totalFases = LAV_FASES_CORTO;
}

void setProgramaVaciado()
{
  programa = 3;
  fases = programaVaciado;
  totalFases = LAV_FASES_VACIADO;
}

void setProgramaCorto2()
{
  programa = 4;
  fases = programaCorto2;
  totalFases = LAV_FASES_CORTO2;
}

void setProgramaCentrifugar()
{
  programa = 5;
  fases = programaCentrifugar;
  totalFases = LAV_FASES_CENTRIF;
}

void setProgramaCargaAgua()
{
  programa = 6;
  fases = programaCargaAgua;
  totalFases = LAV_FASES_CARGA_AGUA;
}

void setProgramaSoloLavado()
{
  programa = 7;
  fases = programaSoloLavado;
  totalFases = LAV_FASES_SOLO_LAVADO;
}

void setProgramaMini()
{
  programa = 8;
  fases = programaMini;
  totalFases = LAV_FASES_MINI;
}
