#!/usr/bin/env python3
"""Probe shared root refusal and actual object identity in a disposable copy."""
import argparse,json,os,subprocess,time,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--workspace-root',type=Path,required=True);p.add_argument('--compiler',required=True);p.add_argument('--disposable-root',type=Path,required=True);a=p.parse_args();s=a.disposable_root.resolve()
if not str(s).startswith('/private/tmp/') or (s/'.git').exists():raise SystemExit('requires a disposable non-Git copy')
stamp=s/'build/toolchains/clang/build-identity.stamp';stamp_bytes=stamp.read_bytes();stamp_stat=stamp.stat()
target='build/toolchains/clang/obj/third_party/codework_shared/core/core_math/src/core_math.o';obj=s/target;results=[]
def run(name,args=(),ok=True,changed=None):
 before=obj.stat().st_mtime_ns if obj.exists() else None
 q=subprocess.run(['make',('all' if name=='root_refusal' else target),'TARGET_CONTRACT_HELPER='+str(a.workspace_root/'bin/desktop_release_target_contract.sh'),'CLANG_CC='+a.compiler,*args],cwd=s,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 after=obj.stat().st_mtime_ns if obj.exists() else None
 assert (q.returncode==0)==ok,(name,q.stdout[-2000:])
 if changed is not None:assert (before!=after)==changed,(name,before,after)
 results.append({'case':name,'exit':q.returncode,'rebuilt':before!=after})
run('baseline');run('noop',changed=False)
original_object=obj.read_bytes();object_stat=obj.stat()
h=s/'third_party/codework_shared/core/core_math/include/core_math.h';header_stat=h.stat();time.sleep(1.1);os.utime(h,None)
run('header_touch',changed=True);run('header_noop',changed=False)
f=s/'make/shared-adoption.mk';old=f.read_bytes();rule_stat=f.stat();f.write_bytes(old+b'\n# disposable rule probe\n')
try:run('rule_bytes',changed=True)
finally:f.write_bytes(old)
run('rule_restore',changed=True);run('debug_flags',['DEBUG=1'],changed=True);run('flags_restore',changed=True)
run('compiler',['CLANG_CC='+a.compiler+' -Qunused-arguments'],changed=True);run('compiler_restore',changed=True)
old=h.read_bytes();h.write_bytes(old+b'\n/* pin drift */\n')
try:run('pin_refusal',ok=False,changed=False)
finally:h.write_bytes(old)
run('retry',changed=True)
h.rename(h.with_suffix('.held'))
try:run('missing_header',ok=False,changed=False)
finally:h.with_suffix('.held').rename(h)
run('missing_retry');run('root_refusal',['SHARED_ROOT=/nonexistent-l4-root'],ok=False,changed=False)
run('final_noop',changed=False)
# Restore exact fixture timestamps only after proving final default bytes agree.
assert stamp.read_bytes()==stamp_bytes
assert obj.read_bytes()==original_object
os.utime(stamp,ns=(stamp_stat.st_atime_ns,stamp_stat.st_mtime_ns))
os.utime(h,ns=(header_stat.st_atime_ns,header_stat.st_mtime_ns))
os.utime(f,ns=(rule_stat.st_atime_ns,rule_stat.st_mtime_ns))
os.utime(obj,ns=(object_stat.st_atime_ns,object_stat.st_mtime_ns))
print(json.dumps(results,indent=2))
