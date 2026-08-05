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

// Вспомогательная функция для рисования прогресс-бара
void drawProgressBar(int x, int y, int width, int height, int percentage, uint16_t color) {
  if (percentage < 0) percentage = 0;
  if (percentage > 100) percentage = 100;
  
  // Рамка
  tft.drawRect(x, y, width, height, TFT_WHITE);
  // Заполнение
  if (percentage > 0) {
    int fillWidth = (width - 2) * percentage / 100;
    tft.fillRect(x + 1, y + 1, fillWidth, height - 2, color);
  }
}

// Функция для определения цвета в зависимости от загрузки
uint16_t getLoadColor(int percentage) {
  if (percentage < 50) return TFT_GREEN;
  if (percentage < 75) return TFT_YELLOW;
  return TFT_RED;
}

void drawStatBar(int x, int y, int width, int height, const char* label, int percentage, uint16_t color, int textSize) {
  // Рисуем метку
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(textSize);
  tft.drawString(label, x, y);
  
  // Рисуем значение
  String value = String(percentage) + "%";
  int valueX = x + width - tft.textWidth(value);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(value, valueX, y);
  
  // Рисуем бар
  drawProgressBar(x, y + textSize * 8 + 2, width, height, percentage, color);
}

void handleStat(JsonVariant data) {
  // Проверяем, что data - это объект
  if (!data.is<JsonObject>()) {
    return;
  }
  
  JsonObject stat = data.as<JsonObject>();

  // Очищаем весь экран
  tft.fillScreen(TFT_BLACK);
  
  // Получаем данные с проверкой на существование
  int cores = stat["cores"] | 1;
  JsonArray percs = stat["percs"].as<JsonArray>();
  int mem = stat["mem"] | 0;
  int cputemp = stat["cputemp"] | 0;
  
  // Получаем строки с проверкой
  const char* nettx = stat["nettx"] | "0MB/s";
  const char* netrx = stat["netrx"] | "0MB/s";
  int bat = stat["bat"] | 0;
  const char* batstat = stat["batstat"] | "Unknown";

  // --- Отображение загрузки ядер ---
  int totalLoad = 0;
  int coreCount = percs.size();
  
  // Если нет данных о ядрах, выходим
  if (coreCount == 0) {
    tft.setTextSize(3);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("No CPU data", 30, 100);
    return;
  }
  
  int startY = 5;
  int barHeight = 20;
  int barStartX = 10;
  int barWidth = 300;
  
  // ---- 1. CPU BAR ----
  int totalPerc = 0;
  for (int i = 0; i < coreCount; i++) {
    totalPerc += percs[i] | 0;
  }
  int avgLoad = coreCount > 0 ? totalPerc / coreCount : 0;
  
  // Рисуем CPU с процентом и температурой
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("CPU", barStartX, startY);
  
  // Процент CPU прижат к надписи
  String cpuStr = String(avgLoad) + "%";
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  int cpuX = barStartX + tft.textWidth("CPU") + 5;
  tft.drawString(cpuStr, cpuX, startY);
  
  // Температура справа
  String tempStr = String(cputemp) + "C";
  uint16_t tempColor = TFT_GREEN;
  if (cputemp > 75) tempColor = TFT_RED;
  else if (cputemp > 60) tempColor = TFT_YELLOW;
  tft.setTextColor(tempColor, TFT_BLACK);
  int tempX = barStartX + barWidth - tft.textWidth(tempStr);
  tft.drawString(tempStr, tempX, startY);
  
  // Бар CPU
  drawProgressBar(barStartX, startY + 18, barWidth, barHeight, avgLoad, getLoadColor(avgLoad));
  startY += barHeight + 22;
  
  // ---- 2. CPU CORES (всегда в 2 строки, ширина максимальная) ----
  int coresPerRow = (coreCount + 1) / 2;
  int barH = barHeight;
  
  // Ширина бара - максимально возможная
  int coreBarWidth = (barWidth - (coresPerRow - 1) * 4) / coresPerRow;
  // Ограничиваем только минимум, чтобы не было слишком узко
  if (coreBarWidth < 20) coreBarWidth = 20;
  
  // Отступ между строками минимальный
  int rowSpacing = 4;
  
  for (int row = 0; row < 2; row++) {
    int rowStart = row * coresPerRow;
    int rowEnd = min(rowStart + coresPerRow, coreCount);
    int rowCols = rowEnd - rowStart;
    
    if (rowCols == 0) break;
    
    // Распределяем бары равномерно по всей ширине
    int rowWidth = rowCols * (coreBarWidth + 4) - 4;
    int rowStartX = barStartX + (barWidth - rowWidth) / 2;
    int y = startY + row * (barH + rowSpacing);
    
    for (int i = rowStart; i < rowEnd; i++) {
      int perc = percs[i] | 0;
      int x = rowStartX + (i - rowStart) * (coreBarWidth + 4);
      
      // Прогресс-бар без процентов
      drawProgressBar(x, y, coreBarWidth, barH, perc, getLoadColor(perc));
    }
  }
  
  // Обновляем startY после двух строк ядер
  startY += 2 * (barH + rowSpacing) + 2;
  
  // ---- 3. MEMORY BAR ----
  drawStatBar(barStartX, startY, barWidth, barHeight, "MEMORY", mem, TFT_BLUE, 2);
  startY += barHeight + 22;
  
  // ---- 4. BATTERY BAR ----
  uint16_t batColor = TFT_GREEN;
  if (bat < 20) batColor = TFT_RED;
  else if (bat < 50) batColor = TFT_YELLOW;
  
  // Статус батареи
  String batStatusText = "";
  if (String(batstat) == "Charging") {
    batStatusText = " CHARGING";
  } else if (String(batstat) == "Discharging") {
    batStatusText = " DISCHARGING";
  } else if (String(batstat) == "Full") {
    batStatusText = " FULL";
  } else {
    batStatusText = " " + String(batstat);
  }
  
  // Рисуем метку батареи со статусом
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  String batLabel = "BATTERY" + batStatusText;
  tft.drawString(batLabel, barStartX, startY);
  
  // Рисуем значение
  String value = String(bat) + "%";
  int valueX = barStartX + barWidth - tft.textWidth(value);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(value, valueX, startY);
  
  // Рисуем бар
  drawProgressBar(barStartX, startY + 18, barWidth, barHeight, bat, batColor);
  startY += barHeight + 22;
  
  // ---- 5. NETWORK ----
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("TX:", barStartX, startY);
  tft.drawString("RX:", barStartX + 150, startY);
  
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(nettx, barStartX + 50, startY);
  tft.drawString(netrx, barStartX + 200, startY);
  
  // ---- 6. LAYOUT INFO (белый цвет, прижат вправо) ----
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  String layoutStr = "Layout: " + String(curLayout + 1) + "/" + String(layoutNum);
  int layoutX = barStartX + barWidth - tft.textWidth(layoutStr);
  tft.drawString(layoutStr, layoutX, startY + 28);
}

// Dispatch one host line. Envelope: {"type":"...","data":...}
void handleHostMessage(const String &line) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, line);
  if (error) {
    return;
  }

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
