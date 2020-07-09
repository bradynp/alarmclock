/*
    by Bradyn Braithwaite, 2020
*/
#include "alcObjects.h"

alck::alck() {

    thisClock         = new TM1637Display(displayClockPin, displayDataIOPin);
    temperatureSensor = new DHT(humidAndTempSensorPin, 11); // always set to mode 11
    temperatureSensor->begin();
    pinMode(button_A_setter, INPUT_PULLUP);
    pinMode(button_B_hour, INPUT_PULLUP);
    pinMode(button_C_minute, INPUT_PULLUP);
    pinMode(militaryPin, INPUT_PULLUP);
    pinMode(buzzerPin, OUTPUT);
    digitalWrite(buzzerPin, LOW);
    this->brightnessValue = 2;
    thisClock->setBrightness(brightnessValue);
    thisClock->clear();
    this->militaryTimeMode     = false;
    alarmIsSet                 = false;
    obeyDimTime                = false;
    Offset.hour                = 12;
    Offset.minute              = 0;
    alarmIsSet                 = false;
    wakeTargetOffset.hour      = 0;
    wakeTargetOffset.minute    = 0;
    darkHoursStart             = 0;
    darkHoursEnd               = 0;
    millisWhenButtonLastPushed = 0;
};
alck::~alck() {};
void alck::runNow() {
    do {
        lightSensorandBrightnessHandler();
        timingFunction();
        alarmingFunction();
        buttonInputHandler();
        temperatureFunction();
    } while (true);
}
unsigned int alck::outputTimeAsNumber(timeUnit t_offset) {
    const double time_scale = 1.0;
    static unsigned int hourComponent, minuteComponent, minute_true;
    unsigned long sec = time_scale * millis() / 1000;
    minute_true       = (sec / 60 + Offset.minute);
    minuteComponent   = minute_true % 60;
    hourComponent     = (minute_true / 60 + Offset.hour) % 24;
    return 100 * hourComponent + minuteComponent;
}

unsigned int alck::qTime() {
    return outputTimeAsNumber(Offset);
}

bool alck::noButtonsAreBeingPushed() {
    return !(!digitalRead(button_B_hour) || !digitalRead(button_C_minute) || !digitalRead(button_A_setter));
    // DeMorgan would be disappointed in me
}

unsigned long alck::timeSincelastButtonPush() {
    return millis() - millisWhenButtonLastPushed;
}

void alck::alarmingFunction() {
    static const float loudnessScale = 0.95;
    static bool markedToRun          = true;
    static bool sounding             = false;
    if (sounding || (alarmIsSet && (qTime() == 100 * (wakeTargetOffset.hour % 24) + wakeTargetOffset.minute % 60) && markedToRun)) {
        sounding = true;
        while (noButtonsAreBeingPushed()) {
            if (millis() % 400 > 200) {
                analogWrite(buzzerPin, 0xFF * loudnessScale);
            }
            else {
                digitalWrite(buzzerPin, LOW);
            }
        }
        while (!noButtonsAreBeingPushed()) { // means a button is being pushed
            sounding    = false;
            markedToRun = false;
            digitalWrite(buzzerPin, LOW); // halt buzzer
            delay(1000);                  // hopefully, not change anything by accident
        }
    }
    else if (!markedToRun && qTime() != (100 * wakeTargetOffset.hour + wakeTargetOffset.minute)) {
        markedToRun = true;
    }
}

void alck::temperatureFunction() {
    static const unsigned int intervalOfService = 5; // minutes between showing the temperature
    const unsigned int persistentDelay          = 5000;
    static bool markedToRun                     = true;
    static const unsigned int requiredDelay     = 18000;
    static float temperature_F;
    static float temperature_C;
    bool Celsius = militaryTimeMode;                                                                                                                       // deprecated
    if (timeSincelastButtonPush() > requiredDelay && qTime() != 1200 && qTime() % intervalOfService == 0 && markedToRun && digitalRead(button_C_minute)) { // note: button_C_minute: prevents showing temp while passing the trigger when setting time
        thisClock->clear();
        // Take reading:
        if (Celsius) {
            temperature_C = temperatureSensor->readTemperature(false); // false means celsius
            if (temperature_C < 100) {
                thisClock->showNumberDec(temperature_C, false, 2, 1);
                thisClock->showNumberHexEx(0xC, 0, false, 1, 3);
            }
        }
        else {
            temperature_F = temperatureSensor->readTemperature(true); // true means fahrenheit
            if (temperature_F < 100) {
                thisClock->showNumberDec(temperature_F, false, 2, 1);
                thisClock->showNumberHexEx(0xF, 0, false, 1, 3);
            }
        }
        delay(persistentDelay); // TODO: fix blocking
        markedToRun = false;
    }
    else if (qTime() % intervalOfService != 0) {
        markedToRun = true;
    }
}

void alck::timingFunction() {
    timeReadyToShow = qTime();
    if (!militaryTimeMode) {
        int augmentHour = qTime() / 100;
        if (augmentHour == 0) {
            timeReadyToShow += 1200;
        }
        else if (augmentHour > 12) {
            timeReadyToShow -= 1200;
        }
    }
    if (!alarmIsSet) {
        ColonController |= 255; // the colon flashes only when alarm is set.
    }
    else if (millis() % 2000 > 1000 && !militaryTimeMode) {
        ColonController |= 255; // mask all bits to show a colon. Some chips require mask = 64
    }
    else {
        ColonController = 0;
    }
    thisClock->showNumberDecEx(timeReadyToShow, ColonController, (timeReadyToShow < 99) || militaryTimeMode, 4, 0);
}

void alck::lightSensorandBrightnessHandler() {
    static bool markedToRun    = true;
    const bool dynamicLighting = false;                            // a future chassis design may allow for dynamic brightness adjustments
    const bool immediateChange = false;                            // the change is usually spread over a couple of minutes
    const short thresholds[4]  = {140, 250, 400, 750};             // temporary uncalibrated values
    unsigned int lightLevels   = analogRead(lightSensorAnalogPin); // use +5v and 15kOhm. ensure correct pull orientation
    if (obeyDimTime && qTime() % 100 >= darkHoursStart || qTime() % 100 <= darkHoursEnd) {
        thisClock->setBrightness(0);
    }
    else {
        if (dynamicLighting && !immediateChange) {
            if ((qTime() % 2 == 1) && markedToRun) {
                markedToRun = false;
                // proceed
                if (lightLevels < thresholds[0]) {
                    brightnessValue = 1;
                }
                else if (lightLevels < thresholds[1]) {
                    brightnessValue = 2;
                }
                else if (lightLevels < thresholds[2]) {
                    brightnessValue = 3;
                }
                else if (lightLevels < thresholds[3]) {
                    brightnessValue = 5;
                }
                else {
                    brightnessValue = 7;
                }
                thisClock->setBrightness(brightnessValue);
            }
            else if (qTime() % 2 != 1) {
                markedToRun = true;
            }
        }
        else if (dynamicLighting && immediateChange) {
            short linBrite = floor(0.0065525317 * lightLevels + 0.23986);
            thisClock->setBrightness(linBrite < 8 && linBrite >= 0 ? linBrite : 4);
            // values computed by linear regression with fallback
        }
        else {
            thisClock->setBrightness(brightnessValue);
        }
    }
}

void alck::flashRapidWhileSetup() {
    if (millis() % 1000 > 450) {
        thisClock->setBrightness(2);
    }
    else {
        thisClock->setBrightness(7);
    }
}

void alck::buttonInputHandler() {
    static const unsigned int set_wait_debounce = 500;
    // for checking releases; such as with temperature display
    if (!digitalRead(button_A_setter) || !digitalRead(button_B_hour) || !digitalRead(button_C_minute)) {
        millisWhenButtonLastPushed = millis();
    }
    // for toggling alarm
    if (!digitalRead(button_B_hour) && !digitalRead(button_C_minute)) { // press both hour and minute to toggle
        alarmIsSet = !alarmIsSet;
        delay(set_wait_debounce);
    }
    // for setting alarm
    if (!digitalRead(button_A_setter)) {
        delay(700);
        while (!digitalRead(button_A_setter)) {
            flashRapidWhileSetup();
            militaryTimeMode = true;
            thisClock->showNumberDecEx(100 * (wakeTargetOffset.hour % 24) + wakeTargetOffset.minute % 60, 64, true, 4, 0); // show the alarm clock setting
            militaryTimeMode = false;
            if (!digitalRead(button_B_hour)) {
                wakeTargetOffset.hour++;
                delay(120);
            }
            if (!digitalRead(button_C_minute)) {
                wakeTargetOffset.minute++;
                delay(120);
            }
        }
        // for setting time
        while (digitalRead(button_A_setter)) {
            flashRapidWhileSetup();
            militaryTimeMode = true;
            thisClock->showNumberDecEx(qTime(), 64, true, 4, 0); // show the time setting
            militaryTimeMode = false;
            if (!digitalRead(button_B_hour)) {
                Offset.hour++;
                delay(120);
            }
            if (!digitalRead(button_C_minute)) {
                Offset.minute++;
                delay(120);
            }
        }
        delay(set_wait_debounce);
    }
}