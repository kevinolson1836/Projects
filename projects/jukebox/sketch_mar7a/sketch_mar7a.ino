/**
 * ESP32 WiFi Jukebox Controller
 * DFPlayer Mini + NFC + Web Interface
 */

#include <WiFi.h>
#include <DFRobotDFPlayerMini.h>
#include <Wire.h>
#include <Adafruit_PN532.h>

// ===== WIFI =====
const char* ssid = "It Hurts When IP";
const char* password = "Getsomemeds!";

// ===== NFC I2C =====
#define SDA_PIN 22
#define SCL_PIN 21

Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);

// ===== TAG MEMORY =====
String lastTagID = "";
unsigned long lastTagTime = 0;

// ===== KNOWN TAGS =====
String C13A = "5347D85D220001";
String C13B = "53A7475D220001";
String C13C = "5326D15D220001";
String C13D = "53653C5D220001";

// ===== WEB SERVER =====
WiFiServer server(80);

// ===== DFPLAYER =====
HardwareSerial dfSerial(2);
DFRobotDFPlayerMini dfplayer;

int volumeLevel = 20;

// ===== TIMERS =====
unsigned long lastNFCTime = 0;

void printDFStatus(uint8_t type, int value);

void setup() {

  Serial.begin(115200);
  delay(2000);

  Serial.println("\n==========================");
  Serial.println("ESP32 JUKEBOX BOOT");
  Serial.println("==========================");

  // ===== I2C START =====
  Serial.println("Starting I2C...");
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println("I2C Started");

  // ===== DFPLAYER =====
  Serial.println("\nStarting DFPlayer serial...");
  dfSerial.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println("Initializing DFPlayer...");

  if (!dfplayer.begin(dfSerial)) {

    Serial.println("❌ DFPlayer FAILED");

  } else {

    Serial.println("✅ DFPlayer online");

    dfplayer.volume(volumeLevel);

    Serial.print("Files on SD: ");
    Serial.println(dfplayer.readFileCounts());

    Serial.print("Folders on SD: ");
    Serial.println(dfplayer.readFolderCounts());
  }

  // ===== NFC =====
  Serial.println("\nInitializing NFC Reader...");

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();

  if (!versiondata) {

    Serial.println("❌ PN532 NOT DETECTED");

  } else {

    Serial.println("✅ PN532 detected");

    Serial.print("Firmware: ");
    Serial.print((versiondata >> 16) & 0xFF);
    Serial.print(".");
    Serial.println((versiondata >> 8) & 0xFF);

    nfc.SAMConfig();
    Serial.println("NFC ready.");
  }

  // ===== WIFI =====
  Serial.println("\nConnecting to WiFi...");

  WiFi.begin(ssid, password);

  int timeout = 20;

  while (WiFi.status() != WL_CONNECTED && timeout--) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.isConnected()) {

    Serial.println("\n✅ WiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

  } else {

    Serial.println("\n⚠ WiFi failed — starting AP");

    WiFi.softAP("Jukebox", "12345678");

    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }

  server.begin();
  Serial.println("Web server started");

  Serial.println("\nSYSTEM READY");
}

void loop() {

  // ===== DFPLAYER EVENTS =====
  if (dfplayer.available()) {
    printDFStatus(dfplayer.readType(), dfplayer.read());
  }

  // ===== NFC SCANNING =====
  if (millis() - lastNFCTime > 300) {

    lastNFCTime = millis();

    uint8_t uid[7];
    uint8_t uidLength;

    bool success = nfc.readPassiveTargetID(
      PN532_MIFARE_ISO14443A,
      uid,
      &uidLength,
      50
    );

    if (!success) return;

    Serial.println("CARD DETECTED!");

    Serial.print("UID: ");

    String tagID = "";

    for (int i = 0; i < uidLength; i++) {

      Serial.print(uid[i], HEX);
      Serial.print(" ");

      if (uid[i] < 0x10) tagID += "0";
      tagID += String(uid[i], HEX);
    }

    Serial.println();

    tagID.toUpperCase();

    Serial.print("TagID Parsed: ");
    Serial.println(tagID);

    // ===== 10 SECOND DUPLICATE FILTER (FIXED) =====
    if (tagID == lastTagID) {

      if (millis() - lastTagTime < 10000) {

        Serial.println("Same tag seen again — resetting 10s timer");
        lastTagTime = millis();   // RESET TIMER
        return;

      }
    }

    // ===== TAG ACTIONS =====
    if (tagID == C13A || tagID == C13B || tagID == C13C || tagID == C13D) {

      Serial.println("MATCHED TAG → Playing C13 (YELLOW DISK)");
      dfplayer.play(1);

    } 
    // else if (tagID == knownTag2) {

    //   Serial.println("MATCHED TAG → Playing Track 3");
    //   dfplayer.play(3);

    // } 
    else {

      Serial.println("UNKNOWN TAG");
    }

    // ===== STORE LAST TAG INFO =====
    lastTagID = tagID;
    lastTagTime = millis();
  }

  // ===== WEB SERVER =====
  WiFiClient client = server.available();

  if (client) {

    Serial.println("Web client connected");

    String request = client.readStringUntil('\r');
    client.readStringUntil('\n');

    Serial.print("HTTP Request: ");
    Serial.println(request);

    if (request.indexOf("/play") != -1) dfplayer.start();
    if (request.indexOf("/pause") != -1) dfplayer.pause();
    if (request.indexOf("/next") != -1) dfplayer.next();
    if (request.indexOf("/prev") != -1) dfplayer.previous();

    if (request.indexOf("/volup") != -1) {

      if (volumeLevel < 30) volumeLevel++;
      dfplayer.volume(volumeLevel);
    }

    if (request.indexOf("/voldown") != -1) {

      if (volumeLevel > 0) volumeLevel--;
      dfplayer.volume(volumeLevel);
    }

    while (client.available()) {
      client.read();
    }

    String html = R"(
<!DOCTYPE html>
<html>
<head>
<title>ESP32 Jukebox</title>
<style>
body{font-family:Arial;background:#222;color:white;text-align:center;}
button{font-size:28px;padding:20px;margin:10px;width:150px;}
</style>
</head>
<body>
<h1>ESP32 Jukebox</h1>
<button onclick="location.href='/play'">PLAY</button>
<button onclick="location.href='/pause'">PAUSE</button>
<br>
<button onclick="location.href='/prev'">PREV</button>
<button onclick="location.href='/next'">NEXT</button>
<br>
<button onclick="location.href='/voldown'">VOL -</button>
<button onclick="location.href='/volup'">VOL +</button>
</body>
</html>
)";

    client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
    client.print(html);
    client.stop();
  }
}

void printDFStatus(uint8_t type, int value) {

  Serial.print("DFPlayer Event: ");

  switch (type) {

    case TimeOut:
      Serial.println("Timeout");
      break;

    case DFPlayerPlayFinished:
      Serial.print("Track finished: ");
      Serial.println(value);
      break;

    case DFPlayerError:
      Serial.print("Error: ");
      Serial.println(value);
      break;

    default:
      Serial.println(type);
  }
}