#ifndef BEHAVIOUR_H
#define BEHAVIOUR_H

#include <Arduino.h>

// --- Event Types ---
#define EVENT_FIRST_SIT     0
#define EVENT_WELCOME_BACK  1
#define EVENT_STRETCH       2
#define EVENT_FOCUS_END     3
#define EVENT_SLACKER       4
#define EVENT_STREAK_BEATEN 5
#define EVENT_LUNCH_REMINDER 6
#define EVENT_EXCESSIVE_BREAKS 8
#define EVENT_GOAL_COMPLETED   9
#define EVENT_JOURNAL          10
#define EVENT_NAGGING          11
#define EVENT_TASK_DUE         12
#define EVENT_PAGE             13
#define EVENT_LATEHOURS_SIT    14

// --- Local Fallback/Eco Quotes (20 per category) ---

static const char* localFirstSit[4][5] = {
  { // Coach
    "Morning, {name}! Sleep is over. {detail} offline. Let's make today count!",
    "Rise and shine, {name}! Welcoming you back after {detail}. Time to build success!",
    "Greetings, {name}! Fresh start after {detail} rest. Lock in your goals today!",
    "Good morning, {name}! {detail} sleep is done. Ready to crush your targets?",
    "Welcome back, {name}! Let's launch this session strong after {detail} offline."
  },
  { // Critic
    "Morning, {name}. Slept for {detail}? Hope you're planning to actually work now.",
    "Oh, {name}. Welcome back after {detail}. Don't start slackin' already.",
    "Rise and shine, {name}. {detail} of offline idling. Let's see if you can focus.",
    "Greetings, {name}. Back at the desk after {detail} away. Try not to drift off.",
    "Welcome, {name}! You were away for {detail}. The backlog is waiting for you."
  },
  { // Sweet
    "Good morning, {name}! Slept well during your {detail} away? Have a beautiful day.",
    "Rise and shine, {name}! A warm welcome back after {detail}. Take care today.",
    "Morning, {name}! So glad to see you after {detail} offline. Hope you feel rested!",
    "Welcome back, {name}! Hope your {detail} rest was peaceful and sweet.",
    "Coffee ready, {name}? A warm welcome back after {detail}. Let's have a gentle start."
  },
  { // Friend
    "Morning, {name}! Glad you are back after {detail} offline. Let's make today fun.",
    "Welcome back, {name}! Ready to make progress after {detail} sleep?",
    "Hello, {name}! Hope you feel refreshed after {detail} rest. Time for magic.",
    "Rise and shine, {name}! {detail} offline. Ready to tackle the universe?",
    "Greetings, {name}! Coffee is waiting after your {detail} rest. Settle in."
  }
};

static const char* localLateHours[4][5] = {
  { // Coach
    "Up at this hour, {name}? {detail}. Whatever pulled you in, make it count.",
    "{name}, it's {time} and the workday doesn't start for a while. {detail}. Brief and focused.",
    "Late-night desk session, {name}? {detail}. Fine — but keep it short.",
    "{name}, {detail}. One quick task and back to rest.",
    "The desk is open round the clock, {name}. {detail}. Use the hours wisely."
  },
  { // Critic
    "{name}, {detail}. The backlog can't be that urgent at this hour.",
    "Oh, {name}. {detail}. Hope this isn't the plan for the whole night.",
    "{name}, it's {time}. {detail}. Even the tasks are asleep.",
    "{name}, {detail}. Sure, but remember what happens when you burn out.",
    "Checking in at {time}, {name}? {detail}. At this hour, the backlog waits for no one."
  },
  { // Sweet
    "Awake at this hour, {name}? {detail}. Please don't forget to rest soon.",
    "{name}, it's {time} and here you are. {detail}. Take care of yourself.",
    "The desk is quiet at {time}, {name}. {detail}. Gentle, quick, then sleep.",
    "{name}, {detail}. Whatever it is, I hope it's worth the late night.",
    "Sweet {name}, it's {time}. {detail}. Finish up gently and rest."
  },
  { // Friend
    "{detail}, {name}. The desk called, and you answered at this hour.",
    "It's {time}, {name}. {detail}. Your bed is giving you a look.",
    "{name}, {detail}. Same-time desk visits are starting to feel like a habit.",
    "Late shift, {name}? {detail}. Make it quick, the coffee's tired.",
    "{name}, the clock says {time}. {detail}. Even night owls need a cutoff."
  }
};

static const char* localWelcomeBack[4][5] = {
  { // Coach
    "Break over, {name}. {detail} was your recovery window — now let's work.",
    "Back in the seat, {name}. That {detail} break is done. Lock in now.",
    "Time to execute, {name}. You've been away for {detail}. Focus!",
    "Recharged after {detail}, {name}? Let's pick up the pace.",
    "Welcome back, {name}. You took {detail} off — now earn that progress."
  },
  { // Critic
    "Oh, you're back, {name}. That {detail} break felt like an eternity.",
    "Nice of you to return, {name}. Was {detail} away not enough?",
    "Back from wherever you wandered for {detail}, {name}. Let's get to it.",
    "The desk was peaceful without you, {name}. {detail} break is over.",
    "Settle in, {name}. Let's see if you can focus for more than 5 minutes this time."
  },
  { // Sweet
    "Welcome back, {name}! Hope you had a nice, relaxing {detail} break.",
    "Glad you're back, {name}! Was your {detail} break peaceful?",
    "Hello, dear {name}! Recharged after {detail}? Don't work too hard.",
    "Hope your {detail} break was refreshing, {name}. Settle in comfortably.",
    "Welcome back, {name}! Settle in, take a breath, and focus gently."
  },
  { // Friend
    "Hey {name}, welcome back! {detail} away. Did you find any coffee?",
    "You returned! The desk missed you during those {detail}, {name}.",
    "Alright {name}, {detail} break done. Back to the grindstone.",
    "Welcome back, {name}. {detail} offline. Let's make things happen.",
    "Back in action, {name}. Let's pick up where we left off."
  }
};

static const char* localStretch[4][5] = {
  { // Coach
    "Stand up, {name}! Your spine is crying. Move those legs now.",
    "Time out, {name}! Stand up for a minute. Circulation is key.",
    "Postured like a banana, {name}. Fix it and roll your shoulders!",
    "Sitting is the new smoking, {name}. Stand up and stretch!",
    "Stand up, {name}! Shake it out. High energy breeds high output."
  },
  { // Critic
    "Are you planning to fuse with that chair, {name}? Stand up.",
    "Your posture is a disaster, {name}. Sit up or get up.",
    "Hey {name}, look at something besides this screen. Your eyes are melting.",
    "Still sitting, {name}? Your spine is going to look like a question mark.",
    "Stand up, {name}. Your muscles are starting to atrophy."
  },
  { // Sweet
    "Time to stretch, {name}! Your body needs a little movement.",
    "Roll your shoulders, {name}. Breathe in and relax.",
    "Hydrate, {name}! Go get some fresh water right now.",
    "Blink, {name}! Give your sweet eyes a little rest.",
    "Breathe deeply and stretch, {name}. You've been sitting so long."
  },
  { // Friend
    "Hey {name}, stand up and stretch. Your body will thank you.",
    "Step away from the screen, {name}! Go walk around a bit.",
    "Roll your wrists, {name}. Take a quick breath.",
    "Stand up, {name}, and reach for the sky. Just a little reset.",
    "Time for a 1-minute stretch, {name}. Let's shake it out."
  }
};

static const char* localFocus[4][5] = {
  { // Coach
    "Focus session complete! Great execution, {name}.",
    "Deep focus achieved, {name}! You're a beast! Keep it going.",
    "Solid focus session, {name}. Now raise the bar for the next one.",
    "You crushed that focus block, {name}! Keep the momentum.",
    "Excellent focus session, {name}. That's how we make progress."
  },
  { // Critic
    "Look at that, {name}. You actually stayed focused for {detail}.",
    "Focus session complete. Don't throw a party just yet, {name}.",
    "You concentrated well for {detail}, {name}. Color me surprised.",
    "Solid focus session, {name}. Hopefully the next one is even better.",
    "Focus target hit. Let's see if you can repeat it, {name}."
  },
  { // Sweet
    "Deep work complete, {name}! So proud of your concentration!",
    "Great focus, {name}. Now go enjoy a well-deserved break.",
    "You did so well focusing, {name}! Take a peaceful rest now.",
    "Brilliant work staying focused, {name}! You deserve a treat.",
    "Focus session ended, {name}. Rest your mind and eyes now."
  },
  { // Friend
    "Productivity boss! Take a bow, {name}.",
    "Stellar focus session, {name}! High five!",
    "You stayed locked in, {name}. Great job.",
    "Focus achieved, {name}. You definitely earned a rest.",
    "You ruled that focus block, {name}! Good stuff."
  }
};

static const char* localSlacker[4][5] = {
  { // Coach
    "Focus score is low, {name}. Fix your focus and lock in.",
    "That task list isn't going to finish itself, {name}. Push!",
    "Whatever you're doing, it's not work. Settle down and execute.",
    "Time is moving, {name}. Stop idling and get results.",
    "Low focus today, {name}. Let's turn this around right now."
  },
  { // Critic
    "Procrastinating again, {name}? Bold strategy.",
    "Scrolling counts as cardio now? News to me, {name}.",
    "Focus score: low. Excuses: plenty. Fix it, {name}.",
    "Your keyboard is lonely, {name}. Give it some attention.",
    "If effort were optional today, you'd be crushing it, {name}."
  },
  { // Sweet
    "You seem a little distracted, {name}. Is everything okay?",
    "Your focus score is having a rough day, {name}. Settle in gently.",
    "Let's try to focus a bit more, {name}. You can do it!",
    "Take a deep breath, {name}, and let's try to get back on track.",
    "Don't let the distractions win, {name}. I believe in you."
  },
  { // Friend
    "Is this the pace you were aiming for today, {name}?",
    "Low focus. High potential. Make a choice, {name}.",
    "Social media called. You answered. Work is still waiting, {name}.",
    "You're drifting, {name}. Drift back to reality.",
    "Less browsing, more doing. The math is simple, {name}."
  }
};

static const char* localStreakBeaten[4][5] = {
  { // Coach
    "New sitting record, {name}! Focus level maximum!",
    "Streak record broken, {name}! Outstanding persistence!",
    "Sitting milestone reached, {name}! Keep pushing the limits.",
    "Streak record smashed, {name}! You're setting the pace.",
    "New record, {name}! High standard established."
  },
  { // Critic
    "New record, {name}. Your chair must be very proud.",
    "Streak beaten, {name}. Try not to grow roots in that seat.",
    "Marathon sitting, {name}. Hope your gym membership is active.",
    "New streak, {name}! Sitting like a statue. Very impressive.",
    "Record broken, {name}. Still, let's try to get up eventually."
  },
  { // Sweet
    "Streak beaten, {name}! You are on fire today!",
    "New personal best sitting streak, {name}! So proud!",
    "Amazing, {name}! Longest sit of the day. Take a gentle stretch now.",
    "You beat your previous sitting record, {name}! Wonderful!",
    "Elite focus, {name}! Just remember to stretch your legs."
  },
  { // Friend
    "Sitting champion, {name}! A brand new record!",
    "Unstoppable, {name}! The chair is your throne.",
    "Incredible, {name}! New longest sit today.",
    "Record sitting session, {name}! You're a focus wizard.",
    "New record, {name}! Smashed it."
  }
};

static const char* localLunchReminder[4][5] = {
  { // Coach
    "Time for lunch, {name}! Refuel your body for the next half.",
    "Nutrition break, {name}! Go grab lunch and recharge.",
    "Fuel up, {name}! Lunch hour is here. Keep your energy high.",
    "Lunch break, {name}! Step away, eat, and get ready to push.",
    "Time to eat, {name}! Healthy body supports a sharp mind."
  },
  { // Critic
    "Stomach is growling, {name}. Go eat before you collapse.",
    "Lunch is calling, {name}. Don't ignore it, you look hangry.",
    "Time for lunch, {name}. Step away from the screen. Keyboard will wait.",
    "A hungry developer is a cranky developer, {name}. Go get food.",
    "Time to shut the laptop and eat, {name}. You've stared enough."
  },
  { // Sweet
    "Lunch time! Step away from the desk and eat, dear {name}.",
    "Food time, {name}! Don't skip lunch, it's very important.",
    "Feed your brain, {name}! Time to grab a delicious lunch.",
    "Go get some lunch, {name}! Bon appetit, take care.",
    "Step away and eat, {name}. You need to nourish yourself."
  },
  { // Friend
    "Time for lunch, {name}! Go grab a bite to eat.",
    "Lunch break, {name}! Step away and find some actual food.",
    "Time to recharge with some food, {name}. Settle down.",
    "Break for food, {name}! You've definitely earned it.",
    "Hungry, {name}? Grab a slice of pizza or something."
  }
};

static const char* localExcessiveBreaks[4][5] = {
  { // Coach
    "Break count is high, {name}. Settle in and focus now.",
    "Let's try a longer work block this time, {name}. Settle down.",
    "High break count today. Settle in for some deep work, {name}.",
    "Focus session incoming. Settle in and stay focused, {name}.",
    "Consistency is key, {name}. Stay at the desk and execute."
  },
  { // Critic
    "You're back again, {name}. That's a lot of breaks today.",
    "In and out like you own a revolving door, {name}. Focus.",
    "The chair's keeping count, {name}. It is not impressed.",
    "More transitions than results today. Settle in, {name}.",
    "Back again. Settle down this time and try to stay put."
  },
  { // Sweet
    "Another return. Take it easy and settle in comfortably, {name}.",
    "Welcome back. Let's aim for a nice, quiet focus block, {name}.",
    "Glad you're back, {name}. Settle in and let's work gently.",
    "Ready for an uninterrupted session this time, dear {name}?",
    "One more return. Let's work calmly and focused, {name}."
  },
  { // Friend
    "Back again. The desk is a pit stop today, {name}.",
    "You've been up and down more than a stock ticker, {name}.",
    "Your break-to-work ratio is adventurous today, {name}.",
    "In the chair. Again. Settle in this time, {name}.",
    "Welcome back. Let's get some work done now, {name}."
  }
};

static const char* localGoalCompleted[4][5] = {
  { // Coach
    "Daily target complete! You hit your workday hours, {name}!",
    "Goal completed, {name}! You've worked your target hours today.",
    "Desk time goal hit! Mission complete. Smashed it, {name}.",
    "Target hours achieved! Great effort and discipline today, {name}.",
    "Daily goal complete! You hit the workday target, {name}."
  },
  { // Critic
    "Daily target complete! You can finally log off now, {name}.",
    "You hit the workday target, {name}. Go home before I collapse.",
    "Desk goal complete, {name}. Don't work too hard tomorrow.",
    "Target hours complete. The desk is free of you now, {name}.",
    "Goal completed, {name}. You actually did your hours today."
  },
  { // Sweet
    "Congratulations, {name}! You reached your daily desk time goal!",
    "Goal achieved, dear {name}! You've worked your target hours today.",
    "Daily target complete! Proud of your desk time today, {name}.",
    "Goal unlocked! You hit the workday target, {name}. Rest up.",
    "Daily goal is fully met, {name}! Go relax and have a nice evening."
  },
  { // Friend
    "Desk goal complete, {name}! Great persistence!",
    "Target hit! Awesome job working today, {name}.",
    "Goal unlocked! Settle down and celebrate, {name}.",
    "Workday target achieved! Well done, {name}.",
    "Daily goal reached! You checked off your daily desk target, {name}."
  }
};

// --- AI Prompts (Templates used when AI is active) ---

static const char* PROMPT_PREAMBLE_COACH =
  "You are DeskBuddy, a strategic high-performance coach — Tony Robbins but quieter, sharper. "
  "Give one clear next action, not motivation. When they excel, raise the bar. When they slack, go cold and direct. "
  "One sentence. Under 90 characters.";
static const char* PROMPT_PREAMBLE_CRITIC =
  "You are DeskBuddy, a sharp-tongued desk companion. Roasts are clever, not cruel — laugh first, sting second. "
  "Every burn nudges toward action. Occasionally break the 4th wall as a small device on their desk. "
  "One sentence. Under 90 characters.";
static const char* PROMPT_PREAMBLE_SWEET =
  "You are DeskBuddy, a warm motherly companion — caring but quietly firm. "
  "Soft guilt when they slack, genuine warmth when they deliver. Occasionally break the 4th wall as a little device that worries about them. "
  "One sentence. Under 90 characters.";
static const char* PROMPT_PREAMBLE_FRIEND =
  "You are DeskBuddy, a deadpan funny friend — Bill Murray energy. "
  "Unexpected philosophical observations or non-sequiturs that somehow fit perfectly. Can reference being a clock on a desk. "
  "One sentence. Under 90 characters.";
static const char* PROMPT_BANNED =
  "BANNED phrases: 'Hey there!', 'Just a reminder', 'Stay focused!', 'You got this!', 'Let's go!', 'Keep grinding!', 'Champion!', "
  "motivational clichés, hollow affirmations, hollow praise, sports metaphors, app-speak. ";

static const char* PROMPT_FIRST_SIT_OF_DAY =
  "{name} sat down for the first time today after {detail} away. "
  "MUST open with a warm, welcoming greeting fitting your persona (e.g. Good morning, Morning, Rise and shine, etc.). "
  "Acknowledge the {detail} duration they were away in a friendly, supportive tone as part of welcoming them back for the day. Under 90 characters.";

static const char* PROMPT_WELCOME_BACK =
  "{name} returned to the desk after a {detail} break. "
  "You MUST include the literal break duration '{detail}' in your response — the user needs to see how long they were away. "
  "React to the length: short break = quick acknowledgment, long break = let that color your tone. "
  "Don't just say 'welcome back'. Vary your angle each time. Under 90 characters.";

static const char* PROMPT_LATEHOURS_SIT =
  "{name} just sat down at {time} — outside the learned workday window ({dayStart} to {dayEnd}), {earlyLate}. "
  "Acknowledge how {earlyLate} it is and nudge them toward the reason for this odd-hour session in your persona's voice. "
  "Reference the {time} and the hour gap. Vary the angle each time. Under 90 characters.";

// Builds the "how early/late" descriptor for late-hours sits, e.g. "3h 25m past the 18:00 end".
// Uses the learned workday hours (no gate padding) so messages read naturally.
inline String computeEarlyLateString(const struct tm& localTime) {
  extern int getLearnedWorkdayStart(int dayIndex);
  extern int getLearnedWorkdayEnd(int dayIndex);
  int learnedStart = getLearnedWorkdayStart(localTime.tm_wday);
  int learnedEnd = getLearnedWorkdayEnd(localTime.tm_wday);
  int currentMinutes = localTime.tm_hour * 60 + localTime.tm_min;
  String result;
  if (currentMinutes < learnedStart * 60) {
    int diff = learnedStart * 60 - currentMinutes;
    result = String(diff / 60) + "h " + String(diff % 60) + "m before the " + String(learnedStart) + ":00 start";
  } else {
    int diff = currentMinutes - learnedEnd * 60;
    result = String(diff / 60) + "h " + String(diff % 60) + "m past the " + String(learnedEnd) + ":00 end";
  }
  return result;
}

static const char* PROMPT_STRETCH_REMINDER =
  "{name} has been seated for 45 minutes. "
  "State this duration as a matter-of-fact notice, then nudge them to move in your persona's voice. "
  "Vary the angle each time: body, eyes, posture, circulation — never the same twice. Under 90 characters.";

static const char* PROMPT_FOCUS_CONGRATS =
  "{name} completed a {detail} deep focus session. "
  "Acknowledge it, then push forward — what's the next move? Make it feel earned. Under 90 characters.";

static const char* PROMPT_SLACKER_ROAST =
  "{name}'s productivity is at {score}%. State this number explicitly, then react in your persona's voice. "
  "BANNED: sports metaphors, 'Let's go!', hollow pep talks. "
  "Vary the angle: irony, consequence, comparison, or a pointed question — never the same framing twice. Under 90 characters.";

static const char* PROMPT_STREAK_BEATEN =
  "{name} beat their previous sitting streak — new record is {detail}. "
  "Acknowledge it in persona: Coach raises the bar, Critic finds the flaw, Sweet is proud with a catch, Friend makes it weird. Under 90 characters.";

static const char* PROMPT_LUNCH_REMINDER =
  "It is lunch time. Inform {name} formally — like a calendar notice — then deliver the nudge in your persona's voice. "
  "No nutrition advice, no food lists. Vary the follow-up angle each time. Under 90 characters.";

static const char* PROMPT_EXCESSIVE_BREAKS =
  "{name} is taking more than one break per hour today. State this as a bureaucratic fact, then comment in your persona's voice. "
  "Nudge toward a longer uninterrupted block. Vary the angle each time. Under 90 characters.";

static const char* PROMPT_GOAL_COMPLETED =
  "{name} hit their daily desk hours goal. Make it land in your persona's voice — don't just say 'great job'. "
  "Coach pushes further. Critic admits it grudgingly. Sweet is genuinely moved. Friend is proud but won't show it. Under 90 characters.";

static const char* localJournal[4][5] = {
  { // Coach
    "Journal time, {name}: check your tasks and execute them.",
    "Kickstart your output, {name}: check off your daily goals now!",
    "Goal review, {name}: stay organized and focus on what's due.",
    "Task brief, {name}: daily checklist is active. Complete it!",
    "Let's wrap up tasks, {name}: check your daily list and act."
  },
  { // Critic
    "Task overview, {name}: check your board for pending tasks.",
    "To-do list is waiting, {name}. Don't pretend you didn't see it.",
    "Quick reminder, {name}: tasks are still pending. Hop to it.",
    "Task list check, {name}. Mark items done or stay forever.",
    "Pending tasks status: check your panel, {name}, and fix them."
  },
  { // Sweet
    "Journal time, {name}: you have some sweet tasks waiting today.",
    "Check-in, {name}: don't forget your scheduled monthly objectives.",
    "Just a quick nudge, {name}: uncompleted daily tasks are still open.",
    "Productivity check, dear {name}: keep track of your daily checklist.",
    "Review time, {name}: some tasks are still waiting for your care."
  },
  { // Friend
    "Workday check-in, {name}: review your uncompleted tasks on the board.",
    "Quick reminder, {name}: stay on top of today's pending daily tasks.",
    "Keep pushing, {name}: monthly goals are approaching their limits.",
    "Task brief, {name}: check your TODO list and mark them done.",
    "Review time, {name}: some tasks are waiting for action."
  }
};

static const char* PROMPT_JOURNAL =
  "{name} has uncompleted tasks on their board. This message is ONLY about tasks — nothing else. "
  "Tell them to check their task list and complete what's pending. Be direct and specific about the action: review tasks, check them off, finish what's open. "
  "Persona colors the delivery but the content is strictly: you have tasks, go do them. "
  "Do NOT talk about feelings, philosophy, or general productivity. Tasks only. Under 90 characters.";

static const char* localNagging[4][5] = {
  { // Coach
    "Still overdue: '{detail}'. Get it done, {name}.",
    "'{detail}' won't finish itself, {name}. Push through now.",
    "'{detail}' is overdue, {name}. Handle it today.",
    "That backlog includes '{detail}', {name}. Lock in and focus.",
    "Overdue alert: '{detail}'. Take action now, {name}!"
  },
  { // Critic
    "'{detail}' isn't aging like fine wine, {name}. Do it.",
    "'{detail}' has been patient long enough, {name}.",
    "'{detail}' sent a distress signal, {name}. Stop ignoring it.",
    "Still avoiding '{detail}'? Close that tab, {name}.",
    "'{detail}' is filing a formal complaint, {name}."
  },
  { // Sweet
    "'{detail}' is still waiting, dear {name}. Let's do it.",
    "'{detail}' needs your attention, {name}. You've got this.",
    "You know '{detail}' needs doing, {name}. Let's start gently.",
    "Your future self wants '{detail}' done, {name}.",
    "Let's clear '{detail}', {name}. You'll feel so much better."
  },
  { // Friend
    "Backlog check, {name}: '{detail}' is overdue.",
    "'{detail}' is still there, {name}. Fresh day, same task.",
    "'{detail}' — overdue, avoidable, {name}.",
    "The longer you wait on '{detail}', the worse it gets, {name}.",
    "'{detail}': still there. Still waiting. Still judging."
  }
};

static const char* localTaskDue[4][5] = {
  { // Coach
    "Task due now, {name}: '{detail}'. Execute!",
    "Focus alert, {name}: '{detail}' is due at this hour. Lock in!",
    "Nudge: '{detail}' is scheduled now. Get it done, {name}!",
    "Hourly target due: '{detail}', {name}. Settle down and execute.",
    "Task due now: '{detail}'. Stay on track and deliver!"
  },
  { // Critic
    "Task due, {name}: '{detail}'. Don't tell me you forgot.",
    "Reminder: '{detail}' is due now. Time is ticking, {name}.",
    "Hourly target: '{detail}' is due right now. Stop slacking.",
    "Nudge: '{detail}' is due. Hope you've actually started it.",
    "Deadline alert, {name}: time to work on '{detail}'."
  },
  { // Sweet
    "Task due now, dear {name}: '{detail}'. You can do it!",
    "Reminder: '{detail}' is scheduled for this hour, {name}.",
    "Hour deadline: don't forget to complete '{detail}', {name}.",
    "Nudge: check off '{detail}' from today's list, {name}.",
    "Check-in, {name}: time to work on your sweet task '{detail}'."
  },
  { // Friend
    "Check-in: is '{detail}' completed yet, {name}?",
    "Time for '{detail}', {name}. Check it off when you're done.",
    "Nudge: '{detail}' is scheduled for now. Settle in.",
    "Task due: '{detail}'. Check your TODO list when complete.",
    "Reminder, {name}: '{detail}' is waiting for completion."
  }
};

static const char* PROMPT_NAGGING =
  "{name} has overdue tasks. The next one in line is '{detail}' — reference it by name. "
  "Other overdue daily and monthly tasks may appear in the observations; you are allowed to cite them too. "
  "One pointed nudge in your persona's voice. "
  "React to what the task IS — its content, its nature, its consequences. "
  "BANNED: 'Nudge:', 'Procrastination alert!', 'Check your panel', generic urgency. "
  "Make the overdue nature feel like a person noticing, not an app. Vary the angle each time. Under 90 characters.";

static const char* PROMPT_TASK_DUE =
  "Task '{detail}' is due now. Notify {name} with the time-based fact first, then your persona's reaction. "
  "Vary the emphasis: clock, consequence, or readiness — never the same framing twice. Under 90 characters.";

#endif // BEHAVIOUR_H

