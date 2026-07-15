#include "MessageManager.h"
#include "Constants.h"

void MessageManager::update(unsigned long currentTimeMs) {
  lastUpdateTime = currentTimeMs;
  clearExpiredMessages();
  sortMessagesByPriority();
}

void MessageManager::clearExpiredMessages() {
  messageQueue.erase(
    std::remove_if(messageQueue.begin(), messageQueue.end(),
      [](const QueuedMessage& msg) {
        return msg.displayed || millis() > msg.scheduleTime + msg.relevanceWindow;
      }),
    messageQueue.end()
  );
}

void MessageManager::sortMessagesByPriority() {
  std::sort(messageQueue.begin(), messageQueue.end(),
    [](const QueuedMessage& a, const QueuedMessage& b) {
      if (a.priority != b.priority) return a.priority > b.priority;
      return a.scheduleTime < b.scheduleTime;
    });
}

bool MessageManager::hasMessagesDue() {
  for (const auto& msg : messageQueue) {
    if (!msg.displayed && millis() >= msg.scheduleTime) {
      if (millis() <= msg.scheduleTime + msg.relevanceWindow) {
        return true;
      }
    }
  }
  return false;
}

bool MessageManager::hasScheduledMessages() {
  return !messageQueue.empty();
}

MessageManager::DueMessage MessageManager::getNextDueMessage() {
  for (auto& msg : messageQueue) {
    if (!msg.displayed) {
      if (millis() >= msg.scheduleTime) {
        if (millis() <= msg.scheduleTime + msg.relevanceWindow) {
          msg.displayed = true;
          return DueMessage(msg.eventType, msg.content);
        }
      }
    }
  }
  return DueMessage(-1, "");
}

void MessageManager::scheduleMessage(int eventType, const String& content,
                                     unsigned long priority,
                                     unsigned long delayMs,
                                     unsigned long relevanceWindow) {
  QueuedMessage msg(content, priority,
                   millis() + delayMs,
                   relevanceWindow > 0 ? relevanceWindow : RELEVANCE_NORMAL,
                   eventType);
  messageQueue.push_back(msg);
  sortMessagesByPriority();
}

void MessageManager::scheduleMessageWithPriority(int eventType, const String& content,
                                                   int priority,
                                                   unsigned long delayMs,
                                                   int relevance) {
  unsigned long p;
  switch (priority) {
    case P_URGENT:  p = PRIORITY_HIGH; break;
    case P_HIGH:    p = PRIORITY_HIGH; break;
    case P_NORMAL:  p = PRIORITY_NORMAL; break;
    default:        p = PRIORITY_LOW; break;
  }
  unsigned long r;
  switch (relevance) {
    case R_CRITICAL:  r = RELEVANCE_URGENT; break;
    case R_IMPORTANT: r = RELEVANCE_NORMAL; break;
    case R_NORMAL:    r = RELEVANCE_NORMAL; break;
    default:          r = RELEVANCE_LOW; break;
  }
  scheduleMessage(eventType, content, p, delayMs, r);
}

void MessageManager::triggerSmartEvent(int eventType, const String& content,
                                        unsigned long priority,
                                        unsigned long delayMs,
                                        unsigned long relevanceWindow) {
  scheduleMessage(eventType, content, priority, delayMs, relevanceWindow);
}

void MessageManager::scheduleWelcomeBackMessage(const String& breakDuration) {
  scheduleMessageWithPriority(
    EVENT_WELCOME_BACK,
    "Welcome back! Away for " + breakDuration,
    P_URGENT, WELCOME_DELAY_MS, R_IMPORTANT
  );
}

void MessageManager::scheduleFocusSessionCongrats(const String& focusDuration) {
  scheduleMessageWithPriority(
    EVENT_FOCUS_END,
    "Focus session: " + focusDuration + ". Great work!",
    P_HIGH, 0, R_NORMAL
  );
}

void MessageManager::scheduleProductivityHints() {
  scheduleMessageWithPriority(
    EVENT_STREAK_BEATEN,
    "Take a moment to reset. Small breaks boost focus.",
    P_NORMAL, 0, R_NORMAL
  );
}

void MessageManager::scheduleStretchReminder(unsigned long continuousTime) {
  if (continuousTime >= STRETCH_INTERVAL_MS) {
    scheduleMessageWithPriority(
      EVENT_STRETCH,
      "Time to stretch! Move a bit.",
      P_NORMAL, 0, R_NORMAL
    );
  }
}

void MessageManager::scheduleLunchReminder() {
  scheduleMessageWithPriority(
    EVENT_LUNCH_REMINDER,
    "Time to refuel! Take a lunch break.",
    P_NORMAL, LUNCH_REMINDER_DELAY_MS, R_BRIEF
  );
}
