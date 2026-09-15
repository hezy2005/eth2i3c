# eth2i3c（FreeRTOS+TCP）

这是 STM32H563 上的 Ethernet 转 I2C/I3C 测试网关。PC 通过 TCP 发送单行命令，Controller 使用 I3C1
完成总线操作，再返回文本响应。

## 功能范围

- 只支持一个 STM32H563 Controller，不保留多 Controller 管理和 `controller_set_active`。
- TCP 端口为 `1000`，同一时间服务一个客户端。
- 静态 IPv4 地址为 `192.168.1.212`，掩码 `255.255.255.0`，网关 `192.168.1.1`。
- 保留 I3C 正常使用所需的 RSTDAA、ENTDAA、GETPID、GETBCR、GETDCR、GETMWL、GETMRL、
  GETSTATUS、SETMWL、SETMRL、ENEC 和 DISEC。
- 支持按照静态地址（SA）和动态地址（DA）创建、绑定、选择 Target，并指定 I2C/I3C 协议。
- 支持寄存器 random write、random read 和 current read。
- 不包含 fan、power、GPIO、monitor 等产品命令。

配套的 `eth2i3c_target` 也运行在 STM32H563 上，直接采用已测试通过的 Target 状态机。寄存器模型是
两个连续的 128 字节 bank：

```c
uint8_t lower_memory[TARGET_MEMORY_BANK_SIZE] = {0};
uint8_t high_memory[TARGET_MEMORY_BANK_SIZE] = {0};
```

## 软件分层

- 应用层使用 CMSIS-RTOS2 API，例如 `osThreadNew`、`osMutexAcquire` 和 `osDelay`。
- FreeRTOS+TCP 及其 STM32 网卡适配层按中间件要求直接使用 FreeRTOS API。
- `common/Drivers/CMSIS` 提供 STM32H563 Device Header 和 CMSIS-Core。
- `common/CMSIS-FreeRTOS/CMSIS/RTOS2/FreeRTOS` 提供 CMSIS-RTOS2 到 FreeRTOS 的适配层。
- FreeRTOS+TCP 的 `phyHandling.c` 已直接识别 LAN8742A，并通过 STM32 HAL 的 MDIO 接口访问 PHY，
  所以工程不再重复链接 ST BSP 的 `lan8742.c`。

## 工程

- STM32CubeIDE：`STM32CubeIDE/.project`
- IAR EWARM：`EWARM/Project.eww`
- 不维护 MDK-ARM 工程。

两个工程都引用 `../common` 下的公共 Drivers、CMSIS-FreeRTOS 和 FreeRTOS-Plus-TCP。I3C 初始化和
DMA/回调逻辑来自已测试通过的原 Controller 工程，继续由源码维护。

## Python 使用

`python/i3c.py` 默认连接 `192.168.1.212:1000`。

```python
from i3c import I3CController

c = I3CController()
c.rstdaa()
c.entdaa()
print(c.targets)

c.target_bind(0x50, 0x10)
c.target_set_active_by_da(0x10)
print(c.getpid(), c.getbcr(), c.getdcr())

c.w(0x01, [0x10, 0x20, 0x30, 0x40])
print(c.r(0x01, 4))
print(c.r_current(4))

c.h(0, 128, print_addr_ahead=True)
print(c.r(3))
c.w(26, 0x08)
```

Target 管理接口还包括 `target_create_by_sa`、`target_list`、`target_set_active_by_sa`、
`target_set_active_by_da`、`target_bind`、`target_set_protocol_by_sa` 和
`target_set_protocol_by_da`。

运行内置测试：

```text
python python/i3c.py --host 192.168.1.212 --port 1000
```

原始 TCP 命令格式见 `i3c_cmd_format.txt`。
