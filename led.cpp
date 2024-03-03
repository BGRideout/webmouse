//      *****  LED Class Implementation  *****

#include "led.h"
#include "pico/cyw43_arch.h"

LED *LED::singleton_ = nullptr;

LED::LED() : on_(false), flash_index_(0), updating_(false)
{
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
    add_repeating_timer_ms(250, timer_callback, this, &timer_);
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
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
            on_ = on;
        }
        flash_index_ += 1;
    }
    else
    {
        if (on_)
        {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
            on_ = false;
        }
    }
}

bool LED::timer_callback(repeating_timer_t *rt)
{
    get()->flash();
    return true;
}
