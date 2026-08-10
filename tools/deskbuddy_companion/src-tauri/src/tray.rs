use std::fs;
use std::io::{Cursor, Write};
use std::net::{SocketAddr, TcpStream, UdpSocket};
use std::path::PathBuf;
use std::sync::{Arc, atomic::{AtomicBool, Ordering}, Mutex};
use std::thread;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

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

#[derive(Clone)]
struct DeviceInfo {
    ip: String,
    port: u16,
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

fn build_menu(
    app: &tauri::AppHandle,
    device: Option<&DeviceInfo>,
) -> Result<tauri::menu::Menu<tauri::Wry>, tauri::Error> {
    let status_text = match device {
        Some(info) => format!("DeskBuddy at {}:{}", info.ip, info.port),
        None => "Searching for DeskBuddy...".into(),
    };

    let status_item = MenuItemBuilder::with_id("status", status_text)
        .enabled(false)
        .build(app)?;
    let open_item = MenuItemBuilder::with_id("open", "Open Dashboard")
        .enabled(device.is_some())
        .build(app)?;
    let set_ip_item = MenuItemBuilder::with_id("set-ip", "Set IP address...").build(app)?;
    let refresh_item = MenuItemBuilder::with_id("refresh", "Refresh").build(app)?;
    let quit_item = MenuItemBuilder::with_id("quit", "Quit").build(app)?;

    MenuBuilder::new(app)
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

fn open_dashboard(device: Option<&DeviceInfo>) {
    match device {
        Some(info) => {
            let url = format!("http://{}:{}/", info.ip, info.port);
            log::info!("Opening {}", url);
            if let Err(e) = open::that(&url) {
                log::error!("Failed to open browser: {}", e);
                let _ = open::that("http://deskbuddy.local/");
            }
        }
        None => {
            log::info!("No device found, trying mDNS hostname");
            let _ = open::that("http://deskbuddy.local/");
        }
    }
}

fn open_settings_window(app: &tauri::AppHandle) {
    if let Some(w) = app.get_webview_window("settings") {
        let _ = w.destroy();
    }
    let Ok(w) = WebviewWindowBuilder::new(app, "settings", WebviewUrl::App("settings.html".into()))
        .title("DeskBuddy IP Address")
        .inner_size(420.0, 200.0)
        .resizable(false)
        .maximizable(false)
        .minimizable(false)
        .build() else { return };

    let win = w.clone();
    let _ = w.on_window_event(move |event| {
        if let WindowEvent::CloseRequested { .. } = event {
            let _ = win.destroy();
        }
    });
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
        let _ = w.destroy();
    }
    Ok(())
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
    let quit_for_setup = quit_requested.clone();
    let quit_for_run = quit_requested.clone();

    let app = tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![save_ip_cmd, get_saved_ip_cmd, close_settings_window])
        .setup(move |app| {
            let handle = app.handle().clone();
            let device_state: Arc<Mutex<Option<DeviceInfo>>> = Arc::new(Mutex::new(None));
            let green_icon = load_icon(ICON_GREEN).expect("Failed to load green icon");
            let gray_icon = load_icon(ICON_GRAY).expect("Failed to load gray icon");

            let initial_menu = build_menu(&handle, None)?;
            let handle2 = handle.clone();
            let handle3 = handle.clone();
            let device_state2 = device_state.clone();
            let green_icon2 = green_icon.clone();
            let gray_icon2 = gray_icon.clone();
            let saved_ip2 = saved_ip.clone();

            let tray = TrayIconBuilder::with_id("deskbuddy-tray")
                .icon(gray_icon.clone())
                .menu(&initial_menu)
                .tooltip("DeskBuddy Companion")
                .on_menu_event({
                    let device_state = device_state.clone();
                    let handle = handle.clone();
                    let quit = quit_for_setup.clone();
                    move |app, event| {
                        match event.id().as_ref() {
                            "open" => {
                                let device = device_state.lock().unwrap();
                                open_dashboard(device.as_ref());
                            }
                            "set-ip" => {
                                open_settings_window(&handle);
                            }
                            "refresh" => {
                                let _ = app.emit("do-refresh", ());
                            }
                            "quit" => {
                                quit.store(true, Ordering::SeqCst);
                                app.exit(0);
                            }
                            _ => {}
                        }
                    }
                })
                .on_tray_icon_event({
                    let device_state = device_state.clone();
                    move |_tray, event| {
                        if let TrayIconEvent::Click {
                            button: MouseButton::Left,
                            button_state: MouseButtonState::Up,
                            ..
                        } = event
                        {
                            let device = device_state.lock().unwrap();
                            open_dashboard(device.as_ref());
                        }
                    }
                })
                .build(app)?;

            let tray2 = tray.clone();

            app.listen("device-found", {
                let tray = tray.clone();
                let device_state = device_state2.clone();
                let green_icon = green_icon2.clone();
                let handle = handle2.clone();

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

                            if let Ok(menu) = build_menu(&handle, Some(&info)) {
                                let _ = tray.set_menu(Some(menu));
                            }
                            let _ = tray.set_icon(Some(green_icon.clone()));
                            let _ = tray.set_tooltip(Some(
                                format!("DeskBuddy at {}:{}", info.ip, info.port).as_str(),
                            ));
                        }
                    }
                }
            });

            app.listen("device-lost", {
                let tray = tray2.clone();
                let device_state = device_state.clone();
                let gray_icon = gray_icon2.clone();
                let handle = handle2.clone();

                move |_event| {
                    app_log("Device lost");
                    *device_state.lock().unwrap() = None;
                    if let Ok(menu) = build_menu(&handle, None) {
                        let _ = tray.set_menu(Some(menu));
                    }
                    let _ = tray.set_icon(Some(gray_icon.clone()));
                    let _ = tray.set_tooltip(Some("DeskBuddy not found"));
                }
            });

            app.listen("do-refresh", {
                let tray = tray2.clone();
                let device_state = device_state.clone();
                let gray_icon = gray_icon2.clone();
                let handle = handle2.clone();

                move |_event| {
                    app_log("Manual refresh triggered");
                    *device_state.lock().unwrap() = None;
                    if let Ok(menu) = build_menu(&handle, None) {
                        let _ = tray.set_menu(Some(menu));
                    }
                    let _ = tray.set_icon(Some(gray_icon.clone()));
                    let _ = tray.set_tooltip(Some("Refreshing..."));
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
