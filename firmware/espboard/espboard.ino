#include <TFT_eSPI.h>
#include <SPI.h>
#include <EncButton.h>
#include <ArduinoJson.h>
#include <string>

#define INITREQ "begincmdinitendcmd"
#define STATREQ "begincmdstatendcmd"
#define INITINTERVAL 1000

bool initialized = false;
int lastInitSent = 0;
int curLayout = 0;
int layoutNum = 0;
bool showStat = false;
JsonDocument conf;

const int numBtns = 11;
Button btns[] = {Button(13), Button(14), Button(16), Button(17), Button(19),
                 Button(21), Button(22), Button(25), Button(26), Button(27), Button(33)};
VirtButton resetButton;

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  //Serial.setDebugOutput(false);
  /*while (!Serial)
    continue;*/
  tft.init();
  tft.setRotation(3); // 0, 1, 2, 3 — поворот экрана
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(3);
  tft.drawString("Initializing...", 25, 110);
  tft.setTextSize(2);
}

JsonDocument readJson(bool *ok){
  JsonDocument doc;
  if(Serial.available() > 0){
    DeserializationError error = deserializeJson(doc, Serial);
    if (error) {
      while(Serial.available() > 0)
        Serial.read();
      *ok = false;
      return doc;
    }
  } else {
    *ok = false;
    return doc;
  }
  *ok = true;
  return doc;
}

void drawLayout(){
  JsonArray root = conf.as<JsonArray>();
  JsonObject layout = root[curLayout].as<JsonObject>();
  JsonArray buttons = layout["Buttons"].as<JsonArray>();
  int x = 0;
  int y = 20;
  for(int i=0;i<10;i++){
    String label = "";
    for(int j=0;j<buttons.size();j++){
      JsonObject btn = buttons[j].as<JsonObject>();
      int num = btn["Num"];
      if(num-1 == i){
        label = String(btn["Label"]);
        break;
      }
    }
    while(label.length() < 11)
      label += " ";
    if(i>=5)
      x = 160;
    if(i==0 || i==5)
      tft.setTextColor(TFT_WHITE, TFT_BLUE);
    if(i==1 || i==6)
      tft.setTextColor(TFT_WHITE, TFT_RED);
    if(i==2 || i==7)
      tft.setTextColor(TFT_BLACK, TFT_YELLOW);
    if(i==3 || i==8)
      tft.setTextColor(TFT_BLACK, TFT_WHITE);
    if(i==4 || i==9)
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(i+1) + ":" + label, x, y);
    y += 30;
    if(i == 4)
      y = 20;
  }
  tft.setTextColor(TFT_BLACK, TFT_MAGENTA);
  tft.drawString("11:Mode", 0, 170);
  tft.drawString("Hold 11:Layout", 150, 170);
  tft.drawString("Hold 1+10:Reboot", 0, 200);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Layout: " + String(curLayout+1), 200, 220);
}

bool initConfig(){
  if(Serial.availableForWrite() > 0){
    if(millis() - lastInitSent > INITINTERVAL){
      Serial.print(INITREQ);
      lastInitSent = millis();
    }
    bool ok;
    conf = readJson(&ok);
    if(!ok){
      return false;
    }
    tft.fillScreen(TFT_BLACK);
    JsonArray root = conf.as<JsonArray>();
    for (JsonObject layout : root) {
      JsonArray buttons = layout["Buttons"].as<JsonArray>();
      for (JsonObject btn : buttons) {
        int num = btn["Num"];
        const char* command = btn["Command"];
        const char* label = btn["Label"];
      }
      layoutNum++;
    }
    initialized = true;
    drawLayout();
    return true;
  }
}

void loop() {
  for(int i=0;i<numBtns;i++)
    btns[i].tick();
  resetButton.tick(btns[0], btns[9]);
  if(resetButton.hold())
    ESP.restart();

  if(!initialized){
    if (!initConfig())
      return;
  }
  
  for(int i=0;i<numBtns;i++){
    if(btns[i].click()){
      //tft.println("Click " + String(i+1));
      if(i != 10)
        Serial.print("begincmdbutton " + String(curLayout+1) + "-" + String(i+1) + "endcmd");
    }
  }
  if(btns[10].hold()){
    curLayout++;
    if(curLayout >= layoutNum)
      curLayout = 0;
    drawLayout();
  } 
}
