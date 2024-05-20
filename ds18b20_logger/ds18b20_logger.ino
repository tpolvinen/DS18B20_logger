#include <SPI.h>
#include "SdFat.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "RTClib.h"

// one row of data without remarks or errors is
// 35 bytes -> with 30 sec. measurement interval
// 24 hours produces 2880 lines, 2880 * 35 =
// 100800 bytes = 100.8 kilobytes.
// Header row is 48 bytes -> 100848 bytes.
// Row with remark "Kissaa rapsutettu!" is 53 bytes.
// With one remark every 5 minutes file size should
// allow for extra 288 * 5 bytes =  1440 bytes,
// file size -> 102288 bytes, 102.3 kilobytes

// Clock adjusted: rtc.adjust(DateTime(2024, 5, 20, 20, 22, 13));
// To check if clock has defaulted to 2000-01-01 00:00
// --> Unix time: 1716236533 seconds
constexpr unsigned long LAST_CLOCK_ADJUSTMENT_SEC = 1716236533; // last clock adjustment time in unixtime seconds
constexpr int8_t LEDPIN = 0;  // function indicator led connected to this pin
constexpr int32_t MAX_SD_FILESIZE = 102288;  // max. file size in bytes, will start a new file when reached
constexpr int8_t MEASUREMENT_INTERVAL_SEC = 30;  // time between measurements in seconds
constexpr int16_t CLOCK_CHECK_INTERVAL_MS = 1000;  // time between updating DateTime now in milliseconds
constexpr int8_t ONE_WIRE_BUS = 6;  // DS18B20 sensors connected to this pin
constexpr int8_t TEMPERATURE_PRECISION = 12;  // DS18B20 sensor precision

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// Assign address manually. The addresses below will need to be changed
// to valid device addresses on your bus. Device address can be retrieved
// by using either oneWire.search(deviceAddress) or individually via
// sensors.getAddress(deviceAddress, index)
DeviceAddress insideThermometer = { 0x28, 0x41, 0x42, 0xE3, 0x5D, 0x20, 0x01, 0x03 };
DeviceAddress outsideThermometer   = { 0x28, 0xA7, 0x22, 0x64, 0x62, 0x20, 0x01, 0x57 };

unsigned long currentMs;

RTC_DS3231 rtc;
DateTime now;
unsigned long startMeasurementIntervalSec = 0; // to mark the start of current measurementInterval in RTC unixtime
unsigned long startClockCheckIntervalMs = 0;
int8_t today;

const int buttonPin = 13;
int currentButtonState;
int lastButtonState = HIGH;   // the previous reading from the input pin
bool buttonHasBeenPressed = false;
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

const uint8_t chipSelect = 8;
SdFat sd;
SdFile file;
bool createNewFile = false;
char fileName[] = "00000000.CSV";
uint32_t fileSize;

// Error messages stored in flash.
#define error(msg) sd.errorHalt(F(msg))

void newFile() {
  now = rtc.now();
  fileName[0] = (now.year() / 10) % 10 + '0';
  fileName[1] = now.year() % 10 + '0';
  fileName[2] = now.month() / 10 + '0';
  fileName[3] = now.month() % 10 + '0';
  fileName[4] = now.day() / 10 + '0';
  fileName[5] = now.day() % 10 + '0';
  for (uint8_t i = 0; i < 100; i++) {
    fileName[6] = i / 10 + '0';
    fileName[7] = i % 10 + '0';
    if (! sd.exists(fileName)) {
      // only break from loop with a new fileName if it doesn't exist
      break;
    }
  }
  if (! file.open(fileName, O_WRONLY | O_CREAT | O_EXCL)) {
    error("file.open error");
  }
  createNewFile = false;
  Serial.print(F("Logging to: "));
  Serial.println(fileName);
  file.println(F("datetime,T inside,T outside,remark,error,error,error"));
  Serial.println("File header: datetime,T inside,T outside,remark,error,error,error");

  if (! file.sync() || file.getWriteError()) {
    error("file.sync() error");
  }
}

void setup() {
  // initialize the pushbutton pin as an pull-up input
  // the pull-up input pin will be HIGH when the switch is open and LOW when the switch is closed.
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(LEDPIN, OUTPUT);

  Serial.begin(9600);
  while (! Serial) {
    delay(10);
  }

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }
  now = rtc.now();
  today = now.day();
  // start measurementInterval and set it back by interval time -> starts measurements in loop() right away
  startMeasurementIntervalSec = now.unixtime() - MEASUREMENT_INTERVAL_SEC;
  startClockCheckIntervalMs = millis();

  // Start up the sensor library
  sensors.begin();

  // locate devices on the bus
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");
  // report parasite power requirements
  Serial.print("Parasite power is: ");
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");

  // show the device addresses
  Serial.print("Device 0 Address: ");
  //printAddress(insideThermometer);
  for (uint8_t i = 0; i < 8; i++) {
    // zero pad the address if necessary
    if (insideThermometer[i] < 16) Serial.print("0");
    Serial.print(insideThermometer[i], HEX);
  }
  Serial.println();
  Serial.print("Device 1 Address: ");
  //printAddress(outsideThermometer);
  for (uint8_t i = 0; i < 8; i++) {
    // zero pad the address if necessary
    if (outsideThermometer[i] < 16) Serial.print("0");
    Serial.print(outsideThermometer[i], HEX);
  }
  Serial.println();

  // set the resolution to defined bits per device
  sensors.setResolution(insideThermometer, TEMPERATURE_PRECISION);
  sensors.setResolution(outsideThermometer, TEMPERATURE_PRECISION);

  Serial.print("Device 0 Resolution: ");
  Serial.print(sensors.getResolution(insideThermometer), DEC);
  Serial.println();
  Serial.print("Device 1 Resolution: ");
  Serial.print(sensors.getResolution(outsideThermometer), DEC);
  Serial.println();

  delay(1000);
  if (! sd.begin(chipSelect, SD_SCK_MHZ(50))) {
    sd.initErrorHalt();
  }
  newFile();
}

void loop() {

  uint16_t thisYear;
  int8_t thisMonth, thisDay, thisHour, thisMinute, thisSecond;
  char dateAndTimeArr[20]; //19 digits plus the null char
  bool clockFaultState = false;

  int reading = digitalRead(buttonPin);
  currentMs = millis();

  if (reading != lastButtonState) {
    lastDebounceTime = currentMs;
    currentButtonState = reading;
  }

  if (currentMs - lastDebounceTime >= debounceDelay) {
    if (lastButtonState == LOW && currentButtonState == HIGH) {
      buttonHasBeenPressed = true;
      for (int8_t i = 0; i < 10; i++) {
        digitalWrite(LEDPIN, HIGH);
        delay(50);
        digitalWrite(LEDPIN, LOW);
        delay(30);
      }
    }
    lastButtonState = currentButtonState;
  }

  if (currentMs - startClockCheckIntervalMs >= CLOCK_CHECK_INTERVAL_MS) {
    startClockCheckIntervalMs = currentMs;
    now = rtc.now();
    digitalWrite(LEDPIN, HIGH);
    delay(75);
    digitalWrite(LEDPIN, LOW);
  }

  if (now.unixtime() - startMeasurementIntervalSec >= MEASUREMENT_INTERVAL_SEC) {
    digitalWrite(LEDPIN, HIGH);
    startMeasurementIntervalSec = now.unixtime();
    if (startMeasurementIntervalSec <  LAST_CLOCK_ADJUSTMENT_SEC) {
      clockFaultState = true;
    }
    thisYear = now.year();
    thisMonth = now.month();
    thisDay = now.day();
    thisHour = now.hour();
    thisMinute = now.minute();
    thisSecond = now.second();
    sprintf_P(dateAndTimeArr, PSTR("%4d-%02d-%02dT%d:%02d:%02d"),
              thisYear, thisMonth, thisDay, thisHour, thisMinute, thisSecond);
    if (today !=  thisDay) {
      createNewFile = true;
      today = thisDay;
    }
    if (file.fileSize() > MAX_SD_FILESIZE) {
      createNewFile = true;
    }
    if (createNewFile) {
      file.close();
      newFile();
    }

    // call sensors.requestTemperatures() to issue a global temperature
    // request to all devices on the bus
    sensors.requestTemperatures();
    float insideTempC = sensors.getTempC(insideThermometer);
    float outsideTempC = sensors.getTempC(outsideThermometer);
    bool insideSensorErrorState = false;
    bool outsideSensorErrorState = false;
    bool insideSensorFaultState = false;
    bool outsideSensorFaultState = false;

    if (insideTempC == DEVICE_DISCONNECTED_C ||
        insideTempC > 125 ||
        insideTempC < -55) {
      insideSensorErrorState = true;
    }
    if (insideTempC == 85.00) {
      insideSensorFaultState = true;
    }
    if (outsideTempC == DEVICE_DISCONNECTED_C ||
        outsideTempC > 125 ||
        outsideTempC < -55) {
      outsideSensorErrorState = true;
    }
    if (outsideTempC == 85.00) {
      outsideSensorFaultState = true;
    }

    file.print(dateAndTimeArr);
    file.write(',');
    file.print(insideTempC);
    file.write(',');
    file.print(outsideTempC);
    file.write(',');
    if (buttonHasBeenPressed) {
      file.print("Kissaa rapsutettu!");
      Serial.print(" Kissaa rapsutettu!");
      buttonHasBeenPressed = false;
    }
    file.write(',');
    if (insideSensorErrorState) {
      file.print("Vika:inside");
      Serial.print(" Vika:inside");
      insideSensorErrorState = false;
    }
    if (insideSensorFaultState) {
      file.print(" Tarkasta mittaus!");
      Serial.print(" Tarkasta mittaus!");
      insideSensorFaultState = false;
    }
    file.write(',');
    if (outsideSensorErrorState) {
      file.print("Vika:outside");
      Serial.print(" Vika:outside");
      outsideSensorErrorState = false;
    }
    if (outsideSensorFaultState) {
      file.print(" Tarkasta mittaus!");
      Serial.print(" Tarkasta mittaus!");
      outsideSensorFaultState = false;
    }
    file.write(',');
    if (clockFaultState) {
      file.print(" Tarkista kello!");
      Serial.print(" Tarkista kello!");
      clockFaultState = false;
    }
    file.println();
    if (! file.sync() || file.getWriteError()) {
      error("file.sync() error at loop()");
    }

    Serial.print(file.fileSize());
    Serial.print("  ");
    Serial.print(dateAndTimeArr);
    Serial.print(", ");
    Serial.print(insideTempC);
    Serial.print(", ");
    Serial.print(outsideTempC);
    Serial.println();
    digitalWrite(LEDPIN, LOW);
  }
}
