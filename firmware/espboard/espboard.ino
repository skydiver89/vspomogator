#include <TFT_eSPI.h>
#include <SPI.h>
#include <EncButton.h>
#include <ArduinoJson.h>

#define INITREQ "begincmdinitendcmd"
#define INITOK "begincmdinitokendcmd"
#define STATREQ "begincmdstatendcmd"
#define INITINTERVAL 1000
#define STATINTERVAL 1000
#define RX_LINE_MAX 2048

bool initialized = false;
unsigned long lastInitSent = 0;
unsigned long lastStatSent = 0;
bool STATMODE = false;
int curLayout = 0;
int layoutNum = 0;
JsonDocument conf;
String rxLine;

const int numBtns = 11;
Button btns[] = {Button(13), Button(14), Button(16), Button(17), Button(19),
                 Button(21), Button(22), Button(25), Button(26), Button(27), Button(33)};
VirtButton resetButton;

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(100);
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(3);
  tft.drawString("Initializing...", 25, 110);
  tft.setTextSize(2);
  rxLine.reserve(256);
}

// Non-blocking: never deserializeJson(Serial) — it blocks loop()/buttons.
bool pollSerialLine(String &line) {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (rxLine.length() == 0)
        continue;
      line = rxLine;
      rxLine = "";
      return true;
    }
    rxLine += c;
    if (rxLine.length() > RX_LINE_MAX)
      rxLine = "";
  }
  return false;
}

void drawLayout() {
  JsonArray root = conf.as<JsonArray>();
  JsonObject layout = root[curLayout].as<JsonObject>();
  JsonArray buttons = layout["Buttons"].as<JsonArray>();
  int x = 0;
  int y = 20;
  for (int i = 0; i < 10; i++) {
    String label = "";
    if (i < buttons.size()) {
      JsonObject btn = buttons[i].as<JsonObject>();
      label = String(btn["Label"]);
    }

    while (label.length() < 11)
      label += " ";
    if (i >= 5)
      x = 160;
    if (i == 0 || i == 5)
      tft.setTextColor(TFT_WHITE, TFT_BLUE);
    if (i == 1 || i == 6)
      tft.setTextColor(TFT_WHITE, TFT_RED);
    if (i == 2 || i == 7)
      tft.setTextColor(TFT_BLACK, TFT_YELLOW);
    if (i == 3 || i == 8)
      tft.setTextColor(TFT_BLACK, TFT_WHITE);
    if (i == 4 || i == 9)
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(i + 1) + ":" + label, x, y);
    y += 30;
    if (i == 4)
      y = 20;
  }
  tft.setTextColor(TFT_BLACK, TFT_MAGENTA);
  tft.drawString("11:Mode", 0, 170);
  tft.drawString("Hold 11:Layout", 150, 170);
  tft.drawString("Hold 1+10:Reboot", 0, 200);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Layout: " + String(curLayout + 1), 200, 220);
}

void requestInit() {
  unsigned long now = millis();
  if (now - lastInitSent < INITINTERVAL)
    return;
  lastInitSent = now;
  Serial.print(INITREQ);
}

void requestStat() {
  unsigned long now = millis();
  if (now - lastStatSent < STATINTERVAL)
    return;
  lastStatSent = now;
  Serial.print(STATREQ);
}

// --- host → device messages (newline-delimited JSON) ---

void handleConfig(JsonVariant data) {
  if (!data.is<JsonArray>())
    return;
  JsonArray root = data.as<JsonArray>();
  if (root.size() == 0)
    return;

  conf.clear();
  conf.set(data);
  layoutNum = root.size();
  if (curLayout >= layoutNum)
    curLayout = 0;

  bool wasInit = initialized;
  initialized = true;
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  drawLayout();
  if (!wasInit)
    Serial.print(INITOK);
}

void handleStat(JsonVariant data) {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Layout: " + String(curLayout + 1), 200, 220);
}

// Dispatch one host line. Envelope: {"type":"...","data":...}
void handleHostMessage(const String &line) {
  JsonDocument doc;
  if (deserializeJson(doc, line))
    return;

  const char *type = doc["type"];
  if (!type)
    return;

  if (strcmp(type, "config") == 0) {
    handleConfig(doc["data"]);
    return;
  }

  if (strcmp(type, "stat") == 0) {
    handleStat(doc["data"]);
    return;
  }
}

void pollHost() {
  String line;
  while (pollSerialLine(line))
    handleHostMessage(line);
}

void loop() {
  for (int i = 0; i < numBtns; i++)
    btns[i].tick();
  resetButton.tick(btns[0], btns[9]);

  if (resetButton.hold()) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(3);
    tft.drawString("Rebooting...", 40, 110);
    ESP.restart();
  }

  pollHost();

  if (!initialized) {
    requestInit();
    return;
  }

  if (STATMODE)
    requestStat();

  for (int i = 0; i < numBtns; i++) {
    if (btns[i].click() && i != 10)
      Serial.print("begincmdbutton " + String(curLayout + 1) + "-" + String(i + 1) + "endcmd");
  }
  if (btns[10].click()){
    tft.fillScreen(TFT_BLACK);
    STATMODE = !STATMODE;
    if(!STATMODE)
      drawLayout();
  }
  if (btns[10].hold()) {
    curLayout++;
    if (curLayout >= layoutNum)
      curLayout = 0;
    drawLayout();
  }
}
