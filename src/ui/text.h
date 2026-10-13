// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtWidgets>
#include "core/message.h"
namespace chosuta {
QString uiText(const QString &,const char *,const char *zh=nullptr,const char *ja=nullptr);
struct ActionText {const char *key,*zh=nullptr,*ja=nullptr;QString text(const QString &language)const{return uiText(language,key,zh,ja);}};
inline ActionText actionText(const char *key,const char *zh=nullptr,const char *ja=nullptr){return {key,zh,ja};}
class TranslatedCommand:public QUndoCommand {
    ActionText label;
public:
    TranslatedCommand(ActionText value,const QString &language):QUndoCommand(value.text(language)),label(value){}
    void retranslate(const QString &language){setText(label.text(language));}
};
QString messageText(const QString &,const Message &,bool details=true);
QString messageDetails(const QString &,const Message &);
QString messagesText(const QString &,const QVector<Message> &);
QString widgetLanguage();
void setWidgetLanguage(const QString &);
QString mouthLabel(const QString &language,const QString &id);
QString selectedId(const QComboBox *);
void selectId(QComboBox *,const QString &);
}
