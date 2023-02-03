/*
Connections:
NodeMCU    -> Matrix
MOSI-D7-GPIO13  -> DIN
CLK-D5-GPIO14   -> Clk
GPIO0-D3        -> LOAD
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>
#include <ArduinoJson.h>

// =======================================================================
// Device configuration:
// =======================================================================
const char* ssid     = "ssid";                      // SSID
const char* password = "password";                    // password


String weatherKey = "api key";  // API key, click http://openweathermap.org/api
String weatherLang = "&lang=en";
String cityID = "city code"; // city code
// =======================================================================


WiFiClient client;

String weatherMain = "";
String weatherDescription = "";
String weatherLocation = "";
String country;
int humidity;
int pressure;
float temp;
float tempMin, tempMax;
int clouds;
float windSpeed;
String date;
String currencyRates;
String weatherString;

long period;
int offset=1,refresh=0;
int pinCS = D8; // Pin connection CS
int numberOfHorizontalDisplays = 8; // Number of LED Matrices Horizontally
int numberOfVerticalDisplays = 1; // Number of LED Matrices Vertically
String decodedMsg;

Max72xxPanel matrix = Max72xxPanel( pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);

//matrix.cp437(true);

int wait = 50; // ticker speed

int spacer = 2;
int width = 5 + spacer; // Adjusting the spacing between characters

void setup(void) {

matrix.setIntensity(2); // Matrix brightness from 0 to 15


// initial coordinates of matrices 8*8
  matrix.setRotation(0, 1);        // 1 matrix
  matrix.setRotation(1, 1);        // 2 matrix
  matrix.setRotation(2, 1);        // 3 matrix
  matrix.setRotation(3, 1);        // 4 matrix
  matrix.setRotation(4, 1);        // 5 matrix
  matrix.setRotation(5, 1);        // 6 matrix
  matrix.setRotation(6, 1);        // 7 matrix
  matrix.setRotation(7, 1);        // 8 matrix


  Serial.begin(115200);                            // Debug
  WiFi.mode(WIFI_STA);                           
  WiFi.begin(ssid, password);                      // Connecting to WIFI
  while (WiFi.status() != WL_CONNECTED) {          // Waiting until blue
    delay(500);
    Serial.print(".");
  }



}

// =======================================================================
#define MAX_DIGITS 16
byte dig[MAX_DIGITS]={0};
byte digold[MAX_DIGITS]={0};
byte digtrans[MAX_DIGITS]={0};
int updCnt = 0;
int dots = 0;
long dotTime = 0;
long clkTime = 0;
int dx=0;
int dy=0;
byte del=0;
int h,m,s;
// =======================================================================
void loop(void) {




if(updCnt<=0) { // every 10 cycles we get time and weather data
    updCnt = 10;
    Serial.println("Getting data ...");
    getWeatherData();
    getTime();
    Serial.println("Data loaded");
    clkTime = millis();
  }

  if(millis()-clkTime > 15000 && !del && dots) { //every 15 seconds we launch a running line
ScrollText(utf8rus("Have a nice day! :) ")); //here is the text of the line, then there will be weather, etc.
delay(5);
ScrollText(utf8rus(date));
delay(5);
ScrollText(utf8rus(weatherString));
    updCnt--;
    clkTime = millis();
  }
  
DisplayTime();
  if(millis()-dotTime > 1000) {
    dotTime = millis();
    dots = !dots;
  }


}

// =======================================================================
void DisplayTime(){
    updateTime();
    matrix.fillScreen(LOW);
    int y = (matrix.height() - 8) / 2; // Center text vertically
    
     
    if(s & 1){matrix.drawChar(14, y, (String(":"))[0], HIGH, LOW, 1);} //every even second we print a colon in the center (to blink)
    else{matrix.drawChar(14, y, (String(" "))[0], HIGH, LOW, 1);}
    
    String hour1 = String (h/10);
    String hour2 = String (h%10);
    String min1 = String (m/10);
    String min2 = String (m%10);
    String sec1 = String (s/10);
    String sec2 = String (s%10);
    int xh = 2;
    int xm = 19;
    int xs = 34;

    matrix.drawChar(xh, y, hour1[0], HIGH, LOW, 1);
    matrix.drawChar(xh+6, y, hour2[0], HIGH, LOW, 1);
    matrix.drawChar(xm, y, min1[0], HIGH, LOW, 1);
    matrix.drawChar(xm+6, y, min2[0], HIGH, LOW, 1);
    matrix.drawChar(xs, y, sec1[0], HIGH, LOW, 1);
    matrix.drawChar(xs+6, y, sec2[0], HIGH, LOW, 1);  


  
    matrix.write(); // Display output
}

// =======================================================================
void DisplayText(String text){
    matrix.fillScreen(LOW);
    for (int i=0; i<text.length(); i++){
    
    int letter =(matrix.width())- i * (width-1);
    int x = (matrix.width() +1) -letter;
    int y = (matrix.height() - 8) / 2; // Center text vertically
    matrix.drawChar(x, y, text[i], HIGH, LOW, 1);
    matrix.write(); // Display output
    
    }

}
// =======================================================================
void ScrollText (String text){
    for ( int i = 0 ; i < width * text.length() + matrix.width() - 1 - spacer; i++ ) {
    if (refresh==1) i=0;
    refresh=0;
    matrix.fillScreen(LOW);
    int letter = i / width;
    int x = (matrix.width() - 1) - i % width;
    int y = (matrix.height() - 8) / 2; // Center text vertically
 
    while ( x + width - spacer >= 0 && letter >= 0 ) {
      if ( letter < text.length() ) {
        matrix.drawChar(x, y, text[letter], HIGH, LOW, 1);
      }
      letter--;
      x -= width;
    }
    matrix.write(); // Display output
    delay(wait);
  }
}


// =======================================================================
// We take the weather from the site openweathermap.org
// =======================================================================


char weatherHost[] = "api.openweathermap.org";            // remote server we will connect to
//const char *weatherHost = "api.openweathermap.org";     // Original 

void getWeatherData()
{
  Serial.print("connecting to "); Serial.println(weatherHost);
  if (client.connect(weatherHost, 80)) {
/*    client.println(String("GET /data/2.5/weather?id=") + cityID + "&units=metric&appid=" + weatherKey + weatherLang + "\r\n" +
                "Host: " + weatherHost + "\r\nUser-Agent: ArduinoWiFi/1.1\r\n" +
                "Connection: close\r\n\r\n");  
      //Original
*/     
    client.println("GET /data/2.5/weather?id=" + cityID + "&units=imperial" + "&appid=" + weatherKey + weatherLang);
    client.println("Host: api.openweathermap.org");
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();      

  } else {
    Serial.println("connection failed");
    return;
  }

  String line;

/*  int repeatCounter = 0;
  while (!client.available() && repeatCounter < 10) {
    delay(500);
    Serial.println("w.");
    repeatCounter++;
  }  //Original
*/
  while (client.connected() && !client.available())
    delay(1);                                          //waits for data

  Serial.println("Client connected");
  while (client.connected() && client.available()) {
    
    char c = client.read(); 
    if (c == '[' || c == ']') c = ' ';
    line += c;
  }

  client.stop();
  
   Serial.print("Line is " );
   Serial.println(line.c_str());
   
  //DynamicJsonBuffer jsonBuf;  v5 Json
  //DynamicJsonDocument doc(ESP.getMaxFreeBlockSize() - 512);   //v6 Json
  
  //JsonObject &root = jsonBuf.parseObject(line);
  
  //DeserializationError error = deserializeJson(doc, line);  // v6 Json
  
  char jsonArray [line.length() + 1];
  line.toCharArray(jsonArray, sizeof(jsonArray));
  jsonArray[line.length() + 1] = '\0';
  StaticJsonDocument<1024> doc;
  DeserializationError  error = deserializeJson(doc, jsonArray);

 // if (!root.success()) v5 Json
   if (error)
  {
    Serial.print("deserializeJson() failed with ");
    Serial.println(error.c_str());
    return;
  }
  // Note: for Json v6 replace "root" with "doc"
  
  //weatherMain = doc["weather"]["main"].as<String>();
  weatherDescription = doc["weather"]["description"].as<String>();
  weatherDescription.toLowerCase();
  //  weatherLocation = doc["name"].as<String>();
  //  country = doc["sys"]["country"].as<String>();
  temp = doc["main"]["temp"];
  humidity = doc["main"]["humidity"];
  pressure = doc["main"]["pressure"];
  tempMin = doc["main"]["temp_min"];
  tempMax = doc["main"]["temp_max"];
  windSpeed = doc["wind"]["speed"];
  clouds = doc["clouds"]["all"];
  String deg = String(char('~'+25));
  weatherString = "TEMP " + String(temp,1)+"F ";
  weatherString += weatherDescription;
  weatherString += " HUM " + String(humidity) + "% ";
  weatherString += "PRES " + String(pressure/1.3332239) + "mm ";
  weatherString += "CLOUD " + String(clouds) + "% ";
  weatherString += "WIND " + String(windSpeed,1) + "mph ";
}

// =======================================================================
// take time from GOOGLE
// =======================================================================

float utcOffset = -7; //time zone correction
long localEpoc = 0;
long localMillisAtUpdate = 0;

void getTime()
{
  WiFiClient client;
  if (!client.connect("www.google.com", 80)) {
    Serial.println("connection to google failed");
    return;
  }

  client.print(String("GET / HTTP/1.1\r\n") +
               String("Host: www.google.com\r\n") +
               String("Connection: close\r\n\r\n"));
  int repeatCounter = 0;
  while (!client.available() && repeatCounter < 10) {
    delay(500);
    //Serial.println(".");
    repeatCounter++;
  }

  String line;
  client.setNoDelay(false);
  while(client.connected() && client.available()) {
    line = client.readStringUntil('\n');
    line.toUpperCase();
    if (line.startsWith("DATE: ")) {
      date = "     "+line.substring(6, 22);
      h = line.substring(23, 25).toInt();
      m = line.substring(26, 28).toInt();
      s = line.substring(29, 31).toInt();
      localMillisAtUpdate = millis();
      localEpoc = (h * 60 * 60 + m * 60 + s);
    }
  }
  client.stop();
}

// =======================================================================r

void updateTime()
{
  long curEpoch = localEpoc + ((millis() - localMillisAtUpdate) / 1000);
  long epochpartial = round(curEpoch + 3600 * utcOffset + 86400L);
  long epoch = epochpartial % 86400L;
  h = ((epoch  % 86400L) / 3600) % 24;
  m = (epoch % 3600) / 60;
  s = epoch % 60;
}

// =======================================================================


String utf8rus(String source)
{
  int i,k;
  String target;
  unsigned char n;
  char m[2] = { '0', '\0' };

  k = source.length(); i = 0;

  while (i < k) {
    n = source[i]; i++;

    if (n >= 0xC0) {
      switch (n) {
        case 0xD0: {
          n = source[i]; i++;
          if (n == 0x81) { n = 0xA8; break; }
          if (n >= 0x90 && n <= 0xBF) n = n + 0x30-1;
          break;
        }
        case 0xD1: {
          n = source[i]; i++;
          if (n == 0x91) { n = 0xB8; break; }
          if (n >= 0x80 && n <= 0x8F) n = n + 0x70-1;
          break;
        }
      }
    }
    m[0] = n; target = target + String(m);
  }
return target;
}
