/*
  ESP8266Ping.cpp - Ping library for ESP8266.
  Copyright (c) 2015 Daniele Colanardi. All rights reserved.
  Fixed for ESP8266 Core 3.x by Gemini
*/

#include "ESP8266Ping.h"

#define PING_ID 0xAFAF

ESP8266Ping Ping;

ESP8266Ping::ESP8266Ping() {
  _pcb = NULL;
  _ping_running = false;
  _ping_timeout = 1000; // Default
}

ESP8266Ping::~ESP8266Ping() {
  _remove_pcb();
}

void ESP8266Ping::_create_pcb() {
  if (_pcb == NULL) {
    _pcb = raw_new(IP_PROTO_ICMP);
    raw_bind(_pcb, IP_ADDR_ANY);
    raw_recv(_pcb, _recv_cb, this);
  }
}

void ESP8266Ping::_remove_pcb() {
  if (_pcb != NULL) {
    raw_remove(_pcb);
    _pcb = NULL;
  }
}

u8_t ESP8266Ping::_recv_cb(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr) {
  ESP8266Ping *instance = (ESP8266Ping *)arg;
  struct icmp_echo_hdr *iecho;

  if (pbuf_header(p, -PBUF_IP_HLEN) == 0) {
    iecho = (struct icmp_echo_hdr *)p->payload;

    if ((iecho->id == PING_ID) && (iecho->seqno == htons(instance->_ping_seq_num))) {
      int time_taken = millis() - instance->_ping_start_time;
      instance->_ping_recv++;
      instance->_ping_total_time += time_taken;
      if (time_taken < instance->_ping_min_time) {
        instance->_ping_min_time = time_taken;
      }
      if (time_taken > instance->_ping_max_time) {
        instance->_ping_max_time = time_taken;
      }
    }
  }
  pbuf_free(p);

  if (instance->_ping_recv + instance->_ping_sent >= instance->_ping_count) {
    instance->_finish_ping();
  }
  
  return 1; 
}

void ESP8266Ping::_timeout_cb(void *arg) {
  ESP8266Ping *instance = (ESP8266Ping *)arg;
  
  if (instance->_ping_recv + instance->_ping_sent >= instance->_ping_count) {
    instance->_finish_ping();
  } else {
    instance->_send_ping(&instance->_ping_target);
  }
}

void ESP8266Ping::_send_ping(ip_addr_t *addr) {
  struct pbuf *p;
  struct icmp_echo_hdr *iecho;

  p = pbuf_alloc(PBUF_IP, sizeof(struct icmp_echo_hdr), PBUF_RAM);
  if (!p) {
    return;
  }

  iecho = (struct icmp_echo_hdr *)p->payload;
  iecho->type = ICMP_ECHO;
  iecho->code = 0;
  iecho->id = PING_ID;
  iecho->seqno = htons(++_ping_seq_num);

  iecho->chksum = 0;
  iecho->chksum = inet_chksum(iecho, sizeof(struct icmp_echo_hdr));

  _ping_start_time = millis();
  raw_sendto(_pcb, p, addr);
  
  _ping_sent++; 
  
  pbuf_free(p);

  os_timer_disarm(&_ping_timer);
  os_timer_setfn(&_ping_timer, (os_timer_func_t *)_timeout_cb, this);
  os_timer_arm(&_ping_timer, _ping_timeout, false); // Usar timeout dinámico
}

void ESP8266Ping::_finish_ping() {
  os_timer_disarm(&_ping_timer);
  _ping_running = false;
  _remove_pcb();
}

bool ESP8266Ping::ping(IPAddress ip, int count, int timeout) {
  if (_ping_running) {
    return false;
  }

  _create_pcb();
  if (_pcb == NULL) {
    return false;
  }

  _ping_target.addr = ip;
  _ping_count = count;
  _ping_timeout = timeout; // Set timeout
  _ping_sent = 0;
  _ping_recv = 0;
  _ping_min_time = 0x7FFFFFFF; 
  _ping_max_time = 0;
  _ping_total_time = 0;
  _ping_seq_num = 0;
  _ping_running = true;

  _send_ping(&_ping_target);

  // Wait for ping to complete
  while (_ping_running) {
    yield();
  }

  return _ping_recv > 0;
}

bool ESP8266Ping::ping(const char *host, int count, int timeout) {
  ip_addr_t addr;
  if (WiFi.hostByName(host, (IPAddress&)addr)) {
       return ping((IPAddress&)addr, count, timeout);
  }
  return false;
}

int ESP8266Ping::averageTime() {
  if (_ping_recv == 0) {
    return 0;
  }
  return _ping_total_time / _ping_recv;
}

int ESP8266Ping::maxTime() {
  return _ping_max_time;
}

int ESP8266Ping::minTime() {
  return _ping_min_time;
}

int ESP8266Ping::packetsRecv() {
  return _ping_recv;
}

int ESP8266Ping::packetsSent() {
  return _ping_sent;
}
