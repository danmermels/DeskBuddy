#ifndef BEHAVIOUR_H
#define BEHAVIOUR_H

// --- Event Types ---
#define EVENT_FIRST_SIT     0
#define EVENT_WELCOME_BACK  1
#define EVENT_STRETCH       2
#define EVENT_FOCUS_END     3
#define EVENT_SLACKER       4
#define EVENT_STREAK_BEATEN 5

// --- Local Fallback/Eco Quotes (20 per category) ---

const char* localFirstSit[20] = {
  "Morning, %s. Ready to crush some goals?",
  "Rise and shine, %s! The desk is waiting.",
  "Welcome, %s! Let's build something epic.",
  "Ah, %s, you're back. Let's make it count!",
  "Coffee ready, %s? Time to focus!",
  "Good morning, %s! Let's make today count.",
  "Hello, %s! Ready to tackle the day?",
  "Another day, another opportunity, %s.",
  "Wake up, %s! The day is calling.",
  "Starting fresh, %s! Let's do this.",
  "Welcome, %s, to your focus arena.",
  "System online, %s. Let's write history.",
  "New day, %s. New projects to conquer.",
  "Ready to show who's boss today, %s?",
  "Hello, %s! Make sure to take breaks.",
  "Welcome, %s! Let's create success.",
  "A fresh start, %s. Work with passion.",
  "Morning, %s! May your tasks be smooth.",
  "Welcome back, %s. Time to perform magic.",
  "Good morning, %s! Let's start strong."
};

const char* localWelcomeBack[20] = {
  "Welcome back, %s! Missed me?",
  "Break's over, %s! Back to the desk.",
  "Did you stretch, %s? Good. Now work.",
  "Ready to continue the masterpiece, %s?",
  "Recharged, %s? Let's focus!",
  "Ah, %s, you returned. Let's focus.",
  "Back to work, %s!",
  "Hope that break was refreshing, %s.",
  "Where were we, %s? Ah yes, working.",
  "Back in the saddle, %s. Focus time!",
  "Welcome back, %s. Let's write history.",
  "The desk missed your presence, %s.",
  "Break done, %s. Let's make progress!",
  "Welcome back, %s. Zero in!",
  "Welcome back, %s! Let's keep going.",
  "Did you get coffee, %s? Start focus.",
  "Back to reality, %s. Let's focus.",
  "Welcome back, %s! Everything is ready.",
  "Let's pick up where we left off, %s.",
  "No more slacking, %s. Welcome back!"
};

const char* localStretch[20] = {
  "Stand up, %s! Your spine is crying.",
  "Time to stretch, %s! Move those legs.",
  "Hey %s, look at something far away!",
  "Roll your shoulders, %s. Breathe in.",
  "Hydrate, %s! Go get some water.",
  "Your posture is like a banana, %s. Fix!",
  "Time out, %s! Stand up for a minute.",
  "Blink, %s! Your eyes need a rest.",
  "Step away from the screen, %s!",
  "Walk around, %s. Your body will thank you.",
  "Roll your wrists, %s. Take a breath.",
  "Stand up, %s, and reach for the sky!",
  "Sitting is the new smoking, %s. Move!",
  "Time for a 1-minute stretch, %s.",
  "Are you slouching, %s? Sit up straight!",
  "Get some water, %s, stay fresh.",
  "Give your eyes a 20-second break, %s.",
  "Breathe deeply and stretch, %s.",
  "Stand up, %s! Shake it out.",
  "Walk, stretch, breathe, %s. Do it now."
};

const char* localFocus[20] = {
  "Focus session complete! Great work, %s.",
  "Deep focus achieved, %s! You're a beast!",
  "Nice work focusing there, %s!",
  "Productivity boss! Take a bow, %s.",
  "Stellar focus session, %s! Take a break.",
  "You concentrated well, %s. Awesome!",
  "Solid focus session, %s. Proud of you!",
  "Deep work complete, %s. High five!",
  "You crushed that focus block, %s!",
  "Great focus, %s. Enjoy your break!",
  "Focus champion, %s! Time to stretch.",
  "Excellent focus session, %s. Keep it up!",
  "You stayed locked in, %s. Great job!",
  "Focus achieved, %s. You earned a rest.",
  "Brilliant work staying focused, %s!",
  "Focus session ended, %s. Take a walk.",
  "You ruled that focus block, %s!",
  "Incredible focus today, %s!",
  "Focus target hit, %s. Rest your mind.",
  "Superb concentration, %s. Time to relax."
};

const char* localSlacker[20] = {
  "Procrastinator alert! Focus, %s!",
  "Are you actually working, %s?",
  "Your productivity score is crying, %s.",
  "Focus score is low, %s. Stop slacking!",
  "Wake up, %s! Less browsing, more working.",
  "Is this your maximum speed, %s?",
  "Focus, please, %s! Time is ticking.",
  "You're getting distracted, %s. Lock in!",
  "Your keyboard is feeling lonely, %s.",
  "Let's turn this score around, %s!",
  "Slack off less, %s, focus more.",
  "You're drifting, %s. Focus up!",
  "Is that social media I see, %s?",
  "Don't let procrastination win, %s.",
  "Your focus score is bottoming out, %s.",
  "Less scrolling, %s, more working.",
  "Get back to work, %s!",
  "Stop dreaming, %s! Let's work!",
  "Zero focus, %s. Let's change that.",
  "Attention span of a goldfish today, %s?"
};

const char* localStreakBeaten[20] = {
  "New sitting record, %s! Keep it up!",
  "Streak beaten, %s! You are on fire!",
  "Sitting champion, %s! A new record!",
  "Unstoppable, %s! New sitting streak!",
  "New record, %s! Marathon sitting!",
  "Streak record broken, %s! Outstanding!",
  "Sitting boss, %s! You beat your record!",
  "New personal best sitting streak, %s!",
  "Sitting record smashed, %s! Elite focus!",
  "Incredible, %s! New longest sit today!",
  "Record sitting session, %s! Keep going!",
  "New streak, %s! Sitting like a statue!",
  "Record broken, %s! Focus level maximum!",
  "Sitting legend, %s! New longest streak!",
  "You beat your previous sitting record, %s!",
  "Marathon sit, %s! Previous record beaten!",
  "New record, %s! The chair is your throne!",
  "Sitting milestone reached, %s! Great job!",
  "New streak record, %s! Absolute focus!",
  "Amazing, %s! Longest sit of the day!"
};

// --- Gemini AI Prompts (Templates used when AI is active) ---

const char* PROMPT_FIRST_SIT_OF_DAY = 
  "%s has just sat down at their desk for the first time today. Say hello in a witty, encouraging, or playful way in 1 sentence under 30 characters.";

const char* PROMPT_WELCOME_BACK = 
  "%s has returned to their desk after a break of %s. Welcome them back in a short, witty, or motivational way in 1 sentence under 30 characters.";

const char* PROMPT_STRETCH_REMINDER = 
  "%s has been sitting continuously for 45 minutes. Tell them to stretch or walk in a sassy, playful way in 1 sentence under 30 characters.";

const char* PROMPT_FOCUS_CONGRATS = 
  "%s just completed a deep focus session of %s. Congratulate them on their hard work or joke about their dedication in 1 sentence under 30 characters.";

const char* PROMPT_SLACKER_ROAST = 
  "%s has been present for 1 hour but has focused for less than 20% of that time. Playfully roast them for slacking or procrastinating in 1 sentence under 30 characters.";

const char* PROMPT_STREAK_BEATEN = 
  "%s has just beaten their previous longest sitting streak of the day, which was %s. Congratulate them or joke about their sitting endurance in 1 sentence under 30 characters.";

#endif // BEHAVIOUR_H
