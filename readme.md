
# IndustrialMonitor - V2 (工业设备智能监控看板)

一个基于 C++17 与 Qt 6 构建的现代化跨平台工业级设备监控看板系统。在 V1 稳健的 UI 骨架、暗黑主题与 MVC 模型/视图渲染基础上，**V2 阶段致力于解决“工业数据本地持久化”与“底层硬件通信协议抽象”两大核心工程命题**。

## 📌 项目愿景
V2 阶段的目标是将系统从“前端视口原型”打造为“具备企业级台账管理与硬件驱动解耦的工业生产软件”。通过确立 DAO 数据库架构，打通设备台账增删改查全流程业务闭环，并定义纯虚通信接口 `IDevice`，为后续高并发多线程与真实 PLC（Modbus TCP/RTU）采集打下坚如磐石的面向对象基石。
- **生产级 SQLite 持久化**：告别纯内存假数据，引入线程安全连接池、WAL（Write-Ahead Logging）预写日志与显式事务（Transaction）机制。
- **规范 DAO 数据访问分层**：隔离所有底层 SQL 语法，采用参数化预编译绑定，杜绝 SQL 注入，实现业务对象与关系型记录的安全双向映射。
- **工业人机交互全闭环**：落地正则强校验器（`QValidator`）、设备修改参数回显、二次确认防呆销毁以及表格右键上下文快捷菜单。
- **硬件通信抽象基座**：遵循依赖倒置原则（DIP），定义纯虚通信接口 `IDevice` 与设备工厂 `DeviceFactory`，实现业务上层对具体硬件通信协议的完全解耦。

---

## 🛠 项目结构 (V2 更新)
```text
IndustrialMonitor/
├── .gitignore
├── IndustrialMonitor.pro         # qmake 主工程配置文件（引入 QT += sql 模块）
├── bin/                          # 二进制输出目录
│   ├── IndustrialMonitor.exe     # 编译生成的可执行程序
│   ├── config.ini                # 系统基础运行与凭证配置文件
│   └── industrial_monitor.db     # [V2 核心] 生产级 SQLite 本地数据库文件
├── build/                        # 构建临时目录（moc/obj/rcc/ui 隔离沉底）
├── res/                          # 资源目录（暗黑 QSS 样式表与矢量图元）
└── src/                          # 核心源码目录
    ├── common/                   # 全局公共定义与工具箱
    │   ├── DeviceDef.h           # 工业设备结构体与强类型状态枚举
    │   ├── ThemeManager.h/.cpp   # 样式引擎与 F5 热重载管理器
    │   └── ValidatorUtils.h/.cpp # [V2 规划] IP/端口/编号强校验工具箱
    ├── db/                       # [V2 核心新增] 数据库引擎底层基座
    │   ├── DatabaseManager.h     # 数据库全局连接与生命周期管理单例
    │   └── DatabaseManager.cpp   # 具名连接池、WAL 性能调优与退出安全实现
    ├── dao/                      # [V2 规划] 数据访问对象层 (Data Access Object)
    │   ├── DeviceDao.h
    │   └── DeviceDao.cpp         # 预编译 SQL 映射、设备台账安全 CRUD
    ├── hardware/                 # [V2 核心规划] 工业通信协议与驱动抽象层
    │   ├── IDevice.h             # 工业设备通信纯虚基类（核心契约）
    │   ├── DeviceFactory.h/.cpp  # 多态设备实例化工厂
    │   └── mock/
    │       ├── MockDevice.h
    │       └── MockDevice.cpp    # 虚拟仿真硬件驱动（模拟通信握手与寄存器读取）
    ├── models/                   # MVC 模型层
    │   ├── DeviceTableModel.h/.cpp # 只读高性能虚拟化表格模型
    │   └── DeviceProxyModel.h/.cpp # 多列联合模糊秒搜与数值精确排序代理
    ├── views/                    # 界面视图层
    │   ├── LoginDialog.h/.cpp    # 阻断式安全登录弹窗
    │   ├── AddDeviceDialog.h/.cpp # 新增设备表单弹窗
    │   ├── EditDeviceDialog.h/.cpp # [V2 规划] 设备参数修改与回显弹窗
    │   ├── mainwindow.h/.cpp     # 主监控控制台窗口
    │   └── StatusDelegate.h/.cpp # 像素级呼吸发光状态指示灯委托
    └── main.cpp                  # 统一应用程序入口（生命周期拦截与数据库装配）
```

---

## 📅 进度跟踪 (V2 15天挑战)

### 第 1 阶段：工业级 SQLite 持久化基座与 DAO 架构封装
- [x] **Day 01: SQLite 底层引擎封装与生命周期防御 (`DatabaseManager`)**
  - 在 `.pro` 中引入 `QT += sql` 依赖，规划 `src/db/` 物理目录。
  - 封装 Meyers 单例 `DatabaseManager`，确立物理路径绝对锚定（`bin/industrial_monitor.db`）。
  - 建立具名连接隔离机制（`IndustrialMonitor_Main_Connection`），杜绝覆写全局默认连接。
  - 注入工业级 PRAGMA 优化三剑客：`foreign_keys = ON`、`journal_mode = WAL`、`synchronous = NORMAL`。
  - 实现局部作用域强行析构关闭闭环，彻底终结 `removeDatabase` 连接池泄漏警告。
- [ ] **Day 02: 工控台账表 Schema 设计与自动迁移 (Schema Migration)**
  - 设计生产级设备表 `t_device`（编号、名称、IP、端口、状态、温度与硬件负荷缓存）。
  - 为通信 IP 字段建立唯一性与 B-Tree 索引加速。
  - 落地启动期元数据检查与自动建表机制，支持初始种子数据（Seed Data）安全注入。
- [ ] **Day 03: DAO 模式落地与预编译安全增删查 (`DeviceDao`)**
  - 编写 `DeviceDao` 数据访问对象，彻底剥离业务层与底层原生 SQL 语法。
  - 全量采用 `QSqlQuery::prepare()` 与具名占位符绑定，彻底杜绝 SQL 注入隐患。
  - 实现原生实体 `DeviceInfo` 与数据库关系型记录的双向结构映射。
- [ ] **Day 04: 数据库事务 (Transaction) 与高并发批量写入**
  - 封装批量持久化接口 `batchInsertDevices()`，利用显式事务（`transaction` / `commit` / `rollback`）保护原子性。
  - 编写压测基准比对：验证在 WAL 模式下事务批处理带来的数十倍磁盘写入吞吐提升。
- [ ] **Day 05: Model 层与 SQLite 真实数据源完全换轨**
  - 彻底铲除代码中手写的模拟假数据循环，接入 `DeviceDao::queryAllDevices()` 真实数据。
  - 规范冷启动数据回显与生命周期持久化，实现程序重启后资产台账毫发无损复原。

### 第 2 阶段：UI 业务全闭环——表单强校验、设备修改与交互增强
- [ ] **Day 06: 工控表单硬核校验器体系 (`ValidatorUtils`)**
  - 封装通用正则验证工具，注入严格的 IPv4 正则校验与 1~65535 端口范围过滤。
  - 实现表单输入动态视觉反馈（非法输入高亮告警边框，提交按钮智能禁用置灰）。
- [ ] **Day 07: 设备“编辑/修改”模态弹窗与数据回填 (`EditDeviceDialog`)**
  - 构建独立暗黑修改弹窗，传入 `DeviceInfo` 自动执行高精度数据回显。
  - 资产唯一主键（设备编号）锁定为只读置灰，防止误改主键破坏物理关联。
  - 联动 `DeviceDao::updateDevice()` 与模型层就地刷新。
- [ ] **Day 08: 设备单行删除、多选批量删除与二次模态防呆**
  - 增加工业级防误触警告弹窗（危险警示色二次确认），杜绝车间误操作直接销毁资产。
  - 联动 `DeviceDao::deleteDeviceById()` 与 `beginRemoveRows` 视口无抖动行剔除。
- [ ] **Day 09: 工业表格右键上下文菜单 (`CustomContextMenu`)**
  - 开启视口自定义菜单策略，封装“参数修改”、“安全注销”、“就地重连”、“复制地址”等动作。
  - 实现边界防御：未选中有效行时快捷菜单动作自动置灰禁用。
- [ ] **Day 10: 多维条件复合检索与数据库级分页/计数**
  - 升级首屏搜索栏：实现“文本关键字 + 运行状态下拉框”多条件复合过滤（AND 逻辑组合）。
  - 联动状态栏动态输出全域设备台账与筛选命中指标。

### 第 3 阶段：核心硬件通信抽象 (IDevice) 与 v0.2-Beta 封版交付
- [ ] **Day 11: 工业通信抽象基类 `IDevice` 架构设计**
  - 遵循依赖倒置原则（DIP），确立工业通信纯虚基类 `IDevice`（基于 `QObject` 异步信号槽）。
  - 定义连接状态机枚举（`Disconnected / Connecting / Connected / Error`）与通信核心原语。
- [ ] **Day 12: 通信连接元数据参数封装与通信协议配置**
  - 统一封装多协议网络参数类 `DeviceConfig`，覆盖 TCP 寻址与串口波特率配置。
  - 数据库表结构同步扩展，支持网络参数序列化持久化。
- [ ] **Day 13: 虚拟仿真硬件驱动 (`MockDevice`) 落地**
  - 继承 `IDevice` 实现虚拟硬件驱动，模拟网络握手时延、正弦波传感器遥测生成与下发反馈。
- [ ] **Day 14: 设备工厂模式 (`DeviceFactory`) 与驱动管理器初探**
  - 编写简单工厂类 `DeviceFactory`，实现基于配置动态创建驱动多态指针。
  - 上层业务代码彻底摆脱对具体协议类的直接依赖。
- [ ] **Day 15: 内存巡检、代码整洁度审计与 v0.2-Beta 封版交付**
  - 全工程内存穿透式审计（连接池释放、工厂对象所有权、0 Warning 编译基线）。
  - 正式发布里程碑 Tag：`v0.2-Beta`！

---

## 🚀 Day 01 进展：SQLite 底层引擎封装与生命周期防御 (DatabaseManager)

### 1. 技术核心：Meyers 单例、WAL 预写日志与绝对路径物理锚定
作为 V2 阶段构筑企业级持久化大厦的第一块基石，Day 01 落地了严密、高并发、抗断电损坏的数据库底层基座：
- **物理连接与逻辑句柄彻底解耦**：
  - 严格遵守 Qt 官方规约，**在单例类中绝不将 `QSqlDatabase` 保存为成员变量**。单例类仅保存固定的具名连接标识符字符串（`m_connectionName`）。
  - 所有的数据库操作均通过 `QSqlDatabase::database(m_connectionName)` 按需索取局部有效句柄，用完即随局部栈作用域自然销毁，从根源上消灭了程序退出时的连接悬挂警告。
- **独立具名连接隔离 (Named Connection)**：
  - 摒弃无参调用 `addDatabase("QSQLITE")` 覆写全局默认连接的危险做法，分配专属具名标识 `"IndustrialMonitor_Main_Connection"`，为后续多线程隔离采集预留绝对安全的命名空间。
- **物理路径绝对安全锚定**：
  - 严格采用 `QCoreApplication::applicationDirPath() + "/industrial_monitor.db"` 进行绝对路径绑定。无论程序是从 IDE 启动、快捷方式拉起还是开机自启，`.db` 实体文件永远稳稳当当落在运行根目录 `bin/` 下。
- **工业级 PRAGMA 性能与安全三剑客调优**：
  - **`PRAGMA journal_mode = WAL;`**：开启预写日志模式，实现并发读写分离，读写互不阻塞，抗车间异常断电损坏能力倍增。
  - **`PRAGMA synchronous = NORMAL;`**：在 WAL 模式下兼顾高写入吞吐与断电安全性。
  - **`PRAGMA foreign_keys = ON;`**：显式激活外键硬约束，为后续台账数据完整性建立第一道底线。

---

### 2. 开发复盘：Day 01 攻克的连接池泄漏陷阱与 WAL 检查点机制

#### **陷阱 A: 严禁将 QSqlDatabase 存为类成员——彻底杜绝连接池析构泄漏警告**
- **现象回顾**：在早期架构讨论中，容易产生“在单例里直接存一个 `QSqlDatabase m_db;` 随用随取”的偷懒想法。
- **底层隐患**：Qt 底层维护了一个全局连接池字典。当程序退出时，若单例的全局静态存储期对象析构发生在 Qt 连接池清理之后，Qt 就会抛出经典告警：`QSqlDatabasePrivate::removeDatabase: connection 'xxx' is still in use...`。
- **治理方案**：单例只存连接名字字符串！并在 `closeDatabase()` 中使用**局部花括号作用域**强行将局部的 `QSqlDatabase` 实例先行析构并清空计数，最后再调用 `removeDatabase()`，实现控制台 100% 干净退出。

#### **陷阱 B: 退出后为什么看不见 `-wal` 文件？——SQLite Checkpoint（检查点回写）底层机制释疑**
- **疑惑排查**：程序运行后，在 `bin/` 目录下看到了 `industrial_monitor.db`，但没有发现预期的 `industrial_monitor.db-wal` 辅助文件。
- **底层原理解密**：
  1. `-wal` 预写日志文件只有在程序**处于运行状态且发生了真实的数据写入事务（INSERT/UPDATE）时**才会由 SQLite 底层动态弹出并保留。
  2. 当程序退出调用 `closeDatabase()` 并执行 `db.close()` 时，SQLite 会自动触发 **Checkpoint（检查点合并）** 操作：将 WAL 日志中的增量数据全量刷回主 `.db` 文件，然后**自动安全删除 `-wal` 和 `-shm` 临时文件**！
  3. **结论**：退出后目录下只剩一个干干净净的 `.db` 文件，恰恰证明了数据库是在最健康、无破损的状态下完成了正常回写闭环。控制台打印 `[DB] Current journal mode confirmed as: wal` 即是最高级别生效凭证。

#### **陷阱 C: 编译器跳过 qmake 与文件名拼写纠偏**
- **细节排查**：在 `.pro` 中新追加 `QT += sql` 与新文件后，若直接点击“构建”，编译器可能提示“配置没有改变，跳过 qmake”，导致新模块未被链接。必须手动执行一次“执行 qmake”强制重塑 Makefile；同时纠正了路径字符串中的单词手误（`industrail` $\rightarrow$ `industrial`）。

---

### 3. 如何验证
1. **SQL 模块编译与单例打开测试**：
   - 启动程序进入登录界面，控制台输出标准初始化日志：
     ```text
     [DB] Initializing SQLite database at: ".../bin/industrial_monitor.db"
     [DB] Current journal mode confirmed as: wal
     [DB] SQLite database connected successfully with WAL mode enabled.
     ```
2. **物理文件与落盘验证**：
   - 查看项目 `bin/` 目录，确认自动生成了物理数据库文件 `industrial_monitor.db`。
3. **安全关闭与进程退出零警告测试**：
   - 登录系统后关闭主窗口（或在登录框直接点击取消退出）。
   - **断言**：控制台精准打印关闭与移除连接日志，退出码为 `0`，**绝无任何 `removeDatabase ... is still in use` 警告日志**！

---

### 4. 运行快照（控制台初始化与生命周期日志）

```text
[DB] Initializing SQLite database at: "D:/Cpp/Qt/IndustrialMonitor/IndustrialMonitor/bin/industrial_monitor.db"
[DB] Current journal mode confirmed as: wal
[DB] SQLite database connected successfully with WAL mode enabled.
[ThemeManager] Theme loaded successfully from: ":/qss/dark_style.qss"
[MainWindow] Initialization complete. Geometry: 1280x800.
... [Normal Operation] ...
[DB] Database connection closed.
[DB] Database connection removed from pool cleanly.
```

---

## 💻 编译与运行
- **开发套件**：Qt 6.8+ (MinGW 64-bit)
- **编译依赖**：`QT += core gui widgets sql`
- **构建步骤**：
  1. 使用 Qt Creator 打开根目录下的 `IndustrialMonitor.pro`。
  2. 点击左侧工具栏 **“构建” -> “执行 qmake”**（首次引入 sql 模块必须执行）。
  3. 点击左下角 **运行 (Ctrl + R)** 即可启动监控看板 V2 原型。