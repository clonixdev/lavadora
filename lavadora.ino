#include <Servo.h>
#include <ArduinoJson.h>
#include <NeoSWSerial.h>
#include <string.h>
#include <avr/wdt.h>
#include "recovery_eeprom.h"
#include "programas_lavadora.h"
#include "motor_service.h"
					
NeoSWSerial espSerial(15, 16); //RX TX
bool led = true;
bool encendida = false;
bool hasError = false;
unsigned long hora = 0;
const int intervalo = 1000;
int contador = 0;
int segundos = 0;
int minuto = 0;
int paso = 0;   // REGISTRO DE PASO PARA EL LAVADO Y EL CICLO DE ACELERACION DEL TANQUE
int sttone = 0; // TONO INICIAL
Servo jabservo;
int totalFases = 0;
int tiempoTranscurrido = 0; 
int ultimoSegundoEnviado = -1;
int ultimoSegundoLavadora = -1;
int presostato = 17;
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

struct ErrorRingEntry {
  unsigned long t_ms;
  uint8_t code;
  uint8_t fase;
};
static ErrorRingEntry g_err_ring[3];
static uint8_t g_err_ring_pos = 0;

void logMessage(String msg);
bool startLavadora(const char* programa);
void errorbuzzerPWM(void);
void setProgramaLargo(void);
void setProgramaCorto(void);
void setProgramaVaciado(void);
void setProgramaCorto2(void);
void setProgramaCentrifugar(void);
void calcTiempoTotal(void);

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
    default: return 0;
  }
}

static bool checkpoint_matches_program(const WashCheckpoint* cp) {
  return cp->totalFases_cp == total_fases_for_programa_id(cp->programa);
}

static void send_boot_recovery_json(const WashCheckpoint* cp) {
  JsonDocument doc;
  doc["recovery_pending"] = true;
  doc["reason"] = (cp->recovery_state == REC_ERROR) ? "error" : "power_loss";
  doc["programa"] = cp->programa;
  doc["faseActual"] = cp->faseActual;
  doc["totalFases"] = cp->totalFases_cp;
  doc["minuto"] = cp->minuto;
  doc["segundo"] = cp->segundos;
  doc["last_error_code"] = cp->last_error_code;
  doc["tambor_saved"] = cp->tambor_saved;
  String out;
  serializeJson(doc, out);
  logMessage(out);
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
  acelerado = cp->acelerado ? 1 : 0;
  sttone = cp->sttone ? 1 : 0;
  tiempoTranscurrido = (int)cp->tiempoTranscurrido;
  llenadoError = 0;
  hora = millis();
  contador = 0;
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

static const char* nombre_fase_actual_json(void) {
  if (!encendida || fases == nullptr || faseActual < 0 || faseActual >= totalFases)
    return "idle";
  switch (fases[faseActual].funcion) {
    case LLENADO_PRE_LAVADO: return "llenado_pre";
    case LLENADO_LAVADO: return "llenado_lavado";
    case LLENADO_SUAVIZANTE: return "llenado_suav";
    case LLENADO: return "llenado";
    case LAVADO: return "lavado";
    case VACIADO: return "vaciado";
    case CENTRIFUGAR: return "centrifugar";
    case ESPERA: return "espera";
    default: return "desconocido";
  }
}

// CONFIGURACION DE PINES
void setup()
{

  Serial.begin(9600);
  espSerial.begin(9600);
   
  // CONFIGURAMOS LOS PINES DE SALIDA NECESARIOS PARA NUESTRA LAVADORA
  pinMode(presostato, INPUT);
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
  /* ServoTimer2 (Timer2) suele irregular con NeoSWSerial y otras ISRs; Servo.h
     (Timer1) da pulsos mas estables con un solo servo en pin digital. */
  jabservo.attach(jabonera, 750, 2250);
  delay(50);
  jabservo.writeMicroseconds(jabPosPreLavado);
  last_jab_servo_angle = jabPosPreLavado;
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

void loopTimer()
{
  if (millis() - hora >= intervalo)
  {
    hora = millis();
    digitalWrite(LED_BUILTIN, led);
    led = !led;
    contador = contador + 1;
    segundos = segundos + 1;

    if (segundos == 60)
    {
      minuto = minuto + 1;
      segundos = 0;
      tiempoTranscurrido++; 
    }
    if (contador == 3)
    {
      contador = 0;
      paso = paso + 1;
    }

    if (paso > 20)
    {
      paso = 0;
    }
  }
}

void calibrarJabonera(int value)
{
  jabservo.writeMicroseconds(value);
  last_jab_servo_angle = value;
}

void calibrarJaboneraTest()
{
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
}

void setJabonera()
{
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
}

void loop()
{
  processCommand();

  tamborVacio = digitalRead(presostato);

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

void logMessage(String msg) {
  Serial.println(msg);
  espSerial.println(msg);
}

void serialSendStatus()
{
	JsonDocument doc;
	doc["Encendida"] = encendida;
  doc["FaseActual"] = faseActual;
  doc["totalFases"] = totalFases;
	doc["TamborVacio"] = tamborVacio;
	doc["Minuto"] = minuto;
	doc["Segundo"] = segundos;
	doc["Paso"] = paso;
	doc["TiempoTotal"] = tiempoTotal;
  int tiempoRestante = tiempoTotal - tiempoTranscurrido;
	doc["TiempoRestante"] = tiempoRestante;
  doc["ProgramaId"] = programa;
  doc["Fase"] = nombre_fase_actual_json();
  doc["recovery_pending"] = g_recovery_ui_pending;
  if (g_recovery_reason[0] != '\0')
    doc["recovery_reason"] = g_recovery_reason;
  doc["last_error_code"] = g_last_error_code;
  doc["ErrRing0"] = g_err_ring[0].code;
  doc["ErrRing1"] = g_err_ring[1].code;
  doc["ErrRing2"] = g_err_ring[2].code;
  
  serializeJson(doc, Serial);
	serializeJson(doc, espSerial);
  Serial.println();
	espSerial.println();
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
      trigger_error_checkpoint(ERR_FILL_TIMEOUT, "{\"error\":\"Timeout de llenado\",\"code\":1}");
      return;
    }
  }

  switch (fase.funcion) {
    case LLENADO_LAVADO:
      acelerado = 0;
      if (tamborVacio == 0)
        g_fill_has_seen_full = true;
      break;
    case LLENADO_PRE_LAVADO:
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
    segundos = 0;
    if (faseActual < totalFases)
      setJabonera();
  }
}

void processCommand()
{
  Stream* input = nullptr;
  if (espSerial.available())
    input = &espSerial;
  else if (Serial.available())
    input = &Serial;
  if (!input)
    return;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, *input);
  
  if (error) {
    logMessage("{\"error\":\"Invalid JSON code 1\"}");
     Serial.println(error.c_str());
	  return;
  }

  
	if (!doc.containsKey("command")) {
	  logMessage("{\"error\":\"Missing 'command' key\"}");
	  return;
	}
  const char* command = doc["command"];
  

  // Comparar el comando recibido
  if (strcmp(command, "start") == 0)
  {


	if (doc.containsKey("programa")) {
       const char* prog = doc["programa"];
       if (startLavadora(prog))
         logMessage("{\"status\":\"ok\",\"command\":\"start\"}");
    }else {
          logMessage("{\"error\":\"Invalid Command Programa no definido\"}");
    return;
    }

    
  }
  else if (strcmp(command, "stop") == 0)
  {
    stopLavadora();
	 logMessage("{\"status\":\"ok\",\"command\":\"stop\"}");
  }
  else if (strcmp(command, "discard_recovery") == 0)
  {
    discard_recovery_state();
    logMessage("{\"status\":\"ok\",\"command\":\"discard_recovery\"}");
  }
  else if (strcmp(command, "resume") == 0)
  {
    bool confirm = doc["confirm"].as<bool>();
    bool ackCent = doc["ack_centrifuge"].as<bool>();
    if (!confirm) {
      logMessage("{\"error\":\"resume requiere confirm:true\"}");
      return;
    }
    WashCheckpoint cp;
    if (!checkpoint_read(&cp)) {
      logMessage("{\"error\":\"Sin checkpoint valido\"}");
      return;
    }
    if (cp.recovery_state != REC_RUNNING && cp.recovery_state != REC_ERROR) {
      logMessage("{\"error\":\"No hay recuperacion pendiente\"}");
      return;
    }
    if (!restore_from_checkpoint(&cp, ackCent)) {
      logMessage("{\"error\":\"No se pudo reanudar; si la fase es centrifugar envie ack_centrifuge:true\"}");
      return;
    }
    logMessage("{\"status\":\"ok\",\"command\":\"resume\"}");
  }
  else if (strcmp(command, "jabon") == 0)
  {
    
    calibrarJaboneraTest();
    return;
    
  }
    else if (strcmp(command, "jabon1") == 0)
  {
    calibrarJabonera(jabPosPreLavado);
    return;
  }
    else if (strcmp(command, "jabon2") == 0)
  {
    calibrarJabonera(jabPosLavado);
    return;
  }
  else if (strcmp(command, "jabon3") == 0)
  {
    calibrarJabonera(jabPosSuavizante);
    return;
  }
  else
  {
    logMessage("{\"error\":\"Unknown command\"}");
  }
}


bool startLavadora(const char* programa)
{
  if (g_recovery_ui_pending) {
    logMessage("{\"error\":\"Hay recuperacion pendiente: use discard_recovery o resume confirm:true\"}");
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
  checkpoint_clear();
  g_recovery_ui_pending = false;
  g_recovery_reason[0] = '\0';
  apagar();
}

void resetTimer()
{

  contador = 0;
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
