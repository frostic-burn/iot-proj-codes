#include <Keypad.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>

// ================= WIFI =================
const char* ssid = "dlink_DWR-111";
const char* password = ""; // "" if open network

WebServer server(80);

// ================= KEYPAD =================
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33, 32};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ================= SERVO =================
Servo myServo;
int servoPin = 18;

const int SERVO_OPEN = 110;   // slightly reduced for safety
const int SERVO_CLOSE = 0;

// Smooth movement vars
int currentAngle = 0;
int targetAngle = 0;
unsigned long lastServoUpdate = 0;
const int servoStepDelay = 20; // increase for slower movement

// ================= LOGIC =================
String input = "";
String correctPIN = "1449";

bool isUnlocked = false;
bool servoActive = false;
unsigned long servoStartTime = 0;

// ================= REMAP =================
char remapKey(char k) {
  switch(k) {
    case 'D': return '1';
    case 'C': return '2';
    case 'B': return '3';
    case 'A': return 'A';

    case '#': return '4';
    case '9': return '5';
    case '6': return '6';
    case '3': return 'B';

    case '0': return '7';
    case '8': return '8';
    case '5': return '9';
    case '2': return 'C';

    case '*': return '*';
    case '7': return '0';
    case '4': return '#';
    case '1': return 'D';
  }
  return k;
}

// ================= FRONTEND =================
String page() {
  String state = isUnlocked ? "UNLOCKED" : "LOCKED";

  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Smart Gate</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body {
  font-family: Arial;
  text-align: center;
  background: #f9fafb;
  padding: 30px;
}
.card {
  background: white;
  padding: 30px;
  border-radius: 15px;
  box-shadow: 0 5px 15px rgba(0,0,0,0.1);
  display: inline-block;
}
.state {
  font-size: 22px;
  margin: 20px;
  font-weight: bold;
}
button {
  padding: 15px 25px;
  margin: 10px;
  font-size: 18px;
  border: none;
  border-radius: 8px;
  cursor: pointer;
}
.open { background: #4CAF50; color: white; }
.close { background: #f44336; color: white; }
</style>
</head>
<body>

<div class="card">
  <h1>Smart Gate</h1>
  <div class="state">State: )rawliteral" + state + R"rawliteral(</div>

  <button class="open" onclick="fetch('/open').then(()=>location.reload())">Force Open</button>
  <button class="close" onclick="fetch('/close').then(()=>location.reload())">Force Close</button>
</div>

</body>
</html>
)rawliteral";
}

// ================= WEB =================
void handleRoot() {
  server.send(200, "text/html", page());
}

void handleOpen() {
  targetAngle = SERVO_OPEN;
  isUnlocked = true;

  servoActive = true;
  servoStartTime = millis();

  server.send(200, "text/plain", "Opened");
}

void handleClose() {
  targetAngle = SERVO_CLOSE;
  isUnlocked = false;
  servoActive = false;

  server.send(200, "text/plain", "Closed");
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  myServo.attach(servoPin);
  myServo.write(SERVO_CLOSE);

  currentAngle = SERVO_CLOSE;
  targetAngle = SERVO_CLOSE;

  WiFi.begin(ssid, password);
  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/open", handleOpen);
  server.on("/close", handleClose);

  server.begin();
}

// ================= LOOP =================
void loop() {
  server.handleClient();

  // ===== SMOOTH SERVO =====
  if (millis() - lastServoUpdate > servoStepDelay) {
    lastServoUpdate = millis();

    if (currentAngle < targetAngle) {
      currentAngle++;
      myServo.write(currentAngle);
    } 
    else if (currentAngle > targetAngle) {
      currentAngle--;
      myServo.write(currentAngle);
    }
  }

  // ===== AUTO CLOSE AFTER 5s =====
  if (servoActive && millis() - servoStartTime >= 5000) {
    targetAngle = SERVO_CLOSE;
    isUnlocked = false;
    servoActive = false;

    Serial.println("Auto Closed");
  }

  // ===== KEYPAD =====
  char rawKey = keypad.getKey();

  if (rawKey) {
    char key = remapKey(rawKey);

    Serial.print("Key: ");
    Serial.println(key);

    if (key >= '0' && key <= '9') {
      input += key;
    }

    else if (key == 'A') {
      if (input == correctPIN) {
        targetAngle = SERVO_OPEN;
        isUnlocked = true;

        servoActive = true;
        servoStartTime = millis();

        Serial.println("Correct PIN");
      } else {
        Serial.println("Wrong PIN");
      }

      input = ""; // auto clear
    }

    else if (key == 'B') {
      input = "";
      targetAngle = SERVO_CLOSE;
      isUnlocked = false;
      servoActive = false;

      Serial.println("Reset");
    }

    else if (key == '*') {
      input = "";
    }
  }
}
