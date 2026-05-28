/*
 * 文件: src/app_ui/ui_main.h
 * 作用: 该文件是 app_ui 对外入口的一部分，负责 UI 子系统初始化、对外导出接口或与应用层的集成连接。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时建议先理解该文件在 app_ui 流程中的位置，再修改注释所描述的职责边界，避免把数据准备、状态切换和视图渲染职责混杂到一起。
 */

#ifndef AREX_UI_MAIN_H
#define AREX_UI_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif



/* app_ui 对外只暴露一个主入口，外部模块不需要感知内部 screen/ui_state/ui_engine 的装配细节。 */
void UI_main(void);

#ifdef __cplusplus
}
#endif

#endif /* AREX_UI_MAIN_H */
