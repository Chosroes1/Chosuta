// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtWidgets>
#include <functional>
namespace chosuta {
// Local file URLs only. Receiver decides which file types and actions are valid.
class FileDropFilter final:public QObject {
    std::function<void(QStringList)> receive;
public:
    FileDropFilter(QWidget *target,std::function<void(QStringList)> callback):QObject(target),receive(std::move(callback)) {
        target->setAcceptDrops(true);target->installEventFilter(this);
    }
    bool eventFilter(QObject *,QEvent *event)override {
        if(event->type()!=QEvent::DragEnter&&event->type()!=QEvent::Drop)return false;
        auto drop=static_cast<QDropEvent *>(event);QStringList paths;
        for(const auto &url:drop->mimeData()->urls()){
            if(!url.isLocalFile()){drop->ignore();return true;}
            paths.append(url.toLocalFile());
        }
        if(paths.isEmpty()){drop->ignore();return true;}
        drop->acceptProposedAction();if(event->type()==QEvent::Drop)receive(paths);return true;
    }
};
}
