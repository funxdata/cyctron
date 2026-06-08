#pragma once

#include "arch.h"

struct ct_timer {
  uint64_t period_ms;          // Timer period in milliseconds
  uint64_t expire;             // Expiration timestamp in milliseconds
  unsigned flags;              // Possible flags values below
#define CT_TIMER_ONCE 0        // Call function once
#define CT_TIMER_REPEAT 1      // Call function periodically
#define CT_TIMER_RUN_NOW 2     // Call immediately when timer is set
#define CT_TIMER_CALLED 4      // Timer function was called at least once
#define CT_TIMER_AUTODELETE 8  // mg_free() timer when done
  void (*fn)(void *);          // Function to call
  void *arg;                   // Function argument
  struct ct_timer *next;       // Linkage
};

void ct_timer_init(struct ct_timer **head, struct ct_timer *timer,
                   uint64_t milliseconds, unsigned flags, void (*fn)(void *),
                   void *arg);
void ct_timer_free(struct ct_timer **head, struct mg_timer *);
void ct_timer_poll(struct ct_timer **head, uint64_t new_ms);
bool ct_timer_expired(uint64_t *expiration, uint64_t period, uint64_t now);