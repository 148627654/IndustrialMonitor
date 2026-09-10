这里为你量身定制的 `README.md`，完全按照你的参考规范编写。它不仅系统性地规划了整个 **V1 阶段（v0.1-Alpha 里程碑）** 的演进路线，还深度还原了你在 **Day 01** 实际踩坑、排查并攻克的技术细节：

---

# IndustrialMonitor - V1 (工业设备智能监控看板)

一个基于 C++17 与 Qt 6 构建的现代化跨平台工业级设备监控看板系统。V1 阶段致力于确立规范的工控架构范式，摆脱传统初级控件堆砌，打造具备高视觉质感与毫秒级吞吐表现的 SCADA 监控原型。

## 📌 项目愿景
V1 阶段的目标是将系统从“基础窗口演示”打造为“工业级桌面客户端基座”。通过严格的架构解耦、规范的 MVC 模型视图设计和原生自绘技术，支撑起百级工业产线设备的状态实时呈现。
- **工业美学与低视疲劳**：自研深灰暗黑工业主题引擎，支持运行期热重载（Hot-Reload）。
- **极速响应 Model/View 架构**：基于 `QAbstractTableModel` 实现百万级虚拟化渲染吞吐，严禁使用卡顿的 `QTableWidget`。
- **原生像素级自绘**：通过 `QStyledItemDelegate` 与 `QPainter` 硬件加速绘制呼吸发光状态灯与动态性能进度条。
- **高信噪比检索**：基于 `QSortFilterProxyModel` 实现无感、实时的多条件模糊检索与列排序。

---

## 🛠 项目结构 (V1)
```text
IndustrialMonitor/
├── .gitignore
├── IndustrialMonitor.pro         # qmake 主工程配置文件（严格重定向产物路径）
├── bin/                          # 二进制输出目录（exe 产物与运行期目录）
├── build/                        # 构建临时目录（moc/obj/rcc/ui 统一隔离，不污染源码）
├── res/                          # 资源目录
│   ├── icons/                    # 系统状态图元与工业矢量图标
│   ├── qss/
│   │   └── dark_style.qss        # 核心工业暗黑配色样式表
│   └── res.qrc                   # Qt 二进制资源配置文件
└── src/                          # 核心源码目录
    ├── common/                   # 全局工具类、公共定义与主题管理器
    │   ├── ThemeManager.h
    │   └── ThemeManager.cpp
    ├── core/                     # 业务核心逻辑（后续迭代承载通信与数据泵）
    ├── models/                   # 核心 MVC 数据模型（QAbstractTableModel 派生）
    ├── views/                    # 界面视图（MainWindow、LoginDialog、自定义委托）
    │   ├── mainwindow.h
    │   ├── mainwindow.cpp
    │   └── mainwindow.ui
    └── main.cpp                  # 统一应用程序入口
```

---

## 📅 进度跟踪 (V1 15天挑战)

### 第 1 阶段：工程规范、暗黑主题与主窗口骨架
- [x] **Day 01: 准备工作与工业级暗黑主题引擎 (ThemeManager)**
  - 实现编译产物完全隔离（`bin/` 与 `build/` 重定向）。
  - 建立物理分层源码架构（`src/`、`res/`、`views/` 等）。
  - 封装单例 `ThemeManager`，打通 `res.qrc` 资源打包与外部文件读取双机制。
  - 实现 `F5` 快捷键全局事件过滤器，支持 QSS 样式运行期免编译热重载。
- [ ] **Day 02: 工业监控登录窗口 (LoginDialog UI 与交互)**
  - 创建独立工业登录弹窗 `LoginDialog`（继承自 `QDialog`）。
  - 精细化暗黑风格输入组件：账号/密码框聚焦发光、明密文一键切换。
- [ ] **Day 03: 登录业务逻辑与配置持久化 (QSettings)**
  - 集成 `QSettings` 对本地运行凭证实现 `.ini` 级持久化存储。
  - 落地“记住密码”与免密自动填入防呆逻辑，在 `main.cpp` 中形成阻断式生命周期。
- [ ] **Day 04: 主窗口结构与侧边栏布局 (MainWindow Layout)**
  - 构建 `MainWindow` 核心布局：左侧紧凑导航栏 + 右侧 `QStackedWidget` 堆叠核心。
  - 建立工控系统全屏自适应与响应式尺寸边界（Min: 1280x800）。
- [ ] **Day 05: 动态侧边栏导航控制 (Navigation Binding)**
  - 基于互斥 `QButtonGroup` 驱动侧边控制栏。
  - 绑定信号槽，实现“设备监控 / 实时曲线 / 报警日志 / 系统设置”页面毫秒级切换。

### 第 2 阶段：侧边导航交互与专业 Model/View 架构
- [ ] **Day 06: 工业状态栏与全域状态看板 (StatusBar)**
  - 封装底部高信噪比 `QStatusBar`：操作员标识、全局通信链路指示、高精度时钟。
- [ ] **Day 07: 设备数据实体定义与页面搭建**
  - 定义 `DeviceInfo` 结构体与 `DeviceStatus` 状态枚举。
  - 构建设备监控首屏：顶部检索工具栏 + 核心展示容器。
- [ ] **Day 08: 打造专业级 QAbstractTableModel (只读基石)**
  - 继承 `QAbstractTableModel`，重写 `rowCount`、`columnCount`、`headerData`、`data`。
  - 建立只读高效虚拟数据通道，杜绝 `QTableWidget` 带来的内存膨胀。
- [ ] **Day 09: 数据填充与模型-视图 (Model-View) 联调**
  - 封装 `beginResetModel` / `endResetModel` 安全数据注入机制。
  - 接入整行高亮选中、交替行底色、表头自动伸缩等工业交互特性。
- [ ] **Day 10: 搜索过滤与平滑排序 (QSortFilterProxyModel)**
  - 引入代理模型（ProxyModel）实现数据展示层与逻辑层的三层解耦。
  - 连接文本变化信号，毫秒级实现 IP 地址与设备名称的多列正则过滤。

### 第 3 阶段：自定义委托绘制、交互精雕与里程碑发布
- [ ] **Day 11: 自定义委托（绘制工业三色呼吸状态指示灯）**
  - 实现 `StatusDelegate`（继承自 `QStyledItemDelegate`）。
  - 基于 `QPainter` 抗锯齿渲染发光点阵（正常绿、预警黄、故障红、离线灰）。
- [ ] **Day 12: 自定义委托（单元格内嵌微型性能进度条）**
  - 编写 `ProgressBarDelegate` 接管 CPU 与内存指标列。
  - 注入动态区间变色算法（<60% 正常绿，60%~85% 告警黄，>85% 危险绯红）。
- [ ] **Day 13: 滚动条精雕与全域视觉统一**
  - 精细化暗黑高质感滚动条（细条悬停加粗、凹槽无边框融合）。
  - 修正所有子窗体焦点游离与边缘像素瑕疵。
- [ ] **Day 14: 内存巡检与生命周期防御**
  - 全工程内存排查，规范 Qt 父子对象树（Object Tree）所有权归属。
  - 修复全部隐式数据类型转换与潜在内存泄漏点。
- [ ] **Day 15: 里程碑封版与代码重构 (v0.1-Alpha)**
  - 规范 Doxygen 风格注释，打包独立运行所需依赖库。
  - 封版并标记 Git Tag：`v0.1-Alpha`。

---

## 🚀 Day 01 进展：工程规范架构与暗黑主题引擎

### 1. 技术核心：构建隔离与动态热重载体系
为避免工控桌面应用在开发迭代中陷入“目录杂乱”与“样式微调重启耗时”的死循环，Day 01 建立了标准化防线：
- **产物彻底隔离 (Out-of-Source Layout)**：
  - 通过 `.pro` 配置 `DESTDIR = $$PWD/bin`，将生成的目标可执行文件收敛至 `bin/`。
  - 将所有中间文件（`moc_*.cpp`、`*.o`、`ui_*.h`、`qrc_*.cpp`）全部分流至 `build/` 下的独立子目录。
- **全局事件过滤热重载 (Hot-Reload via EventFilter)**：
  - 将 `ThemeManager` 挂载为单例，并为 `qApp` 安装事件过滤器（`installEventFilter`）。
  - 捕获 `Qt::Key_F5` 按键事件，动态重载外部 QSS 样式表，免去了频繁重新编译界面的机械动作。

### 2. 开发复盘：Day 01 攻克的构建与链接血泪史
在今天的架构搭建中，遭遇了三大极具代表性的跨平台与底层编译陷阱，并彻底排查攻克：

#### **陷阱 A: 大小写不敏感引发的 Duplicate Symbols (链接期死锁)**
- **现象**：MinGW 报出大量 `multiple definition of 'ThemeManager::...'` 错误，指向 `ThemeManager.o` 与 `thememanager.o`。
- **根因**：Windows 操作系统不区分大小写，而在 `IndustrialMonitor.pro` 中，`SOURCES` 同时包含了 `ThemeManager.cpp` 和 `thememanager.cpp`。Makefile 针对不同大小写生成了两套目标构建规则，导致同一个物理源文件被链接器编译了两次。
- **解决**：清理 `.pro` 中的冗余条目，强制统一使用 PascalCase（大驼峰）命名，并彻底抹除旧的编译残留。

#### **陷阱 B: Q_OBJECT 元对象虚表缺失 (`undefined reference to vtable`)**
- **现象**：类继承自 `QObject` 并加入了 `Q_OBJECT` 宏，但构建直接中断并报错虚表未定义。
- **根因**：头文件移动了物理位置（从根目录移至 `src/common/`）后，qmake 的依赖图谱未更新，导致元对象编译器（`moc`）跳过了对新路径下 `ThemeManager.h` 的代码生成。
- **解决**：在工程结构发生物理迁移后，必须**手动执行一次 qmake**，强制让 moc 建立最新的依赖映射。

#### **陷阱 C: 运行时工作路径与虚拟资源前缀漂移**
- **现象**：界面正常弹出，但依然是 Windows 亮白默认样式，QSS 样式完全未渲染。
- **根因**：由于配置了 `DESTDIR = $$PWD/bin`，程序的执行目录位于 `bin/`，直接通过相对路径 `"res/qss/..."` 会导致文件读取失败；同时，`res.qrc` 内部的虚拟前缀与实际相对路径存在层级偏差。
- **解决**：将路径规范化为 Qt 内置资源定位器形式（`:/qss/dark_style.qss`），并确保 `res.qrc` 相对于文件自身的映射层级准确无误。

### 3. 如何验证
1. **工程结构干净度验证**：
   - 执行清理并重新编译，确认源码树中无任何中间物，`bin/` 产出独立运行文件，`build/` 收容所有对象文件。
2. **工业暗黑调色盘验收**：
   - 运行程序，主视口成功染上 `#1E1E1E` 工业哑光深灰底色。
   - 输入框呈现 `#2D2D2D` 容器底色并伴有高亮边框；按钮展示 `#0E639C` 经典工控蓝，悬停变色平滑过渡。

**Day 01 运行快照：**

![image](./readme.assets/file-20260910154026183.png)

```text
[ThemeManager] Theme loaded successfully from: ":/qss/dark_style.qss"
[ThemeManager] Global Hot-Reload EventFilter installed (F5 Active).
```

---

## 💻 编译与运行
- **开发套件**：Qt 6.8+ (MinGW 64-bit )
- **构建步骤**：
  1. 使用 Qt Creator 打开根目录下的 `IndustrialMonitor.pro`。
  2. 点击左侧工具栏 **“构建” -> “执行 qmake”**。
  3. 点击左下角 **运行 (Ctrl + R)** 即可启动监控看板原型。