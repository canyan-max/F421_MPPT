# AT32 MPPT 工程当前记录

更新时间：2026-11-02。参数以源码为准，主机回归不等于上板通过。
本文件仅保留有效方案、硬件限制、验证结果和未完成事项；已撤回的试验
不再作为当前指令。历史实现通过 Git 提交查看，不整包恢复旧 PI 试验。

## 工作约定与版本

- 小步修改，不整体替换算法，不增加无必要主状态。遵循根目录及分层 AGENTS.md。
- 普通代码修改只执行 `bash ./keil_build_flash.bash -b`，不自动烧录、提交或推送。
  用户明确授权后才执行对应动作；保留工作区改动，不修改厂商库。
- 已提交并推送检查点：`ea3b388`，含上下限冲突恢复、诊断拆分及故障3记录。
- 最近同版 MDK 编译：0 error、0 warning，Code23400/RO440/RW332/ZI2628。
  duty-reference 两种维护开关和 MPPT 趋势主机回归通过。未由代理烧录。
- 2026-11-02用户指定当前版本为“初步可用基线”，本地提交文档整理；代码沿用ea3b388。
  INPUT-ERR-01等未完成事项保留，不作为完整硬件验收。未改变参数/策略，不重新编译。

## 硬件与测量事实

- AT32F421C8T7，120 MHz，裸机、静态分配；MDK Flash64 KiB/SRAM16 KiB。
- PWM100 kHz，TMR1 PR1199；D接口单位为千分比，950代表95%，不是定时器CCR。
  非零编码范围1～950；1只是接口编码下限，不是充电安全门槛。关闭输出用关PWM接口。
- ADC由TMR15每500 us触发；六路扫描OUT_CUR/IN_CUR/BAT_VOL/VIN_VOL/NTC/VREFINT。
  电压/两路电流8点滤波；256连续有效帧后ready；充电控制10 ms一轮。
- input_current_ma是Buck总输出电流；output_current_ma是末端负载电流；
  battery_current_ma=总输出-负载，正值表示净充电。没有PV侧输入电流测量。
  MPPT以Vbat×Buck总输出电流作为输出功率代理，不计算Vin×该电流作为输入功率。
- BAT_VOL由ADC换算，非D推算；Watch/J-Scope读取异步，故障后实时值不是触发快照。
- 无可靠负向/过零电流检测，不能单独关闭下管；PB12不是双向隔离开关。
  电池为负载供电不等于向输入反灌。器件50 V耐压极限不等于允许工作上限。
- 接线纠正后，早期中途接入负载异常已消失，不再沿用接错线时的因果推断。

## 当前启动、D边界和更新时机

- 默认关功率输出、持续采样；ready及保护条件通过后进入正常启动。
- 开PWM前采样Vin，保存g_charge_input_voc_mv作为Voc估计；没有额外开路等待/
  稳定度检测，不把它当作已经验证的真实开路电压。
- g_charge_input_reference_mv=floor(启动Voc估计×875/1000)，本轮运行保持。
- 初次D=round(Vbat/Vin×1000)+10‰，限制不超过950及辅助上限；
  START_SETTLE约20 ms内按新采样重算相同启动候选，随后进入CV/CC/MPPT。
- 有效运行帧约每10 ms重算：
  - 下限g_charge_duty_min_reference_permille=round(实时Vbat/实时Vin×1000)。
  - 上限g_charge_input_duty_max_permille=floor(实时Vbat/保存输入参考×1000)，
    范围限制1～950；保存的Voc/输入参考不随带载Vin更新。
- 下限目的：缺少反向/过零检测时，用电压比作软件约束，降低反向升压损坏
  输入器件的风险。保留下限，不因维持CV/追功率而随意放开；不是硬件防反灌保证。
- 上限目的：辅助限制继续取电，避免D一路推到950拖低前端。
  D上限不等于真实功率边界，也不能保证Vin维持在参考；尚无独立输入电压调节环。
- CC/CV/MPPT当前D及候选D统一限幅到有效[min,max]。下限抬升可主动抬D；
  过压/硬过流等保护优先执行。MPPT被限幅时resume清除旧功率比较基准。
- min==max允许；min>max时先停止本轮，不能为满足下限继续抬D。
  复用FAULT6/DUTY_LIMIT7，g_charge_input_range_conflict=1，记录原状态/D/测量。
  保留冲突证据到新启动，显式初始化/启停按原复位规则清理。
- 连续满足原启动/恢复条件5 s后回WAIT_SAMPLE，重新采PWM关闭时的Vin，
  不要求恢复到旧参考；这不是额外的输入稳定度检测。停止失败先重试关断。
  显式stop后不自动启动。
- g_charge_input_limited只观察Vin低于保存参考，不主动减D、不控制CV退出/重入。
- 当前没有定时关PWM重测Voc，也没有运行中自动重启快速搜索。
  周期重测刷新的是上限的旧参考，不直接改变实时下限，不能代替输入不足时即时减载。
- 已撤回：CV_WAIT状态10、CV参考减15‰、越过下限保持旧D、输入低200 ms
  后退出CV及200 mV恢复回差；这些均不是当前策略。

## 充电状态、参数与模块

- 优先级：保护>CV>CC>MPPT。状态OFF0/WAIT_SAMPLE1/START_SETTLE2/WAKE_CV3/
  CC4/CV5/FAULT6/FULL_WAIT7/MAINTENANCE8/MPPT9，无状态10。
- 12 V：CV目标14.4 V，14.38 V进入，<=14.30 V退出；独立OVP>=14.6 V。
  24 V源码为CV28.8 V、OVP29.2 V，暂不继续调24 V。
- CC总输出目标10 A，9980 mA进入；满足原迟滞条件且<=9900 mA释放。
  硬过流12 A。D达到上限本身不是单独故障。
- MPPT启动搜索300 ms稳定+200 ms平均，每窗向上5‰，明显失益回退，
  最多20窗口。正常每500 ms一步1‰，0.1 W死区，最多8次累计趋势比较。
  CC/CV中断取消粗搜索；释放回MPPT保持当前D、重建基准并验证邻点方向。
  MPPT阶段OFF0/STARTUP1/TRACKING2/PAUSED3/REACQUIRE4。
- CV充满判据：Vbat>=目标-200 mV、净电流-500～500 mA连续10 s（1000帧），
  不满足清零；锁存充满后，有负载继续14.4 V CV。长期满电供电策略未验收。
- 负载>=100 mA置存在；<=80 mA连续1 s确认无负载，中间保持。
  已充满且无负载则关PWM进FULL_WAIT7；负载存在约20 ms，或Vbat<=13.8 V
  连续5 s，且启动条件允许时重新正常启动。输入缺失时等待。
- SVC_CHARGE_ENABLE_MAINTENANCE=0。保留可选维护代码，不调用状态8，
  充满/负载公共判断仍复用maintenance模块。原维护14.1 V及回充配置保留。
- 充电温度<=0℃/≥65℃触发，>=5℃/<=55℃恢复，确认2 s；普通恢复5 s，
  充电过流恢复20 s；待机仍检查采样、NTC、温度、过压及硬过流。
- svc_charge管理采样/状态/保护调用及PWM；control计算初始D/输入条件；
  cc/cv/mppt只计算，不写PWM；config存参数；protection判断故障；
  diagnostics集中CV观察、误差、Watch兼容标志和故障快照，不控制PWM。
- 诊断头为svc_charge_diagnostics.h，原Watch名称保留。g_charge_cv_observation
  在新有效CV帧调D前更新，退出CV保留，显式init/start/stop清零。
  总输出<=100 mA只是观察阈值，不是反灌安全判据。

## 快速保护、生成代码及临时旁路

- 12 V DMA每帧BAT_VOL原值>=1760直接plat_pwm_outputs_off，仅关PWM，ADC继续。
  24 V禁用该快速原值判断；Service独立14.6 V/10 ms保护保留。
- 快速路径无事件锁存/上层通知/恢复互锁，不关输入开关；普通set_duty只写CCR，
  不会重新启用被快速路径关闭的PWM。可能出现业务D非零但硬件输出已关闭。
  g_charge_fast_ovp_trip_raw及off_status为历史最近触发值，无时间戳。
- 用户500 us实测尖峰约15.4～15.8 V，原因未定位；100 us无法正常运行，
  当前已恢复500 us。1760为当前试验门限，不回退到先前1765。
- 自定义启动装配位于main.c原生user code 0的app_startup_before_sampling，
  由wk_tmr15_init原生tmr15_init 1调用，采样定时器/EXINT使能前完成绑定。
  当前生成的TMR1初始化先使能输出，随后在原生user区关闭输出及计数器。
  再次生成保留行为和上电波形仍需验证，不能恢复生成器删除过的自建标记区。
- PB0短路硬件信号异常，用户授权临时屏蔽：BSP_DISCHARGE_SHORT_TEST_BYPASS=1，
  project/src/at32f421_int.c中PB0 IRQ分发亦被用户注释。保留两处，不自动恢复。

## 验证结论

- 用户已初步确认MPPT启动、CC/MPPT切换及CV/充满流程；最新上下限冲突恢复
  初测暂未观察反复启动。以上不是所有输入范围、真实PV或长期稳定性验收。
- 最近主机回归覆盖两种维护开关的D范围/同帧限幅、min==max、冲突关断前
  不写PWM、5 s恢复重采Voc、停止失败重试、显式停止、OVP/OC优先，以及
  256连续帧ready/断帧复位；MPPT趋势/启动/方向交接回归通过。
- 较早维护/状态完整回归属于对应历史配置；不把旧4 A测试通过当10 A验收。
  最新诊断拆分未单独完成硬件回归。

## 待办（保留未完成，不因文档清理删除）

1. INPUT-ERR-01：必须定位故障3的真实触发路径，尚未修复。
   - 2026-10-30截图：状态6、故障3，故障后实时Vin23912 mV、Vbat13979 mV、
     Buck总输出0 mA；不能以停机后读数认定触发时欠压。
   - 当前故障3来源包含Vin<=Vbat、D高限且总输出<100 mA连续200 ms，
     以及总输出<100 mA且净电流<0连续50 ms。
   - 最后一旧判断update_maintenance_reverse仍在CC/MPPT调用，不检查Vin，
     微小负偏差可满足；此前主机曾复现20 V/总输出15/负载17/净-2 mA误判路径。
     本次截图原因仍待故障快照证实，不擅自取消保护。
   - 下一步检查g_charge_fault_state_snapshot、g_charge_fault_duty_snapshot及
     g_charge_fault_measure_snapshot的Vin、总输出、负载、净电流。
2. VOC-01：周期性暂停取电、等待后重测Voc的机制待设计，未实施。
   目的为刷新长期变化后的输入参考；周期、等待时间、准入/恢复及负载由电池
   接管的波形尚未确定，不直接照搬其他芯片参数，不替代即时输入调节。
3. INPUT-CTRL-01：运行中前端不足的减载/退出协调待确认。D限幅不等于输入
   电压闭环；检查动态下限抬D和输入下跌的相互作用，不随意突破下限。
4. MPPT-01：缓慢升压有时收敛慢；用户暂缓专门重搜流程，后续继续验证。
5. FAST-OVP-01：定位15.4～15.8 V峰值、检测/关断延迟及残余能量；
   快速关PWM与业务状态/恢复的交接尚未解决，历史原值不能证明本次触发。
6. 板上验证：带载CV/无载待机/负载及电压唤醒、输入骤变/软启动、轻载CV
   受下限阻挡时的行为、保护注入、校准、实际PV、大电流温升、长期运行。
   独立输入工作范围/最高准入值仍未确定；50 V耐压不可直接用作工作上限。

## 放电与LED（当前保留）

- 放电200 ms任务，PB11高开低关，默认低；手动许可默认1，setter(0)
  立即请求关，Watch改标志下周期生效；正常采样约5 s允许开。
- 低温-40/-30℃、高温75/65℃，确认2 s/恢复5 s；12 V欠压10/12 V，
  24 V20/24 V，确认2 s/恢复5 s；负载过流9 A确认2 s/恢复10 s；停更2 s关断。
- 状态WAIT0/MANUAL_OFF1/ON2/PROTECTED3/FAULT4；故障位低温1/高温2/
  欠压4/过流8/测量16/GPIO32/配置64/短路128，短路当前旁路。
- PB5充电/PB3电池/PB4负载LED低亮；待机7充电灯灭。输出命令不是MOS
  实际电气反馈，充放电启停不互锁。

## 双串口检查点

- 2026-11-02：用户要求提交当前双串口初步可用版本，后续再接其他协议。
  保留用户最终生成配置：UART2 PA8 TX/PA15 RX（PA2/PA3回到模拟输入），
  UART1 PB6 TX/PB7 RX，均9600/8N1；包含生成器配置、MDK和kfifo依赖。
  TMR1生成代码的输出使能保留用户版本，原生user区仍立即关闭输出及计数器；
  此次不改控制策略，不把初始化期间的电气行为视为已验证。

## 文档入口

- 2026-11-02新增UART/485底层：adapter按逻辑ID绑定USART1/2且每路独立状态，
  UART2已接入IRQ/bsp_uart2及同一svc_comm；UART1仍由bsp_rs485管理PF7。
  用户确认PF7高发送低接收、DE与/RE相连。
  UART1保持9600/8N1，SysTick 1 ms，5计数差静默断段；kfifo总容量256字节、
  4段描述，错误/溢出丢损坏段。发送建立改为1计数差（约0～1 ms加调度延迟），
  待用户复测；原2计数差示波器约2.04 ms。TDC切回接收用户初测波形未见提前释放，
  不额外增加发送尾部延时，非完整电气验收。接收静默仍为5计数差。
  main现调用svc_comm，临时回环已移除；只应答HIK的03/0F测量查询。
  按用户要求精简错误处理：read_segment三参数，返回OK/EMPTY/DROPPED/
  BUFFER_SMALL/PARAM/ERROR；损坏段直接丢弃，BUFFER_SMALL保留段并返回所需容量。
  tx_get_result保留IDLE/BUSY/DONE/FAILED；仅TDC+切回接收成功为DONE。
  删除错误位、通知清除接口和状态快照，只留基本Watch计数。
  超时中止成功后FAILED，等待5计数差静默可再次提交；GPIO/adapter异常停止通信
  需复位。中止未确认则保持BUSY保护借用缓冲。初始化只在启动时调用。
  主机模拟通过：双UART隔离、断段/边界/溢出/错误恢复、TDC与超时后重发；
  MDK 0 error/0 warning，Code26260/RO448/RW380/ZI3252，未烧录；本次异常路径
  未上板验证。测试入口见99_Doc/rs485-bringup.md。

- 2026-11-02协议类型整理：06_Component/Protocols/proto_hik.h自行定义
  proto_hik_battery_t与proto_hik_solar_t，移除telemetry_types.h及comm_*依赖。
  保留原字段和逐字段组包格式；电压10 mV、电流10 mA、电量10 mAh、温度0.1 K。
  电池电流为int16_t；温度上限由现有40字节最大应答推导为5项。
  svc_comm两路上下文各持有RX256/TX40字节缓冲及待发长度，组包使用局部协议
  结构体；BUSY期间不覆盖本路发送字节，各路独立查询并从原路回复。
  03接电池电压/净电流（正充负放）/有效NTC温度；0F接输入电压，按用户指定
  将Buck输出电流input_current_ma填太阳能电流；不代表PV输入电流测量。
  无来源或未约定映射字段填0；无有效测量不应答，SN/INFO等命令暂不应答。
  换算电压/电流除10并饱和，温度0.1℃+2731转0.1 K，无效NTC数量为0。
  MDK编译0 error/0 warning，Code26984/RO448/RW380/ZI3292；未烧录。
  主机模拟验证映射/缺失字段0/带符号电流/边界饱和/无效温度与校验/发送忙；
  后续用户已确认两路串口均可通信，见下方初步可用记录。
- 2026-11-02 UART2接入：用户生成配置最终为PA8 TX/PA15 RX、9600/8N1、优先级0，
  沿用该配置；UART1优先级2。UART2无需GPIO/建立等待，支持发送期间继续接收。
  bsp_uart2使用kfifo256字节、4段描述、SysTick5计数差静默，错误/溢出丢损坏段；
  发送超时count*2+20 ms，中止成功释放缓冲、等待5计数差再发，中止失败保留BUSY。
  USART1/2原生user区分发adapter，跨两个user区用#if 0屏蔽生成示例避免重复读及
  不可达警告；wk_usart2的user区关闭中断直到BSP绑定。再生成后核对保留。
  svc_comm_init初始化两路并返回首个错误，各通道保存init_status独立运行；
  main Watch为g_comm_init_status，分路看g_comm_channels[0/1].init_status，
  统计g_rs485_stats/g_uart2_stats。两路目前仅03/0F协议应答，没有调试日志复用。
  .agents/scripts/test-comm-uarts.ps1模拟真实adapter/BSP/kfifo/Service/协议，验证
  双路交错发送/应答原路返回/缓冲隔离、UART2全双工接收、溢出/错误恢复、计数回绕、
  超时中止后重发；通过。MDK0 error/0 warning，Code28452/RO448/RW404/ZI3932，
  代理未烧录；2026-11-02用户确认UART1（RS485）和UART2（普通串口）均可通信，
  暂定当前双串口协议版本为“初步可用”基线，先保留现有实现。
  用户未补充具体测试帧、并发压力或时长；异常注入、长期/双路连续通信、波形及
  优先级0下采样影响不据此视为已验收。

- 当前充电说明：99_Doc/charge-state-split-2026-10-23.md。
- 采样换算：99_Doc/bsp_measure.md；分层：99_Doc/architecture.md。
- 删除已替代的旧试验/重复说明，实测硬件参数收拢至本文；历史CV证据：
  99_Doc/mppt-cv-950-investigation-2026-10-17.md（按当时版本理解）。
