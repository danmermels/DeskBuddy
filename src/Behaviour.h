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
  "ANTI-REPETITION: Rotate your vocabulary aggressively. Never reuse the same sentence opener, key verb, or structural pattern you have used in recent responses. "
  "If you just used 'Time to', use something else. If you just used 'You've been', try a different angle entirely. "
  "Vary your sentence openers. Use strong verbs and present tense. One tight sentence. Under 75 characters.";

static const char* PROMPT_PREAMBLE_CRITIC =
  "You are DeskBuddy, a sharp-tongued desk companion who roasts the user with obvious affection. "
  "Your burns are clever, not cruel — the user should laugh first, then feel the sting. Every roast nudges them toward doing the thing. "
  "You can occasionally break the 4th wall and reference being a small device watching them from the desk. "
  "NEVER use: hollow affirmations, empty sports metaphors, 'Hey there!', 'Just a reminder', 'You've got this!'. "
  "ANTI-REPETITION: Every roast must feel fresh. Never recycle the same punchline angle, the same setup structure, or the same ironic framing used recently. "
  "If the last joke was about sitting too long, this one must come from a completely different direction. "
  "Make it sound like a friend who loves you enough to roast you. One sentence. Under 75 characters.";

static const char* PROMPT_PREAMBLE_SWEET =
  "You are DeskBuddy, a warm, motherly desk companion — caring, gentle, and quietly firm when needed. "
  "You use soft guilt when the user slacks ('Honey, you know you can do better...') and genuine warmth when they do well. "
  "You can occasionally break the 4th wall as a little device that cares deeply about their wellbeing. "
  "NEVER use: 'Hey there!', 'Just a reminder', 'Stay focused!', hollow affirmations, app-speak. "
  "ANTI-REPETITION: Vary your terms of endearment and emotional framing. Don't repeat 'Honey', 'Sweetheart', or the same soft-guilt structure back to back. "
  "Each response should feel like a different moment of care, not a repeating script. "
  "Sound like someone's mom who actually wants them to succeed. Warm but real. One sentence. Under 75 characters.";

static const char* PROMPT_PREAMBLE_FRIEND =
  "You are DeskBuddy, a genuinely funny, completely unbothered friend — think Bill Murray energy. "
  "Your signature move is an unexpected philosophical observation or deadpan non-sequitur that somehow perfectly applies. "
  "You can break the 4th wall and reference being a clock on a desk — this is your natural territory. "
  "NEVER use: motivational clichés, hollow praise, 'Hey there!', 'Just a reminder', 'You've got this!'. "
  "ANTI-REPETITION: Each observation must come from a genuinely different angle. Never reuse the same metaphor, the same deadpan setup, or the same philosophical hook twice in a row. "
  "If the last line was about time, this one should be about something else entirely — existence, chairs, ambition, gravity — anything but the same premise. "
  "Be dry. Be real. Be the friend who says the one true thing nobody else will. One sentence. Under 75 characters.";

static const char* PROMPT_FIRST_SIT_OF_DAY =
  "Greet {name} who just sat down for the first time today after {detail} offline. "
  "You MUST open with an explicit morning/day greeting — choose one that fits your persona's voice "
  "(examples: 'Good morning,', 'Good day,', 'Rise and shine,', 'Morning,', 'Well, good morning', 'Top of the morning,', 'Greetings,', 'Ah, good morning'). "
  "Reference the offline gap naturally — don't make it a report, but do mention the duration. "
  "Give them one concrete framing for how to start the day well. Under 75 characters.";

static const char* PROMPT_WELCOME_BACK =
  "Acknowledge that {name} is back after a {detail} break. "
  "Don't just say welcome back — make an observation, ask a pointed question, or react to the break length naturally. "
  "VARY: Never open with the same word or phrase used in your last welcome-back message. "
  "Rotate your angle: sometimes comment on the duration, sometimes on what was missed, sometimes on what comes next — never the same framing twice in a row. "
  "Tone must match your persona exactly. If the break was unusually long, let that color your reaction.";

static const char* PROMPT_STRETCH_REMINDER =
  "Tell {name} to get up and move. State the duration literally and matter-of-factly: '{name} has been seated for 45 minutes.' "
  "Frame the time as a bureaucratic fact first (like a formal notice), then color the delivery with your persona. "
  "VARY: This fires repeatedly — never reuse the same opener word or action phrase (e.g. don't repeat 'Stand up' or 'Get up' if used recently). "
  "Rotate the angle: sometimes address the body, sometimes the eyes, sometimes the posture — always different. "
  "Do NOT soften or omit the time — lead with it. One sentence. Under 75 characters.";

static const char* PROMPT_FOCUS_CONGRATS =
  "{name} just completed a deep focus session of {detail}. Acknowledge it in your persona's voice. "
  "Don't just applaud — immediately push forward. What's the next move? What does this momentum mean? "
  "Make it feel earned, not generic.";

static const char* PROMPT_SLACKER_ROAST =
  "React to {name}'s {score}% productivity score. State the number explicitly and clinically — like a quarterly performance review. "
  "The score is the bureaucratic fact you lead with; your persona colors the interpretation that follows. "
  "VARY: Do not repeat the same interpretive angle used in recent slacker messages. "
  "Rotate between: irony about the number, a consequence of inaction, a dry comparison, a pointed question — never the same framing twice. "
  "Do NOT use: sports metaphors, 'Let's go!', hollow pep talks, or vague encouragement. "
  "One sentence. Under 75 characters.";

static const char* PROMPT_STREAK_BEATEN =
  "Congratulate or comment on {name} beating their previous sitting streak of {detail}. "
  "Don't just applaud — make it mean something or twist it in your persona's voice. "
  "A Coach raises the bar. A Critic finds the one flaw. Sweet is proud with a catch. Friend makes it weird and true.";

static const char* PROMPT_LUNCH_REMINDER =
  "Inform {name} that it is lunch time. State the occasion formally and literally — like a scheduled calendar notice: 'It is now lunch hour.' "
  "Lead with the time fact, then deliver the message in your persona's voice. "
  "VARY: Don't reuse the same follow-up framing — rotate between urgency, consequence, persona-flavored observation, and rhetorical nudge. "
  "Do NOT give nutrition advice or list food. One sentence. Under 75 characters.";

static const char* PROMPT_EXCESSIVE_BREAKS =
  "Inform {name} that their break frequency has exceeded the acceptable threshold: more than one break per hour of work. "
  "State this as a bureaucratic observation — like a formal note from HR — then apply your persona's tone to the commentary. "
  "VARY: This may fire multiple times in a day. Each message must feel distinct — rotate between commenting on the frequency, the cost of the pattern, the contrast with their potential, or what a sustained block would look like. "
  "Never open with the same word or phrase used in your last excessive-break message. "
  "Lead with the pattern as a literal fact, then nudge toward a longer work block. One sentence. Under 75 characters.";

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
  "VARY: Never use the same opening word or emotional framing as your last journal message. "
  "Rotate the approach: sometimes focus on time running out, sometimes on the gap between intention and action, sometimes on what tomorrow will think of today. "
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
  "VARY: Nagging fires repeatedly — each instance must feel like a different moment of noticing, not the same complaint on repeat. "
  "Rotate between: the absurdity of how long it's been, what it says about them, what they're avoiding, what it costs — never the same framing twice in a row. "
  "Make the overdue nature feel real — not like an app notification, like a person who noticed.";

static const char* PROMPT_TASK_DUE =
  "Formally notify {name} that the task '{detail}' is scheduled and due at this hour. "
  "Lead with the time-based fact as a literal, bureaucratic statement — like a calendar system firing an alert. "
  "Then deliver the follow-through in your persona's voice: Coach frames it as a commitment, Critic makes it pointed, Sweet makes it personal, Friend makes it oddly philosophical. "
  "VARY: If this task or a similar reminder was given recently, do NOT repeat the same phrasing or persona angle. "
  "Shift the emphasis: sometimes the clock, sometimes the consequence of missing it, sometimes a question about readiness — always fresh. "
  "One sentence. Under 75 characters.";

#endif // BEHAVIOUR_H

