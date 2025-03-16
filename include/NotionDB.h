#ifndef NOTION_DB_H
#define NOTION_DB_H

#include <Arduino.h>
#include <ArduinoJson.h>

class NotionDB
{
private:
  String _baseUrl;

public:
  NotionDB();

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
  String getRecords(String databaseId, int limit = 0, int offset = 0, String sort = "", bool asc = true);

  /**
   * @brief 随机获取一条数据库记录
   *
   * @param databaseId Notion 数据库 ID
   * @return DynamicJsonDocument 返回单条记录的 JSON 文档
   */
  DynamicJsonDocument getRandomRecord(String databaseId);

  /**
   * @brief 从记录中提取文本内容
   *
   * @param record 单条记录的 JSON 文档
   * @param excludeFields 要排除的字段，默认为 "Created"
   * @return String 返回格式化后的文本内容
   */
  String extractText(DynamicJsonDocument &record, String excludeFields = "Created");
};

#endif // NOTION_DB_H