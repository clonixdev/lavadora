
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
    digitalWrite(motor, HIGH);
    delay(100);
    digitalWrite(vel1, HIGH);
    digitalWrite(vel2, HIGH);
    delay(100);
    digitalWrite(bomba, HIGH);
  }
  else if (paso == 1)
  { // PASO DE LAVADO 1  CICLO DE MOTOR APAGADO
    digitalWrite(motor, HIGH);
    delay(100);
    digitalWrite(giro, HIGH);
  }
  else if (paso == 2)
  { // PASO DE LAVADO 2  CICLO DE GIRO EN EL SENTIDO CONTRARIO A LAS MANECILLAS DEL RELOJ
    digitalWrite(giro, HIGH);
    delay(100);
    digitalWrite(motor, LOW);
  }
  else if (paso == 3)
  { // PASO DE LAVADO 3  CICLO DE MOTOR APAGADO
    digitalWrite(motor, HIGH);
    delay(100);
    digitalWrite(giro, LOW);
  }
  else if (paso == 4)
  { // PASO DE LAVADO 4  CICLO DE GIRO EN EL SENTIDO DE LAS MANECILLAS DEL RELOJ
    digitalWrite(giro, LOW);
    delay(100);
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
  delay(100);
  digitalWrite(giro, HIGH);
  delay(100);
  digitalWrite(vel1, HIGH);
  delay(100);
  digitalWrite(vel2, HIGH);
  delay(100);
  digitalWrite(motor, HIGH);
  delay(100);
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
  delay(100);
  digitalWrite(vel2, LOW);
  delay(100);
  digitalWrite(bomba, LOW); // ACTIVAMOS LA BOMBA DE DESAGOTE
  delay(100);
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

void bloqueoPuerta(){
  digitalWrite(bloqueo, HIGH);
}