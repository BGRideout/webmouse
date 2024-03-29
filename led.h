//      ***** LED flash class  *****

#ifndef LED_H
#define LED_H

#include <initializer_list>
#include <vector>

#include "pico/async_context.h"

class LED
{
private:
    bool                on_;
    std::vector<bool>   flash_pattern_;
    int                 flash_index_;
    void flash();
    void set_led(bool on);

    bool                updating_;
    std::vector<bool>   update_;

    static LED *singleton_;

    async_at_time_worker_t time_worker_;
    static void timer_callback(async_context_t *context, async_at_time_worker_t *worker);

    LED();

public:
    static LED *get() {if (!singleton_) singleton_ = new LED(); return singleton_; }
    void set_flash(const std::initializer_list<bool> &pattern = {});

    void begin_pattern_update();
    void add_to_pattern(const std::initializer_list<bool> &pattern);
    void end_pattern_update();
};

#endif
