#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_NeoPixel.h>


#define LED_PIN 16
#define LEN_COUNT 1
#define RESET_BUTTON_PIN 18

const char *ssid = "";
const char *password = "";
bool reset = false;

Adafruit_NeoPixel leds(LEN_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

char *page = "<!DOCTYPE html><html lang='en'><head> <meta charset='UTF-8'> <meta name='viewport' content='width=device-width, initial-scale=1.0'> <title>Game Buzzer Status</title> <style>html, body { height: 100%; margin: 0; display: flex; flex-direction: column;}.container { display: flex; flex: 1; flex-direction: column; justify-content: space-between; /* Center the columns and button vertically */ align-items: center; /* Center everything horizontally */}.columns { display: flex; justify-content: space-around; width: 100%; flex-grow: 1;}.column { justify-content: center; flex: 1; margin: 5px; text-align: center; color: white; display: flex; flex-direction: column; background-color: #AED9E0; border: 2px solid teal; border-radius: 10px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.2);}.buzzer { font-size: 90px; align-self: center; border: 4px solid white; border-radius: 50%; width: 110px;}#resetButton { height: 50px; width: 100%; margin-top: 5px; background-color: #3498db; color: white; border: none; border-radius: 5px; cursor: pointer;}</style></head><body> <div class='container'> <div class='columns'> <div class='column' style='background-color: rgb(174, 217, 224);' inactive-color='rgb(174, 217, 224)' active-color='rgb(52, 152, 219)' active='false'> <div class='buzzer' id='buzzer1'> 1 </div> </div> <div class='column' style='background-color: rgb(184, 224, 168);' inactive-color='rgb(184, 224, 168)' active-color='rgb(46, 204, 113)' active='false'> <div class='buzzer' id='buzzer2'> 2 </div> </div> <div class='column' style='background-color: rgb(186 191 191);' inactive-color='rgb(186 191 191)' active-color='rgb(255, 255, 255)' active='false'> <div class='buzzer' id='buzzer3'> 3 </div> </div> <div class='column' style='background-color: rgb(255, 107, 107);' inactive-color='rgb(255, 107, 107)' active-color='rgb(231, 76, 60)' active='false'> <div class='buzzer' id='buzzer4'> 4 </div> </div> <div class='column' style='background-color: rgb(245, 243, 132);' inactive-color='rgb(245, 243, 132)' active-color='rgb(241, 196, 15)' active='false'> <div class='buzzer' id='buzzer5'> 5 </div> </div> </div> <button id='resetButton'>Reset</button> </div></body><script>const websocket = new WebSocket( `ws://${window.location.host}/ws`, 'polling',);const buzzers = document.querySelectorAll('.column');buzzers.forEach((buzzer, index) => { buzzer.addEventListener('click', () => { const column = buzzer.closest('.column'); const isActive = column.isActive; if (isActive) { column.isActive = false; column.style.backgroundColor = column.getAttribute('inactive-color'); fetch('/off?id='+index, { method: 'GET' }); } else { fetch('/on?id='+index, { method: 'GET' }); column.isActive = true; column.style.backgroundColor = column.getAttribute('active-color'); } });});const resetButton = document.getElementById('resetButton');resetButton.addEventListener('click', () => { buzzers.forEach((buzzer) => { const column = buzzer.closest('.column'); column.isActive = false; column.style.backgroundColor = column.getAttribute('inactive-color'); }); fetch('/reset', { method: 'GET' }) .then((response) => { console.log('Reset request sent.'); }) .catch((error) => { console.error('Error sending reset request:', error); });});websocket.onmessage = (event) => { console.log(event.data); console.log(new Date()/1); if (event.data.split(' ')[0] == 'buzz') { buzzers[+event.data.split(' ')[1]].style.backgroundColor = buzzers[+event.data.split(' ')[1]].getAttribute('active-color'); }};</script></html>";
bool buzzed = false;
//Color correspondance       blue            green                  white                         red                 yellow
String buzzers[5] = { "A8:03:2A:61:64:20",  "B0:A7:32:33:8F:5C",         "",             "24:DC:C3:A8:98:90",  "B0:A7:32:28:49:58" };
String colors[5] =  { "0,0,150",            "0,150,0",            "150,150,150",          "150,0,0",            "150,150,0" };

void turn_on_led(int r, int g, int b)
{
  for (int i = 0; i < LEN_COUNT; i++)
  {
    leds.setPixelColor(i, leds.Color(r, g, b));
  }
  leds.show();
}

void turn_off_led()
{
  leds.clear();
  leds.show();
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  Serial.println("onWsEvent");
  if(type == WS_EVT_CONNECT){
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  } else if(type == WS_EVT_DISCONNECT){
    Serial.printf("ws[%s][%u] disconnect\n", server->url(), client->id());
  } else if(type == WS_EVT_ERROR){
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    String msg = "";
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      Serial.printf("%s\n",msg.c_str());

      if(info->opcode == WS_TEXT)
        client->text("I got your text message");
      else
        client->binary("I got your binary message");
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if(info->index == 0) {
        //if(info->num == 0)
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      Serial.printf("%s\n",msg.c_str());

      if((info->index + len) == info->len){
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          if(info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}

void IRAM_ATTR on_reset_press()
{
  reset=true;
}

void setup()
{
  //----------------------------------------------------Serial
  Serial.begin(115200);
  Serial.println("\n");

  pinMode(RESET_BUTTON_PIN, INPUT_PULLDOWN);

  //----------------------------------------------------WIFI
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid, password);
  
  WiFi.setSleep(false);
  Serial.print("Tentative de connexion...");

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }

  Serial.println("\n");
  Serial.println("Connexion etablie!");
  Serial.print("Adresse IP: ");
  Serial.println(WiFi.localIP());



  //---------------------------Soft AP
  //  Serial.print("Setting AP (Access Point)…");
  // // Remove the password parameter, if you want the AP (Access Point) to be open
  // WiFi.softAP("game buzzer", "blindtest", 1, 0, 5);

  // IPAddress IP = WiFi.softAPIP();
  // Serial.print("AP IP address: ");
  // Serial.println(IP);

  //----------------------------------------------------SERVER
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(200, "text/html", page);
  });

  server.on("/buzz", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    AsyncWebParameter* p = request->getParam(0);
    Serial.print("Param name: ");
    Serial.println(p->name());
    Serial.print("Param value: ");
    Serial.println(p->value());

    if (!buzzed) {
      Serial.println("BUZZ");
      buzzed = true;
      ws.textAll("disable");
    if (p->value().compareTo("A8:03:2A:61:64:20") == 0) { //blue
      ws.textAll("buzz 0");
      turn_on_led(0,0,150);
    } else if (p->value().compareTo("B0:A7:32:33:8F:5C") == 0) { //green
      ws.textAll("buzz 1");
      turn_on_led(0,150,0);
    } else if (p->value().compareTo("4567") == 0) { //white
      ws.textAll("buzz 2");
    } else if (p->value().compareTo("24:DC:C3:A8:98:90") == 0) { //red
      ws.textAll("buzz 3");
      turn_on_led(150,0,0);
    } else if (p->value().compareTo("B0:A7:32:28:49:58") == 0) { //yellow
      ws.textAll("buzz 4");
      turn_on_led(150,150,0);
    } else {
      buzzed = true;
      request->send(404, "text/html", "config not found");
    }
      request->send(200, "text/html", "buzz");  // Envoie de la page HTML




    } else {
      Serial.println("TOO Late");
      request->send(403, "text/html", "nop");  // Envoie de la page HTML
    }
  });

  
  server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request)
  { 
    AsyncWebParameter* p = request->getParam(0);
    Serial.print("Param name: ");
    Serial.println(p->name());
    Serial.print("Param value: ");
    Serial.println(p->value());
    request->send(200, "text/html", "on");
    ws.textAll("on " + buzzers[p->value().toInt()]);
  });

  server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    AsyncWebParameter* p = request->getParam(0);
    Serial.print("Param name: ");
    Serial.println(p->name());
    Serial.print("Param value: ");
    Serial.println(p->value());
    request->send(200, "text/html", "off");
    ws.textAll("off " + buzzers[p->value().toInt()]);
  });
  
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    buzzed = false;
    request->send(200, "text/html", "reset");
    ws.textAll("reset");
    turn_off_led();
  });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    AsyncWebParameter* p = request->getParam(0);
    Serial.print("Param name: ");
    Serial.println(p->name());
    Serial.print("Param value: ");
    Serial.println(p->value());
    Serial.println("------");

    if (p->value().compareTo("A8:03:2A:61:64:20") == 0) { //blue
      request->send(200, "text/html", "0,0,150");
    } else if (p->value().compareTo("B0:A7:32:33:8F:5C") == 0) { //green
      request->send(200, "text/html", "0,150,0");
    } else if (p->value().compareTo("ABCD") == 0) { //white
      request->send(200, "text/html", "150,150,150");
    } else if (p->value().compareTo("24:DC:C3:A8:98:90") == 0) { //red
      request->send(200, "text/html", "150,0,0");
    } else if (p->value().compareTo("B0:A7:32:28:49:58") == 0) { //yellow
      request->send(200, "text/html", "150,150,0");
    } else {
      request->send(404, "text/html", "config not found");
    }
  });

  
  ws.onEvent(onWsEvent);

  server.addHandler(&ws);
  
  // server.addHandler(&events);
  server.begin();
  
  attachInterrupt(RESET_BUTTON_PIN, on_reset_press, RISING);
  Serial.println("Serveur actif!");

  turn_on_led(0,150,0);
  sleep(1);
  turn_off_led();
  sleep(1);
  turn_on_led(0,150,0);
  sleep(1);
  turn_off_led();

}

void loop()
{
  if (reset) {
    ws.textAll("reset");
    turn_off_led();
    buzzed = false;
    reset = false;
  }
  
}