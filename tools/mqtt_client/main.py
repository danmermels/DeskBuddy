import json
import time
import os
import threading
import paho.mqtt.client as mqtt
import customtkinter as ctk

# Configure CustomTkinter appearance
ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")

CONFIG_FILE = "config.json"
LOG_FILE = "deskbuddy_logs.txt"

class EditButtonDialog(ctk.CTkToplevel):
    def __init__(self, parent, button_idx, current_cfg, save_callback):
        super().__init__(parent)
        self.button_idx = button_idx
        self.save_callback = save_callback

        self.title(f"Edit Button {button_idx + 1}")
        self.geometry("380x300")
        self.resizable(False, False)
        
        # Grab focus and make it modal
        self.transient(parent)
        self.grab_set()

        # Labels & Entries
        self.label_lbl = ctk.CTkLabel(self, text="Button Label:")
        self.label_lbl.pack(padx=20, pady=(15, 0), anchor="w")
        self.label_entry = ctk.CTkEntry(self, width=340)
        self.label_entry.insert(0, current_cfg.get("label", ""))
        self.label_entry.pack(padx=20, pady=5)

        self.topic_lbl = ctk.CTkLabel(self, text="MQTT Topic:")
        self.topic_lbl.pack(padx=20, pady=5, anchor="w")
        self.topic_entry = ctk.CTkEntry(self, width=340)
        self.topic_entry.insert(0, current_cfg.get("topic", ""))
        self.topic_entry.pack(padx=20, pady=5)

        self.payload_lbl = ctk.CTkLabel(self, text="Payload:")
        self.payload_lbl.pack(padx=20, pady=5, anchor="w")
        self.payload_entry = ctk.CTkEntry(self, width=340)
        self.payload_entry.insert(0, current_cfg.get("payload", ""))
        self.payload_entry.pack(padx=20, pady=5)

        # Buttons
        self.btn_frame = ctk.CTkFrame(self, fg_color="transparent")
        self.btn_frame.pack(padx=20, pady=20, fill="x")

        self.save_btn = ctk.CTkButton(self.btn_frame, text="Save", command=self.save, width=100)
        self.save_btn.pack(side="right", padx=(10, 0))

        self.cancel_btn = ctk.CTkButton(self.btn_frame, text="Cancel", command=self.destroy, fg_color="#475569", width=100)
        self.cancel_btn.pack(side="right")

    def save(self):
        new_cfg = {
            "label": self.label_entry.get(),
            "topic": self.topic_entry.get(),
            "payload": self.payload_entry.get()
        }
        self.save_callback(self.button_idx, new_cfg)
        self.destroy()


class DeskBuddyMqttClient(ctk.CTk):
    def __init__(self):
        super().__init__()

        self.title("DeskBuddy MQTT Monitor & Controller")
        self.geometry("1200x800")

        self.mqtt_client = mqtt.Client()
        self.mqtt_client.on_connect = self.on_connect
        self.mqtt_client.on_message = self.on_message
        
        self.load_config()

        # Grid Layout: Left (Main Panel) vs Right (System Logs narrow panel)
        self.grid_columnconfigure(0, weight=4) # Main Panel (DeskBuddy logs, Custom listener 1 & 2, Command matrix)
        self.grid_columnconfigure(1, weight=1) # Narrow Right Panel (MQTT App Logs + Connection Controls)
        self.grid_rowconfigure(0, weight=1)

        self.setup_ui()
        
        # Connect to broker on startup after UI initializes
        self.after(500, self.connect_on_startup)

    def load_config(self):
        if os.path.exists(CONFIG_FILE):
            with open(CONFIG_FILE, 'r') as f:
                try:
                    self.config = json.load(f)
                except Exception:
                    self.config = {}
        else:
            self.config = {}

        # Default fallback values
        if "broker_ip" not in self.config:
            self.config["broker_ip"] = "192.168.1.100"
        if "broker_port" not in self.config:
            self.config["broker_port"] = 1883
        if "topic_listener_1" not in self.config:
            self.config["topic_listener_1"] = "deskbuddy/telemetry"
        if "topic_listener_2" not in self.config:
            self.config["topic_listener_2"] = "deskbuddy/state"
        if "buttons" not in self.config:
            self.config["buttons"] = [{"label": f"Empty {i+1}", "topic": "test", "payload": ""} for i in range(30)]

    def save_config_to_file(self):
        with open(CONFIG_FILE, 'w') as f:
            json.dump(self.config, f, indent=4)

    def setup_ui(self):
        # ==========================================
        # LEFT PANEL (Main Area)
        # ==========================================
        self.main_frame = ctk.CTkFrame(self)
        self.main_frame.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")
        self.main_frame.grid_columnconfigure(0, weight=1)
        self.main_frame.grid_rowconfigure(0, weight=3) # DeskBuddy Logs
        self.main_frame.grid_rowconfigure(1, weight=2) # Custom Listeners
        self.main_frame.grid_rowconfigure(2, weight=3) # Command Matrix

        # 1. DeskBuddy Log Panel
        self.db_log_frame = ctk.CTkFrame(self.main_frame)
        self.db_log_frame.grid(row=0, column=0, padx=10, pady=(10, 5), sticky="nsew")
        self.db_log_frame.grid_rowconfigure(1, weight=1)
        self.db_log_frame.grid_columnconfigure(0, weight=1)

        self.db_log_header = ctk.CTkLabel(self.db_log_frame, text="DeskBuddy Logs (deskbuddy/log/#)", font=ctk.CTkFont(size=14, weight="bold"))
        self.db_log_header.grid(row=0, column=0, padx=10, pady=5, sticky="w")

        self.db_log_textbox = ctk.CTkTextbox(self.db_log_frame, state="disabled")
        self.db_log_textbox.grid(row=1, column=0, padx=10, pady=(0, 10), sticky="nsew")
        
        # Color coding tags for DeskBuddy logs
        self.db_log_textbox.tag_config("SYSTEM", foreground="#38bdf8")
        self.db_log_textbox.tag_config("ERROR", foreground="#ef4444")
        self.db_log_textbox.tag_config("WIFI", foreground="#22c55e")
        self.db_log_textbox.tag_config("MQTT", foreground="#eab308")
        self.db_log_textbox.tag_config("DEFAULT", foreground="#f8fafc")

        # 2. Two Custom Topic Listeners side-by-side
        self.listeners_frame = ctk.CTkFrame(self.main_frame, fg_color="transparent")
        self.listeners_frame.grid(row=1, column=0, padx=10, pady=5, sticky="nsew")
        self.listeners_frame.grid_columnconfigure(0, weight=1)
        self.listeners_frame.grid_columnconfigure(1, weight=4)
        self.listeners_frame.grid_rowconfigure(0, weight=1)

        # Custom Listener 1
        self.listener_1_frame = ctk.CTkFrame(self.listeners_frame)
        self.listener_1_frame.grid(row=0, column=0, padx=(0, 5), pady=0, sticky="nsew")
        self.listener_1_frame.grid_rowconfigure(2, weight=1)
        self.listener_1_frame.grid_columnconfigure(0, weight=1)

        self.l1_title = ctk.CTkLabel(self.listener_1_frame, text="Custom Listener 1", font=ctk.CTkFont(size=12, weight="bold"))
        self.l1_title.grid(row=0, column=0, padx=10, pady=(5, 0), sticky="w")
        
        self.l1_controls = ctk.CTkFrame(self.listener_1_frame, fg_color="transparent")
        self.l1_controls.grid(row=1, column=0, padx=10, pady=3, sticky="ew")
        self.l1_entry = ctk.CTkEntry(self.l1_controls, placeholder_text="Topic to subscribe")
        self.l1_entry.insert(0, self.config["topic_listener_1"])
        self.l1_entry.pack(side="left", fill="x", expand=True, padx=(0, 5))
        self.l1_entry.bind("<Return>", lambda event: self.update_sub_1())
        self.l1_clear_btn = ctk.CTkButton(self.l1_controls, text="Clear", command=self.clear_listener_1, width=60, fg_color="#475569")
        self.l1_clear_btn.pack(side="right")
        self.l1_sub_btn = ctk.CTkButton(self.l1_controls, text="Update", command=self.update_sub_1, width=60)
        self.l1_sub_btn.pack(side="right", padx=(0, 5))

        self.l1_textbox = ctk.CTkTextbox(self.listener_1_frame, state="disabled")
        self.l1_textbox.grid(row=2, column=0, padx=10, pady=(0, 10), sticky="nsew")

        # Custom Listener 2
        self.listener_2_frame = ctk.CTkFrame(self.listeners_frame)
        self.listener_2_frame.grid(row=0, column=1, padx=(5, 0), pady=0, sticky="nsew")
        self.listener_2_frame.grid_rowconfigure(2, weight=1)
        self.listener_2_frame.grid_columnconfigure(0, weight=1)

        self.l2_title = ctk.CTkLabel(self.listener_2_frame, text="Custom Listener 2", font=ctk.CTkFont(size=12, weight="bold"))
        self.l2_title.grid(row=0, column=0, padx=10, pady=(5, 0), sticky="w")

        self.l2_controls = ctk.CTkFrame(self.listener_2_frame, fg_color="transparent")
        self.l2_controls.grid(row=1, column=0, padx=10, pady=3, sticky="ew")
        self.l2_entry = ctk.CTkEntry(self.l2_controls, placeholder_text="Topic to subscribe")
        self.l2_entry.insert(0, self.config["topic_listener_2"])
        self.l2_entry.pack(side="left", fill="x", expand=True, padx=(0, 5))
        self.l2_entry.bind("<Return>", lambda event: self.update_sub_2())
        self.l2_clear_btn = ctk.CTkButton(self.l2_controls, text="Clear", command=self.clear_listener_2, width=60, fg_color="#475569")
        self.l2_clear_btn.pack(side="right")
        self.l2_sub_btn = ctk.CTkButton(self.l2_controls, text="Update", command=self.update_sub_2, width=60)
        self.l2_sub_btn.pack(side="right", padx=(0, 5))

        self.l2_textbox = ctk.CTkTextbox(self.listener_2_frame, state="disabled")
        self.l2_textbox.grid(row=2, column=0, padx=10, pady=(0, 10), sticky="nsew")

        # 3. Command Matrix (Buttons)
        self.matrix_frame_container = ctk.CTkFrame(self.main_frame)
        self.matrix_frame_container.grid(row=2, column=0, padx=10, pady=(5, 10), sticky="nsew")
        self.matrix_frame_container.grid_rowconfigure(1, weight=1)
        self.matrix_frame_container.grid_columnconfigure(0, weight=1)

        self.matrix_header = ctk.CTkLabel(self.matrix_frame_container, 
                                           text="Command Matrix (Right-click a button to edit its properties)", 
                                           font=ctk.CTkFont(size=13, weight="bold"))
        self.matrix_header.grid(row=0, column=0, padx=10, pady=5, sticky="w")

        self.matrix_grid = ctk.CTkFrame(self.matrix_frame_container, fg_color="transparent")
        self.matrix_grid.grid(row=1, column=0, padx=10, pady=(0, 10), sticky="nsew")

        # 5 Columns, 6 Rows for 30 buttons
        for i in range(5):
            self.matrix_grid.grid_columnconfigure(i, weight=1)
        for i in range(6):
            self.matrix_grid.grid_rowconfigure(i, weight=1)

        self.ui_buttons = []
        self.render_buttons()

        # ==========================================
        # RIGHT PANEL (Narrow App Logs & Control)
        # ==========================================
        self.right_frame = ctk.CTkFrame(self, width=280)
        self.right_frame.grid(row=0, column=1, padx=(0, 10), pady=10, sticky="nsew")
        self.right_frame.grid_rowconfigure(2, weight=1)
        self.right_frame.grid_columnconfigure(0, weight=1)

        # Connection Box
        self.conn_frame = ctk.CTkFrame(self.right_frame)
        self.conn_frame.grid(row=0, column=0, padx=10, pady=10, sticky="ew")
        
        self.conn_title = ctk.CTkLabel(self.conn_frame, text="Broker Configuration", font=ctk.CTkFont(size=12, weight="bold"))
        self.conn_title.pack(padx=10, pady=(5, 5), anchor="w")

        self.ip_entry = ctk.CTkEntry(self.conn_frame, placeholder_text="Broker IP")
        self.ip_entry.insert(0, self.config["broker_ip"])
        self.ip_entry.pack(padx=10, pady=2, fill="x")

        self.port_entry = ctk.CTkEntry(self.conn_frame, placeholder_text="Broker Port")
        self.port_entry.insert(0, str(self.config["broker_port"]))
        self.port_entry.pack(padx=10, pady=2, fill="x")

        self.connect_btn = ctk.CTkButton(self.conn_frame, text="Connect", command=self.toggle_connection, fg_color="#38bdf8", text_color="#0f172a")
        self.connect_btn.pack(padx=10, pady=(10, 10), fill="x")

        # App System Logs Box
        self.app_logs_title = ctk.CTkLabel(self.right_frame, text="App System Logs", font=ctk.CTkFont(size=13, weight="bold"))
        self.app_logs_title.grid(row=1, column=0, padx=10, pady=(10, 0), sticky="w")

        self.app_log_textbox = ctk.CTkTextbox(self.right_frame, state="disabled")
        self.app_log_textbox.grid(row=2, column=0, padx=10, pady=(5, 10), sticky="nsew")

    def render_buttons(self):
        # Clear existing button widgets if any
        for btn in self.ui_buttons:
            btn.destroy()
        self.ui_buttons.clear()

        buttons_cfg = self.config["buttons"]
        for idx, btn_cfg in enumerate(buttons_cfg[:30]):
            row = idx // 5
            col = idx % 5
            
            label = btn_cfg.get("label", f"Empty {idx+1}")
            topic = btn_cfg.get("topic", "")
            payload = btn_cfg.get("payload", "")

            # Create a button wrapper widget
            btn = ctk.CTkButton(self.matrix_grid, text=label, font=ctk.CTkFont(size=11))
            # Set action
            btn.configure(command=lambda t=topic, p=payload: self.publish_msg(t, p))
            btn.grid(row=row, column=col, padx=4, pady=4, sticky="nsew")
            
            # Bind right-click to edit (Button-3 on Windows, Button-2 on macOS)
            btn.bind("<Button-3>", lambda event, i=idx: self.open_edit_dialog(i))
            btn.bind("<Button-2>", lambda event, i=idx: self.open_edit_dialog(i))

            self.ui_buttons.append(btn)

    def open_edit_dialog(self, button_idx):
        current_cfg = self.config["buttons"][button_idx]
        EditButtonDialog(self, button_idx, current_cfg, self.save_button_cfg)

    def save_button_cfg(self, idx, new_cfg):
        self.config["buttons"][idx] = new_cfg
        self.save_config_to_file()
        self.log_app_event(f"Configured Button {idx+1}: Label='{new_cfg['label']}', Topic='{new_cfg['topic']}', Payload='{new_cfg['payload']}'")
        self.render_buttons()

    def update_sub_1(self):
        new_topic = self.l1_entry.get().strip()
        old_topic = self.config["topic_listener_1"]
        self.config["topic_listener_1"] = new_topic
        self.save_config_to_file()

        if self.mqtt_client.is_connected():
            if old_topic:
                self.mqtt_client.unsubscribe(old_topic)
                self.log_app_event(f"Unsubscribed Listener 1 from {old_topic}")
            if new_topic:
                self.mqtt_client.subscribe(new_topic)
                self.log_app_event(f"Subscribed Listener 1 to {new_topic}")
        else:
            self.log_app_event(f"Updated Listener 1 topic to {new_topic} (offline)")

    def update_sub_2(self):
        new_topic = self.l2_entry.get().strip()
        old_topic = self.config["topic_listener_2"]
        self.config["topic_listener_2"] = new_topic
        self.save_config_to_file()

        if self.mqtt_client.is_connected():
            if old_topic:
                self.mqtt_client.unsubscribe(old_topic)
                self.log_app_event(f"Unsubscribed Listener 2 from {old_topic}")
            if new_topic:
                self.mqtt_client.subscribe(new_topic)
                self.log_app_event(f"Subscribed Listener 2 to {new_topic}")
        else:
            self.log_app_event(f"Updated Listener 2 topic to {new_topic} (offline)")

    def clear_listener_1(self):
        self.l1_textbox.configure(state="normal")
        self.l1_textbox.delete("1.0", "end")
        self.l1_textbox.configure(state="disabled")
        self.log_app_event("Cleared Custom Listener 1 textbox.")

    def clear_listener_2(self):
        self.l2_textbox.configure(state="normal")
        self.l2_textbox.delete("1.0", "end")
        self.l2_textbox.configure(state="disabled")
        self.log_app_event("Cleared Custom Listener 2 textbox.")

    def connect_on_startup(self):
        self.log_app_event("Connecting on startup...")
        self.toggle_connection()

    def toggle_connection(self):
        if not self.mqtt_client.is_connected():
            ip = self.ip_entry.get().strip()
            port_str = self.port_entry.get().strip()
            
            try:
                port = int(port_str)
            except ValueError:
                self.log_app_event("ERROR: Invalid port number.")
                return

            self.config["broker_ip"] = ip
            self.config["broker_port"] = port
            
            # Read current listener entries and save them so connecting uses the visible values
            self.config["topic_listener_1"] = self.l1_entry.get().strip()
            self.config["topic_listener_2"] = self.l2_entry.get().strip()
            
            self.save_config_to_file()

            try:
                self.mqtt_client.connect_async(ip, port, 60)
                self.mqtt_client.loop_start()
                self.connect_btn.configure(text="Disconnect", fg_color="#ef4444", text_color="#ffffff")
                self.log_app_event(f"Connecting to {ip}:{port}...")
            except Exception as e:
                self.log_app_event(f"ERROR: Connection initiation failed: {str(e)}")
        else:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()
            self.connect_btn.configure(text="Connect", fg_color="#38bdf8", text_color="#0f172a")
            self.log_app_event("Disconnected from broker.")

    def on_connect(self, client, userdata, flags, rc):
        self.after(0, self.handle_connect_main_thread, rc)

    def handle_connect_main_thread(self, rc):
        if rc == 0:
            self.log_app_event("Connected to MQTT broker successfully!")
            
            # Default subscribe to DeskBuddy logs
            self.mqtt_client.subscribe("deskbuddy/log/#")
            self.log_app_event("Subscribed to DeskBuddy logs (deskbuddy/log/#)")

            # Subscribe to custom listeners
            t1 = self.config.get("topic_listener_1")
            t2 = self.config.get("topic_listener_2")
            if t1:
                self.mqtt_client.subscribe(t1)
                self.log_app_event(f"Subscribed Listener 1 to {t1}")
            if t2:
                self.mqtt_client.subscribe(t2)
                self.log_app_event(f"Subscribed Listener 2 to {t2}")
        else:
            self.log_app_event(f"Connection failed with result code {rc}")

    def on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode('utf-8', errors='ignore')
        self.after(0, self.handle_message_main_thread, topic, payload)

    def handle_message_main_thread(self, topic, payload):
        matched = False

        # Check if log topic
        if topic.startswith("deskbuddy/log/"):
            category = topic.split("/")[-1].upper()
            self.log_deskbuddy_msg(category, payload)
            matched = True

        # Check Custom Listener 1
        t1 = self.config.get("topic_listener_1")
        if t1 and mqtt.topic_matches_sub(t1, topic):
            self.update_listener_textbox(self.l1_textbox, topic, payload)
            matched = True

        # Check Custom Listener 2
        t2 = self.config.get("topic_listener_2")
        if t2 and mqtt.topic_matches_sub(t2, topic):
            self.update_listener_textbox(self.l2_textbox, topic, payload)
            matched = True

        if not matched:
            # Fallback output for other topics to app log
            self.log_app_event(f"Received unsubscribed topic {topic}: {payload}")

    def update_listener_textbox(self, textbox, topic, payload):
        textbox.configure(state="normal")
        
        timestamp = time.strftime('%H:%M:%S', time.localtime())
        
        # Custom parsing for AI request/response logs
        if topic == "deskbuddy/debug/ai/request" and "---------- AI Request ----------" in payload:
            try:
                # Extract URL and Body
                lines = payload.splitlines()
                url_line = [l for l in lines if l.startswith("URL:")][0]
                body_line = [l for l in lines if l.startswith("Body:")][0]
                
                url = url_line.replace("URL:", "").strip()
                body_json_str = body_line.replace("Body:", "").strip()
                
                body_data = json.loads(body_json_str)
                prompt = body_data["messages"][0]["content"]
                model = body_data.get("model", "Unknown Model")
                
                formatted_payload = (
                    f"🤖 AI REQUEST\n"
                    f"  URL: {url}\n"
                    f"  Model: {model}\n"
                    f"  Prompt:\n"
                    f"  ----------------------------------------------------------------------\n"
                    f"  {prompt}\n"
                    f"  ----------------------------------------------------------------------"
                )
            except Exception as e:
                formatted_payload = f"Failed to parse request: {str(e)}\n\n{payload}"
                
        elif topic == "deskbuddy/debug/ai/response" and "---------- AI Response ----------" in payload:
            try:
                lines = payload.splitlines()
                http_line = [l for l in lines if l.startswith("HTTP Code:")][0]
                payload_line = [l for l in lines if l.startswith("Payload:")][0]
                
                http_code = http_line.replace("HTTP Code:", "").strip()
                raw_payload = payload_line.replace("Payload:", "").strip()
                
                # Parse Groq API JSON response
                resp_data = json.loads(raw_payload)
                choices = resp_data.get("choices", [])
                if choices:
                    content = choices[0]["message"]["content"]
                else:
                    content = "[No Choices Found]"
                
                # Pretty print raw payload
                pretty_raw = json.dumps(resp_data, indent=2)
                
                formatted_payload = (
                    f"💬 AI RESPONSE (HTTP {http_code})\n"
                    f"  Result: \"{content}\"\n"
                    f"  Raw JSON:\n"
                    f"  {pretty_raw}"
                )
            except Exception as e:
                formatted_payload = f"Failed to parse response: {str(e)}\n\n{payload}"
        else:
            # Fallback formatting for JSON or raw text
            try:
                parsed = json.loads(payload)
                formatted_payload = json.dumps(parsed, indent=2)
            except Exception:
                formatted_payload = payload

        output = f"[{timestamp}] Topic: {topic}\n{formatted_payload}\n\n"
        
        textbox.insert("1.0", output) # Keep newest on top
        
        # Trim text size if too long
        content = textbox.get("1.0", "end")
        if len(content) > 15000:
            textbox.delete("150.0", "end")
            
        textbox.configure(state="disabled")

    def log_deskbuddy_msg(self, category, message):
        self.db_log_textbox.configure(state="normal")
        
        tag = "DEFAULT"
        if category in ["SYSTEM"]: tag = "SYSTEM"
        elif category in ["ERROR"]: tag = "ERROR"
        elif category in ["WIFI", "NETWORK"]: tag = "WIFI"
        elif category in ["MQTT"]: tag = "MQTT"

        timestamp = time.strftime('%H:%M:%S', time.localtime())
        log_str = f"[{timestamp}] [{category}] {message}\n"
        
        self.db_log_textbox.insert("end", log_str, tag)
        self.db_log_textbox.see("end")
        self.db_log_textbox.configure(state="disabled")

        # Save to local file
        try:
            with open(LOG_FILE, "a", encoding="utf-8") as f:
                f.write(log_str)
        except Exception as e:
            self.log_app_event(f"ERROR saving log to disk: {str(e)}")

    def log_app_event(self, message):
        self.app_log_textbox.configure(state="normal")
        timestamp = time.strftime('%H:%M:%S', time.localtime())
        log_entry = f"[{timestamp}] {message}\n"
        self.app_log_textbox.insert("end", log_entry)
        self.app_log_textbox.see("end")
        self.app_log_textbox.configure(state="disabled")

    def publish_msg(self, topic, payload):
        if not topic:
            self.log_app_event("ERROR: Cannot publish. Topic is blank.")
            return

        if self.mqtt_client.is_connected():
            # Support basic payload cleanups
            # Unescape quotes if needed
            if payload.startswith('"') and payload.endswith('"'):
                payload_clean = payload[1:-1]
            else:
                payload_clean = payload

            self.mqtt_client.publish(topic, payload_clean)
            self.log_app_event(f"Published to '{topic}' -> {payload_clean}")
        else:
            self.log_app_event("ERROR: Cannot publish. Not connected to broker.")


if __name__ == "__main__":
    app = DeskBuddyMqttClient()
    app.mainloop()
