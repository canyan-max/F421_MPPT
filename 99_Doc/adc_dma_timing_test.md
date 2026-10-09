# ADC / DMA 节拍台架测试

当前测试已在 main.c 接入 plat_adc_start：100 个 uint16_t 静态元素，20 组五路数据，循环搬运。
TMR1 CH1=300，周期 10 us，ADC 一组约 6.5 us。
功率主输出关闭，Timer 计数和 CH1 触发保持；占空比扫描暂停。
PB10（BT_VDD_EN）、PF7（RS485_TXEN）已加入 GPIO 平台资源，测试不切换这些功能引脚。

## 示波器
只给控制部分供电，功率侧和电池保持断开。
探头接 PB3，地夹接 MCU GND；建议先用 50 us/div。
DMA 每完成 50 个半字（10 组）翻转一次 PB3：相邻边沿 100 us，完整周期 200 us，频率 5 kHz，电平约 0～3.3 V。
每路每 10 us 采样一次；该 GPIO 标记表示批量完成及 ISR 延迟，不能直接测出 CH1 的 2.5 us 相位。
PB4 为错误电平，正常保持低；有错误置高，不依据 LED 亮灭判断电平。

## 调试观察
adc_test_start_result 为 PLATFORM_ERR_OK。
adc_test_half_count / adc_test_full_count 持续增长，差值通常不超过 1。
adc_test_error_count 保持 0。
adc_test_buffer 通道顺序为 OUT_CUR、IN_CUR、BAT_VOL、VIN_VOL、NTC，循环重复。
暂停 CPU 调试会积压 DMA 标志并可能触发保护停止；断点后重新复位再测波形。

## 验收范围
连续波形能验证 ADC 转换/DMA 完成链路持续工作、批量节拍符合预期。
原始值正确、通道对应和模拟精度需要后续施加已知电压验证，不能仅凭 GPIO 方波判定。
本次只编译，未烧录或确认板上结果。
重新生成初始化代码时，确认功率主输出仍关闭，并保留测试缓冲接入逻辑。

## 触发配置修正
CH1 使用 PWM 模式 B，无引脚输出；TMR1 总输出逻辑保持使能。不能用关闭总输出的方式假定 CH1 ADC 触发不受影响。
当前采样测试禁用 CH3/CH3C，且 PA10、PB1 切为普通推挽 GPIO 并写低，隔离功率输出。恢复 PWM 时必须重新配置两引脚复用及通道使能；不要只打开总输出。
此修正已编译，实际触发恢复及波形仍待上板验证。

## 重新生成后的接入修复
生成区中的 plat_adc_start 曾被覆盖。测试绑定现放在 wk_dma1_channel1_init 的 user code 区，先注册缓冲/回调，再暂时关闭通道；生成 main 使用同一缓冲地址和 100 元素长度配置并使能通道，最后启动 Timer。长度/地址宏在允许编辑的 dma define user 区。测试回调和静态生命周期缓冲保留在 main user 区。
当前重新生成后的 CH3/CH3C 已恢复 PWM 输出，测试保持功率侧断电。
