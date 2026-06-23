#ifndef BEHAVIOUR_H
#define BEHAVIOUR_H

// --- Event Types ---
#define EVENT_FIRST_SIT     0
#define EVENT_WELCOME_BACK  1
#define EVENT_STRETCH       2
#define EVENT_FOCUS_END     3
#define EVENT_SLACKER       4

// --- Local Fallback/Eco Quotes (20 per category) ---

const char* localFirstSit[20] = {
  "Morning, human. Ready to write bugs?",
  "Rise and shine! The IDE is waiting.",
  "Welcome! Let's build something epic.",
  "Ah, you're back. Let's make it count!",
  "Coffee ready? Time to write code!",
  "Good morning! Please type carefully.",
  "Hello! Ready to smash some code?",
  "Another day, another line of code.",
  "Wake up! The compiler needs food.",
  "Starting fresh! Let's do this.",
  "Welcome to your keyboard arena.",
  "System online. Let's write history.",
  "New day, new bugs to squash.",
  "Ready to show who's boss today?",
  "Hello! Make sure to take breaks.",
  "Welcome! Let's compile success.",
  "A fresh start. Code with passion.",
  "Morning! May your code compile first.",
  "Welcome back. Time to perform magic.",
  "Hello world! Let's do this."
};

const char* localWelcomeBack[20] = {
  "Welcome back! Missed me?",
  "Break's over! Back to the keys.",
  "Did you stretch? Good. Now work.",
  "Ready to continue the masterpiece?",
  "Recharged? Let's compile!",
  "Ah, you returned. Let's focus.",
  "Back to work, captain!",
  "Hope that break was refreshing.",
  "Where were we? Ah yes, coding.",
  "Back in the saddle. Code time!",
  "Welcome back. Let's write logic.",
  "The keyboard missed your touch.",
  "Break done. Let's ship it!",
  "Welcome back. Zero in!",
  "Return of the coder. Welcome back!",
  "Did you get coffee? Start typing.",
  "Back to reality. Let's focus.",
  "Welcome back! Compiler is ready.",
  "Let's pick up where we left off.",
  "No more slacking. Welcome back!"
};

const char* localStretch[20] = {
  "Stand up! Your spine is crying.",
  "Time to stretch! Move those legs.",
  "Hey, look at something far away!",
  "Roll your shoulders. Breathe in.",
  "Hydrate! Go get some water.",
  "Your posture is like a banana. Fix!",
  "Time out! Stand up for a minute.",
  "Blink! Your eyes need a rest.",
  "Step away from the screen!",
  "Walk around. Your body will thank you.",
  "Roll your wrists. Take a breath.",
  "Stand up and reach for the sky!",
  "Sitting is the new smoking. Move!",
  "Time for a 1-minute stretch.",
  "Are you slouching? Sit up straight!",
  "Get some water, stay fresh.",
  "Give your eyes a 20-second break.",
  "Breathe deeply and stretch.",
  "Stand up! Shake it out.",
  "Walk, stretch, breathe. Do it now."
};

const char* localFocus[20] = {
  "Focus session complete! Great work.",
  "Deep focus achieved. You're a beast!",
  "Nice work focusing there!",
  "Productivity boss! Take a bow.",
  "Stellar focus session! Take a break.",
  "You concentrated well. Awesome!",
  "Solid focus session. Proud of you!",
  "Deep work complete. High five!",
  "You crushed that focus block!",
  "Great focus. Enjoy your break!",
  "Focus champion! Time to stretch.",
  "Excellent focus session. Keep it up!",
  "You stayed locked in. Great job!",
  "Focus achieved. You earned a rest.",
  "Brilliant work staying focused!",
  "Focus session ended. Take a walk.",
  "You ruled that focus block!",
  "Incredible focus today!",
  "Focus target hit. Rest your mind.",
  "Superb concentration. Time to relax."
};

const char* localSlacker[20] = {
  "Procrastinator alert! Focus!",
  "Are you actually working?",
  "Your productivity score is crying.",
  "Focus score is low. Stop slacking!",
  "Wake up! Less browsing, more coding.",
  "Is this your maximum speed?",
  "Focus, please! Time is ticking.",
  "You're getting distracted. Lock in!",
  "Your keyboard is feeling lonely.",
  "Let's turn this score around!",
  "Slack off less, compile more.",
  "You're drifting. Focus up!",
  "Is that social media I see?",
  "Don't let procrastination win.",
  "Your focus score is bottoming out.",
  "Less scrolling, more typing.",
  "Get back to work, slacker!",
  "Stop dreaming! Code!",
  "Zero focus. Let's change that.",
  "Attention span of a goldfish today?"
};

// --- Gemini AI Prompts (Templates used when AI is active) ---

const char* PROMPT_FIRST_SIT_OF_DAY = 
  "The user has just sat down at their desk for the first time today. Say hello in a witty, encouraging, or playful way in 1 sentence under 30 characters.";

const char* PROMPT_WELCOME_BACK = 
  "The user has returned to their desk after a break of %s. Welcome them back in a short, witty, or motivational way in 1 sentence under 30 characters.";

const char* PROMPT_STRETCH_REMINDER = 
  "The user has been sitting continuously for 45 minutes. Tell them to stretch or walk in a sassy, playful way in 1 sentence under 30 characters.";

const char* PROMPT_FOCUS_CONGRATS = 
  "The user just completed a deep focus session of %s. Congratulate them on their hard work or joke about their dedication in 1 sentence under 30 characters.";

const char* PROMPT_SLACKER_ROAST = 
  "The user has been present for 1 hour but has focused for less than 20%% of that time. Playfully roast them for slacking or procrastinating in 1 sentence under 30 characters.";

#endif // BEHAVIOUR_H
