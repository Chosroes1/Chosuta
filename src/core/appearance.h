// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model.h"
namespace chosuta {
    struct AppearanceContext {
        QString direction="any",interval="any",beat="any",anchor;
        QString intervalReason={};
    };
    struct AssetSelection {
        QString shape,id;
        AppearanceContext context;
        QStringList candidates;
        bool missing=false,fixed=false;
    };
    void validateAppearance(const Project::Appearance &);
    QJsonObject appearanceJson(const Project::Appearance &);
    Project::Appearance readAppearance(const QJsonValue &);
    QString appearanceId(const QString &shape,const AppearanceContext &);
    std::optional<QString> normalizedAssetId(const QString &name);
    class AppearanceResolver {
        QHash<QString,AppearanceContext> contexts;
        QHash<QString,qint64> ends;
        public:
        AppearanceResolver()=default;
        explicit AppearanceResolver(const Project &);
        AppearanceContext context(const QString &source)const {return contexts.value(source);}
        AssetSelection event(const Project &,const Event &,const QSet<QString> *available=nullptr)const;
        AssetSelection at(const Project &,const QVector<Event> &,double seconds,bool disjoint=false,const QSet<QString> *available=nullptr)const;
        AssetSelection select(const Project &,QString shape,AppearanceContext context,bool fixed=false,const QSet<QString> *available=nullptr)const;
    };
    struct AssetImportEntry {QString path,id,status,message;};
    struct AssetImportPlan {QVector<AssetImportEntry> entries;bool cancelled=false;QString error;};
    void configureImageReaderLimit();
    AssetImportPlan scanAssetDirectory(const QString &,const QMap<QString,QString> &,const std::atomic_bool *cancel=nullptr,Progress progress={});
}
