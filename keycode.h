#ifndef KEYCODE_H
#define KEYCODE_H

#include <stdint.h>

class KEYCODE
{
private:
    static bool lookup_keycode(uint8_t character, const uint8_t * table, int size, uint8_t &keycode);

public:
    static bool get_code_and_modifier(uint8_t character, uint8_t &keycode, uint8_t &modifier);
};

#endif