//                  *****  Web Mouse Watchdog Class Implementation  *****

#include <webmouse_watchdog.h>
#include <hardware/watchdog.h>
#include <pico/cyw43_arch.h>
#include <cyw43_locker.h>

#define WATCHDOG_TIMEOUT_MSEC   8000

WebmouseWatchdog::WebmouseWatchdog() : watchdog_active_(false)
{
    watchdog_worker_ = {.do_work = watchdog_periodic, .user_data = this};
    check_enabled_ = default_check_enabled_;
    watchdog_periodic(cyw43_arch_async_context());
}

WebmouseWatchdog::~WebmouseWatchdog()
{
    watchdog_disable();
    async_context_remove_at_time_worker(cyw43_arch_async_context(), &watchdog_worker_);
}

void WebmouseWatchdog::watchdog_periodic(async_context_t *ctx)
{
    if (watchdog_active_)
    {
        if (check_enabled_())
        {
            CYW43Locker lock;
            watchdog_update();
        }
        else
        {
            watchdog_disable();
            watchdog_active_ = false;
            printf("watchdog disabled\n");
        }
    }
    else
    {
        if (check_enabled_())
        {
            watchdog_enable(WATCHDOG_TIMEOUT_MSEC, true);
            watchdog_active_ = true;
            printf("watchdog enabled\n");
        }
    }

    async_context_add_at_time_worker_in_ms(ctx, &watchdog_worker_, WATCHDOG_TIMEOUT_MSEC / 2);
}