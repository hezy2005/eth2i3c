# 构建依赖

[返回项目 README](README.md)

工程文件通过相对路径引用同级 `common` 目录，因此单独 clone 本仓库后，需要先准备以下依赖。

## 已验证版本

| 组件 | 已验证版本 | 本地版本依据 |
| --- | --- | --- |
| STM32CubeH5 / STM32H5 HAL | V1.5.0 | `stm32h5xx_hal.c` 中 HAL 版本为 1.5.0 |
| CMSIS-FreeRTOS | 11.3.1-dev | `ARM.CMSIS-FreeRTOS.pdsc` |
| FreeRTOS Kernel | V11.3.0 | `Source/include/task.h` |
| FreeRTOS-Plus-TCP | V4.4.1 | `source/include/FreeRTOS_IP.h` |
| IAR C/C++ Compiler for Arm | 9.50.2 | EWARM clean build 已验证 |
| STM32CubeIDE | 1.17.0 | Debug clean build 已验证 |
| Python | 3.8 或更高版本 | 客户端仅使用标准库 |

## 目录结构

```text
workspace/
├── common/
│   ├── CMSIS-FreeRTOS/
│   ├── Drivers/
│   │   ├── CMSIS/
│   │   └── STM32H5xx_HAL_Driver/
│   └── FreeRTOS-Plus-TCP/
├── eth2i3c/
└── eth2i3c_target/
```

`common` 中只需要保留工程实际引用的组件。IAR 和 STM32CubeIDE 工程已经使用上述相对路径，不需要配置
开发者机器上的绝对目录。

## 获取依赖

- STM32H5 CMSIS Device 和 HAL/LL Drivers 来自
  [STM32CubeH5](https://github.com/STMicroelectronics/STM32CubeH5) V1.5.0。
- CMSIS-FreeRTOS 来自
  [ARM-software/CMSIS-FreeRTOS](https://github.com/ARM-software/CMSIS-FreeRTOS)。
- FreeRTOS-Plus-TCP 来自
  [FreeRTOS/FreeRTOS-Plus-TCP](https://github.com/FreeRTOS/FreeRTOS-Plus-TCP)。

当前 `common` 是经过实机验证的源码快照，不包含用于确认精确 commit 的 Git 元数据。替换为更新版本时，
应重新执行两个 IDE 的 clean build，并复测以太网连接、ENTDAA 和寄存器访问。
