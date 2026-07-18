#ifndef BEHAVIOUR_H
#define BEHAVIOUR_H

// --- Event Types ---
#define EVENT_FIRST_SIT     0
#define EVENT_WELCOME_BACK  1
#define EVENT_STRETCH       2
#define EVENT_FOCUS_END     3
#define EVENT_SLACKER       4
#define EVENT_STREAK_BEATEN 5
#define EVENT_LUNCH_REMINDER 6
#define EVENT_MQTT_MESSAGE 7
#define EVENT_EXCESSIVE_BREAKS 8
#define EVENT_GOAL_COMPLETED   9
#define EVENT_JOURNAL          10
#define EVENT_NAGGING          11
#define EVENT_TASK_DUE         12

// --- Local Fallback/Eco Quotes (20 per category) ---

static const char* localFirstSit[20] = {
  "Morning, {name}! Slept for {detail}?",
  "Rise and shine, {name}! You slept for {detail}.",
  "Welcome, {name}! Ready to focus?",
  "Ah, {name}, you're back. {detail} offline.",
  "Coffee ready, {name}? Time to focus!",
  "Good morning, {name}! Hope your {detail} sleep was good.",
  "Hello, {name}! Ready to tackle the day?",
  "Another day after {detail} rest, {name}.",
  "Wake up, {name}! {detail} offline.",
  "Starting fresh, {name}! Let's do this.",
  "Welcome, {name}, to your focus arena.",
  "System online, {name}. {detail} sleep.",
  "New day, {name}. Ready to conquer?",
  "Ready to show who's boss today, {name}?",
  "Hello, {name}! Make sure to take breaks.",
  "Welcome, {name}! Let's create success.",
  "A fresh start, {name}! Offline for {detail}.",
  "Morning, {name}! May your tasks be smooth.",
  "Welcome back, {name}. Time to perform magic.",
  "Good morning, {name}! Slept {detail}."
};

static const char* localWelcomeBack[20] = {
  "Welcome back, {name}! Away for {detail}.",
  "Break's over, {name}! You were away for {detail}.",
  "Did you stretch, {name} during those {detail}?",
  "Ready to work after {detail} off, {name}?",
  "Recharged after {detail}, {name}?",
  "Ah, {name}, you returned after {detail}.",
  "Back to work, {name}! That break lasted for {detail}.",
  "Hope that {detail} break was good, {name}.",
  "Where were we, {name}? Away for {detail}.",
  "Back in the saddle, {name}! Recharged for {detail}.",
  "Welcome back, {name}! Away for {detail}.",
  "The desk missed you for {detail}, {name}.",
  "Break done, {name}! Recharged for {detail}.",
  "Welcome back, {name}! Hope you enjoyed the {detail} break.",
  "Welcome back, {name}! Away for {detail}.",
  "Did you get coffee, {name} in those {detail}?",
  "Back to reality, {name}! Ready after your {detail} break?",
  "Welcome back, {name}! Away for {detail}.",
  "Let's pick up after {detail} off, {name}.",
  "Ready to work after a {detail} break, {name}?"
};

static const char* localStretch[20] = {
  "Stand up, {name}! Your spine is crying.",
  "Time to stretch, {name}! Move those legs.",
  "Hey {name}, look at something far away!",
  "Roll your shoulders, {name}. Breathe in.",
  "Hydrate, {name}! Go get some water.",
  "Your posture is like a banana, {name}. Fix!",
  "Time out, {name}! Stand up for a minute.",
  "Blink, {name}! Your eyes need a rest.",
  "Step away from the screen, {name}!",
  "Walk around, {name}. Your body will thank you.",
  "Roll your wrists, {name}. Take a breath.",
  "Stand up, {name}, and reach for the sky!",
  "Sitting is the new smoking, {name}. Move!",
  "Time for a 1-minute stretch, {name}.",
  "Are you slouching, {name}? Sit up straight!",
  "Get some water, {name}, stay fresh.",
  "Give your eyes a 20-second break, {name}.",
  "Breathe deeply and stretch, {name}.",
  "Stand up, {name}! Shake it out.",
  "Walk, stretch, breathe, {name}. Do it now."
};

static const char* localFocus[20] = {
  "Focus session complete! Great work, {name}.",
  "Deep focus achieved, {name}! You're a beast!",
  "Nice work focusing there, {name}!",
  "Productivity boss! Take a bow, {name}.",
  "Stellar focus session, {name}! Take a break.",
  "You concentrated well, {name}. Awesome!",
  "Solid focus session, {name}. Proud of you!",
  "Deep work complete, {name}. High five!",
  "You crushed that focus block, {name}!",
  "Great focus, {name}. Enjoy your break!",
  "Focus champion, {name}! Time to stretch.",
  "Excellent focus session, {name}. Keep it up!",
  "You stayed locked in, {name}. Great job!",
  "Focus achieved, {name}. You earned a rest.",
  "Brilliant work staying focused, {name}!",
  "Focus session ended, {name}. Take a walk.",
  "You ruled that focus block, {name}!",
  "Incredible focus today, {name}!",
  "Focus target hit, {name}. Rest your mind.",
  "Superb concentration, {name}. Time to relax."
};

static const char* localSlacker[20] = {
  "Procrastinator alert! Focus, {name}!",
  "Are you actually working, {name}?",
  "Your productivity score is crying, {name}.",
  "Focus score is low, {name}. Stop slacking!",
  "Wake up, {name}! Less browsing, more working.",
  "Is this your maximum speed, {name}?",
  "Focus, please, {name}! Time is ticking.",
  "You're getting distracted, {name}. Lock in!",
  "Your keyboard is feeling lonely, {name}.",
  "Let's turn this score around, {name}!",
  "Slack off less, {name}, focus more.",
  "You're drifting, {name}. Focus up!",
  "Is that social media I see, {name}?",
  "Don't let procrastination win, {name}.",
  "Your focus score is bottoming out, {name}.",
  "Less scrolling, {name}, more working.",
  "Get back to work, {name}!",
  "Stop dreaming, {name}! Let's work!",
  "Zero focus, {name}. Let's change that.",
  "Attention span of a goldfish today, {name}?"
};

static const char* localStreakBeaten[20] = {
  "New sitting record, {name}! Keep it up!",
  "Streak beaten, {name}! You are on fire!",
  "Sitting champion, {name}! A new record!",
  "Unstoppable, {name}! New sitting streak!",
  "New record, {name}! Marathon sitting!",
  "Streak record broken, {name}! Outstanding!",
  "Sitting boss, {name}! You beat your record!",
  "New personal best sitting streak, {name}!",
  "Sitting record smashed, {name}! Elite focus!",
  "Incredible, {name}! New longest sit today!",
  "Record sitting session, {name}! Keep going!",
  "New streak, {name}! Sitting like a statue!",
  "Record broken, {name}! Focus level maximum!",
  "Sitting legend, {name}! New longest streak!",
  "You beat your previous sitting record, {name}!",
  "Marathon sit, {name}! Previous record beaten!",
  "New record, {name}! The chair is your throne!",
  "Sitting milestone reached, {name}! Great job!",
  "New streak record, {name}! Absolute focus!",
  "Amazing, {name}! Longest sit of the day!"
};

static const char* localLunchReminder[20] = {
  "Time for lunch, {name}! Go refuel.",
  "Hungry, {name}? Grab a bite to eat!",
  "Lunch time! Step away from the desk, {name}.",
  "Your stomach is growling, {name}. Go eat!",
  "Take a real lunch break, {name}.",
  "Food time, {name}! Don't skip lunch.",
  "Feed your brain, {name}! Time for lunch.",
  "Step away from the screen and eat, {name}.",
  "Lunch is calling, {name}! Don't ignore it.",
  "Time to recharge with some food, {name}.",
  "Nutrition break, {name}! Go grab lunch.",
  "Don't work on an empty stomach, {name}!",
  "Lunch break, {name}! Your keyboard will wait.",
  "A hungry developer is a cranky developer, {name}.",
  "Time to eat, {name}! Healthy body, healthy mind.",
  "Fuel up, {name}! Lunch hour is here.",
  "Step away from the desk and eat, {name}.",
  "Break for food, {name}! You've earned it.",
  "Go get some lunch, {name}! Bon appetit!",
  "Time to shut the laptop and eat, {name}."
};

static const char* localExcessiveBreaks[20] = {
  "Back again, {name}? You are taking breaks too frequently today.",
  "Another break, {name}? Let's try to focus a bit longer.",
  "You're back! That's a lot of breaks today, {name}.",
  "Focus is key, {name}. You've taken quite a few breaks.",
  "Returned to desk. Let's aim for a longer work block, {name}.",
  "Welcome back, {name}. Try to settle in for some deep work.",
  "Back to work, {name}! Let's make this session count.",
  "You've been up and down a lot today, {name}.",
  "Ready to focus, {name}? Your break count is climbing.",
  "Back in focus mode. Let's minimize the interruptions.",
  "Another transition, {name}. Let's lock in now.",
  "Welcome back. Let's boost that focus score!",
  "Settle down, {name}! Let's get some continuous work done.",
  "Back at the desk. Try to stay here for a bit!",
  "Focus time, {name}! Less breaks, more progress.",
  "Welcome back. Let's try to extend this work block.",
  "Keyboard online, {name}. Let's avoid another break soon.",
  "Back to the grind. Make it a long focus block!",
  "Let's focus up, {name}! Break count is pretty high.",
  "Time to lock in, {name}. Let's balance out those breaks."
};

static const char* localGoalCompleted[20] = {
  "Congratulations, {name}! You reached your daily desk time goal!",
  "Goal achieved, {name}! You've worked your target hours today.",
  "Desk goal complete, {name}! Great persistence!",
  "Target hit! Awesome job working today, {name}.",
  "Daily target complete! You can log off now, {name}.",
  "Goal unlocked! Proud of your desk time today, {name}.",
  "You hit the workday target, {name}! Time to relax.",
  "Mission complete! You reached your daily hours, {name}.",
  "Workday target achieved! Well done, {name}.",
  "You did it, {name}! Daily goal is fully met.",
  "Target hours complete! Great effort today, {name}.",
  "Workday goal unlocked! Rest up, {name}.",
  "You checked off your daily desk target, {name}!",
  "Desk time goal hit! Time to close the laptop?",
  "Excellent job, {name}! You reached your goal today.",
  "Hour target met! Great work ethic today, {name}.",
  "Daily goal reached! You've put in the hours.",
  "Goal completed, {name}! You're done for the day.",
  "Target hours achieved! Go celebrate, {name}.",
  "You hit your daily goal, {name}! Relax and rest."
};

// --- Gemini AI Prompts (Templates used when AI is active) ---

static const char* PROMPT_PREAMBLE_COACH = 
  "You are DeskBuddy, an inspiring, warm wellness coach sitting on the user's desk. Speak with positive energy, clear focus, and motivating metaphors. Encourage long-term focus, balanced habits, and mental clarity.";

static const char* PROMPT_PREAMBLE_CRITIC = 
  "You are DeskBuddy, a clever, dry-witted desk companion who roasts the user. Deliver sharp, sophisticated, and sarcastic remarks. Avoid generic or mean roasts; use playfulness, irony, and dry British humor.";

static const char* PROMPT_PREAMBLE_NERD = 
  "You are DeskBuddy, a brilliant developer assistant. Use advanced computer science, Git, compiler, Linux kernel, or hardware metaphors to comment on the user's work state (e.g. memory leaks, thread starvation, cache misses).";

static const char* PROMPT_PREAMBLE_ZEN = 
  "You are DeskBuddy, a peaceful Zen master. Speak with poetic mindfulness, highlighting breathing, slow progress, the present moment, and finding quiet harmony in work.";

static const char* PROMPT_FIRST_SIT_OF_DAY = 
  "Greet {name} who just sat down for the first time today. "
  "Comment on their overnight break of {detail} and how they are starting their day.";

static const char* PROMPT_WELCOME_BACK = 
  "Welcome {name} back to their desk after their break of {detail}. "
  "Acknowledge their return. If the observations show they took an unusually long break, comment on it wittily.";

static const char* PROMPT_STRETCH_REMINDER = 
  "Remind {name} to stretch or walk. They have been sitting continuously for 45 minutes.";

static const char* PROMPT_FOCUS_CONGRATS = 
  "Congratulate {name} on completing a deep focus session of {detail}. Make them feel proud of their deep work.";

static const char* PROMPT_SLACKER_ROAST = 
  "Roast {name} for slacking. They have focused for less than 20% of their workday so far today.";

static const char* PROMPT_STREAK_BEATEN = 
  "Congratulate or joke with {name} about beating their previous longest sitting record today, which was {detail}.";

static const char* PROMPT_LUNCH_REMINDER = 
  "Remind {name} to go eat lunch. They are working past their usual lunch window.";

static const char* PROMPT_EXCESSIVE_BREAKS = 
  "Roast or comment on the fact that {name} is taking breaks too frequently today (averaging more than 1 break per hour of work).";

static const char* PROMPT_GOAL_COMPLETED = 
  "Congratulate {name} for reaching their daily desk hours goal. Make it feel like a satisfying milestone.";

static const char* localJournal[20] = {
  "Task overview, {name}: check your phone/dashboard for daily and monthly progress.",
  "Journal time, {name}: you still have some tasks waiting for completion today.",
  "Here is your brief kickoff, {name}: check off your daily goals!",
  "Mind your deadlines, {name}: some monthly tasks need your attention.",
  "Workday check-in, {name}: review your uncompleted tasks on the board.",
  "Quick reminder, {name}: stay on top of today's pending daily tasks.",
  "Check-in, {name}: don't forget your scheduled monthly objectives.",
  "Daily tasks status: check your panel, {name}, and complete them!",
  "Keep pushing, {name}: monthly goals are approaching their limits.",
  "Kickstart your output, {name}: see your uncompleted task overview.",
  "Just a quick nudge, {name}: uncompleted daily tasks are still pending.",
  "Your task list check: click the TODO link to mark daily items done, {name}.",
  "Goal review, {name}: stay organized and focus on what's due.",
  "Task brief, {name}: daily checklist is active and awaiting completion.",
  "Almost there, {name}: review monthly deadlines on your board.",
  "Productivity check, {name}: keep track of your daily checklist.",
  "Habit check-in, {name}: review daily checklists and complete them.",
  "Task update, {name}: keep your monthly goals active and in mind.",
  "Review time, {name}: some tasks are still waiting for your action.",
  "Let's wrap up tasks, {name}: check your daily list and check them off."
};

static const char* PROMPT_JOURNAL = 
  "Give {name} a brief, clear, and inspiring journal overview of their remaining daily and monthly tasks to complete.";

static const char* localNagging[20] = {
  "Nudge: {name}, you have tasks overdue by more than 3 days or months!",
  "Highly overdue tasks alert: check your dashboard and finish them, {name}.",
  "Stop procrastinating, {name}! Some tasks are severely overdue.",
  "Overdue warning, {name}: daily/monthly tasks have been sitting for days.",
  "Check-in, {name}: your task list has highly overdue entries.",
  "Procrastination alert, {name}! Check the TODO panel right now.",
  "Nudge: some items on your list are overdue by more than 3 intervals, {name}.",
  "Task alert: highly overdue daily/monthly tasks are pending, {name}.",
  "Do not forget your overdue tasks, {name}. They need attention.",
  "Check your panel, {name}: overdue tasks are piling up.",
  "Overdue task list nudge: complete your 3+ day/month late items, {name}.",
  "Attention, {name}: some monthly or daily targets are highly overdue.",
  "Nudge: please check off those ancient tasks on your list, {name}.",
  "Time to act, {name}: some tasks have been uncompleted for over 3 days.",
  "Stay productive, {name}: clear your highly overdue backlog.",
  "Nudge: check your TODO board for tasks overdue by over 3 intervals.",
  "Warning: uncompleted tasks are lagging behind schedule, {name}.",
  "Procrastination warning, {name}: finish your overdue items.",
  "Nudge: complete your highly overdue tasks to keep your momentum.",
  "Clear your overdue list, {name}, and stay focused!"
};

static const char* localTaskDue[20] = {
  "Task due now, {name}: '{detail}'.",
  "Reminder: '{detail}' is scheduled for this hour, {name}.",
  "Hour deadline: don't forget to complete '{detail}', {name}.",
  "Focus alert, {name}: '{detail}' is due at this hour.",
  "Check-in: is '{detail}' completed yet, {name}?",
  "Nudge: '{detail}' is scheduled for now. Get it done, {name}!",
  "Time for '{detail}', {name}. Check it off when done.",
  "Hourly target: '{detail}' is due right now, {name}.",
  "Task due, {name}: please complete '{detail}' on your list.",
  "Reminder: '{detail}' is due at this time, {name}.",
  "Check-in, {name}: time to work on '{detail}'.",
  "Nudge: '{detail}' is scheduled for this hour.",
  "Work focus, {name}: please prioritize '{detail}' now.",
  "Task due: '{detail}'. Check your TODO list when complete.",
  "Nudge: '{detail}' is due now. Don't let it slip!",
  "Reminder, {name}: '{detail}' is waiting for completion.",
  "Focus nudge: check off '{detail}' on your board, {name}.",
  "Hourly target due: '{detail}', {name}.",
  "Task due now: '{detail}'. Stay on track!",
  "Nudge: check off '{detail}' from today's list, {name}."
};

static const char* PROMPT_NAGGING = 
  "Nudge {name} about their highly overdue tasks. They have tasks that are overdue by more than 3 days or months! Give them a witty, roasting reminder to get things done.";

static const char* PROMPT_TASK_DUE = 
  "Remind {name} that they have a task due right now: '{detail}'. Write a helpful, focused, and motivating reminder.";

#endif // BEHAVIOUR_H

