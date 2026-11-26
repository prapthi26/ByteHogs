/*
  ===========================================
             HUMMER AI - VERSION 2.0
  Avishkar Hackathon - AIT College
  ===========================================

  Hardware:
  Arduino Uno
  L298N Motor Driver
  4 DC Geared Motors
  HC-SR04 Ultrasonic Sensor
  2 IR Obstacle Sensors
  HC-05 / Compatible Bluetooth Serial Module

  BLUETOOTH COMMANDS:

  F = Forward
  B = Backward
  L = Left
  R = Right
  S = Stop
  M = Manual Mode / Reset Emergency Stop
  A = Autonomous Mode
  P = Patrol Mode
  E = Emergency Stop
  H = Manual heartbeat
  0-9 = Speed level
  Q = Request sensor information

  TELEMETRY:
  DATA,MODE,DISTANCE,LEFT_IR,RIGHT_IR,SPEED

  ===========================================
*/

// ===========================================
// 1. LIBRARY
// ===========================================

#include <SoftwareSerial.h>

// Arduino RX = 12
// Arduino TX = 13
SoftwareSerial BT(12, 13);


// ===========================================
// 2. MOTOR PINS
// ===========================================

const byte ENA = 5;
const byte IN1 = 8;
const byte IN2 = 9;

const byte ENB = 6;
const byte IN3 = 10;
const byte IN4 = 11;


// ===========================================
// 3. SENSOR PINS
// ===========================================

const byte TRIG = 2;
const byte ECHO = 3;

const byte IR_LEFT = A0;
const byte IR_RIGHT = A1;

// Assumes active-LOW IR modules.
// Verify this with your actual sensors.
const byte IR_ACTIVE = LOW;


// ===========================================
// 4. SETTINGS
// ===========================================

const unsigned long TELEMETRY_INTERVAL = 300;
const unsigned long SENSOR_INTERVAL = 70;

const unsigned long MANUAL_TIMEOUT = 1000;

const float MANUAL_STOP_DISTANCE = 15.0;
const float AUTO_STOP_DISTANCE = 20.0;

int speedLevel = 6;
int speedValue = 146;


// ===========================================
// 5. ROBOT MODES
// ===========================================

enum RobotMode {
  MANUAL,
  AUTO,
  PATROL
};

RobotMode mode = MANUAL;

char manualCommand = 'S';

bool emergencyStop = false;


// ===========================================
// 6. SENSOR VARIABLES
// ===========================================

float distanceCM = -1;

bool leftObstacle = false;
bool rightObstacle = false;

unsigned long lastSensorRead = 0;
unsigned long lastTelemetry = 0;


// ===========================================
// 7. SAFETY VARIABLES
// ===========================================

unsigned long lastManualHeartbeat = 0;

bool manualTimeoutStop = false;


// ===========================================
// 8. AUTONOMOUS NAVIGATION
// ===========================================

enum AutoPhase {
  CRUISING,
  REVERSING,
  TURNING
};

AutoPhase autoPhase = CRUISING;

unsigned long phaseStart = 0;

char turnDirection = 'R';


// ===========================================
// 9. SETUP
// ===========================================

void setup() {

  Serial.begin(9600);

  BT.begin(9600);

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  pinMode(IR_LEFT, INPUT);
  pinMode(IR_RIGHT, INPUT);

  stopMotors();

  lastManualHeartbeat = millis();

  BT.println("HUMMER_AI_READY");

  Serial.println("HUMMER AI READY");

}


// ===========================================
// 10. MAIN LOOP
// ===========================================

void loop() {

  readBluetoothCommands();

  updateSensors();

  checkManualSafety();

  if (emergencyStop) {

    stopMotors();

  }

  else if (mode == MANUAL) {

    manualDrive();

  }

  else if (mode == AUTO) {

    autonomousDrive(false);

  }

  else if (mode == PATROL) {

    autonomousDrive(true);

  }

  sendTelemetry();

}


// ===========================================
// 11. BLUETOOTH COMMAND RECEIVER
// ===========================================

void readBluetoothCommands() {

  while (BT.available()) {

    char c = BT.read();

    if (c == '\n' || c == '\r') {
      continue;
    }

    // Speed control

    if (c >= '0' && c <= '9') {

      speedLevel = c - '0';

      speedValue = map(
        speedLevel,
        0,
        9,
        0,
        220
      );

      BT.print("SPEED:");
      BT.println(speedLevel);

      continue;
    }

    switch (c) {

      case 'E':

        emergencyStop = true;

        manualCommand = 'S';

        stopMotors();

        BT.println("EMERGENCY_STOP");

        break;


      case 'M':

        emergencyStop = false;

        mode = MANUAL;

        manualCommand = 'S';

        manualTimeoutStop = false;

        lastManualHeartbeat = millis();

        resetNavigation();

        stopMotors();

        BT.println("MODE:MANUAL");

        break;


      case 'F':
      case 'B':
      case 'L':
      case 'R':
      case 'S':

        if (!emergencyStop) {

          mode = MANUAL;

          manualCommand = c;

          lastManualHeartbeat = millis();

          manualTimeoutStop = false;

        }

        break;


      case 'H':

        if (!emergencyStop && mode == MANUAL) {

          lastManualHeartbeat = millis();

        }

        break;


      case 'A':

        if (!emergencyStop) {

          mode = AUTO;

          manualCommand = 'S';

          resetNavigation();

          stopMotors();

          BT.println("MODE:AUTO");

        }

        break;


      case 'P':

        if (!emergencyStop) {

          mode = PATROL;

          manualCommand = 'S';

          resetNavigation();

          stopMotors();

          BT.println("MODE:PATROL");

        }

        break;


      case 'Q':

        sendTelemetryNow();

        break;

    }

  }

}


// ===========================================
// 12. SENSOR READING
// ===========================================

void updateSensors() {

  if (millis() - lastSensorRead <
      SENSOR_INTERVAL) {

    return;

  }

  lastSensorRead = millis();

  distanceCM = getDistance();

  leftObstacle =
    digitalRead(IR_LEFT) == IR_ACTIVE;

  rightObstacle =
    digitalRead(IR_RIGHT) == IR_ACTIVE;

}


float getDistance() {

  digitalWrite(TRIG, LOW);

  delayMicroseconds(3);

  digitalWrite(TRIG, HIGH);

  delayMicroseconds(10);

  digitalWrite(TRIG, LOW);

  unsigned long duration = pulseIn(
    ECHO,
    HIGH,
    25000
  );

  if (duration == 0) {

    return -1;

  }

  return duration * 0.0343 / 2.0;

}


// ===========================================
// 13. MOTOR CONTROL
// ===========================================

void setMotor(int leftSpeed, int rightSpeed) {

  leftSpeed = constrain(leftSpeed, -220, 220);

  rightSpeed = constrain(rightSpeed, -220, 220);


  // LEFT MOTOR CHANNEL

  digitalWrite(IN1, leftSpeed > 0);

  digitalWrite(IN2, leftSpeed < 0);

  analogWrite(ENA, abs(leftSpeed));


  // RIGHT MOTOR CHANNEL

  digitalWrite(IN3, rightSpeed > 0);

  digitalWrite(IN4, rightSpeed < 0);

  analogWrite(ENB, abs(rightSpeed));

}


void stopMotors() {

  analogWrite(ENA, 0);

  analogWrite(ENB, 0);

  digitalWrite(IN1, LOW);

  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);

  digitalWrite(IN4, LOW);

}


void forward() {

  setMotor(speedValue, speedValue);

}


void backward() {

  setMotor(-speedValue, -speedValue);

}


void turnLeft() {

  setMotor(-speedValue, speedValue);

}


void turnRight() {

  setMotor(speedValue, -speedValue);

}


// ===========================================
// 14. MANUAL SAFETY
// ===========================================

void checkManualSafety() {

  if (mode != MANUAL || emergencyStop) {
    return;
  }

  if (millis() - lastManualHeartbeat >
      MANUAL_TIMEOUT) {

    manualCommand = 'S';

    manualTimeoutStop = true;

    stopMotors();

  }

}


// ===========================================
// 15. MANUAL MODE
// ===========================================

void manualDrive() {

  if (manualTimeoutStop) {

    stopMotors();

    return;

  }

  if (manualCommand == 'F') {

    if (distanceCM < 0 ||
        distanceCM <= MANUAL_STOP_DISTANCE) {

      stopMotors();

      return;

    }

  }

  switch (manualCommand) {

    case 'F':

      forward();

      break;


    case 'B':

      backward();

      break;


    case 'L':

      turnLeft();

      break;


    case 'R':

      turnRight();

      break;


    default:

      stopMotors();

      break;

  }

}


// ===========================================
// 16. RESET NAVIGATION
// ===========================================

void resetNavigation() {

  autoPhase = CRUISING;

  phaseStart = millis();

  turnDirection = 'R';

}


// ===========================================
// 17. AUTONOMOUS AND PATROL MODE
// ===========================================

void autonomousDrive(bool patrolMode) {

  unsigned long now = millis();

  int cruiseSpeed = patrolMode
                    ? speedValue * 3 / 4
                    : speedValue;


  // CRUISING PHASE

  if (autoPhase == CRUISING) {

    bool ultrasonicBlocked =
      distanceCM < 0 ||
      distanceCM <= AUTO_STOP_DISTANCE;

    bool obstacleDetected =
      ultrasonicBlocked ||
      leftObstacle ||
      rightObstacle;


    if (obstacleDetected) {

      stopMotors();

      phaseStart = now;

      autoPhase = REVERSING;


      if (leftObstacle && !rightObstacle) {

        turnDirection = 'R';

      }

      else {

        turnDirection = 'L';

      }

      return;

    }


    setMotor(cruiseSpeed, cruiseSpeed);

  }


  // REVERSING PHASE

  else if (autoPhase == REVERSING) {

    backward();

    if (now - phaseStart >= 250) {

      stopMotors();

      phaseStart = now;

      autoPhase = TURNING;

    }

  }


  // TURNING PHASE

  else if (autoPhase == TURNING) {

    if (turnDirection == 'L') {

      turnLeft();

    }

    else {

      turnRight();

    }

    if (now - phaseStart >= 400) {

      stopMotors();

      autoPhase = CRUISING;

    }

  }

}


// ===========================================
// 18. BLUETOOTH TELEMETRY
// ===========================================

void sendTelemetry() {

  if (millis() - lastTelemetry <
      TELEMETRY_INTERVAL) {

    return;

  }

  lastTelemetry = millis();

  sendTelemetryNow();

}


void sendTelemetryNow() {

  BT.print("DATA,");

  if (mode == MANUAL) {

    BT.print("MANUAL");

  }

  else if (mode == AUTO) {

    BT.print("AUTO");

  }

  else {

    BT.print("PATROL");

  }

  BT.print(",");

  if (distanceCM < 0) {

    BT.print("-1");

  }

  else {

    BT.print(distanceCM, 1);

  }

  BT.print(",");

  BT.print(leftObstacle);

  BT.print(",");

  BT.print(rightObstacle);

  BT.print(",");

  BT.println(speedLevel);

}