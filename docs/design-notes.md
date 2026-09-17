# 设计与调试记录

[返回项目 README](../README.md)

本文记录实现过程中对稳定性和可移植性影响较大的问题。它们也是当前实现中若干设计选择的依据。

## FreeRTOS+TCP 随机数回调

FreeRTOS+TCP 在创建 TCP 连接序列号时调用应用提供的 `xApplicationGetRandomNumber()`。早期实现读取
STM32 UID 作为种子，但当前 STM32H563 执行和安全配置下，读取 UID 会触发 HardFault。典型调用链为：

```text
prvHandleListen_IPV4
  -> ulApplicationGetNextSequenceNumber
  -> xApplicationGetRandomNumber
  -> HAL_GetUIDw0
  -> HardFault_Handler
```

当前实现使用持久的 xorshift 状态，并混入 `HAL_GetTick()`，避免访问 UID。该实现只满足 TCP 初始序列号
所需的轻量随机性，不应作为密码学随机数使用。如果后续启用 STM32 RNG，应在完成时钟、安全属性和实机
验证后再替换。

## 空闲连接与 socket 返回值

`FreeRTOS_recv()` 的 `FREERTOS_EWOULDBLOCK` 表示本次等待超时，并不表示客户端已经断开。服务任务在收到
该返回值后继续等待，使 Python 交互会话可以长时间保持连接。

返回 `0` 或 `FREERTOS_ECLOSED` 才按正常关闭处理，不打印错误；其他负值会记录错误并结束当前客户端会话。
监听 socket 保持运行，随后仍可接受新的客户端。

## 跨工具链的 64 位格式化

IAR 与 STM32CubeIDE 使用的 C 库对 64 位 `printf` 格式支持存在差异。Target 列表最初直接用 `%llx` 输出
64 位 payload，在 STM32CubeIDE 构建中曾产生错误文本，导致 Python 无法解析。

当前实现把 payload 拆成两个 `uint32_t`，使用固定宽度的 `%08lX%08lX` 拼接；GETPID 等 CCC 响应则按
字节输出，由 Python 客户端统一转换成整数。这样避免协议格式依赖具体工具链的 64 位格式化实现。

## I3C open-drain 与 push-pull 时序

I3C 地址、ACK、T-bit 和仲裁阶段会使用 open-drain 时序，数据阶段可以使用 push-pull 时序。当前配置下，
push-pull 数据阶段为 12.5 MHz，而 open-drain 阶段约为 1.85 MHz，因此 ENTDAA 开始处的 `0x7E` 广播地址
看起来明显更慢。这是预期行为，不是 12.5 MHz 配置失效。

计算过程及实测波形见 [I3C 波形分析](i3c-waveform-analysis.md)。

## 应用层与中间件边界

- 任务创建和应用同步优先使用 CMSIS-RTOS2，减少应用层对特定 RTOS 内核接口的依赖。
- FreeRTOS+TCP socket、网络事件和应用 hook 按中间件接口直接使用 FreeRTOS API。
- Controller 总线访问由一个互斥入口串行化；Target 表只保存一个 Controller 所需的设备状态。
- TCP 层只负责逐行收发，命令解析和 I3C 操作分别位于独立模块中。
