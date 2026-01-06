/*
  ESP8266Ping.h - Ping library for ESP8266.
  Copyright (c) 2015 Daniele Colanardi. All rights reserved.
  Fixed for ESP8266 Core 3.x by Gemini
*/

#ifndef ESP8266Ping_h
#define ESP8266Ping_h

#include <Arduino.h>
#include <ESP8266WiFi.h>

extern "C" {
  #include <lwip/icmp.h>
  #include <lwip/inet_chksum.h>
  #include <lwip/raw.h>
  #include <lwip/netif.h>
  #include <lwip/init.h>
}

#include <functional>

typedef std::function<void(void)> THandlerFunction;

class ESP8266Ping {
public:
  ESP8266Ping();
  ~ESP8266Ping();

  bool ping(IPAddress ip, int count = 1, int timeout = 1000);
  bool ping(const char *host, int count = 1, int timeout = 1000);

  int averageTime();
  int maxTime();
  int minTime();
  int packetsRecv();
  int packetsSent();

private:
  static u8_t _recv_cb(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr);
  static void _timeout_cb(void *arg);

  void _create_pcb();
  void _remove_pcb();
  void _send_ping(ip_addr_t *addr);
  void _finish_ping();

  struct raw_pcb *_pcb;
  ip_addr_t _ping_target;
  uint16_t _ping_seq_num;
  uint32_t _ping_start_time;
  int _ping_count;
  int _ping_timeout; // Variable para almacenar el timeout dinámico
  int _ping_sent;
  int _ping_recv;
  int _ping_min_time;
  int _ping_max_time;
  int _ping_total_time;
  bool _ping_running;
  os_timer_t _ping_timer;
};

extern ESP8266Ping Ping;

#endif