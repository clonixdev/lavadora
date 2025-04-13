#include "pitches.h"
#include <ServoTimer2.h>
#include <ArduinoJson.h>
#include <NeoSWSerial.h>
					
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
ServoTimer2 jabservo;

int tiempoTranscurrido = 0; 
int ultimoSegundoEnviado = -1;
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
int jabPosLavado = 1000;
int jabPosSuavizante = 1500;
int jabPosPreLavado = 1800;

int tamborVacio = 0;
int tiempoTotal = 0;
int tiempoStart = 0;
int tiempoEnd = 0;
int faseActual = 0;
int llenadoError = 0;
int programa = 1;

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
  uint8_t tiempo; // en minutos
};

const FaseIndex* fases = nullptr;

// FASES DE LAVADO, FUNCION - TIEMPO en minutos
const FaseIndex programaLargo[] = {
  {LLENADO_PRE_LAVADO, 5}, {LLENADO, 5}, {LAVADO, 8}, {VACIADO, 1},
  {LLENADO_PRE_LAVADO, 5}, {LLENADO, 5}, {LAVADO, 8}, {VACIADO, 1},
  {CENTRIFUGAR, 5}, {LLENADO_LAVADO, 5}, {LLENADO, 5}, {LAVADO, 8},
  {VACIADO, 1}, {LLENADO_SUAVIZANTE, 5}, {LLENADO, 5}, {LAVADO, 8},
  {VACIADO, 1}, {LLENADO_SUAVIZANTE, 5}, {LLENADO, 5}, {LAVADO, 8},
  {VACIADO, 1}, {CENTRIFUGAR, 10}, {ESPERA, 2}, {VACIADO, 1}, {CENTRIFUGAR, 10}
};

const FaseIndex programaCorto[] = {
  {LLENADO_PRE_LAVADO, 5}, {LLENADO, 5}, {LAVADO, 8}, {VACIADO, 1},
  {LLENADO_SUAVIZANTE, 5}, {LLENADO, 5}, {LAVADO, 8}, {VACIADO, 1},
  {LLENADO_SUAVIZANTE, 5}, {LLENADO, 5}, {LAVADO, 8}, {VACIADO, 1},
  {CENTRIFUGAR, 10}, {ESPERA, 2}, {VACIADO, 1}, {CENTRIFUGAR, 10}
};

const FaseIndex programaVaciado[] = {
  {VACIADO, 2},
};

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
  digitalWrite(bloqueo, LOW); // BLOQUEO DE PUERTA
 
  jabservo.attach(jabonera);
  
  powerOnbuzzerPWM();
}

void buzzerPWM(int pin, int freq, int duration) {
  int delayValue = 1000000 / freq / 2; // mitad del ciclo
  int numCycles = freq * duration / 1000;

  for (int i = 0; i < numCycles; i++) {
    digitalWrite(pin, HIGH);
    delayMicroseconds(delayValue);
    digitalWrite(pin, LOW);
    delayMicroseconds(delayValue);
  }
}

void nobuzzerPWM(int pin) {
  digitalWrite(pin, LOW); // Asegura que el pin esté en bajo
}

void calcTiempoTotal()
{
  const FaseIndex* fasesLoc = getPrograma();
  int length = 0;

  switch (programa) {
    case 1: length = sizeof(programaLargo) / sizeof(FaseIndex); break;
    case 2: length = sizeof(programaCorto) / sizeof(FaseIndex); break;
    case 3: length = sizeof(programaVaciado) / sizeof(FaseIndex); break;
  }

  tiempoTotal = 0;
  for (int i = 0; i < length; i++) {
    tiempoTotal += fasesLoc[i].tiempo;
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
void apagarLlenado()
{
  digitalWrite(val1, HIGH); // ENCENDEMOS LA VALVULA PARA QUE PUEDA ENTRAR AGUA
  digitalWrite(bomba, HIGH);
}

// FUNCION DE LAVADO
void lavado()
{

  if (paso == 0)
  {
    digitalWrite(vel1, HIGH);
    digitalWrite(vel2, HIGH);
    digitalWrite(motor, HIGH);
    digitalWrite(bomba, HIGH);
  }
  else if (paso == 1)
  { // PASO DE LAVADO 1  CICLO DE MOTOR APAGADO
    digitalWrite(motor, HIGH);
    digitalWrite(giro, HIGH);
  }
  else if (paso == 2)
  { // PASO DE LAVADO 2  CICLO DE GIRO EN EL SENTIDO CONTRARIO A LAS MANECILLAS DEL RELOJ
    digitalWrite(giro, HIGH);
    digitalWrite(motor, LOW);
  }
  else if (paso == 3)
  { // PASO DE LAVADO 3  CICLO DE MOTOR APAGADO
    digitalWrite(motor, HIGH);
    digitalWrite(giro, LOW);
  }
  else if (paso == 4)
  { // PASO DE LAVADO 4  CICLO DE GIRO EN EL SENTIDO DE LAS MANECILLAS DEL RELOJ
    digitalWrite(giro, LOW);
    digitalWrite(motor, LOW);
  }
  if (paso > 4)
  { // RESETEAR LOS PASOS PARA REPETIR EL CICLO DE LAVADO DURANTE EL TIEMPO ESTIMADO
    paso = 0;
  }
}

// FUNCION DE VACIADO DE TANQUE
void vaciado()
{

  digitalWrite(val1, HIGH); // APAGAMOS FUNCIONES QUE NO NECESITAMOS
  digitalWrite(giro, HIGH);
  digitalWrite(vel1, HIGH);
  digitalWrite(vel2, HIGH);
  digitalWrite(motor, HIGH);
  digitalWrite(bomba, LOW); // ENCENDIDO DE LA BOMBA PARA VACIAR EL TANQUE
}

void centrifugar()
{ // FUNCION DE CENTRIFUGADO

  digitalWrite(val1, HIGH);
  digitalWrite(giro, HIGH); // AH


  if (acelerado == 0)
  {
    digitalWrite(vel1, HIGH);
    digitalWrite(vel2, HIGH);
    delay(1000);
    digitalWrite(motor, LOW);
    delay(3000);
    digitalWrite(motor, HIGH);
    delay(10);
    digitalWrite(vel1, LOW);
    digitalWrite(vel2, LOW);
    digitalWrite(giro, LOW); // AH
    delay(300);
    digitalWrite(motor, LOW);
    acelerado = 1;
  }


  // SENTIDO DE GIRO EN CENTRIFUGADO , EL CENTRIFUGADO FUNCIONA BIEN CON 1 SENTIDO NO FUNCIONA DE LA MISMA MANERA EN LOS DOS
  //  ACTIVAMOS LOS 2 RELES DE CAMBIOI DE VELOCDIAD
  digitalWrite(vel1, LOW);
  digitalWrite(vel2, LOW);
  digitalWrite(bomba, LOW); // ACTIVAMOS LA BOMBA DE DESAGOTE
  digitalWrite(motor, LOW);
}

void apagar()
{

  digitalWrite(val1, HIGH);
  digitalWrite(giro, HIGH);
  digitalWrite(vel1, HIGH);
  digitalWrite(vel2, HIGH);
  digitalWrite(bomba, HIGH);
  digitalWrite(motor, HIGH);
  digitalWrite(bloqueo, HIGH);
}

void buzzerEnd()
{
  buzzerPWM(alarma, NOTE_A5, 100);
  delay(600);
  nobuzzerPWM(alarma);
  buzzerPWM(alarma, NOTE_A5, 100);
  delay(600);
  nobuzzerPWM(alarma);
  buzzerPWM(alarma, NOTE_A5, 100);
  delay(600);
  nobuzzerPWM(alarma);
  buzzerPWM(alarma, NOTE_A5, 100);
  delay(600);
  nobuzzerPWM(alarma);
  buzzerPWM(alarma, NOTE_A5, 100);
  delay(5000);
  buzzerPWM(alarma, NOTE_A5, 100);
  delay(600);
  nobuzzerPWM(alarma);
  buzzerPWM(alarma, NOTE_A5, 100);
  delay(600);
  nobuzzerPWM(alarma);
  buzzerPWM(alarma, NOTE_A5, 100);
  delay(600);
  nobuzzerPWM(alarma);
  buzzerPWM(alarma, NOTE_A5, 100);
  delay(600);
  nobuzzerPWM(alarma);
  buzzerPWM(alarma, NOTE_A5, 100);
  delay(5000);
  nobuzzerPWM(alarma);
}

// TONO INICIO
void startbuzzerPWM()
{
  buzzerPWM(alarma, NOTE_A5, 200);
  delay(500);
  buzzerPWM(alarma, NOTE_B5, 200);
  delay(500);
  nobuzzerPWM(alarma);
}

void powerOnbuzzerPWM()
{
  buzzerPWM(alarma, NOTE_A5, 200);
  /*delay(1000);
  nobuzzerPWM(alarma);*/
}

void errorbuzzerPWM()
{
  buzzerPWM(alarma, NOTE_C5, 500);
  delay(1000);
  buzzerPWM(alarma, NOTE_C5, 500);
  delay(1000);
  nobuzzerPWM(alarma);
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
    
  }
}

void calibrarJabonera()
{
 logMessage("CALIBRAR POSICION DE JABONERA");
  delay(5000);
  startbuzzerPWM();
  servoTest();
  delay(5000);
  startbuzzerPWM();
  Serial.println(jabservo.read());
  jabservo.write(jabPosPreLavado);
  delay(5000);
  startbuzzerPWM();
  startbuzzerPWM();
   Serial.println(jabservo.read());
  jabservo.write(jabPosLavado);
  delay(5000);
  startbuzzerPWM();
  startbuzzerPWM();
  startbuzzerPWM();
   Serial.println(jabservo.read());
  jabservo.write(jabPosSuavizante);
 
  apagar();
  return;
}

void setJabonera()
{
  FaseIndex fase = fases[faseActual];

  
  if (fase.funcion == LLENADO_PRE_LAVADO)
  {
    jabservo.write(jabPosPreLavado);
  }
  else if (fase.funcion == LLENADO_LAVADO)
  {
    jabservo.write(jabPosLavado);
  }
  else if (fase.funcion == LLENADO_SUAVIZANTE)
  {
    jabservo.write(jabPosSuavizante);
  }else {
   jabservo.write(jabPosPreLavado); 
  }
}

void servoTest(){
   jabservo.write(jabPosPreLavado);
    delay(1000);  
  jabservo.write(jabPosLavado);
    delay(1000);  
  jabservo.write(jabPosSuavizante);
   delay(10);  
}
void loop()
{

  tamborVacio = digitalRead(presostato);

  /////////////////////////////////////////// control tiempos
  loopTimer();

  if (encendida)
  {
    loopLavadora();
  }

  if (segundos % 2 == 0 && segundos != ultimoSegundoEnviado)
  {
    serialSendStatus();
    ultimoSegundoEnviado = segundos;
  }

  processCommand();
	 

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
	doc["TamborVacio"] = tamborVacio;
	doc["Minuto"] = minuto;
	doc["Segundo"] = segundos;
	doc["Paso"] = paso;
	doc["TiempoTotal"] = tiempoTotal;
  int tiempoRestante = tiempoTotal - tiempoTranscurrido;
  doc["TiempoRestante"] = tiempoRestante;
  
  serializeJson(doc, Serial);
	serializeJson(doc, espSerial);
  Serial.println();
	espSerial.println();
}

const FaseIndex* getPrograma() {
  if (programa == 1) {
    return programaLargo;
  } else if (programa == 2) {
    return programaCorto;
  } else if (programa == 3) {
    return programaVaciado;
  } else {
    return programaLargo; // Por defecto
  }
}

void loopLavadora()
{

  if (sttone == 0)
  {
    startbuzzerPWM();
    sttone = 1;
  }

  if (llenadoError)
  {
    errorbuzzerPWM();
    hasError = true;
    logMessage("{\"error\":\"Error de llenado\"}");
    apagar();
    return;
  }

  int totalFases = 0;
  switch (programa) {
    case 1: totalFases = sizeof(programaLargo) / sizeof(FaseIndex); break;
    case 2: totalFases = sizeof(programaCorto) / sizeof(FaseIndex); break;
    case 3: totalFases = sizeof(programaVaciado) / sizeof(FaseIndex); break;
  }

  if (faseActual >= totalFases) {
    encendida = false;
    buzzerEnd();
    apagar();
    return;
  }
  
  FaseIndex fase = fases[faseActual];

  switch (fase.funcion) {
    case LLENADO:
      setJabonera();
      llenado();
      lavado();
      break;
    case LLENADO_PRE_LAVADO:
    case LLENADO_LAVADO:
    case LLENADO_SUAVIZANTE:
      setJabonera();
      llenado();
      if (tamborVacio == 0)
      {
        minuto = minuto + 1;
        segundos = 0;
        logMessage("AVANCE TAMBOR LLENO");
      }
      break;
    case LAVADO:
      apagarLlenado();
      lavado();
      break;
    case VACIADO:
      apagarLlenado();
      vaciado();
      break;
    case CENTRIFUGAR:
      apagarLlenado();
      centrifugar();
      break;
    case ESPERA:
      apagarLlenado();
      break;
  }

  if (minuto >= fase.tiempo) {
    minuto = 0;
    faseActual++;
    paso = 0;
  }
}

void processCommand()
{

  if (!Serial.available())
    return false;  // No hay datos disponibles
  
  //String input = source->readStringUntil('\n');

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, Serial);
  
  if (error) {
    logMessage("{\"error\":\"Invalid JSON code 1\"}");
     Serial.println(error.c_str());
	  return false;
  }

  
	if (!doc.containsKey("command")) {
	  logMessage("{\"error\":\"Missing 'command' key\"}");
	  return false;
	}
  const char* command = doc["command"];
  

  // Comparar el comando recibido
  if (strcmp(command, "start") == 0)
  {


	if (doc.containsKey("programa")) {
       const char* programa = doc["programa"];
       startLavadora(programa);
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
  else if (strcmp(command, "jabon") == 0)
  {
    calibrarJabonera();
	logMessage("{\"status\":\"ok\",\"command\":\"jabon\"}");
  }
  else
  {
    logMessage("{\"error\":\"Unknown command\"}");
  }
}

void startLavadora(const char* programa)
{

  if (strcmp(programa, "corto") == 0)
  {
    setProgramaCorto();
  }
  else if (strcmp(programa, "vaciado") == 0)
  {
    setProgramaVaciado();
  }
  else
  {
    setProgramaLargo();
  }

  calcTiempoTotal();
  resetTimer();
  sttone = 0;
  encendida = 1;

}
void stopLavadora()
{
  encendida = 0;
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
}

void setProgramaLargo()
{
 programa = 1;
}

void setProgramaCorto()
{
 programa = 2;
}

void setProgramaVaciado()
{
  programa = 3;
}
