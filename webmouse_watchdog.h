//                  *****  Web Mouse Watchdog Class  *****

#ifndef WEBMOUSE_WATCHDOG_H
#define WEBMOUSE_WATCHDOG_H

#include <pico/async_context.h>

class WebmouseWatchdog
{
private:
    bool                    watchdog_active_;           // Watchdog active flag
    async_at_time_worker_t  watchdog_worker_;           // Worker function

    void watchdog_periodic(async_context_t *ctx);
    static void watchdog_periodic(async_context_t *ctx, async_at_time_worker_t *param)
        {static_cast<WebmouseWatchdog *>(param->user_data)->watchdog_periodic(ctx);}

    bool (*check_enabled_)();
    static bool default_check_enabled_() { return true; }

public:
    WebmouseWatchdog();
    ~WebmouseWatchdog();

    void setEnableCheck(bool (*cb)()) { check_enabled_ = cb ? cb : default_check_enabled_; }
};

#endif
