# UART / RS485 底层回发测试

2026-11-02：现已通过svc_comm接入HIK测量查询，替换main临时原样回发。
用户已确认UART1（RS485）和UART2（普通串口）均可通信，暂定为“初步可用”。
保留当前实现作为后续开发基线；未提供具体测试帧、并发压力或测试时长。
此结论不扩展到异常恢复、长期连续运行及采样中断影响。

## 当前协议接入

main启动调用svc_comm_init，循环调用svc_comm_process；Service负责推进BSP、
取段、调用svc_packdata_handle并异步发送。每轮每路最多取一段。
svc_comm遍历g_comm_ports通道表，表索引0/1分别代表UART1/RS485、UART2；
各路上下文数量由表长推导，初始化失败不阻止其他成功通道运行。
两路各有RX256/TX40字节缓冲和待发长度，协议结构体为局部变量；本路BUSY期间
不覆盖发送缓冲，一路发送不会阻挡另一路组包。应答仅从收到查询的端口返回。
UART1走bsp_rs485；UART2走bsp_uart2，无GPIO或发送建立延时，发送期间仍接收。
未接受的BUSY提交保留应答到后续循环；已接受的发送不自动重发。

协议与项目通过数据提供回调连接：

- proto_hik_provider_t定义pf_get_battery/pf_get_solar；返回1表示有数据，0不应答。
- svc_packdata的get_hik_battery/get_hik_solar负责本项目取数、单位换算和字段映射；
  只读g_hik_provider在每次调用proto_hik_handle时传入，没有协议全局注册。
- proto_hik_handle负责校验、命令分发和组包，先清零局部协议结构体再调用回调；
  回调不能保留结构体指针。缺失回调、无数据或输出容量不足均返回应答长度0。
- proto_hik.c/h留在06_Component，不包含Service/BSP头文件。换项目只替换provider。
  INFO/SN仍未接入，不改变已有应答范围。

| 查询 | 字段来源 |
| --- | --- |
| 03 电池 | battery_voltage_mv / 10，battery_current_ma / 10（正充负放） |
| 03 温度 | 有效temperature_deci_c + 2731，单位0.1 K，数量1；无效则数量0 |
| 0F 太阳能 | input_voltage_mv / 10；用户指定input_current_ma / 10填太阳能电流 |

input_current_ma是Buck输出电流，此映射按用户兼容需求采用，不是PV电流测量。
其余未确定字段填0；转换先饱和再窄化，不假设真实SOC、容量或FET状态。
没有有效测量、校验失败、非03/0F命令均不应答；SN存储和INFO查询尚未接入。

两路均使用9600/8N1，沿用用户当前初始化参数：

- UART1：PB6 TX/PB7 RX，经板上485收发器连接USB转485，优先级2。
- UART2：PA8 TX/PA15 RX，连接匹配板端电平的USB转TTL（TX接PA15、RX接PA8、共地），
  当前用户配置优先级0。仅复用同一协议，无调试日志输出。
- UART2也以5个1 ms计数差（约4～5 ms）静默封段，FIFO256字节/4段描述。
  一个查询内部不能停顿达到静默门限，不能将此当作无间隙连续帧解析器。

串口助手十六进制发送：

- 电池查询：`DD A5 03 00 FF FD 77`。
- 太阳能查询：`DD A5 0F 00 FF F1 77`。

返回协议应答，不再返回原查询字节。电池有效温度时应答32字节，无效时30字节；
太阳能应答12字节。等待应答完成后再发下一次查询。
Watch查看g_comm_init_status（两路初始化的首个错误）、
g_comm_channels[0].init_status（UART1）、g_comm_channels[1].init_status（UART2），
以及g_rs485_stats/g_uart2_stats。main始终调用Service，初始化成功的通道独立运行。
旧echo_*和g_rs485_init_status变量已移除。

最新表驱动整理MDK编译0 error/0 warning：Code28748/RO504/RW404/ZI3932；未烧录。
双UART主机回归通过：原路回复/独立TX缓冲、UART2全双工、非法帧与未知ID、
空参数/容量边界/无数据不应答、发送BUSY保留原应答及异常恢复。本次未上板复测。
此前仅协议文件独立GCC编译/测试也通过；协议仍无项目依赖。
主机模拟已验证映射、零字段、带符号电流、饱和边界、无效温度/校验及发送忙。
.agents/scripts/test-comm-uarts.ps1使用真实adapter/BSP/kfifo/Service/协议配合模拟硬件，
通过双路交错查询/原路应答/发送缓冲隔离、UART2发送期间接收、段溢出/接收错误、
计数回绕及超时中止后重发。用户已确认两路实际可通信；UART2波形及采样中断延迟
尚未提供验证结果。
UART2沿用优先级0，本次没有测量其最坏中断耗时和对采样的影响。
USART1/2均在user区分发adapter，并用跨user区的#if 0屏蔽生成示例，避免重复读取；
生成初始化完成后由BSP绑定再启用RX/错误中断，再生成后核对这些保留区。

## 后续增加协议

svc_packdata.c的g_protocols是协议ID表，现有`0xDD -> HIK`与`0x15 -> 本机Modbus RTU`。
本机协议支持03读取；06当前无可写项并返回异常02，详情见local_communication.md。
svc_packdata_handle遍历表，匹配请求首字节后调用该协议的统一签名入口；
该入口将本项目provider传给独立协议模块。匹配后即由该协议处理，校验失败、
命令不支持或无数据时直接不应答，不再把同一段交给其他协议。

增加协议时：

1. 将可移植协议模块放到06_Component/Protocols，并接入工程。
2. 在svc_packdata.c增加本项目数据回调、调用该协议的入口和g_protocols表项。
3. 核对请求/应答最大长度；当前comm每路RX256/TX40，较大协议需调整容量，
   同时不能超过BSP当前收发能力。

协议单位和数据类型各自独立，不强制共用一份测量结构体；不用每协议建Service。
只增加协议不修改svc_comm流程。若协议共用首字节，需扩展packdata匹配规则，
不能在表中登记重复前缀。当前仍按BSP静默段处理，一段包含多帧时不自动拆帧。
若增加物理通道，在comm中增加BSP归一化函数和通道表项，公共流程保持不变；
新通道的硬件初始化、IRQ和SysTick装配仍需另行接入。
## 以下为早期底层回环验证记录

以下回环开关、echo_* Watch变量和main装配描述属于此前版本，现由上述Service替换。
BSP容量、收发时序及底层接口仍沿用。


## 分工

- `plat_uart.h` / `at32f421_uart_adapter.c`：逻辑 ID 索引硬件表与独立上下文。
  当前 BOARD_UART_RS485=0 对应 USART1，BOARD_UART_DEBUG=1 对应 USART2。
  两路分别保存回调、发送缓冲指针、长度、位置及发送状态；共用实现。
  UART2 只具备适配能力，本次未接入其 IRQ/NVIC 或通信业务。
- `bsp_rs485.c`：仅绑定 UART1，用现有 kfifo 收字节，保存每段结束位置，
  管理 PF7 和接收超时。用户确认 PF7 高发送、低接收，DE 与 /RE 相连。
- `project/src/main.c`：临时原样回发装配；主循环调用 BSP 的 process，
  然后取一段、异步提交发送。APP_RS485_ECHO_TEST_ENABLE=1 默认启用。
  后续关闭该开关，用协议调用同一 BSP 接口即可。

## 当前试验参数和边界

- USART1 保持生成的 9600、8N1，PB6 TX / PB7 RX，NVIC 优先级2。
- 复用现有1 ms SysTick。距最后一次接收计数差>=5才封段，正常节拍下约
  4～5 ms静默；不是硬件精准定时，也不代表完整 Modbus RTU 支持。
  每段内部不能有达到该门限的停顿。封段只存 FIFO 结束位置和损坏标记。
- 接收 FIFO 总容量256字节，最多保存4个段描述。不是每段各有256字节空间。
  单段256字节可原样回发；超过容量、UART错误导致本段丢弃，下一次静默后
  恢复收集。段描述满则丢弃新段并回退尚未发布的数据，不影响旧段边界。
- BSP不修改现有kfifo。主循环每次最多在短临界区取16字节，SysTick封段也
  保存/恢复原中断状态，防止优先级更高的UART抢占边界发布。
- 一次只发送一段，借用调用者缓冲直到BSP busy清零。PF7拉高后计数差>=1
  才启动UART，约0～1 ms加主循环调度延迟，不保证至少1 ms。
  原2计数差配置用户测得约2.04 ms；本次缩短为1计数差，待用户复测首字节。
  收发器型号尚未确认；这是试验参数，不代表数据手册时序验收。
- TDBE逐字节装载，最后一个字节后切换到TDC。只有TDC到达才释放PF7；
  发送期间不接收，适用于主机查询后等待应答的半双工测试。
- 发送总超时为长度×2+20 ms，依据当前9600/8N1保守设置。超时终止后续装载，
  PF7切低并返回FAILED；可能截断已装载字节，不计为完成，不自动重发。
  中止成功后等待新的5计数差静默，可再次提交；GPIO或adapter失败需复位。
  GPIO切换失败时只能报告操作失败，不能保证实际电平已切低。
  更改波特率时必须重新核对上述时间参数。
- IRQ入口独占分发到adapter，原生user区中的return避免再次生成的示例分支
  消费同一字节。wk_usart的user区关闭中断，直到BSP绑定完成才开放接收。
  再生成后仍应检查这些保留区。

## 上板测试

使用外部USB转485与板上A/B正确连接，并核对参考地；不用短接MCU TX/RX。
串口助手设置9600、8N1，关闭本地回显和自动周期发送，适配器发送后及时释放
总线。一次发送例如十六进制 `01 02 00 A5 FF`，等待返回相同5字节，再发下一段。

建议依次测1字节、含00/FF的短段、256字节，重复查询观察内容与次数；再发送
超过256字节的连续段，应不回发该损坏段，随后短段应可恢复回发。测试间隔先
留几十毫秒，256字节一问一答需约0.55 s以上，不要在板端应答时继续发送。

Watch：

- `g_rs485_init_status`：正常为 PLATFORM_ERR_OK=0。
- `g_rs485_echo_submit_count`：主循环成功提交回发次数。
- `g_rs485_echo_status`：最后一次发送提交的平台返回值。
- `g_rs485_echo_rx_result`：最后一次取段结果，空闲为EMPTY，损坏段为DROPPED。
- `g_rs485_echo_tx_result`：主循环更新的发送状态，IDLE / BUSY / DONE / FAILED。
- `g_rs485_stats`：收字节、封段、接收错误、溢出、丢段、发送完成、发送错误及
  超时计数；只供Watch观察，多字段读取不保证同一瞬间。

## 简化后的错误处理（未接Service）

`bsp_rs485_read_segment(data, capacity, &count)`取出一整段：

| 返回值 | 含义 |
| --- | --- |
| BSP_RS485_RX_OK | 有效数据，count为长度 |
| BSP_RS485_RX_EMPTY | 没有完整段，正常等待 |
| BSP_RS485_RX_DROPPED | 当前损坏段已丢弃，count=0 |
| BSP_RS485_RX_BUFFER_SMALL | 保留段，count返回所需容量 |
| BSP_RS485_RX_PARAM | 参数无效，输出不改动 |
| BSP_RS485_RX_ERROR | 未初始化或底层无法继续，停止交付 |

串口错误或FIFO满标记当前段损坏，静默后继续接收下一段；段描述满丢新段，
保留旧段。不提供错误位、错误通知清除接口或状态快照。

`bsp_rs485_tx_get_result()`返回IDLE / BUSY / DONE / FAILED。仅TDC到达且方向
切回接收成功才是DONE；结果保持到下一次发送被接受。发送缓冲在BUSY期间
必须保持有效且不改动。超时中止成功后返回FAILED，等待静默后可重新提交；
若中止无法确认，则保持BUSY保护借用缓冲，复位前不能复用。GPIO/adapter
失败停止通信，不在底层自动重试或重新初始化。初始化接口仅在启动时调用。

用示波器核对PF7在首个起始位前拉高、末尾停止位后拉低；检查建立时间及
回发等待时间，并观察充电/采样是否正常。不能用发送完成计数证明485电气通过。

## 已执行验证

- `.agents/scripts/test-rs485.ps1`：真实adapter/BSP/kfifo配合硬件模拟，覆盖非法
  ID、UART1/2收发和中止隔离、静默断段、相邻段端点、256字节与缓冲回绕、
  毫秒计数回绕、溢出/串口错误恢复、段描述满、旧TDC、发送完成和超时故障。
  超时后静默等待及重新发送也已覆盖；仅为主机模拟。
- Git Bash `bash ./keil_build_flash.bash -b`：0 error、0 warning；
  Code26260/RO448/RW380/ZI3252。此次精简未烧录，异常路径未上板验证；
  用户此前已初测正常回发及TDC方向切换，新1计数差建立时间待复测。
