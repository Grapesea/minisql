# MiniSQL开发备忘

## Buffer Pool Manager

```cpp
// DiskFileMetaPage
public:
  uint32_t num_allocated_pages_{0};
  uint32_t num_extents_{0};  // each extent consists with a bit map and BIT_MAP_SIZE pages
  uint32_t extent_used_page_[0];
```

前两个是数，后一个是可变长初始值为0的数组.

### DiskManager

debug:

```bash
b DiskManager::AllocatePage()

b DiskManager::ReadPhysicalPage(int, char*)
```





