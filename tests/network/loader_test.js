'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync(process.argv[2],'utf8');
const tail=source.split('static const char WEB_FOOT[]')[1];
const html=tail.match(/R"HTML\(([\s\S]*)\)HTML";/)[1].replace(/\)HTML"\s+BRIDGE_VERSION\s+R"HTML\(/g,'2.5.45');
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
function boot(ready=false){
  const label={textContent:'连接中'},scripts=[],timers=new Map();let next=0;
  const context=vm.createContext({bridgeAppReady:ready,document:{getElementById:()=>label,createElement:()=>({remove(){this.removed=true}}),body:{append:s=>scripts.push(s)}},setTimeout:(fn,delay)=>{timers.set(++next,{fn,delay});return next},clearTimeout:id=>timers.delete(id)});
  vm.runInContext(script,context);
  return {label,scripts,timers,run(delay){const [id,t]=[...timers].find(([,t])=>t.delay===delay);timers.delete(id);t.fn()}};
}
const failed=boot();assert.equal(failed.scripts[0].src,'/assets/app.js?v=2.5.45');
failed.scripts[0].onerror();assert.match(failed.label.textContent,/重试/);assert(failed.scripts[0].removed);
failed.run(1000);assert.match(failed.scripts[1].src,/retry=2$/);
failed.scripts[1].onerror();failed.run(2000);failed.scripts[2].onerror();
assert.match(failed.label.textContent,/加载失败/);assert.equal(failed.timers.size,0);
const fatal=boot();fatal.scripts[0].onload();assert.match(fatal.label.textContent,/启动失败/);
const good=boot(true);good.scripts[0].onload();assert.equal(good.label.textContent,'连接中');assert.equal(good.timers.size,0);
const slow=boot();slow.run(6000);assert.match(slow.label.textContent,/下载页面脚本/);
const cssHandler=source.match(/onerror="([^"]+)"/)[1];
const link={dataset:{},href:'http://device/assets/app.css?v=2.5.45'};
const cssContext=vm.createContext({link,setTimeout:fn=>fn()});
for(let n=1;n<=4;n++){
  vm.runInContext('(function(){'+cssHandler+'}).call(link)',cssContext);
  assert.match(link.href,new RegExp('&retryCss='+Math.min(n,3)+'$'));
}
console.log('PASS: actual CSS/script loading retries interrupted downloads, bounds retries and reports startup/slow-load errors');
