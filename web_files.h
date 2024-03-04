#ifndef WEB_FILES_H
#define WEB_FILES_H

#include <string>
#include <stdint.h>
#include <map>

class WEB_FILES
{
private:
    std::map<std::string, const char *> files_;

    static WEB_FILES *singleton_;
    WEB_FILES();

public:
    static WEB_FILES *get() { if (!singleton_) singleton_ = new WEB_FILES(); return singleton_; }
    bool get_file(const std::string &name, const char * &data, uint16_t &datalen);
};

#endif
