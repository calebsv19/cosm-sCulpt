#!/usr/bin/env python3
"""MCP stdio and deterministic CLI adapter for the running sCulpt editor."""
import argparse
import json
import os
from pathlib import Path
import socket
import stat
import sys

def call(operation, args, endpoint=None):
    if endpoint is None:
        candidates=[]
        for path in Path('/tmp').glob(f'sculpt-agent-{os.getuid()}-*.json'):
            try:
                data=json.loads(path.read_text());os.kill(data['pid'],0)
                candidates.append(path)
            except (OSError,ValueError,KeyError): pass
        if len(candidates)!=1: raise ValueError('Select an endpoint: expected exactly one live sCulpt process')
        endpoint=candidates[0]
    path=Path(endpoint);info=path.lstat()
    if not stat.S_ISREG(info.st_mode) or info.st_uid!=os.getuid() or stat.S_IMODE(info.st_mode)!=0o600:
        raise ValueError('Endpoint must be an owned 0600 regular descriptor')
    descriptor=json.loads(path.read_text())
    if descriptor['schema']!='sculpt_live_control_v1':raise ValueError('Wrong endpoint schema')
    expected=f'/tmp/sculpt-agent-{os.getuid()}-{descriptor["pid"]}.sock'
    if descriptor['socket']!=expected:raise ValueError('Endpoint socket identity mismatch')
    os.kill(descriptor['pid'],0)
    si=Path(expected).lstat()
    if not stat.S_ISSOCK(si.st_mode) or si.st_uid!=os.getuid() or stat.S_IMODE(si.st_mode)!=0o600:raise ValueError('Socket must be owned 0600')
    data=json.dumps(dict(args,operation=operation),sort_keys=True,separators=(',',':'),allow_nan=False).encode()+b'\n'
    if len(data)>65535:raise ValueError('Request exceeds native limit')
    with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as s:
        s.settimeout(30);s.connect(expected);s.sendall(data);chunks=[]
        while True:
            block=s.recv(65536)
            if not block:break
            chunks.append(block)
            if sum(map(len,chunks))>1048576:raise ValueError('Response exceeds limit')
    return json.loads(b''.join(chunks))

def catalog():
    descriptions={
        'state':'Read the active native document identity, revision, dirty state, views and collections.',
        'inspect':'Measure a native collection at its current pose.',
        'snapshot':'Create a new native layout snapshot, including unsaved geometry. Active document stays unchanged.',
        'save_as':'Create a new native layout and make it the active saved document; never overwrite.',
        'open':'Open a native layout in the actual editor; refuses unsaved current edits.',
        'view':'Apply a saved working view, use opaque materials, and frame it in the live viewport.',
        'capture':'Schedule a real Vulkan viewport capture to a new absolute BMP path.',
        'quit':'Quit the editor only when the active document is saved.'}
    tools=[]
    for op,description in descriptions.items():
        properties={};required=[]
        if op not in ('state','inspect'):
            properties.update(request_id={'type':'string','minLength':1},expected_revision={'type':'string','minLength':1});required+=['request_id','expected_revision']
        key='collection_id' if op=='inspect' else 'view_id' if op=='view' else 'path' if op in ('snapshot','save_as','open','capture') else None
        if key:properties[key]={'type':'string','minLength':1};required.append(key)
        if op=='view':
            for key in ('eye_m','target_m'):properties[key]={'type':'array','items':{'type':'number'},'minItems':3,'maxItems':3}
        tools.append({'name':'sculpt_'+op,'description':description,'inputSchema':{'type':'object','properties':properties,'required':required,'additionalProperties':False}})
    return tools

def serve(endpoint):
    for line in sys.stdin:
        request={}
        try:
            request=json.loads(line);method=request['method'];params=request.get('params',{})
            if method=='initialize':result={'protocolVersion':'2024-11-05','capabilities':{'tools':{}},'serverInfo':{'name':'sculpt-live-editor','version':'0.1.0'}}
            elif method=='tools/list':result={'tools':catalog()}
            elif method=='tools/call':
                name=params['name'];known={t['name']:t for t in catalog()}
                if name not in known:raise ValueError('Unknown tool')
                args=params.get('arguments',{});schema=known[name]['inputSchema']
                if set(args)-set(schema['properties']) or set(schema['required'])-set(args):raise ValueError('Invalid or missing arguments')
                value=call(name.removeprefix('sculpt_'),args,endpoint)
                result={'content':[{'type':'text','text':json.dumps(value)}],'isError':not value.get('ok',False)}
            elif method=='ping':result={}
            elif method.startswith('notifications/'):continue
            else:raise ValueError('Unknown method')
            response={'jsonrpc':'2.0','id':request.get('id'),'result':result}
        except (ValueError,KeyError,OSError,TypeError) as e:
            response={'jsonrpc':'2.0','id':request.get('id'),'error':{'code':-32602,'message':str(e)}}
        if 'id' in request:print(json.dumps(response),flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--endpoint');parser.add_argument('--call');parser.add_argument('--arguments',default='{}')
    args=parser.parse_args()
    if args.call:print(json.dumps(call(args.call,json.loads(args.arguments),args.endpoint),indent=2))
    else:serve(args.endpoint)
