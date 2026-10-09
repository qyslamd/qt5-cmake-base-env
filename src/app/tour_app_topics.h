#ifndef FILE_APP_TOUR_APP_FLOWS_H_
#define FILE_APP_TOUR_APP_FLOWS_H_

class QMainWindow;
class TourTopic;

// 应用侧的主题定义。tour/ 的其余部分是引擎（配置 / 控制器 / 演示层），
// 它不认识具体业务；引导讲什么由使用方在这里写，改主题不必动引擎。
//
// 返回的主题归调用方（实际归 TourController::addTopic）所有。
TourTopic* BuildTemplateTourTopic(QMainWindow* window);

#endif  // FILE_APP_TOUR_APP_FLOWS_H_
