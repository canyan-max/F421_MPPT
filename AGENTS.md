# AGENTS.md

## 工作规则
- Windows 命令使用 PowerShell 7：`C:\Program Files\PowerShell\7\pwsh.exe`。
- 实现前核对源码、工程配置和硬件；不猜测引脚、量程、保护阈值及控制方向。
- 保留用户改动，只修改当前需求涉及的文件。生成代码优先在 user code 区扩展。
- libraries/ 为厂商库，原则上不修改；确需修改先说明并征得同意。
- 裸机、静态分配，不使用 malloc/free 或 FreeRTOS 接口。
- 参数、数组边界、共享数据和硬件等待超时必须处理；临界区短且保存、恢复原中断状态。
- 中断不执行阻塞等待、延时、格式化日志或复杂业务。确定控制执行位置前评估最坏执行时间。
- 功率输出与持续采样分离；默认关断功率输出，实际 HIN/LIN 低电平行为必须验证。
- 未经验证的死区、采样时间、控制频率仅是试验参数。

## 工程事实
- AT32F421C8T7，运行 120 MHz，裸机；MDK 分配 Flash 64 KiB / SRAM 16 KiB。
- 工程：project/MDK_V5/AT32_MPPT_APP.uvprojx。
- 当前工程选择 ARM Compiler 5.06 update 6、ArteryTek.AT32F421_DFP.2.2.0，以工程实际配置为准。
- 不从参考项目推断 FPU、外设映射、编译器或内存配置。

## 分层
01_App -> 02_Service -> 03_Platform/bsp -> 03_Platform/mcu_interface
04_Impl 实现 mcu_interface，可依赖厂商库和 00_Project_Config。
06_Component 提供独立算法；下层禁止反向依赖上层，公开接口不泄漏厂商类型。
project/ 保留初始化、启动、中断入口及装配；参考目录 voice_checkpointe_app/ 不参与当前构建。
详细边界见 99_Doc/architecture.md 和各层 AGENTS.md。

## 验证
- 代码或工程配置修改后只编译：在本目录通过 Git Bash 执行 `bash ./keil_build_flash.bash -b`，要求 0 error、0 warning。
- 不使用无参数脚本，避免自动烧录。只有用户明确要求才烧录。
- 路径失效时报告，不擅自更改工具路径。纯文档、未接入构建的骨架整理无需编译。
- 报告已执行的验证及未验证的硬件行为，不把源码检查等同于上板通过。
