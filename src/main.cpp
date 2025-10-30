/*
  6-DOF Servo Controller over Bluetooth (single-servo active policy)

  Protocol (via Bluetooth on HC-06 @ 9600 baud):
    - Send commands as:  SERVOANGLE   e.g.,  A120  or  F90
      * SERVO is a letter A..F (see pin map below)
      * ANGLE is clamped per-servo to its safe range (see ranges below)
    - '?'  -> prints HELP with per-servo ranges and examples
    - 'G'  -> prints STATUS with current angles and speed

  Pin map and safe ranges (documented 2025-10-30):
    A @ D4:  0–180
    B @ D5:  45–90
    C @ D6:  0–85
    D @ D7:  90–180
    E @ D8:  0–180
    F @ D9:  90–180

  Power strategy:
    - To avoid brown-outs and excessive current draw, only one servo is attached
      (powered) at a time. After reaching its target, it is detached. This is a
      software mitigation when bulk capacitance is unavailable.
*/

#include <Arduino.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// --- SERVO CONFIGURATION ---
const int NUM_SERVOS = 6;
const char SERVO_NAMES[] = {'A', 'B', 'C', 'D', 'E', 'F'};
const int SERVO_PINS[] = {4, 5, 6, 7, 8, 9};

// --- SERVO SAFE OPERATING RANGES ---
// As documented and tested on 2025-10-30
const int minAngles[] = {0, 45, 0, 90, 0, 90};
const int maxAngles[] = {180, 90, 85, 180, 180, 180};
// ------------------------------------

// --- GLOBAL STATE ---
Servo servos[NUM_SERVOS];
int currentAngles[NUM_SERVOS];
int targetAngles[NUM_SERVOS];

int moveSpeed = 20; // Delay in ms for each degree step. Higher = slower.
#define LED_PIN LED_BUILTIN
SoftwareSerial BTSerial(2, 3); // RX, TX
// Only one servo may be active (attached/moving) at a time
int activeServo = -1;
unsigned long lastInfoMs = 0;
unsigned long lastMoveMs = 0; // Track last movement time for non-blocking

// Command buffer for Bluetooth input
char btCommandBuffer[32];
int btBufferIndex = 0;

// Forward declarations
bool processCommand(char* cmdStr, Stream &out);

// Helpers
void detachAllExcept(int keepIdx) {
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (i != keepIdx && servos[i].attached()) {
      servos[i].detach();
    }
  }
}

void attachIfNeeded(int idx) {
  if (!servos[idx].attached()) {
    servos[idx].attach(SERVO_PINS[idx]);
  }
}

void setup() {
  // Start serial communications at the confirmed 9600 baud rate
  Serial.begin(9600);
  BTSerial.begin(9600);

  pinMode(LED_PIN, OUTPUT);

  // Initialize angles but DO NOT keep servos attached (power-saving)
  for (int i = 0; i < NUM_SERVOS; i++) {
    currentAngles[i] = minAngles[i];
    targetAngles[i] = minAngles[i];
    // Note: we attach only when moving a specific servo
  }

  // Ready messages
  Serial.println("--- 6-DOF Servo Control Ready ---");
  Serial.println("Send 'SERVOANGLE' (e.g., A90). Send '?' for HELP. Send 'G' for STATUS.");
  BTSerial.println("6-DOF Control Ready. Use 'SERVOANGLE' (e.g., A90). '?'=HELP, 'G'=STATUS");
  lastInfoMs = millis();
}

void loop() {
  // --- Command Parsing (Bluetooth) - Buffer until newline ---
  // ALWAYS process incoming commands, even while moving
  while (BTSerial.available() > 0) {
    char c = BTSerial.read();
    
    // Check for end of line
    if (c == '\n' || c == '\r') {
      if (btBufferIndex > 0) {
        btCommandBuffer[btBufferIndex] = '\0'; // Null terminate
        processCommand(btCommandBuffer, BTSerial);
        btBufferIndex = 0; // Reset buffer
        lastInfoMs = millis();
      }
    } else if (btBufferIndex < 31) {
      // Add to buffer
      btCommandBuffer[btBufferIndex++] = c;
    }
  }

  // --- Smooth Movement Logic (single-servo, NON-BLOCKING) ---
  if (activeServo != -1) {
    // Only move if enough time has passed (non-blocking delay)
    if (millis() - lastMoveMs >= (unsigned long)moveSpeed) {
      lastMoveMs = millis();
      
      int i = activeServo;
      if (currentAngles[i] != targetAngles[i]) {
        if (currentAngles[i] < targetAngles[i]) currentAngles[i]++;
        else currentAngles[i]--;
        servos[i].write(currentAngles[i]);
      } else {
        // Reached target: detach to reduce power draw
        if (servos[i].attached()) servos[i].detach();
        activeServo = -1;
        digitalWrite(LED_PIN, LOW);
        char doneBuf[32];
        sprintf(doneBuf, "DONE:%c=%d", SERVO_NAMES[i], currentAngles[i]);
        Serial.println(doneBuf);
        BTSerial.println(doneBuf);
      }
    }
  }
  
  // Periodic READY hint (non-intrusive), every ~5s when idle
  if (activeServo == -1 && (millis() - lastInfoMs) > 5000UL) {
    BTSerial.println("READY: Use A..F+angle (e.g., A90, F150), '?' for HELP, 'G' for STATUS");
    Serial.println("READY: Use A..F+angle (e.g., A90, F150), '?' for HELP, 'G' for STATUS");
    lastInfoMs = millis();
  }
}

// Parse and execute a single command from a string. Returns true if something was handled.
bool processCommand(char* cmdStr, Stream &out) {
  if (cmdStr[0] == '\0') return false; // Empty command
  
  char u = toupper(cmdStr[0]);

  // HELP
  if (u == '?' || u == 'H') {
    out.println("HELP: Send SERVOANGLE (e.g., A90 or F150). Ranges:");
    out.print(" A: "); out.print(minAngles[0]); out.print('-'); out.println(maxAngles[0]);
    out.print(" B: "); out.print(minAngles[1]); out.print('-'); out.println(maxAngles[1]);
    out.print(" C: "); out.print(minAngles[2]); out.print('-'); out.println(maxAngles[2]);
    out.print(" D: "); out.print(minAngles[3]); out.print('-'); out.println(maxAngles[3]);
    out.print(" E: "); out.print(minAngles[4]); out.print('-'); out.println(maxAngles[4]);
    out.print(" F: "); out.print(minAngles[5]); out.print('-'); out.println(maxAngles[5]);
    return true;
  }

  // STATUS
  if (u == 'G') {
    out.print("STATUS:");
    for (int i = 0; i < NUM_SERVOS; i++) {
      out.print(' ');
      out.print(SERVO_NAMES[i]);
      out.print('=');
      out.print(currentAngles[i]);
    }
    out.print(" | SPEED="); out.println(moveSpeed);
    return true;
  }

  // SERVOANGLE - first char is servo, rest is number
  int servoIndex = -1;
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (u == SERVO_NAMES[i]) { servoIndex = i; break; }
  }
  
  if (servoIndex != -1 && strlen(cmdStr) > 1) {
    // Parse the number starting from position 1
    int newAngle = atoi(cmdStr + 1);
    
    if (newAngle >= minAngles[servoIndex] && newAngle <= maxAngles[servoIndex]) {
      // If switching to a different servo while one is active, save current position
      if (activeServo != -1 && activeServo != servoIndex) {
        // The previous servo didn't finish - keep track of where it stopped
        if (servos[activeServo].attached()) {
          servos[activeServo].detach();
        }
      }
      
      detachAllExcept(servoIndex);
      attachIfNeeded(servoIndex);
      targetAngles[servoIndex] = newAngle;
      activeServo = servoIndex;
      char buffer[64];
      sprintf(buffer, "OK:%c->%d (range %d-%d)", SERVO_NAMES[servoIndex], newAngle, minAngles[servoIndex], maxAngles[servoIndex]);
      out.println(buffer);
      Serial.println(buffer);
      digitalWrite(LED_PIN, HIGH);
    } else {
      char buffer[64];
      sprintf(buffer, "ERR:%c range %d-%d", SERVO_NAMES[servoIndex], minAngles[servoIndex], maxAngles[servoIndex]);
      out.println(buffer);
      Serial.println(buffer);
    }
    return true;
  }
  
  // Unknown
  out.print("ERR:Unknown cmd '"); out.print(cmdStr); out.println("'");
  return true;
}