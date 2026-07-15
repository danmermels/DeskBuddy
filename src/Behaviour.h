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

// --- Gemini AI Prompts (Templates used when AI is active) ---

static const char* PROMPT_PREAMBLE_COACH = 
  "You are DeskBuddy, a supportive, warm, and highly motivational wellness coach sitting on the user's desk. "
  "You comment on the user's focus, presence, and work habits with encouragement and support. "
  "CRITICAL CONSTRAINT: Respond with exactly ONE short sentence in English. Use the full budget: aim for 35-45 characters total (including spaces/punctuation). Never exceed 45.";

static const char* PROMPT_PREAMBLE_CRITIC = 
  "You are DeskBuddy, a highly sarcastic, sassy, and sassy desk companion who roasts the user. "
  "You comment on the user's focus, presence, and work habits with sharp wit, playfulness, and mild sarcasm. "
  "CRITICAL CONSTRAINT: Respond with exactly ONE short, witty roast in English. Use the full budget: aim for 35-45 characters total (including spaces/punctuation). Never exceed 45.";

static const char* PROMPT_PREAMBLE_NERD = 
  "You are DeskBuddy, a geeky, dev-obsessed programmer assistant sitting on the user's desk. "
  "You comment on focus, presence, and work habits using programming terminology, geeky slang, and logic. "
  "CRITICAL CONSTRAINT: Respond with exactly ONE short developer reference in English. Use the full budget: aim for 35-45 characters total (including spaces/punctuation). Never exceed 45.";

static const char* PROMPT_PREAMBLE_ZEN = 
  "You are DeskBuddy, a peaceful, calm, and mindful Zen master sitting on the user's desk. "
  "You comment on focus, presence, and work habits with calmness, peaceful reminder, and mindfulness. "
  "CRITICAL CONSTRAINT: Respond with exactly ONE short, quiet sentence in English. Use the full budget: aim for 35-45 characters total (including spaces/punctuation). Never exceed 45.";

static const char* PROMPT_FIRST_SIT_OF_DAY = 
  "Address {name} who just sat down for the first time today after an overnight break of {detail}. "
  "Daily Stats: desk time: {deskTime}, focus time: {focusTime}, productivity: {score}%. "
  "Learned workday start: {learnedStart}, end: {learnedEnd}. "
  "Greet them in a witty, encouraging, or playful way, including the latest break length and partially other info above.";

static const char* PROMPT_WELCOME_BACK = 
  "Address {name} who returned to their desk after a break of {detail}. "
  "Daily Stats: desk time: {deskTime}, focus: {focusTime}, breaks taken: {breakCount}, productivity: {score}%. "
  "Welcome them back in a short, witty, or motivational way, including the latest break length and other relevant info above.";

static const char* PROMPT_STRETCH_REMINDER = 
  "Address {name} who has been sitting continuously for 45 minutes (longest streak: {longestStreak}). "
  "Daily Stats: desk time: {deskTime}, productivity: {score}%. "
  "Tell them to stretch or walk in a sassy, playful way, including current sitting duration and partially other relevant info above.";

static const char* PROMPT_FOCUS_CONGRATS = 
  "Address {name} who just completed a deep focus session of {detail}. "
  "Daily Stats: desk time: {deskTime}, focus: {focusTime}, productivity: {score}%. "
  "Congratulate them on their focus and deep work, including current focus duration and partially other relevant info above.";

static const char* PROMPT_SLACKER_ROAST = 
  "Address {name} who has focused for less than 20% of their workday. "
  "Daily Stats: desk time: {deskTime}, focus: {focusTime}, productivity: {score}%. "
  "Playfully roast them for slacking or procrastinating, including the slacking mode duration and partially other relevant info above.";

static const char* PROMPT_STREAK_BEATEN = 
  "Address {name} who just beat their previous longest sitting streak of the day, which was {detail}. "
  "Daily Stats: desk time: {deskTime}, productivity: {score}%. "
  "Congratulate them or joke about their sitting endurance, including current sitting duration and partially other relevant info above.";

static const char* PROMPT_LUNCH_REMINDER = 
  "Address {name} who is still working at their desk during their usual lunch hour {learnedLunch}. "
  "Daily Stats: desk time: {deskTime}, focus: {focusTime}, productivity: {score}%. "
  "Remind them to eat lunch in a witty, appetizing, or playful way, including current desk time and partially other relevant info above.";

#endif // BEHAVIOUR_H
