#!/usr/bin/env python3
"""Check actual fisiCs object dependencies in a marked disposable sCulpt copy."""
import argparse,json,os,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--workspace',type=Path,required=True);p.add_argument('--evidence',type=Path,required=True);a=p.parse_args();s=a.source.resolve()
if not (s/'.l4-contained-copy').is_file() or (s/'.git').exists():p.error('requires marked disposable source')
a.evidence.mkdir(parents=True,exist_ok=True)
target='build/toolchains/fisics/obj/third_party/codework_shared/core/core_math/src/core_math.o';obj=s/target;results=[]
base=['make',target,'BUILD_TOOLCHAIN=fisics','FISICS_BIN='+str(a.workspace/'fisiCs/fisics'),'TARGET_CONTRACT_HELPER='+str(a.workspace/'bin/desktop_release_target_contract.sh')]
def run(name,settings=(),changed=None,ok=True):
 before=obj.stat().st_mtime_ns if obj.exists() else None
 with (a.evidence/(name+'.log')).open('w') as f:q=subprocess.run(base+list(settings),cwd=s,stdout=f,stderr=subprocess.STDOUT)
 assert (q.returncode==0)==ok,(name,q.returncode)
 after=obj.stat().st_mtime_ns if obj.exists() else None
 if changed is not None:assert (before!=after)==changed,name
 results.append(dict(case=name,exit=q.returncode,rebuilt=before!=after));(a.evidence/'incremental.json').write_text(json.dumps(results,indent=2))
run('prime');assert obj.with_suffix('.d').is_file();run('noop',changed=False)
h=s/'third_party/codework_shared/core/core_math/include/core_math.h';time.sleep(1.1);h.touch();run('header-touch',changed=True);run('header-noop',changed=False)
for name,path in [('rule',s/'make/shared-adoption.mk'),('adapter',s/'tools/fisics_compile.py')]:
 old=path.read_bytes();path.write_bytes(old+b'\n# identity probe\n')
 try:run(name,changed=True)
 finally:path.write_bytes(old)
 run(name+'-restore',changed=True)
flags=(s/'build/toolchains/fisics/build-identity.stamp').read_text().splitlines()[2]
outer=s/'l4_outer.h';nested=s/'l4_nested.h';assert not outer.exists() and not nested.exists()
outer.write_text('#include "l4_nested.h"\n');nested.write_text('/* dependency */\n');settings=['APP_CFLAGS='+flags+' -include '+str(outer)]
try:
 run('forced-transitive',settings,changed=True);assert str(nested) in obj.with_suffix('.d').read_text()
 time.sleep(1.1);nested.touch();run('transitive-touch',settings,changed=True);run('transitive-noop',settings,changed=False)
 old_obj=obj.read_bytes();old_dep=obj.with_suffix('.d').read_bytes();nested.unlink()
 run('missing-refusal',settings,changed=False,ok=False);assert obj.read_bytes()==old_obj and obj.with_suffix('.d').read_bytes()==old_dep
 nested.write_text('/* restored */\n');run('missing-retry',settings,changed=True)
finally:
 outer.unlink(missing_ok=True);nested.unlink(missing_ok=True)
run('flags-restore',changed=True);run('final-noop',changed=False)
print('passed',len(results),'cases')
