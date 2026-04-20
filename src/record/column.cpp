#include "record/column.h"

#include "glog/logging.h"

Column::Column(std::string column_name, TypeId type, uint32_t index, bool nullable, bool unique)
    : name_(std::move(column_name)), type_(type), table_ind_(index), nullable_(nullable), unique_(unique) {
  ASSERT(type != TypeId::kTypeChar, "Wrong constructor for CHAR type.");
  switch (type) {
    case TypeId::kTypeInt:
      len_ = sizeof(int32_t);
      break;
    case TypeId::kTypeFloat:
      len_ = sizeof(float_t);
      break;
    default:
      ASSERT(false, "Unsupported column type.");
  }
}

Column::Column(std::string column_name, TypeId type, uint32_t length, uint32_t index, bool nullable, bool unique)
    : name_(std::move(column_name)),
      type_(type),
      len_(length),
      table_ind_(index),
      nullable_(nullable),
      unique_(unique) {
  ASSERT(type == TypeId::kTypeChar, "Wrong constructor for non-VARCHAR type.");
}

Column::Column(const Column *other)
    : name_(other->name_),
      type_(other->type_),
      len_(other->len_),
      table_ind_(other->table_ind_),
      nullable_(other->nullable_),
      unique_(other->unique_) {}

/**
* TODO: Student Implement
*/
uint32_t Column::SerializeTo(char *buf) const {
  uint32_t offset = 0;
  MACH_WRITE_UINT32(buf + offset, COLUMN_MAGIC_NUM);
  offset += sizeof(uint32_t);

  MACH_WRITE_UINT32(buf + offset, static_cast<uint32_t>(name_.size()));
  offset += sizeof(uint32_t);
  MACH_WRITE_STRING(buf + offset, name_);
  offset += static_cast<uint32_t>(name_.size());

  MACH_WRITE_TO(TypeId, buf + offset, type_);
  offset += sizeof(TypeId);
  MACH_WRITE_UINT32(buf + offset, len_);
  offset += sizeof(uint32_t);
  MACH_WRITE_UINT32(buf + offset, table_ind_);
  offset += sizeof(uint32_t);

  MACH_WRITE_UINT32(buf + offset, static_cast<uint32_t>(nullable_));
  offset += sizeof(uint32_t);
  MACH_WRITE_UINT32(buf + offset, static_cast<uint32_t>(unique_));
  offset += sizeof(uint32_t);

  return offset;
}

/**
 * TODO: Student Implement
 */
uint32_t Column::GetSerializedSize() const {
  return sizeof(uint32_t) + MACH_STR_SERIALIZED_SIZE(name_) + sizeof(TypeId) + 4 * sizeof(uint32_t);
}

/**
 * TODO: Student Implement
 */
uint32_t Column::DeserializeFrom(char *buf, Column *&column) {
  if (column != nullptr) {
    LOG(WARNING) << "Pointer to column is not null in column deserialize." << std::endl;
  }

  uint32_t offset = 0;
  uint32_t column_magic_num = MACH_READ_UINT32(buf + offset);
  ASSERT(COLUMN_MAGIC_NUM == column_magic_num, "Asserion error: error buf start point.");
  offset += sizeof(uint32_t);

  uint32_t name_len = MACH_READ_UINT32(buf + offset);
  offset += sizeof(uint32_t);
  std::string name(buf + offset, name_len);
  offset += name_len;

  TypeId type = MACH_READ_FROM(TypeId, buf + offset);
  offset += sizeof(TypeId);

  uint32_t len = MACH_READ_UINT32(buf + offset);
  offset += sizeof(uint32_t);

  uint32_t table_ind = MACH_READ_UINT32(buf + offset);
  offset += sizeof(uint32_t);

  uint32_t nullable = MACH_READ_UINT32(buf + offset);
  offset += sizeof(uint32_t);

  uint32_t unique = MACH_READ_UINT32(buf + offset);
  offset += sizeof(uint32_t);

  if (type == kTypeChar) {
    column = new Column(name, type, len, table_ind, static_cast<bool>(nullable), static_cast<bool>(unique));
  } else {
    column = new Column(name, type, table_ind, static_cast<bool>(nullable), static_cast<bool>(unique));
  }

  return offset;
}
