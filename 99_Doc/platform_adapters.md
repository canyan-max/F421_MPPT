# GPIO / ADC 平台适配使用说明

## GPIO
复用参考工程的逻辑资源 ID、读写和翻转接口形式；底层适配 F421。
在 wk_gpio_config() 后使用 plat_gpio_write/read/toggle。
board_resources.h 提供 LED1/2/3、SOL_SWITCH、LOAD_SWITCH 五个逻辑 ID。
接口表示电平，不表示设备开启/关闭；GPIO read 读取引脚，toggle 翻转输出锁存值。
当前绑定服从生成代码：LED 为 PB3/4/5，太阳能开关 PB12，负载开关 PB11。负载宏已统一为 LOADMOS_OUT_PB11_PIN / LOADMOS_OUT_PB11_GPIO_PORT，GPIO 初始化与适配表一致。
EXINT 未配置，不提供空壳中断接口。

## ADC
plat_adc.h 定义五路原始 12 位数据顺序，不进行电压、电流或温度换算。
使用前 ADC 已校准、DMA 已初始化，触发 Timer 停止且 ADC 无转换在进行。
调用 plat_adc_start(buffer, count, callback)，成功后再启动触发 Timer。
缓冲静态分配、至少半字对齐，count 是 uint16_t 元素数，必须是 10 的倍数。
调用方管理缓冲生命周期，不再同时使用 main 中旧的 DMA 地址/长度配置路径。
当前尚未接入 start：用户原有主循环及未填写的 DMA buffer 配置保留，不代表采样已启用成功。

DMA 中断入口调用 plat_adc_dma_irq_handler，回调在中断中执行。
半满回调提供前半区，全满提供后半区；错误或半满/全满积压同时出现时停止 DMA 并通知错误。
100 元素缓冲对应每半区 10 组数据，每 100 us 完成一个半区。
指针只是 DMA 内存视图，不能无限期保留；需要在覆盖前处理或复制，并检测延迟/覆盖。
适配层不保证单一事件下的半区永远有效；回调及后续消费必须满足实时期限。
回调禁止阻塞、日志和复杂计算。后续添加计数、统计及 GPIO 标记时应记录最大执行时间。
停止时先停触发，再调用 plat_adc_stop；不停止 Timer 的持续采样和关闭功率 PWM 是另一项独立设计。

## 验证
新增实现已加入 MDK；仅接入 DMA 中断入口，没有绑定测试 buffer、改变功率输出或占空比扫描。
编译不代替上板验证。后续验证参数拒绝、GPIO 电平、DMA 顺序、回调节拍、错误停止和覆盖检测。

GPIO toggle 不屏蔽中断，是输出锁存器的读改写；同一 GPIO 端口的写入需串行安排，避免主循环与 ISR 并发写入导致翻转丢失。需要明确电平时优先用 write。
ADC start/stop 仅由主循环/启动上下文串行调用，禁止从回调或其他 ISR 调用。stop 先发布停止状态，再关闭 DMA、清标志和释放绑定，不全局关中断。
