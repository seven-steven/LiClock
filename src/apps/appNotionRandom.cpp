#include "AppManager.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "NotionDB.h" // 引入 NotionDB 头文件

// #define notion_width 32
// #define notion_height 32
static const uint8_t notion_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x80, 0x3F, 0x00, 0xF0, 0xFF, 0xFF, 0x00, 0x70, 0x00, 0x80, 0x01,
  0x70, 0x00, 0x00, 0x03, 0xF0, 0x00, 0xE0, 0x0F, 0xF0, 0xFF, 0xFF, 0x0F,
  0xF0, 0xFF, 0x00, 0x0C, 0xF0, 0x01, 0x00, 0x08, 0xF0, 0x01, 0x00, 0x08,
  0xF0, 0xF1, 0xF0, 0x08, 0xF0, 0xF1, 0x61, 0x08, 0xF0, 0xE1, 0x63, 0x08,
  0xF0, 0xE1, 0x63, 0x08, 0xF0, 0xE1, 0x67, 0x08, 0xF0, 0x61, 0x6F, 0x08,
  0xF0, 0x61, 0x6E, 0x08, 0xF0, 0x61, 0x7E, 0x08, 0xF0, 0x61, 0x7C, 0x08,
  0xF0, 0x61, 0x7C, 0x08, 0xF0, 0x61, 0x78, 0x08, 0xE0, 0xF1, 0x70, 0x08,
  0xE0, 0x01, 0x00, 0x08, 0xC0, 0x01, 0x00, 0x0C, 0x80, 0xC1, 0xFF, 0x0F,
  0x00, 0xFF, 0xFF, 0x07, 0x00, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, };

// 引用全局变量，不再重新定义
extern int part_refresh_count;
extern bool force_full_update;

// 退出回调函数
static void appNotionRandom_exit()
{
  force_full_update = true;
}

// 深度睡眠回调函数
static void appNotionRandom_deepsleep()
{
  // 深度睡眠前的操作
}

// 唤醒回调函数
static void appNotionRandom_wakeup();

class AppNotionRandom : public AppBase
{
private:
  String databaseId;     // Notion 数据库ID
  String currentContent; // 当前显示的内容
  String createTime;     // 页面创建时间
  int refreshInterval;   // 刷新时间间隔(分钟)
  NotionDB notionDB;     // NotionDB 实例
  bool configValid;      // 配置是否有效

  /**
   * @brief 从公开Notion数据库中随机获取一个页面的内容
   *
   * @return true 获取成功
   * @return false 获取失败
   */
  bool getRandomPage()
  {
    Serial.println("\n[NotionRandom] 开始获取随机页面");

    // 使用 NotionDB 获取随机记录
    DynamicJsonDocument record = notionDB.getRandomRecord(databaseId);

    // 检查是否成功获取记录
    if (record.size() == 0)
    {
      Serial.println("[NotionRandom] 错误：无法获取数据库记录");
      return false;
    }

    // 提取创建时间 (如果存在)
    if (record.containsKey("Created"))
    {
      createTime = record["Created"].as<String>();
      // 处理日期格式，获取YYYY-MM-DD
      if (createTime.length() > 10)
      {
        createTime = createTime.substring(0, 10);
      }
      Serial.printf("[NotionRandom] 页面创建时间: %s\n", createTime.c_str());
    }
    else
    {
      createTime = "未知日期";
      Serial.println("[NotionRandom] 页面没有创建时间信息");
    }

    // 使用 NotionDB 提取文本内容
    currentContent = notionDB.extractText(record);

    if (currentContent.length() == 0)
    {
      Serial.println("[NotionRandom] 警告：页面内容为空");
      currentContent = "该页面没有文本内容";
    }
    else
    {
      Serial.printf("[NotionRandom] 提取的文本内容(%d字符):\n%s\n",
                    currentContent.length(), currentContent.substring(0, 200).c_str());
      if (currentContent.length() > 200)
      {
        Serial.println("...(内容过长，省略显示)");
      }
    }

    return true;
  }

  /**
   * @brief 绘制内容到墨水屏
   */
  void drawContent()
  {
    Serial.println("[NotionRandom] 开始绘制内容到屏幕");
    display.clearScreen();
    GUI::drawWindowsWithTitle("Notion 随机内容", 0, 0, 296, 128);

    // 显示创建时间
    u8g2Fonts.setCursor(10, 25);
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.printf("创建于: %s", createTime.c_str());

    // 显示内容
    u8g2Fonts.setCursor(10, 45);
    GUI::autoIndentDraw(currentContent.c_str(), 276);

    // 显示下次刷新时间
    u8g2Fonts.setCursor(10, 120);
    u8g2Fonts.printf("下次刷新: %d分钟后", refreshInterval);

    Serial.printf("[NotionRandom] 刷新显示(全刷新:%d, 局部刷新计数:%d)\n",
                  force_full_update, part_refresh_count);
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
  AppNotionRandom()
  {
    name = "notionrandom";
    title = "Notion随机";
    description = "Notion公开数据库随机内容";
    image = notion_bits;
    refreshInterval = 60; // 默认60分钟
    noDefaultEvent = true;
    configValid = false;
  }

  void setup()
  {
    Serial.println("\n[NotionRandom] 应用启动");

    // 设置回调函数
    exit = appNotionRandom_exit;
    deepsleep = appNotionRandom_deepsleep;
    wakeup = appNotionRandom_wakeup;

    // 从配置中读取Notion数据库ID
    if (config[PARAM_NOTION_DATABASE] && config[PARAM_NOTION_DATABASE].as<String>().length() > 0)
    {
      databaseId = config[PARAM_NOTION_DATABASE].as<String>();
      Serial.println("[NotionRandom] 从配置中读取数据库ID: " + databaseId);
    }
    else
    {
      // 数据库ID未配置
      databaseId = "";
      Serial.println("[NotionRandom] 错误: 数据库ID未配置");
    }

    // 从配置中读取刷新时间间隔
    if (config[PARAM_NOTION_REFRESH] && config[PARAM_NOTION_REFRESH].as<int>() > 0)
    {
      refreshInterval = config[PARAM_NOTION_REFRESH].as<int>();
    }
    else
    {
      refreshInterval = 60; // 默认60分钟
      Serial.println("[NotionRandom] 使用默认刷新间隔: 60分钟");
    }

    // 确保刷新间隔在合理范围内
    if (refreshInterval < 1)
      refreshInterval = 1; // 最小1分钟
    if (refreshInterval > 1440)
      refreshInterval = 1440; // 最大24小时(1440分钟)

    Serial.printf("[NotionRandom] 配置 - 数据库ID: %s, 刷新间隔: %d分钟\n",
                  databaseId.c_str(), refreshInterval);

    // 验证数据库ID是否配置
    if (databaseId.length() == 0)
    {
      Serial.println("[NotionRandom] 错误: 数据库ID未配置");
      GUI::msgbox("配置错误", "请先在Web配置页面设置Notion数据库ID");
      configValid = false;
      appManager.goBack();
      return;
    }
    configValid = true;

    // 连接WiFi并获取内容
    Serial.println("[NotionRandom] 开始连接WiFi...");
    hal.autoConnectWiFi();

    // 获取随机页面内容
    bool success = getRandomPage();

    // 断开WiFi连接
    Serial.println("[NotionRandom] 断开WiFi连接");
    WiFi.disconnect();

    if (!success)
    {
      Serial.println("[NotionRandom] 获取Notion内容失败");
      GUI::msgbox("错误", "获取Notion内容失败");
      appManager.goBack();
      return;
    }

    // 显示内容
    drawContent();

    // 设置下次唤醒时间
    Serial.printf("[NotionRandom] 设置下次唤醒时间为%d秒后\n", refreshInterval * 60);
    appManager.nextWakeup = refreshInterval * 60;
    appManager.noDeepSleep = false;
  }
};

// 创建应用实例
static AppNotionRandom app;

// 唤醒回调函数的实现
static void appNotionRandom_wakeup()
{
  // 重新执行setup()
  app.setup();
}