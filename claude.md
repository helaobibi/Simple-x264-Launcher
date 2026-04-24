# Simple x264/x265 Launcher — 项目总结

## 概述

Simple x264/x265 Launcher 是一个基于 Qt 的轻量级 GUI 前端，用于驱动 x264 (H.264/AVC) 和 x265 (H.265/HEVC) 编码器。原始项目由 LoRd_MuldeR 开发，仅支持 Windows；当前仓库为 **macOS 移植版**，使用 Qt6 + CMake 构建，通过自行实现的 `mutils_compat` 兼容层替代了 Windows 专有的 MUtils 库。

- **版本**: 3.00.5 (Build 1226)
- **许可证**: GPLv2+
- **语言**: C++17
- **框架**: Qt6 (Core, Gui, Widgets, Network)
- **构建系统**: CMake ≥ 3.16，输出 macOS `.app` bundle

---

## 目录结构

```
├── CMakeLists.txt          # 构建配置，定义源文件列表和 macOS bundle 属性
├── src/                    # 72 个 C++ 源文件 / 头文件
│   ├── main.cpp            # 入口点，初始化 Qt、IPC、样式、主窗口
│   ├── global.{h,cpp}      # 全局函数：版本号、portable 模式、数据路径、防休眠
│   ├── version.h           # 版本号宏 (3.0.5 / Build 1226)
│   ├── mutils_compat.{h,cpp}  # ★ macOS 兼容层，替代 Windows MUtils
│   ├── cli.h / ipc.h       # CLI 参数常量 / IPC 操作码常量
│   ├── win_*.{h,cpp}       # 窗口/对话框 (Main, AddJob, About, Help, Editor, Preferences)
│   ├── model_*.{h,cpp}     # 数据模型 (Options, JobList, LogFile, ClipInfo, Preferences, RecentlyUsed, SysInfo, Status)
│   ├── encoder_*.{h,cpp}   # 编码器抽象与具体实现 (x264, x265) + 工厂
│   ├── source_*.{h,cpp}    # 输入源抽象与具体实现 (VapourSynth) + 工厂
│   ├── tool_abstract.{h,cpp}  # 外部工具基类（进程管理、版本检测、超时控制）
│   ├── thread_*.{h,cpp}    # 线程类 (Abstract, Encode, Startup, Binaries, VapourSynth, IPC)
│   ├── job_object.{h,cpp}  # 进程组管理（macOS 用 pid 跟踪）
│   ├── mediainfo.{h,cpp}   # 文件类型嗅探 (YUV4MPEG2 / VapourSynth)
│   ├── input_filter.{h,cpp}   # 键盘/鼠标事件过滤器
│   └── string_validator.{h,cpp} # 自定义编码器参数校验
├── gui/                    # 6 个 Qt Designer .ui 文件
├── res/                    # 资源文件 (icons, sounds, resources.qrc)
├── etc/                    # CSS 样式、manifest、安装脚本等
├── asm/                    # 汇编相关（目前为空或最小化）
└── build/                  # 构建输出目录
```

---

## 核心架构

### 1. 启动流程 (`main.cpp`)

```
main() → simple_x264_main()
  ├─ 打印版本 logo
  ├─ 解析 CLI 参数 (MUtils::OS::arguments)
  ├─ 检测 CPU 特性 (MUtils::CPUFeatures::detect)
  ├─ 创建 QApplication (MUtils::Startup::create_qt)
  ├─ 创建 IPC 通道 (单实例控制)
  ├─ 设置 Qt 样式 (Fusion / 可选暗色模式 QDarkStyle)
  ├─ 创建 MainWindow 并 show()
  └─ qApp->exec()
```

### 2. 类继承体系

```
QThread
 └─ AbstractThread           # 安全线程基类，异常捕获
     ├─ StartupThread         # 启动时检测线程
     │   ├─ BinariesCheckThread   # 校验编码器二进制文件
     │   └─ VapourSynthCheckThread # 检测 VapourSynth
     └─ EncodeThread          # 编码作业线程 (核心)

QObject
 └─ AbstractTool             # 外部工具基类 (进程启动/超时/解析)
     ├─ AbstractEncoder       # 编码器基类 (版本检测 + 编码执行)
     │   ├─ X264Encoder
     │   └─ X265Encoder
     └─ AbstractSource        # 输入源基类 (属性检测 + 管道创建)
         └─ VapoursynthSource

QAbstractItemModel
 ├─ JobListModel             # 作业队列模型 (用于主窗口 QTreeView)
 └─ LogFileModel             # 日志模型 (用于日志视图)

QMainWindow
 └─ MainWindow               # 主窗口 (win_main)

QDialog
 ├─ AddJobDialog             # 添加作业对话框 (win_addJob)
 ├─ PreferencesDialog        # 偏好设置 (win_preferences)
 ├─ HelpDialog               # 帮助/编码器参数查看 (win_help)
 └─ AboutDialog              # 关于 (win_about)
```

### 3. 工厂模式

- **`EncoderFactory`** — 根据 `OptionsModel::EncType` (X264/X265) 创建具体编码器对象
- **`SourceFactory`** — 根据 `SourceType` (目前仅 VapourSynth) 创建输入源对象

### 4. 编码流程 (`EncodeThread::threadMain`)

```
1. 打印作业信息 (源文件、输出文件、编码器设置)
2. 检查编码器版本 → encoder->checkVersion() + isVersionSupported()
3. 检查输入源版本 (若有管道源) → pipedSource->checkVersion()
4. 检测源属性 → pipedSource->checkSourceProperties(clipInfo)
5. 执行编码:
   - 多 Pass 模式: runEncodingPass(pass=1) → runEncodingPass(pass=2)
   - 单 Pass 模式: runEncodingPass()
6. 完成/失败/中止状态更新
```

编码期间通过 `ExecutionStateHandler` (RAII) 调用 IOKit 防止 macOS 休眠。

### 5. MUtils 兼容层 (`mutils_compat.h/.cpp`)

这是 macOS 移植的关键文件，替代了原始 Windows MUtils 库：

| 命名空间/类 | 功能 |
|---|---|
| `MUtils::OS` | 参数解析、已知文件夹路径、进程挂起/优先级、系统关机、可执行文件检测 |
| `MUtils::CPUFeatures` | CPU 信息检测 (通过 sysctl) |
| `MUtils::IPCChannel` | 进程间通信 (单实例 + 命令传递) |
| `MUtils::Taskbar7` | 任务栏进度条 (macOS 上为空实现 stub) |
| `MUtils::GUI` | 窗口缩放、前置、闪烁、关闭按钮控制 |
| `MUtils::Sound` | 系统声音 (NSSound/afplay) |
| `MUtils::Version` | 编译日期/时间、编译器信息 |
| `MUtils::Registry` | 注册表操作 (macOS 上为 stub) |
| `MUtils::Startup` | Qt 应用创建 |
| `MUtils::init_process` | QProcess 初始化 |

宏替代: `MUTILS_DELETE`, `MUTILS_UTF8`, `MUTILS_THROW`, `MUTILS_BOOL2STR`, `MUTILS_DEBUG`

### 6. 数据模型

| 模型 | 作用 |
|---|---|
| `OptionsModel` | 编码选项 (编码器类型/架构/变体/RC模式/码率/量化/preset/tune/profile/自定义参数)，支持模板保存/加载 |
| `SysinfoModel` | 系统信息 (CPU 特性、VapourSynth 可用性、二进制路径)，线程安全 (QMutex + 宏生成 getter/setter) |
| `PreferencesModel` | 用户偏好 (自动运行/最大并发/优先级/声音/日志保存等)，线程安全 |
| `RecentlyUsed` | 最近使用的目录和过滤器 |
| `ClipInfo` | 视频片段属性 (帧数/分辨率/帧率) |
| `JobListModel` | 作业队列 (QAbstractItemModel)，持有 EncodeThread/状态/进度/日志 |
| `LogFileModel` | 单个作业的日志 (QAbstractItemModel) |
| `JobStatus` | 枚举: Enqueued/Starting/Indexing/Running/Pass1/Pass2/Completed/Failed/Pausing/Paused/Resuming/Aborting/Aborted |

### 7. UI 窗口

| 窗口 | 文件 | 功能 |
|---|---|---|
| **MainWindow** | `win_main.cpp` (54K) | 作业列表管理、启动/暂停/中止/删除、拖放文件、系统托盘、日志查看 |
| **AddJobDialog** | `win_addJob.cpp` (36K) | 选择源/输出文件、编码器配置、模板管理、拖放 |
| **PreferencesDialog** | `win_preferences.cpp` | 全局偏好设置 |
| **HelpDialog** | `win_help.cpp` | 显示编码器 `--help` 输出 |
| **AboutDialog** | `win_about.cpp` | 关于信息 |
| **EditorDialog** | `win_editor.cpp` | 多行文本编辑器 (编辑自定义参数) |

### 8. CLI 支持

通过命令行参数可以控制 Launcher 行为 (`cli.h`):

- `--add-file` / `--add-job` — 通过 CLI 创建作业
- `--force-start` / `--force-enqueue` — 控制作业启动行为
- `--skip-vapoursynth-check` / `--skip-version-checks` — 跳过检测
- `--no-deadlock-detection` — 禁用死锁检测
- `--dark-gui-mode` — 暗色主题
- `--no-style` — 禁用 Fusion 样式

### 9. IPC 机制

- `IPCChannel` 实现单实例控制
- `IPCThread_Recv` / `IPCThread_Send` 处理实例间消息
- 操作码: NOOP / PING / ADD_FILE / ADD_JOB

---

## macOS 移植要点

1. **MUtils 替代**: `mutils_compat.{h,cpp}` 完整替代了 Windows MUtils 库
2. **Avisynth 移除**: `SourceFactory` 仅保留 VapourSynth，移除了 Avisynth 支持
3. **NVEncC 移除**: `EncoderFactory` 仅保留 x264/x265
4. **进程管理**: `JobObject` 使用 `pid_t` 跟踪子进程 (替代 Windows Job Object)
5. **防休眠**: 使用 IOKit `IOPMAssertionCreateWithName` (替代 `SetThreadExecutionState`)
6. **构建**: CMake + Qt6，链接 IOKit 和 CoreFoundation 框架
7. **样式**: 默认使用 Qt Fusion 样式，可选 QDarkStyle 暗色主题

---

## 输出格式支持

- **MKV** (Matroska)
- **MP4** (MPEG-4 Part 14)
- **264** (AVC/H.264 Elementary Stream)
- **HEVC** (H.265 Elementary Stream)

## 编码模式

- **Quantizer** (CRF/QP)
- **Bitrate** (CBR/ABR, kbps)
- **Multi-pass** (2-pass 编码)

---

## 关键设计模式

- **工厂模式**: `EncoderFactory` / `SourceFactory` 解耦具体实现
- **抽象基类**: `AbstractTool` → `AbstractEncoder` / `AbstractSource`，通过虚函数实现多态
- **MVC 模式**: `JobListModel` + `LogFileModel` 作为 Qt Model/View 的 Model 层
- **线程模型**: 每个编码作业一个 `EncodeThread`，通过 Qt 信号槽跨线程通信
- **RAII**: `ExecutionStateHandler` 管理系统休眠状态，`QScopedPointer` 管理资源生命周期
- **宏生成**: `SysinfoModel` 和 `PreferencesModel` 使用宏生成线程安全的 getter/setter
- **模板方法**: `AbstractTool::checkVersion` 定义骨架，子类实现 `checkVersion_init` / `checkVersion_parseLine`
