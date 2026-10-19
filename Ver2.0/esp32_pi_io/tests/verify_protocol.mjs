// Execute control method bodies from the actual C++ headers with mocked hardware.
// ArduinoJson and C++ syntax are adapted to JavaScript; this is not a hardware test.
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const source=n=>fs.readFileSync(new URL('../'+n,import.meta.url),'utf8');
function body(file,name) {
 const s=source(file), m=new RegExp('^[ \t]*(?:bool|void|uint16_t|uint32_t)\\s+'+name+'\\([^{}]*\\)\\s*(?:const\\s*)?\\{','m').exec(s);
 assert(m,name); let start=m.index+m[0].length,end=start,depth=1;
 while(depth && end<s.length) { if(s[end]==='{') ++depth;if(s[end]==='}') --depth;++end; }
 return s.slice(start,end-1);
}
function adapt(s) {
 return s.replace(/\bin\b/g,'input')
 .replace(/JsonDocument input;/,'let input;')
 .replace(/deserializeJson\(input,line,DeserializationOption::NestingLimit\(4\)\)/,'badJSON(input=parsed(line))')
 .replace(/!input\.is<JsonObject>\(\)/,'!isObject(input)')
 .replace(/input\["target"\]\.isNull\(\)/g,'(input["target"]===null || input["target"]===undefined)')
 .replace(/input\["([^\"]+)"\]\.is<([^>]+)>\(\)/g,(_,k,t)=>`isType(input["${k}"],"${t}")`)
 .replace(/input\["([^\"]+)"\]\.as<[^>]+>\(\)/g,'input["$1"]')
 .replace(/String text; serializeJson\(input,text\);/,'let text=JSON.stringify(input);')
 .replace(/JsonDocument old; deserializeJson\(old,traLoiCu\);/,'let old=JSON.parse(traLoiCu);')
 .replace(/JsonDocument out;/g,'let out={};')
 .replace(/const bool chon\[3\]=\{([^}]+)\};/,'const chon=[$1];')
 .replace(/\(uint32_t\)/g,'').replace(/\(millis\(\)-lanLienLac\)/g,'((millis()-lanLienLac)>>>0)')
 .replace(/\b(?:uint32_t|uint8_t|size_t|int|char) (\w+)/g,'let $1')
 .replace(/const char \*(\w+)/g,'const $1')
 .replace(/traLoiCu\.length\(\)/g,'traLoiCu.length')
 .replace(/dong\[doDai\]=0; xuLy\(dong\);/,'xuLy(dong.slice(0,doDai).join(""));');
}
let now=0,sent=[],rx=[],writes=[];
const relay=vm.createContext({SW1:false,SW2:false,SW3:true,trangThai:[false,false,false],
 RELAY_PINS:[15,16,17],RELAY_ON:1,RELAY_OFF:0,digitalWrite:(p,v)=>writes.push([p,v])});
for(const name of ['choPhep','dat','tatTatCa','dangBat']) {
 let code=body('DieuKhienBom.h',name).replace(/for \(int /g,'for (let ');
 vm.runInContext(`function ${name}(${name==='choPhep'?'i':name==='dat'?'chon':''}){${code}}`,relay);
}
const isType=(v,t)=>t==='uint32_t'?Number.isInteger(v)&&v>=0&&v<=0xffffffff:t==='bool'?typeof v==='boolean':typeof v==='string';
const c=vm.createContext({
 millis:()=>now,session:0,idGanNhat:0,lanLienLac:0,bootId:99,lenhCu:'',traLoiCu:'',
 camBienDoc:0,chonCamBien:target=>['ALL','MOI_TRUONG.AHT20','MOI_TRUONG.BMP280','BUONG_KHI.AHT20','BUONG_KHI.BMP280','BUONG_KHI.SCD30','BUONG_KHI.CH3SH','BUONG_KHI.CO','BUONG_KHI.H2S','BUONG_KHI.NH3'].indexOf(target),
 dangDoc:false,choDoc:false,idDoc:0,phienDoc:0,PI_TIMEOUT_MS:15000,MAX_COMMAND_BYTES:384,
 doDai:0,dongQuaDai:false,mocKyTu:0,dong:[],soNgauNhien:()=>2468,
 bom:{tatTatCa:()=>relay.tatTatCa(),dat:x=>relay.dat(x)},
 chung:(o,type,id,cmd)=>Object.assign(o,{type,id,cmd,session:c.session,boot_id:99}),
 trangThai:o=>o.relays=Array.from(relay.trangThai),
 gui:(o,id=0)=>{sent.push(JSON.parse(JSON.stringify(o)));if(id && id===c.idGanNhat)c.traLoiCu=JSON.stringify(o);},
 baoLoi:(id,cmd,error,cache=false)=>c.gui({type:'response',id,cmd,error,ok:false},cache?id:0),
 parsed:s=>{try{return JSON.parse(s)}catch{return undefined}},badJSON:x=>x===undefined,
 isObject:x=>x && typeof x==='object' && !Array.isArray(x),isType,
 strcmp:(a,b)=>a===b?0:1,esp_reset_reason:()=>1,
 Serial:{available:()=>rx.length,read:()=>rx.shift()},dayUSB:()=>{}
});
for(const name of ['xuLy','capNhat']) vm.runInContext(`function ${name}(${name==='xuLy'?'line':''}){${adapt(body('GiaoTiepPi.h',name))}}`,c);
const last=()=>sent.at(-1),cmd=(id,command,args={})=>c.xuLy(JSON.stringify({id,cmd:command,session:c.session,...args}));
// Idle for a week: no reading, no autonomous pump activity.
now=604800000;c.capNhat();assert.equal(c.choDoc,false);assert.equal(relay.dangBat(),false);assert.equal(writes.length,0);
cmd(1,'RELAY',{sw1:false,sw2:false,sw3:true});assert.equal(last().error,'HELLO_REQUIRED');
cmd(1,'HELLO');assert.equal(last().ok,true);assert.equal(c.session,2468);
assert.deepEqual(Array.from(relay.trangThai),[false,false,false]);
cmd(1,'RELAY',{sw1:false,sw2:false,sw3:true});assert.equal(last().ok,true);assert.equal(relay.dangBat(),true);
const n=writes.length;
cmd(1,'RELAY',{sw1:false,sw2:false,sw3:true});assert.equal(last().replayed,true);assert.equal(writes.length,n);
cmd(1,'RELAY',{sw1:false,sw2:false,sw3:false});assert.equal(last().error,'ID_REUSED');assert.equal(writes.length,n);
cmd(2,'RELAY',{sw1:true,sw2:false,sw3:false});assert.equal(last().error,'RELAY_DISABLED');assert.equal(writes.length,n);
cmd(3,'RELAY',{sw1:false,sw2:false,sw3:1});assert.equal(last().error,'BOOLEAN_REQUIRED');
cmd(4,'READ');assert.equal(c.choDoc,true);assert.equal(c.dangDoc,true);assert.equal(last().type,'accepted');
cmd(4,'READ');assert.equal(last().error,'PENDING');
cmd(5,'READ');assert.equal(last().error,'BUSY');
cmd(6,'PING');assert.equal(last().ok,true);assert.equal(c.dangDoc,true);
cmd(7,'STOP');assert.equal(relay.dangBat(),false);assert.equal(c.dangDoc,true);
cmd(8,'HELLO');assert.equal(last().error,'BUSY');
c.dangDoc=false;c.choDoc=false;
cmd(8,'RELAY',{sw1:false,sw2:false,sw3:true});assert.equal(relay.dangBat(),true);
cmd(7,'STOP');assert.equal(last().error,'OLD_ID');assert.equal(relay.dangBat(),true);
now+=14999;c.capNhat();assert.equal(relay.dangBat(),true);
now+=1;c.capNhat();assert.equal(relay.dangBat(),false);assert.equal(c.session,0);assert.equal(last().cmd,'PI_TIMEOUT');assert.equal(last().pi_connected,false);
const after=writes.length;c.capNhat();assert.equal(writes.length,after); // no repeated off-event flood
cmd(9,'RELAY',{sw1:false,sw2:false,sw3:true,session:2468});assert.equal(last().error,'HELLO_REQUIRED');
// USB fragmented / CRLF input and invalid envelopes.
rx=Array.from('{"id":1,"cmd":"HEL');c.capNhat();assert.equal(c.session,0);
rx=Array.from('LO"}\r\n');c.capNhat();assert.equal(c.session,2468);
c.xuLy('{bad');assert.equal(last().error,'INVALID_JSON');
c.xuLy('[]');assert.equal(last().error,'INVALID_JSON');
for(const id of [0,-1,1.5,4294967296,'1',null]) { c.xuLy(JSON.stringify({id,cmd:'PING'}));assert.equal(last().error,'INVALID_ENVELOPE'); }
cmd(1,'UNKNOWN');assert.equal(last().error,'UNKNOWN_COMMAND');
const keep=c.lanLienLac;now+=100;cmd(2,'RELAY',{sw3:true});assert.equal(c.lanLienLac,keep);
// Drop the WHOLE oversized line; never interpret its trailing suffix as a relay command.
rx=Array.from('x'.repeat(400)+'{"id":3,"cmd":"STOP","session":2468}\n');while(rx.length)c.capNhat();
assert.equal(last().error,'LINE_TOO_LONG');assert.equal(c.idGanNhat,2);
rx=Array.from('{"id":');c.capNhat();now+=2001;c.capNhat();assert.equal(last().error,'LINE_TIMEOUT');
// Unsigned millis wrap-around for heartbeat expiry.
c.session=2468;c.lanLienLac=0xfffffff0;now=(0xfffffff0+14999)>>>0;c.capNhat();assert.equal(c.session,2468);
now=(0xfffffff0+15000)>>>0;c.capNhat();assert.equal(c.session,0);
// Relay OFF always allowed; default state and all SW masks.
for(let mask=0;mask<8;++mask) {
 Object.assign(relay,{SW1:!!(mask&1),SW2:!!(mask&2),SW3:!!(mask&4)});
 relay.tatTatCa();const chon=[!!(mask&1),!!(mask&2),!!(mask&4)];assert.equal(relay.dat(chon),true);
 assert.deepEqual(Array.from(relay.trangThai),chon);relay.tatTatCa();assert.equal(relay.dangBat(),false);
}
assert(!source('esp32_pi_io.ino').includes('LICH_DOC'));
assert(!source('ThoiGian.h').includes('class LichDoc'));
assert(source('esp32_pi_io.ino').includes('Serial0.end()'));
assert(source('CauHinh.h').includes('#if !ARDUINO_USB_CDC_ON_BOOT'));
console.log('PASS: actual command handling, HELLO/session, relay masks, duplicate/old IDs, READ busy, STOP/PING during read, timeout/rollover, malformed/fragmented/oversized input, no autonomous scheduler.');

// Real RS485 transaction body, with UART bytes and clock mocked.
let gasNow=20000, incoming=[], nextResponse=[], requests=[], callbackCount=0;
const g=vm.createContext({
 RS485_DE_RE:-1,RS485_COMMAND_GAP_MS:50,RS485_TIMEOUT_MS:1000,
 millis:()=>gasNow,delay:ms=>gasNow+=ms,cho:ms=>gasNow+=ms,
 phucVu:()=>++callbackCount,digitalWrite(){},delayMicroseconds(){},
 uart:{available:()=>incoming.length,read:()=>incoming.shift(),
 write:data=>{requests.push(Array.from(data));incoming.push(...nextResponse)},flush(){}}
});
let crc=body('CamBienKhi.h','crc16').replace(/\b(?:uint16_t|size_t|int) (\w+)/g,'let $1');
vm.runInContext(`function crc16(data,len){${crc}}`,g);
let tx=body('CamBienKhi.h','giaoDich')
 .replace(/uint8_t request\[6\]=\{([^}]+)\};/,'let request=new Uint8Array([$1]);')
 .replace(/request\+1/g,'request.subarray(1)').replace(/reply\+1/g,'reply.subarray(1)')
 .replace(/sizeof\(request\)/g,'request.length')
 .replace(/\(uint8_t\)\(crc>>8\)/g,'((crc>>8)&255)').replace(/\(uint8_t\)crc/g,'(crc&255)')
 .replace(/\(uint32_t\)/g,'')
 .replace(/\b(?:uint16_t|uint8_t|uint32_t|size_t) (\w+)/g,'let $1')
 .replace(/memmove\(reply,reply\+next,count\);/g,'reply.copyWithin(0,next,next+count);')
 .replace(/\bloi\b/g,'error.value');
vm.runInContext(`function giaoDich(command,address,reply,length,error){${tx}}`,g);
const hex=s=>Uint8Array.from(s.split(' ').map(x=>parseInt(x,16)));
const info=hex('AA 0F 01 29 00 0A 00 05 00 0A 00 05 02 A4 CC EE');
let error={value:''},reply=new Uint8Array(16);
nextResponse=[0x55,0x00,...hex('AA 0F 01 C5 80 EE'),...info];
assert.equal(g.giaoDich(15,1,reply,16,error),true);assert.deepEqual(Array.from(reply),Array.from(info));
assert.deepEqual(requests.at(-1),Array.from(hex('AA 0F 01 C5 80 EE')));
nextResponse=[];incoming=[];assert.equal(g.giaoDich(15,1,reply,16,error),false);assert.equal(error.value,'NO_RESPONSE');
nextResponse=Array.from(info);nextResponse[3]^=1;incoming=[];
assert.equal(g.giaoDich(15,1,reply,16,error),false);assert.equal(error.value,'INVALID_FRAME_OR_CRC');
nextResponse=Array.from(info);incoming=[];
assert.equal(g.giaoDich(15,2,reply,16,error),false);assert.equal(error.value,'INVALID_FRAME_OR_CRC');
assert(callbackCount>1000); // service USB/timeout while waiting for failed gas sensors
console.log('PASS: actual RS485 CRC, command bytes, noise/echo resync, no response, CRC/address rejection, callback servicing.');

// Target validation is enforced before scheduling any physical measurement.
c.dangDoc=false;c.choDoc=false;cmd(1,'HELLO');
cmd(1,'READ',{target:'UNKNOWN'});assert.equal(last().error,'INVALID_TARGET');assert.equal(c.choDoc,false);
cmd(2,'READ',{target:'BUONG_KHI.H2S'});assert.equal(c.camBienDoc,8);assert.equal(c.choDoc,true);
c.dangDoc=false;c.choDoc=false;cmd(3,'READ');assert.equal(c.camBienDoc,0);
assert(source('CamBienKhi.h').includes('if (chiSo>=0 && i!=chiSo) continue;'));
assert(source('CamBien.h').includes('docAHT && ahtOK'));
console.log('PASS: READ target validation, dispatch selection, ALL backward compatibility.');
