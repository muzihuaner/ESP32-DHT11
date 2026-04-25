#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"
#include "time.h"   // 时间库

// --- 基础配置 ---
const char* ssid = "HUANWIFI";
const char* password = "asdfghjkl...";
const char* feishu_webhook_url = "https://open.feishu.cn/open-apis/bot/v2/hook/602afa60-47fa-4f92-8023-1fc05b073318";

// NTP 时间服务器（国内阿里云和NTS中心，UTC+8）
const char* ntpServer1 = "ntp.aliyun.com";
const char* ntpServer2 = "ntp.ntsc.ac.cn";
const long gmtOffset_sec = 8 * 3600;   // 北京时间 UTC+8
const int daylightOffset_sec = 0;

// --- 硬件配置 ---
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- 全局对象与变量 ---
WiFiServer server(80);
unsigned long lastFeishuTime = 0;
const unsigned long feishuInterval = 300000; // 5分钟

// 获取格式化的时间字符串
String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "时间同步中...";
  }
  char buffer[64];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  // 连接 WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi 已连接!");
  Serial.print("本地网页访问地址: http://");
  Serial.println(WiFi.localIP());

  // 启动 NTP 时间同步
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  Serial.println("正在同步网络时间...");

  server.begin();
}

void loop() {
  // 功能 1: 飞书定时推送
  unsigned long now = millis();
  if (now - lastFeishuTime >= feishuInterval || lastFeishuTime == 0) {
    lastFeishuTime = now;
    sendToFeishu();
  }

  // 功能 2: 处理网页访问
  WiFiClient client = server.available();
  if (client) {
    handleWebPage(client);
  }
}

// 网页处理逻辑（已优化视觉+时间显示）
void handleWebPage(WiFiClient client) {
  String currentLine = "";
  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      if (c == '\n') {
        if (currentLine.length() == 0) {
          float h = dht.readHumidity();
          float t = dht.readTemperature();
          String timeStr = getFormattedTime();

          // 发送 HTTP 响应
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html; charset=utf-8");
          client.println("Connection: close");
          client.println();

          // 优化后的 HTML+CSS
          client.println("<!DOCTYPE html><html><head>");
          client.println("<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>");
          client.println("<style>");
          client.println("body{font-family:'Segoe UI',Tahoma,Geneva,Verdana,sans-serif;text-align:center;");
          client.println("background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;");
          client.println("margin:0;display:flex;align-items:center;justify-content:center;}");
          client.println(".container{width:90%;max-width:360px;}");
          client.println("h2{color:white;text-shadow:0 2px 4px rgba(0,0,0,0.3);margin-bottom:25px;font-size:1.8em;}");
          client.println(".card{background:rgba(255,255,255,0.95);backdrop-filter:blur(10px);border-radius:20px;");
          client.println("padding:25px 20px;box-shadow:0 20px 40px rgba(0,0,0,0.2);}");
          client.println(".row{display:flex;justify-content:space-around;margin:15px 0;}");
          client.println(".sensor{text-align:center;width:45%;}");
          client.println(".sensor .label{font-size:0.9em;color:#666;margin-bottom:5px;}");
          client.println(".sensor .value{font-size:2.8em;font-weight:700;}");
          client.println(".temp .value{color:#ff5e57;}");
          client.println(".humi .value{color:#3498db;}");
          client.println(".time-info{margin-top:20px;font-size:0.85em;color:#888;border-top:1px solid #eee;padding-top:15px;}");
          client.println(".footer-note{font-size:0.75em;color:#aaa;margin-top:8px;}");
          client.println("</style></head><body>");

          client.println("<div class='container'>");
          client.println("<h2>🌿 环境实时监控</h2>");
          client.println("<div class='card'>");

          // 温度和湿度卡片
          client.println("<div class='row'>");
          client.println("<div class='sensor temp'><div class='label'>🌡️ 温度</div>");
          client.print("<div class='value'>"); client.print(t, 1); client.println("</div><div class='unit'>℃</div></div>");
          client.println("<div class='sensor humi'><div class='label'>💧 湿度</div>");
          client.print("<div class='value'>"); client.print(h, 1); client.println("</div><div class='unit'>%</div></div>");
          client.println("</div>");

          // 更新时间
          client.println("<div class='time-info'>");
          client.print("🕒 更新时间: "); client.print(timeStr);
          client.println("</div>");

          // 页脚提示
          client.println("<div class='footer-note'>页面每 10 秒自动刷新</div>");
          client.println("</div></div>");

          // JavaScript 定时刷新
          client.println("<script>setTimeout(function(){location.reload();},10000);</script>");
          client.println("</body></html>");
          break;
        } else {
          currentLine = "";
        }
      } else if (c != '\r') {
        currentLine += c;
      }
    }
  }
  client.stop();
}

// 飞书推送逻辑（增加时间戳）
void sendToFeishu() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) return;

    String timeStr = getFormattedTime();

    // 构建消息内容
    String msg = "📊 **环境数据定时报告**\n------------------\n";
    msg += "🌡️ 当前温度: " + String(t, 1) + " ℃\n";
    msg += "💧 当前湿度: " + String(h, 1) + " %\n";
    msg += "🕒 报告时间: " + timeStr;

    StaticJsonDocument<200> doc;
    doc["msg_type"] = "text";
    doc["content"]["text"] = msg;

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    http.begin(feishu_webhook_url);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(jsonPayload);

    Serial.printf("[飞书] 发送结果代码: %d\n", code);
    http.end();
  }
}