#include <OneWire.h>
#include <DallasTemperature.h>

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

void setup() {
  Serial.begin(9600);

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

  printTemperature(insideThermometer);
  printTemperature(outsideThermometer);
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

// function to print the temperature for a device
void printTemperature(DeviceAddress deviceAddress) {
  float tempC = sensors.getTempC(deviceAddress);
  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: Could not read temperature data");
    return;
  }
  Serial.print("Temp C: ");
  Serial.print(tempC);
}

// function to print a device's resolution
void printResolution(DeviceAddress deviceAddress) {
  Serial.print("Resolution: ");
  Serial.print(sensors.getResolution(deviceAddress));
  Serial.println();
}

// main function to print information about a device
void printData(DeviceAddress deviceAddress) {
  Serial.print("Device Address: ");
  printAddress(deviceAddress);
  Serial.print(" ");
  printTemperature(deviceAddress);
  Serial.println();
}

void loop() {
  String dataRemark;
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  sensors.requestTemperatures();
  float insideTempC = sensors.getTempC(insideThermometer);
  float outsideTempC = sensors.getTempC(outsideThermometer);
  if (insideTempC == DEVICE_DISCONNECTED_C) {
    dataRemark.concat(" Error: Failed to read from insideThermometer! ");
  }
  if (outsideTempC == DEVICE_DISCONNECTED_C) {
    dataRemark.concat(" Error: Failed to read from outsideThermometer! ");
  }

  Serial.print("insideTempC: ");
  Serial.print(insideTempC);
  Serial.print(", ");
  Serial.print("outsideTempC: ");
  Serial.print(outsideTempC);
  Serial.print(", ");
  Serial.print("dataRemark: ");
  Serial.print(dataRemark);
  Serial.println();

}
