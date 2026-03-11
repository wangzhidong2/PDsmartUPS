/*
  路由器应急补电系统（仅AP热点版）
  核心功能：1. 自建热点smartUPS（密码12345678）；2. 模块1断电自动切酷态科；3. 中文彩色网页监控
  硬件适配：A0=电压检测，D6=继电器控制（高电平吸合）
  访问方式：手机连smartUPS热点 → 浏览器输入192.168.4.1
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ====================== 模块1核心配置（完全沿用文档参数） ======================
const int AO_DETECT = A0;       // 电压检测引脚
const int RELAY_PIN = D6;       // 继电器控制引脚
const int VOLT_THRESHOLD = 400; // 电压触发阈值
const int CHECK_INTERVAL = 200; // 主检测间隔200ms
const int STABLE_CHECK = 2;     // 防抖2次
const int STABLE_INTERVAL = 50; // 防抖间隔50ms

// ====================== AP热点配置 ======================
ESP8266WebServer server(80);     // 网页服务器
const char* AP_SSID = "smartUPS";// 热点名称
const char* AP_PASS = "12345678";// 热点密码
String powerStatus = "未知";     // 市电状态（网页显示）
String statusColor = "#666666";  // 状态颜色（初始灰色）
unsigned long lastCheckTime = 0; // 非阻塞定时器

// ====================== 模块1核心函数（不变） ======================
void checkPowerAndControlRelay() {
  int lowVoltCount = 0;
  for (int i = 0; i < STABLE_CHECK; i++) {
    int analogValue = analogRead(AO_DETECT);
    Serial.print("第");
    Serial.print(i + 1);
    Serial.print("次检测 - A0模拟值:");
    Serial.println(analogValue);
    if (analogValue < VOLT_THRESHOLD) lowVoltCount++;
    delay(STABLE_INTERVAL);
  }

  // 更新状态+控制继电器
  if (lowVoltCount >= STABLE_CHECK) {
    digitalWrite(RELAY_PIN, HIGH);
    powerStatus = "市电断电，酷态科供电中";
    statusColor = "#FF5733"; // 橙色
    Serial.println(">>> 市电断电已接通应急供电 <<<");
  } else {
    digitalWrite(RELAY_PIN, LOW);
    powerStatus = "市电正常，路由器用原装电源";
    statusColor = "#33CC33"; // 绿色
    Serial.println(">>> 市电正常！已断开应急供电 <<<");
  }
  Serial.println("----------------------------------------");
}

// ====================== 中文网页监控（极简版） ======================
void handleRoot() {
  String html = R"=====(
    <!DOCTYPE html>
    <html lang="zh-CN">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>智能UPS监控</title>
      <style>
        body { font-family: "Microsoft YaHei", sans-serif; text-align: center; padding-top: 50px; }
        .status-box { font-size: 24px; padding: 30px; border-radius: 10px; margin: 20px auto; width: 80%; max-width: 500px; color: white; }
      </style>
    </head>
    <body>
      <h1>智能UPS监控系统</h1>
      <div class="status-box" style="background-color: )=====" + statusColor + R"=====(">
        )=====" + powerStatus + R"=====(
      </div>
      <p>页面每1秒自动刷新</p>
      <script>setInterval("location.reload()", 1000);</script>
    </body>
    </html>
  )=====";
  server.send(200, "text/html", html);
}

// ====================== 初始化函数（仅启动AP热点） ======================
void setup() {
  // 模块1初始化
  Serial.begin(9600);
  delay(1000);
  pinMode(AO_DETECT, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("===== 模块1初始化完成 =====");
  Serial.print("电压阈值：");
  Serial.println(VOLT_THRESHOLD);

  // 启动AP热点（仅AP模式，无其他WiFi逻辑）
  WiFi.mode(WIFI_AP); // 仅开启AP模式
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.println("\n===== AP热点启动成功 =====");
  Serial.print("热点名称：");
  Serial.println(AP_SSID);
  Serial.print("热点密码：");
  Serial.println(AP_PASS);
  Serial.print("访问地址：");
  Serial.println(WiFi.softAPIP()); // 固定为192.168.4.1

  // 启动网页服务器
  server.on("/", handleRoot);
  server.begin();
  Serial.println("===== 网页服务器启动完成 =====");
  Serial.println("----------------------------------------");
}

// ====================== 主循环（极简稳定） ======================
void loop() {
  // 非阻塞执行模块1检测
  if (millis() - lastCheckTime >= CHECK_INTERVAL) {
    checkPowerAndControlRelay();
    lastCheckTime = millis();
  }
  // 处理网页请求（仅AP热点内的访问）
  server.handleClient();
}
