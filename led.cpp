//      *****  LED Class Implementation  *****

#include "led.h"
#include "pico/cyw43_arch.h"

#define FLASH_INTERVAL 150

LED *LED::singleton_ = nullptr;

LED::LED() : on_(false), flash_index_(0), updating_(false), time_worker_({0})
{
    set_led(false);
    time_worker_.do_work = timer_callback;
    async_context_add_at_time_worker_in_ms(cyw43_arch_async_context(), &time_worker_, FLASH_INTERVAL);
}

void LED::set_led(bool on)
{
    cyw43_thread_enter();
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
    cyw43_thread_exit();
    on_ = on;
}

void LED::set_flash(const std::initializer_list<bool> &pattern)
{
    flash_index_ = 0;
    flash_pattern_ = pattern;
}

void LED::begin_pattern_update()
{
    update_.clear();
    updating_ = false;
}

void LED::add_to_pattern(const std::initializer_list<bool> &pattern)
{
    update_.insert(update_.end(), pattern);
}

void LED::end_pattern_update()
{
    updating_ = true;
}

void LED::flash()
{
    if (flash_index_ >= flash_pattern_.size())
    {
        flash_index_ = 0;
    }

    if (flash_index_ == 0 && updating_)
    {
        flash_pattern_ = update_;
        update_.clear();
        updating_ = false;
    }

    if (flash_pattern_.size() > 0)
    {
        bool on = flash_pattern_.at(flash_index_);
        if (on ^ on_)
        {
            set_led(on);
        }
        flash_index_ += 1;
    }
    else
    {
        if (on_)
        {
            set_led(false);
        }
    }
}

void LED::timer_callback(async_context_t *context, async_at_time_worker_t *worker)
{
    async_context_add_at_time_worker_in_ms(context, worker, FLASH_INTERVAL);
    get()->flash();
}
