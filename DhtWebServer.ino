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
char fileName[] = "/DataPoints.bin";
char mDnsName[] = "espDht";

DHT dht(DHTPIN, DHTTYPE);
NTPClient timeClient(ntpServer, 3600, 60000);
WiFiManager wifiManager;
ESP8266WebServer server(80);

unsigned long curDataPointPos = 0;
unsigned long lastTimeMeasured = 0;
float lastHumidity = NAN;
float lastTemperature = NAN;

typedef struct
{
	unsigned long Time;
	float Humidity;
	float Temperature;
} DataPoint;

void handleCsv() {
	File dataPoints = SPIFFS.open(fileName, "r");
	if (!dataPoints) {
		server.send(404, "text/plain", "Bin not Found or failure by open it.");
	}
	else
	{
		DataPoint dataPoint;
		dataPoints.seek(curDataPointPos * sizeof(dataPoint), SeekSet);

		WiFiClient client = server.client();
		client.println("HTTP/1.1 200 OK");
		client.println("Content-Type: text/plain");
		client.println();
		while (dataPoints.available())
		{
			dataPoints.read((byte*)&dataPoint, sizeof(dataPoint));
			client.print(dataPoint.Time);
			client.print(";");
			client.print(dataPoint.Humidity);
			client.print(";");
			client.println(dataPoint.Temperature);
		}
		dataPoints.seek(0, SeekSet);
		while (dataPoints.available() && dataPoints.position() < curDataPointPos * sizeof(dataPoint))
		{
			dataPoints.read((byte*)&dataPoint, sizeof(dataPoint));
			client.print(dataPoint.Time);
			client.print(";");
			client.print(dataPoint.Humidity);
			client.print(";");
			client.println(dataPoint.Temperature);
		}
		client.flush();
	}
	dataPoints.close();
}

void handleJson() {
	File dataPoints = SPIFFS.open(fileName, "r");
	if (!dataPoints) {
		server.send(404, "text/plain", "Bin not Found or failure by open it.");
	}
	else
	{
		DataPoint dataPoint;
		dataPoints.seek(curDataPointPos * sizeof(dataPoint), SeekSet);

		WiFiClient client = server.client();
		client.println("HTTP/1.1 200 OK");
		client.println("Content-Type: text/plain");
		client.println();
		client.println("[");
		while (dataPoints.available())
		{
			dataPoints.read((byte*)&dataPoint, sizeof(dataPoint));
			client.print("{\"Time\":");
			client.print(dataPoint.Time);
			client.print(",");
			client.print("\"Humidity\":");
			client.print(dataPoint.Humidity);
			client.print(",");
			client.print("\"Temperature\":");
			client.print(dataPoint.Temperature);
			client.print("}");
			if (dataPoints.available())
			{
				client.println(",");
			}
			else
			{
				client.println("");
			}
		}
		dataPoints.seek(0, SeekSet);
		while (dataPoints.available() && dataPoints.position() < curDataPointPos * sizeof(dataPoint))
		{
			dataPoints.read((byte*)&dataPoint, sizeof(dataPoint));
			client.print("{\"Time\":");
			client.print(dataPoint.Time);
			client.print(",");
			client.print("\"Humidity\":");
			client.print(dataPoint.Humidity);
			client.print(",");
			client.print("\"Temperature\":");
			client.print(dataPoint.Temperature);
			client.print("}");
			if (dataPoints.available() && dataPoints.position() < curDataPointPos * sizeof(dataPoint))
			{
				client.println(",");
			}
			else
			{
				client.println("");
			}
		}
		client.println("]");
		client.flush();
	}
	dataPoints.close();
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
	File dataPoints = SPIFFS.open(fileName, "r");
	if (!dataPoints) {
		Serial.println("file open failed - maxline = 0");
	}
	else
	{
		dataPoints.seek(0, SeekSet);
		unsigned long maxTicks = 0;
		unsigned long lineCount = 0;
		while (dataPoints.available())
		{
			DataPoint dataPoint;
			dataPoints.read((byte*)&dataPoint, sizeof(dataPoint));
			if (maxTicks <= dataPoint.Time)
			{
				maxLine = lineCount;
				maxTicks = dataPoint.Time;
			}
			lineCount++;
		}
		maxLine++;
	}
	dataPoints.close();
	Serial.print("Start Position: ");
	Serial.println(maxLine);
	return maxLine;
}

void writeFile(unsigned long time, float h, float t)
{
	if (!SPIFFS.exists(fileName))
	{
		File dataPoints = SPIFFS.open(fileName, "w");
		dataPoints.close();
	}
	File dataPoints = SPIFFS.open(fileName, "r+");
	if (!dataPoints) {
		Serial.println("file open failed");
	}
	else
	{
		if (curDataPointPos >= maxDataPoints)
		{
			curDataPointPos = 0;
		}
		DataPoint dataPoint = { time, h, t};
		dataPoints.seek(curDataPointPos * sizeof(dataPoint), SeekSet);

		Serial.print("Pos: ");
		Serial.print(curDataPointPos);
		Serial.print("(");
		Serial.print(dataPoints.position());
		Serial.print("): ");

		dataPoints.write((byte*)&dataPoint, sizeof(dataPoint));
		curDataPointPos++;

		Serial.print(dataPoint.Time);
		Serial.print(";");
		Serial.print(dataPoint.Humidity);
		Serial.print(";");
		Serial.println(dataPoint.Temperature);
	}
	dataPoints.close();
}


void setup() {
	Serial.begin(115200);
	wifiManager.autoConnect();

	SPIFFS.begin();

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

