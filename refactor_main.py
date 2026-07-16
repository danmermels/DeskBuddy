"""Refactor main.cpp to use State.h structs instead of global variables."""

import re

with open('src/main.cpp', 'r') as f:
    content = f.read()

# ===== STEP 1: Add State.h include =====
old_block = (
    "// ============================================================\n"
    "// TODO: Consolidate ~100 globals into a DeskBuddyState context\n"
    "//       struct to eliminate extern web across all subsystem headers.\n"
    "// ============================================================"
)
new_block = (
    "// ============================================================\n"
    "// Event Type Macros and Function Declarations (needed by State.h)\n"
    "// ============================================================\n"
    "#include \"Behaviour.h\"\n"
    "#include \"Learning.h\"\n"
    "#include \"PresenceAnalysis.h\"\n\n"
    "// ============================================================\n"
    "// Global State Management (depends on macros/functions from above headers)\n"
    "// ============================================================\n"
    "#include \"State.h\"\n\n"
    "// ============================================================\n"
    "// Subsystem Headers (extern globals are linked from this file)\n"
    "// ============================================================"
)
content = content.replace(old_block, new_block)

# ===== STEP 2: Add global state instances =====
old_hw = (
    "// ============================================================\n"
    "// Hardware Instances\n"
    "// ============================================================"
)
new_hw = (
    "// ============================================================\n"
    "// Global State Instantiation\n"
    "// ============================================================\n"
    "ConfigState appConfig;\n"
    "StatsState appStats;\n"
    "TodoState appTodo;\n"
    "RuntimeState appState;\n\n"
    "// ============================================================\n"
    "// Hardware Instances\n"
    "// ============================================================"
)
content = content.replace(old_hw, new_hw)

# ===== STEP 3: Remove old global declarations =====
# Lines to remove (exact match, handling both \r\n and \n)
old_decls = [
    'float filteredDetectionDist = 0.0;',
    'int deskDistanceLimit = DISTANCE_LIMIT_DEFAULT;',
    'int focusDistanceLimit = FOCUS_DISTANCE_LIMIT_DEFAULT;',
    'int motionRatioLimit = MOTION_RATIO_LIMIT_DEFAULT;',
    'unsigned long totalDeskTime = 0;',
    'unsigned long totalFocusTime = 0;',
    'unsigned long totalBreakTime = 0;',
    'int breakCount = 0;',
    'int productivityScore = 0;',
    'unsigned long latestBreakDuration = 0;',
    'unsigned long overnightBreakDuration = 0;',
    'uint32_t lastAwayEpoch = 0;',
    'unsigned long currentBreakDurationMs = 0;',
    'bool isStopByTracking = false;',
    'uint32_t originalLastAwayEpoch = 0;',
    'unsigned long totalStopByTimeMs = 0;',
    'unsigned long previousLatestBreakDuration = 0;',
    'int lastMidnightCheckDay = -1;',
    'volatile uint32_t currentSitDownSessionId = 0;',
    'uint32_t geminiQuerySessionId = 0;',
    'int currentPresenceState = STATE_AWAY;',
    'unsigned long lastStateTransitionTime = 0;',
    'unsigned long lastLoopTime = 0;',
    'unsigned long continuousPresenceStart = 0;',
    'unsigned long continuousStillStart = 0;',
    'unsigned long lastStretchReminderTime = 0;',
    'volatile bool isAILoading = false;',
    'String aiResponse = "";',
    'volatile bool hasNewAIResponse = false;',
    'volatile bool lastResponseIsAi = false;',
    'String currentPrompt = "";',
    'String lastTriggeredEventDetail = "";',
    'String currentUserName = "human";',
    'SemaphoreHandle_t geminiMutex = NULL;',
    'volatile bool otaInProgress = false;',
    'MqttMessage mqttHistory[MQTT_HISTORY_SIZE];',
    'int mqttHistoryHead = 0;',
    'int mqttHistoryCount = 0;',
    'SemaphoreHandle_t mqttHistoryMutex = NULL;',
    'Preferences preferences;',
    'float targetHours = 8.0;',
    'int aiMode = 1; // 0 = Eco, 1 = Balanced, 2 = Frequent',
    'int aiPersona = 0; // 0 = Coach, 1 = Critic, 2 = Nerd, 3 = Zen',
    'int clockFace = 0;',
    'int dailyAiRequestCount = 0;',
    'String userName = "human";',
    'bool firstSitToday = true;',
    'uint32_t firstSitEpoch = 0;',
    'unsigned long longestSittingStreak = 0;',
    'bool streakAlertTriggered = false;',
    'int lastNtpDay = -1;',
    'int lastTriggeredEventType = EVENT_FIRST_SIT;',
    'float filterWindow = 2.0;',
    'bool hasMail = false;',
    'bool time24h = true;',
    'uint8_t hourlyPresenceHistory[24] = {0};',
    'uint32_t presenceMsCurrentDay[24] = {0};',
    'int historyDaysCount = 0;',
    'bool lunchReminderTriggered = false;',
    'unsigned long sitDownTime = 0;',
    'uint32_t sitDownEpoch = 0;',
    'bool rolloverPending = false;',
    'unsigned long requiredValidationBufferMs = VALIDATION_BUFFER_MS;',
    '  unsigned long requiredValidationBufferMs = VALIDATION_BUFFER_MS;',
    'uint32_t fsWriteCount = 0;',
    'uint32_t fsReadCount = 0;',
    'int rawDetectionDist = 0;',
    'bool sensorPresenceDetected = false;',
    'bool sensorMovingTargetDetected = false;',
    'bool sensorStaticPresenceDetected = false;',
    'unsigned long sessionDeskTime = 0;',
    'unsigned long sessionMotionTime = 0;',
    'unsigned long totalMotionTime = 0;',
    'int motionCount = 0;',
    'unsigned long sessionDistanceSum = 0;',
    'unsigned long sessionDistanceCount = 0;',
    'float sessionDistanceAverage = 0.0;',
]

for decl in old_decls:
    content = content.replace('\n' + decl + '\r\n', '\n')
    content = content.replace('\n' + decl + '\n', '\n')

# ===== STEP 4: Replace variable references =====
# Use a helper that only matches outside of double-quoted strings
def replace_outside_strings(text, var_name, replacement):
    """Replace var_name with replacement, but only when not inside a string and not already prefixed."""
    # Match var_name that is:
    # 1. Not preceded by . (to avoid matching struct.field patterns)
    # 2. Not inside double quotes
    # 3. Not preceded or followed by a word character (whole word)
    pattern = r'(?<!["\w.])' + re.escape(var_name) + r'(?!["\w])'
    return re.sub(pattern, replacement, text)

# ConfigState
content = replace_outside_strings(content, 'targetHours', 'appConfig.targetHours')
content = replace_outside_strings(content, 'aiMode', 'appConfig.aiMode')
content = replace_outside_strings(content, 'aiPersona', 'appConfig.aiPersona')
content = replace_outside_strings(content, 'clockFace', 'appConfig.clockFace')
content = replace_outside_strings(content, 'userName', 'appConfig.userName')
content = replace_outside_strings(content, 'focusDistanceLimit', 'appConfig.focusDistanceLimit')
content = replace_outside_strings(content, 'motionRatioLimit', 'appConfig.motionRatioLimit')
content = replace_outside_strings(content, 'deskDistanceLimit', 'appConfig.deskDistanceLimit')
content = replace_outside_strings(content, 'filterWindow', 'appConfig.filterWindow')
content = replace_outside_strings(content, 'hasMail', 'appConfig.hasMail')
content = replace_outside_strings(content, 'time24h', 'appConfig.time24h')

# StatsState
content = replace_outside_strings(content, 'totalDeskTime', 'appStats.totalDeskTime')
content = replace_outside_strings(content, 'totalFocusTime', 'appStats.totalFocusTime')
content = replace_outside_strings(content, 'totalBreakTime', 'appStats.totalBreakTime')
content = replace_outside_strings(content, 'breakCount', 'appStats.breakCount')
content = replace_outside_strings(content, 'productivityScore', 'appStats.productivityScore')
content = replace_outside_strings(content, 'latestBreakDuration', 'appStats.latestBreakDuration')
content = replace_outside_strings(content, 'overnightBreakDuration', 'appStats.overnightBreakDuration')
content = replace_outside_strings(content, 'firstSitEpoch', 'appStats.firstSitEpoch')
content = replace_outside_strings(content, 'firstSitToday', 'appStats.firstSitToday')
content = replace_outside_strings(content, 'longestSittingStreak', 'appStats.longestSittingStreak')
content = replace_outside_strings(content, 'historyDaysCount', 'appStats.historyDaysCount')
content = replace_outside_strings(content, 'lunchReminderTriggered', 'appStats.lunchReminderTriggered')
content = replace_outside_strings(content, 'lastNtpDay', 'appStats.lastNtpDay')
content = replace_outside_strings(content, 'lastMidnightCheckDay', 'appStats.lastMidnightCheckDay')
content = replace_outside_strings(content, 'previousLatestBreakDuration', 'appStats.previousLatestBreakDuration')
content = replace_outside_strings(content, 'fsWriteCount', 'appStats.fsWriteCount')
content = replace_outside_strings(content, 'fsReadCount', 'appStats.fsReadCount')

# RuntimeState
content = replace_outside_strings(content, 'currentPresenceState', 'appState.currentPresenceState')
content = replace_outside_strings(content, 'filteredDetectionDist', 'appState.filteredDetectionDist')
content = replace_outside_strings(content, 'lastAwayEpoch', 'appState.lastAwayEpoch')
content = replace_outside_strings(content, 'currentBreakDurationMs', 'appState.currentBreakDurationMs')
content = replace_outside_strings(content, 'isStopByTracking', 'appState.isStopByTracking')
content = replace_outside_strings(content, 'originalLastAwayEpoch', 'appState.originalLastAwayEpoch')
content = replace_outside_strings(content, 'totalStopByTimeMs', 'appState.totalStopByTimeMs')
content = replace_outside_strings(content, 'lastStateTransitionTime', 'appState.lastStateTransitionTime')
content = replace_outside_strings(content, 'lastLoopTime', 'appState.lastLoopTime')
content = replace_outside_strings(content, 'continuousPresenceStart', 'appState.continuousPresenceStart')
content = replace_outside_strings(content, 'continuousStillStart', 'appState.continuousStillStart')
content = replace_outside_strings(content, 'lastStretchReminderTime', 'appState.lastStretchReminderTime')
content = replace_outside_strings(content, 'dailyAiRequestCount', 'appState.dailyAiRequestCount')
content = replace_outside_strings(content, 'streakAlertTriggered', 'appState.streakAlertTriggered')
content = replace_outside_strings(content, 'otaInProgress', 'appState.otaInProgress')
content = replace_outside_strings(content, 'sitDownTime', 'appState.sitDownTime')
content = replace_outside_strings(content, 'sitDownEpoch', 'appState.sitDownEpoch')
content = replace_outside_strings(content, 'rolloverPending', 'appState.rolloverPending')
content = replace_outside_strings(content, 'requiredValidationBufferMs', 'appState.requiredValidationBufferMs')
content = replace_outside_strings(content, 'sensorPresenceDetected', 'appState.sensorPresenceDetected')
content = replace_outside_strings(content, 'sensorMovingTargetDetected', 'appState.sensorMovingTargetDetected')
content = replace_outside_strings(content, 'sensorStaticPresenceDetected', 'appState.sensorStaticPresenceDetected')
content = replace_outside_strings(content, 'rawDetectionDist', 'appState.rawDetectionDist')
content = replace_outside_strings(content, 'sessionDistanceAverage', 'appState.sessionDistanceAverage')
content = replace_outside_strings(content, 'sessionDistanceSum', 'appState.sessionDistanceSum')
content = replace_outside_strings(content, 'sessionDistanceCount', 'appState.sessionDistanceCount')
content = replace_outside_strings(content, 'sessionDeskTime', 'appState.sessionDeskTime')
content = replace_outside_strings(content, 'sessionMotionTime', 'appState.sessionMotionTime')
content = replace_outside_strings(content, 'totalMotionTime', 'appState.totalMotionTime')
content = replace_outside_strings(content, 'motionCount', 'appState.motionCount')

# Special case: lastTriggeredEventType is in both StatsState and RuntimeState
# It's used in main.cpp as the event type tracker -> RuntimeState
content = replace_outside_strings(content, 'lastTriggeredEventType', 'appState.lastTriggeredEventType')

# ===== STEP 5: Clean up triple blank lines =====
content = re.sub(r'\n{3,}', '\n\n', content)

with open('src/main.cpp', 'w') as f:
    f.write(content)

print("main.cpp refactored successfully!")
