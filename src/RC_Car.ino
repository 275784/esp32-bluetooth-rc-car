#include <Bluepad32.h>
#include <ESP32Servo.h>

ControllerPtr myController = nullptr;

// SERWO
Servo esc;
Servo steeringServo;

// ESC / PWM
const int ESC_PIN = 22;          // D22 / GPIO22
const int SERVO_MIN_US = 1000;
const int SERVO_MAX_US = 2000;
const int NEUTRAL_US   = 1500;

// Martwa strefa triggerów
const int TRIGGER_DEADZONE = 40;

// SERVO SKRĘTU
const int STEERING_PIN = 23;     // D23 / GPIO23

// Ustawienia środka i zakresu
// Jeśli dalej nie jest idealnie prosto, zmieniaj CENTER np. 94, 95, 97, 98
const int STEER_CENTER_ANGLE = 96;
const int STEER_RANGE_ANGLE  = 30;   // zakres 96-30 do 96+30
const int STEER_STICK_DEADZONE = 90; // większa martwa strefa lewej gałki
const int STEER_STEP = 2;            // wolniejsze ruchy = mniejsze szarpanie
const bool STEER_INVERT = true;     // jak skręca odwrotnie, zmień na true

int currentSteerAngle = STEER_CENTER_ANGLE;
int targetSteerAngle  = STEER_CENTER_ANGLE;

// PRĘDKOŚCI
// Normalny tryb
const int FORWARD_NORMAL_MAX_US = 1840;

// Sport
const int FORWARD_SPORT_MAX_US  = 2000;

// Wsteczny
const int REVERSE_FIXED_US      = 1200;

// STANY
bool runEnabled = true;
bool sportMode = false;

// Kierunek aktywny:
//  0 = brak
//  1 = przód
// -1 = tył
int activeDirection = 0;

// Aktualny i docelowy PWM
int currentPwmUs = NEUTRAL_US;
int targetPwmUs  = NEUTRAL_US;

// Miękkie przejścia
const int ACCEL_STEP_US       = 5;
const int BRAKE_TO_NEUTRAL_US = 10;
const int REVERSE_STEP_US     = 4;
const unsigned long NEUTRAL_HOLD_MS = 180;

bool neutralHoldActive = false;
unsigned long neutralHoldStart = 0;

// Pamięć przycisków
bool prevA = false;
bool prevB = false;

// Debug
unsigned long lastPrint = 0;

// POMOCNICZE
int mapConstrainInt(int x, int inMin, int inMax, int outMin, int outMax) {
  x = constrain(x, inMin, inMax);
  return map(x, inMin, inMax, outMin, outMax);
}

void setESC(int us) {
  us = constrain(us, SERVO_MIN_US, SERVO_MAX_US);
  esc.writeMicroseconds(us);
}

void printRunState() {
  Serial.print("RUN = ");
  Serial.println(runEnabled ? "ENABLED" : "DISABLED");
}

void printSportMode() {
  Serial.print("SPORT = ");
  Serial.println(sportMode ? "ON" : "OFF");
}

void setNeutralImmediate() {
  currentPwmUs = NEUTRAL_US;
  targetPwmUs  = NEUTRAL_US;
  neutralHoldActive = false;
  setESC(NEUTRAL_US);
}

void goToSafeNeutral() {
  targetPwmUs = NEUTRAL_US;
}

void stepToward(int &value, int target, int step) {
  if (value < target) {
    value += step;
    if (value > target) value = target;
  } else if (value > target) {
    value -= step;
    if (value < target) value = target;
  }
}

bool crossesNeutral(int fromUs, int toUs) {
  return ((fromUs > NEUTRAL_US && toUs < NEUTRAL_US) ||
          (fromUs < NEUTRAL_US && toUs > NEUTRAL_US));
}

void updateMotorOutput() {
  // Najpierw łagodne przejście przez neutral
  if (crossesNeutral(currentPwmUs, targetPwmUs)) {
    stepToward(currentPwmUs, NEUTRAL_US, BRAKE_TO_NEUTRAL_US);
    setESC(currentPwmUs);

    if (currentPwmUs == NEUTRAL_US) {
      if (!neutralHoldActive) {
        neutralHoldActive = true;
        neutralHoldStart = millis();
      }

      if (millis() - neutralHoldStart < NEUTRAL_HOLD_MS) {
        return;
      } else {
        neutralHoldActive = false;
      }
    } else {
      neutralHoldActive = false;
      return;
    }
  } else {
    neutralHoldActive = false;
  }

  int step = ACCEL_STEP_US;

  if (targetPwmUs == NEUTRAL_US) {
    step = BRAKE_TO_NEUTRAL_US;
  }

  if (currentPwmUs < NEUTRAL_US || targetPwmUs < NEUTRAL_US) {
    step = REVERSE_STEP_US;
  }

  stepToward(currentPwmUs, targetPwmUs, step);
  setESC(currentPwmUs);
}

int getForwardTarget(int rt) {
  int maxUs = sportMode ? FORWARD_SPORT_MAX_US : FORWARD_NORMAL_MAX_US;
  return mapConstrainInt(rt, TRIGGER_DEADZONE, 1023, NEUTRAL_US, maxUs);
}

int getReverseTarget(int lt) {
  return mapConstrainInt(lt, TRIGGER_DEADZONE, 1023, NEUTRAL_US, REVERSE_FIXED_US);
}

// STEROWANIE SERWEM
void setSteeringImmediate(int angle) {
  int minAngle = STEER_CENTER_ANGLE - STEER_RANGE_ANGLE;
  int maxAngle = STEER_CENTER_ANGLE + STEER_RANGE_ANGLE;

  angle = constrain(angle, minAngle, maxAngle);
  currentSteerAngle = angle;
  targetSteerAngle = angle;
  steeringServo.write(angle);
}

int getSteeringTargetFromStick(int axisX) {
  // Bluepad32: lewa gałka poziomo zwykle około -512 ... 512
  if (abs(axisX) < STEER_STICK_DEADZONE) {
    return STEER_CENTER_ANGLE;
  }

  int x = constrain(axisX, -512, 512);

  if (STEER_INVERT) {
    x = -x;
  }

  int minAngle = STEER_CENTER_ANGLE - STEER_RANGE_ANGLE;
  int maxAngle = STEER_CENTER_ANGLE + STEER_RANGE_ANGLE;

  return map(x, -512, 512, minAngle, maxAngle);
}

void updateSteeringOutput() {
  stepToward(currentSteerAngle, targetSteerAngle, STEER_STEP);
  steeringServo.write(currentSteerAngle);
}

void processSteering(ControllerPtr gp) {
  int axisX = gp->axisX();   // lewa gałka poziomo
  targetSteerAngle = getSteeringTargetFromStick(axisX);
}

// CALLBACKI BLUEPAD32
void onConnectedController(ControllerPtr ctl) {
  if (myController == nullptr) {
    myController = ctl;
    Serial.println("Pad polaczony");

    ControllerProperties p = ctl->getProperties();
    char buf[120];
    sprintf(buf, "VID/PID: %04x:%04x, flags: 0x%02x",
            p.vendor_id, p.product_id, p.flags);
    Serial.println(buf);

    printRunState();
    printSportMode();
  } else {
    Serial.println("Juz mam podlaczony jeden pad, ignoruje kolejny");
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  if (myController == ctl) {
    myController = nullptr;
    runEnabled = false;
    activeDirection = 0;
    setNeutralImmediate();
    setSteeringImmediate(STEER_CENTER_ANGLE);
    Serial.println("Pad rozlaczony");
  }
}

// PRZYCISKI
void processButtons(ControllerPtr gp) {
  bool a = gp->a();
  bool b = gp->b();

  // A = SPORT ON / OFF
  if (a && !prevA) {
    sportMode = !sportMode;
    activeDirection = 0;
    goToSafeNeutral();
    printSportMode();
  }

  // B = RUN / STOP
  if (b && !prevB) {
    runEnabled = !runEnabled;
    activeDirection = 0;
    goToSafeNeutral();
    printRunState();
  }

  prevA = a;
  prevB = b;
}

// JAZDA
void processDrive(ControllerPtr gp) {
  int rt = gp->throttle();   // 0..1023
  int lt = gp->brake();      // 0..1023

  bool rtPressed = (rt > TRIGGER_DEADZONE);
  bool ltPressed = (lt > TRIGGER_DEADZONE);

  // STOP = wszystko ignorowane
  if (!runEnabled) {
    activeDirection = 0;
    targetPwmUs = NEUTRAL_US;
    return;
  }

  // Oba triggery naraz = neutral
  if (rtPressed && ltPressed) {
    activeDirection = 0;
    targetPwmUs = NEUTRAL_US;
    return;
  }

  // Jeśli aktywny był przód
  if (activeDirection == 1) {
    // Próba przejścia na tył bez puszczenia gazu -> najpierw neutral
    if (ltPressed && !rtPressed) {
      activeDirection = 0;
      targetPwmUs = NEUTRAL_US;
      return;
    }

    // Puściłeś RT
    if (!rtPressed) {
      activeDirection = 0;
      targetPwmUs = NEUTRAL_US;
      return;
    }
  }

  // Jeśli aktywny był tył
  else if (activeDirection == -1) {
    // Próba przejścia na przód bez puszczenia tyłu -> najpierw neutral
    if (rtPressed && !ltPressed) {
      activeDirection = 0;
      targetPwmUs = NEUTRAL_US;
      return;
    }

    // Puściłeś LT
    if (!ltPressed) {
      activeDirection = 0;
      targetPwmUs = NEUTRAL_US;
      return;
    }
  }

  // Brak aktywnego kierunku
  else {
    if (rtPressed && !ltPressed) {
      activeDirection = 1;
    } else if (ltPressed && !rtPressed) {
      activeDirection = -1;
    } else {
      targetPwmUs = NEUTRAL_US;
      return;
    }
  }

  // Wyznaczanie celu PWM
  if (activeDirection == 1 && rtPressed) {
    targetPwmUs = getForwardTarget(rt);
  } else if (activeDirection == -1 && ltPressed) {
    targetPwmUs = getReverseTarget(lt);
  } else {
    targetPwmUs = NEUTRAL_US;
  }
}

void processGamepad(ControllerPtr gp) {
  processButtons(gp);
  processDrive(gp);
  processSteering(gp);

  if (millis() - lastPrint > 250) {
    lastPrint = millis();

    Serial.print("RUN=");
    Serial.print(runEnabled ? "ON" : "OFF");
    Serial.print(" SPORT=");
    Serial.print(sportMode ? "ON" : "OFF");
    Serial.print(" DIR=");
    Serial.print(activeDirection);
    Serial.print(" CUR=");
    Serial.print(currentPwmUs);
    Serial.print(" TAR=");
    Serial.print(targetPwmUs);
    Serial.print(" RT=");
    Serial.print(gp->throttle());
    Serial.print(" LT=");
    Serial.print(gp->brake());
    Serial.print(" STICK_X=");
    Serial.print(gp->axisX());
    Serial.print(" STEER=");
    Serial.println(targetSteerAngle);
  }
}

// SETUP
void setup() {
  Serial.begin(115200);
  delay(1000);

  esc.setPeriodHertz(50);
  esc.attach(ESC_PIN, SERVO_MIN_US, SERVO_MAX_US);
  setNeutralImmediate();

  steeringServo.setPeriodHertz(50);
  steeringServo.attach(STEERING_PIN, 500, 2400);
  setSteeringImmediate(STEER_CENTER_ANGLE);

  Serial.println("Start ESP32 + Xbox + ESC + STEERING");
  Serial.println("D22/GPIO22 -> SIGNAL ESC");
  Serial.println("D23/GPIO23 -> SERVO SKRETU");
  Serial.println("GND ESP32 musi byc wspolne z GND ESC i GND serwa");
  Serial.println("RT = przod");
  Serial.println("LT = tyl / hamowanie");
  Serial.println("Lewa galka poziomo = skret kol");
  Serial.println("A = SPORT ON/OFF");
  Serial.println("B = RUN / STOP");
  Serial.println("Bez biegow");
  Serial.println("Bez AUTO/MANUAL");

  BP32.setup(&onConnectedController, &onDisconnectedController);

  // Odkomentuj tylko raz, jesli chcesz skasowac stare sparowane urzadzenia:
  // BP32.forgetBluetoothKeys();

  const uint8_t* addr = BP32.localBdAddress();
  Serial.print("BT MAC ESP32: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(addr[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  printRunState();
  printSportMode();
}


// LOOP
void loop() {
  BP32.update();

  if (myController && myController->isConnected() && myController->isGamepad()) {
    processGamepad(myController);
  } else {
    targetPwmUs = NEUTRAL_US;
    targetSteerAngle = STEER_CENTER_ANGLE;
  }

  updateMotorOutput();
  updateSteeringOutput();
  delay(10);
}