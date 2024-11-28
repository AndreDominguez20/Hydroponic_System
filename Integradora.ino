#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET 13
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pines para actuadores
const int tdsPump =4;
const int generalPump=5;
const int heatingPlate=6;

// Botón de emergencia
const int buttonPin = 2;
volatile bool emergencyStop = false;
volatile unsigned long lastInterruptTime = 0;

// Sensores
const int TdsSensorPin = A1;
const int TempSensorPin = 8;
const int PhSensorPin = A2;

// Variables de sensores
const int SAMPLE_COUNT = 10;
int tdsAnalogBuffer[SAMPLE_COUNT];
int tdsAnalogIndex = 0;
float tdsValue = 0;
int phSamples[10];

// Configuración del sensor de temperatura
OneWire oneWire(TempSensorPin);
DallasTemperature tempSensor(&oneWire);

float VREF() {
  return (analogRead(A3) * 5.0) / 1023;
}

void toggleEmergencyStop() {
  unsigned long interruptTime = millis();
  // Evitar ruido (rebotes) del botón
  if (interruptTime - lastInterruptTime > 200) { // Tiempo mínimo entre pulsos (200 ms)
    emergencyStop = !emergencyStop;             // Alternar estado
    lastInterruptTime = interruptTime;
  }
}

// Función para calcular el valor mediano
int getMedianValue(int buffer[], int count) {
  int sortedBuffer[count];
  memcpy(sortedBuffer, buffer, count * sizeof(int));
  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (sortedBuffer[i] > sortedBuffer[j]) {
        int temp = sortedBuffer[i];
        sortedBuffer[i] = sortedBuffer[j];
        sortedBuffer[j] = temp;
      }
    }
  }
  return count % 2 == 0 ? (sortedBuffer[count / 2 - 1] + sortedBuffer[count / 2]) / 2
                        : sortedBuffer[count / 2];
}

float getTemperature() {
  tempSensor.requestTemperatures();
  return tempSensor.getTempCByIndex(0);
}

float getTDSValue() {
  int analogValue = analogRead(TdsSensorPin);
  tdsAnalogBuffer[tdsAnalogIndex++] = analogValue;
  if (tdsAnalogIndex == SAMPLE_COUNT) tdsAnalogIndex = 0;

  float averageVoltage = getMedianValue(tdsAnalogBuffer, SAMPLE_COUNT) * VREF() / 1024.0;
  float temperature = getTemperature();
  float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
  float compensatedVoltage = averageVoltage / compensationCoefficient;
  return (133.42 * pow(compensatedVoltage, 3) - 255.86 * pow(compensatedVoltage, 2) + 857.39 * compensatedVoltage) * 0.46;
}

float getPHValue() {
  for (int i = 0; i < 10; i++) {
    phSamples[i] = analogRead(PhSensorPin);
    delay(100);
  }
  int avgValue = 0;
  for (int i = 2; i < 8; i++) avgValue += phSamples[i];
  float phVoltage = (float)avgValue * VREF() / 1024 / 6;
  return 2.80 * phVoltage;
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Configuración de pines
  pinMode(tdsPump, OUTPUT);
  pinMode(generalPump, OUTPUT);
  pinMode(heatingPlate, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP); 

  attachInterrupt(digitalPinToInterrupt(buttonPin), toggleEmergencyStop, FALLING);

  tempSensor.begin();

  // Configuración del display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Failed to initialize display");
    while (1);
  }
  display.clearDisplay();
  display.setCursor(25, 15);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("Initializing");
  delay(100);
  display.display();
}

void loop() {
  if (emergencyStop) {
    // Detener actuadores y mostrar mensaje de emergencia
    digitalWrite(tdsPump, LOW);
    digitalWrite(generalPump, HIGH);
    digitalWrite(heatingPlate, LOW);

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 25);
    display.println("EMERGENCY STOP");
    display.display();
    delay(100);
    return; 
  }

  // Lógica del sistema
  digitalWrite(generalPump, LOW);

  tdsValue = getTDSValue();
  Serial.print("TDS Value: ");
  Serial.print(tdsValue);
  Serial.println(" ppm");

  float phValue = getPHValue();
  Serial.print("pH: ");
  Serial.println(phValue);

  float temperature = getTemperature();
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("TDS:");
  display.print(tdsValue);
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print("pH: ");
  display.print(phValue);
  display.setCursor(0, 40);
  display.print("Temp:");
  display.print(temperature);
  display.display();
  delay(100);

  if (tdsValue <= 400) {
    digitalWrite(tdsPump, HIGH);
  } else if (tdsValue>=580) {
    digitalWrite(tdsPump, LOW);
  }

  if (temperature <= 20) {
    digitalWrite(heatingPlate, HIGH);
  } else if (temperature >= 25) {
    digitalWrite(heatingPlate, LOW);
  }

  delay(1000);
}
