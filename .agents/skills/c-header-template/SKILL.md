---
name: c-header-template
description: 按用户的 Keil C/H 模板新建自研 C 源文件和头文件，统一文件头、分区、保护宏及四空格缩进。适用于明确要求此模板或项目 AGENTS.md 约定使用此模板的工程；普通修改不批量重排现有文件，不改厂商生成格式。
---

# C/H 文件模板

创建新自研 `.c` / `.h` 时，读取并使用本 skill 的对应模板：
- [C 模板](assets/File.c.template)
- [头文件模板](assets/File.h.template)

## 模板规则
- 保留文件头的 `@file`、`@brief`、`@version`、`@note` 排列、分隔线及分区顺序；缩进使用 4 个空格，不插入 tab。
- `{{FILE_NAME}}` 替换为带扩展名的实际文件名；`{{FILE_STEM}}` 为不含扩展名的文件名。例如 `plat_adc.c` 包含 `"plat_adc.h"`，禁止生成 `.c.h`。
- `{{BRIEF}}` 写实际职责，不保留通用占位描述，不把业务或算法文件误称为 HAL。
- `{{HEADER_GUARD}}` 使用实际文件名转大写、非字母数字替换为下划线，必要时加模块前缀避免冲突；若结果以数字或保留标识符形式开头，补安全的模块前缀，不生成双下划线或下划线加大写开头的保护宏。例：`plat_adc.h` -> `PLAT_ADC_H`。生成完整 `#ifndef / #define / #endif`。
- 版本默认 V1.0；不自动添加作者、日期、许可证或未经确认的项目信息。
- `.h` 的 includes 分区置于 extern "C" 块之前；C++ 兼容块包住本文件的类型和公开声明，不包住所包含的头文件。头文件应能独立包含，不依赖调用方预先 include。保留 stdint/stddef 包含（不添加 lib header file 行尾注释）与 define/typedef/variables/function 分区；variables 只放确需导出的 extern 声明，不定义存储。
- include 按 C 标准库、厂商及厂商生成头文件、项目自有头文件的顺序分组，组间不插入空行；项目自有组内优先放同名头文件。没有对应头文件时，不制造不存在的 include，也不强制创建空头文件。
- 保留空分区注释，不放示例变量、空函数或无用宏。私有函数/状态按需 static，公共声明与实现一致。
- 此 skill 只约束文件布局，不授权改变分层、接口、硬件绑定、算法或构建行为。
- 已有文件只按请求修改，不为套模板整体重排。厂商库、生成代码、第三方文件和许可证保留原格式。

- 保护宏的 `#define` 行仅保留宏名，不追加 `/*end of 文件名*/` 等文件结束注释；文件结束注释只放在文件末尾。

- `@brief` 需要换行时，续行保留注释星号，并将正文对齐到上一行说明文字的起始列，不紧贴星号；仍遵循 80 列目标。格式化时保留这一文件头布局，必要时仅格式化文件头以后的代码范围。

- 分区标题统一小写，包括 includes、private functions、exported functions 及 define to prevent recursive inclusion；保留分隔线和分区顺序。
- include 行不生成解释头文件名称的模板注释，例如 stdint lib header file、stddef lib header file。

## 完成检查
确认没有未替换占位符、保护宏闭合、同名 include 正确、分区顺序正确。实际 C/H 加入工程后按工程规则编译；模板资源本身不加入 MDK。
