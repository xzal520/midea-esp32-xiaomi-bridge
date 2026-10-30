'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
class Element{
  constructor(){this.children=[];this.dataset={};this.events={};this.checked=false;this.disabled=false;this.textContent='';this.scrollTop=0;this.scrollHeight=100;}
  addEventListener(name,fn){this.events[name]=fn;}
  appendChild(child){child.parent=this;this.children.push(child);return child;}
  append(...children){for(const child of children)this.appendChild(child);}
  replaceChildren(){for(const child of this.children)child.parent=null;this.children=[];}
  get firstChild(){return this.children[0];}
  remove(){this.parent.children.splice(this.parent.children.indexOf(this),1);this.parent=null;}
}
const nodes=new Map(['io16RxToggle','io16RxState','io16RxStats','io16RxLog','io16RxLogStatus','io16RxNotice','io16RxAutoScroll','io16RxClear','log','autoScroll','logStatus','clearLog','send','hex'].map(id=>[id,new Element()]));
nodes.get('io16RxAutoScroll').checked=nodes.get('autoScroll').checked=true;
const document={hidden:false,getElementById:id=>nodes.get(id)||null,querySelector:()=>null,querySelectorAll:()=>[],createElement:()=>new Element()};
const context=vm.createContext({document,location:{pathname:'/debug'},setTimeout,clearTimeout,AbortController,console});
const script=fs.readFileSync(process.argv[2],'utf8');
const startup=script.lastIndexOf('refreshBackground();document.addEventListener');
assert(startup>=0);vm.runInContext(script.slice(0,startup),context);
let state={enabled:true,bytes:4,frames:1,overflows:0,signalErrors:0,latestId:2};
let entries=[{id:1,time:'00:00:01.000',dir:'IO16RX',hex:'DE AD 00 FF'},{id:2,dir:'RX',hex:'MUST NOT APPEAR'}];
let rejectPost=false;const calls=[];
context.request=async(url,options={})=>{
  calls.push({url,options});
  if(url==='/api/io16/rx'){
    assert.equal(options.method,'POST');if(rejectPost)throw Error('switch failed');
    state={...state,enabled:options.body==='on'};return {text:async()=> 'switched'};
  }
  if(url==='/api/io16/log/clear'){entries=[];return {json:async()=>({latestId:state.latestId,message:'cleared'})};}
  if(url.startsWith('/api/io16/log?')){
    const since=Number(url.split('=')[1]);return {json:async()=>({state,entries:entries.filter(e=>e.id>since)})};
  }
  if(url.startsWith('/api/log?'))return {json:async()=>({entries:[{id:17,time:'00:00:01.100',dir:'RX',hex:'AA 23'}]})};
  throw Error('Unexpected request '+url);
};
context.getState=async()=>{};
const settle=()=>new Promise(resolve=>setImmediate(resolve));
(async()=>{
  vm.runInContext('io16RxEnabled=true',context);
  await context.poll();await context.pollIo16Rx();
  assert.equal(nodes.get('log').children.length,1);assert.equal(nodes.get('io16RxLog').children.length,1);
  assert.equal(nodes.get('io16RxLog').children[0].children[3].textContent,'DE AD 00 FF');
  assert.equal(vm.runInContext('cursor',context),17);assert.equal(vm.runInContext('io16RxCursor',context),1);
  await nodes.get('io16RxClear').events.click();await context.pollIo16Rx();
  assert.equal(nodes.get('io16RxLog').children.length,0);assert.equal(nodes.get('log').children.length,1);
  state={...state,latestId:0};entries=[];await context.pollIo16Rx();assert.equal(vm.runInContext('io16RxCursor',context),0);
  const toggle=nodes.get('io16RxToggle');toggle.checked=true;
  await toggle.events.change({currentTarget:toggle});await settle();
  assert.equal(toggle.checked,true);assert.equal(nodes.get('send').disabled,true);assert.equal(nodes.get('hex').disabled,true);
  toggle.checked=false;await toggle.events.change({currentTarget:toggle});await settle();
  assert.equal(nodes.get('send').disabled,false);assert.equal(toggle.checked,false);
  assert.deepEqual(calls.filter(c=>c.url==='/api/io16/rx').map(c=>c.options.body),['on','off']);
  rejectPost=true;toggle.checked=true;await toggle.events.change({currentTarget:toggle});await settle();
  assert.equal(toggle.checked,false);assert.equal(nodes.get('io16RxNotice').dataset.error,'true');
  const before=calls.length;document.hidden=true;await context.pollIo16Rx();assert.equal(calls.length,before);document.hidden=false;
  vm.runInContext('updating=true',context);await context.pollIo16Rx();assert.equal(calls.length,before);
  console.log('PASS: actual web handlers isolate both log windows, keep independent cursors, switch/restore controls and recover from errors');
})().catch(error=>{console.error(error);process.exitCode=1;});
