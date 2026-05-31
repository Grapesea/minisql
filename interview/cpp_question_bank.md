# MiniSQL C++ 面试拷问题库

> 由Codex (ChatGPT-5.4-high) 整理.

建议使用方式：
- 先从“初级必问”挑 5 到 8 题，确认基础是否扎实. 
- 再从“中级深挖”追 3 到 5 题，看是否真的读过项目. 
- 最后从“高级追杀”挑 2 到 3 题，看工程判断力和边界意识. 
- 每道题都尽量要求候选人结合文件解释，不接受只背概念. 

## 一、初级必问

### 1. 虚函数与多态

1. 为什么抽象基类 `Index` 的析构函数必须是 `virtual`？
代码锚点：[src/include/index/index.h](../src/include/index/index.h)

2. `override` 的作用是什么？如果子类漏写 `override` 会怎样？
代码锚点：[src/include/index/b_plus_tree_index.h](../src/include/index/b_plus_tree_index.h)

3. 纯虚函数是什么？`Index` 为什么是抽象类？
代码锚点：[src/include/index/index.h](../src/include/index/index.h)

4. 运行时多态和编译期多态分别是什么？在这个项目里各举一处. 
代码锚点：[src/include/index/index.h](../src/include/index/index.h), [src/include/index/basic_comparator.h](../src/include/index/basic_comparator.h)

### 2. 拷贝控制与对象语义

1. 什么是 Rule of Three、Rule of Five、Rule of Zero？
代码锚点：[src/include/record/field.h](../src/include/record/field.h)

2. `Field` 为什么需要自定义析构函数和拷贝构造？
代码锚点：[src/include/record/field.h](../src/include/record/field.h)

3. `Field::operator=(Field &other)` 这里为什么不够规范？为什么一般写成 `const Field &other`？
代码锚点：[src/include/record/field.h](../src/include/record/field.h)

4. 拷贝构造和拷贝赋值的区别是什么？什么时候分别触发？
代码锚点：[src/include/record/field.h](../src/include/record/field.h), [src/storage/table_iterator.cpp](../src/storage/table_iterator.cpp)

5. 为什么返回 `T&` 而不是返回 `T`？
代码锚点：[src/include/common/singleton.h](../src/include/common/singleton.h), [src/storage/table_iterator.cpp](../src/storage/table_iterator.cpp)

### 3. 指针、引用、const

1. 指针和引用的区别是什么？项目里各自适合哪些场景？
代码锚点：[src/include/index/index.h](../src/include/index/index.h)

2. `const Row &key`、`Txn *txn`、`RowId row_id` 为什么采用不同的传参方式？
代码锚点：[src/include/index/index.h](../src/include/index/index.h)

3. `const` 成员函数是什么意思？为什么 `GetState()` 最好加 `const`？
代码锚点：[src/include/concurrency/txn.h](../src/include/concurrency/txn.h)

4. `const GenericKey *` 和 `GenericKey *` 的差别是什么？
代码锚点：[src/include/index/b_plus_tree.h](../src/include/index/b_plus_tree.h)

### 4. 模板基础

1. 为什么模板通常写在头文件里？
代码锚点：[src/include/common/singleton.h](../src/include/common/singleton.h), [src/include/index/basic_comparator.h](../src/include/index/basic_comparator.h)

2. `template <typename N> bool CoalesceOrRedistribute(N *&node, ...)` 里的 `N *&` 是什么意思？
代码锚点：[src/include/index/b_plus_tree.h](../src/include/index/b_plus_tree.h)

3. 模板和虚函数的适用场景有何不同？
代码锚点：[src/include/index/basic_comparator.h](../src/include/index/basic_comparator.h), [src/include/index/index.h](../src/include/index/index.h)

### 5. RAII 与资源管理

1. 什么是 RAII？这个项目里有哪些地方体现了 RAII？
代码锚点：[src/index/index_iterator.cpp](../src/index/index_iterator.cpp), [src/include/common/singleton.h](../src/include/common/singleton.h)

2. 反过来说，这个项目里哪些地方明显没有用好 RAII？
代码锚点：[src/common/instance.cpp](../src/common/instance.cpp), [src/include/index/generic_key.h](../src/include/index/generic_key.h)

3. 为什么现代 C++ 更推荐 `unique_ptr` 而不是裸指针管理资源？
代码锚点：[src/include/common/instance.h](../src/include/common/instance.h), [src/common/instance.cpp](../src/common/instance.cpp)

## 二、中级深挖

### 6. 单例、静态对象与线程安全

1. `static T instance;` 为什么在 C++11 后是线程安全初始化？
代码锚点：[src/include/common/singleton.h](../src/include/common/singleton.h)

2. 函数内静态对象的构造时机和析构时机是什么？
代码锚点：[src/include/common/singleton.h](../src/include/common/singleton.h)

3. 这个 `Singleton` 只禁了拷贝，没有禁移动，会不会有问题？
代码锚点：[src/include/common/singleton.h](../src/include/common/singleton.h)

### 7. 内存分配、布局和未定义行为

1. `malloc/free` 和 `new/delete` 的本质区别是什么？
代码锚点：[src/include/index/generic_key.h](../src/include/index/generic_key.h), [src/buffer/buffer_pool_manager.cpp](../src/buffer/buffer_pool_manager.cpp)

2. `GenericKey` 里的 `char data[0]` 是什么技巧？标准吗？为什么危险？
代码锚点：[src/include/index/generic_key.h](../src/include/index/generic_key.h)

3. `InitKey()` 里 `malloc` 分配后应该用什么释放？为什么注释里的 “remember delete” 是错的？
代码锚点：[src/include/index/generic_key.h](../src/include/index/generic_key.h)

4. `reinterpret_cast<LeafPage *>(FetchPage(...)->GetData())` 有哪些前提条件？
代码锚点：[src/index/index_iterator.cpp](../src/index/index_iterator.cpp)

5. `Page::GetLSN()` 为什么可能踩到对齐和严格别名问题？
代码锚点：[src/include/page/page.h](../src/include/page/page.h)

6. `memcpy` 与 `reinterpret_cast` 在序列化场景里分别适用什么边界？
代码锚点：[src/include/common/macros.h](../src/include/common/macros.h), [src/include/page/page.h](../src/include/page/page.h)

### 8. 异常安全

1. `DBStorageEngine` 构造函数里如果 `new BufferPoolManager` 成功、后面 `new CatalogManager` 抛异常，会发生什么？
代码锚点：[src/common/instance.cpp](../src/common/instance.cpp)

2. 什么是基本异常安全、强异常安全、无异常保证？

3. `Field` 的 copy-swap 风格赋值大概属于什么异常安全级别？
代码锚点：[src/include/record/field.h](../src/include/record/field.h)

4. 为什么构造函数里混用裸指针和手动 `new` 很难写对异常安全？
代码锚点：[src/common/instance.cpp](../src/common/instance.cpp)

### 9. 头文件设计

1. 为什么不建议在头文件中写 `using namespace std;`？
代码锚点：[src/include/buffer/buffer_pool_manager.h](../src/include/buffer/buffer_pool_manager.h)

2. 前向声明和 `#include` 的取舍是什么？什么时候前向声明不够？
代码锚点：[src/include/storage/table_iterator.h](../src/include/storage/table_iterator.h)

3. 头文件里放实现会带来哪些编译和链接影响？
代码锚点：[src/include/common/singleton.h](../src/include/common/singleton.h)

### 10. 迭代器与运算符重载

1. 前置 `++it` 和后置 `it++` 的区别是什么？为什么前置更常被推荐？
代码锚点：[src/storage/table_iterator.cpp](../src/storage/table_iterator.cpp), [src/index/index_iterator.cpp](../src/index/index_iterator.cpp)

2. `operator*`、`operator->` 应该满足什么语义约束？
代码锚点：[src/storage/table_iterator.cpp](../src/storage/table_iterator.cpp)

3. 为什么 `operator!=` 通常可以由 `operator==` 推导出来？
代码锚点：[src/storage/table_iterator.cpp](../src/storage/table_iterator.cpp)

4. `IndexIterator` 析构时调用 `UnpinPage`，这个设计有什么好处？有什么风险？
代码锚点：[src/index/index_iterator.cpp](../src/index/index_iterator.cpp)

5. 迭代器是否应该有虚析构？`TableIterator` 这里为什么未必需要 `virtual`？
代码锚点：[src/include/storage/table_iterator.h](../src/include/storage/table_iterator.h)

## 三、高级追杀

### 11. 并发原语与条件变量

1. `ReaderWriterLatch` 为什么 `wait` 前后都要拿同一把 mutex？
代码锚点：[src/include/common/rwlatch.h](../src/include/common/rwlatch.h)

2. 为什么条件变量等待必须用 `while`，不能只用 `if`？
代码锚点：[src/include/common/rwlatch.h](../src/include/common/rwlatch.h), [src/concurrency/lock_manager.cpp](../src/concurrency/lock_manager.cpp)

3. `std::unique_lock` 和 `std::lock_guard` 的区别是什么？为什么 `cv.wait` 不能直接用 `lock_guard`？
代码锚点：[src/include/common/rwlatch.h](../src/include/common/rwlatch.h), [src/concurrency/lock_manager.cpp](../src/concurrency/lock_manager.cpp)

4. `BufferPoolManager` 为什么用了 `recursive_mutex`？这通常说明什么问题？
代码锚点：[src/include/buffer/buffer_pool_manager.h](../src/include/buffer/buffer_pool_manager.h)

5. `pin_count_` 和 page latch 是两套不同机制吗？分别保护什么？
代码锚点：[src/include/page/page.h](../src/include/page/page.h), [src/buffer/buffer_pool_manager.cpp](../src/buffer/buffer_pool_manager.cpp)

6. 如果把 `pin_count_` 改成原子变量，是否就可以去掉外层互斥锁？
代码锚点：[src/include/page/page.h](../src/include/page/page.h), [src/buffer/buffer_pool_manager.cpp](../src/buffer/buffer_pool_manager.cpp)

### 12. 锁管理器实现细节

1. `LockShared` 和 `LockExclusive` 的等待条件为什么不一样？
代码锚点：[src/concurrency/lock_manager.cpp](../src/concurrency/lock_manager.cpp)

2. `LockUpgrade` 为什么只能允许一个升级者？
代码锚点：[src/concurrency/lock_manager.cpp](../src/concurrency/lock_manager.cpp)

3. 什么是 2PL？`Unlock` 里事务状态从 `Growing` 切到 `Shrinking` 的依据是什么？
代码锚点：[src/concurrency/lock_manager.cpp](../src/concurrency/lock_manager.cpp), [src/include/concurrency/txn.h](../src/include/concurrency/txn.h)

4. 死锁检测为什么要维护 waits-for graph？为什么要杀死 youngest transaction？
代码锚点：[src/concurrency/lock_manager.cpp](../src/concurrency/lock_manager.cpp)

5. 条件变量唤醒后为什么要重新检查事务是否 aborted？
代码锚点：[src/concurrency/lock_manager.cpp](../src/concurrency/lock_manager.cpp)

### 13. C/C++ 互操作与 ABI

1. parser 相关文件是 C 还是 C++？混编时最容易出什么问题？
代码锚点：[src/include/parser/parser.h](../src/include/parser/parser.h), [src/CMakeLists.txt](../src/CMakeLists.txt)

2. `extern "C"` 的作用是什么？为什么 C 头文件被 C++ 调用时经常需要它？
代码锚点：[src/include/parser/parser.h](../src/include/parser/parser.h)

3. 名字改编 name mangling 是什么？它为什么影响链接？

### 14. 构建系统与语言标准

1. `CMAKE_CXX_STANDARD 17` 和手工加 `-std=gnu++11` 同时存在时会发生什么？
代码锚点：[CMakeLists.txt](../CMakeLists.txt)

2. `gnu++11` 和 `c++11` 的差异是什么？
代码锚点：[CMakeLists.txt](../CMakeLists.txt)

3. 如果项目无意中使用了 C++17 特性，但编译标志最后按 C++11 生效，会出现什么现象？
代码锚点：[CMakeLists.txt](../CMakeLists.txt)

## 四、最适合直接拷打的“项目漏洞题”

### 15. 让候选人直接找坑

1. 这个项目里最明显的资源泄漏风险在哪？
提示锚点：[src/include/index/generic_key.h](../src/include/index/generic_key.h), [src/common/instance.cpp](../src/common/instance.cpp)

2. 这个项目里最明显的 Rule of Three/Five 缺口在哪？
提示锚点：[src/include/record/field.h](../src/include/record/field.h), [src/include/storage/table_iterator.h](../src/include/storage/table_iterator.h)

3. 这个项目里最可能的未定义行为点在哪？
提示锚点：[src/include/index/generic_key.h](../src/include/index/generic_key.h), [src/include/page/page.h](../src/include/page/page.h)

4. 这个项目里最不现代 C++ 的设计是什么？你会怎么改？
提示锚点：[src/include/common/instance.h](../src/include/common/instance.h), [src/common/instance.cpp](../src/common/instance.cpp)

5. 如果你只允许重构 3 个地方以提升工程质量，你选哪里？为什么？

## 五、推荐追问路径

### 路径 A：基础真假检测

1. 先问 `virtual` 析构和 `override`. 
2. 再问 `Field` 的拷贝控制. 
3. 最后问 `malloc/new` 和 `GenericKey`. 

### 路径 B：是否真的看过项目

1. 让他解释 `IndexIterator` 如何管理 page pin. 
2. 让他解释 `TableIterator` 为什么缓存 `row_`. 
3. 让他解释 `BufferPoolManager` 的 free list 和 replacer 配合. 

### 路径 C：并发能力检测

1. 先问条件变量为什么要 `while`. 
2. 再问 `LockUpgrade` 为什么只能一个升级者. 
3. 最后问 waits-for graph 和 deadlock victim 选择. 

### 路径 D：高级工程判断

1. 让他指出构造函数异常安全问题. 
2. 让他指出 `char data[0]` 和 `reinterpret_cast` 风险. 
3. 让他给出现代化改造方案. 
