#include "web_server.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "adc_sensors.h"

extern float waterLevel, pHvalue, tdsValue, waterTemp, voltage;
extern String waterQuality;
extern SemaphoreHandle_t xDataMutex;
extern bool sensorOLEDEnabled;

extern WaterQualityThresholds thresholds;

// 历史数据环形缓冲区
#define MAX_HISTORY 100
struct DataRecord {
    unsigned long timestamp;
    float waterLevel;
    float pHvalue;
    float tds;
    float waterTemp;
    float voltage;
    char quality[4];
};
DataRecord history[MAX_HISTORY];
int historyIndex = 0;
int historyCount = 0;

WebServer server(80);

void addRecord(float wl, float ph, float tds, float wt, float volt, String quality) {
    if (xSemaphoreTake(xDataMutex, portMAX_DELAY) == pdTRUE) {
        history[historyIndex].timestamp = millis() / 1000;
        history[historyIndex].waterLevel = wl;
        history[historyIndex].pHvalue = ph;
        history[historyIndex].tds = tds;
        history[historyIndex].waterTemp = wt;
        history[historyIndex].voltage = volt;
        strncpy(history[historyIndex].quality, quality.c_str(), 3);
        history[historyIndex].quality[3] = '\0';
        historyIndex = (historyIndex + 1) % MAX_HISTORY;
        if (historyCount < MAX_HISTORY) historyCount++;
        xSemaphoreGive(xDataMutex);
    }
}

void initWiFi(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
}

void handleRoot() {
    String html = R"raw(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>智能水情监测系统</title>
  <style>
    body { font-family: Arial; margin:20px; }
    button, input { margin:4px; padding:6px; }
    table { border-collapse: collapse; width:100%; margin-top:10px; }
    th, td { border:1px solid #ddd; padding:6px; }
    .section { border:1px solid #ccc; margin:15px 0; padding:10px; border-radius:8px; }
  </style>
  <script>
    let pollingInterval = null;
    let timeoutId = null;
    const AUTO_STOP_SECONDS = 60;

    function fetchData() {
      fetch('/data')
        .then(response=>response.json())
        .then(data=>{
          document.getElementById('water').innerText = data.waterLevel.toFixed(2);
          document.getElementById('ph').innerText = data.pHvalue.toFixed(2);
          document.getElementById('tds').innerText = data.tds;
          document.getElementById('temp').innerText = data.waterTemp.toFixed(1);
          document.getElementById('volt').innerText = data.voltage.toFixed(2);
          document.getElementById('quality').innerText = data.quality;
          updateQualityIcon(data.quality);
        })
        .catch(err=>console.log(err));
    }

    function updateQualityIcon(quality) {
      let icon = '';
      if (quality === '优') icon = '😊';
      else if (quality === '良') icon = '😐';
      else if (quality === '差') icon = '😟';
      document.getElementById('qualityIcon').innerText = icon;
    }

    function startMonitoring() {
      if(pollingInterval) return;
      fetchData();
      pollingInterval = setInterval(fetchData,1000);
      if(timeoutId) clearTimeout(timeoutId);
      timeoutId = setTimeout(stopMonitoring, AUTO_STOP_SECONDS*1000);
      document.getElementById('startBtn').disabled=true;
      document.getElementById('stopBtn').disabled=false;
    }
    function stopMonitoring() {
      if(pollingInterval) clearInterval(pollingInterval);
      if(timeoutId) clearTimeout(timeoutId);
      pollingInterval=null;
      document.getElementById('startBtn').disabled=false;
      document.getElementById('stopBtn').disabled=true;
    }

    function enableSystem() { fetch('/enable',{method:'POST'}); }
    function disableSystem() { fetch('/disable',{method:'POST'}); }

    function setThresholds() {
      let params = new URLSearchParams({
        waterAlarmLevel: document.getElementById('waterAlarmLevel').value,
        ph_you_min: document.getElementById('ph_you_min').value,
        ph_you_max: document.getElementById('ph_you_max').value,
        ph_liang_min: document.getElementById('ph_liang_min').value,
        ph_liang_max: document.getElementById('ph_liang_max').value,
        tds_you_min: document.getElementById('tds_you_min').value,
        tds_you_max: document.getElementById('tds_you_max').value,
        tds_liang_min: document.getElementById('tds_liang_min').value,
        tds_liang_max: document.getElementById('tds_liang_max').value
      });

      fetch('/set_thresholds', { method: 'POST', body: params })
        .then(response => {
          if (response.ok) {
            alert('阈值已更新');
            return;
          }
          // 尝试获取错误文本
          return response.text().then(text => {
            throw new Error(text || `服务器错误 (${response.status})`);
          });
        })
        .catch(err => {
          console.error('设置阈值失败:', err);
          alert('设置失败: ' + (err.message || '未知错误，请检查网络或阈值范围'));
        });
    }

    function setVoltageOffset() {
      let offset = document.getElementById('voltageOffset').value;
      fetch('/set_voltage_offset?offset=' + offset, {method:'POST'})
        .then(()=>alert('电压校准值已设置'))
        .catch(err=>alert('设置失败'));
    }

    function queryHistory() {
      let cnt = document.getElementById('queryCount').value;
      fetch('/history?count='+cnt)
        .then(res=>res.json())
        .then(data=>{
          let html='<table border="1"><tr><th>时间(s)</th><th>水位(mm)</th><th>pH</th><th>TDS(ppm)</th><th>水温(°C)</th><th>电压(V)</th><th>水质</th></tr>';
          for(let d of data) {
            html+=`<tr><td>${d.ts}</td><td>${d.water.toFixed(2)}</td><td>${d.ph.toFixed(2)}</td><td>${d.tds}</td><td>${d.temp.toFixed(1)}</td><td>${d.volt.toFixed(2)}</td><td>${d.quality}${d.quality==='优'?'😊':(d.quality==='良'?'😐':'😟')}</td></tr>`;
          }
          html+='</table>';
          document.getElementById('historyTable').innerHTML=html;
        });
    }
  </script>
</head>
<body>
<h1>智能水情监测系统</h1>
<div class="section">
  <button onclick="enableSystem()">🔌 打开系统(OLED+电压+水温)</button>
  <button onclick="disableSystem()">⛔ 关闭系统</button>
</div>
<div class="section">
  <button id="startBtn" onclick="startMonitoring()">📊 查看实时数据</button>
  <button id="stopBtn" onclick="stopMonitoring()" disabled>⏹️ 关闭查询</button>
  <span>（超时自动关闭60秒）</span>
</div>
<h2>实时数据</h2>
<p>💧水位: <span id="water">0.0</span> mm &nbsp; 🧪pH: <span id="ph">0.00</span><br>
🐟TDS: <span id="tds">0</span> ppm &nbsp; ⚠️水质: <span id="quality">-</span> <span id="qualityIcon"></span><br>
🌡️水温: <span id="temp">0.0</span> °C &nbsp; 🔋电压: <span id="volt">0.00</span> V</p>

<div class="section">
  <h3>水质 & 水位 阈值设置</h3>
  <label>水位报警阈值(mm): <input type="number" step="1" id="waterAlarmLevel" value="20"></label><br>
  <label>优等pH范围: <input type="number" step="0.1" id="ph_you_min" value="6.5"> ~ <input type="number" step="0.1" id="ph_you_max" value="8.5"></label><br>
  <label>良等pH范围: <input type="number" step="0.1" id="ph_liang_min" value="5.5"> ~ <input type="number" step="0.1" id="ph_liang_max" value="9.5"></label><br>
  <label>优等TDS范围: <input type="number" step="10" id="tds_you_min" value="0"> ~ <input type="number" step="10" id="tds_you_max" value="300"> ppm</label><br>
  <label>良等TDS范围: <input type="number" step="10" id="tds_liang_min" value="300"> ~ <input type="number" step="10" id="tds_liang_max" value="600"> ppm</label><br>
  <button onclick="setThresholds()">应用阈值</button>
  <p style="font-size:12px; color:gray;">注：超出良的范围即为“差”，水质差会触发报警。</p>
</div>

<div class="section">
  <h3>电压校准</h3>
  <label>校准偏移量(V): <input type="number" step="0.1" id="voltageOffset" value="0.0"></label>
  <button onclick="setVoltageOffset()">设置偏移</button>
  <p style="font-size:12px;">实际电压 = 测量值 + 偏移量（可正可负）</p>
</div>

<h2>历史数据查询</h2>
<label>最近条数: <input type="number" id="queryCount" value="10" min="1" max="100"></label>
<button onclick="queryHistory()">查询</button>
<div id="historyTable"></div>
</body>
</html>
    )raw";
    server.send(200, "text/html", html);
}

void handleData() {
    float localWater, localPH, localTDS, localTemp, localVolt;
    String localQuality;
    if (xSemaphoreTake(xDataMutex, portMAX_DELAY) == pdTRUE) {
        localWater = waterLevel;
        localPH = pHvalue;
        localTDS = tdsValue;
        localTemp = waterTemp;
        localVolt = voltage;
        localQuality = waterQuality;
        xSemaphoreGive(xDataMutex);
    }
    String json = "{";
    json += "\"waterLevel\":" + String(localWater, 2) + ",";
    json += "\"pHvalue\":" + String(localPH, 2) + ",";
    json += "\"tds\":" + String(localTDS, 0) + ",";
    json += "\"waterTemp\":" + String(localTemp, 1) + ",";
    json += "\"voltage\":" + String(localVolt, 2) + ",";
    json += "\"quality\":\"" + localQuality + "\"";
    json += "}";
    server.send(200, "application/json", json);
}

void handleHistory() {
    int n = server.arg("count").toInt();
    if (n <= 0 || n > MAX_HISTORY) n = 10;
    if (xSemaphoreTake(xDataMutex, portMAX_DELAY) != pdTRUE) {
        server.send(500, "application/json", "{\"error\":\"lock failed\"}");
        return;
    }
    n = min(n, historyCount);
    String json = "[";
    int newestIdx = (historyIndex == 0) ? MAX_HISTORY - 1 : historyIndex - 1;
    for (int i = 0; i < n; i++) {
        int idx = (newestIdx - i + MAX_HISTORY) % MAX_HISTORY;
        if (i > 0) json += ",";
        json += "{\"ts\":" + String(history[idx].timestamp) +
                ",\"water\":" + String(history[idx].waterLevel, 2) +
                ",\"ph\":" + String(history[idx].pHvalue, 2) +
                ",\"tds\":" + String(history[idx].tds, 0) +
                ",\"temp\":" + String(history[idx].waterTemp, 1) +
                ",\"volt\":" + String(history[idx].voltage, 2) +
                ",\"quality\":\"" + String(history[idx].quality) + "\"}";
    }
    json += "]";
    xSemaphoreGive(xDataMutex);
    server.send(200, "application/json", json);
}

void handleEnable() {
    if (xSemaphoreTake(xDataMutex, portMAX_DELAY) == pdTRUE) {
        sensorOLEDEnabled = true;
        xSemaphoreGive(xDataMutex);
    }
    server.send(200, "text/plain", "OK");
}

void handleDisable() {
    if (xSemaphoreTake(xDataMutex, portMAX_DELAY) == pdTRUE) {
        sensorOLEDEnabled = false;
        xSemaphoreGive(xDataMutex);
    }
    server.send(200, "text/plain", "OK");
}

void handleSetThresholds() {
    // 第一步：检查所有必需参数是否存在
    if (!server.hasArg("waterAlarmLevel") ||
        !server.hasArg("ph_you_min") || !server.hasArg("ph_you_max") ||
        !server.hasArg("ph_liang_min") || !server.hasArg("ph_liang_max") ||
        !server.hasArg("tds_you_min") || !server.hasArg("tds_you_max") ||
        !server.hasArg("tds_liang_min") || !server.hasArg("tds_liang_max")) {
        server.send(400, "text/plain", "Missing parameters");
        return;
    }

    // 第二步：转换为临时变量
    float waterAlarmLevel = server.arg("waterAlarmLevel").toFloat();
    float ph_you_min = server.arg("ph_you_min").toFloat();
    float ph_you_max = server.arg("ph_you_max").toFloat();
    float ph_liang_min = server.arg("ph_liang_min").toFloat();
    float ph_liang_max = server.arg("ph_liang_max").toFloat();
    float tds_you_min = server.arg("tds_you_min").toFloat();
    float tds_you_max = server.arg("tds_you_max").toFloat();
    float tds_liang_min = server.arg("tds_liang_min").toFloat();
    float tds_liang_max = server.arg("tds_liang_max").toFloat();

    // 第三步：基本合法性检查（min <= max）
    if (ph_you_min > ph_you_max || ph_liang_min > ph_liang_max ||
        tds_you_min > tds_you_max || tds_liang_min > tds_liang_max) {
        server.send(400, "text/plain", "Invalid range: min > max");
        return;
    }

    // 第四步：物理范围合理性检查
    if (ph_you_min < 0 || ph_you_max > 14 || ph_liang_min < 0 || ph_liang_max > 14) {
        server.send(400, "text/plain", "pH must be between 0 and 14");
        return;
    }
    if (tds_you_min < 0 || tds_liang_min < 0) {
        server.send(400, "text/plain", "TDS cannot be negative");
        return;
    }
    if (waterAlarmLevel < 0) {
        server.send(400, "text/plain", "Water alarm level cannot be negative");
        return;
    }

    // 第五步：范围关系检查
    if (ph_liang_min > ph_you_min || ph_liang_max < ph_you_max) {
        server.send(400, "text/plain", "良pH范围必须完全包含优pH范围 (良_min ≤ 优_min 且 良_max ≥ 优_max)");
        return;
    }
    if (tds_liang_min < tds_you_max) {
        server.send(400, "text/plain", "良TDS范围必须完全大于优TDS范围,即良_min ≥ 优_max");
        return;
    }

    // 第六步：加锁并更新全局阈值
    if (xSemaphoreTake(xDataMutex, portMAX_DELAY) == pdTRUE) {
        thresholds.waterAlarmLevel = waterAlarmLevel;
        thresholds.ph_you_min = ph_you_min;
        thresholds.ph_you_max = ph_you_max;
        thresholds.ph_liang_min = ph_liang_min;
        thresholds.ph_liang_max = ph_liang_max;
        thresholds.tds_you_min = tds_you_min;
        thresholds.tds_you_max = tds_you_max;
        thresholds.tds_liang_min = tds_liang_min;
        thresholds.tds_liang_max = tds_liang_max;
        xSemaphoreGive(xDataMutex);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(500, "text/plain", "Lock failed");
    }
}

void handleSetVoltageOffset() {
    if (server.hasArg("offset")) {
        float offset = server.arg("offset").toFloat();
        setVoltageOffset(offset);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing offset");
    }
}

void setupWebServer() {
    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.on("/history", handleHistory);
    server.on("/enable", HTTP_POST, handleEnable);
    server.on("/disable", HTTP_POST, handleDisable);
    server.on("/set_thresholds", HTTP_POST, handleSetThresholds);
    server.on("/set_voltage_offset", HTTP_POST, handleSetVoltageOffset);
    server.begin();
    Serial.println("HTTP server started");
}