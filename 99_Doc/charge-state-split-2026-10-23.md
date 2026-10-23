# MPPT / CC / CV状态与模块拆分（2026-10-23）

## 当前检查点：尚未完成上板验证

以下正文记录最初的结构拆分，后续变化以本节及工程memory为准：

- MPPT恢复新增REACQUIRE=4，重新建立功率基准并验证邻点方向，不沿用
  CC/CV接管前的方向；主状态优先级仍为保护、CV、CC、MPPT。
- FULL_WAIT主状态并入MAINTENANCE=8，数值7空置；充满电池电流窗口
  为-500～500 mA，连续10 s。12 V CV目标14.4 V，维护目标14.1 V。
  进入维护先关PWM等待回落，再启动恒压；运行中普通电压偏差由CV调D，
  不再用目标加20 mV直接停机，避免周期性关断/重新启动。
- 维护净放电>=200 mA连续5 s请求正常充电，不再要求电池低于13.8 V，
  不再借输入电压故障退出。运行中保持D重新选择CC/MPPT，CV仍优先；
  原已停PWM时转WAIT_SAMPLE执行启动检查。真实保护继续独立执行。
- PB0短路硬件错误，用户要求临时屏蔽：BSP测试旁路保持1；保留用户在
  EXINT1_0_IRQHandler中注释短路回调分发的改动。恢复时需同时核对这
  两处，不能只恢复BSP宏。当前版本没有该软件短路关断。
- 最新MDK编译0 error、0 warning；状态/MPPT趋势/LED主机测试通过。
  Code22320/RO440/RW276/ZI2588。最新净放电交接未上板测试，不标为
  完成硬件验收。慢升压收敛、固定CV上限下掉电保护覆盖及50 ms低输出
  判据与轻载维护冲突仍待处理。

## 范围

以7d380ba加启动快速搜索为基础。用户要求三者分开、MPPT有明确状态，
本轮只拆分模块及交接表达，保留原固定步长CC/CV、P&O及启动5‰搜索。
没有PI、1 A独立CC、CV确认计时、负载变化触发重扫或新保护阈值。

## 主状态与MPPT阶段

g_charge_state：旧0～8值保留，4现在只表示CC，5表示CV，新增9表示MPPT。
START_SETTLE结束后按电流选择MPPT或CC；CC释放回MPPT；CV退出按当前
电流选择MPPT或CC。交接不重算启动D、不关PWM，也不跳占空比。

g_charge_mppt_phase及svc_charge_mppt_get_phase：

| 值 | 阶段 | 含义 |
| --- | --- | --- |
| 0 | OFF | 初始化、停止或故障后未运行 |
| 1 | STARTUP | 一次性5‰启动快速搜索 |
| 2 | TRACKING | 原1‰精细跟踪，包含等待/测量窗口 |
| 3 | PAUSED | CC/CV接管，禁止继续功率比较 |

CC/CV交回MPPT时，resume在当前实际D丢弃旧比较，重建精细跟踪基准；
不重新武装快速搜索。只有正常启动显式调用startup_begin。
旧g_charge_current_limited保留为兼容Watch量，不能再修改它选择算法。
主状态是控制选择依据；状态发布点由原隐含flag改为显式状态，启动/CV
退出时即发布下一控制状态，算法执行仍在新采样周期内完成。

## 职责

- svc_charge.c：采样/保护、主状态与交接、统一BSP功率操作。MPPT和CC
  有独立入口process_mppt/process_constant_current，复用采样、保护及
  输出处理；CV保留独立流程和入口D上限试验。新鲜采样只消费一次。
- svc_charge_mppt.c/h：原功率扰动计算、启动快速搜索与显式阶段管理，
  不写PWM。
- 新svc_charge_cc.c/h：从svc_charge_control原样搬出的恒流计算。
- 新svc_charge_cv.c/h：从svc_charge_control原样搬出的恒压/电流约束计算。
- svc_charge_control.c/h：初始D、输入可用性和共有结果类型。
- svc_charge_maintenance.c：继续调用同一CV算法，维护策略不变。
- svc_led.c：把独立MPPT也识别为充电活动状态。

CC/CV函数签名与计算体保留，声明移入各自头文件。模块不互相调度，
公开接口不引入厂商类型。MDK只新增两个C及其头文件，无硬件配置变化。

## 条件保持

12 V当前配置：3980 mA接管CC，3900 mA释放；14.38 V接管CV，
14.30 V退出CV。CV仍保留入口D上限、原充满/维护和输入保护行为。
这些是代码条件，不是新增硬件验收结论；原CV上限保护衔接待办仍保留。
CC/CV交接先后条件本轮不调优，中途加负载效果还需要上板观察。

## 验证

- CC/CV计算函数与拆分前源码去掉空白后逐字符相同，无计算逻辑变更。
- 新test-charge-state-split.ps1直接运行真实Service/算法/保护；覆盖
  MPPT/CC阈值/迟滞、CV优先及退出直接选择CC/MPPT、同D恢复精细跟踪、
  旧CV上限保留、故障/stop复位MPPT阶段。测量和PWM仅为主机stub。
- 可选BaselineDirectory对照拆分前保存的三份源文件。12000个混合任务
  快照中，把新MPPT9映射回旧4后，D、state类别、故障/status、误差、
  上限量、PWM启停/更新次数、MPPT功率/方向/计数及充满计时相同。
  不将此有限序列当作所有输入/所有状态时序等价或上板验证。
- MPPT趋势/启动/阶段、LED全状态（含9）测试通过；git diff --check通过。
- 新C/H使用项目c-header-template；本次12个C/H按格式脚本处理。
- Git Bash运行bash ./keil_build_flash.bash -b：0 error、0 warning；
  Code22372/RO440/RW272/ZI2584，RAM2856 B。未烧录/提交/推送。
