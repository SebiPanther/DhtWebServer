#include <ESP8266mDNS.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include "FS.h"
#include <DHT.h>
#include <WiFiManager.h>


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

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void writeFile(unsigned long longTime, float h, float t)
{
      Serial.print(longTime);
      Serial.print(";");
      Serial.print(h);
      Serial.print(";");
      Serial.println(t);
      
      File valuesFile = SPIFFS.open(fileName, "a");
      if (!valuesFile) {
          Serial.println("file open failed");
      }
      else
      {
        valuesFile.print(longTime);
        valuesFile.print(";");
        valuesFile.print(h);
        valuesFile.print(";");
        valuesFile.println(t);
      }
      valuesFile.close();
}

void setup() {
    Serial.begin(115200);
    wifiManager.autoConnect();
    
    SPIFFS.begin();
    SPIFFS.remove(fileName);

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
    
    lastTimeMeasured = millis();
}

void loop() {
  server.handleClient();
  if((lastTimeMeasured + 5000) < millis())
  {
    timeClient.update();
    
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if(!isnan(h) && !isnan(t) && (h != lastHumidity || t != lastTemperature))
    {      
      writeFile(timeClient.getRawTime(), h, t);
            
      lastHumidity = h;
      lastTemperature = t;
      lastTimeMeasured = millis();
    }
  }
}
