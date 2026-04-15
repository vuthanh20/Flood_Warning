lucide.createIcons();

// ==========================================
// 1. CẤU HÌNH BẢN ĐỒ LEAFLET
// ==========================================
const map = L.map('map').setView([21.0285, 105.8542], 12);

L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
    attribution: '&copy; OpenStreetMap',
    subdomains: 'abcd',
    maxZoom: 20
}).addTo(map);

let mapMarkers = {};


// ==========================================
// 2. DANH SÁCH TRẠM VÀ TỌA ĐỘ
// ==========================================
let nodesData = { 
    "Hà Nội": [ 
        { id: "tram_do_1", en: "Node 01", vi: "Trạm Đo 1" },
        { id: "tram_do_2", en: "Node 02", vi: "Trạm Đo 2" }
    ] 
};

const nodeCoords = {
    "tram_do_1": { lat: 21.0285, lng: 105.8542 }, 
    "tram_do_2": { lat: 21.0385, lng: 105.7842 }  
};


// ==========================================
// 3. KẾT NỐI FIREBASE & CHART.JS
// ==========================================
const firebaseConfig = {
    databaseURL: "https://flood-c8eda-default-rtdb.asia-southeast1.firebasedatabase.app"
};
firebase.initializeApp(firebaseConfig);
const db = firebase.database();

const ctx = document.getElementById('waterChart').getContext('2d');
let waterChart = new Chart(ctx, {
    type: 'line',
    data: {
        labels: [], 
        datasets: [{
            data: [], 
            borderColor: '#6B4DFF',
            backgroundColor: 'rgba(107, 77, 255, 0.1)',
            borderWidth: 3,
            fill: true,
            tension: 0.4, 
            pointRadius: 5,         
            pointHoverRadius: 7,
            pointBackgroundColor: [], 
            pointBorderColor: []
        }]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: false, 
        plugins: { legend: { display: false }, tooltip: { enabled: true } },
        scales: {
            x: { 
                display: true, 
                grid: { display: false, drawBorder: false }, 
                ticks: { 
                    color: '#8E8F9F', 
                    maxTicksLimit: 6, 
                    maxRotation: 0, 
                    minRotation: 0,
                    font: { size: 11 }
                }
            }, 
            y: { display: false, min: 0 } 
        },
        layout: { padding: 0 }
    }
});

const xAxisLabelsContainer = document.getElementById('x-axis-labels');
if (xAxisLabelsContainer) xAxisLabelsContainer.style.display = 'none';


// ==========================================
// 4. LẮNG NGHE NGƯỠNG CẢNH BÁO TỪ CÀI ĐẶT
// ==========================================
let nodeThresholds = {}; 
let alertSettings = {}; 

db.ref('nguong_canh_bao').on('value', (snapshot) => {
    nodeThresholds = snapshot.val() || {};
    
    const settingsNodeSelect = document.getElementById('settings-node-select');
    if(settingsNodeSelect && document.getElementById('settings-view').classList.contains('block')) {
        loadThresholdsForSettings();
    }
    
    const currentProvSelected = document.getElementById('province-select') ? document.getElementById('province-select').value : 'all';
    const currentNodeSelected = document.getElementById('node-select') ? document.getElementById('node-select').value : null;
    if(currentNodeSelected) renderChartAndTable(currentProvSelected, currentNodeSelected);
});

const settingsNodeSelect = document.getElementById('settings-node-select');
const inputWarn = document.getElementById('input-warn-thresh');
const inputDanger = document.getElementById('input-danger-thresh');
const btnSaveSettings = document.getElementById('btn-save-settings');
const msgSaved = document.getElementById('settings-success-msg');

function populateSettingsDropdown(lang) {
    if(!settingsNodeSelect) return;
    settingsNodeSelect.innerHTML = '';
    for(let prov in nodesData) {
        nodesData[prov].forEach(node => {
            const opt = document.createElement('option');
            opt.value = node.id;
            opt.innerText = lang === 'en' ? node.en : node.vi;
            settingsNodeSelect.appendChild(opt);
        });
    }
    loadThresholdsForSettings();
}

function loadThresholdsForSettings() {
    const nId = settingsNodeSelect.value;
    if(!nId) return;
    let w_thresh = (nodeThresholds[nId] && nodeThresholds[nId].canh_bao !== undefined) ? nodeThresholds[nId].canh_bao : 25;
    let d_thresh = (nodeThresholds[nId] && nodeThresholds[nId].nguy_hiem !== undefined) ? nodeThresholds[nId].nguy_hiem : 40;
    inputWarn.value = w_thresh;
    inputDanger.value = d_thresh;
}

if(settingsNodeSelect) {
    settingsNodeSelect.addEventListener('change', loadThresholdsForSettings);
}

if(btnSaveSettings) {
    btnSaveSettings.addEventListener('click', () => {
        const nId = settingsNodeSelect.value;
        const w_val = parseFloat(inputWarn.value);
        const d_val = parseFloat(inputDanger.value);
        if(nId) {
            db.ref(`nguong_canh_bao/${nId}`).set({
                canh_bao: w_val,
                nguy_hiem: d_val
            }).then(() => {
                msgSaved.classList.remove('hidden');
                setTimeout(() => msgSaved.classList.add('hidden'), 3000);
            });
        }
    });
}


// ==========================================
// 5. NÚT GẠT BẬT TẮT CẢNH BÁO
// ==========================================
function startAlertListener() {
    db.ref('canh_bao').on('value', (snapshot) => {
        alertSettings = snapshot.val() || {};
        const currentNodeSelected = document.getElementById('node-select') ? document.getElementById('node-select').value : null;
        if(currentNodeSelected) updateAlertToggleUI(currentNodeSelected);
    });
}
startAlertListener();

function updateAlertToggleUI(nodeId) {
    const toggle = document.getElementById('alert-toggle');
    const statusText = document.getElementById('alert-status-text');
    const alertIcon = document.getElementById('alert-icon');
    if(!toggle || !statusText || !alertIcon || !nodeId) return;

    let isAlertOn = alertSettings[nodeId] && alertSettings[nodeId].trang_thai === true;

    if(toggle.checked !== isAlertOn) toggle.checked = isAlertOn;
    
    statusText.innerText = isAlertOn ? "ON" : "OFF";
    if (isAlertOn) {
        statusText.className = "text-2xl font-medium tracking-tight text-[#FF4757] mb-0.5";
        alertIcon.style.color = '#FF4757';
    } else {
        statusText.className = "text-2xl font-medium tracking-tight text-[#8E8F9F] mb-0.5";
        alertIcon.style.color = '#8E8F9F';
    }
}

const alertToggleInput = document.getElementById('alert-toggle');
if (alertToggleInput) {
    alertToggleInput.addEventListener('change', (e) => {
        const isChecked = e.target.checked;
        const currentNodeSelected = document.getElementById('node-select') ? document.getElementById('node-select').value : null;
        if (currentNodeSelected) {
            db.ref(`canh_bao/${currentNodeSelected}/trang_thai`).set(isChecked);
        }
    });
}


// ==========================================
// 6. MAP, KPI & THEO DÕI OFFLINE
// ==========================================
function updateMapMarker(nodeId, displayName, lat, lng, waterLevel) {
    let w_thresh = (nodeThresholds[nodeId] && nodeThresholds[nodeId].canh_bao !== undefined) ? nodeThresholds[nodeId].canh_bao : 25;
    let d_thresh = (nodeThresholds[nodeId] && nodeThresholds[nodeId].nguy_hiem !== undefined) ? nodeThresholds[nodeId].nguy_hiem : 40;

    let bgColorClass = 'marker-safe';
    if (waterLevel >= d_thresh) bgColorClass = 'marker-danger';
    else if (waterLevel >= w_thresh) bgColorClass = 'marker-warning';

    let numberIcon = L.divIcon({
        className: `custom-map-marker ${bgColorClass}`,
        html: `<div>${waterLevel}</div>`,
        iconSize: [38, 38],
        iconAnchor: [19, 19]
    });

    if (mapMarkers[nodeId]) {
        mapMarkers[nodeId].setLatLng([lat, lng]);
        mapMarkers[nodeId].setIcon(numberIcon);
        mapMarkers[nodeId].setPopupContent(`<b>${displayName}</b><br>Mực nước: ${waterLevel}`);
    } else {
        mapMarkers[nodeId] = L.marker([lat, lng], {icon: numberIcon}).addTo(map);
        mapMarkers[nodeId].bindPopup(`<b>${displayName}</b><br>Mực nước: ${waterLevel}`);
    }
}

let latestLevels = {};

function updateKPIs(selectedProv, selectedNodeId) {
    if (!selectedNodeId) return;

    let level = latestLevels[selectedNodeId] !== undefined ? latestLevels[selectedNodeId] : "--";
    const waterKpi = document.getElementById('kpi-water-val');
    if (waterKpi) waterKpi.innerText = level;
    
    updateAlertToggleUI(selectedNodeId);
    updateSensorKPI(); 
}

function updateSensorKPI() {
    const selectedNodeId = document.getElementById('node-select') ? document.getElementById('node-select').value : null;
    const lang = document.getElementById('lang-switch') ? document.getElementById('lang-switch').value : 'en';
    const sensorVal = document.getElementById('kpi-sensors-val');
    const sensorIcon = document.getElementById('kpi-sensors-icon');

    if (!selectedNodeId || !sensorVal || !sensorIcon) return;

    let now = new Date().getTime();
    let isOnline = false;
    
    let nodeRecords = globalHistoryData.filter(r => r.nodeId === selectedNodeId);
    if (nodeRecords.length > 0) {
        let lastRecord = nodeRecords[nodeRecords.length - 1];
        if ((now - lastRecord.timestamp) <= 120000) { 
            isOnline = true;
        }
    }

    let text = isOnline ? (lang === 'en' ? "Online" : "Hoạt động") : (lang === 'en' ? "Offline" : "Mất K.nối");
    sensorVal.innerText = text;
    sensorVal.className = `text-3xl font-medium tracking-tight mb-0.5 ${isOnline ? 'text-[#20C997]' : 'text-[#FF4757]'}`;
    
    let colorClass = isOnline ? 'text-[#20C997]' : 'text-[#FF4757]';
    sensorIcon.setAttribute('class', `w-6 h-6 ${colorClass} lucide lucide-cpu`);
}

setInterval(updateSensorKPI, 5000);

function startLiveMapAndKPIs() {
    for (const province in nodesData) {
        nodesData[province].forEach(node => {
            db.ref(node.id).on('value', (snapshot) => {
                const data = snapshot.val();
                if(!data) return;
                latestLevels[node.id] = data.muc_nuoc;
                
                let coords = nodeCoords[node.id] || { lat: 21.0285, lng: 105.8542 };
                let lang = document.getElementById('lang-switch').value;
                let displayName = lang === 'en' ? node.en : node.vi;
                
                updateMapMarker(node.id, displayName, coords.lat, coords.lng, data.muc_nuoc);

                const currentProvSelected = document.getElementById('province-select') ? document.getElementById('province-select').value : 'all';
                const currentNodeSelected = document.getElementById('node-select') ? document.getElementById('node-select').value : null;
                if(currentNodeSelected === node.id) {
                    updateKPIs(currentProvSelected, currentNodeSelected);
                }
            });
        });
    }
}
startLiveMapAndKPIs();


// ==========================================
// 7. RENDER BẢNG LỊCH SỬ & BIỂU ĐỒ THEO TRẠM CỤ THỂ
// ==========================================
let globalHistoryData = [];

function getNodeName(id, lang) {
    for(let p in nodesData) {
        let found = nodesData[p].find(n => n.id === id);
        if(found) return lang === 'en' ? found.en : found.vi;
    }
    return id;
}

function getProvinceForNode(nodeId) {
    for (let prov in nodesData) {
        if (nodesData[prov].find(n => n.id === nodeId)) return prov;
    }
    return "Unknown";
}

function parseCustomDate(timeStr) {
    try {
        let parts = timeStr.split(" - ");
        let timeParts = parts[0].split(":");
        let dateParts = parts[1].split("/");
        return new Date(dateParts[2], dateParts[1]-1, dateParts[0], timeParts[0], timeParts[1], timeParts[2]).getTime();
    } catch(e) {
        return new Date().getTime();
    }
}

function startHistoryListener() {
    db.ref('lich_su').on('value', (snapshot) => {
        const data = snapshot.val();
        if (!data) return;

        let allRecords = [];
        Object.keys(data).forEach(nId => {
            let nodeData = data[nId];
            Object.keys(nodeData).forEach(key => {
                let record = nodeData[key];
                record.nodeId = nId;
                
                if (record.thoi_gian_rtc && !record.timestamp) {
                    record.timestamp = parseCustomDate(record.thoi_gian_rtc);
                } else if (!record.timestamp) {
                    record.timestamp = new Date().getTime(); 
                }
                
                allRecords.push(record);
            });
        });

        allRecords.sort((a, b) => a.timestamp - b.timestamp);
        globalHistoryData = allRecords;

        const currentProvSelected = document.getElementById('province-select') ? document.getElementById('province-select').value : 'all';
        const currentNodeSelected = document.getElementById('node-select') ? document.getElementById('node-select').value : null;
        if(currentNodeSelected) {
            renderChartAndTable(currentProvSelected, currentNodeSelected);
            updateSensorKPI(); 
        }
    });
}
startHistoryListener();


function renderChartAndTable(selectedProv, selectedNodeId) {
    if (!globalHistoryData.length || !selectedNodeId) return;
    let lang = document.getElementById('lang-switch').value;

    // --- 1. RENDER BẢNG (Chỉ cho trạm được chọn) ---
    let filteredTableRecords = globalHistoryData.filter(record => record.nodeId === selectedNodeId);
    let tableRecords = filteredTableRecords.slice(-100);
    let tableHtml = '';
    
    for (let i = tableRecords.length - 1; i >= 0; i--) {
        let record = tableRecords[i];
        
        let timeString = record.thoi_gian_rtc; 
        if (!timeString) {
            let dateObj = new Date(record.timestamp);
            timeString = dateObj.getHours().toString().padStart(2, '0') + ':' + dateObj.getMinutes().toString().padStart(2, '0');
        }

        let nodeProv = getProvinceForNode(record.nodeId);
        let displayProv = (lang === 'en') ? removeDiacritics(nodeProv) : nodeProv;
        let displayName = getNodeName(record.nodeId, lang);
        
        let w_thresh = (nodeThresholds[record.nodeId] && nodeThresholds[record.nodeId].canh_bao !== undefined) ? nodeThresholds[record.nodeId].canh_bao : 25;
        let d_thresh = (nodeThresholds[record.nodeId] && nodeThresholds[record.nodeId].nguy_hiem !== undefined) ? nodeThresholds[record.nodeId].nguy_hiem : 40;
        
        let statusText = record.trang_thai || "An toan";
        let statusClass = 'bg-[#20C997]/10 text-[#20C997]'; 
        
        if (record.muc_nuoc >= d_thresh) {
            statusClass = 'bg-[#FF4757]/10 text-[#FF4757]';
        } else if (record.muc_nuoc >= w_thresh) {
            statusClass = 'bg-[#F59E0B]/10 text-[#F59E0B]';
        }

        tableHtml += `
            <tr class="border-b border-[#8E8F9F]/10 hover:bg-white/5 transition-colors">
                <td class="py-4 whitespace-nowrap">${timeString}</td>
                <td class="py-4 font-medium text-[#8E8F9F]">${displayProv}</td>
                <td class="py-4">${displayName}</td>
                <td class="py-4 text-[#20C997] font-medium">${record.muc_nuoc}</td>
                <td class="py-4"><span class="px-3 py-1 rounded-full ${statusClass} text-xs font-medium">${statusText}</span></td>
            </tr>
        `;
    }
    const tableBody = document.getElementById('history-table-body');
    if(tableBody) tableBody.innerHTML = tableHtml;

    // --- 2. RENDER BIỂU ĐỒ (Chỉ cho trạm được chọn) ---
    const chartTimeSelect = document.getElementById('chart-time-select');
    const timeRange = chartTimeSelect ? chartTimeSelect.value : '24h';
    
    let now = new Date().getTime();
    let cutoffTime = 0;
    if (timeRange === '12h') cutoffTime = now - 12 * 3600 * 1000;
    else if (timeRange === '24h') cutoffTime = now - 24 * 3600 * 1000;
    else if (timeRange === '1w') cutoffTime = now - 7 * 24 * 3600 * 1000;
    else if (timeRange === '1m') cutoffTime = now - 30 * 24 * 3600 * 1000;

    let chartRecords = filteredTableRecords.filter(r => r.timestamp >= cutoffTime).slice(-50); 
    
    let times = [];
    let levels = [];
    let pointColors = [];

    let w_thresh = (nodeThresholds[selectedNodeId] && nodeThresholds[selectedNodeId].canh_bao !== undefined) ? nodeThresholds[selectedNodeId].canh_bao : 25;
    let d_thresh = (nodeThresholds[selectedNodeId] && nodeThresholds[selectedNodeId].nguy_hiem !== undefined) ? nodeThresholds[selectedNodeId].nguy_hiem : 40;

    chartRecords.forEach(record => {
        let chartTimeLabel = "";
        if (record.thoi_gian_rtc) {
            let parts = record.thoi_gian_rtc.split(" - ");
            if(parts.length === 2) chartTimeLabel = [parts[0], parts[1]]; 
            else chartTimeLabel = record.thoi_gian_rtc;
        }

        times.push(chartTimeLabel);
        levels.push(record.muc_nuoc);

        if (record.muc_nuoc >= d_thresh) pointColors.push('#FF4757'); 
        else if (record.muc_nuoc >= w_thresh) pointColors.push('#F59E0B'); 
        else pointColors.push('#20C997'); 
    });

    let maxDataLevel = levels.length > 0 ? Math.max(...levels) : 6;
    let maxLevel = Math.max(maxDataLevel, 6); 
    maxLevel = Math.ceil(maxLevel / 3) * 3;

    const y4 = document.getElementById('y-label-4');
    const y3 = document.getElementById('y-label-3');
    const y2 = document.getElementById('y-label-2');

    if (y4) y4.innerText = maxLevel;
    if (y3) y3.innerText = parseFloat((maxLevel * 2 / 3).toFixed(1));
    if (y2) y2.innerText = parseFloat((maxLevel / 3).toFixed(1));

    waterChart.options.scales.y.max = maxLevel;
    waterChart.data.labels = times;
    waterChart.data.datasets[0].data = levels;
    
    waterChart.data.datasets[0].pointBackgroundColor = pointColors;
    waterChart.data.datasets[0].pointBorderColor = pointColors;
    waterChart.data.datasets[0].borderColor = '#8E8F9F'; 
    
    waterChart.update('none'); 
}

const chartTimeSelect = document.getElementById('chart-time-select');
if(chartTimeSelect) {
    chartTimeSelect.addEventListener('change', (e) => {
        const currentProvSelected = document.getElementById('province-select') ? document.getElementById('province-select').value : 'all';
        const currentNodeSelected = document.getElementById('node-select') ? document.getElementById('node-select').value : null;
        if(currentNodeSelected) renderChartAndTable(currentProvSelected, currentNodeSelected);
    });
}


// ==========================================
// 8. ĐIỀU HƯỚNG VÀ NGÔN NGỮ
// ==========================================
const i18n = {
    en: {
        "nav-dashboard": "Dashboard", "nav-history": "History", "nav-map": "Node Map", "nav-settings": "Settings",
        "opt-all-prov": "All Provinces",
        "lang-en": "English", "lang-vi": "Vietnamese",
        "title-dashboard": "Dashboard", "title-history": "History", "title-map": "Node Map", "title-settings": "Settings",
        "kpi-alerts": "Alert Control", "kpi-water": "Current Level", "kpi-rain": "24h Rainfall", "kpi-sensors": "Sensor Status",
        "chart-title": "Water Level Trends", 
        "chart-12h": "12 hours", "chart-24h": "24 hours", "chart-1w": "1 week", "chart-1m": "1 month",
        "btn-export": "Export Data", "th-date": "Date & Time", "th-province": "Province", "th-node": "Node ID", "th-level": "Water Level",
        "th-status": "Status",
        "title-sys-config": "Threshold Configuration", "lbl-select-node": "Select Node to Configure",
        "lbl-warn-threshold": "Warning Threshold (cm)", "lbl-danger-threshold": "Danger Threshold (cm)",
        "btn-save": "Save Changes", "msg-saved": "Saved successfully!"
    },
    vi: {
        "nav-dashboard": "Bảng Điều Khiển", "nav-history": "Lịch Sử", "nav-map": "Bản Đồ Trạm", "nav-settings": "Cài Đặt",
        "opt-all-prov": "Tất Cả Tỉnh Thành",
        "lang-en": "Tiếng Anh", "lang-vi": "Tiếng Việt",
        "title-dashboard": "Bảng Điều Khiển", "title-history": "Lịch Sử", "title-map": "Bản Đồ Trạm", "title-settings": "Cài Đặt",
        "kpi-alerts": "Bật/Tắt Cảnh Báo", "kpi-water": "Mực Nước Hiện Tại", "kpi-rain": "Lượng Mưa 24h", "kpi-sensors": "Trạng Thái Cảm Biến",
        "chart-title": "Xu Hướng Mực Nước", 
        "chart-12h": "12 giờ", "chart-24h": "24 giờ", "chart-1w": "1 tuần", "chart-1m": "1 tháng",
        "btn-export": "Xuất Dữ Liệu", "th-date": "Ngày & Giờ", "th-province": "Tỉnh Thành", "th-node": "Mã Trạm", "th-level": "Mực Nước",
        "th-status": "Trạng Thái",
        "title-sys-config": "Cấu Hình Ngưỡng", "lbl-select-node": "Chọn Trạm để Cài đặt",
        "lbl-warn-threshold": "Ngưỡng Cảnh báo (cm)", "lbl-danger-threshold": "Ngưỡng Nguy hiểm (cm)",
        "btn-save": "Lưu Thay Đổi", "msg-saved": "Đã lưu cấu hình!"
    }
};

const navLinks = document.querySelectorAll('.nav-link');
const views = document.querySelectorAll('.view-section');

navLinks.forEach(link => {
    link.addEventListener('click', (e) => {
        e.preventDefault();
        const targetId = link.getAttribute('data-target');
        views.forEach(view => {
            if (view.id === targetId) {
                view.classList.remove('hidden');
                if(targetId === 'node-map-view') {
                    view.classList.add('flex');
                    setTimeout(() => { map.invalidateSize(); }, 100); 
                } else {
                    view.classList.add('block');
                }
            } else {
                view.classList.add('hidden');
                view.classList.remove('block', 'flex');
            }
        });

        navLinks.forEach(nav => {
            nav.classList.remove('text-white');
            nav.classList.add('text-[#8E8F9F]');
            nav.querySelector('.nav-indicator').classList.replace('opacity-100', 'opacity-0');
            const icon = nav.querySelector('.nav-icon');
            if(icon) icon.classList.remove('text-[#6B4DFF]');
        });

        link.classList.remove('text-[#8E8F9F]');
        link.classList.add('text-white');
        link.querySelector('.nav-indicator').classList.replace('opacity-0', 'opacity-100');
        const activeIcon = link.querySelector('.nav-icon');
        if(activeIcon) activeIcon.classList.add('text-[#6B4DFF]');
    });
});

const nodeSelect = document.getElementById('node-select');
const langSwitch = document.getElementById('lang-switch');
const provinceSelect = document.getElementById('province-select');

// XÓA MỤC ALL NODES
function updateNodeDropdown(provinceVal, lang) {
    if(!nodeSelect) return;
    nodeSelect.innerHTML = ''; 

    let nodesToShow = [];
    if (provinceVal === 'all') {
        for(let p in nodesData) {
            nodesToShow = nodesToShow.concat(nodesData[p]);
        }
    } else if (nodesData[provinceVal]) {
        nodesToShow = nodesData[provinceVal];
    }

    nodesToShow.forEach(node => {
        const opt = document.createElement('option');
        opt.value = node.id;
        opt.innerText = lang === 'en' ? node.en : node.vi;
        nodeSelect.appendChild(opt);
    });

    if (nodesToShow.length > 0) {
        // Tự động chọn trạm đầu tiên trong danh sách
        nodeSelect.value = nodesToShow[0].id;
        const currentProvSelected = document.getElementById('province-select') ? document.getElementById('province-select').value : 'all';
        renderChartAndTable(currentProvSelected, nodeSelect.value);
        updateKPIs(currentProvSelected, nodeSelect.value);
    } else {
        waterChart.data.labels = [];
        waterChart.data.datasets[0].data = [];
        waterChart.update('none');
        
        const tb = document.getElementById('history-table-body');
        if(tb) tb.innerHTML = '';
        
        const waterKpi = document.getElementById('kpi-water-val');
        if (waterKpi) waterKpi.innerText = '--';
        
        const sensorVal = document.getElementById('kpi-sensors-val');
        if (sensorVal) sensorVal.innerText = '--';
    }
}

if (nodeSelect) {
    nodeSelect.addEventListener('change', (e) => {
        const currentProvSelected = document.getElementById('province-select') ? document.getElementById('province-select').value : 'all';
        const selectedNodeId = e.target.value;
        renderChartAndTable(currentProvSelected, selectedNodeId);
        updateKPIs(currentProvSelected, selectedNodeId);
    });
}

if(provinceSelect) {
    provinceSelect.addEventListener('change', (e) => {
        updateNodeDropdown(e.target.value, langSwitch.value);
    });
    Array.from(provinceSelect.options).forEach(opt => {
        if (opt.value !== 'all') opt.setAttribute('data-vi-name', opt.innerText);
    });
}

function removeDiacritics(str) {
    return str.normalize("NFD").replace(/[\u0300-\u036f]/g, "").replace(/đ/g, "d").replace(/Đ/g, "D");
}

function updateLanguage(lang) {
    document.querySelectorAll('[data-i18n]').forEach(el => {
        const key = el.getAttribute('data-i18n');
        if (i18n[lang] && i18n[lang][key]) el.innerText = i18n[lang][key];
    });

    if(provinceSelect) {
        Array.from(provinceSelect.options).forEach(opt => {
            if (opt.value !== 'all') {
                const viName = opt.getAttribute('data-vi-name');
                opt.innerText = lang === 'en' ? removeDiacritics(viName) : viName;
            }
        });
        updateNodeDropdown(provinceSelect.value, lang);
    }
    
    populateSettingsDropdown(lang);
}

if(langSwitch) {
    langSwitch.addEventListener('change', (e) => updateLanguage(e.target.value));
}

updateLanguage('en');