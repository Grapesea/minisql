#include "record/row.h"

/**
 * TODO: Student Implement
 */
uint32_t Row::SerializeTo(char *buf, Schema *schema) const { 
  // 一开始就没看懂，先用中文吐槽一下：
  // 这里的意思是row遵循着schema的变量存储规则，等待序列化的数据存放在Row也就是self里面，
  // buf是等待被写入的内存的起始指针.
  ASSERT(schema != nullptr, "Invalid schema before serialize.");
  ASSERT(schema->GetColumnCount() == fields_.size(), "Fields size do not match schema's column size.");

  uint32_t offset = 0;
  uint32_t field_num = schema->GetColumnCount();

  // Header: field num.
  MACH_WRITE_UINT32(buf, field_num);
  offset = sizeof(uint32_t);

  // Header: null bitmap (1 means NULL), 
  uint32_t bitmap_size = (field_num + 7) / 8; // waste some bits to calc up to bytes
  memset(buf + offset, 0, bitmap_size);
  for (uint32_t i = 0; i < field_num; i++) {
    if (fields_[i]->IsNull()) {
      buf[offset + i / 8] |= static_cast<char>(1U << (i % 8));
    }
  }
  offset += bitmap_size;

  // serialize all non-null fields.
  for (uint32_t i = 0; i < field_num; i++) {
    if (!fields_[i]->IsNull()) {
      auto new_off = fields_[i]->SerializeTo(buf + offset);
      offset += new_off;
    }
  }
  return offset;
}

uint32_t Row::DeserializeFrom(char *buf, Schema *schema) {
  ASSERT(schema != nullptr, "Invalid schema before serialize.");
  ASSERT(fields_.empty(), "Non empty field in row.");
  uint32_t offset = 0;
  uint32_t field_num_buf = MACH_READ_UINT32(buf + offset);
  offset += sizeof(uint32_t);
  uint32_t field_num = schema->GetColumnCount();
  ASSERT(field_num == field_num_buf, "Wrong data in buffer");

  uint32_t bitmap_size = (field_num + 7) / 8;
  const char *bmap = buf + offset;

  offset += bitmap_size;

  for (uint32_t i = 0; i < field_num; i++) {
    bool is_null = ((bmap[i / 8] >> (i % 8)) & 1U) != 0;
    Field *f = nullptr;
    TypeId type = schema->GetColumn(i)->GetType();
    offset += Field::DeserializeFrom(buf + offset, type, &f, is_null);
    fields_.push_back(f);
  }
  return offset;
}

uint32_t Row::GetSerializedSize(Schema *schema) const {
  ASSERT(schema != nullptr, "Invalid schema before serialize.");
  ASSERT(schema->GetColumnCount() == fields_.size(), "Fields size do not match schema's column size.");

  uint32_t offset = sizeof(uint32_t);
  uint32_t column_num = schema->GetColumnCount();
  uint32_t bitmap_size = (column_num + 7) / 8;
  offset += bitmap_size;

  for (uint32_t i = 0; i < column_num; i++){
    if (!fields_[i]->IsNull()){
      offset += fields_[i]->GetSerializedSize(); // Can not use 'GetLength'! It will miss 4 per round in length. 
    }
  }

  return offset;
}

void Row::GetKeyFromRow(const Schema *schema, const Schema *key_schema, Row &key_row) {
  auto columns = key_schema->GetColumns();
  std::vector<Field> fields;
  uint32_t idx;
  for (auto column : columns) {
    schema->GetColumnIndex(column->GetName(), idx);
    fields.emplace_back(*this->GetField(idx));
  }
  key_row = Row(fields);
}
