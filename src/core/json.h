// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model.h"
#include <cmath>
#include <limits>
namespace chosuta {
// Validate before Qt conversions, which otherwise silently replace malformed values.
inline QJsonObject jsonFields(const QJsonValue &value,const QString &path,
    std::initializer_list<const char *> strings={},std::initializer_list<const char *> numbers={},
    std::initializer_list<const char *> booleans={},std::initializer_list<const char *> objects={},
    std::initializer_list<const char *> arrays={},std::initializer_list<const char *> integers={}) {
    if(!value.isObject())throw Failure(Message::format("%1: expected object",{path}));
    const auto o=value.toObject();
    auto fields=[&](auto keys,QJsonValue::Type type){for(const auto key:keys)
        if(o.value(QLatin1String(key)).type()!=type)throw Failure(Message::format("%1: missing or invalid type",{path+"."+QLatin1String(key)}));};
    fields(strings,QJsonValue::String);fields(numbers,QJsonValue::Double);fields(booleans,QJsonValue::Bool);
    fields(objects,QJsonValue::Object);fields(arrays,QJsonValue::Array);fields(integers,QJsonValue::Double);
    for(const auto key:integers){const double n=o.value(QLatin1String(key)).toDouble();
        if(!std::isfinite(n)||n!=std::floor(n)||n<std::numeric_limits<int>::min()||n>std::numeric_limits<int>::max())throw Failure(Message::format("%1: expected integer",{path+"."+QLatin1String(key)}));}
    return o;
}
inline void jsonString(const QJsonValue &v,const QString &path){if(!v.isString())throw Failure(Message::format("%1: expected string",{path}));}
}
