#include <Arduino.h>
#include "AppController.h"

#ifndef RGB_BUILTIN
#define RGB_BUILTIN 48
#endif

AppController app;

void setup() {
    neopixelWrite(RGB_BUILTIN, 0, 0, 0);
    app.begin();
}

void loop() {
    app.update();
}
