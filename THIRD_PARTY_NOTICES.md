# 第三方软件声明

本仓库包含 STM32CubeMX 生成或基于 STM32CubeH5 模板维护的启动、系统初始化、链接脚本和外设配置文件。
这些文件保留各自的版权声明，并按文件头及 STM32CubeH5 软件包中的许可证使用。

本仓库通过外部 `../common` 目录引用以下组件，不直接复制其完整源码：

| 组件 | 版权方 | 许可证 |
| --- | --- | --- |
| STM32H5 CMSIS Device、HAL/LL Drivers | STMicroelectronics | Apache-2.0 / BSD-3-Clause |
| CMSIS Core | Arm Limited | Apache-2.0 |
| CMSIS-FreeRTOS、FreeRTOS Kernel | Arm Limited、Amazon Web Services 等 | MIT |
| FreeRTOS-Plus-TCP | Amazon Web Services 及贡献者 | MIT |

使用或再分发上述组件时，应以对应源码目录中的许可证文件为准。本项目的构建工具 IAR Embedded
Workbench 和 STM32CubeIDE 不属于本仓库的分发内容。

官方项目：

- [STM32CubeH5](https://github.com/STMicroelectronics/STM32CubeH5)
- [CMSIS-FreeRTOS](https://github.com/ARM-software/CMSIS-FreeRTOS)
- [FreeRTOS-Plus-TCP](https://github.com/FreeRTOS/FreeRTOS-Plus-TCP)
