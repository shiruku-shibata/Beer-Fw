#include "core/scheduler.h"

#include "core/log.h"
#include "modules/module.h"

static bool     s_active[MODULE_MAX]  = {};
static uint32_t s_lastRun[MODULE_MAX] = {};

void scheduler_init(void)
{
    if (g_moduleCount > MODULE_MAX) {
        LOG_E("Too many modules: %u (max %d)", (unsigned)g_moduleCount, MODULE_MAX);
    }

    for (size_t i = 0; i < g_moduleCount && i < MODULE_MAX; i++) {
        const Module *m = g_modules[i];
        s_active[i] = (m->init == nullptr) || m->init();
        if (s_active[i]) {
            LOG_I("Module '%s' ready (id=0x%02X, %lu ms)", m->name, m->id, (unsigned long)m->period_ms);
        } else {
            LOG_W("Module '%s' init failed, disabled", m->name);
        }
    }
}

void scheduler_run(uint32_t now)
{
    for (size_t i = 0; i < g_moduleCount && i < MODULE_MAX; i++) {
        if (!s_active[i]) continue;

        const Module *m = g_modules[i];
        if (m->update == nullptr) continue;
        if (now - s_lastRun[i] < m->period_ms) continue;

        s_lastRun[i] = now;
        m->update();
    }
}

uint32_t scheduler_ms_until_next(uint32_t now, uint32_t max_ms)
{
    uint32_t wait = max_ms;
    for (size_t i = 0; i < g_moduleCount && i < MODULE_MAX; i++) {
        if (!s_active[i] || g_modules[i]->update == nullptr) continue;

        uint32_t elapsed = now - s_lastRun[i];
        uint32_t period  = g_modules[i]->period_ms;
        uint32_t remain  = (elapsed >= period) ? 0 : period - elapsed;
        if (remain < wait) wait = remain;
    }
    return wait;
}

bool scheduler_busy(void)
{
    for (size_t i = 0; i < g_moduleCount && i < MODULE_MAX; i++) {
        if (s_active[i] && g_modules[i]->busy != nullptr && g_modules[i]->busy()) return true;
    }
    return false;
}

bool scheduler_take_events(void)
{
    bool any = false;
    for (size_t i = 0; i < g_moduleCount && i < MODULE_MAX; i++) {
        if (s_active[i] && g_modules[i]->take_event != nullptr && g_modules[i]->take_event()) any = true;
    }
    return any;
}

bool scheduler_is_active(size_t index)
{
    return index < MODULE_MAX && s_active[index];
}
