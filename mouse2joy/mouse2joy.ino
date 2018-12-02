/***************************************************************************
MIT License

Copyright (c) 2018 gdsports625@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
****************************************************************************/

// Mouse to Joystick for Xbox Adaptive Controller

#include "USBHost_t36.h"
#include <elapsedMillis.h>

elapsedMillis BetweenMouseMoves;

USBHost myusb;
USBHub hub1(myusb);
USBHIDParser hid1(myusb);
MouseController mouse1(myusb);

// For configuration file on micro SD card
#include <SD.h>
#include <SPI.h>

// For Teensy 3.6 built-in card slot
const int chipSelect = BUILTIN_SDCARD;

// Configuration is stored in JSON format.
#include <ArduinoJson.h>

// Serial does not exist when using joystick only (no serial) mode so send
// debug to Serial1. Use joystick only for maximum compatibility with older
// systems. Joystick + Serial works with Microsoft Xbox Adaptive Controller.
// Change the 0 to 1 to enable debug output.
#ifdef USB_JOYSTICK
#define SerialDebug if(0)Serial1
#else
#define SerialDebug if(0)Serial
#endif
// configure the joystick to manual send mode.  This gives precise
// control over when the computer receives updates, but it does
// require you to manually call Joystick.send_now().
#define BEGIN()     useManualSend(true)
#define SETXAXIS(x) X(x)
#define SETYAXIS(x) Y(x)
#define SETBUTTON(button_num, value)    button(((button_num)+1), (value))
#define SENDSTATE() send_now()

USBDriver *drivers[] = {&hub1, &hid1};
#define CNT_DEVICES (sizeof(drivers)/sizeof(drivers[0]))
const char * driver_names[CNT_DEVICES] = {"Hub1", "HID1"};
bool driver_active[CNT_DEVICES] = {false, false};

// Lets also look at HID Input devices
USBHIDInput *hiddrivers[] = {&mouse1};
#define CNT_HIDDEVICES (sizeof(hiddrivers)/sizeof(hiddrivers[0]))
const char * hid_driver_names[CNT_DEVICES] = {"Mouse1"};
bool hid_driver_active[CNT_DEVICES] = {false};
bool show_changed_only = false;

// Default button mapping. JSON file can override.
// 0 = Xbox controller View button
// 1 = Xbox controller Menu button
// 2 = Xbox controller right stick click button
// 3 = Xbox controller right bumper button
// 4 = Xbox controller X button
// 5 = Xbox controller Y button
// 6 = Xbox controller X1 button
// 7 = Xbox controller X2 button
uint8_t mapMouseButtonToXACRightButton[8] = {
  4,  // X
  5,  // Y
  3,  // Right Bumper
  2,  // Right Stick button
  6,  // X1
  7,  // X2
  0,  // View
  1,  // Menu
};

// Load configuration file from micro SD card
void load_config()
{
  File configFile;
  char json[512];
  int jsonLen;

  for (int i = 0; i < 8; i++) {
    SerialDebug.printf("mapMouseButtonToXACRightButton[%d] = %d\n",
        i, mapMouseButtonToXACRightButton[i]);
  }
  SerialDebug.print("Initializing SD card...");

  if (!SD.begin(chipSelect)) {
    SerialDebug.println("initialization failed!");
    return;
  }
  SerialDebug.println("initialization done.");

  configFile = SD.open("MSE2JOY.JSN");
  jsonLen = -1;
  // Read JSON
  if (configFile) {
    jsonLen = configFile.read(json, sizeof(json)-1);
    configFile.close();
    SerialDebug.println("done.");
  } else {
    SerialDebug.println("error opening MSE2JOY.JSN");
  }
  if (jsonLen > 0) {
    const size_t bufferSize = JSON_ARRAY_SIZE(8) + JSON_OBJECT_SIZE(1) + 40;
    DynamicJsonBuffer jsonBuffer(bufferSize);

    json[jsonLen] = '\0';

    JsonObject& root = jsonBuffer.parseObject(json);
    if (root.success()) {
      JsonArray& mapMouseButtons = root["mapMouseButtons"];
      if (mapMouseButtons.success()) {
        for (int i = 0; i < 8; i++) {
          mapMouseButtonToXACRightButton[i] = mapMouseButtons[i];
          SerialDebug.printf("mapMouseButtonToXACRightButton[%d] = %d\n",
              i, mapMouseButtonToXACRightButton[i]);
        }
      }
      else {
        SerialDebug.println("JSON parse mapMouseButtons fail.");
      }
    }
    else {
      SerialDebug.println("JSON parse root fail.");
    }
  }
  else {
    SerialDebug.println("Empty JSON file.");
  }
}

void setup()
{
  SerialDebug.begin(115200);
  SerialDebug.println("\n\nUSB Host Testing");

  load_config();
  Joystick.BEGIN();

  myusb.begin();
}


void loop()
{
  static int X, Y;
  myusb.Task();

  for (uint8_t i = 0; i < CNT_DEVICES; i++) {
    if (*drivers[i] != driver_active[i]) {
      if (driver_active[i]) {
        SerialDebug.printf("*** Device %s - disconnected ***\n", driver_names[i]);
        driver_active[i] = false;
      } else {
        SerialDebug.printf("*** Device %s %x:%x - connected ***\n", driver_names[i], drivers[i]->idVendor(), drivers[i]->idProduct());
        driver_active[i] = true;

        const uint8_t *psz = drivers[i]->manufacturer();
        if (psz && *psz) SerialDebug.printf("  manufacturer: %s\n", psz);
        psz = drivers[i]->product();
        if (psz && *psz) SerialDebug.printf("  product: %s\n", psz);
        psz = drivers[i]->serialNumber();
        if (psz && *psz) SerialDebug.printf("  Serial: %s\n", psz);
      }
    }
  }

  for (uint8_t i = 0; i < CNT_HIDDEVICES; i++) {
    if (*hiddrivers[i] != hid_driver_active[i]) {
      if (hid_driver_active[i]) {
        SerialDebug.printf("*** HID Device %s - disconnected ***\n", hid_driver_names[i]);
        hid_driver_active[i] = false;
      } else {
        SerialDebug.printf("*** HID Device %s %x:%x - connected ***\n", hid_driver_names[i], hiddrivers[i]->idVendor(), hiddrivers[i]->idProduct());
        hid_driver_active[i] = true;

        const uint8_t *psz = hiddrivers[i]->manufacturer();
        if (psz && *psz) SerialDebug.printf("  manufacturer: %s\n", psz);
        psz = hiddrivers[i]->product();
        if (psz && *psz) SerialDebug.printf("  product: %s\n", psz);
        psz = hiddrivers[i]->serialNumber();
        if (psz && *psz) SerialDebug.printf("  Serial: %s\n", psz);
      }
    }
  }

  if(mouse1.available()) {
    static int X_absmax=50, Y_absmax=50;

    uint8_t buttons = mouse1.getButtons();
    SerialDebug.print("Mouse: buttons = ");
    SerialDebug.print(buttons, HEX);
    for (int i = 0; i < 8; i++) {
      Joystick.SETBUTTON(mapMouseButtonToXACRightButton[i],
          (buttons & (1<<i))!=0);
    }

    X = mouse1.getMouseX();
    SerialDebug.print(",  mouseX = ");
    SerialDebug.print(X);
    int X_abs = abs(X);
    if (X_abs > X_absmax) {
      X_absmax = X_abs;
      SerialDebug.print(", X_absmax = ");
      SerialDebug.print(X_absmax);
    }
    Joystick.SETXAXIS(map(X, -X_absmax, X_absmax, 0, 1023));

    Y = mouse1.getMouseY();
    SerialDebug.print(",  mouseY = ");
    SerialDebug.print(Y);
    int Y_abs = abs(Y);
    if (Y_abs > Y_absmax) {
      Y_absmax = Y_abs;
      SerialDebug.print(", Y_absmax = ");
      SerialDebug.print(Y_absmax);
    }
    Joystick.SETYAXIS(map(Y, -Y_absmax, Y_absmax, 0, 1023));

    SerialDebug.print(",  wheel = ");
    SerialDebug.print(mouse1.getWheel());
    SerialDebug.print(",  wheelH = ");
    SerialDebug.print(mouse1.getWheelH());
    SerialDebug.println();
    mouse1.mouseDataClear();

    Joystick.SENDSTATE();
    BetweenMouseMoves = 0;
  }
  else {
    // If joystick not centered after 10ms of no mouse x,y input
    if ((X != 512) || (Y != 512)) {
      if (BetweenMouseMoves > 10) {
        // Center the joystick
        X = Y = 512;
        Joystick.SETXAXIS(X);
        Joystick.SETYAXIS(Y);
        Joystick.SENDSTATE();
        BetweenMouseMoves = 0;
      }
    }
  }
}
