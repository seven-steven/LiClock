#include "NotionDB.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

/**
 * @brief 构造函数，初始化基础 URL
 */
NotionDB::NotionDB()
{
  _baseUrl = "https://notion-api.splitbee.io/v1/table/";
}

/**
 * @brief 获取 Notion 数据库记录
 *
 * @param databaseId Notion 数据库 ID
 * @param limit 限制获取的记录数，默认为 0（全部）
 * @param offset 起始偏移量，默认为 0
 * @param sort 排序方式，默认为空（按 Notion 默认顺序）
 * @param asc 是否升序，默认为 true
 * @return String 返回 JSON 格式的数据库记录
 */
String NotionDB::getRecords(String databaseId, int limit, int offset, String sort, bool asc)
{
  // 记录开始时间（用于性能分析）
  unsigned long startTime = millis();

  // 日志记录
  Serial.println("[NotionDB] 开始获取数据库记录");
  Serial.printf("[NotionDB] 数据库ID: %s\n", databaseId.c_str());

  // 构建 API URL
  String url = _baseUrl + databaseId;

  // 添加查询参数
  bool hasParams = false;
  if (limit > 0)
  {
    url += hasParams ? "&" : "?";
    url += "limit=" + String(limit);
    hasParams = true;
  }

  if (offset > 0)
  {
    url += hasParams ? "&" : "?";
    url += "offset=" + String(offset);
    hasParams = true;
  }

  if (sort.length() > 0)
  {
    url += hasParams ? "&" : "?";
    url += "sort=" + sort;
    url += "&asc=" + String(asc ? "true" : "false");
    hasParams = true;
  }

  Serial.printf("[NotionDB] 请求 URL: %s\n", url.c_str());

  // 创建 HTTP 客户端
  HTTPClient http;
  http.begin(url);

  // 设置请求头
  http.addHeader("Accept", "application/json");
  http.addHeader("User-Agent", "ESP32-NotionClient/1.0");

  // 发送 GET 请求
  Serial.println("[NotionDB] 发送请求...");
  int httpCode = http.GET();

  // 处理响应
  String result = "";
  if (httpCode == HTTP_CODE_OK)
  {
    result = http.getString();
    Serial.printf("[NotionDB] 成功获取响应 (%d 字节)\n", result.length());
  }
  else
  {
    Serial.printf("[NotionDB] 请求失败，HTTP 代码: %d\n", httpCode);
    result = "{\"error\":\"" + String(httpCode) + "\"}";
  }

  // 关闭连接
  http.end();

  // 记录完成时间
  unsigned long duration = millis() - startTime;
  Serial.printf("[NotionDB] 请求完成，耗时: %lu 毫秒\n", duration);

  return result;
}

/**
 * @brief 随机获取一条数据库记录
 *
 * @param databaseId Notion 数据库 ID
 * @return DynamicJsonDocument 返回单条记录的 JSON 文档
 */
DynamicJsonDocument NotionDB::getRandomRecord(String databaseId)
{
  // 创建 JSON 文档
  DynamicJsonDocument doc(16384);

  // 获取所有记录
  String jsonStr = getRecords(databaseId);

  // 解析 JSON
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (error)
  {
    Serial.printf("[NotionDB] JSON 解析失败: %s\n", error.c_str());
    return doc; // 返回空文档
  }

  // 检查是否为数组
  if (!doc.is<JsonArray>())
  {
    Serial.println("[NotionDB] 响应不是数组格式");
    return doc; // 返回空文档
  }

  // 获取数组
  JsonArray array = doc.as<JsonArray>();
  int size = array.size();

  // 检查数组大小
  if (size == 0)
  {
    Serial.println("[NotionDB] 数据库为空");
    return doc; // 返回空文档
  }

  // 随机选择一条记录
  int randomIndex = random(size);
  Serial.printf("[NotionDB] 从 %d 条记录中随机选择索引 %d\n", size, randomIndex);

  // 提取记录
  DynamicJsonDocument recordDoc(8192);
  JsonObject record = array[randomIndex];
  recordDoc.set(record);

  return recordDoc;
}

/**
 * @brief 从记录中提取文本内容
 *
 * @param record 单条记录的 JSON 文档
 * @param excludeFields 要排除的字段，默认为 "Created"
 * @return String 返回格式化后的文本内容
 */
String NotionDB::extractText(DynamicJsonDocument &record, String excludeFields)
{
  String result = "";

  // 遍历所有字段
  for (JsonPair kv : record.as<JsonObject>())
  {
    String key = kv.key().c_str();

    // 排除指定字段
    if (key == excludeFields)
    {
      continue;
    }

    // 提取值
    String value = "";
    if (kv.value().is<const char *>())
    {
      value = kv.value().as<String>();
    }
    else if (kv.value().is<int>())
    {
      value = String(kv.value().as<int>());
    }
    else if (kv.value().is<float>())
    {
      value = String(kv.value().as<float>());
    }
    else if (kv.value().is<bool>())
    {
      value = kv.value().as<bool>() ? "是" : "否";
    }

    // 添加到结果，只有值非空时才添加
    if (value.length() > 0)
    {
      result += key + ": " + value + "\n\n";
    }
  }

  return result;
}