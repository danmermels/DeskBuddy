use std::fs;
use std::io::{Cursor, Write};
use std::net::{SocketAddr, TcpStream, UdpSocket};
use std::path::PathBuf;
use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc, Mutex,
};
use std::thread;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

use chrono::{Datelike, Local};
use image::ImageReader;
use mdns_sd::{ServiceDaemon, ServiceEvent};
use serde::{Deserialize, Serialize};
use tauri::{
    image::Image,
    menu::{MenuBuilder, MenuItemBuilder},
    tray::{MouseButton, MouseButtonState, TrayIconBuilder, TrayIconEvent},
    Emitter, Listener, Manager, RunEvent, WebviewUrl, WebviewWindowBuilder, WindowEvent,
};

const ICON_GREEN: &[u8] = include_bytes!("../icons/tray-green.png");
const ICON_GRAY: &[u8] = include_bytes!("../icons/tray-gray.png");

#[derive(Debug, Clone, Serialize, Deserialize)]
struct Settings {
    deskbuddy_ip: Option<String>,
    deskbuddy_port: u16,
}

impl Default for Settings {
    fn default() -> Self {
        Settings {
            deskbuddy_ip: None,
            deskbuddy_port: 80,
        }
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
struct DeviceInfo {
    ip: String,
    port: u16,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
struct RadarData {
    #[serde(default)]
    lang: Option<String>,
    #[serde(default)]
    language: Option<String>,

    // Daily Performance Stats
    #[serde(default, rename = "deskTime")]
    desk_time: Option<String>,
    #[serde(default, rename = "focusTime")]
    focus_time: Option<String>,
    #[serde(default, rename = "breakTime")]
    break_time: Option<String>,
    #[serde(default)]
    breaks: Option<u32>,
    #[serde(default, rename = "latestBreak")]
    latest_break: Option<String>,
    #[serde(default, rename = "firstSitTime")]
    first_sit_time: Option<String>,
    #[serde(default, rename = "longestStreak")]
    longest_streak: Option<String>,
    #[serde(default)]
    score: Option<u32>,

    // Odometers
    #[serde(default, rename = "activeOdometer")]
    active_odometer: Option<usize>,
    #[serde(default, rename = "odometerFmt")]
    odometer_fmt: Option<Vec<String>>,
    #[serde(default, rename = "odometerLabels")]
    odometer_labels: Option<Vec<String>>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
struct DailyTask {
    text: String,
    #[serde(default)]
    hour: Option<u32>,
    #[serde(default)]
    minute: Option<u32>,
    #[serde(default)]
    recurrent: bool,
    #[serde(default, rename = "startDate")]
    start_date: Option<String>,
    #[serde(default, rename = "completedDates")]
    completed_dates: Option<Vec<String>>,
    #[serde(default, rename = "targetDate")]
    target_date: Option<String>,
    #[serde(default)]
    completed: Option<bool>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
struct MonthlyTask {
    text: String,
    #[serde(default)]
    day: Option<u32>,
    #[serde(default)]
    month: Option<u32>,
    #[serde(default)]
    year: Option<u32>,
    #[serde(default)]
    recurrent: bool,
    #[serde(default, rename = "startMonth")]
    start_month: Option<String>,
    #[serde(default, rename = "completedMonths")]
    completed_months: Option<Vec<String>>,
    #[serde(default)]
    completed: Option<bool>,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
struct TodoDoc {
    #[serde(default)]
    daily: Vec<DailyTask>,
    #[serde(default)]
    monthly: Vec<MonthlyTask>,
    #[serde(flatten)]
    extra: serde_json::Value,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
struct DisplayTask {
    is_daily: bool,
    orig_index: usize,
    time_sort_key: u32,
    display_label: String,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
struct LiveBuddyState {
    radar: RadarData,
    todo: TodoDoc,
    due_tasks: Vec<DisplayTask>,
}

fn app_dir() -> PathBuf {
    let base = dirs::config_local_dir()
        .unwrap_or_else(|| PathBuf::from("."))
        .join("DeskBuddy Companion");
    let _ = fs::create_dir_all(&base);
    base
}

fn settings_path() -> PathBuf {
    app_dir().join("settings.json")
}

fn log_path() -> PathBuf {
    let base = dirs::data_local_dir()
        .unwrap_or_else(|| PathBuf::from("."))
        .join("DeskBuddy Companion");
    let _ = fs::create_dir_all(&base);
    base.join("companion.log")
}

fn load_settings() -> Settings {
    fs::read_to_string(settings_path())
        .ok()
        .and_then(|s| serde_json::from_str(&s).ok())
        .unwrap_or_default()
}

fn save_settings(s: &Settings) {
    if let Ok(json) = serde_json::to_string_pretty(s) {
        let _ = fs::write(settings_path(), json);
    }
}

fn app_log(msg: &str) {
    let ts = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs();
    let line = format!("[{}] {}\n", ts, msg);
    let _ = fs::OpenOptions::new()
        .create(true)
        .append(true)
        .open(log_path())
        .and_then(|mut f| f.write_all(line.as_bytes()));
}

fn load_icon(png_bytes: &[u8]) -> Result<Image<'static>, Box<dyn std::error::Error>> {
    let img = ImageReader::new(Cursor::new(png_bytes))
        .with_guessed_format()?
        .decode()?
        .into_rgba8();
    let (w, h) = img.dimensions();
    Ok(Image::new_owned(img.into_vec(), w, h))
}

fn check_device(ip: &str, port: u16) -> bool {
    let addr: Result<SocketAddr, _> = format!("{}:{}", ip, port).parse();
    match addr {
        Ok(a) => TcpStream::connect_timeout(&a, Duration::from_secs(2)).is_ok(),
        Err(_) => false,
    }
}

fn fetch_radar_data(ip: &str, port: u16) -> Option<RadarData> {
    let url = format!("http://{}:{}/radar-data", ip, port);
    match ureq::get(&url).timeout(Duration::from_secs(3)).call() {
        Ok(resp) => match resp.into_json::<RadarData>() {
            Ok(data) => {
                app_log(&format!("Fetched radar data successfully from {}", ip));
                Some(data)
            }
            Err(e) => {
                app_log(&format!("Failed to parse /radar-data JSON from {}: {}", ip, e));
                None
            }
        },
        Err(e) => {
            app_log(&format!("HTTP request to {} failed: {}", url, e));
            None
        }
    }
}

fn fetch_tasks_data(ip: &str, port: u16) -> Option<TodoDoc> {
    let url = format!("http://{}:{}/api/tasks", ip, port);
    match ureq::get(&url).timeout(Duration::from_secs(3)).call() {
        Ok(resp) => match resp.into_json::<TodoDoc>() {
            Ok(doc) => {
                app_log(&format!("Fetched tasks successfully from {}", ip));
                Some(doc)
            }
            Err(e) => {
                app_log(&format!("Failed to parse /api/tasks JSON from {}: {}", ip, e));
                None
            }
        },
        Err(e) => {
            app_log(&format!("HTTP request to {} failed: {}", url, e));
            None
        }
    }
}

fn compute_due_tasks(todo: &TodoDoc) -> Vec<DisplayTask> {
    let now = Local::now();
    let cur_year = now.year();
    let cur_month = now.month();
    let cur_day = now.day();

    let cur_date_str = format!("{:04}-{:02}-{:02}", cur_year, cur_month, cur_day);
    let cur_month_str = format!("{:04}-{:02}", cur_year, cur_month);

    let mut list = Vec::new();

    // 1. Process Daily Tasks (due today)
    for (idx, task) in todo.daily.iter().enumerate() {
        let is_due_today = if task.recurrent {
            if let Some(ref start) = task.start_date {
                cur_date_str.as_str() >= start.as_str()
            } else {
                true
            }
        } else if let Some(ref target) = task.target_date {
            target == &cur_date_str
        } else {
            true
        };

        if is_due_today {
            let is_completed = if task.recurrent {
                task.completed_dates
                    .as_ref()
                    .map(|dates| dates.iter().any(|d| d == &cur_date_str))
                    .unwrap_or(false)
            } else {
                task.completed.unwrap_or(false)
            };

            // Only show uncompleted tasks in the tray menu
            if !is_completed {
                let h = task.hour.unwrap_or(0);
                let m = task.minute.unwrap_or(0);
                let sort_key = h * 60 + m;
                let display_label = format!("☐ {:02}:{:02} - {}", h, m, task.text);

                list.push(DisplayTask {
                    is_daily: true,
                    orig_index: idx,
                    time_sort_key: sort_key,
                    display_label,
                });
            }
        }
    }

    // 2. Process Monthly Tasks (due this month)
    for (idx, task) in todo.monthly.iter().enumerate() {
        let is_due_this_month = if task.recurrent {
            if let Some(ref start) = task.start_month {
                cur_month_str.as_str() >= start.as_str()
            } else {
                true
            }
        } else if let (Some(m), Some(y)) = (task.month, task.year) {
            m == cur_month && y as i32 == cur_year
        } else if let Some(m) = task.month {
            m == cur_month
        } else {
            true
        };

        if is_due_this_month {
            let is_completed = if task.recurrent {
                task.completed_months
                    .as_ref()
                    .map(|months| months.iter().any(|mo| mo == &cur_month_str))
                    .unwrap_or(false)
            } else {
                task.completed.unwrap_or(false)
            };

            // Only show uncompleted tasks in the tray menu
            if !is_completed {
                let d = task.day.unwrap_or(1);
                // Monthly tasks sort after daily (offset by 24h = 1440 min + d * 60)
                let sort_key = 1440 + d * 60;
                let display_label = format!("☐ Dia {:02} - {}", d, task.text);

                list.push(DisplayTask {
                    is_daily: false,
                    orig_index: idx,
                    time_sort_key: sort_key,
                    display_label,
                });
            }
        }
    }

    // Sort earliest first
    list.sort_by_key(|t| t.time_sort_key);
    list
}

fn set_active_odometer(ip: &str, port: u16, odo_idx: usize) -> bool {
    let url = format!("http://{}:{}/api/odometer?active={}", ip, port, odo_idx);
    match ureq::post(&url).timeout(Duration::from_secs(3)).call() {
        Ok(_) => {
            app_log(&format!("Switched active odometer to {}", odo_idx));
            true
        }
        Err(e) => {
            app_log(&format!("Failed to set active odometer {}: {}", odo_idx, e));
            false
        }
    }
}

fn toggle_task_completion(ip: &str, port: u16, is_daily: bool, task_idx: usize) -> bool {
    let Some(mut doc) = fetch_tasks_data(ip, port) else {
        return false;
    };

    let now = Local::now();
    let cur_year = now.year();
    let cur_month = now.month();
    let cur_day = now.day();

    let cur_date_str = format!("{:04}-{:02}-{:02}", cur_year, cur_month, cur_day);
    let cur_month_str = format!("{:04}-{:02}", cur_year, cur_month);

    if is_daily {
        if let Some(task) = doc.daily.get_mut(task_idx) {
            if task.recurrent {
                let dates = task.completed_dates.get_or_insert_with(Vec::new);
                if let Some(pos) = dates.iter().position(|d| d == &cur_date_str) {
                    dates.remove(pos);
                } else {
                    dates.push(cur_date_str);
                }
            } else {
                task.completed = Some(!task.completed.unwrap_or(false));
            }
        }
    } else {
        if let Some(task) = doc.monthly.get_mut(task_idx) {
            if task.recurrent {
                let months = task.completed_months.get_or_insert_with(Vec::new);
                if let Some(pos) = months.iter().position(|m| m == &cur_month_str) {
                    months.remove(pos);
                } else {
                    months.push(cur_month_str);
                }
            } else {
                task.completed = Some(!task.completed.unwrap_or(false));
            }
        }
    }

    let url = format!("http://{}:{}/api/tasks/save", ip, port);
    match ureq::post(&url)
        .timeout(Duration::from_secs(3))
        .send_json(serde_json::to_value(&doc).unwrap_or_default())
    {
        Ok(_) => {
            app_log(&format!(
                "Task {}[{}] completion toggled",
                if is_daily { "daily" } else { "monthly" },
                task_idx
            ));
            true
        }
        Err(e) => {
            app_log(&format!("Failed to save tasks: {}", e));
            false
        }
    }
}

fn start_mdns(app: tauri::AppHandle) {
    thread::spawn(move || {
        let daemon = match ServiceDaemon::new() {
            Ok(d) => d,
            Err(e) => {
                app_log(&format!("mDNS daemon failed: {}", e));
                return;
            }
        };

        let receiver = match daemon.browse("_http._tcp.local.") {
            Ok(r) => r,
            Err(e) => {
                app_log(&format!("mDNS browse failed: {}", e));
                return;
            }
        };

        app_log("mDNS discovery thread started");

        loop {
            match receiver.recv() {
                Ok(ServiceEvent::ServiceResolved(info)) => {
                    let fullname = info.get_fullname();
                    if fullname.starts_with("deskbuddy.") {
                        if let Some(addr) = info.get_addresses_v4().iter().next() {
                            let ip = addr.to_string();
                            let port = info.get_port();
                            app_log(&format!("mDNS resolved DeskBuddy at {}:{}", ip, port));
                            let _ = app.emit(
                                "device-found",
                                serde_json::json!({"ip": ip, "port": port, "source": "mdns"}),
                            );
                        }
                    }
                }
                Ok(ServiceEvent::ServiceRemoved(instance_name, _)) => {
                    if instance_name.contains("deskbuddy") {
                        app_log("mDNS: DeskBuddy removed");
                        let _ = app
                            .emit("device-lost", serde_json::json!({"source": "mdns"}));
                    }
                }
                Ok(_) => {}
                Err(_) => {
                    app_log("mDNS receiver disconnected");
                    break;
                }
            }
        }

        app_log("mDNS discovery thread stopped");
    });
}

fn start_health_check(app: tauri::AppHandle, saved_ip: Arc<Mutex<Option<String>>>) {
    thread::spawn(move || {
        app_log("TCP health check thread started");
        loop {
            thread::sleep(Duration::from_secs(5));

            let ip_guard = saved_ip.lock().unwrap();
            if let Some(ref ip) = *ip_guard {
                if check_device(ip, 80) {
                    let _ = app.emit(
                        "device-found",
                        serde_json::json!({"ip": ip.clone(), "port": 80, "source": "tcp"}),
                    );
                }
            }
        }
    });
}

fn start_beacon_listener(app: tauri::AppHandle) {
    thread::spawn(move || {
        let socket = match UdpSocket::bind("0.0.0.0:42042") {
            Ok(s) => {
                let _ = s.set_read_timeout(Some(Duration::from_secs(5)));
                s
            }
            Err(e) => {
                app_log(&format!("UDP beacon bind failed: {}", e));
                return;
            }
        };

        app_log("UDP beacon listener started on port 42042");
        let mut buf = [0u8; 128];
        loop {
            match socket.recv_from(&mut buf) {
                Ok((len, src)) => {
                    let msg = String::from_utf8_lossy(&buf[..len]);
                    if msg.contains("deskbuddy") {
                        let ip = src.ip().to_string();
                        app_log(&format!("Beacon received from {}", ip));
                        let _ = app.emit(
                            "device-found",
                            serde_json::json!({"ip": ip, "port": 80, "source": "beacon"}),
                        );
                    }
                }
                Err(ref e)
                    if e.kind() == std::io::ErrorKind::WouldBlock
                        || e.kind() == std::io::ErrorKind::TimedOut =>
                {
                    continue;
                }
                Err(e) => {
                    app_log(&format!("Beacon socket error: {}", e));
                    break;
                }
            }
        }
        app_log("UDP beacon listener stopped");
    });
}

fn start_status_poller(
    app: tauri::AppHandle,
    device_state: Arc<Mutex<Option<DeviceInfo>>>,
    live_state: Arc<Mutex<LiveBuddyState>>,
) {
    thread::spawn(move || {
        app_log("Status poller thread started");
        loop {
            thread::sleep(Duration::from_secs(4));

            let device_opt = device_state.lock().unwrap().clone();
            if let Some(dev) = device_opt {
                if let Some(radar) = fetch_radar_data(&dev.ip, dev.port) {
                    let todo = fetch_tasks_data(&dev.ip, dev.port).unwrap_or_default();
                    let due_tasks = compute_due_tasks(&todo);

                    {
                        let mut state = live_state.lock().unwrap();
                        state.radar = radar;
                        state.todo = todo;
                        state.due_tasks = due_tasks;
                    }

                    let _ = app.emit("data-updated", ());
                }
            }
        }
    });
}

fn build_menu(
    app: &tauri::AppHandle,
    device: Option<&DeviceInfo>,
    live: &LiveBuddyState,
) -> Result<tauri::menu::Menu<tauri::Wry>, tauri::Error> {
    let mut builder = MenuBuilder::new(app);

    // Detect language: firmware lang field ("pt" or "en"), or check firstSit fallback
    let is_pt = live.radar.lang.as_deref() == Some("pt")
        || live.radar.language.as_deref() == Some("pt-BR")
        || live
            .radar
            .first_sit_time
            .as_ref()
            .map(|s| s.contains("Não") || s.contains("registrado"))
            .unwrap_or(true); // Default to Portuguese per user preference

    // ==========================================
    // SECTION 1: TOP MOST (OPEN TODO PAGE)
    // ==========================================
    let todo_label = if is_pt {
        "📝 Abrir Página de Tarefas (TODO)"
    } else {
        "📝 Open TODO Page"
    };
    let open_todo_item = MenuItemBuilder::with_id("open-todo", todo_label)
        .enabled(device.is_some())
        .build(app)?;
    builder = builder.item(&open_todo_item).separator();

    if device.is_some() {
        // ==========================================
        // SECTION 2: DAILY PERFORMANCE STATS
        // ==========================================
        let desk_time_val = live.radar.desk_time.as_deref().unwrap_or("0m");
        let focus_time_val = live.radar.focus_time.as_deref().unwrap_or("0m");
        let break_time_val = live.radar.break_time.as_deref().unwrap_or("0m");
        let breaks_val = live.radar.breaks.unwrap_or(0);
        let latest_break_val = live.radar.latest_break.as_deref().unwrap_or("0m");
        let first_sit_val = live
            .radar
            .first_sit_time
            .as_deref()
            .unwrap_or(if is_pt { "Não registrado" } else { "Not registered" });
        let longest_streak_val = live.radar.longest_streak.as_deref().unwrap_or("0m");
        let score_val = live.radar.score.unwrap_or(0);

        let (
            l_desk,
            l_focus,
            l_break,
            l_breaks_cnt,
            l_latest_break,
            l_first_sit,
            l_longest_streak,
            l_score,
        ) = if is_pt {
            (
                format!("🪑 Tempo na Mesa: {}", desk_time_val),
                format!("🎯 Foco Profundo: {}", focus_time_val),
                format!("☕ Tempo em Pausas: {}", break_time_val),
                format!("📊 Total de Pausas: {}", breaks_val),
                format!("⏳ Duração Última Pausa: {}", latest_break_val),
                format!("🌅 Primeira Sessão: {}", first_sit_val),
                format!("🔥 Maior Sequência: {}", longest_streak_val),
                format!("⭐ Produtividade: {}%", score_val),
            )
        } else {
            (
                format!("🪑 Time at Desk: {}", desk_time_val),
                format!("🎯 Deep Focus: {}", focus_time_val),
                format!("☕ Time on Breaks: {}", break_time_val),
                format!("📊 Total Breaks: {}", breaks_val),
                format!("⏳ Latest Break Duration: {}", latest_break_val),
                format!("🌅 First Sitting Time: {}", first_sit_val),
                format!("🔥 Longest Streak: {}", longest_streak_val),
                format!("⭐ Productivity Score: {}%", score_val),
            )
        };

        let stat_items = [
            MenuItemBuilder::with_id("stat-desk", l_desk).enabled(false).build(app)?,
            MenuItemBuilder::with_id("stat-focus", l_focus).enabled(false).build(app)?,
            MenuItemBuilder::with_id("stat-break", l_break).enabled(false).build(app)?,
            MenuItemBuilder::with_id("stat-breaks-cnt", l_breaks_cnt).enabled(false).build(app)?,
            MenuItemBuilder::with_id("stat-latest-break", l_latest_break).enabled(false).build(app)?,
            MenuItemBuilder::with_id("stat-first-sit", l_first_sit).enabled(false).build(app)?,
            MenuItemBuilder::with_id("stat-longest-streak", l_longest_streak).enabled(false).build(app)?,
            MenuItemBuilder::with_id("stat-score", l_score).enabled(false).build(app)?,
        ];

        for item in &stat_items {
            builder = builder.item(item);
        }
        builder = builder.separator();

        // ==========================================
        // SECTION 3: TRIP ODOMETERS (4 ODOS + SWITCH)
        // ==========================================
        let active_odo = live.radar.active_odometer.unwrap_or(0);
        let default_labels = if is_pt {
            vec!["Trabalho".into(), "Estudo".into(), "Odômetro 3".into(), "Odômetro 4".into()]
        } else {
            vec!["Work".into(), "Study".into(), "Odometer 3".into(), "Odometer 4".into()]
        };
        let labels = live.radar.odometer_labels.as_ref().unwrap_or(&default_labels);
        let default_fmts = vec!["00:00:00".into(), "00:00:00".into(), "00:00:00".into(), "00:00:00".into()];
        let fmts = live.radar.odometer_fmt.as_ref().unwrap_or(&default_fmts);

        for i in 0..4 {
            let label = labels.get(i).cloned().unwrap_or_else(|| format!("Odo {}", i + 1));
            let time_str = fmts.get(i).cloned().unwrap_or_else(|| "00:00:00".into());
            let marker = if i == active_odo { "▶ 🟢" } else { "   ⚪" };
            let menu_text = format!("{} {}: {}", marker, label, time_str);

            let odo_item = MenuItemBuilder::with_id(format!("odo-{}", i), menu_text).build(app)?;
            builder = builder.item(&odo_item);
        }
        builder = builder.separator();

        // ==========================================
        // SECTION 4: DUE TODAY & THIS MONTH TASKS
        // ==========================================
        if live.due_tasks.is_empty() {
            let empty_label = if is_pt {
                "Nenhuma tarefa pendente hoje/mês"
            } else {
                "No pending tasks today/this month"
            };
            let empty_item = MenuItemBuilder::with_id("tasks-empty", empty_label)
                .enabled(false)
                .build(app)?;
            builder = builder.item(&empty_item);
        } else {
            for (display_idx, task) in live.due_tasks.iter().enumerate() {
                let id = format!("task-toggle-{}", display_idx);
                let task_item = MenuItemBuilder::with_id(id, &task.display_label).build(app)?;
                builder = builder.item(&task_item);
            }
        }
        builder = builder.separator();
    }

    // ==========================================
    // FOOTER: DEVICE STATUS & ACTIONS
    // ==========================================
    let status_text = match device {
        Some(info) => format!("DeskBuddy at {}:{}", info.ip, info.port),
        None => if is_pt {
            "Procurando DeskBuddy...".into()
        } else {
            "Searching for DeskBuddy...".into()
        },
    };

    let (l_open_dash, l_set_ip, l_refresh, l_quit) = if is_pt {
        (
            "Abrir Painel Principal",
            "Definir endereço IP...",
            "Atualizar",
            "Sair",
        )
    } else {
        (
            "Open Dashboard",
            "Set IP address...",
            "Refresh",
            "Quit",
        )
    };

    let status_item = MenuItemBuilder::with_id("status", status_text)
        .enabled(false)
        .build(app)?;
    let open_item = MenuItemBuilder::with_id("open", l_open_dash)
        .enabled(device.is_some())
        .build(app)?;
    let set_ip_item = MenuItemBuilder::with_id("set-ip", l_set_ip).build(app)?;
    let refresh_item = MenuItemBuilder::with_id("refresh", l_refresh).build(app)?;
    let quit_item = MenuItemBuilder::with_id("quit", l_quit).build(app)?;

    builder
        .item(&status_item)
        .separator()
        .item(&open_item)
        .separator()
        .item(&set_ip_item)
        .item(&refresh_item)
        .separator()
        .item(&quit_item)
        .build()
}

fn open_dashboard(device: Option<&DeviceInfo>, subpath: Option<&str>) {
    let path = subpath.unwrap_or("");
    match device {
        Some(info) => {
            let url = format!("http://{}:{}/{}", info.ip, info.port, path);
            log::info!("Opening {}", url);
            if let Err(e) = open::that(&url) {
                log::error!("Failed to open browser: {}", e);
                let fallback = format!("http://deskbuddy.local/{}", path);
                let _ = open::that(&fallback);
            }
        }
        None => {
            log::info!("No device found, trying mDNS hostname");
            let fallback = format!("http://deskbuddy.local/{}", path);
            let _ = open::that(&fallback);
        }
    }
}

fn open_settings_window(app: &tauri::AppHandle) {
    if let Some(w) = app.get_webview_window("settings") {
        let _ = w.show();
        let _ = w.set_focus();
        return;
    }
    let _ = WebviewWindowBuilder::new(app, "settings", WebviewUrl::App("settings.html".into()))
        .title("DeskBuddy IP Address")
        .inner_size(440.0, 220.0)
        .resizable(false)
        .maximizable(false)
        .minimizable(false)
        .build();
}

#[derive(Clone, Debug, Serialize)]
struct AppFullState {
    device: Option<DeviceInfo>,
    live: LiveBuddyState,
}

#[tauri::command]
fn get_state_cmd(
    device_state: tauri::State<'_, Arc<Mutex<Option<DeviceInfo>>>>,
    live_state: tauri::State<'_, Arc<Mutex<LiveBuddyState>>>,
) -> Result<AppFullState, String> {
    let device = device_state.lock().unwrap().clone();
    let live = live_state.lock().unwrap().clone();
    Ok(AppFullState { device, live })
}

#[tauri::command]
fn set_odometer_cmd(
    idx: usize,
    app: tauri::AppHandle,
    device_state: tauri::State<'_, Arc<Mutex<Option<DeviceInfo>>>>,
    live_state: tauri::State<'_, Arc<Mutex<LiveBuddyState>>>,
) -> Result<(), String> {
    // 1. Optimistic update
    {
        let mut state = live_state.lock().unwrap();
        state.radar.active_odometer = Some(idx);
    }
    let _ = app.emit("data-updated", ());

    // 2. Perform HTTP request in background
    let device_opt = device_state.lock().unwrap().clone();
    if let Some(dev) = device_opt {
        let app_h = app.clone();
        let live_st = live_state.inner().clone();
        thread::spawn(move || {
            if set_active_odometer(&dev.ip, dev.port, idx) {
                if let Some(radar) = fetch_radar_data(&dev.ip, dev.port) {
                    let mut state = live_st.lock().unwrap();
                    state.radar = radar;
                }
                let _ = app_h.emit("data-updated", ());
            }
        });
    }
    Ok(())
}

#[tauri::command]
fn complete_task_cmd(
    disp_idx: usize,
    is_daily: bool,
    orig_index: usize,
    app: tauri::AppHandle,
    device_state: tauri::State<'_, Arc<Mutex<Option<DeviceInfo>>>>,
    live_state: tauri::State<'_, Arc<Mutex<LiveBuddyState>>>,
) -> Result<(), String> {
    // 1. Optimistic removal from due tasks
    {
        let mut state = live_state.lock().unwrap();
        if disp_idx < state.due_tasks.len() {
            state.due_tasks.remove(disp_idx);
        }
    }
    let _ = app.emit("data-updated", ());

    // 2. Background HTTP request
    let device_opt = device_state.lock().unwrap().clone();
    if let Some(dev) = device_opt {
        let app_h = app.clone();
        let live_st = live_state.inner().clone();
        thread::spawn(move || {
            if toggle_task_completion(&dev.ip, dev.port, is_daily, orig_index) {
                if let Some(todo) = fetch_tasks_data(&dev.ip, dev.port) {
                    let due_tasks = compute_due_tasks(&todo);
                    let mut state = live_st.lock().unwrap();
                    state.todo = todo;
                    state.due_tasks = due_tasks;
                }
                let _ = app_h.emit("data-updated", ());
            }
        });
    }
    Ok(())
}

#[tauri::command]
fn open_url_cmd(
    subpath: Option<String>,
    app: tauri::AppHandle,
    device_state: tauri::State<'_, Arc<Mutex<Option<DeviceInfo>>>>,
) -> Result<(), String> {
    let device = device_state.lock().unwrap().clone();
    open_dashboard(device.as_ref(), subpath.as_deref());
    if let Some(w) = app.get_webview_window("main") {
        let _ = w.hide();
    }
    Ok(())
}

#[tauri::command]
fn open_settings_cmd(app: tauri::AppHandle) -> Result<(), String> {
    open_settings_window(&app);
    Ok(())
}

#[tauri::command]
fn refresh_data_cmd(app: tauri::AppHandle) -> Result<(), String> {
    let _ = app.emit("do-refresh", ());
    Ok(())
}

#[tauri::command]
fn hide_popup_cmd(app: tauri::AppHandle) -> Result<(), String> {
    if let Some(w) = app.get_webview_window("main") {
        let _ = w.hide();
    }
    Ok(())
}

#[tauri::command]
fn quit_app_cmd(app: tauri::AppHandle) -> Result<(), String> {
    app.exit(0);
    Ok(())
}

#[tauri::command]
fn save_ip_cmd(ip: String, app: tauri::AppHandle) -> Result<(), String> {
    let mut s = load_settings();
    s.deskbuddy_ip = if ip.is_empty() { None } else { Some(ip.clone()) };
    save_settings(&s);
    app_log(&format!("Manual IP saved: {}", ip));
    let _ = app.emit("ip-saved", serde_json::json!({"ip": ip}));
    Ok(())
}

#[tauri::command]
fn get_saved_ip_cmd() -> Result<Option<String>, String> {
    Ok(load_settings().deskbuddy_ip)
}

#[tauri::command]
fn close_settings_window(app: tauri::AppHandle) -> Result<(), String> {
    if let Some(w) = app.get_webview_window("settings") {
        let _ = w.close();
    }
    Ok(())
}

fn position_flyout_window(win: &tauri::WebviewWindow, position: Option<tauri::PhysicalPosition<f64>>) {
    if let Ok(monitor) = win.primary_monitor() {
        if let Some(mon) = monitor {
            let screen_size = mon.size();
            let win_width = 380;
            let win_height = 620;

            let (target_x, target_y) = if let Some(pos) = position {
                // Tray position known
                let x = (pos.x as i32 - win_width / 2).max(10).min(screen_size.width as i32 - win_width - 10);
                let y = if (pos.y as i32) > (screen_size.height as i32 / 2) {
                    pos.y as i32 - win_height - 10
                } else {
                    pos.y as i32 + 10
                };
                (x, y)
            } else {
                // Bottom-right corner fallback (typical for Windows taskbar)
                let x = screen_size.width as i32 - win_width - 16;
                let y = screen_size.height as i32 - win_height - 48;
                (x, y)
            };

            let _ = win.set_position(tauri::Position::Physical(tauri::PhysicalPosition {
                x: target_x,
                y: target_y,
            }));
        }
    }
}

pub fn run() {
    env_logger::init();
    app_log("DeskBuddy Companion starting");

    let settings = load_settings();
    let saved_ip: Arc<Mutex<Option<String>>> =
        Arc::new(Mutex::new(settings.deskbuddy_ip.clone()));

    if let Some(ref ip) = settings.deskbuddy_ip {
        app_log(&format!("Loaded saved IP: {}", ip));
    } else {
        app_log("No saved IP configured");
    }

    let quit_requested = Arc::new(AtomicBool::new(false));
    let quit_for_run = quit_requested.clone();

    let device_state: Arc<Mutex<Option<DeviceInfo>>> = Arc::new(Mutex::new(None));
    let live_state: Arc<Mutex<LiveBuddyState>> = Arc::new(Mutex::new(LiveBuddyState::default()));

    let device_state_for_setup = device_state.clone();
    let live_state_for_setup = live_state.clone();

    let app = tauri::Builder::default()
        .manage(device_state)
        .manage(live_state)
        .invoke_handler(tauri::generate_handler![
            get_state_cmd,
            set_odometer_cmd,
            complete_task_cmd,
            open_url_cmd,
            open_settings_cmd,
            refresh_data_cmd,
            hide_popup_cmd,
            quit_app_cmd,
            save_ip_cmd,
            get_saved_ip_cmd,
            close_settings_window
        ])
        .setup(move |app| {
            let handle = app.handle().clone();
            let device_state = device_state_for_setup;
            let live_state = live_state_for_setup;

            let green_icon = load_icon(ICON_GREEN).expect("Failed to load green icon");
            let gray_icon = load_icon(ICON_GRAY).expect("Failed to load gray icon");

            let initial_menu = build_menu(&handle, None, &LiveBuddyState::default())?;
            let handle2 = handle.clone();
            let handle3 = handle.clone();
            let handle_poller = handle.clone();
            let handle_refresh = handle.clone();
            let device_state_found = device_state.clone();
            let device_state_lost = device_state.clone();
            let device_state_updated = device_state.clone();
            let device_state_refresh = device_state.clone();
            let device_state_poller = device_state.clone();
            let live_state_found = live_state.clone();
            let live_state_lost = live_state.clone();
            let live_state_updated = live_state.clone();
            let live_state_refresh = live_state.clone();
            let live_state_poller = live_state.clone();
            let green_icon2 = green_icon.clone();
            let gray_icon2 = gray_icon.clone();
            let saved_ip2 = saved_ip.clone();

            let tray = TrayIconBuilder::with_id("deskbuddy-tray")
                .icon(gray_icon.clone())
                .menu(&initial_menu)
                .tooltip("DeskBuddy Companion")
                .on_tray_icon_event({
                    let handle = handle.clone();
                    move |_tray, event| {
                        match event {
                            TrayIconEvent::Click {
                                button: MouseButton::Left | MouseButton::Right,
                                button_state: MouseButtonState::Up,
                                position,
                                ..
                            } => {
                                if let Some(w) = handle.get_webview_window("main") {
                                    if w.is_visible().unwrap_or(false) {
                                        let _ = w.hide();
                                    } else {
                                        position_flyout_window(&w, Some(position));
                                        let _ = w.show();
                                        let _ = w.set_focus();
                                        let _ = handle.emit("popup-opened", ());
                                    }
                                }
                            }
                            _ => {}
                        }
                    }
                })
                .build(app)?;

            // Register auto-hide when main flyout window loses focus
            if let Some(main_win) = handle.get_webview_window("main") {
                let win_clone = main_win.clone();
                main_win.on_window_event(move |event| {
                    if let WindowEvent::Focused(false) = event {
                        let _ = win_clone.hide();
                    }
                });
            }

            let tray2 = tray.clone();
            let tray_poller = tray.clone();

            app.listen("device-found", {
                let tray = tray.clone();
                let device_state = device_state_found;
                let green_icon = green_icon2.clone();
                let handle = handle2.clone();
                let live_state = live_state_found;

                move |event| {
                    if let Ok(payload) =
                        serde_json::from_str::<serde_json::Value>(event.payload())
                    {
                        if let (Some(ip), Some(port)) = (
                            payload.get("ip").and_then(|v| v.as_str()),
                            payload.get("port").and_then(|v| v.as_u64()),
                        ) {
                            let source = payload
                                .get("source")
                                .and_then(|v| v.as_str())
                                .unwrap_or("?");
                            app_log(&format!(
                                "Device found via {}: {}:{}",
                                source, ip, port
                            ));

                            let info = DeviceInfo {
                                ip: ip.to_string(),
                                port: port as u16,
                            };
                            *device_state.lock().unwrap() = Some(info.clone());

                            // Fetch immediate data
                            let dev_ip = info.ip.clone();
                            let dev_port = info.port;
                            let app_h = handle.clone();
                            let live_st = live_state.clone();
                            let dev_st = device_state.clone();
                            let tray_h = tray.clone();
                            let g_icon = green_icon.clone();
                            thread::spawn(move || {
                                if let Some(radar) = fetch_radar_data(&dev_ip, dev_port) {
                                    let todo = fetch_tasks_data(&dev_ip, dev_port).unwrap_or_default();
                                    let due_tasks = compute_due_tasks(&todo);
                                    {
                                        let mut state = live_st.lock().unwrap();
                                        state.radar = radar;
                                        state.todo = todo;
                                        state.due_tasks = due_tasks;
                                    }
                                }
                                let dev = dev_st.lock().unwrap().clone();
                                let live = live_st.lock().unwrap().clone();
                                if let Ok(menu) = build_menu(&app_h, dev.as_ref(), &live) {
                                    let _ = tray_h.set_menu(Some(menu));
                                }
                                let _ = tray_h.set_icon(Some(g_icon));
                                let _ = tray_h.set_tooltip(Some(
                                    format!("DeskBuddy at {}:{}", dev_ip, dev_port).as_str(),
                                ));
                                let _ = app_h.emit("data-updated", ());
                                let _ = app_h.emit("device-found-ui", ());
                            });
                        }
                    }
                }
            });

            app.listen("device-lost", {
                let tray = tray2.clone();
                let device_state = device_state_lost;
                let gray_icon = gray_icon2.clone();
                let handle = handle2.clone();
                let live_state = live_state_lost;

                move |_event| {
                    app_log("Device lost");
                    *device_state.lock().unwrap() = None;
                    let live = live_state.lock().unwrap().clone();
                    if let Ok(menu) = build_menu(&handle, None, &live) {
                        let _ = tray.set_menu(Some(menu));
                    }
                    let _ = tray.set_icon(Some(gray_icon.clone()));
                    let _ = tray.set_tooltip(Some("DeskBuddy not found"));
                    let _ = handle.emit("data-updated", ());
                }
            });

            app.listen("data-updated", {
                let tray = tray_poller.clone();
                let device_state = device_state_updated;
                let live_state = live_state_updated;
                let handle = handle_refresh.clone();

                move |_event| {
                    let dev = device_state.lock().unwrap().clone();
                    let live = live_state.lock().unwrap().clone();
                    if let Ok(menu) = build_menu(&handle, dev.as_ref(), &live) {
                        let _ = tray.set_menu(Some(menu));
                    }
                }
            });

            app.listen("do-refresh", {
                let tray = tray2.clone();
                let device_state = device_state_refresh;
                let gray_icon = gray_icon2.clone();
                let handle = handle2.clone();
                let live_state = live_state_refresh;

                move |_event| {
                    app_log("Manual refresh triggered");
                    let dev_opt = device_state.lock().unwrap().clone();
                    if let Some(dev) = dev_opt {
                        let app_h = handle.clone();
                        let live_st = live_state.clone();
                        let dev_st = device_state.clone();
                        let tray_h = tray.clone();
                        thread::spawn(move || {
                            if let Some(radar) = fetch_radar_data(&dev.ip, dev.port) {
                                let todo = fetch_tasks_data(&dev.ip, dev.port).unwrap_or_default();
                                let due_tasks = compute_due_tasks(&todo);
                                {
                                    let mut state = live_st.lock().unwrap();
                                    state.radar = radar;
                                    state.todo = todo;
                                    state.due_tasks = due_tasks;
                                }
                            }
                            let dev_cur = dev_st.lock().unwrap().clone();
                            let live_cur = live_st.lock().unwrap().clone();
                            if let Ok(menu) = build_menu(&app_h, dev_cur.as_ref(), &live_cur) {
                                let _ = tray_h.set_menu(Some(menu));
                            }
                            let _ = app_h.emit("data-updated", ());
                        });
                    } else {
                        let live = live_state.lock().unwrap().clone();
                        if let Ok(menu) = build_menu(&handle, None, &live) {
                            let _ = tray.set_menu(Some(menu));
                        }
                        let _ = tray.set_icon(Some(gray_icon.clone()));
                        let _ = tray.set_tooltip(Some("Refreshing..."));
                    }
                }
            });

            // Listen for IP updates from the settings window
            app.listen("ip-saved", {
                let saved_ip = saved_ip.clone();
                let handle = handle.clone();
                move |event| {
                    if let Ok(payload) =
                        serde_json::from_str::<serde_json::Value>(event.payload())
                    {
                        if let Some(ip) = payload.get("ip").and_then(|v| v.as_str()) {
                            app_log(&format!("IP updated via settings window: {}", ip));
                            *saved_ip.lock().unwrap() = Some(ip.to_string());
                            // Trigger immediate health check
                            if check_device(ip, 80) {
                                let _ = handle.emit(
                                    "device-found",
                                    serde_json::json!({"ip": ip, "port": 80, "source": "tcp"}),
                                );
                            }
                        }
                    }
                }
            });

            start_mdns(handle2);
            start_health_check(handle, saved_ip2);
            start_beacon_listener(handle3);
            start_status_poller(handle_poller, device_state_poller, live_state_poller);

            Ok(())
        })
        .build(tauri::generate_context!())
        .expect("error while building tauri application");

    app.run(move |_handle, event| {
        if let RunEvent::ExitRequested { api, .. } = event {
            if !quit_for_run.load(Ordering::SeqCst) {
                api.prevent_exit();
            }
        }
    });
}
