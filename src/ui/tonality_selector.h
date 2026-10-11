// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtWidgets>
#include "core/intervals.h"
#include "core/performance.h"
namespace chosuta {
class TonalitySelector : public QWidget {
public:
    using Text=std::function<QString(const char *,const char *,const char *)>;
    QComboBox *state,*tonic,*mode;
    QLabel *scale;
    QVector<int> custom;
    std::function<void()> refreshScale;
    void setCustomScale(QVector<int> values){custom=std::move(values);if(refreshScale)refreshScale();}
    TonalitySelector(const Project::Appearance &a,bool region,Text text,QWidget *parent=nullptr):QWidget(parent),custom(a.customDegreeScale){
        auto form=new QFormLayout(this);form->setContentsMargins(0,0,0,0);
        state=new QComboBox;state->setObjectName("tonalityState");if(region)state->addItem(text("Inherit global","继承全局","全体を継承"),"inherit");
        state->addItem(text("Unspecified / disabled","未指定／禁用","未指定・無効"),"disabled");state->addItem(text("Use tonality","使用调性辅助","調性を使用"),"specified");
        state->setCurrentIndex(state->findData(region?"inherit":a.tonalityEnabled?"specified":"disabled"));form->addRow(text("Tonality","调性","調性"),state);
        tonic=new QComboBox;tonic->setObjectName("tonalityTonic");
        for(int count=0;count<=3;++count)for(QChar letter:QString("CDEFGAB"))for(const QString sign:count?QStringList{"b","#"}:QStringList{""}){QString value=QString(letter)+QString(count,sign.isEmpty()?' ':sign[0]);QString label=value;label.replace('#',QChar(0x266f)).replace('b',QChar(0x266d));if(count)label+=" ("+text(sign=="b"?"flat":"sharp",sign=="b"?"降":"升",sign=="b"?"フラット":"シャープ")+(count>1?QString::number(count):QString{})+")";tonic->addItem(label,value);}
        tonic->setCurrentIndex(tonic->findData(a.degreeTonic));form->addRow(text("Tonic","主音","主音"),tonic);
        mode=new QComboBox;mode->setObjectName("tonalityMode");
        const QStringList ids={"major","minor","dorian","phrygian","lydian","mixolydian","locrian","harmonic-minor","melodic-minor","harmonic-major","custom"};
        const QStringList labels={text("Major (Ionian)","大调（Ionian）","長調（Ionian）"),text("Natural minor (Aeolian)","自然小调（Aeolian）","自然短音階（Aeolian）"),"Dorian","Phrygian","Lydian","Mixolydian","Locrian",text("Harmonic minor","和声小调","和声短音階"),text("Melodic minor (ascending scale)","旋律小调（上行音阶）","旋律短音階（上行）"),text("Harmonic major","和声大调","和声長音階"),text("Custom seven notes","自定义七音","独自7音")};
        for(int i=0;i<ids.size();++i)mode->addItem(labels[i],ids[i]);mode->setCurrentIndex(mode->findData(a.degreeMode));form->addRow(text("Mode","调式","旋法"),mode);
        scale=new QLabel;scale->setWordWrap(true);form->addRow(text("Scale","音阶","音階"),scale);
        auto update=[this]{bool enabled=state->currentData()=="specified";tonic->setEnabled(enabled);mode->setEnabled(enabled);Project::Appearance a;a.degreeTonic=tonic->currentData().toString();a.degreeMode=mode->currentData().toString();a.customDegreeScale=custom;auto root=parseWrittenPitch(a.degreeTonic+"4");QStringList names;auto offsets=a.degreeMode=="custom"?custom:degreeModes().value(a.degreeMode);if(root)for(int offset:offsets){Note n;n.pitch=root->midi+offset;if(auto w=writtenPitch(a,n)){QString name=w->name;name.remove(QRegularExpression("-?[0-9]+$"));names<<name.replace('#',QChar(0x266f)).replace('b',QChar(0x266d));}}scale->setText(enabled?names.join("  "):QString{});};
        refreshScale=update;connect(state,&QComboBox::currentIndexChanged,this,update);connect(tonic,&QComboBox::currentIndexChanged,this,update);connect(mode,&QComboBox::currentIndexChanged,this,update);update();
    }
    QString key()const{return state->currentData()=="inherit"?"inherit":state->currentData()=="specified"?tonic->currentData().toString()+" "+mode->currentData().toString():QString{};}
    void apply(Project::Appearance &a)const{a.tonalityEnabled=state->currentData()=="specified";a.degreeTonic=tonic->currentData().toString();a.degreeMode=mode->currentData().toString();}
};
}
