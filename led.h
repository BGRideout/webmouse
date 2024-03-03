//      ***** LED flash class  *****

#ifndef LED_H
#define LED_H

#include <initializer_list>
#include <vector>

#include "pico/time.h"

class LED
{
private:
    bool                on_;
    std::vector<bool>   flash_pattern_;
    int                 flash_index_;
    void flash();

    bool                updating_;
    std::vector<bool>   update_;

    static LED *singleton_;

    repeating_timer_t timer_;
    static bool timer_callback(repeating_timer_t *rt);

    LED();

public:
    static LED *get() {if (!singleton_) singleton_ = new LED(); return singleton_; }
    void set_flash(const std::initializer_list<bool> &pattern = {});

    void begin_pattern_update();
    void add_to_pattern(const std::initializer_list<bool> &pattern);
    void end_pattern_update();
};

#endif
