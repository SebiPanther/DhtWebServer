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
int maxDataPoints = 500;
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

void handleCsv() {
  File valuesFile = SPIFFS.open(fileName, "r");
  if (!valuesFile) {
    server.send(404, "text/plain", "Csv not Found or failure by open it.");
  }
  else
  {
    WiFiClient client = server.client();
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.print("Content-Length: ");
    client.println(valuesFile.size());
    client.println();
    unsigned long lineCount = 0;
    while (valuesFile.available())
    {
      String curValues = valuesFile.readStringUntil('\n');
      if (lineCount > curDataPointPos)
      {
        client.print(curValues);
      }
      lineCount++;
    }
    valuesFile.seek(0, SeekSet);
    lineCount = 0;
    while (valuesFile.available())
    {
      String curValues = valuesFile.readStringUntil('\n');
      if (lineCount <= curDataPointPos)
      {
        client.print(curValues);
      }
      else
      {
        break;
      }
      lineCount++;
    }
  }
  valuesFile.close();
}

void writeJsonObj(WiFiClient client, String curValues) {
  client.print("{\"Time\":");
  client.print(curValues.substring(0, 10));
  client.print(",");
  client.print("\"Humidity\":");
  client.print(curValues.substring(11, 16));
  client.print(",");
  client.print("\"Temperature\":");
  client.print(curValues.substring(17, 22));
  client.print("}");
}

void handleJson() {
  File valuesFile = SPIFFS.open(fileName, "r");
  if (!valuesFile) {
    server.send(404, "text/plain", "Json not Found or failure by open it.");
  }
  else
  {
    WiFiClient client = server.client();
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println();
    client.println("[");
    unsigned long lineCount = 0;
    while (valuesFile.available())
    {
      String curValues = valuesFile.readStringUntil('\n');
      if (lineCount > curDataPointPos)
      {
        writeJsonObj(client, curValues);
        if (!valuesFile.available() && curDataPointPos >= maxDataPoints)
        {
          client.println("");
        }
        else
        {
          client.println(",");
        }
      }
      lineCount++;
    }
    valuesFile.seek(0, SeekSet);
    lineCount = 0;
    while (valuesFile.available())
    {
      String curValues = valuesFile.readStringUntil('\n');
      if (lineCount <= curDataPointPos)
      {
        writeJsonObj(client, curValues);
        if (lineCount < curDataPointPos)
        {
          client.println(",");
        }
        else
        {
          client.println("");
        }
      }
      else
      {
        break;
      }
      lineCount++;
    }
    client.println("]");
  }
  valuesFile.close();
}

void handleNotFound() {
  String path = server.uri();;
  if (path == "/")
  {
    path = "/index.html";
  }

  String dataType = "text/plain";
  if (path.endsWith(".htm")) dataType = "text/html";
  else if (path.endsWith(".css")) dataType = "text/css";
  else if (path.endsWith(".js")) dataType = "application/javascript";
  else if (path.endsWith(".png")) dataType = "image/png";
  else if (path.endsWith(".gif")) dataType = "image/gif";
  else if (path.endsWith(".jpg")) dataType = "image/jpeg";
  else if (path.endsWith(".ico")) dataType = "image/x-icon";
  else if (path.endsWith(".xml")) dataType = "text/xml";
  else if (path.endsWith(".pdf")) dataType = "application/pdf";
  else if (path.endsWith(".zip")) dataType = "application/zip";

  File file = SPIFFS.open(path, "r");
  if (!file)
  {
    String message = "File Not Found or unable to Load.\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    message += "Maybe you should try /csv or /json ;)";
    for (uint8_t i = 0; i < server.args(); i++) {
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
  }
  else
  {
    server.streamFile(file, dataType);
  }
  file.close();
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
  //SPIFFS.remove(fileName);

  MDNS.begin(mDnsName);
  MDNS.addService("http", "tcp", 80);
  Serial.print("MDNS Name: ");
  Serial.println(mDnsName);

  server.on("/csv", handleCsv);
  server.on("/json", handleJson);
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
