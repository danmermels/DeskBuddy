#ifndef MESSAGEMANAGER_H
#define MESSAGEMANAGER_H

#include <Arduino.h>
#include <vector>
#include <algorithm>
#include "Behaviour.h"

#include "Constants.h"

class MessageManager {
private:
  struct QueuedMessage {
    String content;
    unsigned long priority;
    unsigned long scheduleTime;
    unsigned long relevanceWindow;
    int eventType;
    bool displayed;

    QueuedMessage(const String& c, unsigned long p, unsigned long s,
                  unsigned long r, int e, bool disp = false)
      : content(c), priority(p), scheduleTime(s), relevanceWindow(r),
        eventType(e), displayed(disp) {}
  };

  std::vector<QueuedMessage> messageQueue;
  unsigned long lastUpdateTime;

  void clearExpiredMessages();
  void sortMessagesByPriority();

public:
  static constexpr unsigned long PRIORITY_HIGH = MSG_PRIORITY_HIGH;
  static constexpr unsigned long PRIORITY_NORMAL = MSG_PRIORITY_NORMAL;
  static constexpr unsigned long PRIORITY_LOW = MSG_PRIORITY_LOW;

  static constexpr unsigned long RELEVANCE_URGENT = MSG_RELEVANCE_URGENT;
  static constexpr unsigned long RELEVANCE_NORMAL = MSG_RELEVANCE_NORMAL;
  static constexpr unsigned long RELEVANCE_LOW = MSG_RELEVANCE_LOW;

  enum Priority { P_LOW = 0, P_NORMAL = 1, P_HIGH = 2, P_URGENT = 3 };
  enum Relevance { R_BRIEF = 1, R_NORMAL = 2, R_IMPORTANT = 3, R_CRITICAL = 4 };
  struct DueMessage {
    int eventType;
    String content;
    DueMessage(int t, const String& c) : eventType(t), content(c) {}
  };

  MessageManager() : lastUpdateTime(0) {}

  void update(unsigned long currentTimeMs);
  bool hasMessagesDue();
  bool hasScheduledMessages();
  DueMessage getNextDueMessage();

  void scheduleMessage(int eventType, const String& content,
                       unsigned long priority,
                       unsigned long delayMs = 0,
                       unsigned long relevanceWindow = 0);

  void scheduleMessageWithPriority(int eventType, const String& content,
                                   int priority,
                                   unsigned long delayMs = 0,
                                   int relevance = R_NORMAL);

  void triggerSmartEvent(int eventType, const String& content,
                         unsigned long priority = 1500,
                         unsigned long delayMs = 5000,
                         unsigned long relevanceWindow = 300000);

  // Convenience scheduling methods
  void scheduleWelcomeBackMessage(const String& breakDuration);
  void scheduleFocusSessionCongrats(const String& focusDuration);
  void scheduleProductivityHints();
  void scheduleStretchReminder(unsigned long continuousTime);
  void scheduleLunchReminder();
};

#endif
