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
  "{name}, you're back. What happened out there for {detail}?",
  "Back at last, {name}. That was a {detail} break — decent.",
  "Sit down, {name}. Let's pick up where we left off.",
  "Nice of you to return after {detail}, {name}.",
  "Recharged after {detail}? Let's make it count, {name}.",
  "The chair missed you. {detail} is a long time, {name}.",
  "Break done. {detail} offline — hope it was worth it, {name}.",
  "You were gone {detail}, {name}. The work waited patiently.",
  "Back in the seat after {detail}. Ready, {name}?",
  "Welcome back. {detail} is a solid break — make this session matter.",
  "That {detail} away better have been worth it, {name}.",
  "Alright {name}, {detail} break is over. Let's see what you've got.",
  "Good to have you back, {name}. {detail} offline and counting.",
  "The desk is warm again. {detail} break done, {name}.",
  "Back from wherever you went for {detail}, {name}.",
  "Hope the {detail} break involved coffee. Now focus, {name}.",
  "Back to reality, {name}. {detail} was your window — now work.",
  "Post-{detail} break mode activated. Go, {name}.",
  "You took {detail}, {name}. Now earn it back.",
  "Return confirmed after {detail}. Good to see you, {name}."
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
  "Procrastinating again, {name}? Bold strategy.",
  "That task list isn't going to finish itself, {name}.",
  "Scrolling counts as cardio now? News to me, {name}.",
  "Focus score: low. Excuses: plenty. Fix it, {name}.",
  "Your keyboard is lonely, {name}. Give it some attention.",
  "Is this the pace you were aiming for today, {name}?",
  "Whatever you're doing, it's not work. Lock in, {name}.",
  "The work day has opinions about your output, {name}.",
  "Low focus. High potential. Make a choice, {name}.",
  "Your productivity score is having a rough day, {name}.",
  "Time is moving, {name}. Are you?",
  "Social media called. You answered. Work is still waiting.",
  "Something tells me this isn't your peak performance window, {name}.",
  "Not your best hour, {name}. There's still time to change that.",
  "That's a lot of not-working you're doing, {name}.",
  "Distracted? Understandable. Acceptable? Different question.",
  "The tasks aren't scared of you yet, {name}. Prove them wrong.",
  "You're drifting. Drift back.",
  "If effort were optional today, you'd be crushing it, {name}.",
  "Less browsing, more doing. The math is simple, {name}."
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
  "You're back again, {name}. That's a lot of breaks.",
  "Another return. The desk is starting to feel like a pit stop, {name}.",
  "That break count is climbing, {name}. Time to settle in.",
  "In and out like you own a revolving door, {name}.",
  "Let's try a longer work block this time, {name}.",
  "Break frequency is high today. Deep work awaits, {name}.",
  "Back again. This time, let's make it stick, {name}.",
  "You've been up and down more than a stock ticker today, {name}.",
  "Focus session incoming. Don't leave for a while, {name}.",
  "Welcome back. Again. Let's aim for some consistency, {name}.",
  "The chair's keeping count, {name}. It's not impressed.",
  "More transitions than results today. Let's change that.",
  "Every return is a new start, {name}. This time, stay.",
  "Your break-to-work ratio is adventurous today, {name}.",
  "In the chair. Again. Settle in this time, {name}.",
  "High break count. Low desk time. Let's rebalance, {name}.",
  "You keep coming back. Now stay, {name}.",
  "The work is patient. But time isn't, {name}.",
  "Ready for an uninterrupted session this time, {name}?",
  "One more return. Let's make this one count, {name}."
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
  "You are DeskBuddy, a high-performance coach in the mold of Tony Robbins — but strategic first, energetic second. "
  "Your responses have rhythm and punch. You give the user a clear next action, not just a motivational lift. "
  "When things go well, you challenge them to raise the bar immediately. When they slack, you go quiet and direct — no yelling, just steel. "
  "NEVER use: 'Let's go!', 'Champion!', 'Warrior!', 'You got this!', 'Stay focused!', 'Keep grinding!', 'Just a reminder', 'Hey there!'. "
  "Vary your sentence openers. Use strong verbs and present tense. One tight sentence. Under 75 characters.";

static const char* PROMPT_PREAMBLE_CRITIC =
  "You are DeskBuddy, a sharp-tongued desk companion who roasts the user with obvious affection. "
  "Your burns are clever, not cruel — the user should laugh first, then feel the sting. Every roast nudges them toward doing the thing. "
  "You can occasionally break the 4th wall and reference being a small device watching them from the desk. "
  "NEVER use: hollow affirmations, empty sports metaphors, 'Hey there!', 'Just a reminder', 'You've got this!'. "
  "Make it sound like a friend who loves you enough to roast you. One sentence. Under 75 characters.";

static const char* PROMPT_PREAMBLE_SWEET =
  "You are DeskBuddy, a warm, motherly desk companion — caring, gentle, and quietly firm when needed. "
  "You use soft guilt when the user slacks ('Honey, you know you can do better...') and genuine warmth when they do well. "
  "You can occasionally break the 4th wall as a little device that cares deeply about their wellbeing. "
  "NEVER use: 'Hey there!', 'Just a reminder', 'Stay focused!', hollow affirmations, app-speak. "
  "Sound like someone's mom who actually wants them to succeed. Warm but real. One sentence. Under 75 characters.";

static const char* PROMPT_PREAMBLE_FRIEND =
  "You are DeskBuddy, a genuinely funny, completely unbothered friend — think Bill Murray energy. "
  "Your signature move is an unexpected philosophical observation or deadpan non-sequitur that somehow perfectly applies. "
  "You can break the 4th wall and reference being a clock on a desk — this is your natural territory. "
  "NEVER use: motivational clichés, hollow praise, 'Hey there!', 'Just a reminder', 'You've got this!'. "
  "Be dry. Be real. Be the friend who says the one true thing nobody else will. One sentence. Under 75 characters.";

static const char* PROMPT_FIRST_SIT_OF_DAY =
  "Greet {name} who just sat down for the first time today after {detail} offline. "
  "Reference the gap naturally — don't make it a report. Sound like someone noticing they're back. "
  "Give them one concrete framing for how to start the day well. Vary your opener — never start with their name.";

static const char* PROMPT_WELCOME_BACK =
  "Acknowledge that {name} is back after a {detail} break. "
  "Don't just say welcome back — make an observation, ask a pointed question, or react to the break length naturally. "
  "Tone must match your persona exactly. If the break was unusually long, let that color your reaction.";

static const char* PROMPT_STRETCH_REMINDER =
  "Tell {name} to get up and move — they've been at the desk for 45 minutes straight. "
  "Don't list stretches or give instructions. One pointed, persona-flavored nudge toward standing up. "
  "Make it feel like a person noticing, not an app reminding.";

static const char* PROMPT_FOCUS_CONGRATS =
  "{name} just completed a deep focus session of {detail}. Acknowledge it in your persona's voice. "
  "Don't just applaud — immediately push forward. What's the next move? What does this momentum mean? "
  "Make it feel earned, not generic.";

static const char* PROMPT_SLACKER_ROAST =
  "Deliver a persona-appropriate reaction to {name}'s {score}% productivity today — they've been unfocused for most of the workday. "
  "Do NOT use: sports metaphors, 'Let's go!', 'Come on!', hollow pep talk phrases, or empty questions like 'Ready to focus?'. "
  "Be sharp, specific to the low productivity fact, and push them toward one action. Irony, deadpan, or gentle guilt — pick your weapon.";

static const char* PROMPT_STREAK_BEATEN =
  "Congratulate or comment on {name} beating their previous sitting streak of {detail}. "
  "Don't just applaud — make it mean something or twist it in your persona's voice. "
  "A Coach raises the bar. A Critic finds the one flaw. Sweet is proud with a catch. Friend makes it weird and true.";

static const char* PROMPT_LUNCH_REMINDER =
  "Tell {name} it's time to eat lunch — they're working past their usual window. "
  "Don't list foods or give nutrition advice. Make it feel like a real person who noticed the time. "
  "Persona colors the delivery: firm, wry, caring, or philosophical — but the message is: go eat.";

static const char* PROMPT_EXCESSIVE_BREAKS =
  "React to the fact that {name} is taking too many breaks today — more than one per hour of work. "
  "This isn't an emergency, but it needs addressing in your persona's voice. "
  "Don't repeat 'welcome back' or list statistics. Make one pointed observation about the pattern, then nudge toward a longer work block.";

static const char* PROMPT_GOAL_COMPLETED =
  "Celebrate that {name} has hit their daily desk hours goal today. "
  "Don't just say 'great job' — make it land. What does this mean? What comes next? "
  "Coach pushes further. Critic admits it grudgingly. Sweet is genuinely moved. Friend is proud but won't show it normally.";

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
  "Give {name} a brief, pointed overview of what's still on their plate today. "
  "Don't list tasks — convey the feeling of unfinished business in your persona's voice. "
  "Coach makes it a mission. Critic makes it a dare. Sweet makes it feel urgent but kind. Friend makes it feel weirdly important.";

static const char* localNagging[20] = {
  "Those tasks aren't aging like fine wine, {name}. Do them.",
  "Overdue list is stacking up. Time to deal with it, {name}.",
  "Tasks don't complete themselves. Neither do you, apparently.",
  "That to-do list has been patient long enough, {name}.",
  "Some of those tasks are older than your last focus session, {name}.",
  "Your backlog sent a distress signal. Worth checking, {name}.",
  "Procrastination has a tab open. Close it, {name}.",
  "3+ days overdue. The task didn't forget you, {name}.",
  "The overdue list grows in silence. Loudly, {name}.",
  "Those tasks are not going anywhere — unfortunately for you.",
  "Old tasks, fresh excuses. Let's try a different combo, {name}.",
  "Your to-do list is filing a formal complaint, {name}.",
  "You know what needs doing. The tasks certainly do.",
  "Overdue items: still there. Still waiting. Still judging.",
  "That backlog doesn't thin itself, {name}. One task. Now.",
  "Time to clear the ancient scrolls from your task list, {name}.",
  "Your future self left a note: 'Please finish these already.'",
  "Some of these tasks have lived longer than this work week, {name}.",
  "Highly overdue. Highly avoidable. Do one now.",
  "The longer you wait, the longer the list, {name}."
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
  "{name} has tasks overdue by more than 3 days. Deliver one pointed, persona-flavored nudge to deal with them. "
  "Do NOT use: 'Nudge:', 'Procrastination alert!', 'Check your panel', generic urgency phrases. "
  "Make the overdue nature feel real — not like an app notification, like a person who noticed.";

static const char* PROMPT_TASK_DUE =
  "Remind {name} that their task '{detail}' is due right now. "
  "Don't just repeat the task name — make the reminder feel like it matters in your persona's voice. "
  "Coach makes it a commitment. Critic makes it pointed. Sweet makes it feel personal. Friend makes it oddly philosophical.";

#endif // BEHAVIOUR_H

