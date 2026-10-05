#pragma once
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(

<!DOCTYPE html>

<html lang="vi">

<head>

<meta charset="UTF-8">

<meta name="viewport" content="width=device-width,initial-scale=1">

<title>ESP32 Control Center</title>

<style>

*{box-sizing:border-box}

body{margin:0;font-family:Arial,Helvetica,sans-serif;background:#f3f4f6;color:#111827}

header{background:#111827;color:#fff;padding:18px}

header h1{margin:0;font-size:21px}

header p{margin:5px 0 0;color:#9ca3af;font-size:13px}

nav{display:flex;overflow-x:auto;gap:8px;padding:10px;background:#fff;border-bottom:1px solid #ddd}

nav button{border:0;padding:10px 14px;border-radius:8px;background:#e5e7eb;white-space:nowrap;cursor:pointer}

nav button.active{background:#111827;color:#fff}

main{max-width:900px;margin:auto;padding:15px}

.section{display:none}.section.active{display:block}

.card{background:#fff;border-radius:12px;padding:18px;margin-bottom:15px;box-shadow:0 2px 8px rgba(0,0,0,.06)}

.card h2{margin-top:0;font-size:18px}

.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px}

label{display:block;font-size:13px;margin-bottom:6px;color:#4b5563}

input{width:100%;padding:11px;border:1px solid #d1d5db;border-radius:8px;font-size:15px}

input:focus{outline:2px solid #93c5fd;border-color:#2563eb}

button.action{border:0;border-radius:8px;padding:11px 16px;cursor:pointer;margin:4px;font-weight:600}

button:disabled{opacity:.55;cursor:not-allowed}

.primary{background:#2563eb;color:#fff}.success{background:#16a34a;color:#fff}.warning{background:#f59e0b;color:#fff}.danger{background:#dc2626;color:#fff}.secondary{background:#e5e7eb;color:#111827}

.status{font-size:20px;font-weight:bold;margin-bottom:10px}.status-box{padding:15px;background:#f9fafb;border-radius:10px}

.warning-box{background:#fff7ed;border:1px solid #fed7aa;color:#9a3412;padding:12px;border-radius:8px;margin-top:10px;font-size:13px}

.success-box{background:#ecfdf5;border:1px solid #a7f3d0;color:#065f46;padding:12px;border-radius:8px;margin-top:10px;font-size:13px}

.error-box{background:#fef2f2;border:1px solid #fecaca;color:#991b1b;padding:12px;border-radius:8px;margin-top:10px;font-size:13px;display:none}

.progress{width:100%;height:15px;background:#e5e7eb;border-radius:10px;overflow:hidden}.progress-bar{height:100%;width:0%;background:#2563eb;transition:width .3s}

.small{font-size:13px;color:#6b7280}.value{font-size:25px;font-weight:bold}

.unsaved{display:none;margin:10px 0 0;color:#b45309;font-weight:600;font-size:13px}

</style>

</head>

<body>

<header>

<h1>ESP32 CONTROL CENTER</h1>

<p>Điều khiển Servo 360° và màn hình LCD</p>

</header>

<nav>

<button onclick="showTab('overview')" id="nav-overview">🎮 Bàn điều khiển (Motion Deck)</button>

<button onclick="showTab('servo')" id="nav-servo">⚙️ Servo</button>

<button onclick="showTab('motion')" id="nav-motion">▶️ Chuyển động</button>

<button onclick="showTab('calibration')" id="nav-calibration">🔧 Hiệu chỉnh</button>

<button onclick="showTab('profiles')" id="nav-profiles">📁 Hồ sơ</button>

<button onclick="showTab('network')" id="nav-network">📡 Mạng</button>

<button onclick="showTab('system')" id="nav-system">🖥️ Hệ thống</button>

</nav>

<main>

<section id="overview" class="section active">

<div class="card">
<h2>🎮 ĐIỀU KHIỂN ĐỒNG THỜI (DUAL SERVO MASTER)</h2>
<p class="small">Lệnh thực thi đồng thời trên cả 2 Servo (GPIO 1 & GPIO 2):</p>
<button class="action primary" onclick="openAll()">🚪 MỞ CẢ 2 SERVO</button>
<button class="action warning" onclick="closeAll()">🔒 ĐÓNG CẢ 2 SERVO</button>
<button class="action success" onclick="startAll()">▶️ BẮT ĐẦU CẢ 2 CHU KỲ</button>
<button class="action danger" onclick="stopAll()">⏹️ DỪNG TẤT CẢ (STOP ALL)</button>
</div>

<div class="card" style="border-left: 5px solid #10b981;">
<h2>🟢 BÀN ĐIỀU KHIỂN SERVO 1 (GPIO 1)</h2>
<div class="status-box">
<div>Trạng thái: <strong id="state1">Sẵn sàng</strong> | Chu kỳ: <strong><span id="cycle1">0</span> / <span id="cycles1">1</span></strong></div>
</div>
<button id="btnOpen1" class="action primary" onclick="openDoor1()">🚪 MỞ S1 (Không xoay về)</button>
<button id="btnClose1" class="action warning" onclick="closeDoor1()">🔒 ĐÓNG S1 (Về ban đầu)</button>
<button id="btnStart1" class="action success" onclick="startMotion1()">▶️ Bắt đầu chu kỳ S1</button>
<button id="btnStop1" class="action danger" onclick="stopMotion1()">⏹️ Dừng S1</button>
<button id="btnTestFwd1" class="action secondary" onclick="testForward1()">↻ Test Thuận S1</button>
<button id="btnTestRev1" class="action secondary" onclick="testReverse1()">↺ Test Nghịch S1</button>
</div>

<div class="card" style="border-left: 5px solid #00f2fe;">
<h2>🔵 BÀN ĐIỀU KHIỂN SERVO 2 (GPIO 2)</h2>
<div class="status-box">
<div>Trạng thái: <strong id="state2">Sẵn sàng</strong> | Chu kỳ: <strong><span id="cycle2">0</span> / <span id="cycles2">1</span></strong></div>
</div>
<button id="btnOpen2" class="action primary" onclick="openDoor2()">🚪 MỞ S2 (Không xoay về)</button>
<button id="btnClose2" class="action warning" onclick="closeDoor2()">🔒 ĐÓNG S2 (Về ban đầu)</button>
<button id="btnStart2" class="action success" onclick="startMotion2()">▶️ Bắt đầu chu kỳ S2</button>
<button id="btnStop2" class="action danger" onclick="stopMotion2()">⏹️ Dừng S2</button>
<button id="btnTestFwd2" class="action secondary" onclick="testForward2()">↻ Test Thuận S2</button>
<button id="btnTestRev2" class="action secondary" onclick="testReverse2()">↺ Test Nghịch S2</button>
</div>

<div class="card">
<h2>Trạng thái hệ thống chung</h2>
<div class="status-box">
<div id="state" class="status">Đang tải...</div>
<div>Tổng chu kỳ: <strong><span id="cycle">0</span> / <span id="cycles">0</span></strong></div>
<br><div class="progress"><div id="progress" class="progress-bar"></div></div>
</div>
<div class="warning-box">⚠️ Nút Dừng trên giao diện kích hoạt <strong>ngắt PWM và Clamp LOW</strong> chống trôi motor.</div>
<div id="errorBox" class="error-box"></div>
</div>

</section>



<section id="servo" class="section">

<div class="card">

<h2>⚙️ Cấu hình Servo</h2>

<div class="grid">

<div><label>GPIO Servo</label><input id="servoPin" type="number" value="1" disabled></div>

<div><label>Điểm dừng</label><input id="servoStop" type="number" min="0" max="180"></div>

<div><label>Mức điều khiển chiều thuận</label><input id="servoForward" type="number" min="0" max="180"></div>

<div><label>Mức điều khiển chiều nghịch</label><input id="servoReverse" type="number" min="0" max="180"></div>

</div>

<div id="servoUnsaved" class="unsaved">● Có thay đổi chưa lưu</div>

<br><button class="action primary" onclick="saveConfig()">💾 Lưu cấu hình</button>

</div>

<div class="card">

<h2>🧪 Chạy thử thủ công</h2>

<button id="manualForward" class="action success" onclick="testForward()">↻ Quay thuận</button>

<button id="manualReverse" class="action warning" onclick="testReverse()">↺ Quay nghịch</button>

<button class="action danger" onclick="stopMotion()">⏹️ Dừng</button>

</div>

</section>



<section id="motion" class="section">

<div class="card">

<h2>▶️ Cấu hình chuyển động</h2>

<div class="grid">

<div><label>Thời gian quay thuận (ms)</label><input id="forwardMs" type="number" min="1" max="600000"></div>

<div><label>Thời gian quay nghịch (ms)</label><input id="reverseMs" type="number" min="1" max="600000"></div>

<div><label>Thời gian dừng giữa chiều (ms)</label><input id="pauseMs" type="number" min="0" max="600000"></div>

<div><label>Số chu kỳ</label><input id="cyclesInput" type="number" min="1" max="1000"></div>

</div>

<div id="motionUnsaved" class="unsaved">● Có thay đổi chưa lưu</div>

<br><button class="action primary" onclick="saveConfig()">💾 Lưu cấu hình</button>

</div>

<div class="card">

<h2>Định nghĩa một chu kỳ</h2>

<p><strong>Thuận → Dừng → Nghịch → Dừng</strong></p>

<p class="small">Ví dụ: 3 chu kỳ nghĩa là Servo thực hiện 3 lần chuyển động thuận và nghịch.</p>

</div>

</section>



<section id="calibration" class="section">

<div class="card">

<h2>🔧 Hiệu chỉnh điểm dừng</h2>

<p>Tìm giá trị mà Servo 360° đứng yên hoàn toàn.</p>

<div class="value"><span id="calibrationValue">90</span></div>

<button class="action secondary" onclick="calibrate(-1)">−1</button>

<button class="action primary" onclick="calibrate(0)">🧪 Kiểm tra</button>

<button class="action secondary" onclick="calibrate(1)">+1</button>

<p class="small">Giá trị mới chỉ áp dụng để thử. Sau khi tìm được điểm dừng thật sự, nhấn <strong>Lưu cấu hình</strong> ở tab Servo để ghi vào bộ nhớ ESP32.</p>

</div>

<div class="card">

<h2>📐 Quan hệ thời gian → số răng</h2>

<p>Cơ cấu hiện tại: bánh nhỏ <strong>20 răng</strong>, bánh lớn <strong>80 răng</strong>, tỷ số truyền <strong>4:1</strong>.</p>

<p>Bánh nhỏ quay 90° sẽ làm bánh lớn quay khoảng 22,5°.</p>

<p class="small">Vì Servo là loại 360° liên tục, thời gian quay phải được thực nghiệm và hiệu chỉnh; không thể xem thời gian là góc tuyệt đối.</p>

</div>

</section>



<section id="profiles" class="section">

<div class="card">

<h2>📁 Hồ sơ cấu hình</h2>

<p>Phiên bản hiện tại lưu cấu hình trực tiếp trên ESP32.</p>

<button class="action primary" onclick="saveConfig()">💾 Lưu cấu hình hiện tại</button>

</div>

<div class="card"><h2>Hồ sơ đề xuất</h2><ul><li>Fruit Bowl Default</li><li>Chạy nhanh</li><li>Chạy chậm</li><li>Hiệu chỉnh</li><li>Kiểm tra Servo</li></ul></div>

</section>



<section id="network" class="section">

<div class="card"><h2>📡 Mạng Wi-Fi</h2><div class="status-box"><p>SSID: <strong>ESP32-Servo</strong></p><p>Địa chỉ IP: <strong id="ip">192.168.4.1</strong></p><p>Trạng thái: <strong>Đang phát Wi-Fi</strong></p></div></div>

</section>



<section id="system" class="section">

<div class="card"><h2>🖥️ Hệ thống</h2><p>Thiết bị: <strong>LILYGO T-Display-S3</strong></p><p>Firmware: <strong>ESP32 Servo Control</strong></p><p>Servo GPIO: <strong>GPIO 1</strong></p><p>Web Server: <strong>ESP32 WebServer</strong></p></div>

<div class="card"><h2>⚙️ Cấu hình hệ thống</h2><p>Khôi phục toàn bộ tham số Servo và chuyển động về mặc định của firmware.</p><button class="action danger" onclick="factoryReset()">♻️ Khôi phục mặc định</button></div>
<div class="card"><h2>⚠️ An toàn</h2><p>Servo và cơ cấu bánh răng có thể gây kẹt tay hoặc gây hư hỏng cơ khí.</p><p>Luôn giữ tay khỏi bánh răng khi đang chạy.</p><p>Đối với máy thực tế, cần bổ sung <strong>nút dừng khẩn cấp vật lý</strong>.</p></div>

</section>

</main>



<script>

let configDirty = false;

let lastKnownConfig = null;



function el(id){ return document.getElementById(id); }



function showTab(name){

    document.querySelectorAll('.section').forEach(x => x.classList.remove('active'));

    document.querySelectorAll('nav button').forEach(x => x.classList.remove('active'));

    el(name).classList.add('active');

    el('nav-' + name).classList.add('active');

}



function showError(message){

    const box = el('errorBox');

    box.innerText = message || '';

    box.style.display = message ? 'block' : 'none';

}



function setDirty(value){

    configDirty = value;

    el('servoUnsaved').style.display = value ? 'block' : 'none';

    el('motionUnsaved').style.display = value ? 'block' : 'none';

}



function readForm(){

    return {

        stop: Number(el('servoStop').value),

        forward: Number(el('servoForward').value),

        reverse: Number(el('servoReverse').value),

        forwardMs: Number(el('forwardMs').value),

        reverseMs: Number(el('reverseMs').value),

        pauseMs: Number(el('pauseMs').value),

        cycles: Number(el('cyclesInput').value)

    };

}



function validateClientConfig(c){

    if (!Number.isInteger(c.stop) || c.stop < 0 || c.stop > 180) return 'Điểm dừng phải từ 0 đến 180.';

    if (!Number.isInteger(c.forward) || c.forward < 0 || c.forward > 180) return 'Mức thuận phải từ 0 đến 180.';

    if (!Number.isInteger(c.reverse) || c.reverse < 0 || c.reverse > 180) return 'Mức nghịch phải từ 0 đến 180.';

    if (!Number.isInteger(c.forwardMs) || c.forwardMs < 1 || c.forwardMs > 600000) return 'Thời gian thuận phải từ 1 đến 600000 ms.';

    if (!Number.isInteger(c.reverseMs) || c.reverseMs < 1 || c.reverseMs > 600000) return 'Thời gian nghịch phải từ 1 đến 600000 ms.';

    if (!Number.isInteger(c.pauseMs) || c.pauseMs < 0 || c.pauseMs > 600000) return 'Thời gian d�    el('cycles').innerText = data.cycles;
    el('ip').innerText = data.ip;

    if (data.s1) {
        if (el('state1')) el('state1').innerText = data.s1.state;
        if (el('cycle1')) el('cycle1').innerText = data.s1.cycle;
        if (el('cycles1')) el('cycles1').innerText = data.s1.cycles;
    }
    if (data.s2) {
        if (el('state2')) el('state2').innerText = data.s2.state;
        if (el('cycle2')) el('cycle2').innerText = data.s2.cycle;
        if (el('cycles2')) el('cycles2').innerText = data.s2.cycles;
    }

    const percentage = data.cycles > 0
        ? Math.min(100, Math.max(0, (data.cycle / data.cycles) * 100))
        : 0;
    el('progress').style.width = percentage + '%';

    const running1 = data.s1 ? !!data.s1.running : !!data.running;
    const running2 = data.s2 ? !!data.s2.running : false;

    if (el('btnOpen1')) el('btnOpen1').disabled = running1;
    if (el('btnClose1')) el('btnClose1').disabled = running1;
    if (el('btnStart1')) el('btnStart1').disabled = running1;

    if (el('btnOpen2')) el('btnOpen2').disabled = running2;
    if (el('btnClose2')) el('btnClose2').disabled = running2;
    if (el('btnStart2')) el('btnStart2').disabled = running2;
}

async function apiRequest(url, options = {}){
    const response = await fetch(url, options);
    let data = {};
    try { data = await response.json(); } catch (_) {}
    if (!response.ok || data.ok === false) {
        throw new Error(data.error || ('HTTP ' + response.status));
    }
    return data;
}

let isPolling = false;

async function updateStatus(){
    if (isPolling) return;
    isPolling = true;

    try{
        const data = await apiRequest('/api/status');
        updateStatusFields(data);

        if (!configDirty) {
            applyConfigToForm(data);
        }
        showError('');
    }catch(error){
        el('state').innerText = 'Mất kết nối ESP32';
        showError('Không thể lấy trạng thái: ' + error.message);
    }finally{
        isPolling = false;
    }
}

async function loadConfig(){
    try{
        const data = await apiRequest('/api/config');
        applyConfigToForm(data);
    }catch(error){
        showError('Không thể tải cấu hình: ' + error.message);
    }
}

// Master Dual Controls
async function openAll(){ try{ await apiRequest('/api/all/open', {method:'POST'}); await updateStatus(); }catch(e){ showError('Lỗi mở cả 2: ' + e.message); } }
async function closeAll(){ try{ await apiRequest('/api/all/close', {method:'POST'}); await updateStatus(); }catch(e){ showError('Lỗi đóng cả 2: ' + e.message); } }
async function startAll(){ try{ await apiRequest('/api/all/start', {method:'POST'}); await updateStatus(); }catch(e){ showError('Lỗi chạy cả 2: ' + e.message); } }
async function stopAll(){ try{ await apiRequest('/api/all/stop', {method:'POST'}); await updateStatus(); }catch(e){ showError('Lỗi dừng tất cả: ' + e.message); } }

// Servo 1 Controls (GPIO 1)
async function openDoor1(){ try{ await apiRequest('/api/s1/open', {method:'POST'}); await updateStatus(); }catch(e){ showError('Lỗi mở S1: ' + e.message); } }
async function closeDoor1(){ try{ await apiRequest('/api/s1/close', {method:'POST'}); await updateStatus(); }catch(e){ showError('Lỗi đóng S1: ' + e.message); } }
async function startMotion1(){ try{ await apiRequest('/api/s1/start', {method:'POST'}); await updateStatus(); }catch(e){ showError('Lỗi bắt đầu S1: ' + e.message); } }
async function stopMotion1(){ try{ await apiRequest('/api/s1/stop', {method:'POST'}); await updateStatus(); }catch(e){ showError('Lỗi dừng S1: ' + e.message); } }
async function testForward1(){ try{ await apiRequest('/api/s1/test/forward', {method:'POST'}); await updateStatus(); }catch(e){ showError('Test thuận S1 thất bại: ' + e.message); } }
async function testReverse1(){ try{ await apiRequest('/api/s1/test/reverse', {method:'POST'}); await updateStatus(); }catch(e){ showError('Test nghịch S1 thất bại: ' + e.message); } }

// Servo 2 Controls (GPIO 2)
async function openDoor2(){ try{ await apiRequest('/api/s2/open', {method:'POST'}); await updateStatus(); }catch(e){ showError('Lỗi mở S2: ' + e.message); } }
async function closeDoor2(){ try{ await apiRequest('/api/s2/close', {method:'POST'}); await updateStatus(); }catch(e){ showError('Lỗi đóng S2: ' + e.message); } }
async function startMotion2(){ try{ await apiRequest('/api/s2/start', {method:'POST'}); await updateStatus(); }catch(e){ showError('Lỗi bắt đầu S2: ' + e.message); } }
async function stopMotion2(){ try{ await apiRequest('/api/s2/stop', {method:'POST'}); await updateStatus(); }catch(e){ showError('Lỗi dừng S2: ' + e.message); } }
async function testForward2(){ try{ await apiRequest('/api/s2/test/forward', {method:'POST'}); await updateStatus(); }catch(e){ showError('Test thuận S2 thất bại: ' + e.message); } }
async function testReverse2(){ try{ await apiRequest('/api/s2/test/reverse', {method:'POST'}); await updateStatus(); }catch(e){ showError('Test nghịch S2 thất bại: ' + e.message); } }

// Compatibility Aliases
async function openDoor(){ return openDoor1(); }
async function closeDoor(){ return closeDoor1(); }
async function startMotion(){ return startMotion1(); }
async function stopMotion(){ return stopAll(); }
async function testForward(){ return testForward1(); }
async function testReverse(){ return testReverse1(); }

    }catch(error){ showError('Không thể đóng: ' + error.message); }

}



async function startMotion(){

    try{

        await apiRequest('/api/start', {method:'POST'});

        await updateStatus();

    }catch(error){ showError('Không thể bắt đầu: ' + error.message); }

}



async function stopMotion(){

    try{

        await apiRequest('/api/stop', {method:'POST'});

        await updateStatus();

    }catch(error){ showError('Không thể dừng: ' + error.message); }

}



async function testForward(){

    try{

        await apiRequest('/api/test/forward', {method:'POST'});

        await updateStatus();

    }catch(error){ showError('Test thuận thất bại: ' + error.message); }

}



async function testReverse(){

    try{

        await apiRequest('/api/test/reverse', {method:'POST'});

        await updateStatus();

    }catch(error){ showError('Test nghịch thất bại: ' + error.message); }

}



async function saveConfig(){

    const config = readForm();

    const validationError = validateClientConfig(config);

    if (validationError) {

        showError(validationError);

        return;

    }



    const params = new URLSearchParams();

    params.append('stop', String(config.stop));

    params.append('forward', String(config.forward));

    params.append('reverse', String(config.reverse));

    params.append('forwardMs', String(config.forwardMs));

    params.append('reverseMs', String(config.reverseMs));

    params.append('pauseMs', String(config.pauseMs));

    params.append('cycles', String(config.cycles));



    try{

        const data = await apiRequest('/api/config', {

            method:'POST',

            headers:{'Content-Type':'application/x-www-form-urlencoded'},

            body:params.toString()

        });



        applyConfigToForm(data);

        showError('');

        alert('✅ Đã lưu cấu hình vào ESP32.');

        await updateStatus();

    }catch(error){

        showError('❌ Lưu cấu hình thất bại: ' + error.message);

        alert('❌ Không lưu được cấu hình: ' + error.message);

    }

}



async function factoryReset(){
    if(!confirm('Khôi phục cấu hình mặc định? Các giá trị đã lưu sẽ bị thay thế.')) return;
    try{
        const data = await apiRequest('/api/config/reset', {method:'POST'});
        applyConfigToForm(data);
        alert('✅ Đã khôi phục cấu hình mặc định.');
        await updateStatus();
    }catch(error){
        alert('❌ Không thể khôi phục: ' + error.message);
        showError('Khôi phục thất bại: ' + error.message);
    }
}

async function calibrate(direction){

    let value = Number(el('calibrationValue').innerText);

    if (!Number.isInteger(value)) value = 90;

    value += direction;

    value = Math.max(0, Math.min(180, value));



    el('calibrationValue').innerText = value;

    el('servoStop').value = value;

    setDirty(true);



    const params = new URLSearchParams();

    params.append('value', String(value));



    try{

        const data = await apiRequest('/api/calibration', {

            method:'POST',

            headers:{'Content-Type':'application/x-www-form-urlencoded'},

            body:params.toString()

        });

        el('calibrationValue').innerText = data.value;

        el('servoStop').value = data.value;

        setDirty(true);

    }catch(error){

        showError('Hiệu chỉnh thất bại: ' + error.message);

    }

}



['servoStop','servoForward','servoReverse','forwardMs','reverseMs','pauseMs','cyclesInput']

    .forEach(id => el(id).addEventListener('input', () => setDirty(true)));



showTab('overview');

loadConfig();

updateStatus();

setInterval(updateStatus, 500);

</script>

</body>

</html>

)rawliteral";
