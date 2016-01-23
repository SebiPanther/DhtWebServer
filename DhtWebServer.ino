#include <WiFiManager.h>
#include <NTPClient.h>
#include <DHT.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <WiFiServer.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFi.h>
#include "FS.h"


#define DHTPIN 2
#define DHTTYPE DHT11   // DHT 11
int maxDataPoints = 100;
char ntpServer[] = "europe.pool.ntp.org";
char fileName[] = "/values.csv";
char mDnsName[] = "espDht";

DHT dht(DHTPIN, DHTTYPE);
NTPClient timeClient(ntpServer, 3600, 60000);
WiFiManager wifiManager;
ESP8266WebServer server(80);

unsigned long curDataPointPos = 0;
unsigned long lastTimeMeasured = 0;
float lastHumidity = NAN;
float lastTemperature = NAN;

void handleRoot() {
  File valuesFile = SPIFFS.open(fileName, "r");
  if (!valuesFile) {
    server.send(404, "text/plain", "values.csv not Found or failure by open it.");
  }
  else
  {
    server.streamFile(valuesFile, "text/plain");
  }
  valuesFile.close();
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i<server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

unsigned long findlastPosition()
{
  unsigned long maxLine = 0;
  File valuesFile = SPIFFS.open(fileName, "r");
  if (!valuesFile) {
    Serial.println("file open failed - maxline = 0");
  }
  else
  {
    valuesFile.seek(0, SeekSet);
    unsigned long maxTicks = 0;
    unsigned long lineCount = 0;
    while (valuesFile.available())
    {
      String curValues = valuesFile.readStringUntil('\n');
      char buf[11];
      curValues.substring(0, 10).toCharArray(buf, 11);
      unsigned long longTime = strtoul(buf, '\0', 10);
      if (maxTicks <= longTime)
      {
        maxLine = lineCount;
        maxTicks = longTime;
      }
      lineCount++;
    }
    maxLine++;
  }
  valuesFile.close();
  Serial.print("Start Position: ");
  Serial.println(maxLine);
  return maxLine;
}

void writeFile(unsigned long longTime, float h, float t)
{
  if (!SPIFFS.exists(fileName))
  {
    File valuesFile = SPIFFS.open(fileName, "w");
    valuesFile.close();
  }
  File valuesFile = SPIFFS.open(fileName, "r+");
  if (!valuesFile) {
    Serial.println("file open failed");
  }
  else
  {
    valuesFile.seek(0, SeekSet);
    if (curDataPointPos < maxDataPoints)
    {
      unsigned long lineCount = 0;
      while (valuesFile.available())
      {
        String curValues = valuesFile.readStringUntil('\n');
        if (lineCount >= curDataPointPos)
        {
          break;
        }
        lineCount++;
      }
    }
    else
    {
      curDataPointPos = 0;
    }

    Serial.print("Pos: ");
    Serial.print(curDataPointPos);
    Serial.print("(");
    Serial.print(valuesFile.position());
    Serial.print("): ");

    valuesFile.print(longTime);
    valuesFile.print(";");
    valuesFile.print(h);
    valuesFile.print(";");
    valuesFile.println(t);
    curDataPointPos++;

    Serial.print(longTime);
    Serial.print(";");
    Serial.print(h);
    Serial.print(";");
    Serial.println(t);
  }
  valuesFile.close();
}


void setup() {
  Serial.begin(115200);
  wifiManager.autoConnect();

  SPIFFS.begin();

  MDNS.begin(mDnsName);
  MDNS.addService("http", "tcp", 80);
  Serial.print("MDNS Name: ");
  Serial.println(mDnsName);

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.print("Server: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");

  curDataPointPos = findlastPosition();
  lastTimeMeasured = millis();
}

void loop() {
  server.handleClient();
  timeClient.update();
  if ((lastTimeMeasured + 5000) < millis())
  {

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t) && (h != lastHumidity || t != lastTemperature))
    {
      writeFile(timeClient.getRawTime(), h, t);

      lastHumidity = h;
      lastTemperature = t;
      lastTimeMeasured = millis();
    }
  }
}
