
/*
  路由器应急补电系统（WiFi配网版）
  适配硬件：ESP32 S3 N16R8开发板
  接线说明：
  - GPIO1 (AO_DETECT)：电压检测引脚（连接到分压电路输出）
  - GPIO2 (RELAY_PIN)：继电器控制引脚（连接到继电器模块）
  功能说明：
  1. 应急补电：检测市电电压，当电压低于阈值时自动切换到应急电源
  2. WiFi配网：支持手机式WiFi配置，自动保存到EEPROM
  3. Web界面：Win11风格的网页界面，显示供电状态和系统信息
  4. 日志功能：记录系统运行状态，定期发送到服务器
  使用说明：
  - SERVER_IP需要改为您电脑的IP地址（与UPS在同一个局域网）
  - 电压阈值VOLT_THRESHOLD需要根据实际硬件调整
  - 默认AP热点：SSID=smartUPS，密码=12345678
*/

// 引入必要的库文件
#include <WiFi.h>          // WiFi功能库
#include <WebServer.h>      // Web服务器库
#include <EEPROM.h>         // 闪存存储库
#include <time.h>           // 时间同步库

// ====================== 核心配置 ======================
// 应急补电参数
const int AO_DETECT = 1;       // 电压检测引脚（ESP32 S3 GPIO1）
const int RELAY_PIN = 2;       // 继电器控制引脚（ESP32 S3 GPIO2）
const int VOLT_THRESHOLD = 1500; // 电压阈值（根据实际调整）
const int CHECK_INTERVAL = 20; // 检测间隔（ms）
const int STABLE_CHECK = 1;     // 稳定检测次数
const int STABLE_INTERVAL = 12; // 检测间隔（ms）

// WiFi配网参数
#define EEPROM_SIZE 512         // 闪存存储大小（用于保存WiFi配置）
String wifiSSID = "";           // WiFi名称（用户配置）
String wifiPWD = "";            // WiFi密码（用户配置）
bool isWiFiConnected = false;   // WiFi连接状态（true=已连接，false=未连接）

// AP热点参数
WebServer server(80);           // Web服务器实例，监听80端口
const char* AP_SSID = "smartUPS"; // 默认AP热点名称
const char* AP_PASS = "12345678"; // 默认AP热点密码

// 时间同步配置
String currentTime = "--:--";    // 当前时间（HH:MM格式）

// 日志配置
#define MAX_LOG_ENTRIES 10       // 日志缓冲区最大条目数
struct LogEntry {                // 日志条目结构体
  char timestamp[6];  // 时间戳（HH:MM格式）
  char level[6];      // 日志级别（INFO/ERROR）
  char message[64];   // 日志内容
};
LogEntry logBuffer[MAX_LOG_ENTRIES]; // 日志缓冲区
int logCount = 0;                // 当前日志条目数
const char* SERVER_IP = "192.168.5.10"; // Flask服务器IP（需要改为您电脑的IP）
const int SERVER_PORT = 8080;            // Flask服务器端口
unsigned long lastLogSendTime = 0;       // 上次发送日志的时间
const long LOG_SEND_INTERVAL = 30000;    // 日志发送间隔（30秒）

// 状态变量
String powerStatus = "未知";     // 当前供电状态
String statusColor = "#666666"; // 状态颜色（绿色=正常，橙色=应急）
unsigned long lastCheckTime = 0; // 上次电压检测时间
unsigned long lastSyncTime = 0;  // 上次时间同步时间
const long SYNC_INTERVAL = 3600000; // 时间同步间隔（1小时）

// ====================== WiFi配网函数 ======================
/**
 * 扫描附近的WiFi热点
 * 返回HTML格式的WiFi列表，包含信号强度和连接按钮
 */
String scanWiFiList() {
  String wifiListHtml = "";
  int n = WiFi.scanNetworks(); // 扫描WiFi热点
  if (n == 0) {
    wifiListHtml = "<div style='padding:10px; color:#666;'>未扫描到WiFi热点</div>";
  } else {
    for (int i = 0; i < n; i++) {
      // 生成每个WiFi热点的HTML条目
      wifiListHtml += "<div style='padding:8px 0; border-bottom:1px solid #eee; display:flex; justify-content:space-between; align-items:center;'>";
      wifiListHtml += "<span>" + WiFi.SSID(i) + "</span>";
      // 计算信号强度百分比（-100dBm=0%，0dBm=100%）
      wifiListHtml += "<span style='color:#666; font-size:12px;'>信号：" + String(WiFi.RSSI(i)+100) + "%</span>";
      // 连接按钮
      wifiListHtml += "<button onclick='showConnectModal(\"" + WiFi.SSID(i) + "\")' style='background:#0078D4; color:white; border:none; padding:4px 8px; border-radius:4px; font-size:12px;'>连接</button>";
      wifiListHtml += "</div>";
      delay(10); // 短暂延迟，避免扫描过快
    }
  }
  return wifiListHtml;
}

/**
 * 保存WiFi配置到EEPROM
 * 存储位置：
 * - 0-199: WiFi名称
 * - 200: WiFi名称长度
 * - 201-399: WiFi密码
 * - 400: WiFi密码长度
 */
void saveWiFiToEEPROM() {
  EEPROM.begin(EEPROM_SIZE); // 初始化EEPROM
  for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0); // 清空EEPROM
  
  // 保存WiFi名称
  for (int i = 0; i < wifiSSID.length(); i++) EEPROM.write(i, wifiSSID[i]);
  EEPROM.write(200, wifiSSID.length()); // 保存WiFi名称长度
  
  // 保存WiFi密码
  for (int i = 0; i < wifiPWD.length(); i++) EEPROM.write(201 + i, wifiPWD[i]);
  EEPROM.write(400, wifiPWD.length()); // 保存WiFi密码长度
  
  EEPROM.commit(); // 提交更改
  EEPROM.end(); // 关闭EEPROM
}

/**
 * 从EEPROM读取WiFi配置
 * 读取位置：
 * - 0-199: WiFi名称
 * - 200: WiFi名称长度
 * - 201-399: WiFi密码
 * - 400: WiFi密码长度
 */
void readWiFiFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE); // 初始化EEPROM
  
  // 读取WiFi名称
  int ssidLen = EEPROM.read(200); // 读取WiFi名称长度
  for (int i = 0; i < ssidLen; i++) wifiSSID += char(EEPROM.read(i)); // 读取WiFi名称
  
  // 读取WiFi密码
  int pwdLen = EEPROM.read(400); // 读取WiFi密码长度
  for (int i = 0; i < pwdLen; i++) wifiPWD += char(EEPROM.read(201 + i)); // 读取WiFi密码
  
  EEPROM.end(); // 关闭EEPROM
}

/**
 * 连接到WiFi网络
 * @return true=连接成功，false=连接失败
 */
bool connectWiFi() {
  if (wifiSSID == "" || wifiPWD == "") return false; // WiFi配置为空，直接返回失败
  
  WiFi.begin(wifiSSID.c_str(), wifiPWD.c_str()); // 开始连接WiFi
  int retry = 0;
  
  // 等待连接成功，最多重试20次（10秒）
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500); // 等待500毫秒
    retry++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi连接成功！IP：" + WiFi.localIP().toString());
    return true;
  } else {
    Serial.println("WiFi连接失败");
    return false;
  }
}

/**
 * 处理WiFi连接请求（Web API）
 * 请求参数：ssid=WiFi名称，pwd=WiFi密码
 * 返回结果："连接成功"或"连接失败"
 */
void handleWiFiConnect() {
  if (server.hasArg("ssid") && server.hasArg("pwd")) {
    wifiSSID = server.arg("ssid"); // 获取WiFi名称
    wifiPWD = server.arg("pwd");   // 获取WiFi密码
    
    isWiFiConnected = connectWiFi(); // 连接WiFi
    if (isWiFiConnected) saveWiFiToEEPROM(); // 连接成功则保存配置
    
    server.send(200, "text/plain", isWiFiConnected ? "连接成功" : "连接失败"); // 返回结果
  } else {
    server.send(400, "text/plain", "参数错误"); // 参数缺失，返回错误
  }
}

// ====================== 日志函数 ======================
/**
 * 添加日志条目
 * @param level 日志级别（INFO/ERROR）
 * @param message 日志内容
 */
void addLog(const char* level, const char* message) {
  if (logCount < MAX_LOG_ENTRIES) { // 日志缓冲区未满
    strncpy(logBuffer[logCount].timestamp, currentTime.c_str(), 5); // 复制时间戳
    strncpy(logBuffer[logCount].level, level, 5); // 复制日志级别
    strncpy(logBuffer[logCount].message, message, 63); // 复制日志内容
    logCount++; // 日志条目数加1
  }
}

/**
 * 发送日志到Flask服务器
 * 日志格式：JSON数组，包含time、level、message字段
 */
void sendLogs() {
  if (logCount == 0 || !isWiFiConnected) return; // 没有日志或未连接WiFi，直接返回
  
  WiFiClient client;
  if (client.connect(SERVER_IP, SERVER_PORT)) { // 连接到服务器
    // 构建JSON格式的日志数据
    String json = "{\"logs\":[";
    for (int i = 0; i < logCount; i++) {
      if (i > 0) json += ",";
      json += "{\"time\":\"" + String(logBuffer[i].timestamp) + "\",";
      json += "\"level\":\"" + String(logBuffer[i].level) + "\",";
      json += "\"message\":\"" + String(logBuffer[i].message) + "\"}";
    }
    json += "]}";
    
    // 发送HTTP POST请求
    client.print("POST /api/logs HTTP/1.1\r\n");
    client.print("Host: " + String(SERVER_IP) + "\r\n");
    client.print("Content-Type: application/json\r\n");
    client.print("Content-Length: " + String(json.length()) + "\r\n");
    client.print("\r\n");
    client.print(json);
    
    // 等待服务器响应，超时时间3秒
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 3000) {
        client.stop();
        return;
      }
    }
    
    client.stop(); // 关闭连接
    logCount = 0; // 清空日志缓冲区
  }
}

// ====================== 应急补电函数 ======================
/**
 * 检测市电电压并控制继电器
 * 当电压低于阈值时，接通应急供电
 * 当电压高于阈值时，断开应急供电
 */
void checkPowerAndControlRelay() {
  int lowVoltCount = 0;
  int analogValue = analogRead(AO_DETECT); // 读取电压检测引脚值
  Serial.print("电压检测值:");
  Serial.println(analogValue);
  
  // 判断电压是否低于阈值
  if (analogValue < VOLT_THRESHOLD) lowVoltCount++;

  // 根据稳定检测结果控制继电器
  if (lowVoltCount >= STABLE_CHECK) {
    digitalWrite(RELAY_PIN, HIGH); // 接通继电器（应急供电）
    powerStatus = "市电断电，应急供电中";
    statusColor = "#FF5733"; // 橙色（应急状态）
    Serial.println(">>> 已接通应急供电 <<<");
    addLog("INFO", "市电断电，应急供电中");
  } else {
    digitalWrite(RELAY_PIN, LOW); // 断开继电器（原装电源供电）
    powerStatus = "市电正常，原装电源供电";
    statusColor = "#33CC33"; // 绿色（正常状态）
    Serial.println(">>> 已断开应急供电 <<<");
    addLog("INFO", "市电正常，原装电源供电");
  }
  Serial.println("----------------------------------------");
}

// ====================== API接口 ======================
/**
 * 获取供电状态API
 * 返回JSON格式：{"status": "供电状态", "color": "状态颜色"}
 */
void handlePowerStatus() {
  String json = "{\"status\":\"" + powerStatus + "\", \"color\":\"" + statusColor + "\"}";
  server.send(200, "application/json", json); // 返回JSON格式的供电状态
}

// ====================== Win11风格UI网页 ======================
/**
 * 处理根路径请求，返回Win11风格的监控网页
 * 包含主页、WiFi设置、日志查看三个页面
 */
void handleRoot() {
  String wifiList = scanWiFiList(); // 扫描附近的WiFi热点
  String connectInfo = "";
  
  // 生成WiFi连接信息
  if (isWiFiConnected) {
    connectInfo = "<div style='padding:10px; line-height:1.6;'><div>当前WiFi：" + wifiSSID + "</div><div>IP地址：" + WiFi.localIP().toString() + "</div></div>";
  } else {
    connectInfo = "<div style='padding:10px; color:#666;'>未连接WiFi（当前使用AP热点）</div>";
  }

  String html = "<!DOCTYPE html>";
  html += "<html lang='zh-CN'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>智能UPS监控</title>";
  html += "<style>";
  html += "* { margin: 0; padding: 0; box-sizing: border-box; font-family: 'Microsoft YaHei', sans-serif; }";
  html += "body { background-color: #F3F4F6; color: #111827; display: flex; min-height: 100vh; }";
  html += ".sidebar { width: 220px; background-color: #FFFFFF; border-right: 1px solid #E5E7EB; padding: 20px 0; }";
  html += ".sidebar-item { display: flex; align-items: center; padding: 12px 20px; cursor: pointer; color: #4B5563; text-decoration: none; }";
  html += ".sidebar-item.active { background-color: #EFF6FF; color: #0078D4; border-left: 3px solid #0078D4; }";
  html += ".sidebar-icon { width: 24px; height: 24px; margin-right: 12px; text-align: center; }";
  html += ".main-content { flex: 1; padding: 30px; overflow-y: auto; }";
  html += ".page { display: none; }";
  html += ".page.active { display: block; }";
  html += ".card { background-color: #FFFFFF; border-radius: 8px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); padding: 20px; margin-bottom: 20px; }";
  html += ".card-title { font-size: 18px; font-weight: 600; margin-bottom: 16px; display: flex; justify-content: space-between; align-items: center; }";
  html += ".toggle { width: 48px; height: 24px; background-color: #E5E7EB; border-radius: 12px; position: relative; cursor: pointer; transition: background-color 0.2s; }";
  html += ".toggle.on { background-color: #0078D4; }";
  html += ".toggle::after { content: ''; width: 20px; height: 20px; background-color: #FFFFFF; border-radius: 50%; position: absolute; top: 2px; left: 2px; transition: left 0.2s; }";
  html += ".toggle.on::after { left: 26px; }";
  html += ".power-status-box { font-size: 28px; font-weight: 600; padding: 40px 20px; border-radius: 8px; margin: 20px 0; text-align: center; color: white; }";
  html += ".collapse-content { max-height: 0; overflow: hidden; transition: max-height 0.3s ease; }";
  html += ".collapse-content.show { max-height: 500px; }";
  html += ".arrow { width: 16px; height: 16px; transition: transform 0.2s; }";
  html += ".arrow.rotate { transform: rotate(180deg); }";
  html += ".modal { position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.5); display: none; justify-content: center; align-items: center; }";
  html += ".modal-content { background: white; padding: 20px; border-radius: 8px; width: 90%; max-width: 400px; }";
  html += ".modal-title { font-size: 18px; font-weight: 600; margin-bottom: 16px; }";
  html += ".modal-input { width: 100%; padding: 10px; margin: 8px 0; border: 1px solid #E5E7EB; border-radius: 4px; }";
  html += ".modal-btns { display: flex; justify-content: flex-end; gap: 10px; margin-top: 20px; }";
  html += ".modal-btn { padding: 8px 16px; border: none; border-radius: 4px; cursor: pointer; }";
  html += ".btn-cancel { background: #E5E7EB; color: #111827; }";
  html += ".btn-confirm { background: #0078D4; color: white; }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='sidebar'>";
  html += "<a href='#home' class='sidebar-item active' onclick='switchPage(\"home\")'>";
  html += "<div class='sidebar-icon'>🏠</div>";
  html += "<div>主页</div>";
  html += "</a>";
  html += "<a href='#wifi' class='sidebar-item' onclick='switchPage(\"wifi\")'>";
  html += "<div class='sidebar-icon'>📶</div>";
  html += "<div>WiFi设置</div>";
  html += "</a>";
  html += "<a href='http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + "' class='sidebar-item' target='_blank'>";
  html += "<div class='sidebar-icon'>📋</div>";
  html += "<div>查看日志</div>";
  html += "</a>";
  html += "</div>";
  html += "<div class='main-content'>";
  html += "<div id='home' class='page active'>";
  html += "<h1 style='margin-bottom: 30px; color: #111827;'>智能UPS监控中心</h1>";
  html += "<div class='power-status-box' style='background-color: " + statusColor + "'>" + powerStatus + "</div>";
  html += "<div class='card'>";
  html += "<div class='card-title'>";
  html += "<span>系统状态</span>";
  html += "<span style='font-size:24px; font-weight:600; color:#0078D4;'>" + currentTime + "</span>";
  html += "</div>";
  html += "<div style='line-height:1.8; color:#4B5563;'>";
  html += "<div>检测间隔：" + String(CHECK_INTERVAL) + "ms</div>";
html += String("<div>WiFi连接状态：") + (isWiFiConnected ? "已连接" : "未连接") + "</div>";  html += "<div>访问地址：" + (isWiFiConnected ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "</div>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  html += "<div id='wifi' class='page'>";
  html += "<h1 style='margin-bottom: 30px; color: #111827;'>WiFi设置</h1>";
  html += "<div class='card'>";
  html += "<div class='card-title'>";
  html += "<span>WLAN</span>";
  html += "<div class='toggle on' id='wifiToggle' onclick='toggleWiFi()'></div>";
  html += "</div>";
  html += "</div>";
  html += "<div class='card'>";
  html += "<div class='card-title' onclick='toggleCollapse(\"wifiListCollapse\", \"wifiListArrow\")'>";
  html += "<span>显示可用网络</span>";
  html += "<img src='data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iMTYiIGhlaWdodD0iMTYiIHZpZXdCb3g9IjAgMCAxNiAxNiIgZmlsbD0ibm9uZSIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj48cGF0aCBkPSJNMTIgNnY0aC00bC00LTQtNHY0aC00VjZoNHoiLz48L3N2Zz4=' class='arrow' id='wifiListArrow'>";
  html += "</div>";
  html += "<div class='collapse-content' id='wifiListCollapse'>" + wifiList + "</div>";
  html += "</div>";
  html += "<div class='card'>";
  html += "<div class='card-title' onclick='toggleCollapse(\"connectInfoCollapse\", \"connectInfoArrow\")'>";
  html += "<span>连接信息</span>";
  html += "<img src='data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iMTYiIGhlaWdodD0iMTYiIHZpZXdCb3g9IjAgMCAxNiAxNiIgZmlsbD0ibm9uZSIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj48cGF0aCBkPSJNMTIgNnY0aC00bC00LTQtNHY0aC00VjZoNHoiLz48L3N2Zz4=' class='arrow' id='connectInfoArrow'>";
  html += "</div>";
  html += "<div class='collapse-content' id='connectInfoCollapse'>" + connectInfo + "</div>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  html += "<div class='modal' id='connectModal'>";
  html += "<div class='modal-content'>";
  html += "<div class='modal-title'>连接WiFi</div>";
  html += "<input type='text' id='modalSsid' readonly class='modal-input'>";
  html += "<input type='password' id='modalPwd' placeholder='请输入WiFi密码' class='modal-input'>";
  html += "<div class='modal-btns'>";
  html += "<button class='modal-btn btn-cancel' onclick='hideModal()'>取消</button>";
  html += "<button class='modal-btn btn-confirm' onclick='submitWiFiConnect()'>连接</button>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  html += "<script>";
  html += "function switchPage(pageId) {";
  html += "document.querySelectorAll('.page').forEach(page => page.classList.remove('active'));";
  html += "document.querySelectorAll('.sidebar-item').forEach(item => item.classList.remove('active'));";
  html += "document.getElementById(pageId).classList.add('active');";
  html += "document.querySelector(`.sidebar-item[href=\"#${pageId}\"]`).classList.add('active');";
  html += "}";
  html += "function toggleCollapse(contentId, arrowId) {";
  html += "const content = document.getElementById(contentId);";
  html += "const arrow = document.getElementById(arrowId);";
  html += "content.classList.toggle('show');";
  html += "arrow.classList.toggle('rotate');";
  html += "}";
  html += "function showConnectModal(ssid) {";
  html += "document.getElementById('modalSsid').value = ssid;";
  html += "document.getElementById('modalPwd').value = '';";
  html += "document.getElementById('connectModal').style.display = 'flex';";
  html += "}";
  html += "function hideModal() {";
  html += "document.getElementById('connectModal').style.display = 'none';";
  html += "}";
  html += "async function submitWiFiConnect() {";
  html += "const ssid = document.getElementById('modalSsid').value;";
  html += "const pwd = document.getElementById('modalPwd').value;";
  html += "if (!pwd) { alert('请输入密码'); return; }";
  html += "const response = await fetch(`/wifi/connect?ssid=${encodeURIComponent(ssid)}&pwd=${encodeURIComponent(pwd)}`);";
  html += "const result = await response.text();";
  html += "alert(result);";
  html += "if (result === '连接成功') {";
  html += "hideModal();";
  html += "location.reload();";
  html += "}";
  html += "}";
  html += "function toggleWiFi() {";
  html += "const toggle = document.getElementById('wifiToggle');";
  html += "toggle.classList.toggle('on');";
  html += "}";
  html += "// 实时刷新供电状态";
  html += "setInterval(async () => {";
  html += "try {";
  html += "const response = await fetch('/api/power-status');";
  html += "const data = await response.json();";
  html += "const statusBox = document.querySelector('.power-status-box');";
  html += "statusBox.textContent = data.status;";
  html += "statusBox.style.backgroundColor = data.color;";
  html += "} catch (error) {";
  html += "console.log('无法获取供电状态');";
  html += "}";
  html += "}, 1000);";
  html += "</script>";
  html += "</body>";
  html += "</html>";

  server.send(200, "text/html", html); // 返回HTML网页
}

// ====================== 初始化函数 ======================
/**
 * 系统初始化函数
 * 配置引脚、串口、WiFi等
 */
void setup() {
  Serial.begin(9600); // 初始化串口，波特率9600
  delay(1000); // 等待1秒，确保硬件稳定
  
  // 配置电压检测引脚为输入模式
  pinMode(AO_DETECT, INPUT);
  
  // ADC 衰减配置（ESP32 专用）
  // 功能：设置 GPIO1 引脚的 ADC 测量量程
  // ADC_11db：官方固定对应量程 = 0 ~ 3.3V
  // 适用场景：测量经过电阻分压后的路由器电源电压（0~3.3V）
  // 不设置后果：默认只能测 0~1.1V，超过就读数不准、继电器乱触发
  analogSetPinAttenuation(AO_DETECT, ADC_11db);
  
  // 配置继电器控制引脚为输出模式
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // 默认断开继电器
  
  Serial.println("===== 应急补电模块初始化完成 =====");
  Serial.print("电压阈值：");
  Serial.println(VOLT_THRESHOLD);

  // WiFi初始化
  readWiFiFromEEPROM(); // 从EEPROM读取WiFi配置
  if (wifiSSID != "" && wifiPWD != "") {
    isWiFiConnected = connectWiFi(); // 尝试连接WiFi
  }

  // 启动AP热点
  WiFi.mode(WIFI_AP_STA); // 设置WiFi模式为AP+STA
  
  if (!isWiFiConnected) {
    // 未连接WiFi，启动AP热点
    WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
    WiFi.softAP(AP_SSID, AP_PASS); // 启动AP热点
    Serial.println("\n===== AP热点启动成功 =====");
    Serial.print("热点名称：");
    Serial.println(AP_SSID);
    Serial.print("访问地址：");
    Serial.println(WiFi.softAPIP());
  } else {
    // 已连接WiFi，显示连接信息
    Serial.println("\n===== WiFi连接成功 =====");
    Serial.print("访问地址：");
    Serial.println(WiFi.localIP());
  }

  // 启动网页服务器
  server.on("/", handleRoot); // 根路径请求处理
  server.on("/wifi/connect", handleWiFiConnect); // WiFi连接请求处理
  server.on("/api/power-status", handlePowerStatus); // 供电状态API
  server.begin(); // 启动Web服务器
  Serial.println("===== 网页服务器启动完成 =====");
}

// ====================== 主循环 ======================
/**
 * 系统主循环函数
 * 处理电压检测、Web请求、时间同步、日志发送等
 */
void loop() {
  // 定期检测电压并控制继电器
  if (millis() - lastCheckTime >= CHECK_INTERVAL) {
    checkPowerAndControlRelay(); // 检测电压并控制继电器
    lastCheckTime = millis(); // 更新上次检测时间
  }
  
  server.handleClient(); // 处理Web服务器请求

  // NTP时间同步（仅当WiFi连接时）
  if (isWiFiConnected && WiFi.status() == WL_CONNECTED) {
    if (millis() - lastSyncTime >= SYNC_INTERVAL || lastSyncTime == 0) {
      // 配置NTP服务器，设置时区为东八区
      configTime(8 * 3600, 0, "ntp.aliyun.com", "time.windows.com");
      lastSyncTime = millis(); // 更新上次同步时间
      Serial.println("NTP时间同步完成");
    }
    
    // 格式化时间为HH:MM
    time_t now = time(nullptr);
    struct tm *tm_info = localtime(&now);
    if (tm_info != nullptr) {
      char timeBuffer[6];
      strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", tm_info);
      currentTime = String(timeBuffer); // 更新当前时间
    }
  }

  // 发送日志到服务器
  if (millis() - lastLogSendTime >= LOG_SEND_INTERVAL) {
    sendLogs(); // 发送日志到Flask服务器
    lastLogSendTime = millis(); // 更新上次发送时间
  }

  // WiFi断开重连
  if (isWiFiConnected && WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi断开，尝试重连...");
    isWiFiConnected = connectWiFi(); // 尝试重连WiFi
    WiFi.mode(WIFI_AP_STA); // 重新设置WiFi模式
  }

}





