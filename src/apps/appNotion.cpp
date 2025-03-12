#include "AppManager.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Notion 图标位图数据
static const uint8_t notion_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x1f, 0x00, 0x00, 0xfe, 0x7f, 0x00,
    0x80, 0x1f, 0xf8, 0x01, 0xc0, 0x03, 0xe0, 0x03, 0xe0, 0x01, 0x80, 0x07,
    0xf0, 0x00, 0x00, 0x0f, 0x78, 0x80, 0x01, 0x1e, 0x38, 0x80, 0x01, 0x1c,
    0x1c, 0x80, 0x01, 0x38, 0x1c, 0x80, 0x01, 0x30, 0x0e, 0x80, 0x01, 0x70,
    0x0e, 0x80, 0x01, 0x70, 0x06, 0x80, 0x01, 0x60, 0x06, 0x80, 0x01, 0x60,
    0x06, 0x80, 0x01, 0x60, 0x06, 0x80, 0x03, 0x60, 0x06, 0x80, 0x07, 0x60};

class AppNotion : public AppBase
{
private:
  String notionToken;
  String databaseId;
  String currentContent;
  String createTime;
  int refreshInterval; // 刷新时间间隔(小时)
  DynamicJsonDocument doc{4096};

  bool getRandomPage()
  {
    Serial.println("\n[Notion] 开始获取随机页面");
    HTTPClient http;
    String url = "http://192.168.1.27:8888/v1/databases/" + databaseId + "/query";
    Serial.println("[Notion] 请求URL: " + url);

    http.begin(url);
    http.addHeader("Authorization", "Bearer " + notionToken);
    http.addHeader("Notion-Version", "2022-06-28");
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"page_size\": 100}";
    Serial.println("[Notion] 发送POST请求...");
    int httpCode = http.POST(payload);

    if (httpCode != HTTP_CODE_OK)
    {
      Serial.printf("[Notion] HTTP请求失败, 状态码: %d\n", httpCode);
      http.end();
      return false;
    }

    String response = http.getString();
    Serial.println("[Notion] 收到响应数据长度: " + String(response.length()) + " 字节");
    http.end();

    DeserializationError error = deserializeJson(doc, response);
    if (error)
    {
      Serial.printf("[Notion] JSON解析失败: %s\n", error.c_str());
      return false;
    }

    JsonArray results = doc["results"];
    Serial.printf("[Notion] 数据库中共有 %d 个页面\n", results.size());
    if (results.size() == 0)
    {
      Serial.println("[Notion] 错误：数据库为空");
      return false;
    }

    // 随机选择一个页面
    int randomIndex = random(results.size());
    Serial.printf("[Notion] 随机选择第 %d 个页面\n", randomIndex + 1);
    JsonObject page = results[randomIndex];

    // 获取创建时间
    createTime = page["created_time"].as<String>();
    createTime = createTime.substring(0, 10); // 只保留日期部分
    Serial.printf("[Notion] 页面创建时间: %s\n", createTime.c_str());

    // 获取页面内容
    String pageId = page["id"].as<String>();
    Serial.printf("[Notion] 页面ID: %s\n", pageId.c_str());
    url = "http://192.168.1.27:8888/v1/blocks/" + pageId + "/children";

    http.begin(url);
    http.addHeader("Authorization", "Bearer " + notionToken);
    http.addHeader("Notion-Version", "2022-06-28");

    Serial.println("[Notion] 获取页面内容...");
    httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
      Serial.printf("[Notion] 获取页面内容失败, 状态码: %d\n", httpCode);
      http.end();
      return false;
    }

    response = http.getString();
    Serial.println("[Notion] 页面内容数据长度: " + String(response.length()) + " 字节");
    http.end();

    DynamicJsonDocument contentDoc(8192);
    error = deserializeJson(contentDoc, response);
    if (error)
    {
      Serial.printf("[Notion] 页面内容JSON解析失败: %s\n", error.c_str());
      return false;
    }

    // 提取文本内容
    currentContent = "";
    JsonArray blocks = contentDoc["results"];
    Serial.printf("[Notion] 页面包含 %d 个文本块\n", blocks.size());
    for (JsonObject block : blocks)
    {
      if (block["type"] == "paragraph")
      {
        JsonArray text = block["paragraph"]["rich_text"];
        for (JsonObject textBlock : text)
        {
          currentContent += textBlock["plain_text"].as<String>();
        }
        currentContent += "\n";
      }
    }
    Serial.printf("[Notion] 提取的文本内容(%d字符):\n%s\n", currentContent.length(), currentContent.c_str());

    return true;
  }

  void drawContent()
  {
    Serial.println("[Notion] 开始绘制内容到屏幕");
    display.clearScreen();
    GUI::drawWindowsWithTitle("Notion 随机内容", 0, 0, 296, 128);

    // 显示创建时间
    u8g2Fonts.setCursor(10, 25);
    u8g2Fonts.printf("创建于: %s", createTime.c_str());

    // 显示内容
    u8g2Fonts.setCursor(10, 45);
    GUI::autoIndentDraw(currentContent.c_str(), 276);

    Serial.printf("[Notion] 刷新显示(全刷新:%d, 局部刷新计数:%d)\n", force_full_update, part_refresh_count);
    display.display(force_full_update || part_refresh_count > 20);

    if (force_full_update || part_refresh_count > 20)
    {
      force_full_update = false;
      part_refresh_count = 0;
    }
    else
    {
      part_refresh_count++;
    }
  }

public:
  AppNotion()
  {
    name = "notion";
    title = "Notion";
    description = "Notion 随机内容";
    image = notion_bits;
    refreshInterval = 1; // 默认1小时
    noDefaultEvent = true;
  }

  void setup()
  {
    Serial.println("\n[Notion] 应用启动");
    // 从配置中读取 Notion Token 和 Database ID
    notionToken = "secret_rydzJt4x3sy6vKtZNefpbYmeomsyS3Kpwf8nI120AmC";
    databaseId = "2802f8ddbb294101b52d698c5f37aba9";

    // 从配置中读取刷新时间间隔
    if (config["notion_refresh_interval"])
    {
      Serial.printf("[Notion] 从配置读取刷新间隔: %d小时\n", config["notion_refresh_interval"].as<int>());
      refreshInterval = config["notion_refresh_interval"].as<int>();
      if (refreshInterval < 1)
        refreshInterval = 1; // 最小1小时
      if (refreshInterval > 24)
        refreshInterval = 24; // 最大24小时
    }
    Serial.printf("[Notion] 最终刷新间隔设置为: %d小时\n", refreshInterval);

    if (notionToken.length() == 0 || databaseId.length() == 0)
    {
      Serial.println("[Notion] 错误: Token或DatabaseID未配置");
      GUI::msgbox("错误", "请先配置 Notion Token 和 Database ID");
      appManager.goBack();
      return;
    }

    // 连接 WiFi 并获取内容
    Serial.println("[Notion] 开始连接WiFi...");
    hal.autoConnectWiFi();
    if (!getRandomPage())
    {
      Serial.println("[Notion] 获取Notion内容失败");
      GUI::msgbox("错误", "获取 Notion 内容失败");
      WiFi.disconnect();
      appManager.goBack();
      return;
    }
    Serial.println("[Notion] 断开WiFi连接");
    WiFi.disconnect();

    // 显示内容
    drawContent();

    // 设置下次唤醒时间(refreshInterval小时)
    Serial.printf("[Notion] 设置下次唤醒时间为%d秒后\n", refreshInterval * 3600);
    appManager.nextWakeup = refreshInterval * 3600;
    appManager.noDeepSleep = false;
  }
};

static AppNotion app;