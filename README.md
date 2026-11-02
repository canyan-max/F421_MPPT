# AT32 MPPT 裸机工程

同步Buck太阳能充电控制器，AT32F421C8T7、120 MHz，Flash64 KiB/SRAM16 KiB。
当前已接入启动搜索、MPPT、CC、CV、充满待机、充放电保护和LED，处于台架
验证阶段；未完成真实PV、完整瞬态及长期运行验收。参数以源码为准。

- 工程：project/MDK_V5/AT32_MPPT_APP.uvprojx，ARM Compiler5.06 update6。
- PWM100 kHz；ADC500 us扫描；充电控制10 ms。功率输出与持续采样分离。
- 12 V当前CV14.4 V、独立过压14.6 V、CC总输出10 A、硬过流12 A。
- PB0短路信号硬件异常，当前临时旁路；其他未完成问题见工程记录。
- 50 V为器件耐压极限，不是批准的输入工作上限。

在项目目录通过Git Bash执行 `bash ./keil_build_flash.bash -b` 仅编译。
不要省略 `-b`：无参数脚本默认编译并烧录，只有用户明确要求才烧录。
参考目录voice_checkpointe_app不参与当前构建。

## 文档入口

- [工程当前记录与待办](.agents/project-memory.md)
- [充电流程和D上下限](99_Doc/charge-state-split-2026-10-23.md)
- [工程分层](99_Doc/architecture.md)
- [采样和换算](99_Doc/bsp_measure.md)
- [VIN模拟支路与历史测量](99_Doc/adc_input_voltage_formula.md)
- [历史CV故障证据](99_Doc/mppt-cv-950-investigation-2026-10-17.md)

工作规则见AGENTS.md；历史试验实现通过Git提交查阅。
