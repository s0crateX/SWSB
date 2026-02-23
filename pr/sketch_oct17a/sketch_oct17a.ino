#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ====== Wi-Fi Access Point credentials ======
// The ESP32 creates its own WiFi network. Connect your phone/laptop to it.
const char* ssid = "SmartBin";
const char* password = "smartbin123";

// Web server
WebServer server(80);

// Mode variable
bool autoMode = false; // false = manual, true = automatic

// Counters
unsigned long bioCount = 0;
unsigned long nonBioCount = 0;

// ====== Pin assignments ======
const int capPin = 21;    // capacitive sensor digital output
const int irPin = 19;     // IR sensor digital output
const int buttonPin = 4;  // optional physical pushbutton for manual trigger
const int servo1Pin = 13; // left flap servo (paper)
const int servo2Pin = 14; // right flap servo (plastic)

// ====== Servo angles (adjust!) ======
// Servo1 (left flap - paper)
int servo1Home = 0; 
int servo1Left = 90; 

// Servo2 (right flap - plastic)
int servo2Home = 90; 
int servo2Right = 180; 

// Speed control (ms per degree)
int stepDelay = 10;

// Servo objects
Servo servo1;
Servo servo2;

// Current positions
int servo1Current = servo1Home;
int servo2Current = servo2Home;

// ====== Function prototypes ======
void classifyAndSort(int capVal, int irVal);
void sortPaper();
void sortPlastic();
void slowMoveServo(Servo &s, int &currentAngle, int targetAngle);
String htmlPage();

// ====== HTML page served to browser ======
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

  pinMode(capPin, INPUT_PULLUP);
  pinMode(irPin, INPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  // Attach servos
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo1.attach(servo1Pin, 500, 2400);
  servo2.attach(servo2Pin, 500, 2400);
  servo1.write(servo1Home);
  servo2.write(servo2Home);

  // Start Wi-Fi as Access Point (no router needed)
  WiFi.softAP(ssid, password);
  Serial.println("Access Point started!");
  Serial.print("Connect to WiFi: ");
  Serial.println(ssid);
  Serial.print("Then open browser to: http://");
  Serial.println(WiFi.softAPIP());

  // Web server handlers
  server.on("/", []() {
    server.send(200, "text/html", htmlPage());
  });
  server.on("/autoOn", []() {
    autoMode = true;
    server.send(200, "text/plain", "Automatic mode ON");
  });
  server.on("/autoOff", []() {
    autoMode = false;
    server.send(200, "text/plain", "Manual mode ON");
  });
  server.on("/reset", []() {
    bioCount = 0;
    nonBioCount = 0;
    server.send(200, "text/plain", "Counters reset");
  });
  server.on("/sort", []() {
    int capVal = digitalRead(capPin);
    int irVal = digitalRead(irPin);
    classifyAndSort(capVal, irVal);
    server.send(200, "text/plain", "Item sorted");
  });

  server.begin();
}

void loop() {
  server.handleClient();

  int capVal = digitalRead(capPin); // 0 = detected
  int irVal = digitalRead(irPin);   // 0 = detected

  // Manual trigger
  if (!autoMode && digitalRead(buttonPin) == LOW) {
    Serial.println("Manual button pressed");
    classifyAndSort(capVal, irVal);
    delay(1000);
  }

  // Automatic mode
  if (autoMode) {
    if (capVal == LOW || irVal == 0) {  // only sort when a sensor detects something
      classifyAndSort(capVal, irVal);
      delay(2000);
    }
  }
}

// ====== Sorting decision ======
void classifyAndSort(int capVal, int irVal) {
  Serial.print("CAP: ");
  Serial.print(capVal);
  Serial.print(" | IR: ");
  Serial.println(irVal);

  if (capVal == LOW) {
    Serial.println("Plastic detected → Servo2 (right)");
    sortPlastic();
    nonBioCount++;
  } else {
    if (irVal == 0) {
      Serial.println("Paper detected → Servo1 (left)");
      sortPaper();
      bioCount++;
    } else {
      Serial.println("No item detected, skipping.");
      return;
    }
  }
}

// ====== Servo movement helper ======
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

// ====== Servo actions ======
void sortPaper() {
  slowMoveServo(servo1, servo1Current, servo1Left);
  delay(500);  // let item fall
  slowMoveServo(servo1, servo1Current, servo1Home);
}

void sortPlastic() {
  slowMoveServo(servo2, servo2Current, servo2Right);
  delay(500);  // let item fall
  slowMoveServo(servo2, servo2Current, servo2Home);
}

