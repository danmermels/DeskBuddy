#include "MessageManager.h"
#include "Constants.h"

void MessageManager::update(unsigned long currentTimeMs) {
  lastUpdateTime = currentTimeMs;
  clearExpiredMessages();
  sortQueue();
}

void MessageManager::clearExpiredMessages() {
  unsigned long now = millis();
  messageQueue.erase(
    std::remove_if(messageQueue.begin(), messageQueue.end(),
      [now](const QueuedMessage& msg) {
        return msg.displayed || now > msg.scheduleTime + msg.relevanceWindow;
      }),
    messageQueue.end()
  );
}

void MessageManager::sortQueue() {
  // Highest priority first; same priority keeps insertion order (FIFO).
  std::sort(messageQueue.begin(), messageQueue.end(),
    [](const QueuedMessage& a, const QueuedMessage& b) {
      if (a.priority != b.priority) return a.priority > b.priority;
      return a.seq < b.seq;
    });
}

MessageManager::DueMessage MessageManager::getNextDueMessage(bool journalSequenceActive) {
  if (messageQueue.empty()) return DueMessage(-1, "");

  unsigned long now = millis();
  for (auto& msg : messageQueue) {
    if (msg.displayed) continue;
    if (now > msg.scheduleTime + msg.relevanceWindow) continue;
    if (now < msg.scheduleTime) {
      return DueMessage(-1, "");
    }
    // During journal sequence, only allow journal pages and follow-ups through
    if (journalSequenceActive && msg.eventType != EVENT_JOURNAL && msg.eventType != EVENT_PAGE) {
      continue;
    }
    msg.displayed = true;
    return DueMessage(msg.eventType, msg.content);
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
                   eventType, nextSeq++);
  messageQueue.push_back(msg);
  sortQueue();
  if (messageQueue.size() > MESSAGE_QUEUE_MAX) {
    messageQueue.pop_back(); // bound memory: drop lowest-priority queued message
  }
}

void MessageManager::scheduleMessageWithPriority(int eventType, const String& content,
                                                   int priority,
                                                   unsigned long delayMs,
                                                   int relevance) {
  unsigned long p;
  switch (priority) {
    case P_URGENT:  p = PRIORITY_URGENT; break;
    case P_HIGH:    p = PRIORITY_HIGH; break;
    case P_NORMAL:  p = PRIORITY_NORMAL; break;
    default:        p = PRIORITY_LOW; break;
  }
  // TTL: critical = short; normal = medium; important = long.
  unsigned long r;
  switch (relevance) {
    case R_CRITICAL:    r = RELEVANCE_SHORT; break;
    case R_IMPORTANT:   r = RELEVANCE_LONG; break;
    case R_NORMAL:
    default:            r = RELEVANCE_NORMAL; break;
  }
  scheduleMessage(eventType, content, p, delayMs, r);
}

void MessageManager::scheduleGreetingMessage(int eventType, const String& detail) {
  // Erase any competing greeting (only one greeting per sit-down).
  messageQueue.erase(
    std::remove_if(messageQueue.begin(), messageQueue.end(),
      [](const QueuedMessage& msg) {
        return msg.eventType == EVENT_WELCOME_BACK || msg.eventType == EVENT_FIRST_SIT || msg.eventType == EVENT_LATEHOURS_SIT;
      }),
    messageQueue.end()
  );

  // Immediate queue entry at URGENT. Display grace is WELCOME_HOLD_MS in Display.h.
  scheduleMessageWithPriority(eventType, detail, P_URGENT, 0, R_IMPORTANT);
}
