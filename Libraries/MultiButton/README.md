# MultiButton（X1 复用版本）

复用旧工程 `HC32F460xE_Arduino` 的 `X1_Panel_dev` 提交
`771d4ea0671ebd7069be20a816d6b890dbd98b5c` 中的 `Libraries/MultiButton/`，
保留 MIT 许可证。该版本提供消抖、单击、双击、重复按下、长按开始和长按保持事件，
使用静态 Button 对象，无动态分配。

本地变化为工程格式化、非保留命名的头文件保护/结构体标签，以及
`DEBOUNCE_TICKS=4`（5 ms周期下标称20 ms）；公开Button类型与API保持原样。
产品事件接线位于 `App/Shared/Src/app_keys.cpp`，不在库内执行电源业务。
按键处理全部在主循环，`button_ticks()` 最多每 5 ms 调用一次；主循环迟到会延后
库的消抖/单双击窗口，不对同一 GPIO 样本补跑 tick。2 s/8 s 产品长按使用实际
毫秒差并只触发一次，避免重复 HOLD 回调累加和库计数周期造成的旧版问题。
实板仍需验证 I2C/日志负载下的最长轮询间隔与按键手感。
