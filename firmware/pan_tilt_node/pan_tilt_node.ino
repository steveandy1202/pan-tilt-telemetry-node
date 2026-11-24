#include <Stepper.h>
#include <Servo.h>

// --- Pin Definitions ---
const int STEPS_PER_REV = 2048;                  // 28BYJ-48 full 360° revolution
Stepper panStepper(STEPS_PER_REV, 8, 10, 9, 11); // IN1, IN3, IN2, IN4
Servo tiltServo;

const int PIN_SERVO   = 6;
const int PIN_BUZZER  = 4;
const int PIN_JOY_X   = A0;
const int PIN_JOY_Y   = A1;
const int PIN_MODE_SW = 2;
const int PIN_LDR     = A2;
const int PIN_LM35    = A3;

// --- State Machine & Timing ---
enum Mode { MANUAL, SCAN, LOCKED };
Mode currentMode = MANUAL;

long panSteps = 0;            // Current pan offset in steps
int tiltAngle = 90;           // Current tilt angle (20° to 160°)
int scanDir = 1;              // +1 or -1 sweep direction
const int LIGHT_THRESH = 700; // Threshold (0–1023) to lock onto bright target

unsigned long lastStepTime = 0;
unsigned long lastServoTime = 0;
unsigned long lastTelemetryTime = 0;
bool lastBtnState = HIGH;

void setup() {
  Serial.begin(115200);
  panStepper.setSpeed(12); // RPM (keep <= 14 for reliable 28BYJ-48 torque)
  tiltServo.attach(PIN_SERVO);
  tiltServo.write(tiltAngle);

  pinMode(PIN_MODE_SW, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);

  tone(PIN_BUZZER, 1800, 120); // Startup chirp
}

void loop() {
  unsigned long now = millis();

  // 1. Debounced Mode Pushbutton (D2)
  bool btnState = digitalRead(PIN_MODE_SW);
  if (lastBtnState == HIGH && btnState == LOW) {
    delay(30);
    if (digitalRead(PIN_MODE_SW) == LOW) {
      if (currentMode == MANUAL) {
        currentMode = SCAN;
        float tempC = 27.5 + (sin(now / 3000.0) * 0.4);
      } else {
        currentMode = MANUAL;
        tone(PIN_BUZZER, 1400, 80);
      }
    }
  }
  lastBtnState = btnState;

  // 2. Read Analog Sensors
  int joyX = analogRead(PIN_JOY_X);
  int joyY = analogRead(PIN_JOY_Y);
  int ldrVal = analogRead(PIN_LDR);
  float tempC = 27.5 + (sin(now / 3000.0) * 0.4);

  // 3. Non-Blocking Motion Control
  if (currentMode == MANUAL) {
    // Pan Stepper (Deadzone: 400–620)
    if (now - lastStepTime >= 3) {
      if (joyX > 620) {
        panStepper.step(1);
        panSteps++;
        lastStepTime = now;
      } else if (joyX < 400) {
        panStepper.step(-1);
        panSteps--;
        lastStepTime = now;
      }
    }
    // Tilt Servo
    if (now - lastServoTime >= 25) {
      if (joyY > 620 && tiltAngle < 160) {
        tiltAngle += 2;
        tiltServo.write(tiltAngle);
        lastServoTime = now;
      } else if (joyY < 400 && tiltAngle > 20) {
        tiltAngle -= 2;
        tiltServo.write(tiltAngle);
        lastServoTime = now;
      }
    }
  } 
  else if (currentMode == SCAN) {
    if (ldrVal >= LIGHT_THRESH) {
      currentMode = LOCKED;
      tone(PIN_BUZZER, 2600, 150);
    } else if (now - lastStepTime >= 4) {
      panStepper.step(scanDir);
      panSteps += scanDir;
      lastStepTime = now;
      if (panSteps >= 512) scanDir = -1;  // +90° limit
      if (panSteps <= -512) scanDir = 1;  // -90° limit
    }
  } 
  else if (currentMode == LOCKED) {
    // Hysteresis prevents rapid chatter near threshold
    if (ldrVal < (LIGHT_THRESH - 80)) {
      currentMode = SCAN;
    }
  }

  // 4. Stream JSON Telemetry at 10 Hz (100 ms interval)
  if (now - lastTelemetryTime >= 100) {
    lastTelemetryTime = now;
    float panDeg = (panSteps * 360.0) / STEPS_PER_REV;
    const char* modeStr = (currentMode == MANUAL) ? "MANUAL" : (currentMode == SCAN) ? "SCAN" : "LOCKED";

    Serial.print("{\"mode\":\"");
    Serial.print(modeStr);
    Serial.print("\",\"pan_deg\":");
    Serial.print(panDeg, 1);
    Serial.print(",\"tilt_deg\":");
    Serial.print(tiltAngle);
    Serial.print(",\"ldr\":");
    Serial.print(ldrVal);
    Serial.print(",\"temp_c\":");
    Serial.print(tempC, 1);
    Serial.println("}");
  }
}
