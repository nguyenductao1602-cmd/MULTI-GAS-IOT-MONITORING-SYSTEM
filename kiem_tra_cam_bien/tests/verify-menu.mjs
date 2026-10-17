// Execute actual firmware method bodies with mocked hardware. No reference FSM.
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const source = file => fs.readFileSync(new URL('../'+file, import.meta.url),'utf8');
function body(file,name) {
 const s=source(file), m=new RegExp(`\\b${name}\\([^)]*\\)\\s*(?:const\\s*)?\\{`).exec(s);
 assert.ok(m,`${file}:${name}`);
 let start=m.index+m[0].length,end=start,depth=1;
 while(depth&&end<s.length){if(s[end]==='{')++depth;if(s[end]==='}')--depth;++end;}
 assert.equal(depth,0);return s.slice(start,end-1);
}
function translate(code) {
 return code.replace(/uint8_t (\w+)\[(\d+)\] = \{([^}]+)\};/g,'let $1 = Uint8Array.from([$3]);')
 .replace(/uint8_t (\w+)\[(\d+)\];/g,'let $1 = new Uint8Array($2);')
 .replace(/ThongTinKhi (\w+);/g,'let $1 = {};')
 .replace(/\(uint8_t\)\(crc >> 8\)/g,'((crc >> 8) & 255)')
 .replace(/\(uint8_t\)crc/g,'(crc & 255)')
 .replace(/\((?:uint8_t|uint16_t|uint32_t|unsigned|unsigned long)\)/g,'')
 .replace(/sizeof\((\w+)\)/g,'$1.length')
 .replace(/frame \+ 1/g,'frame.subarray(1)').replace(/request \+ 1/g,'request.subarray(1)')
 .replace(/memmove\(reply, reply \+ next, received\)/g,'reply.copyWithin(0, next, next + received)')
 .replace(/(\d+\.\d+)f\b/g,'$1')
 .replace(/\b(?:uint8_t|uint16_t|uint32_t|size_t|bool|float|int) (\w+)/g,'let $1');
}
// Translate the actual strict parser (char pointer traversal -> JS string slicing).
let parser=body('NhapSerial.h','docSo')
 .replace('text += 2','text = text.slice(2)').replace('!*text','!text.length')
 .replace('while (*text)','while (text.length)')
 .replace('char c = tolower((unsigned char)*text++);','let c = text[0].toLowerCase(); text = text.slice(1);')
 .replaceAll("c - '0'","c.charCodeAt(0) - 48").replaceAll("c - 'a'","c.charCodeAt(0) - 97")
 .replaceAll('return false','return null').replace('value = number;','').replace('return true','return number');
const parse=vm.runInNewContext(`(function(text,base,minimum){${translate(parser)}})`);
for(const [s,b,min,v] of [['29',16,0,41],['0x29',16,0,41],['0B',16,0,11],['ff',16,0,255],
 ['255',10,1,255],['1',10,1,1],['0',16,0,0]]) assert.equal(parse(s,b,min),v);
for(const s of ['', '-1','+1','256','0x10','1x','999999999999','0']) assert.equal(parse(s,10,1),null,s);
for(const s of ['0x','100','GG','-1','99999999999']) assert.equal(parse(s,16,0),null,s);
let calls=[],writes=[],logs=[],cancel=false,reads=[],actual=0x29,replyAddress=1,failRead=false;
let selected=0;
const states=['CHON_CHUC_NANG','NHAP_DIA_CHI_CU','NHAP_DIA_CHI_MOI','NHAP_MA_CAM_BIEN','XAC_NHAN_SAI_MA','CHON_TEST','NHAP_DIA_CHI_TEST','DANG_TEST'];
const ctx=vm.createContext({
 ...Object.fromEntries(states.map((s,i)=>[s,i])),trangThai:0,diaChiCu:1,diaChiMoi:1,maNhap:0,thongTin:{},
 strcmp:(a,b)=>a===b?0:1,docSo:parse,tenKhi:n=>({41:'CH3SH',2:'CO',9:'H2S',11:'NH3'}[n]??'CHUA BIET'),
 Serial:{println:s=>logs.push(s),print:s=>logs.push(s),printf:(s,...args)=>logs.push([s,...args])},
 NHAP:{daThoat:false,kiemTraThoat(){if(cancel)this.daThoat=true;return this.daThoat;},cho(){return !this.daThoat;}},
 TEST:{dung(){selected=0;},chon(n,addr){selected=n;calls.push(['test',n,addr]);},quetI2C(b){calls.push(['scan',b]);},
 dangTestLCD:()=>selected===10,lcdCoPhanHoi:()=>true,capNhat(){calls.push(['tick',selected]);}},
 RS485:{daGuiLenhDoi:false,choKhoiDong:()=>true,
 docThongTin(addr,out){calls.push(['info',addr]); if(failRead)return false;
   Object.assign(out,reads.length?reads.shift():{diaChi:replyAddress,maLoai:actual,donVi:2,daiDo:10});return true;},
 doiDiaChi(addr){writes.push(addr);this.daGuiLenhDoi=true;replyAddress=addr;return true;}}
});
for(const n of ['hienMenu','thoat','vaoDoiDiaChi','hienDanhSach','nhapLaiDiaChi','kiemTraThongTin','thucHienDoi','capNhat','xuLy']) {
 let code=translate(body('MenuSerial.h',n));
 code=code.replace(/docSo\(line, (\d+), (\d+), number\)/g,'((number = docSo(line,$1,$2)) !== null)');
 vm.runInContext(`function ${n}(${n==='xuLy'?'line':''}) {${code}}`,ctx);
}
function reset(){calls=[];writes=[];logs=[];reads=[];cancel=false;actual=0x29;replyAddress=1;failRead=false;ctx.hienMenu();}
function enter(...lines){for(const line of lines)ctx.xuLy(line);}
// Correct type automatically changes address and verifies the actual type at new address.
reset();enter('f1','1','5','29');assert.deepEqual(writes,[5]);assert.equal(ctx.trangThai,0);
assert.deepEqual(calls.filter(([c])=>c==='info').map(([,a])=>a),[1,1,5]);
// Wrong type must never write before an explicit y. n returns to main.
reset();actual=0x0B;enter('f1','1','4','29');assert.equal(ctx.trangThai,4);assert.deepEqual(writes,[]);
enter('yes');assert.equal(ctx.trangThai,4);assert.deepEqual(writes,[]);
enter('n');assert.equal(ctx.trangThai,0);assert.deepEqual(writes,[]);
reset();actual=0x0B;enter('f1','1','4','29','y');assert.deepEqual(writes,[4]);assert.equal(ctx.trangThai,0);
// No response / wrong actual address: ask address again, no write.
reset();failRead=true;enter('f1','1','4','29');assert.equal(ctx.trangThai,1);assert.deepEqual(writes,[]);
reset();replyAddress=3;enter('f1','1','4','29');assert.equal(ctx.trangThai,1);assert.deepEqual(writes,[]);
// Invalid input and old=new never cause writes.
reset();enter('f1','0','256','abc');assert.equal(ctx.trangThai,1);assert.equal(calls.length,0);
enter('1','1','29');assert.deepEqual(writes,[]);assert.equal(ctx.trangThai,0);
// Module type changed while waiting for confirmation: stop instead of writing blindly.
reset();actual=0x0B;enter('f1','1','4','29');actual=0x09;enter('y');assert.deepEqual(writes,[]);assert.equal(ctx.trangThai,1);
// exit works from every UI state; queued exit before write cancels it.
for(let state=0;state<states.length;state++){reset();ctx.trangThai=state;enter('exit');assert.equal(ctx.trangThai,0);assert.deepEqual(writes,[]);}
reset();actual=0x0B;enter('f1','1','4','29');cancel=true;enter('y');assert.deepEqual(writes,[]);assert.equal(ctx.NHAP.daThoat,true);ctx.thoat();assert.equal(ctx.trangThai,0);
// Exit after command transmission must not claim that the write was cancelled.
reset();actual=0x0B;enter('f1','1','4','29');
const originalWrite=ctx.RS485.doiDiaChi;
ctx.RS485.doiDiaChi=function(addr){const v=originalWrite.call(this,addr);ctx.NHAP.daThoat=true;return v;};
enter('y');assert.deepEqual(writes,[4]);assert.equal(ctx.NHAP.daThoat,true);ctx.thoat();
assert.ok(logs.some(s=>typeof s==='string'&&s.includes('Lenh doi da duoc gui')));
ctx.RS485.doiDiaChi=originalWrite;
// F2 selects exactly one device; scan directions/custom address/list/exit.
for(let id=1;id<=10;id++){reset();enter('f2',String(id));ctx.capNhat();assert.equal(selected,id);assert.deepEqual(writes,[]);enter('exit');assert.equal(selected,0);}
reset();enter('f2','12');assert.deepEqual(calls,[['scan',true]]);assert.equal(ctx.trangThai,5);
reset();enter('f2','11');assert.deepEqual(calls,[['scan',false]]);
reset();enter('f2','13','255');assert.deepEqual(calls,[['test',13,255]]);enter('list');assert.equal(ctx.trangThai,5);
// Execute transport parser with a mock UART, including echo/noise/cancellation.
let clock=0,rx=[],tx=[],pending=[],stop=false;
const wire=vm.createContext({
 RS485_TIMEOUT_MS:1000,RS485_DE_RE:-1,HIGH:1,LOW:0,sanSang:true,daGuiLenhDoi:false,
 millis:()=>clock++,delay:n=>clock+=n,delayMicroseconds(){},digitalWrite(){},
 NHAP:{kiemTraThoat:()=>stop,cho:n=>{clock+=n;return !stop;}},
 Serial:{print(){},printf(){},println(){}},inHex(){},
 uart:{available:()=>rx.length,read:()=>rx.shift(),write:(data)=>{tx.push([...data]);rx.push(...pending);},flush(){}}
});
for(const [n,args] of [['crc16','data,length'],['khungHopLe','frame,length'],['giaiMaNongDo','frame,value'],['giaoDich','command,address,reply,length']]) {
 let code=translate(body('GiaoThuc485.h',n));if(n==='giaiMaNongDo')code=code.replace(/\bvalue\b/g,'value.current');
 vm.runInContext(`function ${n}(${args}){${code}}`,wire);
}
const hex=s=>Uint8Array.from(s.split(' ').map(b=>parseInt(b,16)));
const info=hex('AA 0F 01 29 00 0A 00 05 00 0A 00 05 02 A4 CC EE');
assert.equal(wire.khungHopLe(info,16),true);
pending=[0x55,...hex('AA 0F 01 C5 80 EE'),...info];
let out=new Uint8Array(16);assert.equal(wire.giaoDich(15,1,out,16),true);assert.deepEqual(out,info);
assert.deepEqual(tx[0],[0xAA,0x0F,1,0xC5,0x80,0xEE]);
rx=[];pending=[...info];pending[14]^=1;assert.equal(wire.giaoDich(15,1,out,16),false);
rx=[];pending=[];assert.equal(wire.giaoDich(15,1,out,16),false);
stop=true;const count=tx.length;assert.equal(wire.giaoDich(4,5,new Uint8Array(7),7),false);assert.equal(tx.length,count);assert.equal(wire.daGuiLenhDoi,false);
const value={current:0};const data=hex('AA 01 02 00 00 01 09 B9 9C EE');
assert.equal(wire.giaiMaNongDo(data,value),true);assert.equal(value.current,1.09);data[3]=0x80;
assert.equal(wire.giaiMaNongDo(data,value),true);assert.equal(value.current,-1.09);data[6]=100;assert.equal(wire.giaiMaNongDo(data,value),false);
// New direct SCD30 reader: verify CRC8 and byte/float layout from actual helpers.
const sensors=vm.createContext({
 floatFromBits:bits=>{const b=new ArrayBuffer(4),v=new DataView(b);v.setUint32(0,bits,false);return v.getFloat32(0,false);}
});
let c8=body('KiemTra.h','crc8').replace(
 'crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);',
 'crc = ((crc & 0x80) ? ((crc << 1) ^ 0x31) : (crc << 1)) & 255;');
vm.runInContext(`function crc8(data,count){${translate(c8)}}`,sensors);
assert.equal(sensors.crc8(Uint8Array.from([0xBE,0xEF]),2),0x92);
let sf=body('KiemTra.h','soThucSCD30').replace('memcpy(&value, &bits, sizeof(value));','value = floatFromBits(bits);');
vm.runInContext(`function soThucSCD30(data){${translate(sf)}}`,sensors);
assert.equal(sensors.soThucSCD30(Uint8Array.from([0x43,0xC8,0,0,0,0])),400);
assert.equal(sensors.soThucSCD30(Uint8Array.from([0xC1,0x20,0,0,0,0])),-10);
console.log('PASS: strict input, full F1/F2 flows, type override y/n, address retry, preflight change, exit all states/before/after write, single-device selection, CRC/echo/noise/timeout/decode.');
