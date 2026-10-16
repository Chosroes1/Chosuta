// SPDX-License-Identifier: GPL-3.0-or-later
#include "source.h"
#include "zip.h"
#include <QXmlStreamReader>
namespace chosuta {
namespace {
[[noreturn]] void invalid(const QString &field){throw Failure(Message::format("MXL container: invalid or unsupported field %1",{field}));}
}
Score parseMxl(const QByteArray &bytes,const QString &path,const std::atomic_bool *cancel,Progress progress){
    auto check=[&]{if(cancel&&cancel->load())throw Failure("Cancelled");};check();
    ZipArchive archive(bytes,cancel);if(progress)progress(5);
    const auto manifest=archive.read("META-INF/container.xml",64*1024*1024,[&](int n){if(progress)progress(5+n*5/100);});
    QXmlStreamReader xml(manifest);QString rootPath;int roots=0;bool rootfiles=false;
    auto next=[&]{check();return xml.readNextStartElement();};
    auto skip=[&]{int depth=1;while(depth&&!xml.atEnd()){check();const auto token=xml.readNext();if(token==QXmlStreamReader::StartElement)++depth;else if(token==QXmlStreamReader::EndElement)--depth;}};
    if(!next()||xml.name()!=QLatin1String("container"))invalid("container");
    while(next()){
        if(xml.name()!=QLatin1String("rootfiles")){skip();continue;}
        if(rootfiles)invalid("duplicate rootfiles");
        rootfiles=true;
        while(next()){
            if(xml.name()!=QLatin1String("rootfile")){skip();continue;}
            if(++roots==1){
                rootPath=xml.attributes().value("full-path").toString();
                const auto type=xml.attributes().value("media-type").toString();
                if(rootPath.isEmpty())invalid("rootfile[1].full-path");
                if(!type.isEmpty()&&type!="application/vnd.recordare.musicxml+xml")invalid("rootfile[1].media-type");
            }
            // A rootfile is empty; nested rootfiles must not change selection.
            if(next())invalid("rootfile content");
        }
    }
    while(!xml.atEnd()){check();xml.readNext();}
    if(xml.hasError())throw Failure(Message::format("Cannot read MXL container at line %1, column %2",{QString::number(xml.lineNumber()),QString::number(xml.columnNumber())}).withDetail(xml.errorString()));
    if(!rootfiles||!roots)invalid("rootfiles/rootfile");
    rootPath.replace(u'\\',u'/');
    const auto cleaned=QDir::cleanPath(rootPath);
    if(cleaned=="."||cleaned==".."||cleaned.startsWith("../")||cleaned.startsWith('/'))invalid("rootfile[1].full-path (relative to ZIP root)");
    // Full-path is an archive path, not a URI. Do not URL-decode percent signs
    // or resolve against the directory of META-INF/container.xml.
    if(!archive.contains(rootPath))rootPath=cleaned;
    const auto scoreBytes=archive.read(rootPath,64*1024*1024,[&](int n){if(progress)progress(10+n*20/100);});
    Score score;
    try{
        score=parseMusicXml(scoreBytes,path,cancel,[&](int n){if(progress)progress(30+n*69/100);});
    }catch(const Failure &failure){
        auto message=failure.message;
        message.detail=QString("MXL member: %1").arg(rootPath)+(message.detail.isEmpty()?QString():"\n"+message.detail);
        throw Failure(message);
    }
    for(auto &d:score.diagnostics)d.path="MXL/"+rootPath+"/"+d.path;
    score.diagnostics.append({"META-INF/container.xml","mxl-root",Message::format("MXL imports its first declared MusicXML root: %1",{rootPath})});
    auto remaining=archive.names();remaining.removeAll("META-INF/container.xml");remaining.removeAll(rootPath);remaining.removeAll("mimetype");
    if(roots>1||!remaining.isEmpty())score.diagnostics.append({"MXL","retained-members","MXL alternate roots and other archive members are retained in the source; only the first score is imported"});
    if(bytes.size()>48*1024*1024)score.diagnostics.append({"MXL","project-size","The retained MXL archive may exceed the existing 64 MiB project save limit"});
    check();return score;
}
}
