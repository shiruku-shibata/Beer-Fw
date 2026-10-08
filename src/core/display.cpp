#include "core/display.h"

#include <stdio.h>
#include <string.h>
#include "config.h"
#include "core/log.h"
#include "core/power.h"

static uint32_t s_lastDraw     = 0;
static uint32_t s_lastActivity = 0;
static bool     s_on           = true;

/**
 * @brief LCD を横向き・黒背景・既定の明るさで初期化し、自動消灯タイマーを開始する。
 */
void display_init(void)
{
    M5.Display.setRotation(3);
    M5.Display.setBrightness(DISPLAY_BRIGHTNESS);
    s_lastActivity = millis();
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
}

/**
 * @brief 描き直してよいか判定する（点灯中かつ前回から DISPLAY_PERIOD_MS 経過）。
 *
 * @param now 現在時刻 [ms]
 * @return 描き直すなら true（描画時刻を更新する）
 */
bool display_due(uint32_t now)
{
    if (!s_on) return false;
    if (now - s_lastDraw < DISPLAY_PERIOD_MS) return false;
    s_lastDraw = now;
    return true;
}

/**
 * @brief 画面を点灯し、自動消灯までの時間を延長する。消灯中なら次の判定ですぐ描き直す。
 */
void display_wake(void)
{
    s_lastActivity = millis();
    if (s_on) return;

    M5.Display.wakeup();
    s_on       = true;
    s_lastDraw = 0;   // すぐ描き直す
    LOG_D("Display on");
}

/**
 * @brief DISPLAY_AUTO_OFF_MS 操作がなければ画面を消す（パネルスリープ＋バックライト消灯）。
 *
 * @param now 現在時刻 [ms]
 */
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

/**
 * @brief 画面が点灯中か。
 */
bool display_is_on(void)
{
    return s_on;
}

/**
 * @brief 画面をクリアし、1 行目にタイトルを描く。
 *
 * @param title タイトル文字列
 * @param color 文字色（TFT_*）
 */
void display_begin(const char *title, uint16_t color)
{
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    display_print(title, 2, color);
    M5.Display.println();
}

/**
 * @brief 現在のカーソル位置から文字列を描く。
 *
 * @param text 文字列（改行可）
 * @param size 文字サイズ
 * @param color 文字色（TFT_*）
 */
void display_print(const char *text, uint8_t size, uint16_t color)
{
    M5.Display.setTextSize(size);
    M5.Display.setTextColor(color, TFT_BLACK);
    M5.Display.print(text);
}

/**
 * @brief 画面右下に電池残量（BAT:xx%）を描く。
 */
void display_footer(void)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "BAT:%d%%", power_battery_level());

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    M5.Display.setCursor(M5.Display.width() - 6 * (int)strlen(buf), M5.Display.height() - 8);
    M5.Display.print(buf);
}

/**
 * @brief 致命的エラー。画面を点灯して赤背景にメッセージを出し、停止する（復帰しない）。
 *
 * @param message 表示するメッセージ
 */
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
