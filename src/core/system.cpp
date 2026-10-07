#include "core/system.h"

#include <M5Unified.h>
#include "config.h"
#include "core/display.h"
#include "core/log.h"
#include "core/power.h"
#include "core/protocol.h"

void system_init(void)
{
    auto cfg = M5.config();
    cfg.internal_spk = false;   // 未使用の内蔵ブザー・マイクは初期化しない
    cfg.internal_mic = false;
    M5.begin(cfg);

    setCpuFrequencyMhz(POWER_CPU_FREQ_MHZ);
    Serial.begin(COMM_SERIAL_BAUD);
    delay(300);

    display_init();
    power_init();

#if defined(ROLE_NODE)
    LOG_I("Beer-fw node (NODE_ID=%d) protocol v%d%s", NODE_ID, PROTOCOL_VERSION, COMM_ENABLE ? "" : " [standalone]");
#else
    LOG_I("Beer-fw gateway protocol v%d", PROTOCOL_VERSION);
#endif
}

void system_update(void)
{
    M5.update();

    uint32_t now = millis();
    if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
        display_wake();
    }
    display_update(now);
    power_update(now);
}
