#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <LEAmDNS.h>

#ifndef STASSID
#define STASSID "It Hurts When IP"
#define STAPSK "Getsomemeds!"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

WebServer server(80);

const int led = LED_BUILTIN;

const int RED_LED     = 18;
const int YELLOW_LED  = 19;
const int GREEN_LED   = 20;



int RED     = 0;
int YELLOW  = 0;
int GREEN   = 0;

int enable_animation = 0;


void handleRoot() {
  digitalWrite(led, 1);
  server.send(200, "text/html",
"<html>"
"    <button onclick=\"location.href='/on'\">ON</button>"
"    <button onclick=\"location.href='/off'\">OFF</button>"
"    <button onclick=\"location.href='/red'\">Red</button>"
"    <button onclick=\"location.href='/yellow'\">Yellow</button>"
"    <button onclick=\"location.href='/green'\">Green</button>"
"</html>"
);
}

void animation() {
  digitalWrite(GREEN_LED, 1);
  digitalWrite(RED_LED, 0);
  digitalWrite(YELLOW, 0);
  delay(5000);
  digitalWrite(GREEN_LED, 0);
  digitalWrite(YELLOW_LED, 1);
  delay(1000);
  digitalWrite(YELLOW_LED, 0);
  digitalWrite(RED_LED, 1);
  delay(5000);
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (int i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void setup(void) {
  pinMode(led, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  // digitalWrite(led, 1);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("picow")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);


  server.on("/on", []() {
    enable_animation = 1;
    digitalWrite(RED_LED, 1);
    digitalWrite(YELLOW_LED, 1);
    digitalWrite(GREEN_LED, 1);
    GREEN = 1; 
    YELLOW = 1; 
    RED = 1;

    server.send(200, "text/plain",
     "all all all all all\n"
     "on on on on on\n"
    );
  });

  server.on("/off", []() {
    enable_animation = 1;
    digitalWrite(RED_LED, 0);
    digitalWrite(YELLOW_LED, 0);
    digitalWrite(GREEN_LED, 0);
    GREEN = 0; 
    YELLOW = 0;
    RED = 0;

    server.send(200, "text/plain",
     "all all all all all\n"
     "off off off off off\n"
    );
  });



  server.on("/red", []() {
    enable_animation = 1;
    if (RED == 1){
      digitalWrite(RED_LED, 0);
      server.send(200, "text/plain",
      "    <h1> RED RED RED RED</h1>"
      "    <h1> ON ON ON ON ON </h1>"
    );
      RED = 0;
    } else {
      digitalWrite(RED_LED, 1);
      server.send(200, "text/plain",
     "RED RED RED RED RED\n"
     "ON ON ON ON ON\n"
    );
      RED = 1;
    }
  });

  server.on("/yellow", []() {
    enable_animation = 1;
    if (YELLOW == 1){
      
      digitalWrite(YELLOW_LED, 0);
      server.send(200, "text/plain",
     "YELLOW YELLOW YELLOW YELLOW YELLOW\n"
     "OFF OFF OFF OFF OFF\n"
    );
      YELLOW = 0;
    } else {
      digitalWrite(YELLOW_LED, 1);
      server.send(200, "text/plain",
     "YELLOW YELLOW YELLOW YELLOW YELLOW\n"
     "ON ON ON ON ON\n"
    );
      YELLOW = 1;
    }
  });

  server.on("/green", []() {
    enable_animation = 1;
    if (GREEN == 1){
      digitalWrite(GREEN_LED, 0);
      server.send(200, "text/plain",
     "GREEN GREEN GREEN GREEN GREEN\n"
     "OFF OFF OFF OFF OFF\n"
    );
      GREEN = 0;
    } else {
      digitalWrite(GREEN_LED, 1);
      server.send(200, "text/plain", 
     "GREEN GREEN GREEN GREEN GREEN\n"
     "ON ON ON ON ON\n"
    );
      GREEN = 1;
    }
  });


  server.on("/enable", []() {
    enable_animation = 0;
    server.send(200, "text/plain",
     "AUTO MODE ENABLED\n"
  );});



  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
  MDNS.update();
  
  if (enable_animation == 0) {
    animation();
  }
  
}
