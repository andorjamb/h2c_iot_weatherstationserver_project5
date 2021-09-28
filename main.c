
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include "DHT.h"

// final copy
// Note: during testing, all the sensors were working on their own or in pairs, it wasn't possible to test them all simultaneously

#define SEALEVELPRESSURE_HPA (1013.25) //used for altitude, if altitude measurements required

//network credentials
const char *ssid = "";
const char *password = "";

// Set web server port number
WiFiServer server(80);

// Variable to store the HTTP request
String header;

unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000;

// DHT11 sensor
#define DHTPIN 14 // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE); // Initialize DHT sensor

// TEMT6000 sensor
const int LumSensorPin = 34; //default pin for TEMT6000 sensor
int luminosity = 0;

// PIR sensor
int HW740Pin = 26;
String movement;

//BMP280 sensor

Adafruit_BMP280 bmp; // I2C
//default pins for the ESP32 are SCL to 22 and SDA to 21
//check I2C address in case of failure, in this case it was 0x76, library settings modified

void setup()
{
  Serial.begin(115200);
  bool status;
  if (!bmp.begin())
  {
    //if (!bmp.begin()) {
    Serial.println("Could not find a valid BMP280 sensor, check wiring or "
                   "try a different address!");
    while (1)
      delay(10);
  }

  //Default settings
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     //Operating Mode.
                  Adafruit_BMP280::SAMPLING_X2,     //Temp. oversampling
                  Adafruit_BMP280::SAMPLING_X16,    // Pressure oversampling
                  Adafruit_BMP280::FILTER_X16,      // Filtering.
                  Adafruit_BMP280::STANDBY_MS_500); // Standby time.

  pinMode(26, INPUT); //setting input for motion sensor
  dht.begin();        //start DHT sensor

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

void loop()
{
  WiFiClient client = server.available(); // Listen for incoming clients

  if (client)
  {
    //luminosity readings
    luminosity = analogRead(LumSensorPin);
    Serial.println(luminosity);

    //DHT readings (Humidity only)
    float humidity = dht.readHumidity();
    Serial.print("Humidity:");
    Serial.print(humidity, 1);
    Serial.println("%");

    //BMP280 readings (pressure, temp)
    Serial.print("Temperature = ");
    Serial.print(bmp.readTemperature());
    Serial.println(" *C");
    float temp = bmp.readTemperature();

    Serial.print("Pressure = ");
    Serial.print(bmp.readPressure());
    Serial.println(" Pa");
    float pressure = bmp.readPressure();

    //PIR readings
    bool motionDetected = digitalRead(HW740Pin);
    if (motionDetected == 1)
    {
      movement = "YES";
      Serial.println("movement detected");
    }
    else if (motionDetected == 0)
    {
      movement = "NO";
      Serial.println("Noone in the area");
    }

    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");
    String currentLine = ""; // String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime)
    { // loop while the client's connected
      currentTime = millis();
      if (client.available())
      {
        char c = client.read();
        Serial.write(c); // print it out the serial monitor
        header += c;
        if (c == '\n')
        {
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0)
          {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            //HTML head
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta charset=\"UTF-8\"/>");
            client.println("<meta http-equiv=\"X-UA-Compatible\" content=\"IE = edge\"/>");
            client.println("<meta name=\"viewport\" content=\"width = device-width, initial-scale = 1.0\"/>");
            client.println("<title>ESP32 Sensor Readings</title>");
            // CSS
            client.println("<style>body { display:flex; height: auto; justify-content: center; text-align: center; font-family: \"Trebuchet MS\", Arial;}");
            client.println(".capsule { padding: 30px 15px; border: 5px solid black; border-radius: 20px;}");
            client.println("table { border: none; margin: auto; width:75%; text-align: left;}");
            client.println("tr { border: 1px solid #ddd; padding: 12px; }");
            client.println("td { text-align: left; line-height: 1.5; width: 50%; }");
            client.println("button { border: 2px solid black; border-radius: 15px; font-weight: 400; margin-top: 20px; padding: 10px; }");
            client.println("</style>");
            //HTML body

            client.println("</head><body><div class=\"capsule\"><h1>ESP32 Measurements</h1><div class=\"data\"><table>");
            client.println("<tr><td>Temperature:</td><td>");
            client.println(temp);
            client.println(" &degC</td></tr>");
            client.println("<tr><td>Humidity:</td><td>");
            client.println(humidity);
            client.println(" %</td></tr>");
            client.println("<tr><td>Pressure:</td><td>");
            client.println(pressure);
            client.println(" Pa</td></tr><tr>");
            client.println("<td>Luminosity:</td><td>");
            client.println(luminosity);
            client.println(" lux</td></tr>");
            client.println("<tr><td>Movement:</td><td>");
            client.println(movement); //a string that will be either "yes" or "no"
            client.println("</td></tr></table></div>");
            client.println("<button>UPDATE</button></div>");
            client.println("<script>const updateButton = document.querySelector(\"button\");");
            client.println("function updatePage(){reload = location.reload();}");
            client.println("updateButton.addEventListener(\"click\", updatePage, false);</script>");
            client.println("</body></html>");
            client.println();
            break;
          }
          else
          { // if newline, then clear currentLine
            currentLine = "";
          }
        }
        else if (c != '\r')
        {                   // if you got anything else but a carriage return character,
          currentLine += c; // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    /*// Close the connection
        client.stop();
        Serial.println("Client disconnected.");*/
    Serial.println("");
    delay(3000);
  }
}