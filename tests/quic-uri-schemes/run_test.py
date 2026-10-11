#!/usr/bin/env python3
"""Offline exact-source Qt6 probe of URI dispatch, scheme aliases, and ports."""
import argparse, hashlib, os, re, shlex, subprocess, sys, tempfile
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent))
from test_link_tls import function, section
PROXY='src/gharqad/configs/proxy/'
HEADER='src/nekobox/configs/proxy/'
MUTATIONS = {
 'accept-unhandled-scheme': (PROXY+'QUICBean.cpp', '  } else {\n    return false;\n  }\n\n  return true;\n}\n\nQString QUICBean::type()', '  } else {\n    return true;\n  }\n\n  return true;\n}\n\nQString QUICBean::type()'),
}

def main():
 p=argparse.ArgumentParser(description=__doc__); p.add_argument('--repo',type=Path,default=HERE.parents[1]);p.add_argument('--ref');p.add_argument('--keep-generated',type=Path);p.add_argument('--mutation',choices=MUTATIONS);args=p.parse_args()
 hashes={}
 def read(path):
  s=subprocess.check_output(['git','-C',str(args.repo),'show',f'{args.ref}:{path}'],text=True) if args.ref else (args.repo/path).read_text()
  if args.mutation:
   target,before,after=MUTATIONS[args.mutation]
   if path==target:
    if s.count(before)!=1:raise ValueError('Mutation anchor changed: '+args.mutation)
    s=s.replace(before,after)
  hashes[path]=hashlib.sha256(s.encode()).hexdigest();return s
 utils=read('src/gharqad/dataStore/Utils.cpp');uh=read('src/nekobox/dataStore/Utils.hpp');extra=read(HEADER+'AbstractBeanExtra.hpp');parse=read(PROXY+'Link2Bean.cpp');stream=read(HEADER+'V2RayStreamSettings.hpp');eh=read('src/nekobox/dataStore/ProxyEntity.hpp')
 code=(HERE/'support.hpp').read_text().replace('STREAM_FIELDS',section(stream,'class V2rayStreamSettings : public JsonStore {','V2rayStreamSettings() : JsonStore()')).replace('ENTITY_FIELDS',section(eh,'QString display_type;','int id = -1;'))
 ph=read('src/nekobox/dataStore/ProfileFilter.hpp');code=code.replace('PROFILE_FILTER_HEADER', 'struct ProfileFilterKey' + section(ph,'struct ProfileFilterKey','// --- Hash ---'))
 code+='\nnamespace QtBase64 {'+section(uh,'namespace QtBase64 {','namespace Qt515Base64 = QtBase64;')+'\nnamespace Qt515Base64 = QtBase64;\n'
 code+=read('src/gharqad/dataStore/base64.cpp').replace('#include <nekobox/dataStore/Utils.hpp>','')+'\n'
 start=uh.index('#define GetQuery(url)');code+=uh[start:uh.index('\n\n',start)]+'\n'
 for sig in ('QString GetQueryValue(','int GetQueryIntValue(','QByteArray DecodeB64IfValid(','QJsonArray QListStr2QJsonArray(','QString SubStrBefore(','QString SubStrAfter(','void AddQueryString(','void AddQueryInt(','void AddQueryNatural('):code+=function(utils,sig)+'\n'
 code+=read('src/3rdparty/URLParser/url_parser_function.h').replace('#pragma once','')+'\n'
 code+=read('src/3rdparty/URLParser/url_parser.h').replace('#pragma once','').replace('#include "url_parser_function.h"','')+'\n'
 code+='\n#define FIRST_OR_SECOND(a, b) a.isEmpty() ? b : a\nnamespace Configs {\n'
 qh=read(HEADER+'QUICBean.hpp');code+=qh[qh.index('class QUICBean : public AbstractBean {'):qh.index('#undef _add')];code+='QUICBean(ProxyEntity *, int); bool TryParseLink(const QString &) override; QString type() const override; QString ToShareLink() const; CoreObjOutboundBuildResult BuildCoreObjSingBox() const; };\n'
 th=read(HEADER+'TrojanVLESSBean.hpp');ctor='TrojanVLESSBean(Configs::ProxyEntity * entity';start=th.index('class TrojanVLESSBean : public AbstractBean {');prefix=th[start:th.index(ctor,start)];code+=re.sub(r'\bexplicit\s*$','',prefix)+function(th,ctor)+'\nbool TryParseLink(const QString &) override; };\n'
 names='Mieru Http ShadowSocks Chain VMess AnyTLS ShadowTLS Wireguard Tailscale SSH ExtraCore Tor Naive TrustTunnel Juicity Snell'.split()
 for n in names:code+='struct '+n+'Bean : AbstractBean { '+n+'Bean(ProxyEntity *e, int=0) : AbstractBean(e) { throw std::runtime_error("unexpected '+n+' allocation"); } };\n'
 code+='struct SocksBean : AbstractBean { static constexpr int type_Socks5=0; SocksBean(ProxyEntity *e,int) : AbstractBean(e) { throw std::runtime_error("unexpected Socks allocation"); } };\n'
 code+='namespace From_Link {\n'
 for sig in ('void add_network(T * t, const QUrlQuery &obj)',):code+='template<typename T> '+function(extra,sig)+'\n'
 code+='}\nnamespace To_CoreObj_box {\n'
 for sig,params in [('inline void add_default_fields(T & obj, const AbstractBean * bean)','typename T'),('inline void add_non_empty(T & obj, const QString & key, const QString & value)','typename T'),('inline void add_network(T & obj, B * bean)','typename T, typename B')]:code+='template<'+params+'> '+function(extra,sig)+'\n'
 code+='}\nnamespace To_Link {\n'
 export=read(PROXY+'Bean2Link.cpp')
 for sig in ('void add_query_int_natural(', 'void add_query_int(', 'void add_query_nonempty(', 'void add_default_fields(QUrl & url, const AbstractBean * bean)'):code+=function(export,sig)+'\n'
 code+='template<typename T> '+function(extra,'void add_network(QUrlQuery & query, T * obj)')+'\n}\n'
 for sig in ('void From_Link::set_boolean(','void From_Link::add_tls(','void From_Link::add_default_fields(','void From_Link::add_mux_state('):code+=function(parse,sig)+'\n'
 for path,sigs in [('QUICBean.cpp',('QUICBean::QUICBean(','bool QUICBean::TryParseLink(','QString QUICBean::type() const','QString QUICBean::ToShareLink() const','CoreObjOutboundBuildResult QUICBean::BuildCoreObjSingBox() const')),('TrojanVLESSBean.cpp',('bool TrojanVLESSBean::TryParseLink(',))]:
  source=read(PROXY+path)
  for sig in sigs:code+=function(source,sig)+'\n'
 entity=read('src/gharqad/dataStore/ProxyEntity.cpp');db=read('src/gharqad/dataStore/Database.cpp');main=read('src/gharqad/main.cpp')
 code+='\n#define cast(X) } else if (type == #X) {\n'
 for sig in ('bool ProxyEntity::isValid() const','ProxyEntity::ProxyEntity(','std::shared_ptr<const Configs::AbstractBean> ProxyEntity::bean() const'):code+=function(entity,sig)+'\n'
 code+=function(eh,'std::shared_ptr<A> unlock(').replace('std::shared_ptr<A> unlock(','template<typename A> std::shared_ptr<A> ProxyEntity::unlock(')+'\n'
 code+='std::shared_ptr<ProxyEntity> '+function(db,'ProfileManager::NewProxyEntity(')+'\n'+function(db,'QString ProfileManager::GetDisplayType(')+'\n'
 start=main.index('Preset::SingBox::OutboundTypes = {');end=main.index('\n  };',start)+5;code+='void initializeTypes() { '+main[start:end]+' }\n}\n'
 pf=read('src/gharqad/dataStore/ProfileFilter.cpp');code+='namespace Configs {\n'
 for sig in ('ProfileFilterKey::ProfileFilterKey(', 'bool ProfileFilterKey::operator<('):code+=function(pf,sig)+'\n'
 code+='}\n'
 raw=read('src/gharqad/configs/sub/GroupUpdater.cpp');code+='namespace Subscription {\n'+function(raw,'bool RawUpdater::AddProxy(')+'\n'
 # RawUpdater includes a brace-counting JSON splitter. Use source section for all bodies,
 # since a raw character counter would incorrectly count brace literals.
 start=raw.index('int JsonEndIdx(');end=raw.index('static QString jsonFirstString(',start);code+=raw[start:end]
 start=raw.index('void RawUpdater::update(const QString &str3)');end=raw.index('void RawUpdater::updateSIP008(',start);code+=raw[start:end]+'\n}\n'
 code+=(HERE/'checks.cpp').read_text()
 if args.keep_generated:args.keep_generated.write_text(code)
 print('SOURCE',args.ref or str(args.repo),'Qt',subprocess.check_output(['pkg-config','--modversion','Qt6Core'],text=True).strip(),flush=True)
 for path,h in hashes.items():print(h,path,flush=True)
 with tempfile.TemporaryDirectory(prefix='nekobox-dispatch-') as temp:
  cpp=Path(temp)/'test.cpp';exe=Path(temp)/'test';cpp.write_text(code)
  flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
  subprocess.run(['c++','-std=c++20','-fPIC','-Wall','-Wextra','-Werror','-Wno-unused-variable','-O0',*shlex.split(os.environ.get('EXTRA_CXXFLAGS','')),str(cpp),'-o',str(exe),*flags],check=True,timeout=90)
  return subprocess.run([str(exe)],timeout=10).returncode
if __name__=='__main__':raise SystemExit(main())
