#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Registros I2C del AS5600
#define AS5600_ADDR     0x36
#define REG_STATUS      0x0B
#define REG_RAW_ANGLE   0x0C

// Pin del Pulsador de Calibracion en Arduino Uno
const byte CALIB_PIN = 2;

// Variables de Calibracion (0 a 4095)
uint16_t rawMin = 500;   // Valor asignado a 0%
uint16_t rawMax = 3000;  // Valor asignado a 100%

// Filtro Pasa-Bajas para estabilidad
float filteredRaw = 0.0;
const float ALPHA = 0.15; // Factor de filtrado (menor = mas suave)

// Funciones
uint16_t readRawAngle();
uint8_t readStatus();
void executeCalibration();
void drawErrorScreen(const __FlashStringHelper* title, const __FlashStringHelper* msg);

void setup() {
  Wire.begin();
  pinMode(CALIB_PIN, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;); // Si la pantalla falla, detiene ejecucion
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  filteredRaw = readRawAngle();
}

void loop() {
  // 1. Verificacion del boton de calibracion
  if (digitalRead(CALIB_PIN) == LOW) {
    executeCalibration();
  }

  // 2. Lectura directa y filtrado de datos
  uint16_t currentRaw = readRawAngle();
  uint8_t statusReg = readStatus();

  filteredRaw = (ALPHA * currentRaw) + ((1.0 - ALPHA) * filteredRaw);

  // 3. Calculos de Angulo y Porcentaje
  float angleDegrees = (filteredRaw * 360.0) / 4096.0;

  float percentage = 0.0;
  if (rawMax != rawMin) {
    percentage = ((filteredRaw - rawMin) * 100.0) / (float)(rawMax - rawMin);
  }

  // Limitar rango
  if (percentage < 0.0) percentage = 0.0;
  if (percentage > 100.0) percentage = 100.0;

  // 4. Banderas de estado magnetico del AS5600
  bool magnetDetected  = (statusReg & 0x20) != 0; // Bit 5: MD
  bool magnetTooWeak   = (statusReg & 0x10) != 0; // Bit 4: ML
  bool magnetTooStrong = (statusReg & 0x08) != 0; // Bit 3: MH
  
  // Margen de tolerancia fuera de rango calibrado
  bool outOfBounds = (filteredRaw < (rawMin - 150)) || (filteredRaw > (rawMax + 150));

  // 5. Salida en Pantalla OLED
  display.clearDisplay();

  if (!magnetDetected) {
    drawErrorScreen(F("ADVERTENCIA"), F("IMAN NO DETECTADO"));
  } else if (magnetTooWeak) {
    drawErrorScreen(F("ADVERTENCIA"), F("CAMPO MAG. DEBIL"));
  } else if (magnetTooStrong) {
    drawErrorScreen(F("ADVERTENCIA"), F("IMAN MUY CERCA"));
  } else if (outOfBounds) {
    drawErrorScreen(F("ADVERTENCIA"), F("FUERA DE RANGO"));
  } else {
    // Interfaz Principal
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("PEDAL ELECTRONICO"));
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    display.setCursor(0, 16);
    display.print(F("Angulo: "));
    display.print(angleDegrees, 1);
    display.print(F(" deg"));

    display.setCursor(0, 30);
    display.setTextSize(2);
    display.print(F("ACC: "));
    display.print((int)percentage);
    display.print(F("%"));

    // Barra Grafica de Aceleracion
    display.drawRect(0, 52, 128, 12, SSD1306_WHITE);
    int barWidth = map((int)percentage, 0, 100, 0, 124);
    display.fillRect(2, 54, barWidth, 8, SSD1306_WHITE);
  }

  display.display();
  delay(20);
}

uint16_t readRawAngle() {
  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(REG_RAW_ANGLE);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2);
  
  if (Wire.available() >= 2) {
    uint16_t highByte = Wire.read();
    uint16_t lowByte  = Wire.read();
    return ((highByte << 8) | lowByte) & 0x0FFF;
  }
  return 0;
}

uint8_t readStatus() {
  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(REG_STATUS);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)1);

  if (Wire.available() >= 1) {
    return Wire.read();
  }
  return 0;
}

void drawErrorScreen(const __FlashStringHelper* title, const __FlashStringHelper* msg) {
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.setCursor(10, 8);
  display.setTextSize(1);
  display.print(F("!! "));
  display.print(title);
  display.print(F(" !!"));

  display.drawLine(0, 20, 128, 20, SSD1306_WHITE);
  
  display.setCursor(8, 36);
  display.print(msg);
}

void executeCalibration() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("CALIBRACION"));
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setCursor(0, 20);
  display.print(F("1. Libere pedal"));
  display.setCursor(0, 32);
  display.print(F("Pulse boton..."));
  display.display();
  delay(1000);
  while(digitalRead(CALIB_PIN) == HIGH);
  rawMin = readRawAngle();
  delay(500);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(F("CALIBRACION"));
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  display.setCursor(0, 20);
  display.print(F("2. Acelerador a fondo"));
  display.setCursor(0, 32);
  display.print(F("Pulse boton..."));
  display.display();
  delay(1000);
  while(digitalRead(CALIB_PIN) == HIGH);
  rawMax = readRawAngle();

  display.clearDisplay();
  display.setCursor(0, 24);
  display.print(F("  CALIBRADO OK!"));
  display.display();
  delay(1000);
}