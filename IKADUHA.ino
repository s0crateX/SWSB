#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

/*
===============================================================================
Smart Waste Sorting Bin (Documentation)
===============================================================================

PROJECT GOAL:
Automate ang pag sort ng basura into:
1) Paper / Biodegradable  -> left flap (Servo 1)
2) Plastic / Non-bio      -> right flap (Servo 2)

HIGH-LEVEL FLOW:
1. setup()
   - I set ang pins, servo, and WiFi Access Point.
   - I start ang web server and i register ang endpoints/buttons.
2. loop()
   - Laging tumatanggap ng web requests (buttons sa phone/laptop).
   - Binabasa ang sensors.
   - Kapag manual mode: mag sort lang via physical button o web button.
   - Kapag automatic mode: mag sort kapag may na detect ang sensor.
3. classifyAndSort(capVal, irVal)
   - Decision logic kung paper, plastic, o walang item.
4. sortPaper()/sortPlastic()
   - Gumagalaw ang tamang servo para mag Open/close ang flap.

IMPORTANT BEHAVIOR:
- Priority ng detection: Capacitive muna, then IR.
  Ibig sabihin kung parehong active, plastic branch (capacitive) ang mauuna.
- Walang detection = walang servo movement (skip).
- ESP32 ang gumagawa ng sariling WiFi network (no router needed).

===============================================================================
*/

// ====== Wi-Fi Access Point credentials ======
// Dito kumokonekta ang phone/laptop para sa web control page.
const char* ssid = "SmartBin";
const char* password = "smartbin123";

// Web server sa port 80 (default HTTP port)
WebServer server(80);

// Mode flag:
// false = Manual mode
// true  = Automatic mode
bool autoMode = false;
bool prevAutoMode = false;

// Counters na pinapakita sa web UI
unsigned long bioCount = 0;     // paper count
unsigned long nonBioCount = 0;  // plastic count

// ====== Pin assignments ======
// Sensors
const int capPin = 21;    // capacitive sensor digital output
const int irPin = 19;     // IR sensor digital output
// Input button
const int buttonPin = 4;  // optional physical pushbutton for manual trigger
// Servo outputs
const int servo1Pin = 13; // left flap servo (paper)
const int servo2Pin = 14; // right flap servo (plastic)

// ====== Sensor detect logic (IMPORTANT) ======
// Iba-iba ang behavior ng sensor modules:
// - Some modules: LOW = detect
// - Others (e.g. some capacitive boards): HIGH = detect
//
// If laging wrong ang sorting, usually dito lang kailangan baguhin.
const int CAP_DETECT_STATE = HIGH; // set to LOW if your capacitive sensor is active-LOW
const int IR_DETECT_STATE = LOW;   // most IR obstacle sensors are active-LOW

// ====== Auto mode anti-false-trigger settings ======
// Goal: iwas tuloy-tuloy na sort kapag "stuck detected" ang sensor.
const unsigned long DETECT_STABLE_MS = 120;   // detection must stay true this long
const unsigned long AUTO_COOLDOWN_MS = 2000;  // minimum gap between auto sorts

// ====== Servo angles (i calibrate based sa physical build) ======
// Servo1 (left flap - paper)
int servo1Home = 0;
int servo1Left = 90;

// Servo2 (right flap - plastic)
int servo2Home = 90;
int servo2Right = 180;

// Servo speed control: mas mataas = mas mabagal
int stepDelay = 10; // ms per degree

// Servo objects
Servo servo1;
Servo servo2;

// Track current servo positions para smooth ang paggalaw
int servo1Current = servo1Home;
int servo2Current = servo2Home;

// Auto mode state trackers
bool autoTriggerLatched = false;    // true = wait muna ng clear state bago next sort
unsigned long detectStartMs = 0;    // start time ng current detect window
unsigned long lastAutoSortMs = 0;   // last auto sort timestamp

// ====== Function prototypes ======
void classifyAndSort(int capVal, int irVal);
void sortPaper();
void sortPlastic();
void slowMoveServo(Servo &s, int &currentAngle, int targetAngle);
bool isCapDetected(int capVal);
bool isIrDetected(int irVal);
bool isAnyDetected(int capVal, int irVal);
bool isSingleSensorDetected(int capVal, int irVal);
String htmlPage();

// ====== Web page template ======
// Ito ang simpleng HTML dashboard na makikita sa http://192.168.4.1
String htmlPage() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Smart Bin</title></head><body>";
  html += "<h2>Smart Bin Control</h2>";
  html += "<p>Mode: <b>" + String(autoMode ? "Automatic" : "Manual") + "</b></p>";
  html += "<button onclick=\"fetch('/autoOn').then(()=>location.reload())\">Automatic Mode</button>";
  html += "<button onclick=\"fetch('/autoOff').then(()=>location.reload())\">Manual Mode</button>";
  html += "<button onclick=\"fetch('/sort').then(()=>location.reload())\">Sort Item</button>";
  html += "<h3>Counters</h3>";
  html += "<p>Biodegradable (Paper): " + String(bioCount) + "</p>";
  html += "<p>Non-Biodegradable (Plastic): " + String(nonBioCount) + "</p>";
  html += "<button onclick=\"fetch('/reset').then(()=>location.reload())\">Reset Counters</button>";
  html += "</body></html>";
  return html;
}

void setup() {
  Serial.begin(115200);

  // Sensor/button pin modes
  pinMode(capPin, INPUT_PULLUP);
  pinMode(irPin, INPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  Serial.print("CAP_DETECT_STATE: ");
  Serial.println(CAP_DETECT_STATE == HIGH ? "HIGH" : "LOW");
  Serial.print("IR_DETECT_STATE: ");
  Serial.println(IR_DETECT_STATE == HIGH ? "HIGH" : "LOW");

  // Servo setup
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo1.attach(servo1Pin, 500, 2400);
  servo2.attach(servo2Pin, 500, 2400);
  servo1.write(servo1Home);
  servo2.write(servo2Home);

  // WiFi Access Point mode:
  // ESP32 creates its own WiFi network (no router needed).
  WiFi.softAP(ssid, password);
  Serial.println("Access Point started!");
  Serial.print("Connect to WiFi: ");
  Serial.println(ssid);
  Serial.print("Then open browser to: http://");
  Serial.println(WiFi.softAPIP());

  // ====== Web server routes ======
  // Home page
  server.on("/", []() {
    server.send(200, "text/html", htmlPage());
  });

  // Switch to automatic mode
  server.on("/autoOn", []() {
    autoMode = true;
    server.send(200, "text/plain", "Automatic mode ON");
  });

  // Switch to manual mode
  server.on("/autoOff", []() {
    autoMode = false;
    server.send(200, "text/plain", "Manual mode ON");
  });

  // Reset counters
  server.on("/reset", []() {
    bioCount = 0;
    nonBioCount = 0;
    server.send(200, "text/plain", "Counters reset");
  });

  // Manual one-time sort trigger from web button
  server.on("/sort", []() {
    int capVal = digitalRead(capPin);
    int irVal = digitalRead(irPin);
    classifyAndSort(capVal, irVal);
    server.send(200, "text/plain", "Item sorted");
  });

  server.begin();
}

void loop() {
  // Laging i handle ang incoming web requests
  server.handleClient();

  // read current sensor state
  // Detection is based on CAP_DETECT_STATE / IR_DETECT_STATE config above.
  int capVal = digitalRead(capPin);
  int irVal = digitalRead(irPin);
  bool capDetected = isCapDetected(capVal);
  bool irDetected = isIrDetected(irVal);
  bool singleDetected = isSingleSensorDetected(capVal, irVal);
  bool anyDetected = isAnyDetected(capVal, irVal);

  // Track mode transition para ma-arm ng tama ang automatic mode.
  if (autoMode && !prevAutoMode) {
    // Kapag pagpasok ng auto mode ay detected na agad, huwag munang mag-sort.
    // Hintayin munang mag-clear then new detect event.
    autoTriggerLatched = anyDetected;
    detectStartMs = anyDetected ? millis() : 0;
    Serial.println(anyDetected
      ? "Auto mode ON: sensor already detected, waiting for clear state."
      : "Auto mode ON: armed.");
  } else if (!autoMode && prevAutoMode) {
    autoTriggerLatched = false;
    detectStartMs = 0;
  }
  prevAutoMode = autoMode;

  // ====== Manual mode behavior ======
  // Sa manual mode, sorting only happens kapag pinindot ang physical button.
  if (!autoMode && digitalRead(buttonPin) == LOW) {
    Serial.println("Manual button pressed");
    classifyAndSort(capVal, irVal);
    delay(1000); // basic debounce / anti-repeat delay
  }

  // ====== Automatic mode behavior ======
  // Mag-sort lang sa NEW + STABLE detection, then wait for clear state.
  // IMPORTANT: auto-sort only works when EXACTLY ONE sensor is active.
  // If both sensors are active, ambiguous state siya (skip).
  if (autoMode) {
    if (!singleDetected) {
      autoTriggerLatched = false;
      detectStartMs = 0;
      if (capDetected && irDetected) {
        // Both active usually means mounting/reflection issue (e.g., sensor seeing plywood/hole).
        // Skip sorting to avoid wrong flap movement.
        delay(50);
      }
    } else {
      if (detectStartMs == 0) {
        detectStartMs = millis();
      }

      bool stableDetect = (millis() - detectStartMs) >= DETECT_STABLE_MS;
      bool cooldownDone = (millis() - lastAutoSortMs) >= AUTO_COOLDOWN_MS;

      if (!autoTriggerLatched && stableDetect && cooldownDone) {
        classifyAndSort(capVal, irVal);
        lastAutoSortMs = millis();
        autoTriggerLatched = true; // require clear state before next auto sort
      }
    }
  }
}

bool isAnyDetected(int capVal, int irVal) {
  return isCapDetected(capVal) || isIrDetected(irVal);
}

bool isSingleSensorDetected(int capVal, int irVal) {
  bool capDetected = isCapDetected(capVal);
  bool irDetected = isIrDetected(irVal);
  return capDetected ^ irDetected;
}

// ====== Core decision logic ======
// Decision order:
// 1) Capacitive detected -> treat as plastic
// 2) Else if IR detected -> treat as paper
// 3) Else                       -> no item, skip
void classifyAndSort(int capVal, int irVal) {
  bool capDetected = isCapDetected(capVal);
  bool irDetected = isIrDetected(irVal);

  Serial.print("CAP: ");
  Serial.print(capVal);
  Serial.print(" | IR: ");
  Serial.println(irVal);

  if (capDetected && irDetected) {
    Serial.println("Ambiguous: BOTH sensors active. Skipping sort.");
    return;
  }

  if (capDetected) {
    Serial.println("Plastic detected -> Servo2 (right)");
    sortPlastic();
    nonBioCount++;
  } else {
    if (irDetected) {
      Serial.println("Paper detected -> Servo1 (left)");
      sortPaper();
      bioCount++;
    } else {
      Serial.println("No item detected, skipping.");
      return;
    }
  }
}

bool isCapDetected(int capVal) {
  return capVal == CAP_DETECT_STATE;
}

bool isIrDetected(int irVal) {
  return irVal == IR_DETECT_STATE;
}

// ====== Smooth servo movement helper ======
// Slowly nya i move ang servo from currentAngle to targetAngle.
void slowMoveServo(Servo &s, int &currentAngle, int targetAngle) {
  if (currentAngle < targetAngle) {
    for (int pos = currentAngle; pos <= targetAngle; pos++) {
      s.write(pos);
      delay(stepDelay);
    }
  } else {
    for (int pos = currentAngle; pos >= targetAngle; pos--) {
      s.write(pos);
      delay(stepDelay);
    }
  }
  currentAngle = targetAngle;
}

// ====== Servo action: paper ======
// Open left flap, wait for item to fall, close flap.
void sortPaper() {
  slowMoveServo(servo1, servo1Current, servo1Left);
  delay(500);
  slowMoveServo(servo1, servo1Current, servo1Home);
}

// ====== Servo action: plastic ======
// Open right flap, wait for item to fall, close flap.
void sortPlastic() {
  slowMoveServo(servo2, servo2Current, servo2Right);
  delay(500);
  slowMoveServo(servo2, servo2Current, servo2Home);
}
