// SPDX-License-Identifier: GPL-3.0-or-later
#include "zip.h"
#include <unzip.h>
#include <unicode/ucnv.h>
#include <algorithm>
#include <cstring>
namespace chosuta {
namespace {
struct Memory {
    QByteArray bytes;qint64 pos=0;bool error=false;const std::atomic_bool *cancel=nullptr;
    bool cancelled()const{return cancel&&cancel->load();}
};
voidpf ZCALLBACK openMemory(voidpf opaque,const void *,int mode){auto m=static_cast<Memory *>(opaque);m->pos=0;return (mode&ZLIB_FILEFUNC_MODE_READ)?m:nullptr;}
uLong ZCALLBACK readMemory(voidpf,voidpf stream,void *buffer,uLong length){
    auto m=static_cast<Memory *>(stream);if(m->cancelled())return 0;
    auto size=std::min<qint64>(length,m->bytes.size()-m->pos);std::memcpy(buffer,m->bytes.constData()+m->pos,size);m->pos+=size;return uLong(size);
}
uLong ZCALLBACK noWrite(voidpf,voidpf,const void *,uLong){return 0;}
ZPOS64_T ZCALLBACK tellMemory(voidpf,voidpf stream){return static_cast<Memory *>(stream)->pos;}
long ZCALLBACK seekMemory(voidpf,voidpf stream,ZPOS64_T offset,int origin){
    auto m=static_cast<Memory *>(stream);const qint64 delta=qint64(offset);
    qint64 base=origin==ZLIB_FILEFUNC_SEEK_SET?0:origin==ZLIB_FILEFUNC_SEEK_CUR?m->pos:m->bytes.size();
    if((delta<0&&delta < -base)||(delta>=0&&delta>m->bytes.size()-base)){m->error=true;return -1;}
    m->pos=base+delta;return 0;
}
int ZCALLBACK closeMemory(voidpf,voidpf){return 0;}
int ZCALLBACK memoryError(voidpf,voidpf stream){return static_cast<Memory *>(stream)->error?1:0;}
QString memberName(const QByteArray &bytes,bool utf8){
    if(bytes.contains('\0'))throw Failure("ZIP member name contains a zero byte");
    QString name;
    if(utf8){QStringDecoder decoder(QStringDecoder::Utf8);name=decoder(bytes);if(decoder.hasError())throw Failure("Invalid ZIP UTF-8 member name");}
    else {
        UErrorCode error=U_ZERO_ERROR;QByteArray out(bytes.size()*4+1,Qt::Uninitialized);
        const auto length=ucnv_convert("UTF-8","IBM437",out.data(),out.size(),bytes.constData(),bytes.size(),&error);
        if(U_FAILURE(error))throw Failure("Cannot decode ZIP member name");
        name=QString::fromUtf8(out.constData(),length);
    }
    return name.replace(u'\\',u'/');
}
}
struct ZipArchive::Impl {
    Memory memory;unzFile file=nullptr;
    struct Entry {unz64_file_pos position;unz_file_info64 info;};QMap<QString,Entry> entries;
    void check()const{if(memory.cancelled())throw Failure("Cancelled");}
    ~Impl(){if(file)unzClose(file);}
};
ZipArchive::ZipArchive(const QByteArray &bytes,const std::atomic_bool *cancel):impl(std::make_unique<Impl>()){
    auto &p=*impl;p.memory.bytes=bytes;p.memory.cancel=cancel;p.check();
    zlib_filefunc64_def io{openMemory,readMemory,noWrite,tellMemory,seekMemory,closeMemory,memoryError,&p.memory};
    p.file=unzOpen2_64("memory",&io);p.check();if(!p.file)throw Failure("Cannot read ZIP archive");
    unz_global_info64 global{};if(unzGetGlobalInfo64(p.file,&global)!=UNZ_OK)throw Failure("Cannot read ZIP directory");
    int status=global.number_entry?unzGoToFirstFile(p.file):UNZ_END_OF_LIST_OF_FILE;
    for(ZPOS64_T i=0;i<global.number_entry;++i){
        p.check();Impl::Entry entry{};
        if(status!=UNZ_OK||unzGetCurrentFileInfo64(p.file,&entry.info,nullptr,0,nullptr,0,nullptr,0)!=UNZ_OK)throw Failure("Cannot read ZIP directory");
        QByteArray name(entry.info.size_filename+1,Qt::Uninitialized);
        if(unzGetCurrentFileInfo64(p.file,&entry.info,name.data(),name.size(),nullptr,0,nullptr,0)!=UNZ_OK||unzGetFilePos64(p.file,&entry.position)!=UNZ_OK)throw Failure("Cannot read ZIP directory");
        name.resize(entry.info.size_filename);const auto key=memberName(name,(entry.info.flag&0x800)!=0);
        if(p.entries.contains(key))throw Failure(Message::format("Duplicate ZIP member: %1",{key}));
        p.entries[key]=entry;status=unzGoToNextFile(p.file);
    }
    p.check();if(status!=UNZ_END_OF_LIST_OF_FILE)throw Failure("Cannot read ZIP directory");
}
ZipArchive::~ZipArchive()=default;
bool ZipArchive::contains(const QString &name)const{return impl->entries.contains(name);}
QStringList ZipArchive::names()const{return impl->entries.keys();}
QByteArray ZipArchive::read(const QString &name,qint64 maxBytes,Progress progress){
    auto &p=*impl;p.check();if(!p.entries.contains(name))throw Failure(Message::format("Missing ZIP member: %1",{name}));
    const auto e=p.entries.value(name);
    if((e.info.flag&0x41)||!(e.info.compression_method==0||e.info.compression_method==8))throw Failure(Message::format("Unsupported ZIP compression or encryption: %1",{name}));
    if(maxBytes<0||e.info.uncompressed_size>quint64(maxBytes))throw Failure(Message::format("ZIP member exceeds the supported size: %1",{name}));
    if(unzGoToFilePos64(p.file,&e.position)!=UNZ_OK||unzOpenCurrentFile(p.file)!=UNZ_OK){p.check();throw Failure(Message::format("Cannot open ZIP member: %1",{name}));}
    QByteArray out;out.reserve(qsizetype(e.info.uncompressed_size));char chunk[65536];
    int last=-1;
    try{
        while(true){
            p.check();const int count=unzReadCurrentFile(p.file,chunk,sizeof(chunk));
            if(count<0)throw Failure(Message::format("Cannot decompress ZIP member: %1",{name}));
            if(!count)break;
            if(count>maxBytes-out.size())throw Failure(Message::format("ZIP member exceeds the supported size: %1",{name}));
            out.append(chunk,count);const int percent=int(out.size()*100/std::max<quint64>(1,e.info.uncompressed_size));
            if(progress&&percent!=last){last=percent;progress(percent);}
        }
        p.check();
    }catch(...){unzCloseCurrentFile(p.file);throw;}
    const int result=unzCloseCurrentFile(p.file);
    if(result!=UNZ_OK||quint64(out.size())!=e.info.uncompressed_size)throw Failure(Message::format("ZIP member checksum or size mismatch: %1",{name}));
    return out;
}
}
