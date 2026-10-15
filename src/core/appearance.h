// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model.h"
#include "intervals.h"
#include "performance.h"
#include <memory>
namespace chosuta {
    struct AppearanceContext {QString group="base",anchor;QVector<Message> intervalReasons;};
    struct AssetSelection {
        QString shape,id;
        AppearanceContext context;
        QStringList candidates;
        bool missing=false,fixed=false;
    };
    const QStringList &mouthStates();
    std::optional<QString> normalizedAssetId(const QString &name);
    class AppearanceResolver {
        std::shared_ptr<const PerformanceResolver> performance;
        public:
        AppearanceResolver()=default;
        explicit AppearanceResolver(const Project &);
        AppearanceResolver(const Project &,std::shared_ptr<const PerformanceResolver>);
        AssetSelection event(const Project &,const Event &,const QSet<QString> *available=nullptr)const;
        AssetSelection at(const Project &,const QVector<Event> &,double seconds,bool disjoint=false,const QSet<QString> *available=nullptr)const;
        AssetSelection select(const Project &,QString shape,AppearanceContext context={},bool fixed=false,const QSet<QString> *available=nullptr)const;
        const PerformanceResolver *plan()const{return performance.get();}
    };
    struct AssetImportEntry {QString path,id,status;Message message;QString group;};
    struct AssetImportPlan {QVector<AssetImportEntry> entries;bool cancelled=false;Message error;QVector<VariantGroup> groups;};
    void applyAssetImport(Project &,const QVector<AssetImportEntry> &);
    void configureImageReaderLimit();
    AssetImportPlan scanAssetFiles(const QStringList &,const QMap<QString,QString> &,const std::atomic_bool *cancel=nullptr,Progress progress={});
    AssetImportPlan scanAssetDirectory(const QString &,const QMap<QString,QString> &,const std::atomic_bool *cancel=nullptr,Progress progress={});
    AssetImportPlan scanVariantDirectory(const QString &,const Project &,const QString &singleGroup,const std::atomic_bool *cancel=nullptr,Progress progress={});
}
