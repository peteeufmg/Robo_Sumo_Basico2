// ============================
// CONFIGURAÇÃO DE PINOS - ESP32
// ============================
#include <Arduino.h>
#define IR_FRONTAL_ESQUERDA     35
#define IR_FRONTAL_DIREITA      33
#define IR_TRASEIRO_ESQUERDA    32
#define IR_TRASEIRO_DIREITA     34

#define TRIGGER_PIN             21
#define ECHO_PIN_A              22
#define ECHO_PIN_B              18

// Pinos de controle do motor A 
#define MOTOR_A_IN1  4
#define MOTOR_A_IN2  5
#define MOTOR_A_PWM  16  

// Pinos de controle do motor B 
#define MOTOR_B_IN3  17
#define MOTOR_B_IN4  19
#define MOTOR_B_PWM  23   

// ============================
// VELOCIDADE E TEMPO
// ============================

#define VELOCIDADE_MIN 80
#define VELOCIDADE_MAX 255
#define limiteDetecao 40.0

#define TEMPO_RE   300
#define TEMPO_ACAO 400

bool modoDebug = true;

// ============================
// RAMPA
// ============================

#define TEMPO_RAMPA 5
#define PASSO_RAMPA 5

int pwmMotorA = 0;
int pwmMotorB = 0;
unsigned long ultimaAtualizacaoRampa = 0;

// ============================
// Funções de velocidade
// ============================

int mapVelocidade(int percentual) {
  percentual = constrain(percentual, 0, 100);
  return (percentual * (VELOCIDADE_MAX - VELOCIDADE_MIN)) / 100 + VELOCIDADE_MIN;
}

// Função para aplicar PWM no pino de enable e direção nos dois pinos
void setMotor(int in1, int in2, int pwmPin, int velocidade) {
  if (velocidade > 0) { // Frente
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
    analogWrite(pwmPin, velocidade);
  } else if (velocidade < 0) { // Ré
    digitalWrite(in1,HIGH);
    digitalWrite(in2, LOW);
    analogWrite(pwmPin, -velocidade);
  } else { // Parado
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    analogWrite(pwmPin, 0);
  }
}

void atualizarPWMComRampa(int pwmAlvoA, int pwmAlvoB) {
  unsigned long agora = millis();
  if (agora - ultimaAtualizacaoRampa >= TEMPO_RAMPA) {
    if (pwmMotorA != pwmAlvoA) {
      pwmMotorA += (pwmAlvoA > pwmMotorA) ? PASSO_RAMPA : -PASSO_RAMPA;
      pwmMotorA = constrain(pwmMotorA, -255, 255);
      setMotor(MOTOR_A_IN1, MOTOR_A_IN2, MOTOR_A_PWM, pwmMotorA);
    }
    if (pwmMotorB != pwmAlvoB) {
      pwmMotorB += (pwmAlvoB > pwmMotorB) ? PASSO_RAMPA : -PASSO_RAMPA;
      pwmMotorB = constrain(pwmMotorB, -255, 255);
      setMotor(MOTOR_B_IN3, MOTOR_B_IN4, MOTOR_B_PWM, pwmMotorB);
    }
    ultimaAtualizacaoRampa = agora;
  }
}

// ============================
// CONTROLE DOS MOTORES
// ============================

void moveForwardInstantaneo(int pwm) {
  setMotor(MOTOR_A_IN1, MOTOR_A_IN2, MOTOR_A_PWM, pwm);
  setMotor(MOTOR_B_IN3, MOTOR_B_IN4, MOTOR_B_PWM, pwm);
  pwmMotorA = pwm;
  pwmMotorB = pwm;
}

void moveBackwardInstantaneo(int pwm) {
  setMotor(MOTOR_A_IN1, MOTOR_A_IN2, MOTOR_A_PWM, -pwm);
  setMotor(MOTOR_B_IN3, MOTOR_B_IN4, MOTOR_B_PWM, -pwm);
  pwmMotorA = -pwm;
  pwmMotorB = -pwm;
}

void curveLeftInstantaneo(int pwmEsq, int pwmDir) {
  setMotor(MOTOR_A_IN1, MOTOR_A_IN2, MOTOR_A_PWM, pwmEsq);
  setMotor(MOTOR_B_IN3, MOTOR_B_IN4, MOTOR_B_PWM, pwmDir);
  pwmMotorA = pwmEsq;
  pwmMotorB = pwmDir;
}

void curveRightInstantaneo(int pwmEsq, int pwmDir) {
  setMotor(MOTOR_A_IN1, MOTOR_A_IN2, MOTOR_A_PWM, pwmEsq);
  setMotor(MOTOR_B_IN3, MOTOR_B_IN4, MOTOR_B_PWM, pwmDir);
  pwmMotorA = pwmEsq;
  pwmMotorB = pwmDir;
}

void stopMotors() {
  setMotor(MOTOR_A_IN1, MOTOR_A_IN2, MOTOR_A_PWM, 0);
  setMotor(MOTOR_B_IN3, MOTOR_B_IN4, MOTOR_B_PWM, 0);
  pwmMotorA = 0;
  pwmMotorB = 0;
}

// ============================
// SENSOR ULTRASSÔNICO
// ============================

float readUltrasonicDistance(uint8_t trigger, uint8_t echo) {
  digitalWrite(trigger, LOW);
  delayMicroseconds(2);
  digitalWrite(trigger, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigger, LOW);

  unsigned long startTime = micros();
  while (digitalRead(echo) == LOW) {
    if (micros() - startTime > 6000) return 999;
  }

  unsigned long echoStart = micros();
  while (digitalRead(echo) == HIGH) {
    if (micros() - echoStart > 6000) return 999;
  }

  long duration = micros() - echoStart;
  return duration * 0.017;
}

float measureDistance(uint8_t trigger, uint8_t echo) {
    digitalWrite(trigger, HIGH);
    delayMicroseconds(10);  // Envia um pulso de 10 microssegundos
    digitalWrite(trigger, LOW);

    long duration = pulseIn(echo, HIGH, 6000); // Mede o tempo até o eco voltar (timeout de 6ms)
    if (duration == 0) return 999; // Se não houve retorno, retorna um valor alto (nada detectado)
    return duration * 0.017; // Converte tempo em distância (em centímetros)
}
// ============================
// DETECÇÃO DE BORDA
// ============================

bool detectarBorda() {
  bool irFD = digitalRead(IR_FRONTAL_DIREITA);
  bool irFE = digitalRead(IR_FRONTAL_ESQUERDA);
  bool irTD = digitalRead(IR_TRASEIRO_DIREITA);
  bool irTE = digitalRead(IR_TRASEIRO_ESQUERDA);

  if (!irFD || !irFE){
    if (modoDebug) Serial.println("Borda frontal");
    moveBackwardInstantaneo(mapVelocidade(50));
    delay(TEMPO_RE);
    stopMotors();
    return true;
  } else if (!irTD || !irTE) {
    if (modoDebug) Serial.println("Borda traseira");
    moveForwardInstantaneo(mapVelocidade(70));
    delay(TEMPO_ACAO);
    stopMotors();
    return true;
  }
  return false;
}

// ============================
// ATAQUE CURTO
// ============================

void ataqueCurto() {
  int pwm = mapVelocidade(80);
  unsigned long tempoInicial = millis();
  if(millis() - tempoInicial < 50) {
    moveForwardInstantaneo(pwm);
    if(detectarBorda()) {
      Serial.println("Ataque interrompido por borda!");
      stopMotors();
      return;
    } // Espera 1ms para não sobrecarregar o processador
  } 
  //se o loop acabar sem detectar borda ele para os motores
}


// ============================
// LÓGICA DE PERSEGUIÇÃO
// ============================

void perseguirOponente(float distanciaA, float distanciaB) {
  bool detectadoA = (distanciaA < limiteDetecao);
  bool detectadoB = (distanciaB < limiteDetecao);

  if (distanciaA < 20 || distanciaB < 20) {
    ataqueCurto();
    //verificar a borda a cada ataque ou pico de ataque curto
    return;
  }

  float diff = (distanciaB - distanciaA) * limiteDetecao / 100.0;
  int pwmEsq = mapVelocidade(50 - (diff * 0.2));
  int pwmDir = mapVelocidade(50 + (diff * 0.2));

  atualizarPWMComRampa(pwmEsq, pwmDir);
}

void DetectarOponente(int velocidade) {
    setMotor(MOTOR_A_IN1, MOTOR_A_IN2, MOTOR_A_PWM, -velocidade);
    setMotor(MOTOR_B_IN3, MOTOR_B_IN4, MOTOR_B_PWM, velocidade);
    pwmMotorA = -velocidade;
    pwmMotorB = velocidade;
}

// ============================
// SETUP
// ============================

void setup() {
  Serial.begin(115200);

  pinMode(IR_FRONTAL_ESQUERDA, INPUT);
  pinMode(IR_FRONTAL_DIREITA, INPUT);
  pinMode(IR_TRASEIRO_ESQUERDA, INPUT);
  pinMode(IR_TRASEIRO_DIREITA, INPUT);

  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN_A, INPUT);
  pinMode(ECHO_PIN_B, INPUT);

  pinMode(MOTOR_A_IN1, OUTPUT);
  pinMode(MOTOR_A_IN2, OUTPUT);
  pinMode(MOTOR_A_PWM, OUTPUT);
  pinMode(MOTOR_B_IN3, OUTPUT);
  pinMode(MOTOR_B_IN4, OUTPUT);
  pinMode(MOTOR_B_PWM, OUTPUT);

  stopMotors();

  if (modoDebug)
  delay(3000);
}

// ============================
// LOOP
// ============================

void loop() {
  if (!detectarBorda()) {
    float distanciaA = measureDistance(TRIGGER_PIN, ECHO_PIN_A);
    float distanciaB = measureDistance(TRIGGER_PIN, ECHO_PIN_B);

    if (modoDebug) {
      Serial.print("Distância A: "); Serial.print(distanciaA);
      Serial.print(" | Distância B: "); Serial.println(distanciaB);
    }

    // Se não detectou oponente, gira no próprio eixo
    if (distanciaA >= limiteDetecao && distanciaB >= limiteDetecao) {
      DetectarOponente(mapVelocidade(30)); // Velocidade moderada
    } else {
      perseguirOponente(distanciaA, distanciaB);
    }
  }
}
