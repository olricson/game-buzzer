#include <Adafruit_NeoPixel.h>
#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"
// #include "buzzer_sound.h"
// #include "buzzer_sound_no_gain.h"

#include <HTTPClient.h>
#include <WebSocketsClient.h>

// #include "buzzer_sound_-11db.h"
#include "buzzer_sound_qpuc_12db.h"

#define LED_PIN 22 

#define LEN_COUNT 18
#define MAX_BRITHNESS 150

#define BUTTON_PIN 15

#define BUTTON_LED_PIN 18

#define SOUND_VOLUME 0.1

bool enabled = true;
bool pressed = false;

// const char* ssid = "game buzzer";
// const char* password = "blindtest";
const char *ssid = "";
const char *password = "";

int color[3] = {0, 0, 0};

WebSocketsClient webSocket;

Adafruit_NeoPixel leds(LEN_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// MemoryStream mp3(mixkit_arcade_video_game_bonus_2044__11db_mp3, mixkit_arcade_video_game_bonus_2044__11db_mp3_len);
MemoryStream mp3(buzzer_sound_qpuc_12db_mp3, buzzer_sound_qpuc_12db_mp3_len);
AnalogAudioStream analog;                               // Analog output PIN 25
EncodedAudioStream out(&analog, new MP3DecoderHelix()); // output to decoder
StreamCopy copier(out, mp3);                            // copy in to out
AudioInfo cfg;

void turn_on_led(int r, int g, int b)
{
  for (int i = 0; i < LEN_COUNT; i++)
  {
    leds.setPixelColor(i, leds.Color(r, g, b));
  }
  leds.show();
}

void activate_buzzer(bool sound)
{
  enabled = false;
  turn_on_led(color[0], color[1], color[2]);
  if (sound)
  {
    play_buzzer_sound();
  }
}

void turn_off_led()
{
  leds.clear();
  leds.show();
}

void set_led_button_state(bool state)
{
  digitalWrite(BUTTON_LED_PIN, state);
}

void play_buzzer_sound()
{
  while (mp3)
  {
    copier.copy();
  }
  out.end();
  mp3.begin();
  out.begin(cfg);
}

void boot_animation()
{
  turn_on_led(MAX_BRITHNESS, 0, 0);
  set_led_button_state(true);
  delay(1000);
  turn_on_led(0, MAX_BRITHNESS, 0);
  set_led_button_state(false);
  delay(1000);
  set_led_button_state(true);
  turn_on_led(0, 0, MAX_BRITHNESS);
  delay(1000);
  set_led_button_state(false);
  turn_off_led();
  play_buzzer_sound();
}

void IRAM_ATTR on_button_press()
{
  if (!pressed && enabled)
  {
    pressed = true;
  }
}

AudioInfo defaultConfig()
{
  AudioInfo cfg;
  cfg.channels = 1;
  cfg.sample_rate = 16000;
  cfg.bits_per_sample = 16;
  return cfg;
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  String on_cmd = "on " + WiFi.macAddress();
  String off_cmd = "off " + WiFi.macAddress();
  Serial.print("Payload: [");
  Serial.print((char*)payload);
  Serial.println("]");
  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.printf("[WSc] Disconnected!\n");
    break;
  case WStype_CONNECTED:
    Serial.printf("[WSc] Connected to url: %s\n", payload);

    // send message to server when Connected
    webSocket.sendTXT("Connected");
    break;
  case WStype_TEXT:
  
    Serial.printf("[WSc] get text: %s\n", payload);
    if (strcmp((char *)payload, "reset") == 0)
    {
      reset();
    }
    else if (on_cmd.equals((char *)payload))
    {
      activate_buzzer(false);
    }
    else if (off_cmd.equals((char *)payload))
    {
      turn_off_led();
    }
    else if (strcmp((char *)payload, "disable") == 0)
    {
      enabled = false;
    }
    break;
  case WStype_BIN:
  case WStype_ERROR:
  case WStype_FRAGMENT_TEXT_START:
  case WStype_FRAGMENT_BIN_START:
  case WStype_FRAGMENT:
  case WStype_FRAGMENT_FIN:
    break;
  }
}

void setup_wifi()
{

  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  Serial.print("Tentative de connexion...");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }
  Serial.println("Connected !");
}

void reset()
{
  turn_off_led();
  pressed = false;
  enabled = true;
}

void setup_websocket()
{
  // server address, port and URL
  webSocket.begin("192.168.0.225", 80, "/ws");

  // event handler
  webSocket.onEvent(webSocketEvent);

  // use HTTP Basic Authorization this is optional remove if not needed
  // try ever 5000 again if connection has failed
  webSocket.setReconnectInterval(5000);
}

void get_config_from_master() {
  HTTPClient http;

  String serverPath = "http://192.168.0.225/config?id=" + WiFi.macAddress();

  // Your Domain name with URL path or IP address with path
  http.begin(serverPath.c_str());

  // If you need Node-RED/server authentication, insert user and password below
  // http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");

  // Send HTTP GET request
  int httpResponseCode = http.GET();
  if (httpResponseCode == 200)
    {
      String payload = http.getString();

      color[0] = payload.substring(0, payload.indexOf(',',0)).toInt();
      color[1] = payload.substring(payload.indexOf(',',0)+1, payload.indexOf(',',payload.indexOf(',',0)+1)).toInt();
      color[2] = payload.substring(payload.indexOf(',',payload.indexOf(',',0)+1)+1, payload.length()).toInt();
      Serial.printf("Color config: [%d, %d, %d]\n", color[0], color[1], color[2]);
    }
    else
    {
      Serial.println("Fail to get config");
    }
}

void setup()
{
  Serial.begin(115200);
  out.setNotifyAudioChange(analog);
  auto cfg = defaultConfig();

  out.begin(cfg);

  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
  pinMode(BUTTON_LED_PIN, OUTPUT);
  digitalWrite(BUTTON_LED_PIN, 0);
  leds.begin();
  boot_animation();

  setup_wifi();
  setup_websocket();
  
  get_config_from_master();

  attachInterrupt(BUTTON_PIN, on_button_press, RISING);
}

void loop()
{
  webSocket.loop();
  set_led_button_state(enabled);

  if (pressed && enabled)
  {
    HTTPClient http;

    String serverPath = "http://192.168.0.225/buzz?id=" + WiFi.macAddress();

    // Your Domain name with URL path or IP address with path
    http.begin(serverPath.c_str());

    // If you need Node-RED/server authentication, insert user and password below
    // http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");

    // Send HTTP GET request
    int httpResponseCode = http.GET();
    if (httpResponseCode == 200)
    {
      enabled = false;
      activate_buzzer(true);
      pressed = false;
    }

    pressed = false;
    Serial.println(http.getString());
  }
}
