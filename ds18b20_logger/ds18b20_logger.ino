#include <SPI.h>
#include <SD.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "RTClib.h"

// Data wire is plugged into port 6 on the Arduino
#define ONE_WIRE_BUS 6
#define TEMPERATURE_PRECISION 12

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

unsigned long currentMillis;

RTC_DS3231 rtc;
DateTime now;
unsigned long startMeasurementIntervalSec = 0; // to mark the start of current measurementInterval in RTC unixtime
const unsigned long measurementIntervalSec = 5; // seconds, measured in RTC unixtime
unsigned long startClockCheckIntervalMs = 0;
const unsigned long clockCheckIntervalMs = 1000;

const int chipSelect = 10; // for SD card SPI
File datafile;

const int buttonPin = 4;
int currentButtonState;
int lastButtonState = HIGH;   // the previous reading from the input pin
bool writeButtonPress = false;
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

void setup() {

  // initialize the pushbutton pin as an pull-up input
  // the pull-up input pin will be HIGH when the switch is open and LOW when the switch is closed.
  pinMode(buttonPin, INPUT_PULLUP);

  Serial.begin(9600);
  while (!Serial) {
    delay(10);
  }

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }
  now = rtc.now();
  // start measurementInterval and set it back by interval time -> starts measurements in loop() right away
  startMeasurementIntervalSec = now.unixtime() - measurementIntervalSec;
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
  printAddress(insideThermometer);
  Serial.println();

  Serial.print("Device 1 Address: ");
  printAddress(outsideThermometer);
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
  
  Serial.print("Initializing SD card...");
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
  }
  Serial.println("Done.");
  Serial.print("Creating a new file...");
  char filename[] = "00000000.CSV";
  filename[0] = (now.year() / 10) % 10 + '0';
  filename[1] = now.year() % 10 + '0';
  filename[2] = now.month() / 10 + '0';
  filename[3] = now.month() % 10 + '0';
  filename[4] = now.day() / 10 + '0';
  filename[5] = now.day() % 10 + '0';
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = i / 10 + '0';
    filename[7] = i % 10 + '0';
    if (! SD.exists(filename)) {
      // only open a new file if it doesn't exist
      datafile = SD.open(filename, FILE_WRITE);
      break;  // leave the loop!
    }
  }
  if (! datafile) {
    Serial.println("File create failed!");
  }
  Serial.println("Done.");
  Serial.print("Logging to: ");
  Serial.println(filename);

  datafile.println("datetime,T inside,T outside,remark");
  Serial.print("File header: ");
  Serial.println("datetime,T inside,T outside,remark");
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

void loop() {

  uint16_t thisYear;
  int8_t thisMonth, thisDay, thisHour, thisMinute, thisSecond;
  char dateAndTimeArr[20]; //19 digits plus the null char
  char dataRemarkArr[50];

  int reading = digitalRead(buttonPin);
  currentMillis = millis();

  if (reading != lastButtonState) {
    lastDebounceTime = currentMillis;
    currentButtonState = reading;
  }

  if (currentMillis - lastDebounceTime >= debounceDelay) {
    if (lastButtonState == LOW && currentButtonState == HIGH) {
      writeButtonPress = true;
    }
    lastButtonState = currentButtonState;
  }

  if (currentMillis - startClockCheckIntervalMs >= clockCheckIntervalMs) {
    startClockCheckIntervalMs = currentMillis;
    now = rtc.now();
  }

  if (now.unixtime() - startMeasurementIntervalSec >= measurementIntervalSec) {

    startMeasurementIntervalSec = now.unixtime();
    thisYear = now.year();
    thisMonth = now.month();
    thisDay = now.day();
    thisHour = now.hour();
    thisMinute = now.minute();
    thisSecond = now.second();
    sprintf_P(dateAndTimeArr, PSTR("%4d-%02d-%02dT%d:%02d:%02d"),
              thisYear, thisMonth, thisDay, thisHour, thisMinute, thisSecond);
    datafile.print(dateAndTimeArr);
    datafile.print(",");

    strcpy(dataRemarkArr, " ");
    if (writeButtonPress) {
      strcat(dataRemarkArr, "Kissaa rapsutettu!");
      writeButtonPress = false;
    }

    // call sensors.requestTemperatures() to issue a global temperature
    // request to all devices on the bus
    sensors.requestTemperatures();
    float insideTempC = sensors.getTempC(insideThermometer);
    float outsideTempC = sensors.getTempC(outsideThermometer);
    if (insideTempC == DEVICE_DISCONNECTED_C || insideTempC > 125 || insideTempC < -55) {
      strcat(dataRemarkArr, " Vika:inside");
    }
    if (outsideTempC == DEVICE_DISCONNECTED_C || outsideTempC > 125 || outsideTempC < -55) {
      strcat(dataRemarkArr, " Vika:outside");
    }

    datafile.print(insideTempC);
    datafile.print(",");
    datafile.print(outsideTempC);
    datafile.print(",");
    datafile.println(dataRemarkArr);

    Serial.print(dateAndTimeArr);
    Serial.print(", ");
    Serial.print("insideTempC: ");
    Serial.print(insideTempC);
    Serial.print(", ");
    Serial.print("outsideTempC: ");
    Serial.print(outsideTempC);
    Serial.print(", ");
    Serial.print("dataRemarkArr: ");
    Serial.print(dataRemarkArr);
    Serial.println();

    datafile.flush();
  }
}
