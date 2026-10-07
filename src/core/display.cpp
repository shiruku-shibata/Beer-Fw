#include "core/display.h"

#include <stdio.h>
#include <string.h>
#include "config.h"
#include "core/log.h"
#include "core/power.h"

static uint32_t s_lastDraw     = 0;
static uint32_t s_lastActivity = 0;
static bool     s_on           = true;

void display_init(void)
{
    M5.Display.setRotation(3);
    M5.Display.setBrightness(DISPLAY_BRIGHTNESS);
    s_lastActivity = millis();
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
}

bool display_due(uint32_t now)
{
    if (!s_on) return false;
    if (now - s_lastDraw < DISPLAY_PERIOD_MS) return false;
    s_lastDraw = now;
    return true;
}

void display_wake(void)
{
    s_lastActivity = millis();
    if (s_on) return;

    M5.Display.wakeup();
    s_on       = true;
    s_lastDraw = 0;   // すぐ描き直す
    LOG_D("Display on");
}

void display_update(uint32_t now)
{
#if DISPLAY_AUTO_OFF_MS > 0
    if (s_on && now - s_lastActivity >= DISPLAY_AUTO_OFF_MS) {
        M5.Display.sleep();
        s_on = false;
        LOG_D("Display off");
    }
#else
    (void)now;
#endif
}

bool display_is_on(void)
{
    return s_on;
}

void display_begin(const char *title, uint16_t color)
{
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    display_print(title, 2, color);
    M5.Display.println();
}

void display_print(const char *text, uint8_t size, uint16_t color)
{
    M5.Display.setTextSize(size);
    M5.Display.setTextColor(color, TFT_BLACK);
    M5.Display.print(text);
}

void display_footer(void)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "BAT:%d%%", power_battery_level());

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    M5.Display.setCursor(M5.Display.width() - 6 * (int)strlen(buf), M5.Display.height() - 8);
    M5.Display.print(buf);
}

void display_fatal(const char *message)
{
    LOG_E("%s", message);
    display_wake();
    M5.Display.fillScreen(TFT_RED);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(TFT_WHITE, TFT_RED);
    M5.Display.println(message);
    while (true) { delay(1000); }
}
