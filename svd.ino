//==============================================================
// Simple Vibration Detector
//==============================================================

#define VERSION 5
#define FLOAT_BYTE_SIZE 4

#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "M5Atom.h"

#include <ArduinoBLE.h>

// predefined display buffers to display some characters and numbers
#include "characters.c"

bool all = false;
bool data_only = true;

// Predefined colors RGB hex format
int GRB_COLOR_WHITE = 0xffffff;
int GRB_COLOR_BLACK = 0x000000;
int GRB_COLOR_RED = 0xff0000;
int GRB_COLOR_ORANGE = 0xFF8000;
int GRB_COLOR_YELLOW = 0xffff00;
int GRB_COLOR_GREEN = 0x00ff00;
int GRB_COLOR_BLUE = 0x0000ff;
int GRB_COLOR_PURPLE = 0x5a005a;

// SSID and password of your network
String ssid;
String pass;

int status = WL_IDLE_STATUS;

int iAttempt = 0;
int numberis = 0;
int printCount = 0;

// timer for no movement
unsigned long timer_no_movement = 0;
int five_minutes = 60000; // 300000;
int movements = 0;
bool sendStopMsg = true;

// MQTT Broker information
const char *server = "broker.mqttdashboard.com";
int port = 1883;

String topicIn = "hwdchiva/topic-in";
String topicOut = "hwdchiva/topic-out";
char button_msg[] = "button0";

int n_average = 15;
float accX = 0, accY = 0, accZ = 0;
float accX_avg = 0, accY_avg = 0, accZ_avg = 0;
float gyroX = 0, gyroY = 0, gyroZ = 0;
float gyroX_avg = 0, gyroY_avg = 0, gyroZ_avg = 0;
bool IMU_ready = false;

File m_file;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

#define LOCAL_NAME "M5Stack Atom - Vibration Kit"

//==============================================================
// MQTT callback when message recieved
//==============================================================
void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  fillDisplay(GRB_COLOR_BLUE);
}

//==============================================================
// Fill display with color fillColor
//==============================================================
void fillDisplay(int fillColor)
{
  for (int i = 0; i < 25; i++)
  {
    M5.dis.drawpix(i, fillColor);
  }
}

//==============================================================
// Function to display an animated line on the LED Matrix screen
//==============================================================
void display_line()
{
  for (int i = 0; i < 2; i++)
  {
    M5.dis.displaybuff((uint8_t *)image_line);
    delay(500);
    M5.dis.displaybuff((uint8_t *)image_left);
    delay(500);
    M5.dis.displaybuff((uint8_t *)image_line);
    delay(500);
    M5.dis.displaybuff((uint8_t *)image_right);
    delay(500);
  }
  M5.dis.displaybuff((uint8_t *)image_line);
  delay(500);
}

//==============================================================
// Function to display a number on the LED Matrix screen
// the characters are defined in the file "characters.c"
//==============================================================
void display_number(uint8_t number)
{
  // only single digit numbers between 0 and 9 are possible.
  if (number < 20)
  {
    M5.dis.displaybuff((uint8_t *)image_numbers[number]);
  }
  else
  {
    M5.dis.displaybuff((uint8_t *)image_dot);
  }
}

//==============================================================
//
//==============================================================
void printMacAddress()
{
  // the MAC address of your Wifi shield
  byte mac[6];

  // print your MAC address:
  WiFi.macAddress(mac);
  Serial.print("MAC: ");
  Serial.print(mac[5], HEX);
  Serial.print(":");
  Serial.print(mac[4], HEX);
  Serial.print(":");
  Serial.print(mac[3], HEX);
  Serial.print(":");
  Serial.print(mac[2], HEX);
  Serial.print(":");
  Serial.print(mac[1], HEX);
  Serial.print(":");
  Serial.println(mac[0], HEX);
}

//==============================================================
// Read wifi info
//==============================================================
void getWifiInfo()
{
  File fd = SD.open("/WiFi.txt", FILE_READ);
  if (fd)
  {
    ssid = fd.readStringUntil('\r\n');
    pass = fd.readStringUntil('\r\n');
    Serial.println("Found WiFi file: \"" + ssid + "\", \"" + pass + "\"");
  }
  else
  {
    Serial.println("Failed to open WiFi file!");
  }
  fd.close();
}

//==============================================================
// Handle wifi connection
//==============================================================
void connectToWifi()
{

  if (WiFi.status() == WL_CONNECTED)
    return;

  fillDisplay(GRB_COLOR_PURPLE);

  getWifiInfo();

  Serial.print("Attempting to connect to WPA network: ");
  Serial.println(ssid.c_str());

  // Attempt to connect to WPA network
  status = WiFi.begin(ssid, pass);
  delay(10000);

  status = WiFi.status();
  while (status != WL_CONNECTED)
  {
    Serial.println("Couldn't establish WiFi connection! Status: ");
    Serial.print(status);
    Serial.print("\tAttempt: ");
    Serial.println(iAttempt++);

    fillDisplay(GRB_COLOR_WHITE);
    delay(3000);
    fillDisplay(GRB_COLOR_PURPLE);
    delay(3000);
    status = WiFi.status();
  }

  Serial.println("WiFi connection established!!!");
  fillDisplay(GRB_COLOR_ORANGE);
  delay(3000);
}

//==============================================================
// Connect to MQTT broker
//==============================================================
void connectToBroker()
{

  if (mqttClient.connected())
    return;

  fillDisplay(GRB_COLOR_YELLOW);

  Serial.println("Attempting MQTT connection...");
  // Attempt to connect
  if (mqttClient.connect("hwdchiva-client"))
  {
    Serial.println("Connected to MQTT broker successfully...");

    // mqttClient.publish("hwdchiva-out", "hello world");
    // mqttClient.subscribe("hwdchiva-in");

    fillDisplay(GRB_COLOR_GREEN);
  }
  else
  {
    fillDisplay(GRB_COLOR_RED);
    Serial.println("Couldn't establish MQTT connection! Status: ");
    Serial.print(mqttClient.state());
    delay(5000);
  }
}

//==============================================================
// Average the accel data. Simple "running average".
//==============================================================
void processAccelData()
{
  accX_avg = ((accX_avg * (n_average - 1)) + accX) / n_average;
  accY_avg = ((accY_avg * (n_average - 1)) + accY) / n_average;
  accZ_avg = ((accZ_avg * (n_average - 1)) + accZ) / n_average;
  accX = accX_avg;
  accY = accY_avg;
  accZ = accZ_avg;
}

//==============================================================
// Average the gyro data. Simple "running average".
//==============================================================
void processGyroData()
{
  gyroX_avg = ((gyroX_avg * (n_average - 1)) + gyroX) / n_average;
  gyroY_avg = ((gyroY_avg * (n_average - 1)) + gyroY) / n_average;
  gyroZ_avg = ((gyroZ_avg * (n_average - 1)) + gyroZ) / n_average;
  gyroX = gyroX_avg;
  gyroY = gyroY_avg;
  gyroZ = gyroZ_avg;
}

//==============================================================
// Get IMU vibraton data
//==============================================================
void getVibration()
{
  // get the acceleration data
  // accX = right pointing vector
  //      tilt to the right: > 0
  //      tilt to the left:  < 0
  // accy = backward pointing vector
  //      tilt forward:  < 0
  //      tilt backward: > 0
  // accZ = upward pointing vector
  //      flat orientation: -1g

  gyroX = 0, gyroY = 0, gyroZ = 0;
  accX = 0, accY = 0, accZ = 0;

  unsigned long timestamp = millis();
  M5.IMU.getGyroData(&gyroX, &gyroY, &gyroZ);
  M5.IMU.getAccelData(&accX, &accY, &accZ);

  String data = String(timestamp) + "," + String(gyroX) + "," + String(gyroY) + "," + String(gyroZ);

  if (m_file)
  {
    m_file.println(data.c_str());
    m_file.flush();

    if (printCount++ % 100 == 0)
      Serial.print(".");
  }
  else if (all || printCount++ % 10 == 0)
  {
    Serial.println(data.c_str());
  }

  // processAccelData();
  processGyroData();

  if (abs(gyroX) < 1.0 && abs(gyroY) < 1.0 && abs(gyroZ) < 1.0)
  {
    if (millis() > timer_no_movement)
    {
      display_number(2);
      if (sendStopMsg)
      {
        mqttClient.publish(topicOut.c_str(), "stopped!");
        sendStopMsg = false;
      }
    }
    else
      display_number(0);
  }
  else
  {
    timer_no_movement = millis() + five_minutes;
    sendStopMsg = true;
    display_number(1);
    M5.dis.drawpix(movements++, GRB_COLOR_GREEN);
    if (movements > 24)
      movements = 0;
  }
}

//==============================================================
// Mount SD card to read/write
//==============================================================
void setupSDCard()
{
  SPI.begin(23, 33, 19, -1);

  if (!SD.begin(4))
  {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC)
  {
    Serial.println("MMC");
  }
  else if (cardType == CARD_SD)
  {
    Serial.println("SDSC");
  }
  else if (cardType == CARD_SDHC)
  {
    Serial.println("SDHC");
  }
  else
  {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  char fileName[] = "/Washer00.csv"; // Base filename for logging.

  // Construct the filename to be the next incrementally indexed filename in the set [00-99].
  for (byte i = 1; i <= 99; i++)
  {
    // check before modifying target filename.
    if (SD.exists(fileName))
    {
      // the filename exists so increment the 2 digit filename index.
      fileName[7] = i / 10 + '0';
      fileName[8] = i % 10 + '0';
    }
    else
    {
      break; // the filename doesn't exist so break out of the for loop.
    }
  }

  Serial.printf("Writing to file: %s\n", fileName);

  m_file = SD.open(fileName, FILE_WRITE);
  if (!m_file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  m_file.println("timestamp, gyroX, gyroY, gyroZ");
}

//==============================================================
// Setup/Main
//==============================================================
void setup()
{
  M5.begin(true, false, true);
  delay(50);

  //------------------------------------------
  // Setup serial monitor
  // Serial.begin(9600);
  Serial.begin(115200);
  Serial.flush();
  // Wait 3 seconds
  delay(3000);

  // Show startup animation
  display_line();

  // Welcome message
  Serial.println("");
  Serial.println("hwdchiva vibration detector");
  Serial.println("v1.0 | 05.14.2024");

  //------------------------------------------
  // Setup SD Card
  setupSDCard();

  //------------------------------------------
  // Initialize IMU
  Serial.println("Initializing IMU");
  // check if IMU is ready
  if (M5.IMU.Init() == 0)
  {
    IMU_ready = true;
    Serial.println("IMU ready");
    M5.dis.displaybuff((uint8_t *)image_CORRECT);
    timer_no_movement = millis() + five_minutes;
  }
  else
  {
    IMU_ready = false;
    Serial.println("IMU failed!");
    M5.dis.displaybuff((uint8_t *)image_WRONG);
  }

  delay(3000);

  //------------------------------------------
  // attempt to connect to Wifi network
  getWifiInfo();
  if (!data_only)
  {
    connectToWifi();

    mqttClient.setServer(server, port);
    mqttClient.setCallback(callback);
  }

  // Start the core BLE engine.
  if (!BLE.begin())
  {
    Serial.println("Failed to initialized BLE!");
    // setState(ERROR_STATE);
    // while (1) showErrorLed();
  }

  String address = BLE.address();

  // Output BLE settings over Serial.
  Serial.print("address = ");
  Serial.println(address);

  address.toUpperCase();

  static String deviceName = LOCAL_NAME;
  deviceName += " - ";
  deviceName += address[address.length() - 5];
  deviceName += address[address.length() - 4];
  deviceName += address[address.length() - 2];
  deviceName += address[address.length() - 1];

  Serial.print("deviceName = ");
  Serial.println(deviceName);

  Serial.print("localName = ");
  Serial.println(deviceName);

  // Set up properties for the whole service.
  BLE.setLocalName(deviceName.c_str());
  BLE.setDeviceName(deviceName.c_str());

  // Print out full UUID and MAC address.
  Serial.println("Peripheral advertising info: ");
  Serial.print("Name: ");
  Serial.println(LOCAL_NAME);
  Serial.print("MAC: ");
  Serial.println(BLE.address());

  // Allow the hardware to sort itself out
  delay(1500);
}

//==============================================================
// Main processing loop
//==============================================================
void loop()
{
  if (!data_only)
  {
    while (!mqttClient.connected())
    {
      connectToWifi();
      connectToBroker();
      delay(3000);
    }

    mqttClient.loop();
  }

  M5.update();             // M5.update() is required to read the state of the button, see System for details.
  if (M5.Btn.wasPressed()) // If the button is pressed
  {
    if (m_file)
    {
      m_file.close();
      SD.end();
      Serial.println("\nFile Closed.");
    }
    // button_msg[6] = (char)numberis++;
    // mqttClient.publish(topicOut.c_str(), button_msg);
    do
    {
      M5.update();
    } while (M5.Btn.isPressed());
  }

  getVibration();

  delay(100);
}
