/*
    by Bradyn Braithwaite, 2020
*/
#include "Arduino.h"
#include "alcObjects.h"
alckAdvanced nanoAlck;
void setup() {
    nanoAlck.dynamicBrightness = true;
    nanoAlck.useTempRoutine    = true;
    nanoAlck.wakeTargetOffset.setHour(10);
    nanoAlck.wakeTargetOffset.setMinute(0);
}
void loop() {
    nanoAlck.runNow();
}
