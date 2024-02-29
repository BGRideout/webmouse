#ifndef WEB_H
#define WEB_H

#include <list>
#include <map>
#include <string>
#include <vector>

#include "lwip/tcp.h"
#include "lwip/pbuf.h"

#include "ws.h"

class WEB
{
private:
    struct tcp_pcb      *server_;                   // Server PCB

    class SENDBUF
    {
    private:
        uint8_t     *buffer_;                   // Buffer pointer
        int32_t     size_;                      // Buffer length
        int32_t     sent_;                      // Bytes sent
        bool        allocated_;                 // Buffer allocated

    public:
        SENDBUF() : buffer_(nullptr), size_(0), sent_(0), allocated_(false) {}
        SENDBUF(void *buf, uint32_t size, bool alloc = true);
        ~SENDBUF();

        uint32_t to_send() const { return size_ - sent_; }
        bool get_next(u16_t count, void **buffer, u16_t *buflen);
        void requeue(void *buffer, u16_t buflen);
    };

    class CLIENT
    {
    private:
        std::string     rqst_;                      // Request message
        bool            closed_;                    // Closed flag
        bool            websocket_;                 // Web socket open flag

        std::list<SENDBUF *> sendbuf_;              // Send buffers
        WebsocketPacketHeader_t wshdr_;             // Websocket message header

    public:
        CLIENT() : closed_(false), websocket_(false) {}
        ~CLIENT();

        void addToRqst(const char *str, u16_t ll);
        bool rqstIsReady();
        bool getWSMessage();
        void clearRqst() { rqst_.clear(); }
        void resetRqst();
        const std::string &rqst() const { return rqst_; }
        const WebsocketPacketHeader_t &wshdr() const { return wshdr_; }

        bool isClosed() const { return closed_; }
        void setClosed() { closed_ = true; }

        void setWebSocket() { websocket_ = true; }
        bool isWebSocket() const { return websocket_; }

        void queue_send(void *buffer, u16_t buflen, bool allocate);
        bool get_next(u16_t count, void **buffer, u16_t *buflen);
        bool more_to_send() const { return sendbuf_.size() > 0; }
        void requeue(void *buffer, u16_t buflen);
    };
    std::map<struct tcp_pcb *, CLIENT> clients_;    // Connected clients

    static err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err);
    static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
    static err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
    static err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb);
    static void  tcp_server_err(void *arg, err_t err);

    void process_rqst(struct tcp_pcb *client_pcb);
    void send_home_page(struct tcp_pcb *client_pcb);
    void send_css_file(struct tcp_pcb *client_pcb);
    void send_webmouse_js_file(struct tcp_pcb *client_pcb);
    void send_websocket_js_file(struct tcp_pcb *client_pcb);
    void send_config_file(struct tcp_pcb *client_pcb);
    void send_config_js_file(struct tcp_pcb *client_pcb);

    void open_websocket(struct tcp_pcb *client_pcb, std::vector<std::string> &headers);
    void process_websocket(struct tcp_pcb *client_pcb);
    void send_websocket(struct tcp_pcb *client_pcb, enum WebSocketOpCode opc, const std::string &payload, bool mask = false);

    void get_wifi(struct tcp_pcb *client_pcb);

    void close_client(struct tcp_pcb *client_pcb, bool isClosed = false);
    
    static WEB          *singleton_;                // Singleton pointer
    WEB();

    err_t send_buffer(struct tcp_pcb *client_pcb, void *buffer, u16_t buflen, bool allocate = true);
    err_t write_next(struct tcp_pcb *client_pcb);

    void (*message_callback_)(const std::string &msg);

public:
    static WEB *get();
    bool init();

    void set_message_callback(void(*cb)(const std::string &msg)) { message_callback_ = cb; }
    void broadcast_websocket(const std::string &txt);
};

#endif
