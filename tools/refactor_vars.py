import re
import os

script_dir = os.path.dirname(os.path.abspath(__file__))
project_root = script_dir if os.path.basename(script_dir) != 'tools' else os.path.dirname(script_dir)
main_cpp_path = os.path.join(project_root, 'src', 'main.cpp')

with open(main_cpp_path, 'r') as f:
    content = f.read()

# Define all variable renames: (old_pattern, new_pattern)
# Order matters - do more specific/variable-specific renames first
replacements = [
    # ConfigState replacements
    (r'\btargetHours\b', 'appConfig.targetHours'),
    (r'\baiMode\b', 'appConfig.aiMode'),
    (r'\baiPersona\b', 'appConfig.aiPersona'),
    (r'\bclockFace\b', 'appConfig.clockFace'),
    (r'\buserName\b', 'appConfig.userName'),
    (r'\bfocusDistanceLimit\b', 'appConfig.focusDistanceLimit'),
    (r'\bmotionRatioLimit\b', 'appConfig.motionRatioLimit'),
    (r'\bdeskDistanceLimit\b', 'appConfig.deskDistanceLimit'),
    (r'\bfilterWindow\b', 'appConfig.filterWindow'),
    (r'\bhasMail\b', 'appConfig.hasMail'),
    (r'\btime24h\b', 'appConfig.time24h'),

    # StatsState replacements
    (r'\btotalDeskTime\b', 'appStats.totalDeskTime'),
    (r'\btotalFocusTime\b', 'appStats.totalFocusTime'),
    (r'\btotalBreakTime\b', 'appStats.totalBreakTime'),
    (r'\bbreakCount\b', 'appStats.breakCount'),
    (r'\bproductivityScore\b', 'appStats.productivityScore'),
    (r'\blatestBreakDuration\b', 'appStats.latestBreakDuration'),
    (r'\bovernightBreakDuration\b', 'appStats.overnightBreakDuration'),
    (r'\bfirstSitEpoch\b', 'appStats.firstSitEpoch'),
    (r'\bfirstSitToday\b', 'appStats.firstSitToday'),
    (r'\blongestSittingStreak\b', 'appStats.longestSittingStreak'),
    (r'\bhistoryDaysCount\b', 'appStats.historyDaysCount'),
    (r'\blunchReminderTriggered\b', 'appStats.lunchReminderTriggered'),
    (r'\blastNtpDay\b', 'appStats.lastNtpDay'),
    (r'\blastMidnightCheckDay\b', 'appStats.lastMidnightCheckDay'),
    (r'\bpreviousLatestBreakDuration\b', 'appStats.previousLatestBreakDuration'),
    (r'\bfsWriteCount\b', 'appStats.fsWriteCount'),
    (r'\bfsReadCount\b', 'appStats.fsReadCount'),

    # RuntimeState replacements
    (r'\bcurrentPresenceState\b', 'appState.currentPresenceState'),
    (r'\bfilteredDetectionDist\b', 'appState.filteredDetectionDist'),
    (r'\blastAwayEpoch\b', 'appState.lastAwayEpoch'),
    (r'\bcurrentBreakDurationMs\b', 'appState.currentBreakDurationMs'),
    (r'\bisStopByTracking\b', 'appState.isStopByTracking'),
    (r'\boriginalLastAwayEpoch\b', 'appState.originalLastAwayEpoch'),
    (r'\btotalStopByTimeMs\b', 'appState.totalStopByTimeMs'),
    (r'\blastStateTransitionTime\b', 'appState.lastStateTransitionTime'),
    (r'\blastLoopTime\b', 'appState.lastLoopTime'),
    (r'\bcontinuousPresenceStart\b', 'appState.continuousPresenceStart'),
    (r'\bcontinuousStillStart\b', 'appState.continuousStillStart'),
    (r'\blastStretchReminderTime\b', 'appState.lastStretchReminderTime'),
    (r'\bdailyAiRequestCount\b', 'appState.dailyAiRequestCount'),
    (r'\bstreakAlertTriggered\b', 'appState.streakAlertTriggered'),
    (r'\botaInProgress\b', 'appState.otaInProgress'),
    (r'\bsitDownTime\b', 'appState.sitDownTime'),
    (r'\bsitDownEpoch\b', 'appState.sitDownEpoch'),
    (r'\brolloverPending\b', 'appState.rolloverPending'),
    (r'\brequiredValidationBufferMs\b', 'appState.requiredValidationBufferMs'),
    (r'\bsensorPresenceDetected\b', 'appState.sensorPresenceDetected'),
    (r'\bsensorMovingTargetDetected\b', 'appState.sensorMovingTargetDetected'),
    (r'\bsensorStaticPresenceDetected\b', 'appState.sensorStaticPresenceDetected'),
    (r'\brawDetectionDist\b', 'appState.rawDetectionDist'),
    (r'\bsessionDistanceAverage\b', 'appState.sessionDistanceAverage'),
    (r'\bsessionDistanceSum\b', 'appState.sessionDistanceSum'),
    (r'\bsessionDistanceCount\b', 'appState.sessionDistanceCount'),
    (r'\bsessionDeskTime\b', 'appState.sessionDeskTime'),
    (r'\bsessionMotionTime\b', 'appState.sessionMotionTime'),
    (r'\btotalMotionTime\b', 'appState.totalMotionTime'),
    (r'\bmotionCount\b', 'appState.motionCount'),
]

# Apply each replacement
for pattern, replacement in replacements:
    content = re.sub(pattern, replacement, content)

# Remove old global variable declarations that are now in structs
lines_to_remove = [
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

# Remove each old declaration line (being careful not to remove struct member initializations)
lines = content.split('\n')
filtered_lines = []
removed_count = 0
for line in lines:
    stripped = line.strip()
    # Check if this line is an old global declaration to remove
    should_remove = False
    for decl in lines_to_remove:
        if stripped == decl:
            should_remove = True
            removed_count += 1
            break
    if not should_remove:
        filtered_lines.append(line)

content = '\n'.join(filtered_lines)
with open('src/main.cpp', 'w') as f:
    f.write(content)

print(f'Removed {removed_count} old global declarations')
print('Done!')
