#include <TFT.h>
#include <SPI.h>
#include <math.h>

// ---- function declarations (fix compile error) ----
float calibrateCurrentOffset();
float measureCurrent();
String prevStatus = "";

#define cs 10
#define dc 9
#define rst 8

#define VOLTAGE_PIN A7
#define CURRENT_PIN A5
#define RELAY_PIN 4
#define RESET_BTN 6
#define FAULT_PIN 7

// -------- Protection Limits --------
#define OVER_CURRENT 4.0
#define UNDER_VOLTAGE 10.0
#define OVER_VOLTAGE 85.0

TFT TFTscreen = TFT(cs, dc, rst);

// -------- Voltage ----------
float voltage;
float prevVoltage = -1;
float v_calibration = 555.0;

// -------- Current ----------
const float currentCalibration = 0.01700;
const int sampleCount = 1000;
float currentOffset;
float current;
float prevCurrent = -1;

// -------- Status ----------
bool faultActive = false;
bool systemTripped = false;
String faultMessage = "NORMAL";


// ================= SETUP =================
void setup()
{

  pinMode(FAULT_PIN, OUTPUT);
  digitalWrite(FAULT_PIN, LOW);   // no fault initially

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // relay ON (active LOW)

  pinMode(RESET_BTN, INPUT_PULLUP);

  Serial.begin(115200);

  TFTscreen.begin();
  TFTscreen.setRotation(0);
  TFTscreen.background(0, 0, 0);
  TFTscreen.setTextSize(2);

  TFTscreen.stroke(0,255,0);
  TFTscreen.text("V:", 5, 10);

  TFTscreen.stroke(0,255,255);
  TFTscreen.text("I:", 5, 40);

  currentOffset = calibrateCurrentOffset();
}


// ================= LOOP =================
void loop()
{
  // ===== RESET BUTTON =====
  if(digitalRead(RESET_BTN)==LOW)
  {
    delay(60);
    if(digitalRead(RESET_BTN)==LOW)
    {
      systemTripped = false;
      faultMessage = "RESET OK";
      digitalWrite(RELAY_PIN, LOW);
    }
  }

  // -------- VOLTAGE RMS --------
  long sum = 0;
  int samples = 500;
  long offset = 0;

  for(int i=0;i<200;i++)
  {
    offset += analogRead(VOLTAGE_PIN);
    delayMicroseconds(200);
  }

  offset = offset/200;

  for (int i = 0; i < samples; i++)
  {
    int val = analogRead(VOLTAGE_PIN) - offset;
    sum += (long)val * val;
    delayMicroseconds(200);
  }

  float mean = sum / (float)samples;
  float rms = sqrt(mean);

  float sensorVoltage = rms * (5.0 / 1023.0);
  float rawVoltage = sensorVoltage * v_calibration;

  static float smoothVoltage = 0;
  smoothVoltage = (smoothVoltage * 0.4) + (rawVoltage * 0.4);

  voltage = smoothVoltage;

  // -------- CURRENT RMS --------
  current = measureCurrent();

  Serial.print("V:");
  Serial.print(voltage);
  Serial.print(" I:");
  Serial.println(current,3);

  // ===== PROTECTION =====
  faultActive = false;

  if(!systemTripped)
    faultMessage = "NORMAL";

  if (current > OVER_CURRENT)
  {
    faultActive = true;
    faultMessage = "OVER CURRENT";
  }

  if (voltage < UNDER_VOLTAGE)
  {
    faultActive = true;
    faultMessage = "UNDER VOLT";
  }

  if (voltage > OVER_VOLTAGE)
  {
    faultActive = true;
    faultMessage = "OVER VOLT";
  }

  if(faultActive)
  {
    systemTripped = true;
  }

  // relay control
  if(systemTripped)
{
    digitalWrite(RELAY_PIN, HIGH); // relay OFF
    digitalWrite(FAULT_PIN, HIGH); // FAULT SIGNAL
}
  else
{
    digitalWrite(RELAY_PIN, LOW);  // relay ON
    digitalWrite(FAULT_PIN, LOW);  // NORMAL
}

  // ===== TFT VOLTAGE =====
  if (abs(voltage - prevVoltage) > 0.5)
  {
    TFTscreen.stroke(0,0,0);

    char oldV[10];
    dtostrf(prevVoltage, 4, 1, oldV);
    TFTscreen.text(oldV, 30, 10);

    TFTscreen.stroke(255,255,0);

    char newV[10];
    dtostrf(voltage, 4, 1, newV);
    TFTscreen.text(newV, 30, 10);
    TFTscreen.text("V", 100, 10);

    prevVoltage = voltage;
  }

  // ===== TFT CURRENT =====
  if (abs(current - prevCurrent) > 0.02)
  {
    TFTscreen.stroke(0,0,0);

    char oldC[10];
    dtostrf(prevCurrent, 4, 2, oldC);
    TFTscreen.text(oldC, 40, 40);

    TFTscreen.stroke(0,255,255);

    char newC[10];
    dtostrf(current, 4, 2, newC);
    TFTscreen.text(newC, 40, 40);
    TFTscreen.text("A", 100, 40);

    prevCurrent = current;
  }

  // ===== TFT STATUS =====
String currentStatus;

if(systemTripped)
  currentStatus = faultMessage;
else
  currentStatus = "NORMAL";

if(currentStatus != prevStatus)
{
  // clear status area
  TFTscreen.fillRect(0, 70, 160, 20, 0);

  if(systemTripped)
    TFTscreen.stroke(255,0,0);
  else
    TFTscreen.stroke(0,255,0);

  TFTscreen.text(currentStatus.c_str(),5,70);

  prevStatus = currentStatus;
}

  delay(200);
}


// ================= CURRENT FUNCTIONS =================
float calibrateCurrentOffset()
{
  long total = 0;

  for (int i = 0; i < 500; i++)
  {
    total += analogRead(CURRENT_PIN);
  }

  return total / (float)500;
}


float measureCurrent()
{
  long total = 0;

  currentOffset = calibrateCurrentOffset();

  for (int i = 0; i < sampleCount; i++)
  {
    float v = analogRead(CURRENT_PIN) - currentOffset;
    total += v * v;
  }

  float rms = sqrt(total / (float)sampleCount);
  float amp = rms * currentCalibration;

  if (amp < 0.01)
    amp = 0;

  return amp;
}