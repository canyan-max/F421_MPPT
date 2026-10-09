# AT32 MPPT 裸机工程

同步 Buck 太阳能充电控制器。当前处于 PWM / ADC / DMA 台架验证阶段，尚未实现闭环、MPPT 和完整保护。

- MCU：AT32F421C8T7；运行时钟 120 MHz；工程分配 Flash 64 KiB、SRAM 16 KiB。
- 工具链：当前 MDK 工程选择 ARM Compiler 5.06 update 6，DFP 2.2.0。
- 电池：磷酸铁锂，4 串 14.6 V 或 8 串 29.2 V；最大充电 12 A、负载放电 10 A。
- PV 最低工作电压暂定 18 V；组件耐压 50 V 不等于允许的输入运行上限。
- PWM：100 kHz；固定 50% 仅用于开环测试。

结构与实施边界见 [架构说明](99_Doc/architecture.md)。

当前可编译源码仍在 project/ 与 libraries/；新增目录是后续逐步接入的骨架，不代表相关功能已经实现。
voice_checkpointe_app/ 保留作结构参考，不参与当前构建。

在本目录通过 Git Bash 执行 `bash ./keil_build_flash.bash -b` 仅编译。只有明确要求时才烧录；脚本不带参数默认编译并烧录，调试时不要省略参数。
