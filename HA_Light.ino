#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

//
// Update these with values suitable for your network.
//
const char* WIFI_SSID = "WIFI_SSID";
const char* WIFI_PASSWORD = "WIFI_PASSWORD";
const char* MQTT_SERVER = "MQTT_SERVER";
const char* MQTT_SERVER_USER = "MQTT_SERVER_USER";
const char* MQTT_SERVER_PASS = "MQTT_SERVER_PASS";

//
// The topic is for Home Assistant MQTT Discovery
//
const char* TOPIC_HA_CONFIG = "homeassistant/light/my_light/config";
const char* TOPIC_HA_STATE = "homeassistant/light/my_light/state";
const char* TOPIC_HA_CMD = "homeassistant/light/my_light/set";

const char* PAYLOAD_HA_CONFIG = "\
{\
  \"~\": \"homeassistant/light/my_light\",\
  \"name\": \"My Light\",\
  \"unique_id\": \"my_light\",\
  \"cmd_t\": \"~/set\",\
  \"stat_t\": \"~/state\",\
  \"schema\": \"json\",\
  \"brightness\": true\
}";

// Which pin on the Arduino is connected to the NeoPixels?
#define PIN 0 // NodeMCU:D3

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS 64 // RGB LED count

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
int Brightness = 255;
bool Alert = false;
int AlertLightState = 0;

void setup() {
  Serial.begin(9600);

  setup_wifi_stage1();

  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.setBrightness(255); // 0 ~ 255 // This is the max brightness
  Serial.println("setup...");
  delay(1000);

  pixels.clear(); // Set all pixel colors to 'off'
  for (int i = 0; i < NUMPIXELS; i++) { // For each pixel...
    pixels.setPixelColor(i, pixels.Color(0, 0, 0));
    pixels.show();   // Send the updated pixel colors to the hardware.
  }

  setup_wifi_stage2();

  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);

}

void loop()
{
  if (Alert) {
    if (AlertLightState) {
      setLightBrightness(0, 0, 0);
      AlertLightState = 0;
    }
    else {
      setLightBrightness(Brightness, 0, 0);
      AlertLightState = 1;
    }
  }

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

void callback(char* topic, byte* payload, unsigned int length) {
  String str_pl = "";
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] = ");
  for (int i = 0; i < length; i++) {
    str_pl += (char)payload[i];
  }
  Serial.println(str_pl);

  str_pl.toUpperCase();

  if (str_pl.indexOf("\"ON\"") >= 0) {
    //
    // Receive the state - 'ON' from HA
    //
    int bri = getValueInt(str_pl, "BRIGHTNESS");
    Serial.printf("bri = %d\n", bri);
    if (bri >= 0) { // Found the key - "brightness"
      //
      // Receive the brightness from HA
      //
      Brightness = bri;
    }

    //
    // Set the current brightness
    //
    setLightBrightness(Brightness);
    delay(100);

    //
    // Sync the state to HA
    //
    str_pl  = "{\"state\": \"ON\", \"brightness\": ";
    str_pl += Brightness;
    str_pl += "}";
    Serial.println(str_pl);
    client.publish(TOPIC_HA_STATE, str_pl.c_str());
  }
  else if (str_pl.indexOf("\"OFF\"") >= 0) {
    //
    // Receive the state - 'OFF' from HA
    //
    if (Alert)
      setLightBrightness(0);
    else
      smoothOff(Brightness);

    delay(100);

    //
    // Sync the state to HA
    //
    str_pl  = "{\"state\": \"OFF\", \"brightness\": ";
    str_pl += Brightness;
    str_pl += "}";
    Serial.println(str_pl);
    client.publish(TOPIC_HA_STATE, str_pl.c_str());

    //
    // Turn off the alert
    //
    Alert = false;
  }
  else if (str_pl.indexOf("\"ALERT\"") >= 0) {
    //
    // Receive the state - 'ALERT' from HA
    //
    Alert = true;
    delay(100);

    //
    // Sync the state to HA
    //
    str_pl  = "{\"state\": \"ON\", \"brightness\": ";
    str_pl += Brightness;
    str_pl += "}";
    Serial.println(str_pl);
    client.publish(TOPIC_HA_STATE, str_pl.c_str());
  }
}

void setLightBrightness(int bri)
{
  Serial.printf("setLightBrightness %d\n", bri);
  
  setLightBrightness(bri, bri, bri);
}

void setLightBrightness(int rr, int gg, int bb)
{
  Serial.printf("setLightBrightness %d, %d, %d\n", rr, gg, bb);
  
  for (int i = 0; i < NUMPIXELS; i++) { // For each pixel...
    pixels.setPixelColor(i, pixels.Color(rr, gg, bb));
  }
  pixels.show();   // Send the updated pixel colors to the hardware.
}

void smoothOff(int bri)
{
  Serial.printf("setLightBrightness %d\n", bri);

  for(int bri_temp = bri; bri_temp >= 0; bri_temp--) {
    for (int i = 0; i < NUMPIXELS; i++) { // For each pixel...
      pixels.setPixelColor(i, pixels.Color(bri_temp, bri_temp, bri_temp));
    }
    pixels.show();   // Send the updated pixel colors to the hardware.
  }
}

void reconnect() {
  Serial.println("reconnect");
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), MQTT_SERVER_USER, MQTT_SERVER_PASS)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(TOPIC_HA_CONFIG, PAYLOAD_HA_CONFIG);
      delay(1000);
      // ... and resubscribe
      client.subscribe(TOPIC_HA_CMD);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 10 seconds before retrying
      delay(10*1000);
    }
  }
}

void setup_wifi_stage1() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void setup_wifi_stage2() {
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

//
// Passing the key - "brightness" and parsing the value like 153
//
// {"state": "ON", "brightness": 153}
//
int getValueInt(String src, const char* key)
{
  String temp = "";
  int pos = src.indexOf(key);
  //Serial.printf("pos = %d\n", pos);

  if (pos < 0)
    return -1; // Error

  pos += strlen(key); // skip key
  pos++; // Skip "
  //Serial.printf("pos = %d\n", pos);
  for (; pos < src.length(); pos++) {
    if (src[pos] != ':' && src[pos] != ' ') {
      break;
    }
  }
  //Serial.printf("pos = %d\n", pos);

  for (; pos < src.length(); pos++) {
    if (src[pos] >= '0' && src[pos] <= '9') {
      temp += src[pos];
    }
    else if(src[pos] == ' ') { // Reach the end
      break;
    }
    else if(src[pos] == '}') { // Reach the end
      break;
    }
    else {
      return -1; // Error
    }
  }
  return temp.toInt();
}
