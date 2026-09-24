# AGENTS.md — OpenHarmony 事件处理器（EventHandler/EventRunner/Emitter）

适用范围：本文件是本仓库（事件处理器库）根级 Agent 指导，适用于仓库内所有目录的编码任务。

## 1. 代码地图

本仓库实现 OpenHarmony 线程模型基础库：事件循环（`EventRunner`）、事件处理（`EventHandler`）、消息实体（`InnerEvent`）、线程消息队列（`EventQueue`）、fd 监听（`FileDescriptorListener`）以及构建在其上的跨线程通信 API（`@ohos.events.emitter`）。最重要的架构边界是**本仓库是纯库（无 SA、无服务进程、无 `services/` 目录），C++ 核心在 `frameworks/eventhandler/`（libeventhandler），Emitter/NAPI/NDK/仓颉都是它的绑定层；不涉及任何 IPC**。

### 关键区域

- `interfaces/inner_api/`：对外 C++ API 头文件（11 个）：`event_handler.h`、`event_runner.h`、`event_queue.h`、`inner_event.h`、`file_descriptor_listener.h`、`event_handler_errors.h`、`lock_base.h`、`dumper.h`、`event_logger.h`、`event_inner_logger.h`、`native_implement_eventhandler.h`；所有 C++ 消费者依赖此层，变更影响面最大
- `frameworks/eventhandler/`：C++ 核心实现（target `libeventhandler`），含内部头文件目录 `include/`（`event_queue_base.h`、`event_queue_ffrt.h`、`epoll_io_waiter.h`、`none_io_waiter.h`、`deamon_io_waiter.h`、`event_inner_runner.h` 等）
- `frameworks/napi/`：JS API `@ohos.events.emitter` 的 NAPI 绑定（`emitter` 模块 + `emitter_interops` 互操作库）
- `frameworks/emitter/`：Emitter 跨运行时层：`base/`（AsyncCallbackManager 聚合 NAPI/ANI 回调管理）、`napi/`（增强 API：emitterId 实例隔离、跨运行时反序列化）、`ani/`（ArkTS Native 实现 + `@ohos.events.emitter.ets`）
- `frameworks/native/`：NDK C 接口包装（target `eventhandler_native`，头文件在 `interfaces/kits/native/native_interface_eventhandler.h`）
- `frameworks/cj/`：仓颉 FFI 绑定（target `cj_emitter_ffi`，独立 `EventHandlerImpl` runner）
- `frameworks/test/moduletest/`：模块测试；`frameworks/eventhandler/test/unittest/`：核心单测
- `test/fuzztest/`：11 个 fuzzer；`test/systemtest/`：6 个系统测试
- `eventhandler.gni`：特性开关（`eventhandler_ffrt_usage`、PGO、主线程优先级继承锁等）
- `README_zh.md`：四大核心类（EventRunner/InnerEvent/EventHandler/EventQueue）使用说明与构建指引（README_en.md 为未填写模板，勿参考）
- 版本脚本：`libeventhandler.versionscript`、`libeventEmitter.map`、`libeventhandler_native.map`、`libcj_emitter_ffi.map`（符号导出管控，勿改已有符号可见性）

### Where to look

| 任务类型 | 先看哪里 |
|---|---|
| 公共 C++ API 变更 | `interfaces/inner_api/` → `frameworks/eventhandler/src/`（实现）→ 四个 `*.map` 版本脚本 |
| 事件循环/线程模型 | `frameworks/eventhandler/src/event_runner.cpp`（`EventRunner::Create`/`GetMainEventRunner`、`EventRunnerImpl`、`ThreadCollector`） |
| 事件队列/优先级 | `interfaces/inner_api/event_queue.h`（抽象 + Priority 枚举）→ `frameworks/eventhandler/src/event_queue_base.cpp`（std::thread 模式）/ `event_queue_ffrt.cpp`（FFRT 模式） |
| 消息实体/载荷 | `interfaces/inner_api/inner_event.h` + `frameworks/eventhandler/src/inner_event.cpp`（对象池、smartPtr 载荷、weak owner） |
| fd 监听（epoll） | `frameworks/eventhandler/src/epoll_io_waiter.cpp`（epoll + eventfd 唤醒）+ `event_queue.cpp` 中 `AddFileDescriptorListenerBase` |
| JS Emitter 变更 | `frameworks/napi/src/events_emitter.cpp` + `frameworks/emitter/napi/src/napi_emitter.cpp` + `frameworks/emitter/base/`（三处需联动理解） |
| NDK C API | `interfaces/kits/native/native_interface_eventhandler.h` → `frameworks/native/src/native_interface_eventhandler.cpp` |
| 仓颉绑定 | `frameworks/cj/src/` |
| 锁体系 | `interfaces/inner_api/lock_base.h` → `frameworks/eventhandler/include/std_lock.h` + `priority_inheritance_lock.h` |
| 日志/诊断 | `interfaces/inner_api/event_logger.h`（`DEFINE_EH_HILOG_LABEL` + `HILOGX` 宏 + 限流宏）+ `dumper.h` |
| 特性开关 | `eventhandler.gni` |
| 新增/修改测试 | `frameworks/eventhandler/test/unittest/BUILD.gn`（单测）/ `frameworks/test/moduletest/BUILD.gn`（模块测试）/ `test/fuzztest/` |

### 架构分层

```
应用层
  ├─ ArkTS 应用 → frameworks/napi + frameworks/emitter（@ohos.events.emitter）
  │                ├─ frameworks/emitter/ani（ArkTS Native/ANI + boot abc）
  │                └─ frameworks/cj（仓颉 FFI，独立 std::thread runner）
  ├─ C++ 应用/系统组件 → interfaces/inner_api（EventHandler/EventRunner/EventQueue/InnerEvent）
  └─ 原生 C 应用 → interfaces/kits/native + frameworks/native（NDK fd 监听循环）
           ↓（同进程内调用，无 IPC）
C++ 核心库（frameworks/eventhandler → libeventhandler）
  EventHandler（持有 EventRunner，投递 SendEvent/PostTask，子类重写 ProcessEvent）
    → EventRunner（final，私有构造，只能 Create/GetMainEventRunner 创建；主 runner 进程唯一）
      ├─ std::thread 模式 → EventQueueBase（4 个优先级子队列 + idle 队列，anti-starvation）
      │     └─ 锁：LockBase 抽象（StdLock 或 PriorityInheritanceLock）
      ├─ FFRT 模式 → EventQueueFFRT（包装 ffrt::queue，锁为 ffrt::mutex）
      ├─ IO 等待 → NoneIoWaiter（默认，condvar）→ 首次 AddFileDescriptorListener
      │            动态升级为 EpollIoWaiter（epoll + eventfd 唤醒）
      ├─ 进程级守护 → DeamonIoWaiter（独立 epoll 线程：FFRT 队列 / vsync fd / 特定系统参数场景）
      └─ 线程回收 → ThreadCollector（DelayedRefSingleton + Avatar 静态析构）

Emitter 链路（构建在核心库之上）
  JS/ArkTS emit（任意线程）→ EventHandlerInstance（全局单例，FFRT runner "OS_eventsEmtr"）
    → InnerEvent 携带序列化数据 → ProcessEvent → AsyncCallbackManager
      → napi_threadsafe_function 投回注册线程执行回调
```

## 2. 知识路由

在规划或编辑前，先对任务分类，读取对应的代码路径和文档。

### Task-based routing

| 任务类型 | 读取 |
|---|---|
| 公共 C++ API 新增/修改 | `interfaces/inner_api/` 头文件 + `frameworks/eventhandler/src/` 实现 + `libeventhandler.versionscript` |
| 线程/运行器行为变更 | `frameworks/eventhandler/src/event_runner.cpp` + `event_inner_runner.h`（注意 `deposit_` 语义与 `ThreadCollector` 析构顺序注释） |
| 队列/优先级变更 | `event_queue.h`（Priority 枚举 VIP/IMMEDIATE/HIGH/LOW/IDLE）+ `event_queue_base.cpp`（`PickEventLocked`、idle 语义、AT_FRONT 钳制）+ `event_queue_ffrt.cpp` |
| fd 监听变更 | `event_queue.cpp` 的 `AddFileDescriptorListenerBase`/`EnsureIoWaiterLocked` + `epoll_io_waiter.cpp` + `deamon_io_waiter.cpp` |
| InnerEvent 载荷变更 | `inner_event.h`（smartPtrTypeId 类型校验算法）+ `inner_event.cpp`（对象池、`ClearEvent` 析构回调） |
| JS Emitter 变更 | `frameworks/napi/src/events_emitter.cpp` + `frameworks/emitter/napi/src/napi_emitter.cpp` + `frameworks/emitter/base/`（NAPI/ANI 互操作） |
| NDK 变更 | `interfaces/kits/native/native_interface_eventhandler.h` + `frameworks/native/src/` + `libeventhandler_native.map` |
| 锁/优先级继承变更 | `lock_base.h` + `std_lock.h` + `priority_inheritance_lock.h` + `eventhandler.gni`（主 runner PI 锁开关） |
| 错误码变更 | `interfaces/inner_api/event_handler_errors.h`（模块 id 0x10） |
| 新增特性 | `eventhandler.gni` 添加开关 → 条件编译包裹代码 |
| 新增/修改测试 | 对应 test 目录 + 其 `BUILD.gn`（单测/模块测试 target 定义） |

### Path-based routing

| 修改路径 | 需了解的上下文 |
|---|---|
| `interfaces/inner_api/` | 全部下游消费者（含 system 能力与四套绑定）的 API 头文件，符号导出受版本脚本管控 |
| `frameworks/eventhandler/src/event_runner.cpp` | 核心运行器：主 runner 单例、`deposit_` 语义、ThreadCollector 析构顺序、FFRT 分支；近期活跃修改区 |
| `frameworks/eventhandler/src/event_queue_base.cpp` | 队列核心：anti-starvation（连续 5 个让位）、idle 事件不唤醒线程、AT_FRONT 时间钳制 |
| `frameworks/eventhandler/src/event_queue_ffrt.cpp` | FFRT 队列：任务名编码（供 `ffrt_queue_cancel_by_name`）、析构必须异步化避免死锁、`MakeCopyableFunction` |
| `frameworks/emitter/` | 三方（base/napi/ani）联动，`AsyncCallbackManager` 聚合两套运行时回调；头文件 `aync_callback_manager.h` 拼写是既成事实，勿"顺手修正" |
| `frameworks/napi/BUILD.gn` | ArkUI-X 跨平台分支（`is_arkui_x` + `CROSS_PLATFORM` 宏） |
| `frameworks/eventhandler/inner_api_sources.gni` | 核心库源文件清单（15 个 .cpp，FFRT 开启追加 `event_queue_ffrt.cpp`），新增源文件在此登记 |
| `test/fuzztest/` | fuzzer 统一入口 `DoSomethingInterestingWithMyAPI(FuzzedDataProvider*)`，用 `#define private public` 破封装 |

### Vocabulary-based routing

当任务、issue、日志、API 名称中出现以下术语时，先理解其含义和风险再动手：

| 术语 | 含义与风险 | 读取 |
|---|---|---|
| EventRunner | 事件循环分发器，final 类私有构造，只能 `Create()` 或 `GetMainEventRunner()`；主 runner 进程唯一（静态单例） | `interfaces/inner_api/event_runner.h` |
| deposit runner | `Create(true)` 自动起线程的 runner，用户调 `Run()/Stop()` 返回 `EVENT_HANDLER_ERR_RUNNER_NO_PERMIT` | `frameworks/eventhandler/src/event_runner.cpp` |
| 优先级（Priority） | VIP/IMMEDIATE/HIGH/LOW/IDLE 五级（不是 idle/main/default）；IO 回调默认 HIGH，vsync 监听强制 VIP | `interfaces/inner_api/event_queue.h` |
| Idle 任务 | 不唤醒线程，仅当队列整体空闲且到达 handleTime 才分发 | `event_queue_base.cpp` |
| smartPtr 载荷 | InnerEvent 用 `smartPtrTypeId_` 做取出类型校验，取错类型仅告警返回 nullptr；`GetUniqueObject` 是 move 语义只能取一次 | `interfaces/inner_api/inner_event.h` |
| weak owner | InnerEvent/FileDescriptorListener 持有 `weak_ptr<EventHandler>` 防循环引用；EventHandler 析构时 `RemoveOrphan()` 清孤儿事件 | `event_handler.cpp` |
| EpollIoWaiter / DeamonIoWaiter | 前者是队列级 IO 等待器（epoll+eventfd）；后者是进程级守护 epoll 线程（FFRT/vsync 场景）。`Deamon` 拼写为仓库既成事实，勿改 | `frameworks/eventhandler/src/epoll_io_waiter.cpp` / `deamon_io_waiter.cpp` |
| FFRT 队列 | `EventQueueFFRT` 包装 `ffrt::queue`，锁为 `ffrt::mutex`；`HasPreferEvent`/`QueryPendingTaskInfo` 在此模式不支持 | `event_queue_ffrt.cpp` |
| Emitter | `@ohos.events.emitter` API，构建在 EventHandler 之上（全局单例 FFRT runner），非独立实现 | `frameworks/napi/` + `frameworks/emitter/` |
| vsync barrier | 主线程专属性能机制（`SetBarrierMode`/`MarkBarrierTaskIfNeed`），只对主 runner 有意义 | `event_queue.h` + `event_queue_base.cpp` |

### 编辑前必做声明

开始编辑任何代码前，先在回复中声明以下四项，缺一不可：

1. 任务分类（对应上表哪一行）
2. 已读取的代码路径和文档
3. 发现的约束（本文件第 3 节中适用的条目）
4. 是否需要同步修改其他层（如 inner_api 变更需检查 NAPI/NDK/仓颉/Emitter 绑定与版本脚本）

## 3. 约束边界

### 架构不变量

- 纯库仓库：无 SA、无服务进程、无 IPC；不要引入跨进程通信或后台服务设计
- `EventRunner` 保持 final + 私有构造 + 静态工厂；主 runner 进程唯一，不能被复制或二次创建
- 事件 owner 一律 `weak_ptr<EventHandler>`（防循环引用）；EventHandler 继承 `enable_shared_from_this`
- `InnerEvent` 只能经 `InnerEvent::Get()` 工厂从对象池分配，`Pointer`（unique_ptr + 自定义删除器）跨 API 传递
- std::thread 模式队列锁走 `LockBase` 抽象（StdLock/PriorityInheritanceLock 可切换）；FFRT 队列锁固定 `ffrt::mutex`，不要混用锁体系
- 对外头文件符号导出受 `*.map`/`*.versionscript` 管控，新增导出符号必须同步版本脚本
- 日志统一走 `event_logger.h` 宏（`DEFINE_EH_HILOG_LABEL` + `HILOGX`/`EH_LOGX_LIMIT`），不使用裸 hilog

### 禁止事项

- 不要修改公共 API 签名、错误码语义或生命周期语义，除非任务明确要求
- 不要为通过测试而删除日志、限流宏或诊断信息
- 不要修改 `*.map`/`*.versionscript` 中已有符号的可见性
- 不要"顺手修正"既成命名（`DeamonIoWaiter`、`aync_callback_manager.h`），会破坏 git 历史可追溯性
- 不要在 FFRT 队列析构路径引入同步等待（ffrt 任务内析构必须异步化，否则死锁）
- 不要给 `SendSyncEvent`/`PostSyncTask` 增加 IDLE 优先级路径（明确禁止）
- 不要在 lambda 中按值捕获 `shared_ptr<EventHandler>`/listener（用 weak_ptr guard 模式）
- 不要引入新的生产依赖而不经过确认

### 需确认后再修改

- 公共 API 签名变更（需确认兼容性影响和版本策略）
- 优先级模型或 anti-starvation 策略变更（需确认主线程时序敏感场景）
- 锁体系变更（StdLock ↔ PriorityInheritanceLock 切换影响调度行为）
- FFRT 默认开关（`eventhandler_ffrt_usage`）变更（需确认全系统回归）
- 主线程 runner 行为（vsync barrier、`HasPendingHigherEvent` 仅主线程可用）
- 新增外部依赖（需确认许可证和包大小影响）

### 已知陷阱与常见失败模式

- **smartPtr 载荷类型安全是弱校验**：`GetSharedObject<T>`/`GetUniqueObject<T>` 取错类型仅打日志返回 nullptr，编译期发现不了；`GetUniqueObject` 是 move 语义只能取一次
- **SendEvent 失败返回 false 时事件需手动释放**（对象池回收语义，见 `event_handler.h` 注释）
- **deposit runner 调 `Run()/Stop()` 返回错误**：`EVENT_HANDLER_ERR_RUNNER_NO_PERMIT`；`Run()` 重入返回 `EVENT_HANDLER_ERR_RUNNER_ALREADY`——单测常见失败点
- **idle 事件不唤醒线程**：调试"为什么 PostIdleTask 不执行"先看线程是否整体空闲
- **AT_FRONT 插入会被时间钳制**：队头事件 handleTime 更早时钳到队头时间（近期活跃修复区）
- **starvation 保护**：每个子队列连续处理 5 个（`DEFAULT_MAX_HANDLED_EVENT_COUNT`）后强制让低优先级有机会，改优先级逻辑勿破坏
- **SendTimingEvent 负延迟静默改 0**：taskTime 早于当前时间时不报错
- **ThreadCollector 的 Avatar 静态对象必须在 `currentEventRunner` 之后定义**（析构顺序，`event_runner.cpp` 有注释）
- **fd 生命周期**：fd 经 `fdsan_exchange_owner_tag/fdsan_close_with_tag` 带 `EH_LOG_DOMAIN` tag 管理；移除监听需同步清理 DeamonIoWaiter/EpollIoWaiter
- **FFRT 任务名编码** `handlerId|hasTask|innerEventId|param|taskName` 供 `ffrt_queue_cancel_by_name` 正则取消，改字段需全链路一致
- **命名空间是 `OHOS::AppExecFwk`**（含尾部别名 `namespace EventHandling = AppExecFwk`；CJ 侧是 `OHOS::EventsEmitter`），不是 EventFwk
- **NDK C API 无 `OH_` 前缀**：实际导出 `GetEventRunnerNativeObjForThread`/`CreateEventRunnerNativeObj`/`EventRunnerRun` 等
- **测试直接编源**：单测/模块测试把 `inner_api_sources` 编进测试二进制（非链 so），并用 `-Dprivate=public -Dprotected=public` 破封装
- 单测风格 `HWTEST_F(Suite, Case001, TestSize.Level1)`；fuzzer 入口统一 `DoSomethingInterestingWithMyAPI`

## 4. 验证闭环

### 最小验证

```bash
# 构建整个 eventhandler 组件（从 OpenHarmony 根目录执行）
./build.sh --product-name rk3568 --build-target eventhandler

# 构建核心单元测试（group 在 frameworks/eventhandler/test/unittest）
./build.sh --product-name rk3568 --build-target LibEventHandlerTest

# 构建模糊测试（任选一个实际 fuzzer target）
./build.sh --product-name rk3568 --build-target EventHandlerFuzzTest
```

代码风格：命名空间 `OHOS::AppExecFwk`、成员变量尾下划线、`snake_case` 文件名、`DISALLOW_COPY_AND_MOVE`、安全函数 `memset_s/snprintf_s`（securec）；遵循 OpenHarmony 根目录 `.clang-format`（CI 门禁检查），本仓无独立 lint 脚本。

### 分任务验证

| 变更类型 | 必须验证 |
|---|---|
| 公共 C++ API 变更 | 最小验证 + `libeventhandler.versionscript` 新符号导出正确 + NDK/Emitter/仓颉绑定编译通过 |
| 队列/优先级逻辑 | 最小验证 + `LibEventHandlerEventQueueTest` + `LibEventHandlerEventTest` + starvation/idle 用例不回归 |
| 运行器/线程 | 最小验证 + `LibEventHandlerEventRunnerTest`（Create/Run/Stop/deposit 全用例） |
| fd 监听 | 最小验证 + `EpollIoWaiterFuzzTest` 编译 + `FileDescriptorListenerFuzzTest` |
| InnerEvent 载荷 | 最小验证 + `LibEventHandlerInnerEventTest` |
| FFRT 模式 | 最小验证 + FFRT 用例（`Ffrt001~009` 系列）+ 至少一种开关组合（`eventhandler_ffrt_usage` 开/关）下编译通过 |
| Emitter/NAPI | 最小验证 + `EventHandlerSendEventModuleTest` 等模块测试 + 三方（base/napi/ani）一致性 |
| 新增测试 | 对应 `BUILD.gn` target 编译通过（`module_out_path` 遵循 `eventhandler/eventhandler/<层级路径>` 约定） |

### Done 定义

- 构建通过（组件 + 相关单元测试 + 模糊测试）
- 无新增编译警告（核心库开启 CFI/UBSan/boundary sanitize，警告即隐患）
- 变更范围与任务要求一致（无顺手修改无关文件）

### 最终回复要求

任务完成回复必须包含：

1. 变更文件清单（新增/修改/删除）
2. 执行过的验证命令及结果（通过/失败/跳过）
3. 未执行的验证项及原因

### 无法验证时

如果构建环境不可用，不要声称已完成验证。在最终回复中列出应执行的命令、预期结果，并明确标注"未验证"。
