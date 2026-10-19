// Simulate real method bodies from the headers. Hardware IO is mocked.
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const read = file => fs.readFileSync(new URL('../'+file, import.meta.url),'utf8');
function body(file,name) {
  const source=read(file);
  const match=new RegExp(`\\b${name}\\([^)]*\\)\\s*(?:const\\s*)?\\{`).exec(source);
  assert.ok(match, `${file}: ${name}`);
  const begin=match.index+match[0].length;
  let depth=1,end=begin;
  while(depth && end<source.length) {
    if(source[end]==='{') ++depth;
    if(source[end]==='}') --depth;
    ++end;
  }
  assert.equal(depth,0);
  return source.slice(begin,end-1)
    .replace(/const bool (\w+)\[3\] = \{([^}]+)\};/g,'const $1 = [$2];')
    .replace(/uint8_t reply\[(\d+)\];/g,'let reply = new Uint8Array($1);')
    .replace(/\(uint8_t\)\(crc >> 8\)/g,'((crc >> 8) & 255)')
    .replace(/\(uint8_t\)crc/g,'(crc & 255)')
    .replace(/\((?:uint16_t|uint32_t|uint64_t|gpio_num_t|unsigned long)\)/g,'')
    .replace(/(\d+)ULL?\b/g,'$1').replace(/(\d+\.\d+)f\b/g,'$1')
    .replace(/now - mocThoiGian/g,'((now - mocThoiGian) >>> 0)')
    .replace(/millis\(\) - lanDoc/g,'((millis() - lanDoc) >>> 0)')
    .replace(/frame \+ 1/g,'frame.subarray(1)')
    .replace(/sizeof\(reply\)/g,'reply.length')
    .replace(/\b(?:uint16_t|uint32_t|uint64_t|size_t|int|float|bool) (\w+)/g,'let $1');
}
function install(context,file,name,args='') {
  let code=body(file,name);
  if(name==='readGas') code=code.replace(/\bvalue\b/g,'value.current');
  vm.runInContext(`function ${name}(${args}) {${code}}`,context);
}
let now=0,writes=[];
const c=vm.createContext({
  inChanRelay(){}, millis:()=>now, digitalWrite:(p,v)=>writes.push([p,v]), gpio_set_level(){},pinMode(){},OUTPUT:1,
  RELAY_PINS:[15,16,17],RELAY_ON:0,RELAY_OFF:1,SW1:true,SW2:true,SW3:true,
  PUMP_TIME_SECONDS:10,SETTLE_TIME_SECONDS:0,REPEAT_WAIT_SECONDS:60,READ_ONLY_AFTER_SETTLE:false,
  DUNG:0,DANG_BOM:1,CHO_ON_DINH:2,CHO_CHU_KY:3,trangThai:0,mocThoiGian:0,
  thoiGianBom:0,thoiGianOnDinh:0,thoiGianNghi:0,
  Serial:{printf(){},println(){}},khoangDoc:500,lanDoc:0
});
install(c,'ThoiGian.h','doiSangMs','value,multiplier');
for(const name of ['khoiDong','tatTatCa','batDau','capNhat','dangBat','choPhepDoc']) install(c,'DieuKhienBom.h',name);
for(let mask=0;mask<8;mask++) {
  Object.assign(c,{SW1:!!(mask&1),SW2:!!(mask&2),SW3:!!(mask&4)});
  c.khoiDong(); writes=[]; now=100;c.batDau();
  assert.deepEqual(writes.filter(([,v])=>v===0).map(([p])=>p),[0,1,2].filter(i=>mask&(1<<i)).map(i=>15+i));
  assert.equal(c.dangBat(),mask!==0);
  now=10099;c.capNhat();assert.equal(c.trangThai,1);
  now=10100;c.capNhat();assert.equal(c.trangThai,3);
  for(let pin=15;pin<=17;pin++) assert.equal(writes.filter(([p])=>p===pin).at(-1)[1],1);
  now=70100;c.capNhat();assert.equal(c.trangThai,1);
}
Object.assign(c,{PUMP_TIME_SECONDS:0,SETTLE_TIME_SECONDS:0,REPEAT_WAIT_SECONDS:0});
c.khoiDong();assert.equal(c.thoiGianNghi,1000);writes=[];c.batDau();c.capNhat();
assert.equal(writes.some(([,v])=>v===0),false);
Object.assign(c,{PUMP_TIME_SECONDS:1,SETTLE_TIME_SECONDS:2,REPEAT_WAIT_SECONDS:3,READ_ONLY_AFTER_SETTLE:true});
c.khoiDong();now=0;c.batDau();assert.equal(c.choPhepDoc(),false);
now=1000;c.capNhat();assert.equal(c.trangThai,2);assert.equal(c.choPhepDoc(),false);
now=3000;c.capNhat();assert.equal(c.trangThai,3);assert.equal(c.choPhepDoc(),true);
now=0xffffff00;c.batDau();now=743;c.capNhat();assert.equal(c.trangThai,1);
now=744;c.capNhat();assert.equal(c.trangThai,2);
assert.equal(c.doiSangMs(0xffffffff,60000),2147483647);
assert.equal(c.doiSangMs(1,60000),60000);
// Separate context for LichDoc (same method name batDau).
const timer=vm.createContext({millis:()=>now,khoangDoc:500,lanDoc:0,doiSangMs:c.doiSangMs});
install(timer,'ThoiGian.h','batDau','khoangMs');
for(const n of ['denLucDoc','batDauDoc','ketThucDoc']) install(timer,'ThoiGian.h',n);
now=0;timer.batDau(3000);assert.equal(timer.denLucDoc(),true);timer.batDauDoc();
now=2999;assert.equal(timer.denLucDoc(),false);now=3000;assert.equal(timer.denLucDoc(),true);
timer.batDauDoc();now=7000;timer.ketThucDoc();assert.equal(timer.lanDoc,7000);
assert.equal(timer.denLucDoc(),false);now=10000;assert.equal(timer.denLucDoc(),true);
timer.batDau(0);assert.equal(timer.khoangDoc,500);
now=0xfffffff0;timer.batDau(3000);timer.batDauDoc();now=2983;assert.equal(timer.denLucDoc(),false);
now=2984;assert.equal(timer.denLucDoc(),true);
const hex=s=>Uint8Array.from(s.split(' ').map(b=>parseInt(b,16)));
let response;
const gas=vm.createContext({Serial:{printf(){}},gasTransaction:(cmd,address,reply)=>{reply.set(response);return true;}});
install(gas,'CamBienKhi.h','crc16','data,length');install(gas,'CamBienKhi.h','validFrame','frame,length');
install(gas,'CamBienKhi.h','readGas','sensor,value');
for(const s of ['AA 0F 01 C5 80 EE','AA 01 02 81 E1 EE','AA 04 02 82 B1 EE',
 'AA 04 02 10 30 AD EE','AA 0F 01 29 00 0A 00 05 00 0A 00 05 02 A4 CC EE']) {
 const f=hex(s);assert.equal(gas.validFrame(f,f.length),true);f[3]^=1;assert.equal(gas.validFrame(f,f.length),false);
}
response=hex('AA 01 02 00 00 01 09 B9 9C EE');
const value={current:0};assert.equal(gas.readGas({address:2},value),true);assert.equal(value.current,1.09);
response[3]=0x80;assert.equal(gas.readGas({address:2},value),true);assert.equal(value.current,-1.09);
response[6]=100;assert.equal(gas.readGas({address:2},value),false);
// Address write must be blocked on type mismatch and on unchanged address.
Object.assign(gas,{Serial:{println(){},printf(){}},RS485_TX:41,RS485_RX:42,RS485_DE_RE:-1,
 RS485_INVERT_RX:false,RS485_CONFIG_STARTUP_MS:10000,OLD_ADDRESS:1,NEW_ADDRESS:1,
 CONFIGURE_GAS_TYPE:0x29,delay(){},digitalRead:()=>1,identifyGas:()=>true});
let addressWrites=0;
gas.gasTransaction=(cmd,address,reply)=>{assert.equal(cmd,4);++addressWrites;reply[3]=0x10;return true;};
install(gas,'CamBienKhi.h','doiDiaChi');gas.doiDiaChi();assert.equal(addressWrites,0);
gas.NEW_ADDRESS=5;gas.identifyGas=()=>false;gas.doiDiaChi();assert.equal(addressWrites,0);
gas.identifyGas=()=>true;gas.doiDiaChi();assert.equal(addressWrites,1);
// Verify bus ownership and the confirmed CH3SH mapping in the compiled source.
assert.match(read('CauHinh.h'),/SDA1 = 36, SCL1 = 38/);
assert.match(read('CauHinh.h'),/SDA2 = 43, SCL2 = 44/);
assert.match(read('pumping_and_measuring_system.ino'),/MOI_TRUONG\("MOI_TRUONG", Wire, false\)/);
assert.match(read('pumping_and_measuring_system.ino'),/BUONG_KHI\("BUONG_KHI", I2C_BUONG_KHI, true\)/);
assert.match(read('CamBienKhi.h'),/\{"CH3SH", 1, 0x29\}/);
assert.match(read('ManHinh.h'),/lcd\(U8G2_R0, U8X8_PIN_NONE, SCL1, SDA1\)/);
console.log('PASS: relay masks/zero/settle/rollover, read cadence/overrun, CRC/CH3SH, gas decode, address-write guards, I2C/LCD mapping.');
