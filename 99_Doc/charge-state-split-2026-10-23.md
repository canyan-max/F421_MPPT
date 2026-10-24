# 充电状态与模块说明

更新时间：2026-10-24。当前为开发试验版，主机验证不等于上板通过。

## 默认流程

- 主流程统一处理采样、保护、状态交接和PWM；优先级为保护>CV>CC>MPPT。
- 启动：有效采样准备完成后，按Vbat/Vin计算初始D，稳定后执行一次启动
  快速搜索；正常MPPT使用原细步跟踪。CC/CV交回MPPT重新确认搜索方向。
- CV确认充满只锁存标志，有负载时继续供电。无负载连续确认后，关PWM
  进入充满待机，继续采样，不调用维护恒压或维护建流。
- 待机出现负载，或电池达到回充条件，且输入/保护条件允许时，复用正常
  启动重新计算D。输入缺失时继续等待，不仅因此报告运行输入故障。

| 主状态值 | 含义 |
| --- | --- |
| 0 / 1 / 2 | OFF / WAIT_SAMPLE / START_SETTLE |
| 3 / 4 / 5 / 6 | WAKE_CV / CC / CV / FAULT |
| 7 | FULL_WAIT，充满待机，PWM关闭 |
| 8 | MAINTENANCE，仅维护开关开启后使用 |
| 9 | MPPT |

## 模块边界

- svc_charge：状态交接、采样/保护调用、PWM输出；停充等待共用一个入口。
- svc_charge_control：初始D和输入可启动条件。
- svc_charge_cc、svc_charge_cv、svc_charge_mppt：各自算法，不直接操作PWM。
- svc_charge_protection：故障条件、确认及恢复；独立电池过压检查覆盖运行。
- svc_charge_maintenance：复用的充满/负载判断，以及保留的可选维护算法。
- svc_charge_config：参数及维护接入开关。

## 参数与保留维护

- 当前12 V CV目标14.4 V，过压14.6 V；24 V目标28.8 V，过压29.2 V。
  过压达到阈值触发，与CV目标独立；以源码实际配置为准。
- 负载ON为100 mA、OFF为80 mA，无负载确认1 s；待机负载确认约20 ms。
  充满电池净电流窗口仍为-500～500 mA/10 s，属于待评估的试验判据。
- 待机电压回充为13.8 V/27.6 V，连续5 s；当前暂复用maintenance配置。
- SVC_CHARGE_ENABLE_MAINTENANCE位于svc_charge_config.h，默认0。改1可
  重新接入原维护状态8；14.1 V维护、BUILD/RUNNING/WAIT及相关保护代码
  均保留。不能把默认停用误读为维护路径已经通过上板验证。
- 电流语义、全部保护/恢复参数及待办以.agents/project-memory.md为准。

## 验证和限制

- 按用户要求，12 V DMA帧级快速路径只判断BAT_VOL原值>=1760并调用
  plat_pwm_outputs_off()关PWM；继续采样，不关闭输入开关，不锁存事件，
  不增加上层恢复判断。24 V禁用该试验门限，原10 ms软件保护保留。
- Watch使用g_charge_fast_ovp_trip_raw与g_charge_fast_ovp_off_status；
  旧复杂结构g_charge_fast_ovp_state已删除。普通主流程后续启动不被该
  路径互锁；本版仅用于验证原值触发关PWM，过冲/关断延迟待测。
- 本轮仅MDK编译通过；直接关PWM测试脚本已同步简化，尚未执行。
- test-charge-state-split.ps1分别构建默认模式和维护模式，覆盖充满带载、
  待机/维护交接、过压边界、故障和手动停止等行为。
- test-mppt-trend.ps1验证启动、趋势及恢复方向；test-led.ps1验证指示灯。
- 本次清理前后两种模式各24000个混合输入快照的状态、D、故障、PWM动作
  和关键计数一致；仅适用于该输入序列，不证明全部硬件行为等价。
- 删除了失效的旧BaselineDirectory/TRACE_RUN对照入口；当前脚本只验证
  当前策略和保留维护路径，不再与已撤销算法比较。
- 慢升压收敛、CV入口D上限下的掉电保护覆盖、长期满电带载电压以及
  实际PV/24 V/大电流仍待验证。PB0短路检测仍按用户要求屏蔽。
- 历史CV事件证据保留于mppt-cv-950-investigation-2026-10-17.md及其图片
  目录；本说明不再重复历史试验过程或已撤销接口。
