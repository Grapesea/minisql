# MiniSQL C++ 面试标准答案要点

> 由Codex (ChatGPT-5.4-high) 整理.

## 一、初级题答案要点

### 1. `Index` 的析构函数为什么必须是 `virtual`

- 因为它是多态基类，可能通过 `Index *` 指向派生类对象. 
- 如果基类析构非虚，通过基类指针 `delete` 派生类对象会产生未定义行为，通常表现为只调用基类析构，派生成员无法正确释放. 
- 当前项目的 `Index` 设计明显就是抽象接口类. 
代码锚点：[src/include/index/index.h](../src/include/index/index.h)

### 2. `override` 的作用

- 编译期校验“这个函数确实覆盖了基类虚函数”. 
- 若签名不一致，写了 `override` 会直接编译报错，避免静默变成新的普通成员函数. 
- 面试时若候选人只说“可读性更好”不够，核心是编译期防错. 

### 3. Rule of Three / Five / Zero

- Rule of Three：如果类显式定义了析构、拷贝构造、拷贝赋值中的一个，通常要考虑另外两个. 
- Rule of Five：C++11 后还要考虑移动构造、移动赋值. 
- Rule of Zero：优先把资源管理交给现成 RAII 类型，自己不写这些特殊成员函数. 
- `Field` 是典型例子，因为它内部可能拥有 `char*`. 
代码锚点：[src/include/record/field.h](../src/include/record/field.h)

### 4. `Field` 为什么要自定义拷贝构造

- 它内部 union 可能存放 `char *`. 
- 若默认拷贝，只会浅拷贝指针，两个对象指向同一片堆内存. 
- 析构时会重复释放，或者一方修改影响另一方. 
- 当前代码对 `kTypeChar && manage_data_` 做了深拷贝，这个方向是对的. 

### 5. `Field::operator=(Field &other)` 为什么不规范

- 参数通常应是 `const Field &`，因为赋值源对象不应被修改，也应支持给 const 对象赋值. 
- 当前写法不能接收 const 右值/左值，接口过窄. 
- 返回 `Field&` 是为了支持链式赋值并避免多余拷贝. 
- 如果候选人能补充“最好实现成 copy-and-swap 且支持自赋值”，加分. 

### 6. 指针、引用、值传递怎么选

- 小对象且拷贝便宜可值传递，例如 `RowId`. 
- 大对象或不希望拷贝可用 `const T&`，例如 `Row`. 
- 可空、表达可选所有权或多态对象时常用指针，例如 `Txn *`. 
- 候选人如果能说“引用表达非空语义，指针表达可能为空”，说明理解不错. 

### 7. 模板为什么通常写在头文件

- 模板实例化发生在使用点，编译器要看到完整定义才能生成实例. 
- 若只放声明在头文件、定义在 cpp，通常会链接失败，除非做显式实例化. 

### 8. RAII 是什么

- 资源获取即初始化，对象生命周期绑定资源生命周期. 
- 构造获取资源，析构自动释放资源. 
- 项目中 `IndexIterator` 析构里 `UnpinPage` 算一种手工 RAII. 
- 更标准的 RAII 例子是锁对象 `std::unique_lock`、`std::scoped_lock`. 

## 二、中级题答案要点

### 9. 函数内静态对象为什么线程安全

- C++11 起，局部静态变量的初始化由标准保证线程安全，只会初始化一次. 
- 注意这是“初始化线程安全”，不是对象后续成员访问天然线程安全. 
- 候选人如果混淆这两点，要继续追问. 
代码锚点：[src/include/common/singleton.h](../src/include/common/singleton.h)

### 10. 这个 `Singleton` 的缺口

- 它禁用了拷贝，但没有显式禁用移动. 
- 不过由于构造函数受保护、且 `getInstance()` 返回引用，通常外部不易移动出实例. 
- 更稳妥的写法还是同时禁用移动. 
- 另外单例本身会带来测试困难、全局状态污染等工程问题. 

### 11. `malloc/free` 与 `new/delete` 区别

- `new` 分配内存并调用构造；`delete` 调析构并释放内存. 
- `malloc` 只分配原始字节；`free` 只释放字节，不调构造/析构. 
- 两套必须配对使用，混用是未定义行为. 
- 所以 `InitKey()` 用 `malloc`，就不能用 `delete` 释放. 
代码锚点：[src/include/index/generic_key.h](../src/include/index/generic_key.h)

### 12. `char data[0]` 是什么

- 是 C 时代常见的尾部柔性存储技巧，GNU 环境常支持零长数组扩展. 
- 标准 C++ 并不推荐这种写法，可移植性差. 
- 更现代的替代可用 `std::byte[]` 配合单独分配、`std::vector<char>`、或专门的可变长对象封装. 

### 13. `reinterpret_cast` 风险

- 强转本身不创建对象生命周期. 
- 需要确认底层字节区域里确实放着该类型语义上的对象，且满足对齐要求. 
- 否则可能踩到严格别名、对齐、对象生命周期相关 UB. 
- 候选人如果能提到 placement new / `std::launder` / `std::bit_cast` 的边界，属于高分. 

### 14. `Page::GetLSN()` 的问题

- `GetData() + OFFSET_LSN` 得到的是 `char *`，转成 `lsn_t *` 后直接解引用，可能不满足对齐要求. 
- 严格来说更稳妥的做法是用 `memcpy` 读写 POD 值. 
- 当前 `SetLSN` 已经用 `memcpy`，而 `GetLSN` 没统一，风格不一致. 
代码锚点：[src/include/page/page.h](../src/include/page/page.h)

### 15. 异常安全问题在哪里

- `DBStorageEngine` 构造函数里顺序 `new DiskManager`、`new BufferPoolManager`、`new CatalogManager`. 
- 若中途抛异常，已成功分配的裸指针没有 RAII 容器接管，会泄漏. 
- 用 `std::unique_ptr` 做成员或局部临时对象可显著改善. 
代码锚点：[src/common/instance.cpp](../src/common/instance.cpp)

### 16. 为什么头文件里不该 `using namespace std;`

- 会把命名空间污染扩散给所有包含它的翻译单元. 
- 容易产生命名冲突，降低可维护性. 
- 头文件比 cpp 更应克制. 
代码锚点：[src/include/buffer/buffer_pool_manager.h](../src/include/buffer/buffer_pool_manager.h)

### 17. 前置 ++ 和后置 ++ 的区别

- 前置返回自增后的对象引用，通常不产生额外临时对象. 
- 后置要保留旧值语义，通常会构造临时副本. 
- 迭代器场景下优先前置是常见建议. 
代码锚点：[src/storage/table_iterator.cpp](../src/storage/table_iterator.cpp), [src/index/index_iterator.cpp](../src/index/index_iterator.cpp)

### 18. `TableIterator` 的设计点

- `row_` 更像缓存，真实位置由 `rid_` 标识. 
- `operator*` 每次根据 `rid_` 重新拉取 tuple，可避免复制迭代器时缓存失真. 
- 若候选人能解释注释中提到的“拷贝 bug”本质是缓存状态和标识状态分离不当，加分. 
代码锚点：[src/storage/table_iterator.cpp](../src/storage/table_iterator.cpp)

## 三、高级题答案要点

### 19. 条件变量为什么必须 `while`

- 因为可能出现虚假唤醒. 
- 即使不是虚假唤醒，被其他线程抢先修改状态后，唤醒条件也可能不再成立. 
- 正确模式是“持锁检查条件，不满足则等待，醒来后重新检查”. 
代码锚点：[src/include/common/rwlatch.h](../src/include/common/rwlatch.h), [src/concurrency/lock_manager.cpp](../src/concurrency/lock_manager.cpp)

### 20. `unique_lock` 和 `lock_guard` 区别

- `lock_guard` 更轻，只负责作用域加解锁. 
- `unique_lock` 更灵活，可解锁/重锁、可转移所有权、可与条件变量配合. 
- `cv.wait` 需要在等待时临时释放锁并在唤醒后重新加锁，因此需要 `unique_lock`. 

### 21. `recursive_mutex` 说明什么

- 它允许同一线程重复加锁. 
- 常见于调用链里同一把锁可能被重入获取. 
- 但多数情况下这也说明锁边界设计不够清晰，容易掩盖架构问题. 
- 能不用通常尽量不用. 
代码锚点：[src/include/buffer/buffer_pool_manager.h](../src/include/buffer/buffer_pool_manager.h)

### 22. `pin_count_` 与 latch 的差异

- `pin_count_` 是 buffer 管理语义，表示页面是否可被替换. 
- latch 是并发互斥语义，保护页面内容的读写并发. 
- 一个解决缓存淘汰问题，一个解决数据竞争问题，不可互相替代. 
代码锚点：[src/include/page/page.h](../src/include/page/page.h)

### 23. `LockUpgrade` 为什么只能一个升级者

- 若允许多个共享锁持有者同时升级为独占锁，彼此都会等待其他共享持有者释放，极易形成升级死锁. 
- 所以通常只允许单个 upgrader，其他升级请求直接失败或等待特定规则. 
代码锚点：[src/concurrency/lock_manager.cpp](../src/concurrency/lock_manager.cpp)

### 24. 2PL 的状态切换

- 两段锁协议下，事务先处于 Growing 阶段，只能加锁. 
- 一旦开始释放锁，进入 Shrinking 阶段，之后不能再加新锁. 
- 代码里 `Unlock` 根据隔离级别和锁类型切换状态，这是 2PL 语义落地. 
代码锚点：[src/concurrency/lock_manager.cpp](../src/concurrency/lock_manager.cpp), [src/include/concurrency/txn.h](../src/include/concurrency/txn.h)

### 25. 死锁检测为什么用 waits-for graph

- 图中节点是事务，边 `T1 -> T2` 表示 `T1` 等待 `T2`. 
- 图中有环即存在死锁. 
- 检测到环后通常选 victim 回滚，项目里选择 youngest transaction，常见理由是回滚代价较低、避免饿死老事务. 
- 候选人如果只会背定义，不会解释为什么要 `notify_all` 唤醒等待线程，不够. 

### 26. `IndexIterator` 的 RAII 风险

- 好处是离开作用域时自动 unpin，减少忘记释放页面的问题. 
- 风险是如果迭代器被不小心复制、多份对象共享同一 page 状态，可能重复 unpin 或语义混乱. 
- 这类资源型对象通常应认真设计拷贝/移动语义. 
代码锚点：[src/index/index_iterator.cpp](../src/index/index_iterator.cpp)

### 27. parser 的 C/C++ 互操作要点

- C++ 编译器会做名字改编，C 不会. 
- 如果 C 函数声明被 C++ 直接包含，通常要用 `extern "C"` 保持 C 链接约定. 
- 混编时除 ABI 外，还要注意头文件兼容性、`bool`/枚举/异常边界等. 
代码锚点：[src/include/parser/parser.h](../src/include/parser/parser.h)

### 28. CMake 里的标准冲突

- 上面设了 `CMAKE_CXX_STANDARD 17`，下面又手动拼了 `-std=gnu++11`. 
- 实际最终谁生效取决于生成器和参数顺序，但这是明显冲突配置. 
- 至少说明构建配置不干净，可能导致“代码按 17 写，编译按 11 过”的混乱. 
代码锚点：[CMakeLists.txt](../CMakeLists.txt)

## 四、项目级拷打答案

### 29. 最明显的资源泄漏风险

- `GenericKey` 由 `malloc` 分配，生命周期管理不清晰，且注释误导. 
- `DBStorageEngine` 构造函数中多个裸指针 `new`，异常时可能泄漏. 
- 候选人如果还能指出 iterator/pin 生命周期也存在人为管理负担，属于加分. 

### 30. 最明显的 Rule of Five 缺口

- `Field` 已手写析构、拷贝构造、拷贝赋值，但没有显式移动构造/移动赋值. 
- `IndexIterator` / `TableIterator` 也有资源或状态语义，但拷贝/移动策略并不完整. 
- 这类类都值得追问“是否可复制、复制后语义是什么”. 

### 31. 最明显的未定义行为风险

- `char data[0]`
- 原始字节缓冲区上的 `reinterpret_cast`
- 潜在的错配释放
- 可能的对齐问题
- 若候选人只会说“可能段错误”，说明理解还不够深入. 

### 32. 更现代的改法

- 资源所有权统一改为 `std::unique_ptr`. 
- 原始字节封装改成更清晰的 buffer/object 适配层，减少裸 `reinterpret_cast`. 
- 为资源型类补全或禁用拷贝/移动语义. 
- 删除头文件中的 `using namespace std;`. 
- 清理 CMake 标准配置冲突. 

## 五、面试评分建议

### 低分表现

- 只能背定义，无法结合代码. 
- 分不清拷贝构造和赋值. 
- 分不清 `new/delete` 与 `malloc/free`. 
- 不理解条件变量为什么必须 `while`. 

### 中等表现

- 能说清大多数概念. 
- 能指出 `Field` 深拷贝、`GenericKey` 生命周期、`DBStorageEngine` 裸指针问题. 
- 能解释 `pin_count_` 与 latch 的职责区别. 

### 高分表现

- 能主动指出 UB、异常安全、移动语义缺失、构建配置冲突. 
- 能给出现代化重构方案，并说明收益与代价. 
- 面对追问能从“语言规则”落到“这个仓库里的具体 bug 风险”. 
