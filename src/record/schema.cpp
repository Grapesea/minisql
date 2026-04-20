#include "record/schema.h"

/**
 * TODO: Student Implement
 */
uint32_t Schema::SerializeTo(char *buf) const {
  uint32_t size = static_cast<uint32_t>(columns_.size());
  uint32_t offset = 0;

  MACH_WRITE_UINT32(buf + offset, SCHEMA_MAGIC_NUM);
  offset += sizeof(uint32_t);
  MACH_WRITE_UINT32(buf + offset, size);
  offset += sizeof(uint32_t);

  for (uint32_t i = 0; i < size; i++) {
    offset += columns_[i]->SerializeTo(buf + offset);
  }
  return offset;
}

uint32_t Schema::GetSerializedSize() const {
  uint32_t offset = 2 * sizeof(uint32_t);
  for (auto column : columns_) {
    offset += column->GetSerializedSize();
  }
  return offset;
}

uint32_t Schema::DeserializeFrom(char *buf, Schema *&schema) {
  uint32_t offset = 0;
  uint32_t schema_magic_num = MACH_READ_UINT32(buf + offset); // check if the magic num is correct.
  ASSERT(schema_magic_num == SCHEMA_MAGIC_NUM, "Assertion error: error buf start point.");
  offset += sizeof(uint32_t);

  uint32_t size = MACH_READ_UINT32(buf + offset);
  offset += sizeof(uint32_t);

  std::vector<Column *> columns;
  columns.reserve(size);
  for (uint32_t i = 0; i < size; i++) {
    Column *c = nullptr;
    offset += Column::DeserializeFrom(buf + offset, c);
    columns.push_back(c);
  }
  schema = new Schema(columns, true);
  return offset;
}
