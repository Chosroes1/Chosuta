// SPDX-License-Identifier: GPL-3.0-or-later
#include "text.h"
namespace chosuta {
namespace {
// Original, compiled-in translations for Qt's standard controls. No external
// translation package is needed; native OS dialogs still follow the OS locale.
class WidgetTranslator final:public QTranslator {
public:
    QString language;
    explicit WidgetTranslator(QObject *parent):QTranslator(parent){}
    bool isEmpty()const override{return false;}
    QString translate(const char *context,const char *source,const char *,int)const override {
        if(language=="en"||!QString::fromLatin1(context).startsWith('Q'))return {};
        struct Entry{const char *en,*zh,*ja;};
        static const Entry entries[]={
            {"OK","确定","OK"},{"Cancel","取消","キャンセル"},{"Save","保存","保存"},{"Save All","全部保存","すべて保存"},
            {"Open","打开","開く"},{"Yes","是","はい"},{"Yes to All","全部选是","すべてはい"},{"No","否","いいえ"},{"No to All","全部选否","すべていいえ"},
            {"Abort","中止","中止"},{"Retry","重试","再試行"},{"Ignore","忽略","無視"},{"Close","关闭","閉じる"},
            {"Discard","不保存","保存しない"},{"Apply","应用","適用"},{"Reset","重置","リセット"},{"Restore Defaults","恢复默认","既定に戻す"},{"Help","帮助","ヘルプ"},
            {"Show Details...","显示详细信息…","詳細を表示…"},{"Hide Details...","隐藏详细信息…","詳細を隠す…"},
            {"Undo","撤销","元に戻す"},{"Redo","重做","やり直す"},{"Cut","剪切","切り取り"},{"Copy","复制","コピー"},{"Paste","粘贴","貼り付け"},{"Delete","删除","削除"},{"Select All","全选","すべて選択"},
            {"Look in:","查找位置：","場所："},{"File name:","文件名：","ファイル名："},{"Files of type:","文件类型：","ファイルの種類："},{"File type:","文件类型：","ファイルの種類："},
            {"Back","返回","戻る"},{"Parent Directory","上级目录","上のフォルダー"},{"Create New Folder","新建文件夹","新しいフォルダー"},
            {"List View","列表视图","一覧表示"},{"Detail View","详细视图","詳細表示"},{"List","列表","一覧"},{"Details","详细信息","詳細"},
            {"Show hidden files","显示隐藏文件","隠しファイルを表示"},{"New Folder","新建文件夹","新しいフォルダー"},{"Rename","重命名","名前変更"},
            {"All Files (*)","所有文件 (*)","すべてのファイル (*)"},{"Directory:","目录：","フォルダー："},{"Choose","选择","選択"},{"Find Directory","选择目录","フォルダーを選択"},
            {"Name","名称","名前"},{"Size","大小","サイズ"},{"Type","类型","種類"},{"Date Modified","修改时间","更新日時"},
            {"Folder","文件夹","フォルダー"},{"File","文件","ファイル"},{"Computer","计算机","コンピューター"},
            {"Select Color","选择颜色","色を選択"},{"Basic colors","基本颜色","基本色"},{"Custom colors","自定义颜色","カスタム色"},
            {"Add to Custom Colors","添加到自定义颜色","カスタム色に追加"},{"Pick Screen Color","拾取屏幕颜色","画面から色を選択"},
            {"Hue:","色相：","色相："},{"Sat:","饱和度：","彩度："},{"Val:","明度：","明度："},{"Red:","红：","赤："},{"Green:","绿：","緑："},{"Blue:","蓝：","青："},{"Alpha channel:","透明度：","不透明度："},{"HTML:","HTML：","HTML："},
            {"Font","字体","フォント"},{"Font style","字形","スタイル"},{"Point size","字号","サイズ"},{"Effects","效果","効果"},{"Strikeout","删除线","取り消し線"},{"Underline","下划线","下線"},{"Sample","示例","サンプル"},{"Writing System","文字系统","文字体系"},
            {"%1 already exists.\nDo you want to replace it?","%1 已存在。\n是否替换？","%1 は既に存在します。\n置き換えますか？"},
            {"%1\nFile not found.\nPlease verify the correct file name was given.","%1\n找不到文件。\n请检查文件名。","%1\nファイルが見つかりません。\nファイル名を確認してください。"}
        };
        QString key=QString::fromUtf8(source);key.remove('&');
        for(const auto &entry:entries)if(key==QString::fromUtf8(entry.en))return QString::fromUtf8(language=="zh"?entry.zh:entry.ja);
        return {};
    }
};
QPointer<WidgetTranslator> translator;
}
QString widgetLanguage(){return translator?translator->language:QString("en");}
void setWidgetLanguage(const QString &language){
    if(!translator)translator=new WidgetTranslator(qApp);
    if(translator->language==language)return;
    qApp->removeTranslator(translator);translator->language=language;qApp->installTranslator(translator);
}
QString mouthLabel(const QString &language,const QString &id){
    if(id=="closed")return uiText(language,"Closed","闭口","閉口");
    if(id=="rest")return uiText(language,"Rest","休息","休止");
    if(id=="breath")return uiText(language,"Breath","呼吸","ブレス");
    if(id=="unknown")return uiText(language,"Unknown","未知","不明");
    return id;
}
QString selectedId(const QComboBox *box){
    if(box->currentIndex()>=0&&box->currentText()==box->itemText(box->currentIndex()))return box->currentData().toString();
    return box->currentText(); // Explicitly typed custom ID, never translated.
}
void selectId(QComboBox *box,const QString &id){
    int index=box->findData(id);if(index>=0)box->setCurrentIndex(index);else if(box->isEditable())box->setEditText(id);
}
}
