#include "config.h"
#include "persist.h"

#include <stdio.h>
#include <string.h>

CONFIG::CONFIG()
{
    memset(&cfgdata, 0, sizeof(cfgdata));
}

bool CONFIG::read_config()
{
    int nr = -1;
    int fd = Persist::get()->open("config", O_READ);
    if (fd != -1)
    {
        strncpy(cfgdata.title, "Web Mouse", sizeof(cfgdata.title));
        nr = Persist::get()->read(fd, (uint8_t *)&cfgdata, sizeof(cfgdata));
        Persist::get()->close(fd);
    }
    return (nr >= ((uint8_t *)&cfgdata.title - (uint8_t *)&cfgdata));
}

bool CONFIG::write_config()
{
    bool ret = false;
    int fd = Persist::get()->open("config", O_WRITE);
    if (fd !=-1)
    {
        int stsw = Persist::get()->write(fd, (uint8_t *)&cfgdata, sizeof(cfgdata));
        int stsc = Persist::get()->close(fd);
        ret = stsw == sizeof(cfgdata) && stsc == 0;
    }
    else
    {
        printf("Failed to open config file for write\n");
    }
    return ret;
}

bool CONFIG::init()
{
    Persist::get()->init();
    bool ret = read_config();
    if (!ret)
    {
        //  No read set default data
        strncpy(cfgdata.hostname, HOSTNAME, sizeof(cfgdata.hostname) - 1);
        strncpy(cfgdata.ssid, WIFI_SSID, sizeof(cfgdata.ssid) - 1);
        strncpy(cfgdata.password, WIFI_PASSWORD, sizeof(cfgdata.password) - 1);
        ret = write_config();
    }
    return ret;
}

bool CONFIG::set_hostname(const char *hostname)
{
    strncpy(cfgdata.hostname, hostname, sizeof(cfgdata.hostname) - 1);
    return write_config();
}

bool CONFIG::set_wifi_credentials(const char *ssid, const char *password)
{
    strncpy(cfgdata.ssid, ssid, sizeof(cfgdata.ssid) - 1);
    strncpy(cfgdata.password, password, sizeof(cfgdata.password) - 1);
    return write_config();
}

bool CONFIG::set_title(const char *title)
{
    strncpy(cfgdata.title, title, sizeof(cfgdata.title) - 1);
    return write_config();
}