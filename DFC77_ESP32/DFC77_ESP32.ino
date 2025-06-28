/*
    based on this sketch: https://github.com/aknik/ESP32/blob/master/DFC77/DFC77_esp32_Solo.ino

    Some functions are inspired by work of G6EJD ( https://www.youtube.com/channel/UCgtlqH_lkMdIa4jZLItcsTg )

    Refactor by DeltaZero, converts to syncronous, added "cron" that you can bypass, see line 29
                                                                                                        The cron does not start until 10 minutes from reset (see constant onTimeAfterReset)
    Every clock I know starts to listen to the radio at aproximatelly the hour o'clock, so cron takes this into account

    Alarm clocks from Junghans: Every hour (innecesery)
    Weather Station from Brigmton: 2 and 3 AM
    Chinesse movements and derivatives: 1 o'clock AM
*/


#include <WiFi.h>
#include <Ticker.h>
#include <time.h>

#define DEBUG_OLED

#ifdef DEBUG_OLED
#include <M5UnitOLED.h>
M5UnitOLED display;
bool oneShotDraw = false;
#endif /* DEBUG_OLED */

#include <LiteLED.h>
#define LED_TYPE LED_STRIP_SK6812
#define LED_TYPE_IS_RGBW 0
#define LED_GPIO 27
#define LED_BRIGHTNESS 1
static const crgb_t L_RED = 0xff0000;
static const crgb_t L_GREEN = 0x00ff00;
static const crgb_t L_BLUE = 0x0000ff;
static const crgb_t L_WHITE = 0xe0e0e0;
static const crgb_t L_YELLOW = 0xf0f000;
LiteLED myLED(LED_TYPE, LED_TYPE_IS_RGBW);

#include "credentials.h"    // If you put this file in the same folder that the rest of the tabs, then use "" to delimiter,
                            // otherwise use <> or comment it and write your credentials directly on code
                            // const char* ssid = "YourOwnSSID";
                            // const char* password = "YourSoSecretPassword";

#define ANTENNAPIN 21     // Antenna pin. Connect antenna from here to ground, use a 1k resistor to limit transmitting power. A slightly tuned ferrite antenna gets around 3 meters and a wire loop may work if close enough.
//#define CONTINUOUSMODE    // Uncomment this line to bypass de cron and have the transmitter on all the time

// cron (if you choose the correct values you can even run on batteries)
// If you choose really bad this minutes, everything goes wrong, so minuteToWakeUp must be greater than minuteToSleep
#define minuteToWakeUp 50    // Every hoursToWakeUp at this minute the ESP32 wakes up get time and star to transmit
#define minuteToSleep 10     // If it is running at this minute then goes to sleep and waits until minuteToWakeUp

// you can add more hours to adapt to your needs
// When the ESP32 wakes up, check if the actual hour is in the list and
// runs or goes to sleep until next minuteToWakeUp
byte hoursToWakeUp[] = { 0, 1, 2, 3};    

Ticker tickerDecisec;    // TBD at 100ms

//complete array of pulses for a minute
//0 = no pulse, 1=100ms, 2=200ms
int impulseArray[60];
int impulseCount = 0;
int actualHours, actualMinutes, actualSecond, actualDay, actualMonth, actualYear, DayOfW;
long dontGoToSleep = 0;
const long onTimeAfterReset = 600000;    // run tx for ten minutes after reset regardless of cron settings
int timeRunningContinuous = 0;

const char* ntpServer = "ch.pool.ntp.org";                               // enter your closer pool or pool.ntp.org
const char* TZ_INFO = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";    // enter your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)

struct tm timeinfo;

void flashLED(crgb_t, int, int);
void oledWrite(const char*, const char*, const char*, const char*);

void setup() {
#ifdef DEBUG_OLED
    display.begin();
    display.setColorDepth(1);
    if (display.isEPD())
    {
        display.setEpdMode(epd_mode_t::epd_fastest);
        display.invertDisplay(true);
        display.clear(TFT_BLACK);
    }
    if (display.width() < display.height())
    {
        display.setRotation(display.getRotation() ^ 1);
    }
    display.setFont(&fonts::FreeMono9pt7b);
    display.setTextSize(1);
#endif /* DEBUG_OLED */

    myLED.begin(LED_GPIO, 1);
    myLED.brightness(LED_BRIGHTNESS);    // set the LED photon intensity level
    myLED.setPixel(0, L_RED, 1);         // set the LED colour and show it
    delay(1000);

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    Serial.begin(115200);
    Serial.println();
    Serial.println("DCF77 transmitter");

#ifdef DEBUG_OLED
    oledWrite("init...", "", "", "");
#endif /* DEBUG_OLED */
    //can be added to save energy when battery-operated
    if (setCpuFrequencyMhz(80)) {
        #ifdef DEBUG_OLED
            oledWrite(NULL, "set fCPU...", NULL, NULL);
        #endif /* DEBUG_OLED */
        Serial.print("CPU frequency set @");
        Serial.print(getCpuFrequencyMhz());
        Serial.println("Mhz");
        flashLED(L_GREEN, 3, 200);
        #ifdef DEBUG_OLED
            oledWrite(NULL, NULL, "done!", NULL);
            delay(500);
        #endif /* DEBUG_OLED */
    } else {
        #ifdef DEBUG_OLED
            oledWrite(NULL, NULL, "failed :(", NULL);
            delay(500);
        #endif /* DEBUG_OLED */
        Serial.println("Fail to set cpu frequency");
        flashLED(L_RED, 3, 200);
    }

    if (esp_sleep_get_wakeup_cause() == 0) dontGoToSleep = millis();

    ledcAttach(ANTENNAPIN, 77500, 8);    // Set pin PWM, 77500hz DCF freq, resolution of 8bit


    #ifdef DEBUG_OLED
        oledWrite(NULL, "Wi-Fi...", "", NULL);
    #endif /* DEBUG_OLED */
    myLED.setPixel(0, L_BLUE, 1);
    WiFi_on();
    #ifdef DEBUG_OLED
        oledWrite(NULL, NULL, "done!", NULL);
        delay(500);
    #endif /* DEBUG_OLED */
    #ifdef DEBUG_OLED
        oledWrite(NULL, "NTP...", "", NULL);
    #endif /* DEBUG_OLED */
    getNTP();
    #ifdef DEBUG_OLED
        oledWrite(NULL, NULL, "done!", NULL);
        delay(500);
    #endif /* DEBUG_OLED */
    #ifdef DEBUG_OLED
        oledWrite(NULL, "Wi-Fi off...", "", NULL);
    #endif /* DEBUG_OLED */
    WiFi_off();
    flashLED(L_BLUE, 3, 200);
    #ifdef DEBUG_OLED
        char currentTime[9];
        sprintf(currentTime, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        oledWrite("Last tNTP:", currentTime, NULL, NULL);
    #endif /* DEBUG_OLED */
    show_time();
#ifdef DEBUG_OLED
    oledWrite(NULL, NULL, "Sent:", "...");
#endif /* DEBUG_OLED */

    myLED.setPixel(0, L_GREEN, 1);
    CodeTime();    // first conversion just for cronCheck
#ifndef CONTINUOUSMODE
    if ((dontGoToSleep == 0) or ((dontGoToSleep + onTimeAfterReset) < millis())) cronCheck();    // first check before start anything
#else
    Serial.println("CONTINUOUS MODE NO CRON!!!");
#endif

    // sync to the start of a second
    Serial.print("Syncing... ");
    int startSecond = timeinfo.tm_sec;
    long count = 0;
    do {
        count++;
        if (!getLocalTime(&timeinfo)) {
            Serial.println("Error obtaining time...");
            delay(3000);
            ESP.restart();
        }
    } while (startSecond == timeinfo.tm_sec);

    tickerDecisec.attach_ms(100, DcfOut);    // from now on calls DcfOut every 100ms
    Serial.print("Ok ");
    Serial.println(count);
}

void loop() {
    // There is no code inside the loop. This is a syncronous program driven by the Ticker
}

void CodeTime() {
    DayOfW = timeinfo.tm_wday;
    if (DayOfW == 0) DayOfW = 7;
    actualDay = timeinfo.tm_mday;
    actualMonth = timeinfo.tm_mon + 1;
    actualYear = timeinfo.tm_year - 100;
    actualHours = timeinfo.tm_hour;
    actualMinutes = timeinfo.tm_min + 1;    // DCF77 transmitts the next minute
    if (actualMinutes >= 60) {
        actualMinutes = 0;
        actualHours++;
    }
    actualSecond = timeinfo.tm_sec;
    if (actualSecond == 60) actualSecond = 0;

#ifdef DEBUG_OLED
    if (actualSecond == 0 && !oneShotDraw)
    {
        char actualTime[6];
        sprintf(actualTime, "%02d:%02d", actualHours, actualMinutes);
        oledWrite(NULL, NULL, NULL, actualTime);
        oneShotDraw = true;
    }
    if (actualSecond != 0)
    {
        oneShotDraw = false;
    }
#endif /* DEBUG_OLED */

    int n, Tmp, TmpIn;
    int ParityCount = 0;

    //we put the first 20 bits of each minute at a logical zero value
    for (n = 0; n < 20; n++) impulseArray[n] = 1;

    // set DST bit
    if (timeinfo.tm_isdst == 0) {
        impulseArray[18] = 2;    // CET or DST OFF
    } else {
        impulseArray[17] = 2;    // CEST or DST ON
    }

    //bit 20 must be 1 to indicate active time
    impulseArray[20] = 2;

    //calculates the bits for the minutes
    TmpIn = Bin2Bcd(actualMinutes);
    for (n = 21; n < 28; n++) {
        Tmp = TmpIn & 1;
        impulseArray[n] = Tmp + 1;
        ParityCount += Tmp;
        TmpIn >>= 1;
    }
    if ((ParityCount & 1) == 0)
        impulseArray[28] = 1;
    else
        impulseArray[28] = 2;

    //calculates bits for the hours
    ParityCount = 0;
    TmpIn = Bin2Bcd(actualHours);
    for (n = 29; n < 35; n++) {
        Tmp = TmpIn & 1;
        impulseArray[n] = Tmp + 1;
        ParityCount += Tmp;
        TmpIn >>= 1;
    }
    if ((ParityCount & 1) == 0)
        impulseArray[35] = 1;
    else
        impulseArray[35] = 2;
    ParityCount = 0;

    //calculate the bits for the actual Day of Month
    TmpIn = Bin2Bcd(actualDay);
    for (n = 36; n < 42; n++) {
        Tmp = TmpIn & 1;
        impulseArray[n] = Tmp + 1;
        ParityCount += Tmp;
        TmpIn >>= 1;
    }
    TmpIn = Bin2Bcd(DayOfW);
    for (n = 42; n < 45; n++) {
        Tmp = TmpIn & 1;
        impulseArray[n] = Tmp + 1;
        ParityCount += Tmp;
        TmpIn >>= 1;
    }
    //calculates the bits for the actualMonth
    TmpIn = Bin2Bcd(actualMonth);
    for (n = 45; n < 50; n++) {
        Tmp = TmpIn & 1;
        impulseArray[n] = Tmp + 1;
        ParityCount += Tmp;
        TmpIn >>= 1;
    }
    //calculates the bits for actual year
    TmpIn = Bin2Bcd(actualYear);    // 2 digit year
    for (n = 50; n < 58; n++) {
        Tmp = TmpIn & 1;
        impulseArray[n] = Tmp + 1;
        ParityCount += Tmp;
        TmpIn >>= 1;
    }
    //equal date
    if ((ParityCount & 1) == 0)
        impulseArray[58] = 1;
    else
        impulseArray[58] = 2;

    //last missing pulse
    impulseArray[59] = 0;    // No pulse
}

int Bin2Bcd(int dato) {
    int msb, lsb;
    if (dato < 10)
        return dato;
    msb = (dato / 10) << 4;
    lsb = dato % 10;
    return msb + lsb;
}

void DcfOut() {
    switch (impulseCount++) {
        case 0:
            if (impulseArray[actualSecond] != 0) {
                myLED.brightness(LED_BRIGHTNESS, 1);
                ledcWrite(ANTENNAPIN, 0);
            }
            break;
        case 1:
            if (impulseArray[actualSecond] == 1) {
                myLED.brightness(0, 1);
                ledcWrite(ANTENNAPIN, 127);
            }
            break;
        case 2:
            myLED.brightness(0, 1);
            ledcWrite(ANTENNAPIN, 127);
            break;
        case 9:
            impulseCount = 0;

            if (actualSecond == 1 || actualSecond == 15 || actualSecond == 21 || actualSecond == 29) Serial.print("-");
            if (actualSecond == 36 || actualSecond == 42 || actualSecond == 45 || actualSecond == 50) Serial.print("-");
            if (actualSecond == 28 || actualSecond == 35 || actualSecond == 58) Serial.print("P");

            if (impulseArray[actualSecond] == 1)
            {
#ifdef DEBUG_OLED
                char bitInfo[6];
                sprintf(bitInfo, "0[%02d]", actualSecond);
                display.drawString(bitInfo, 64, 33);
#endif /* DEBUG_OLED */
                Serial.print("0");
            }
            if (impulseArray[actualSecond] == 2)
            {
#ifdef DEBUG_OLED
                char bitInfo[6];
                sprintf(bitInfo, "1[%02d]", actualSecond);
                display.drawString(bitInfo, 64, 33);
#endif /* DEBUG_OLED */
                Serial.print("1");
            }

            if (actualSecond == 59) {
                Serial.println();
#ifdef DEBUG_OLED
                char bitInfo[6];
                sprintf(bitInfo, "![%02d]", actualSecond);
                display.drawString(bitInfo, 64, 33);
#endif /* DEBUG_OLED */
                show_time();
#ifndef CONTINUOUSMODE
                if ((dontGoToSleep == 0) or ((dontGoToSleep + onTimeAfterReset) < millis())) cronCheck();
#else
                Serial.println("CONTINUOUS MODE NO CRON!!!");
                timeRunningContinuous++;
                if (timeRunningContinuous > 360) ESP.restart();    // 6 hours running, then restart all over
#endif
            }
            break;
    }
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Error obtaining time...");
        #ifdef DEBUG_OLED
            oledWrite("TimeGetErr", "resetting", NULL, NULL);
        #endif /* DEBUG_OLED */
        delay(3000);
        ESP.restart();
    }
    CodeTime();
}

void flashLED(crgb_t color, int num, int intervalMs) {
    crgb_t oldColor = myLED.getPixelC(0);
    myLED.setPixel(0, color, 1);
    myLED.brightness(LED_BRIGHTNESS, 1);
    for (int i = 0; i < num; i++) {
        delay(intervalMs);
        myLED.brightness(0, 1);
        delay(intervalMs);
        myLED.brightness(LED_BRIGHTNESS, 1);
    }
    myLED.setPixel(0, oldColor, 1);
}

#ifdef DEBUG_OLED
void oledWrite(const char* line1, const char* line2, const char* line3, const char* line4) {
    /* LINE 1 */
    if (line1)
    {
        display.drawString("                ", 1, 1);
        display.drawString(line1, 1, 1);
    }
    /* LINE 2 */
    if (line2)
    {
        display.drawString("                ", 1, 17);
        display.drawString(line2, 1, 17);
    }
    /* LINE 3 */
    if (line3)
    {
        display.drawString("                ", 1, 33);
        display.drawString(line3, 1, 33);
    }
    /* LINE 4 */
    if (line4)
    {
        display.drawString("                ", 1, 49);
        display.drawString(line4, 1, 49);
    }
}
#endif