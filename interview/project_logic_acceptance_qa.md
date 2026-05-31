# MiniSQL 项目验收代码逻辑拷问清单

> 由Codex (ChatGPT-5.4-high) 整理.

这份文档按项目验收的 7 个系统模块重排，重点不是 C++ 语言细节，而是你在答辩时如何把“模块目标、核心数据结构、关键流程、边界处理、已知取舍”讲清楚. 

建议回答模板：
- 先说这个模块解决什么问题. 
- 再说核心状态或核心数据结构. 
- 再说主流程. 
- 最后补边界条件、失败路径和你当前实现的取舍. 

---

## Disk and Buffer Pool Manager

涉及代码：
- `src/storage/disk_manager.cpp`
- `src/buffer/buffer_pool_manager.cpp`
- `src/buffer/lru_replacer.cpp`

### DiskManager

#### 问题 1：为什么逻辑页号和物理页号不是一一直接对应？

参考回答：
- 因为磁盘文件里除了数据页，还有 meta page 和各个 extent 的 bitmap page. 
- 逻辑页号只表示“第几个数据页”. 
- 物理页号则要把这些管理页也算进去. 
- 所以需要 `MapPageId()` 把逻辑页映射成真实文件偏移对应的物理页. 

#### 问题 2：`AllocatePage()` 的分配流程是什么？

参考回答：
- 先读取 meta page，拿到当前 extent 数量. 
- 遍历每个 extent 的 bitmap page，找有没有空闲 slot. 
- 如果某个 bitmap 能分到页，就立刻更新 bitmap、更新 meta 里的计数，然后返回对应逻辑页号. 
- 如果所有 extent 都满了，就创建新 extent 的 bitmap，并在新 extent 的第一个空位上分配. 

#### 问题 3：为什么分配页时要同时更新 `extent_used_page_` 和 `num_allocated_pages_`？

参考回答：
- `extent_used_page_` 是每个 extent 的局部使用计数. 
- `num_allocated_pages_` 是整库范围内的全局统计. 
- 一个描述局部，一个描述整体，两者都要维护，否则后续恢复和状态判断会不准. 

#### 问题 4：`DeAllocatePage()` 为什么只清 bitmap 和 meta，不缩小物理文件？

参考回答：
- 当前实现的回收语义是“该逻辑页以后可重用”，不是“物理文件立即缩容”. 
- 清掉 bitmap 后，这个逻辑页就能再次被 Allocate. 
- 不主动缩文件可以避免搬移后续页，逻辑更简单. 

#### 问题 5：为什么 `IsPageFree()` 不能靠页面内容是否全 0 判断？

参考回答：
- 因为“全 0”不等于“未分配”，可能只是页面内容刚好被初始化为空. 
- 页是否空闲的权威信息在 bitmap，而不是数据页内容本身. 

### LRUReplacer

#### 问题 6：为什么 `Victim()` 选的是 `frames.back()`？

参考回答：
- 这里 `Unpin()` 把新进入 replacer 的 frame 放到 `front()`. 
- 所以头部是最近刚变成可替换的页，尾部是最久没被访问的页. 
- 从 `back()` 淘汰才符合 LRU 语义. 

#### 问题 7：为什么 `Pin(frame_id)` 要把 frame 从 replacer 里删掉？

参考回答：
- replacer 只维护“当前可替换”的 frame. 
- 一旦页面被 pin，说明有人正在使用，不能被换出. 
- 所以 `Pin()` 的真正语义是“从可淘汰集合里移除”. 

#### 问题 8：`Unpin(frame_id)` 为什么要先判重？

参考回答：
- 因为同一个 frame 可能被重复调用 `Unpin`. 
- 如果不判重，链表里会出现重复 frame，后续淘汰和删除都会出错. 
- 这一步是在维护 replacer 内部状态一致性. 

#### 问题 9：`Size()` 为什么没加锁？这算不算问题？

参考回答：
- 严格说是有风险的，因为 `Victim/Pin/Unpin` 都会并发修改链表. 
- 如果在高并发下读 `Size()`，可能看到非严格一致的快照. 
- 更稳妥的写法是 `Size()` 也加锁. 

### BufferPoolManager

#### 问题 10：`FetchPage()` 的完整流程是什么？

参考回答：
- 先查 `page_table_` 看页是否已经在内存. 
- 如果命中，就 `pin_count_++`，并把对应 frame 从 replacer 中移除. 
- 如果没命中，优先从 `free_list_` 取空闲 frame. 
- `free_list_` 空了之后，再从 replacer 里选 victim. 
- 若 victim 是 dirty，就先刷盘，再从 `page_table_` 删除旧映射. 
- 最后把目标页读入这个 frame，更新 `page_id_ / pin_count_ / dirty / page_table_`. 

#### 问题 11：为什么一定是“先 free list，后 replacer”？

参考回答：
- free list 里的 frame 不承载有效页，直接拿来用成本最低. 
- 只有没有空闲 frame 时，才需要替换已有页面. 
- 这样可以减少不必要的刷盘和元数据维护. 

#### 问题 12：命中页后为什么还要调用 `replacer_->Pin(fid)`？

参考回答：
- 命中只说明页在 buffer pool 中，不代表它现在不能被淘汰. 
- 本次 `FetchPage()` 返回给上层后，它就处于使用中状态. 
- 所以必须确保它不在 replacer 里. 

#### 问题 13：为什么 victim 是脏页时一定要先写回磁盘？

参考回答：
- 因为 victim 所在 frame 之后要被新页覆盖. 
- 如果旧页改过但没刷盘，覆盖后修改会直接丢失. 
- 所以替换路径必须保证“刷脏页 -> 删旧映射 -> 装新页”的顺序. 

#### 问题 14：`NewPage()` 为什么先找 frame，再 `AllocatePage()`？

参考回答：
- 因为如果所有 frame 都被 pin 住，就应该直接返回 `nullptr`. 
- 若先分配逻辑页号，再发现装不进去，会浪费页号，甚至造成元数据语义不一致. 

#### 问题 15：`DeletePage()` 为什么必须检查 `pin_count_ > 0`？

参考回答：
- `pin_count_ > 0` 说明还有上层在用这页. 
- 这时删除会让其他逻辑继续拿着一块已经失效的数据工作. 
- 所以只能删除没人持有的页. 

#### 问题 16：`UnpinPage()` 更新 dirty 为什么用 `|=`？

参考回答：
- 因为页面只要曾经被改脏过，在真正刷盘前就必须保持 dirty. 
- 当前这次 `UnpinPage(false)` 不能把之前的 dirty 覆盖回去. 
- 所以用累积语义更正确. 

#### 问题 17：为什么只有 `pin_count_` 变成 0 才能放回 replacer？

参考回答：
- 因为只有最后一个使用者释放后，这页才重新变成“可替换”. 
- 在 `pin_count_ > 0` 时把页放回 replacer，会导致正在使用的页被错误淘汰. 

#### 问题 18：`pin_count_` 和页锁 `WLatch/RLatch` 分别解决什么问题？

参考回答：
- `pin_count_` 解决的是“能不能被替换”. 
- 页锁解决的是“页内容能不能被并发读写”. 
- 一个是缓存替换语义，一个是并发互斥语义，不能混为一谈. 

#### 问题 19：为什么这里用了 `recursive_mutex`？

参考回答：
- 当前写法希望同一线程在某些嵌套调用路径里可以重复持有这把锁. 
- 这能先保证正确性，但从工程角度看也说明锁边界还有继续梳理空间. 

---

## Record Manager

涉及代码：
- `src/storage/table_heap.cpp`
- `src/storage/table_iterator.cpp`
- 以及相关 `TablePage / Row / Schema` 配合逻辑

### TableHeap

#### 问题 1：`InsertTuple()` 为什么从 `first_page_id_` 开始顺着页链找？

参考回答：
- 这份实现把表组织成一条 `TablePage` 链. 
- 每个页存一部分 tuple，通过 `next_page_id` 串起来. 
- 插入时优先复用已有页，只有整条链都没空间时才扩新页. 

#### 问题 2：为什么不直接在尾页建新页，而是先尝试当前页插入？

参考回答：
- 因为先复用已有页能提升空间利用率，避免表文件过快膨胀. 
- 只有当前页真的放不下，且后面也没有合适页时，才扩容. 

#### 问题 3：为什么当前页插不下后要继续看 `next_page_id`？

参考回答：
- 因为当前页空间不足不代表整张表空间不足. 
- 后继页仍可能有可用空间. 
- 这是 heap table 插入的自然路径. 

#### 问题 4：为什么建新页时要先 `Init`，再把旧页的 `next_page_id` 指过去？

参考回答：
- 因为新页首先要具备合法的页内元数据. 
- `Init` 是把 `page_id / prev_page_id / 其它状态` 建起来. 
- 再由旧尾页把它接到链表后面，结构才完整. 

#### 问题 5：为什么 `InsertTuple()`、`UpdateTuple()`、`MarkDelete()` 都是写锁？

参考回答：
- 因为这些操作都会改页内 tuple 布局、slot 状态或者删除标记. 
- 读锁不够，必须用 `WLatch()` 保护结构修改. 

#### 问题 6：`UpdateTuple()` 为什么要区分“页内能原地更新”和“空间不足”？

参考回答：
- 原地更新的前提是新 tuple 还能放回原有页布局. 
- 如果新记录更大，页内空间可能不够. 
- 这种情况下更完整的实现应该做 delete + insert 重定位. 
- 当前实现只识别这种情况并提示，没有继续跨页搬迁，这是已知简化点. 

#### 问题 7：`GetTuple()` 为什么只拿 `RLatch()`？

参考回答：
- 因为它只读页内容，不改页结构. 
- 所以允许多个读者并发访问. 

#### 问题 8：`DeleteTable()` 为什么要递归删整条表页链？

参考回答：
- 因为一张表的数据本来就是由多张 `TablePage` 组成. 
- 删表时不能只删第一页，而是要顺着 `next_page_id` 把整条链都释放掉. 

### 2.2 TableIterator

#### 问题 9：`TableIterator` 为什么同时保存 `rid_` 和 `row_`？

参考回答：
- `rid_` 是逻辑位置，是迭代器真正的主状态. 
- `row_` 是缓存，只有在解引用时才填充真实 tuple 内容. 
- 所以这个设计是“位置 + 懒加载缓存”的组合. 

#### 问题 10：为什么拷贝构造没有直接复制 `row_` 的内容？

参考回答：
- 因为 `row_` 只是缓存，可能已经过期，或者和当前事务上下文不再完全一致. 
- 拷贝迭代器时应该复制“指向哪条记录”，而不是复制一份旧缓存. 

#### 问题 11：`operator*()` 为什么每次都先 `destroy()` 再重新 `GetTuple()`？

参考回答：
- 因为同一个迭代器可以被多次解引用. 
- 每次解引用都应该保证 `row_` 和当前 `rid_` 对齐，而不是复用旧缓存. 

#### 问题 12：`operator++()` 的页内和页间推进逻辑是什么？

参考回答：
- 先在当前页里找下一条 tuple. 
- 如果当前页还有下一条，就只更新 `rid_`. 
- 如果当前页走完了，再跳到后继页找第一条有效 tuple. 
- 中间如果遇到空页，就继续跳过. 

#### 问题 13：`Begin()` 为什么要一直找，直到找到第一张“非空页”？

参考回答：
- 因为表页链上可能存在空页. 
- 迭代器的起点应该是整张表逻辑上的第一条记录，而不是第一张物理页. 

#### 问题 14：`End()` 为什么返回 `INVALID_ROWID` 就够了？

参考回答：
- 因为 end iterator 本质是哨兵状态. 
- 不需要真的绑定到某条记录，只要在比较和解引用前能被识别出来即可. 

---

## Index Manager

涉及代码：
- `src/index/b_plus_tree.cpp`
- `src/index/index_iterator.cpp`
- 以及相关 B+Tree page 实现

### B+Tree 初始化与查找

#### 问题 1：构造 `BPlusTree` 时为什么先去 `INDEX_ROOTS_PAGE_ID` 查 root？

参考回答：
- 因为 root page id 不是写死在内存里的，而是持久化在索引根目录页中. 
- 数据库重启后，索引恢复入口就靠这张页. 

#### 问题 2：为什么构造时还要检查 root 页 `GetSize() == 0`？

参考回答：
- 这是在防御“目录页里记录了 root id，但这个 root 实际已经无效”的情况. 
- 当前实现遇到这种情况会把树视为空，并把头页里的记录删掉. 

#### 问题 3：`FindLeafPage()` 为什么每下降一层都要 unpin 父页？

参考回答：
- 因为搜索过程中只需要继续往下，不需要一直保留整条路径. 
- 每下一层就释放上一层，可以减少 buffer pool 占用. 

### 插入

#### 问题 4：为什么 `Insert()` 分成 `StartNewTree()` 和 `InsertIntoLeaf()` 两条路径？

参考回答：
- 空树时没有根节点，必须先创建根叶子页. 
- 非空树时才是常规“定位叶子 -> 插入 -> 必要时分裂”流程. 

#### 问题 5：`InsertIntoLeaf()` 为什么用“插入前后 size 是否变化”判断重复键？

参考回答：
- 因为这个项目的 B+Tree 只支持 unique key. 
- 如果 key 已存在，叶子页不会真正新增记录数. 
- 所以前后 size 不变就表示重复插入. 

#### 问题 6：叶子分裂后为什么往父节点上传 `new_leaf->KeyAt(0)`？

参考回答：
- 因为这是新右兄弟中的最小 key. 
- 父节点正是用这个 key 来区分左右子树的边界. 

#### 问题 7：`InsertIntoParent()` 为什么 root 分裂必须单独处理？

参考回答：
- 因为 root 分裂意味着树高加一. 
- 这时不能把新兄弟直接插到某个现有父节点，而是要新建一个 internal root. 

### 删除、合并与重分配

#### 问题 8：删除后什么时候 `Coalesce`，什么时候 `Redistribute`？

参考回答：
- 先看当前节点删除后是否已经低于最小占用. 
- 如果兄弟和当前节点加起来仍不超过 max size，就直接合并 `Coalesce`. 
- 否则说明兄弟还有富余记录，可以借一部分过来，走 `Redistribute`. 

#### 问题 9：为什么 `index == 0` 时要改用右兄弟？

参考回答：
- 因为最左孩子没有左兄弟. 
- 所以默认的“向左找兄弟”策略在 `index == 0` 时必须改成右兄弟. 

#### 问题 10：merge 时为什么可能先 `swap(node, neighbor)`？

参考回答：
- 因为当前实现的合并逻辑希望统一成“把 node 搬进 neighbor”. 
- 当当前节点本来是最左孩子时，兄弟在右边，方向不一致，所以先 swap 统一处理方向. 

#### 问题 11：`AdjustRoot()` 需要处理哪些特殊情况？

参考回答：
- root 是叶子页，且删完后 size 变成 0，说明整棵树空了. 
- root 是 internal 页，但只剩一个孩子，说明树高可以下降一层，把唯一孩子提升为新 root. 

### IndexIterator

#### 问题 12：`Begin()` 为什么从最左叶子开始？

参考回答：
- 因为 B+Tree 的有序遍历必须从全局最小 key 开始. 
- 最左叶子就是最小 key 所在的叶子页. 

#### 问题 13：`Begin(key)` 为什么先定位叶子，再在页内计算 `KeyIndex`？

参考回答：
- 因为范围扫描的起点不一定是页首. 
- 要先找到包含这个 key 的叶子，再在页内定位到第一个大于等于 key 的位置. 

#### 问题 14：为什么 `IndexIterator` 析构时要 `UnpinPage(current_page_id, false)`？

参考回答：
- 因为迭代器内部持有当前叶子页. 
- 如果析构时不 unpin，这页会一直被 buffer pool 视为在使用中. 
- 本质上是把页面 pin 生命周期绑到迭代器对象生命周期上. 

---

## Catalog Manager

涉及代码：
- `src/catalog/catalog.cpp`

#### 问题 1：`CatalogManager` 构造为什么分 `init=true` 和 `init=false`？

参考回答：
- `init=true` 表示新建数据库，catalog 还不存在，要从空元数据开始. 
- `init=false` 表示加载已有数据库，要先反序列化 `CatalogMeta`，再把表和索引逐个加载回内存结构. 

#### 问题 2：`CreateTable()` 为什么要 `DeepCopySchema(schema)`？

参考回答：
- 因为表元数据需要长期持有 schema. 
- 如果直接引用外部传进来的 schema，会有生命周期风险. 
- 深拷贝后，catalog 内部对这份 schema 有稳定控制权. 

#### 问题 3：`CreateTable()` 为什么既要创建 `TableHeap`，又要新分配 table meta page？

参考回答：
- `TableHeap` 管的是实际数据页链. 
- table meta page 管的是表级描述信息，比如表 id、表名、首个数据页、schema. 
- 一个负责数据，一个负责目录元数据. 

#### 问题 4：`CreateIndex()` 为什么要先把列名解析成 `key_map`？

参考回答：
- 因为索引最终是建立在 schema 中的列位置上的. 
- 运行时访问和持久化都更依赖列下标，而不是字符串列名本身. 

#### 问题 5：`DropTable()` 为什么要先删掉这个表上的所有索引？

参考回答：
- 因为索引依附于表的 schema 和表数据. 
- 如果表没了索引还留着，就会变成悬空对象. 
- 所以必须先清关联索引，再删除表元数据和数据页. 

#### 问题 6：为什么每次修改 catalog 后都要 `FlushCatalogMetaPage()`？

参考回答：
- 因为 `CatalogMeta` 是重启恢复表和索引入口的核心目录页. 
- 如果只改了内存映射不刷回去，重启后这些对象就找不到了. 

#### 问题 7：为什么重启后还能恢复表和索引？

参考回答：
- 因为 `CatalogMeta` 里持久化了 table/index id 到 meta page 的映射. 
- 重启时先反序列化这张目录页，再按映射逐个 `LoadTable/LoadIndex`. 

---

## Planner and Executor

涉及代码：
- `src/planner/planner.cpp`
- `src/executor/execute_engine.cpp`
- 以及相关 plan / executor 实现

### Planner

#### 问题 1：`PlanQuery()` 是怎么按语句类型分派 plan 的？

参考回答：
- 它先看 AST 根节点类型. 
- `select / insert / delete / update` 分别构造不同的 statement 对象. 
- statement 负责把 syntax tree 转成更结构化的语义表示. 
- 然后再交给 `PlanSelect / PlanInsert / PlanDelete / PlanUpdate` 生成 plan tree. 

#### 问题 2：`PlanSelect()` 为什么有时走 `SeqScan`，有时走 `IndexScan`？

参考回答：
- 它会先找当前表有哪些索引. 
- 再检查 where 条件里出现的列，看看能不能和单列索引匹配. 
- 如果没有可用索引，或者条件里有 `OR`，就退化成顺序扫描. 
- 否则生成 `IndexScanPlanNode`. 

#### 问题 3：为什么 `has_or` 时直接不用索引？

参考回答：
- 因为当前实现的索引选择逻辑比较简化，没有做多索引合并或复杂布尔优化. 
- `OR` 条件下直接走 SeqScan 更稳妥，也更容易保证语义正确. 

#### 问题 4：`PlanInsert()` 为什么先生成 `ValuesPlanNode` 再包一层 `InsertPlanNode`？

参考回答：
- 因为 insert 的输入本质上也是一个“产生数据的子计划”. 
- `ValuesPlanNode` 负责提供待插入的行. 
- `InsertPlanNode` 再消费这些行，把它们落到目标表里. 

### ExecuteEngine

#### 问题 5：`ExecuteCreateTable()` 是如何从 AST 里解析出 schema 的？

参考回答：
- 先取表名节点. 
- 再顺着 `next_` 遍历列定义节点. 
- 每个列定义里先取列名，再取类型字符串. 
- 类型字符串再映射成 `TypeId` 和长度，最终组装成 `Column` 数组构成 `Schema`. 

#### 问题 6：`char(n)` 的长度为什么要靠字符串解析括号？

参考回答：
- 因为当前 parser 传到执行层的类型信息是字符串形式，例如 `char(10)`. 
- 执行器要自己把括号中的数值解析出来，再传给 `Column`. 

#### 问题 7：`ExecuteDropIndex()` 为什么要遍历所有表找这个索引属于谁？

参考回答：
- 因为当前 AST 里 drop index 只给了 index name，没有直接给 table name. 
- 而 catalog 的删除接口需要表名和索引名配对. 
- 所以只能先扫描所有表来反查归属. 

#### 问题 8：`ExecuteShowIndexes()` 为什么没索引时返回的是 `Empty set` 而不是失败？

参考回答：
- 因为“当前表没有索引”是正常查询结果，不是执行异常. 
- 所以输出空集合，但仍返回成功. 

#### 问题 9：`ExecuteExecfile()` 为什么要按分号累积 SQL 再执行？

参考回答：
- 因为一条 SQL 语句可能跨多行. 
- 如果按行直接送 parser，会把一条完整语句拆断. 
- 所以要累计到 `;` 为止，再整体进行词法和语法分析. 

#### 问题 10：为什么每条语句都要 `yy_scan_string -> yyparse -> yy_delete_buffer -> yylex_destroy`？

参考回答：
- 因为 lexer / parser 内部有输入 buffer 和状态. 
- 每处理完一条 SQL，都要释放旧状态，避免下一条语句被污染. 

#### 问题 11：`ExecuteCreateTable()` 为什么失败时要手动 `delete schema`？

参考回答：
- 因为这份 schema 是执行器临时创建的. 
- 创建表成功后，catalog 内部会深拷贝或接管相关信息. 
- 如果失败，这个临时对象没人接管，就必须手动释放. 

---

## Recovery Manager

涉及代码：
- `src/include/recovery/recovery_manager.h`
- `src/include/recovery/log_rec.h`
- `src/include/recovery/log_manager.h`

说明：
- 这个仓库里的 Recovery 更偏“测试用的简化 KV 恢复逻辑”，不是完整数据库 WAL 子系统. 
- 验收时最好主动说明这一点，避免老师按 ARIES 全流程拷问. 

### Log Record

#### 问题 1：`LogRec` 里为什么要有 `lsn_` 和 `prev_lsn_`？

参考回答：
- `lsn_` 是当前日志记录自己的编号. 
- `prev_lsn_` 把同一事务的日志串成一条链. 
- Redo 看的是全局 LSN 顺序，Undo 则常常沿事务自己的 `prev_lsn_` 逆向回退. 

#### 问题 2：为什么 `CreateInsertLog / CreateDeleteLog / CreateUpdateLog` 都要更新 `prev_lsn_map_`？

参考回答：
- 因为每次生成新日志后，它就应该成为该事务“最新的一条日志”. 
- 下次再生成日志时，新的 `prev_lsn_` 才能正确指回它. 

### RecoveryManager 流程

#### 问题 3：`Init(CheckPoint &last_checkpoint)` 在恢复开始前做了什么？

参考回答：
- 它把 checkpoint 中保存的持久化 LSN、活跃事务表、以及已持久化数据状态装回内存. 
- 这样恢复就不是从空白状态开始，而是从最近 checkpoint 之后继续. 

#### 问题 4：`RedoPhase()` 的核心逻辑是什么？

参考回答：
- 按 LSN 顺序遍历日志. 
- 跳过早于 `persist_lsn_` 的记录. 
- 对 begin / commit / abort 更新活跃事务表. 
- 对 insert / delete / update 把对应修改重新应用到内存数据库状态上. 

#### 问题 5：为什么 `Abort` 在 `RedoPhase()` 里会调用 `UndoFrom(log->prev_lsn_)`？

参考回答：
- 因为遇到 abort 说明该事务最终没有提交. 
- 所以 redo 到这条 abort 记录时，要把它之前尚未提交的修改沿事务链回滚掉. 

#### 问题 6：`UndoPhase()` 的核心思路是什么？

参考回答：
- Redo 结束后，`active_txns_` 里剩下的就是崩溃时还没完成的事务. 
- 对每个还活着的事务，从它最后一条 LSN 开始逆向 `UndoFrom()`. 
- Undo 完再把它从活跃事务表中移除. 

#### 问题 7：`UndoFrom()` 为什么沿 `prev_lsn_` 往回走？

参考回答：
- 因为 Undo 要撤销的是某个事务自己的历史操作. 
- `prev_lsn_` 正好把同一事务的日志串起来，能让恢复逆着事务执行路径回滚. 

#### 问题 8：这个 Recovery 实现和完整数据库 WAL/ARIES 相比简化在哪？

参考回答：
- 当前实现只针对测试里的 KV 数据结构. 
- 没有 pageLSN、没有脏页表、没有 CLR、没有真正的日志持久化线程和刷盘协同. 
- 逻辑重点是演示 Redo/Undo 思想，而不是完整工业实现. 

---

## Lock Manager (Optional)

涉及代码：
- `src/concurrency/lock_manager.cpp`

说明：
- 如果验收里老师不考并发，这一节可以简讲. 
- 如果老师追问事务并发控制，这一节往往会成为重点. 

#### 问题 1：为什么 `ReadUncommitted` 下请求共享锁会直接 abort？

参考回答：
- 当前实现规定 `ReadUncommitted` 不允许拿 shared lock. 
- 所以在这个隔离级别下调用 `LockShared` 会直接把事务置为 aborted，并抛对应异常. 

#### 问题 2：`LockShared()` 为什么要在“有人写锁”或“有人升级锁”时等待？

参考回答：
- 有 writer 时，共享锁当然不能再发. 
- 有 upgrader 时，如果还继续发新的 shared lock，会让升级者更难拿到独占锁，甚至造成升级饥饿. 

#### 问题 3：`LockExclusive()` 为什么要同时等 `is_writing_ == false`、`sharing_cnt_ == 0`、`!is_upgrading_`？

参考回答：
- 因为独占锁要求自己是唯一持有者. 
- 所以不能有别的 writer，也不能有任何 shared holder. 
- 同时若有人正在升级，贸然插入新的独占请求会破坏当前队列语义. 

#### 问题 4：为什么 `LockUpgrade()` 只允许一个升级者？

参考回答：
- 多个 shared holder 同时升级会形成经典升级死锁. 
- 每个人都在等别人先释放 shared lock，结果没人能继续. 
- 所以实现用 `is_upgrading_` 保证同一时刻只有一个升级者. 

#### 问题 5：升级时为什么先 `sharing_cnt_--`，但不马上从事务的 `shared_lock_set` 删掉这把锁？

参考回答：
- 队列视角上，它已经不再按普通 shared holder 统计. 
- 但事务视角上，它还没真正成功升级，立刻删除 shared set 会丢失状态信息. 
- 所以等升级成功后再正式从 shared set 转到 exclusive set. 

#### 问题 6：`Unlock()` 为什么会让事务从 `Growing` 进入 `Shrinking`？

参考回答：
- 这是两段锁协议 2PL 的核心状态变化. 
- 事务一旦开始释放锁，就不该再继续申请新锁. 
- 所以从 Growing 切到 Shrinking. 

#### 问题 7：为什么条件变量唤醒后还要检查事务是否已经 aborted？

参考回答：
- 因为事务可能在等待期间被死锁检测线程选成 victim. 
- 即使它被唤醒，也不代表它现在还应该继续争锁. 
- 所以每次唤醒后都要重新检查事务状态. 

#### 问题 8：`RunCycleDetection()` 是怎么构建 waits-for graph 的？

参考回答：
- 遍历每个 `rid` 的锁请求队列. 
- 把已经 `granted_ != None` 的请求视为 holder. 
- 把还没拿到锁的请求视为 waiter. 
- 对每个 waiter 向所有 holder 建边 `waiter -> holder`. 
- 图中出现环就说明有死锁. 

#### 问题 9：为什么这里选择 `newest_tid_in_cycle` 作为 victim？

参考回答：
- 当前策略是杀掉环里事务 id 最大的那个，规则稳定且易于测试. 
- 重点不是它绝对最优，而是必须有确定的 victim 选择策略来打破环. 

#### 问题 10：为什么把 victim 设为 aborted 后还要 `notify_all()`？

参考回答：
- 因为其他等待线程还睡在条件变量上. 
- 不唤醒，它们不会重新检查锁条件，系统就无法继续推进. 

---

## 结尾建议：老师如果让你“自我挑刺”，可以主动讲这几个点

### 点 1：Disk and Buffer Pool Manager
- `LRUReplacer::Size()` 没加锁，严格并发下可以更稳. 
- `BufferPoolManager` 用了 `recursive_mutex`，是正确性优先的实现. 

### 点 2：Record Manager
- `UpdateTuple()` 在页内空间不足时没有自动做 delete + insert 重定位. 

### 点 3：Index Manager
- 当前 B+Tree 更强调基本正确性和测试通过，没有做更复杂的并发索引控制. 

### 点 4：Planner and Executor
- `DropIndex` 需要遍历所有表反查归属，接口设计上还有优化空间. 

### 点 5：Recovery Manager
- 当前恢复实现是简化版，更偏概念验证，不是完整 ARIES/WAL 工业实现. 

### 点 6：Lock Manager
- victim 选择策略比较简化，优先保证逻辑清晰和可测试性. 

主动承认这些边界通常不会减分，反而说明你清楚自己的实现范围和取舍. 
