#ifndef MESSAGEMANAGER_H
#define MESSAGEMANAGER_H

#include <Arduino.h>
#include <vector>
#include <algorithm>
#include "Behaviour.h"

#include "Constants.h"

// Priority queue for behaviour alerts.
// Dequeue rule: always serve the highest-priority pending message first.
// Lower priority never jumps ahead. Same priority is strict FIFO (insertion order).
// delayMs only controls when a message becomes eligible; it does not let lower
// priority cut in while a higher-priority message is still waiting.
// relevanceWindow is the TTL after scheduleTime; expired messages are dropped.
class MessageManager {
private:
  struct QueuedMessage {
    String content;
    unsigned long priority;
    unsigned long scheduleTime;
    unsigned long relevanceWindow;
    int eventType;
    uint32_t seq;
    bool displayed;

    QueuedMessage(const String& c, unsigned long p, unsigned long s,
                  unsigned long r, int e, uint32_t sequence, bool disp = false)
      : content(c), priority(p), scheduleTime(s), relevanceWindow(r),
        eventType(e), seq(sequence), displayed(disp) {}
  };

  std::vector<QueuedMessage> messageQueue;
  unsigned long lastUpdateTime;
  uint32_t nextSeq;

  void clearExpiredMessages();
  void sortQueue();

public:
  static constexpr unsigned long PRIORITY_URGENT = MSG_PRIORITY_URGENT;
  static constexpr unsigned long PRIORITY_HIGH = MSG_PRIORITY_HIGH;
  static constexpr unsigned long PRIORITY_NORMAL = MSG_PRIORITY_NORMAL;
  static constexpr unsigned long PRIORITY_LOW = MSG_PRIORITY_LOW;

  static constexpr unsigned long RELEVANCE_SHORT = MSG_RELEVANCE_SHORT;
  static constexpr unsigned long RELEVANCE_NORMAL = MSG_RELEVANCE_NORMAL;
  static constexpr unsigned long RELEVANCE_LONG = MSG_RELEVANCE_LONG;

  enum Priority { P_LOW = 0, P_NORMAL = 1, P_HIGH = 2, P_URGENT = 3 };
  // TTL classes (not priority): critical expire soon; important stays longer.
  enum Relevance { R_NORMAL = 2, R_IMPORTANT = 3, R_CRITICAL = 4 };
  struct DueMessage {
    int eventType;
    String content;
    DueMessage(int t, const String& c) : eventType(t), content(c) {}
  };

  MessageManager() : lastUpdateTime(0), nextSeq(0) {}

  void update(unsigned long currentTimeMs);
  DueMessage getNextDueMessage();

  void scheduleMessage(int eventType, const String& content,
                       unsigned long priority,
                       unsigned long delayMs = 0,
                       unsigned long relevanceWindow = 0);

  void scheduleMessageWithPriority(int eventType, const String& content,
                                   int priority,
                                   unsigned long delayMs = 0,
                                   int relevance = R_NORMAL);

  void scheduleGreetingMessage(int eventType, const String& detail);
};

#endif
