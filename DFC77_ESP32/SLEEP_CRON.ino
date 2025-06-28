#include <LiteLED.h>

void sleepForMinutes(int minutes) {
    if (minutes < 2) return;
    myLED.brightness(0, 1);     // make sure the RGB LED is off when sleeping
    uint64_t uSecToMinutes = 60000000;
    esp_sleep_enable_timer_wakeup(minutes * uSecToMinutes);  // this timer works on uSecs, so 60M by minute
    //WiFi_off();
    Serial.print("To sleep... ");
    Serial.print(minutes);
    Serial.println(" minutes");
    Serial.flush();
#ifdef DEBUG_OLED
    display.clear(TFT_BLACK);
    char sleepingTime[6];
    sprintf(sleepingTime, "%02d:%02d", actualHours, actualMinutes);
    uint todMinutes = actualHours * 60 + actualMinutes;
    uint todUntilMinutes = (todMinutes + minutes) % (24*60);
    uint8_t untilHours = todUntilMinutes/60;
    uint8_t untilMinutes = todUntilMinutes % 60;
    char forDisplay[11];
    sprintf(forDisplay, "For %d min", minutes);
    char untilDisplay[8];
    sprintf(untilDisplay, "> %02d:%02d", untilHours, untilMinutes);
    oledWrite("Sleeping:", sleepingTime, forDisplay, untilDisplay);
#endif /* DEBUG_OLED */
    esp_deep_sleep_start();
}

void cronCheck() {
    // is this hour in the list?
    boolean work = false;
    for (int n = 0; n < sizeof(hoursToWakeUp); n++) {
        //Serial.println(sizeof(hoursToWakeUp)); Serial.print(work); Serial.print(" "); Serial.print(n); Serial.print(" "); Serial.print(actualHours); Serial.print(" "); Serial.println(hoursToWakeUp[n]);
        if ((actualHours == hoursToWakeUp[n]) or (actualHours == (hoursToWakeUp[n] + 1))){
        work = true;
        // is this the minute to go to sleep?
        if ((actualMinutes > minuteToSleep) and (actualMinutes < minuteToWakeUp)) {
            // go to sleep (minuteToWakeUp - actualMinutes)
            Serial.print(".");
            sleepForMinutes(minuteToWakeUp - actualMinutes);   
        }
        break;
        }
    }
    if (work == false) { // sleep until minuteToWakeUp (take into account the ESP32 can start for some reason between minuteToWakeUp and o'clock)
        if (actualMinutes >= minuteToWakeUp) {
            Serial.print("..");
        sleepForMinutes(minuteToWakeUp + 60 - actualMinutes);
        } else {
        // goto sleep for (minuteToWakeUp - actualMinutes) minutes
            Serial.print("...");
        sleepForMinutes(minuteToWakeUp - actualMinutes);        
        }
    }
}
