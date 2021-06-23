#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>

unsigned long lastRingCheck = 0;
long currentTurnOnTime = 0;
bool ignoreFirstOne = false;
enum TurnOnState
{
  NOT_TRIGGERED = 0,
  TRIGGERED_ON = 1,
  TRIGGERED_WAIT = 2,
  TRIGGERED_OFF = 3
};

TurnOnState onState = NOT_TRIGGERED;

WiFiClient espClient;
PubSubClient client(espClient);

void callback(char *topic, byte *payload, unsigned int length)
{
  String stopic = String(topic);
  String spayload = String((char *)payload).substring(0, length);
  if (stopic.equals("building/intercom/door/set"))
  {
    if (spayload.equals("ON"))
    {
      Serial.println("Got payload!");
      if (!ignoreFirstOne)
      {
        Serial.println("Not ignoring it!");
        onState = TRIGGERED_ON;
      }
      ignoreFirstOne = false;
    }
    // else if (spayload.equals("OFF"))
    // {
    //   turnOnThisTime = false;
    // }
  }
}
void setup()
{
  pinMode(D8, OUTPUT);
  pinMode(A0, INPUT);
  Serial.begin(9600);
  WiFi.begin(AP_NAME, PASSWORD);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected, IP is: ");
  Serial.println(WiFi.localIP());

  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);

  ArduinoOTA.onStart([]()
                     { Serial.println("OTA Start!"); });
  ArduinoOTA.onEnd([]()
                   { Serial.println("OTA End!"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
  ArduinoOTA.onError([](ota_error_t error)
                     { Serial.printf("Error[%u]\n", error); });
  ArduinoOTA.begin();

  // put your setup code here, to run once:
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.println("Attempting MQTT connection!");
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD))
    {

      Serial.println("Connected!");
      client.subscribe("building/intercom/door/set");
      client.publish("building/intercom/av", "online");
      ignoreFirstOne = true;
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void loop()
{
  ArduinoOTA.handle();
  if (!client.connected())
  {
    reconnect();
  }
  else
  {
    client.loop();
  }

  if (onState == TRIGGERED_ON)
  {
    Serial.println("high!");
    digitalWrite(D8, HIGH);
    currentTurnOnTime = millis();
    onState = TRIGGERED_WAIT;
    client.publish("building/intercom/door", "ON");
  }

  else if (onState == TRIGGERED_WAIT && millis() - currentTurnOnTime > 1500)
  {
    Serial.println("low!");
    digitalWrite(D8, LOW);
    onState = TRIGGERED_OFF;
    client.publish("building/intercom/door", "OFF");
  
  }

  if (millis() - lastRingCheck > 100)
  {
    int ringRead = analogRead(A0);
    Serial.printf("A0: %d\n", ringRead);
    lastRingCheck = millis();
    if (ringRead > 0)
    {
      client.publish("building/intercom/ring/status", "ON");
    }
  }
}