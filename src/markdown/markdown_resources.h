#ifndef FILE_MARKDOWN_MARKDOWN_RESOURCES_H_
#define FILE_MARKDOWN_MARKDOWN_RESOURCES_H_

// qrc（markdown_res）的初始化入口。静态库里的 qrc 不会自动初始化，凡是要
// 读载荷（highlight.js / MathJax / mermaid / 默认 CSS）的入口都得先调一次。
// 幂等，重复调用无副作用
void EnsureMarkdownResources();

#endif  // FILE_MARKDOWN_MARKDOWN_RESOURCES_H_
