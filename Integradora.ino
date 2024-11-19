#include <Adafruit_CCS811.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//Define Displays
#define TCA9548A_ADDRESS 0x70 //  address for TCA9548A
#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET 13       // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display1(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); //TDS Display
Adafruit_SSD1306 display2(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); //PH Display
Adafruit_SSD1306 display3(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); //Temperature Display
Adafruit_SSD1306 display4(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);  //CO2 Display

//Actuators
int tdsPump=7;
int generalPump=6;
int heatingPlate=8;

//CO2
Adafruit_CCS811 ccs;

// TDS Sensor
const int TdsSensorPin = A1;        // Analog reference voltage
const int SAMPLE_COUNT = 10;      // Number of samples for median filtering
// TDS Sensor Variables
int tdsAnalogBuffer[SAMPLE_COUNT];
int tdsAnalogIndex = 0;
float tdsValue = 0;
float VREF() {
   return (analogRead(A3) * 5.0) / 1023;
}

// Temperature Sensor
const int TempSensorPin = 4;
OneWire oneWire(TempSensorPin); 
DallasTemperature tempSensor(&oneWire); 

// pH Sensor
const int PhSensorPin = A2;
int phSamples[10];               // Array to store pH sensor readings

// Median Filter Function. this algorithm gets the mean of the TDS samples, for more accuracy 
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

// Temperature Reading
float getTemperature() {
  tempSensor.requestTemperatures();
  return tempSensor.getTempCByIndex(0);
}

// TDS Reading
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

// pH Reading
float getPHValue() {
  for (int i = 0; i < 10; i++) {
    phSamples[i] = analogRead(PhSensorPin);
    delay(100);
  }
  int avgValue = 0;
  for (int i = 2; i < 8; i++) avgValue += phSamples[i];
  float phVoltage = (float)avgValue * VREF() / 1024 / 6;
  return 2.80 * phVoltage;  // Convert millivolts to pH
}

// Function to select the I2C channel on the TCA9548A Multiplexor
void selectI2CChannel(uint8_t channel) {
  if (channel > 7) return; // Ensure channel is within valid range (0-7)
  Wire.beginTransmission(TCA9548A_ADDRESS);
  Wire.write(1 << channel); // Write to the TCA9548A to enable the channel
  Wire.endTransmission();
}

// Setup Function
void setup() {
  Serial.begin(115200);
  Wire.begin();
  pinMode(13, INPUT);
  pinMode(TdsSensorPin, INPUT);
  pinMode(tdsPump,OUTPUT);
  pinMode(generalPump,OUTPUT);
  pinMode(heatingPlate,OUTPUT);
    
  selectI2CChannel(4);
  if (!ccs.begin()) {
    Serial.println("Error al inicializar el sensor CCS811");
    while (1);
  }


  tempSensor.begin();
  //initialize displays the I2C addr (128x64)
  selectI2CChannel(0); // Channel 0 for Display 1 (TDS)
  if (!display1.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Failed to initialize display 1 (TDS)");
    while (1);
  }
  
  selectI2CChannel(1); // Channel 1 for Display 2 (pH)
  if (!display2.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Failed to initialize display 2 (pH)");
    while (1);
  }
  
  selectI2CChannel(2); // Channel 2 for Display 3 (Temperature)
  if (!display3.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Failed to initialize display 3 (Temperature)");
    while (1);
  }
  
  selectI2CChannel(3); // Channel 3 for Display 4 (CO2)
  if (!display4.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Failed to initialize display 4 (CO2)");
    while (1);
  }
  delay(500);
  //Display 1
  selectI2CChannel(0);
  display1.clearDisplay();
  display1.setCursor(25, 15);
  display1.setTextSize(1);
  display1.setTextColor(WHITE);
  display1.println("TDS Sensor");
  display1.setCursor(25, 35);
  display1.setTextSize(1);
  display1.print("Initializing");
  display1.display();
  //display 2
  selectI2CChannel(1);
  display2.clearDisplay();
  display2.setCursor(25, 15);
  display2.setTextSize(1);
  display2.setTextColor(WHITE);
  display2.println("PH Sensor");
  display2.setCursor(25, 35);
  display2.setTextSize(1);
  display2.print("Initializing");
  display2.display();
  //Display 3
  selectI2CChannel(2);
  display3.clearDisplay();
  display3.setCursor(5, 15);
  display3.setTextSize(1);
  display3.setTextColor(WHITE);
  display3.println("Temperature Sensor");
  display3.setCursor(25, 35);
  display3.setTextSize(1);
  display3.print("Initializing");
  display3.display();
  //display 4
  selectI2CChannel(3);
  display4.clearDisplay();
  display4.setCursor(25, 15);
  display4.setTextSize(1);
  display4.setTextColor(WHITE);
  display4.println("CO2 Sensor");
  display4.setCursor(25, 35);
  display4.setTextSize(1);
  display4.print("Initializing");
  display4.display();
  Serial.println("CCS811 test");
  if (!ccs.begin()) 
  {
    Serial.println("Failed to start sensor! Please check your wiring.");
    while (1);
  }
  // Wait for the sensor to be ready
  while (!ccs.available());
}
// Main Loop
void loop() {
	digitalWrite(generalPump, HIGH);
  //Serial.println(VREF());
  // TDS
  tdsValue = getTDSValue();
  Serial.print("TDS Value: ");
  Serial.print(tdsValue);
  Serial.println(" ppm");
  //Display TDS value
  selectI2CChannel(0);
  display1.clearDisplay();
  display1.setTextSize(1);
  display1.setCursor(20, 0);
  display1.print("TDS");
      
  display1.setTextSize(2);
  display1.setCursor(0, 20);
  display1.print("Value:");
  display1.print(tdsValue);
  display1.setTextSize(1);
  display1.print(" ppm");

  if (tdsValue<0 || tdsValue>1000){
  display1.clearDisplay();
  display1.setTextSize(3);
  display1.setCursor(0, 5);
  display1.print("ERROR");
  }
  
  if (tdsValue<=800){
    digitalWrite(tdsPump,HIGH);
    delay(10000);
    digitalWrite(tdsPump,LOW);
  } 

  // pH
  float phValue = getPHValue();
  Serial.print("pH: ");
  Serial.println(phValue);
  //Display Ph value
  selectI2CChannel(1);
  display2.clearDisplay();
  display2.setTextSize(1);
  display2.setCursor(20, 0);
  display2.print("PH");
      
  display2.setTextSize(2);
  display2.setCursor(0, 20);
  display2.print("Ph:");
  display2.print(phValue);

  if (phValue<0 || phValue>14){
  display2.clearDisplay();
  display2.setTextSize(3);
  display2.setCursor(0, 5);
  display2.print("ERROR");
  }

  // Temperature
  float temperature = getTemperature();
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");
  //Display Temperature Value
  selectI2CChannel(2);
  display3.clearDisplay();
  display3.setTextSize(1);
  display3.setCursor(20, 0);
  display3.print("Temperature");
      
  display3.setTextSize(2);
  display3.setCursor(0, 20);
  display3.print("Temp:");
  display3.print(temperature);
  display3.print(" °C");
  //display error
  if (temperature==-127){
  display3.clearDisplay();
  display3.setTextSize(3);
  display3.setCursor(0, 5);
  display3.print("ERROR");
  }
  //Heating plate control
  if (temperature<=20){
    digitalWrite(heatingPlate,HIGH);
  } else if (temperature>=25){
    digitalWrite(heatingPlate,LOW);
  }

  //CO2
  selectI2CChannel(4);
  if (ccs.available()) 
  {
    if (!ccs.readData()) 
    {
      Serial.print("CO2: ");
      Serial.print(ccs.geteCO2());
      Serial.print("ppm, TVOC: ");
      Serial.println(ccs.getTVOC());
      selectI2CChannel(3);
      display4.clearDisplay();
      display4.setTextSize(1);
      display4.setCursor(20, 0);
      display4.print("Air Quality");
      
      display4.setTextSize(2);
      display4.setCursor(0, 20);
      display4.print("CO2:");
      display4.print(ccs.geteCO2());
      display4.setTextSize(1);
      display4.print(" ppm");
 
      display4.setTextSize(2);
      display4.setCursor(0, 45);
      display4.print("TVOC:");
      display4.print(ccs.getTVOC());
      display4.display();
    }
    else 
    {
      Serial.println("ERROR!");
      display4.clearDisplay();
      display4.setTextSize( 2);
      display4.setCursor(0, 5);
      display4.print("ERROR!");
      while (1);
    }
  }
	delay(1000);
		
}
