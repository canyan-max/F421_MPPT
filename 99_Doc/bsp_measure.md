# 当前 ADC 采样与换算

- TMR15 CH1 每 1 ms 触发一次 ADC 普通组，关闭连续转换。
- 六路顺序：OUT_CUR、IN_CUR、BAT_VOL、VIN_VOL、NTC、VREFINT。
- ADC 时钟 20 MHz；前五路采样 71.5 周期，内部参考 239.5 周期，整轮转换估算 33.6 us。
- DMA 循环缓冲 12 个半字，每半缓冲为完整的一轮六路数据。
- BSP 持有 DMA 缓冲、原始帧、计数与换算结果。初始化在 wk_dma 的 user code 区注册 plat_adc_start 回调，生成 main 随后配置同一缓冲并使能 DMA。
- DMA 中断调用 plat_adc_dma_irq_handler，由适配层清标志、检测错误/积压，再调用 BSP 回调复制完整帧。main 仅调用 bsp_measure_process，换算不进入中断。
- 启动失败不启动 TMR15；生成 main 中的这一判断需在重新生成后保留。
- g_measure_value 为 J-Scope 观测结果；g_measure_count 为成功换算次数，g_measure_status 为最近换算状态。
- DMA 出错后停止 DMA，g_adc_dma_error_count 累加；结果可能保留旧值，必须结合错误计数及更新计数判断有效性。
- 已移除一次性原始值快照、PB3 周期测试翻转和旧快慢分组测试逻辑。
- VDDA[mV] = 1200 * 4096 / VREFINT原始值，四舍五入；各路使用同一帧估算 VDDA。
- 统一函数先换算整数 ADC 引脚 mV，各通道函数独立计算电压、电流，输入及输出电流系数分别配置。
- 电池倍率 11；输入电压为引脚电压 * 20 + 1300 mV（PNP 导通区试验模型，近零不适用）。
- 电流为引脚 mV * 4，尚未做零点校准；NTC 只提供原始值与引脚电压。
- 内部参考使用标称 1200 mV，尚未逐片校准；测量稳定性及精度应与万用表、示波器对照。
- 当前 PWM 仍由 TMR1 独立运行，采样不承担闭环控制。
- 用户已通过 J-Scope 观察持续更新，输入约20 V、电池侧约9.9 V，平均值与万用表接近；并非全量程或闭环验证。

- ADC 引脚电压按 raw * VDDA / 4096 四舍五入；最大合法码及满量程检查仍使用4095。常量除法交给编译器优化。
