#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <NeoPixelBus.h>
#include "SimpleTimer.h"
#include "fonts/6x8_vertikal_MSB_1.h"
#include <WebSocketsServer.h>
#include "index_html.h"

bool debug = true;
#define SerialDebug(text)   Serial.print(text);
#define SerialDebugln(text) Serial.println(text);

char MyIp[16];
char MyHostname[16];

ESP8266WebServer server = ESP8266WebServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);

#define Width       13
#define pixelCount (Width*Width)
#define pixelPin    2          // should be ignored because of NeoEsp8266Dma800KbpsMethod
NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart800KbpsMethod> strip(pixelCount, pixelPin);
NeoTopology <ColumnMajorAlternatingLayout> topo(Width, Width);
RgbColor black = RgbColor(0);

#define FontWidth 6
#define MaxTextLen  255
byte Text[MaxTextLen];
int TextPos=0;
int TextLen=0;
SimpleTimer TextTimer;
RgbColor TextCol=RgbColor(128,128,128);

byte Mode=1;

byte RenderLetter(byte * pos, const char x) {
  byte l=0;
  for (int i=0 ; i<FontWidth ; i++) {
    if (font[x][i] != 0) {
      pos[l] = font[x][i];
      l++;
    }
  }
  return l;
}
void RenderText(const uint8_t * text, size_t len) {
  const uint8_t * p = text;
  TextPos=TextLen=0;

  for (size_t i=0 ; i<len && i<MaxTextLen-8 ; i++) {
    TextLen += RenderLetter(&Text[TextLen], *p);
    Text[TextLen]=0; TextLen++;   // one row free space
    p++;
  }
}
void RenderText(const char * text) {
  RenderText((const uint8_t *) text, strlen(text));
}
void RenderText(String s){
  RenderText((const uint8_t *)s.c_str(), s.length());
}

void ShowText() {
  TextPos++;
  if (TextPos > TextLen) TextPos=0;
  strip.ClearTo(black);
  for (uint8_t i=0 ; i<Width ; i++) {
    if (TextPos+i >= TextLen) break;
    for (int x=0 ; x<8 ; x++) {
      if ((Text[TextPos+i] & (1<<x)) != 0) {
        strip.SetPixelColor(topo.Map(2+x,Width-i-1), TextCol);
      }
    }
  }
  strip.Show();
}

#define MaxStones   7
const uint8_t Stones[MaxStones] = {0x0F, 0x2E, 0x4E, 0x8E, 0x6C, 0xCC, 0xC6};
struct sStone {
  int8_t    x,y;
  uint8_t   id;
  int8_t   dir;
  RgbColor  col;
} Stone, NewStone;
SimpleTimer TetrisTimer;
int TetrisTimerId;
int Lines=0;
int Level=1;
void UpdateClient() {
  String s;
  s += "Lines: ";
  s += Lines;
  s += "<br />Level: ";
  s += Level;
  webSocket.broadcastTXT(s);
}
void MoveStoneDown();
void UpdateLevel() {
  int interval = 1000 - (50*Level);
  TetrisTimer.deleteTimer(TetrisTimerId);
  TetrisTimerId = TetrisTimer.setInterval(interval, MoveStoneDown);
}
void DrawStone(bool draw) {
  for (int8_t y=0 ; y<4 ; y++) {
    for (int8_t x=0 ; x<2 ; x++) {
      if ((Stones[Stone.id] & (1<<(y+x*4))) != 0) {
        int8_t x1=Stone.x, y1=Stone.y;
        switch (Stone.dir) {
          case 0: x1+=x-1; y1+=y-2; break;
          case 1: x1+=y-2; y1-=x-1; break;
          case 2: x1-=x-1; y1-=y-2; break;
          case 3: x1-=y-2; y1+=x-1; break;
        }
        if (x1<Width) strip.SetPixelColor(topo.Map(x1,y1), draw?Stone.col:black);
      }
    }
  }
  strip.Show();
}
void CalcNewStone(uint8_t dir) {
  NewStone=Stone;
  switch (dir) {
    case 1: NewStone.x--; break;    // Down
    case 2: NewStone.y++; break;    // Left
    case 3: NewStone.y--; break;    // Right
    case 4: NewStone.dir--;         // Rotate left
            if (NewStone.dir<0) NewStone.dir=3; break;
    case 5: NewStone.dir++;         // Rotate right
            if (NewStone.dir>3) NewStone.dir=0; break;
  }
}
void NextStone() {
  Stone.x=Width;
  Stone.y=Width/2-2;
  Stone.id=random(0,MaxStones);
  Stone.dir=0;
  Stone.col=RgbColor(random(5,255), random(5,255), random(5,255));
}
uint8_t CheckSpace() {
  for (int8_t y=0 ; y<4 ; y++) {
    for (int8_t x=0 ; x<2 ; x++) {
      if ((Stones[NewStone.id] & (1<<(y+x*4))) != 0) {
        int8_t x1=NewStone.x, y1=NewStone.y;
        switch (NewStone.dir) {
          case 0: x1+=x-1; y1+=y-2; break;
          case 1: x1+=y-2; y1-=x-1; break;
          case 2: x1-=x-1; y1-=y-2; break;
          case 3: x1-=y-2; y1+=x-1; break;
        }
        if (y1<0 || y1>=Width) return 1;
        if (x1<0) return 2;
        if (strip.GetPixelColor(topo.Map(x1,y1)).CalculateBrightness() != 0) return 3;
      }
    }
  }
  return 0;
}
void CheckRows() {
  for (int x=0 ; x<Width ; ) {
    uint8_t cnt=0;
    for (int y=0 ; y<Width ; y++) {
      if (strip.GetPixelColor(topo.Map(x,y)).CalculateBrightness() != 0)
        cnt++;
    }
    if (cnt == Width) {
      for (int x1=x ; x1<Width-1 ; x1++) {
        for (int y=0 ; y<Width ; y++) {
          strip.SetPixelColor(topo.Map(x1,y), strip.GetPixelColor(topo.Map(x1+1,y)));
        }
      }
      for (int y=0 ; y<Width ; y++) {
        strip.SetPixelColor(topo.Map(Width,y), black);
      }
      strip.Show();
      Lines++;
      if ((Lines%10) == 0) { Level++; UpdateLevel(); }
      UpdateClient();
      continue;
    }
    x++;
  }
}
void MoveStoneDown() {
  CalcNewStone(1);
  DrawStone(false);
  switch (CheckSpace()) {
    case 0:     // OK!
    case 1:     // Move to far left/right
        Stone=NewStone;
      break;
    case 2:     // reached bottom
    case 3:     // stone blocked
        DrawStone(true);
        CheckRows();
        NextStone();
      break;
  }
  DrawStone(true);
}
void NewGame() {
  strip.ClearTo(black);
  strip.Show();
  NextStone();
  Lines=0;
  Level=1;
  UpdateLevel();
  UpdateClient();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t len) {
  switch(type) {
    case WStype_DISCONNECTED:
        break;
    case WStype_CONNECTED: {
        IPAddress ip = webSocket.remoteIP(num);
        webSocket.sendTXT(num, "Connected");
        if (Mode!=2) {
          Mode=2;
          NewGame();
        }
      } break;
    case WStype_TEXT: {
        // RenderText(payload, len);
        switch (*payload) {
          case 'd': CalcNewStone(1); break;
          case 'l': CalcNewStone(2); break;
          case 'r': CalcNewStone(3); break;
          case 'x': CalcNewStone(4); break;
          case 'y': CalcNewStone(5); break;
          case 'n': NewGame();  break;
        }
        DrawStone(false);
        switch (CheckSpace()) {
          case 0:     // OK!
              Stone=NewStone;
            break;
          case 1:     // Move to far left/right
          case 2:     // reached bottom
          case 3:     // stone blocked
            break;
        }
        DrawStone(true);
      } break;
  }
}

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(200);
  delay(100);

  WiFiManager wifiManager;
  wifiManager.setDebugOutput(debug);
  wifiManager.setTimeout(3*60);
  if(!wifiManager.autoConnect()) {
    delay(3000);
    ESP.reset();
    delay(5000);
  }
  IPAddress MyIP=WiFi.localIP();
  snprintf(MyIp, 16, "%d.%d.%d.%d", MyIP[0], MyIP[1], MyIP[2], MyIP[3]);
  snprintf(MyHostname, 15, "ESP-%08x", ESP.getChipId());
  SerialDebug("ESP-Hostname: ");
  SerialDebugln(MyHostname);
  
  RenderText(WiFi.localIP().toString().c_str());
  TextTimer.setInterval(150, ShowText);
  TetrisTimerId = TetrisTimer.setInterval(1000, MoveStoneDown);

  server.on("/", []() {
    server.send(200, "text/html", index_html);
  });
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  strip.Begin();
  strip.ClearTo(black);
  strip.Show();
}

void loop() {
  server.handleClient();
  webSocket.loop();

  switch (Mode) {
    case 1: TextTimer.run(); TextCol=RgbColor(random(1,255), random(1,255), random(1,255));
      break;
    case 2: TetrisTimer.run();
      break;
    default: Mode=1;
  }
}
