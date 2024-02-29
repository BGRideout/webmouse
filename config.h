#ifndef CONFIG_H
#define CONFIG_H

#define WIFI_SSID "VodafoneMobileWiFi-B56878"
#define WIFI_PASSWORD "9647950309"

class CONFIG
{
private:
    struct _config
    {
        char    hostname[32];
        char    ssid[64];
        char    password[64];
    } cfgdata;

    CONFIG();
    bool read_config();
    bool write_config();

public:
    static CONFIG *get() {static CONFIG *singleton = nullptr; if (!singleton) singleton = new CONFIG(); return singleton;}
    bool init();

    const char *hostname() const { return cfgdata.hostname; }
    const char *ssid() const { return cfgdata.ssid; }
    const char *password() const { return cfgdata.password; }

    bool set_hostname(const char *hostname);
    bool set_wifi_credentials(const char *ssid, const char *password);
};

#endif
