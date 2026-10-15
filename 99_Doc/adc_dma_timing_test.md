# ADC 采样验证

当前接线、采样和换算说明见 [bsp_measure.md](bsp_measure.md)。

检查 g_adc_frame_count 每秒约增加1000，g_adc_dma_error_count 保持0。
J-Scope 查看 g_measure_value 的输入电压、电池电压、电流及 VDDA。
J-Scope 读取频率不等于 MCU ADC 采样频率，结构体实时读取也不是原子快照。
周期 GPIO 翻转已在验证后移除；当前定时器配置为1 ms，一轮六通道。
