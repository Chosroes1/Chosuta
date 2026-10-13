// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtCore>
#include <stdexcept>
namespace chosuta {
inline QString formatMessage(const QString &pattern,const QStringList &arguments){
    static const QRegularExpression placeholder("%([1-9][0-9]*)");
    QString result;qsizetype offset=0;auto matches=placeholder.globalMatch(pattern);
    while(matches.hasNext()){auto m=matches.next();result+=pattern.mid(offset,m.capturedStart()-offset);int index=m.captured(1).toInt()-1;result+=index>=0&&index<arguments.size()?arguments[index]:m.captured();offset=m.capturedEnd();}
    return result+pattern.mid(offset);
}
// Source templates are stable keys; parameters and external details are data.
// Core/CLI render English. The GUI translates the key before substituting data.
struct Message {
    QString key;
    QStringList arguments;
    QString detail;
    Message()=default;
    Message(const char *source):key(QString::fromUtf8(source)){}
    Message(const QString &raw):detail(raw){}
    static Message format(const char *source,QStringList arguments){Message m(source);m.arguments=std::move(arguments);return m;}
    Message withDetail(QString value)const{auto copy=*this;copy.detail=std::move(value);return copy;}
    QString text()const{QString s=formatMessage(key,arguments);return s.isEmpty()?detail:detail.isEmpty()?s:s+"\n"+detail;}
    bool isEmpty()const{return key.isEmpty()&&detail.isEmpty();}
    void clear(){key.clear();arguments.clear();detail.clear();}
    bool operator==(const Message &)const=default;
};
inline QStringList messageStrings(const QVector<Message> &messages){QStringList result;for(const auto &m:messages)result.append(m.text());return result;}
struct Failure:std::runtime_error {
    Message message;
    explicit Failure(Message value):std::runtime_error(value.text().toStdString()),message(std::move(value)){}
};
inline Message exceptionMessage(const std::exception &e){if(auto failure=dynamic_cast<const Failure *>(&e))return failure->message;return Message(QString::fromUtf8(e.what()));}
}
