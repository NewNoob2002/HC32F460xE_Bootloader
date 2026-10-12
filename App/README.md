# App 产品维护入口

| 产品 | 独立配置 | OLED / LVGL |
|---|---|---|
| X1 | [Products/X1/product.cmake](Products/X1/product.cmake) | 关闭 / 关闭 |
| X1Pro | [Products/X1Pro/product.cmake](Products/X1Pro/product.cmake) | 开启 / 开启 |
| XBase | [Products/XBase/product.cmake](Products/XBase/product.cmake) | 继承 X1Pro，按本产品需求增减 |

`app::X1`、`app::X1Pro`、`app::XBase` 是独立产品配置目标，目前未生成 App 固件。共用开关机、LED 和充电业务接入 `App/Shared/`；X1Pro/XBase 共用显示业务接入 `App/Shared/Display/`；产品专属代码放在各自 `Products/<产品>/`，共用 BSP 和协议只维护一份。

完整构建边界、链接地址与接入要求见 [工程结构](../docs/project_structure.md)。Boot 固定 32 KiB，App 从 `0x00008000` 开始。

App 使用 [Linker/HC32F460_app.ld](Linker/HC32F460_app.ld)，三个产品共用 456 KiB 的链接区域。实际 App 可执行 target 链接 `app::<产品>` 和 `app::image`，后者自动接入 App 链接脚本、平台源码和 VTOR 偏移；主机测试只链接产品配置。Boot 使用自己的链接脚本，App 禁止携带 ICG。
