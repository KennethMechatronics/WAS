/*
WAS - WiFi Activated Switches
Version 1.0 
K. Paulsen 2025-10-19
*/

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <credentials.h>
#include <LP5817.h>

// credentials.h
const char* ssid = SSID;
const char* WiFiPw = WIFI_PW;
const char* HostName = "WAS";
const char* MQTTserver = MQTT_SERVER;
const char* MQTTuser = MQTT_USER;
const char* MQTTpw = MQTT_PW;

constexpr const char* topicState = "WAS/currentVentilationState";  // what the ESP publishes
constexpr const char* topicCommand = "WAS/setVentilationState";      // what Home Assistant sends to

// Control conectivity
constexpr bool connectivity = true;
bool newMQTTRequestFlag = false;  // flag for new request from MQTT

// RUN-indication (blinking LED)
bool runIndication = true;

// Pin definitions
constexpr int maxSense = 1;
constexpr int GPIO2 = 2;
constexpr int userBtn = 3;
constexpr int bypass1 = 6;
constexpr int bypass2 = 7;
constexpr int GPIO8 = 8;
constexpr int GPIO9 = 9;
constexpr int normalSense = 10;

enum class ventilationState : int {
  Error = -1,
  Min = 0,
  Normal = 1,
  Max = 2
};

// Initial states as minimum at startup
ventilationState currentState = ventilationState::Min;
ventilationState MQTTrequestedState = ventilationState::Min;

WiFiClient espClient;
PubSubClient MQTTclient(espClient);
LP5817 led;

void connectWiFi() {
  constexpr int maxNumberOfTries = 9;
  int WiFiTry = 0;

  led.setPWM(0, 0, 128);            // R,G,B
  led.update();                     // latch PWM regs 0x18..0x1A

  Serial.println(F("Starting WiFi"));
  WiFi.begin(ssid, WiFiPw);

  Serial.print(F("Connecting to ssid: "));
  Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED && WiFiTry <= maxNumberOfTries) {
    WiFiTry++;
    delay(500);
    Serial.print(F("."));
  }

  if (WiFiTry <= maxNumberOfTries) {
    WiFi.setHostname(HostName);
    Serial.println(F(""));          // New line
    Serial.println(F("WiFi connected"));
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());
    Serial.print(F("Signal Strength: "));
    Serial.print(WiFi.RSSI());
    Serial.println(F(" dBm"));

  } else {
    Serial.println(F("\nERROR: WiFi could not connect"));

  }
  led.setPWM(0, 0, 0);              // R,G,B
  led.update();                     // latch PWM regs 0x18..0x1A
}

void connectMQTT() {
  int MQTTTry = 0;
  constexpr int maxNumberOfTries = 9;

  led.setPWM(0, 0, 255);            // R,G,B
  led.update();                     // latch PWM regs 0x18..0x1A

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  Serial.println(F("Connecting to MQTT"));
  MQTTclient.setServer(MQTTserver, 1883);

  while (!MQTTclient.connected() && MQTTTry <= maxNumberOfTries) {
    MQTTTry++;
    // Serial.print(F("MQTT attempt ")); Serial.println(MQTTTry);
    if (MQTTclient.connect(HostName, MQTTuser, MQTTpw)) {
      Serial.println(F("MQTT connected"));
      MQTTclient.subscribe(topicCommand);  // subscribe ONCE here
      Serial.print(F(F("Subscribed to topic: ")));
      Serial.println(topicCommand);


      led.setPWM(0, 0, 0);                 // R,G,B
      led.update();                        // latch PWM regs 0x18..0x1A
      return;
    } else {
      Serial.print(F("MQTT rc="));
      Serial.println(MQTTclient.state());
      delay(500);
    }
  }
  Serial.println(F("\nERROR: MQTT could not connect"));
}

ventilationState getVentilationState() {
  bool normalOptocoupler = digitalRead(normalSense) == HIGH;
  bool maxOptocoupler = digitalRead(maxSense) == HIGH;

  if (!normalOptocoupler && !maxOptocoupler) return ventilationState::Min;
  if (normalOptocoupler && !maxOptocoupler) return ventilationState::Normal;
  if (!normalOptocoupler && maxOptocoupler) return ventilationState::Max;

  return ventilationState::Error; // both HIGH or invalid
}

inline int asInt(ventilationState state) { return static_cast<int>(state); }

inline const char* toText(ventilationState state) {
  switch (state) {
  case ventilationState::Min:    return "Min";
  case ventilationState::Normal: return "Normal";
  case ventilationState::Max:    return "Max";
  default:                       return "Error";
  }
}

int bypassSwitchPanel(ventilationState MQTTrequestedState) {
  int switchOnDuration = 100;
  const int switchOffDuation = 200;
  int switchTries = 0;
  constexpr int maxSwitchTries = 10;

  currentState = getVentilationState();   // This will newer run because the operator is wrong
  if (currentState == MQTTrequestedState) {
    Serial.print(F(F("Info: Already in requested state: ")));
    Serial.println(toText(currentState));
    return 0; // already in requested state
  }

  // Validate args (cast enum class to int for comparisons)
  if (asInt(MQTTrequestedState) < asInt(ventilationState::Min) ||
    asInt(MQTTrequestedState) > asInt(ventilationState::Max) ||
    asInt(currentState) < asInt(ventilationState::Min) ||
    asInt(currentState) > asInt(ventilationState::Max)) {
    Serial.println(F("Error: undefined state"));
    return -1;
  }

  led.setPWM(0, 0, 128); // Blue while switching  
  led.update();

  while (switchTries < maxSwitchTries) {

    digitalWrite(bypass1, HIGH);
    digitalWrite(bypass2, HIGH);
    delay(switchOnDuration);

    MQTTclient.loop();
    yield();

    digitalWrite(bypass1, LOW);
    digitalWrite(bypass2, LOW);
    delay(switchOffDuation);

    MQTTclient.loop();
    yield();

    switchTries++;
    switchOnDuration += 50; // increase duration to ensure contact

    Serial.print(F("Switch try #"));
    Serial.print(switchTries);
    Serial.print(F("  |  Current state: "));
    Serial.println(toText(currentState));

    currentState = getVentilationState();
    if (currentState == MQTTrequestedState) {
      // success
      led.setPWM(0, 0, 0);
      led.update();

      MQTTclient.publish(topicState, toText(currentState), true);
      Serial.print(F("Success: Reached requested state: "));
      Serial.println(toText(currentState));

      return 0;
    }
  }

  // if failed
  led.setPWM(128, 0, 0);
  led.update();
  Serial.println(F("Error: Max number of switch tries reached"));
  return -2;
}

void callback(char* topic, byte* payload, unsigned int length) {
  String receivedTopic = topic;
  String command;
  for (unsigned int i = 0; i < length; i++) {
    command += (char)payload[i];
  }
  command.trim();
  command.toUpperCase();  // accept "min", "Min", etc.

  Serial.print(F("MQTT message received on ["));
  Serial.print(receivedTopic);
  Serial.print(F("]: "));
  Serial.println(command);


  // Only handle messages on the expected command topic
  if (receivedTopic == topicCommand) {
    // Map command text to internal state
    if (command == "MIN") {
      MQTTrequestedState = ventilationState::Min;
    } else if (command == "NORMAL") {
      MQTTrequestedState = ventilationState::Normal;
    } else if (command == "MAX") {
      MQTTrequestedState = ventilationState::Max;
    }

    Serial.print(F("Home Assistant requested ventilation: "));
    Serial.println(command);
    newMQTTRequestFlag = true;

  } else {
    Serial.print(F("Warning: message on unknown topic: "));
    Serial.println(receivedTopic);
  }

}

void publishStateToMQTT(int scheduledOrUnscheduled) {
  String currentStateText = toText(getVentilationState());
  MQTTclient.publish(topicState, currentStateText.c_str(), true);
  if (scheduledOrUnscheduled == 0) {
    Serial.print(F("Periodic transmission to MQTT: "));
    Serial.println(currentStateText);
  } else {
    Serial.print(F("Manual transmission to MQTT: "));
    Serial.println(currentStateText);
  }

}

void animateRGB() {
  constexpr int fadeDelay = 2; // ms
  // Fade G -> B
  for (int i = 0; i <= 255; i++) {
    led.setPWM(0, 255 - i, i);  // Green down, Blue up
    led.update();
    delay(fadeDelay);
  }
  // Fade B -> R
  for (int i = 0; i <= 255; i++) {
    led.setPWM(i, 0, 255 - i);  // Blue down, Red up
    led.update();
    delay(fadeDelay);
  }
  // Fade R -> G
  for (int i = 0; i <= 255; i++) {
    led.setPWM(255 - i, i, 0);  // Red down, Green up
    led.update();
    delay(fadeDelay);
  }
}

bool ledRGB(int R, int G, int B, bool ledState) {
  if (ledState) {
    led.setPWM(R, G, B);              // R,G,B
  } else {
    led.setPWM(0, 0, 0);              // R,G,B
  }
  led.update();                       // latch PWM regs 0x18..0x1A
  return !ledState;

}

void stateDependentLED(bool ledState) {
  if (currentState != MQTTrequestedState) {
    ledRGB(128, 0, 0, ledState);    // Red if not in requested state
  } else {
    ledRGB(0, 128, 0, ledState);    // Green if in requested state
  }
}

void setup() {
  delay(500);                       // Wait for PSU to settle
  Serial.begin(115200);
  delay(100);
  Serial.print(F("Serial started \nBaud rate: "));
  Serial.println(Serial.baudRate());

  Serial.print(F("Coniguring GPIO"));
  pinMode(maxSense, INPUT_PULLDOWN);
  pinMode(normalSense, INPUT_PULLDOWN);
  pinMode(userBtn, INPUT_PULLUP);   // not used, avoid floating (physical pull-up on board)
  Serial.print(F("."));
  pinMode(bypass1, OUTPUT);
  pinMode(bypass2, OUTPUT);
  Serial.print(F("."));
  pinMode(GPIO2, INPUT_PULLDOWN);   // not used, avoid floating
  pinMode(GPIO8, INPUT_PULLDOWN);   // not used, avoid floating
  pinMode(GPIO9, INPUT_PULLDOWN);   // not used, avoid floating
  Serial.println(F("."));

  digitalWrite(bypass1, LOW);
  digitalWrite(bypass2, LOW);
  Serial.println(F("GPIO configured"));

  Serial.println(F("Configuring I2C"));
  Wire.begin(5, 4);                 // SDA, SCL
  Wire.setClock(400000);            // 400kHz
  Serial.print(F("I2C configured. Frequency: "));
  Serial.print(Wire.getClock());
  Serial.println(F(" Hz"));

  Serial.println(F("Configuring LED driver"));
  led.begin();                      // enables chip & clears POR
  led.setMaxCurrent(false);         // 25.5 mA range (true => 51 mA)
  led.setExponential(true, true, true);
  led.setDC(70, 3, 70);             // analog current per channel (0..255)
  led.update();                     // latch config regs 0x01..0x04
  led.enableOutputs(0x07);          // enable OUT0..2
  led.setPWM(0, 0, 255);            // R,G,B
  led.update();                     // latch PWM regs 0x18..0x1A
  Serial.println(F("LED driver configured"));

  if (connectivity) {
    Serial.println(F("Connectivity enabled"));
    connectWiFi();
    MQTTclient.setCallback(callback);// set first
    connectMQTT();                   // then connect (and subscribe inside)
  } else {
    Serial.println(F("Connectivity disabled"));
  }

  animateRGB();

  Serial.println(F("Setup done"));
  delay(200);
}

void loop() {
  constexpr unsigned long transmissionInterval = 15000;
  static unsigned long prevTransmission = 0;

  constexpr unsigned long requestTimeoutThreshold = 5000;
  static unsigned long prevRequestTime = 0;

  unsigned long currentMillis = millis();

  if (connectivity) {
    if (!MQTTclient.connected()) {
      Serial.println(F("Info: MQTT not connected, trying to reconnect"));
      connectMQTT();
    }

    MQTTclient.loop();

    if (getVentilationState() != currentState) {
      // state changed outside WAS control (physical button on panel) 
      publishStateToMQTT(1);  // Manual
      currentState = getVentilationState();
    }

    currentMillis = millis();
    if (currentMillis - prevTransmission >= transmissionInterval) {      // Publish current state periodically to MQTT
      publishStateToMQTT(0);  // Periodic
      prevTransmission = currentMillis;
    }

    if (!newMQTTRequestFlag) {
      MQTTrequestedState = getVentilationState(); // sync requested state if no pending MQTT request

    } else if (newMQTTRequestFlag && (millis() - prevRequestTime >= requestTimeoutThreshold)) {
      // Only process pending MQTT request if timeout since last try has passed

      currentState = getVentilationState();
      if (MQTTrequestedState != currentState) {

        if (bypassSwitchPanel(MQTTrequestedState) < 0) {
          String errormsg = "Error";
          MQTTclient.publish(topicState, errormsg.c_str());

          newMQTTRequestFlag = false; // give up on error
          prevRequestTime = millis();

        } else {
          prevRequestTime = millis();
          newMQTTRequestFlag = false;
        }

      }
    }
  } else {
    currentState = getVentilationState();
  }

  // runstate-indication (blinking LED)
  if (runIndication) {
    constexpr int blinkDuration = 100; // ms
    constexpr int blinkPause = 4900;   // ms  
    static unsigned long prevBlink = 0;
    static int blinkInterval = 100;
    static bool ledState = false;

    currentMillis = millis();
    if (currentMillis - prevBlink >= blinkInterval) {
      ledState = !ledState;
      stateDependentLED(ledState);    // Red or Green depending on state. Red can mean out of sync with physical panel
      blinkInterval = ledState ? blinkDuration : blinkPause; // Update timing
      prevBlink = currentMillis;
    }
  }
}
