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
            pointRadius: 0, 
            pointHoverRadius: 6
        }]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: false, 
        plugins: { legend: { display: false }, tooltip: { enabled: true } },
        scales: {
            x: { display: false }, 
            y: { display: false, min: 0 } 
        },
        layout: { padding: 0 }
    }
});


// ==========================================
// 4. CÁC HÀM CẬP NHẬT GIAO DIỆN (UI)
// ==========================================
function updateMapMarker(nodeId, displayName, lat, lng, waterLevel) {
    let bgColorClass = 'marker-safe';
    
    if (waterLevel >= 40) bgColorClass = 'marker-danger';
    else if (waterLevel >= 25) bgColorClass = 'marker-warning';

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
let latestStatuses = {};

function updateKPIs(selectedProv, selectedNodeId) {
    let totalAlerts = 0;
    let totalWater = 0;
    let count = 0;

    if (selectedNodeId === 'all') {
        for (let k in latestLevels) {
            let nodeProv = getProvinceForNode(k);
            if (selectedProv === 'all' || nodeProv === selectedProv) {
                totalWater += latestLevels[k];
                count++;
                let st = latestStatuses[k] ? latestStatuses[k].toLowerCase() : "an toan";
                if (!st.includes('an toan') && !st.includes('an toàn')) totalAlerts++;
            }
        }
        let avgWater = count > 0 ? (totalWater / count).toFixed(1) : 0;
        const waterKpi = document.getElementById('kpi-water-val');
        if (waterKpi) waterKpi.innerText = avgWater;

        const alertsKpi = document.getElementById('kpi-alerts-val');
        if (alertsKpi) alertsKpi.innerText = totalAlerts;
    } else {
        let level = latestLevels[selectedNodeId] !== undefined ? latestLevels[selectedNodeId] : "--";
        let st = latestStatuses[selectedNodeId] ? latestStatuses[selectedNodeId].toLowerCase() : "an toan";
        let alertCount = (!st.includes('an toan') && !st.includes('an toàn')) ? 1 : 0;

        const waterKpi = document.getElementById('kpi-water-val');
        if (waterKpi) waterKpi.innerText = level;

        const alertsKpi = document.getElementById('kpi-alerts-val');
        if (alertsKpi) alertsKpi.innerText = alertCount;
    }
}


// ==========================================
// 5. LẮNG NGHE FIREBASE (LIVE MAP & KPI TỔNG)
// ==========================================
function startLiveMapAndKPIs() {
    for (const province in nodesData) {
        nodesData[province].forEach(node => {
            db.ref(node.id).on('value', (snapshot) => {
                const data = snapshot.val();
                if(!data) return;
                
                latestLevels[node.id] = data.muc_nuoc;
                latestStatuses[node.id] = data.trang_thai || "An toan";
                
                let coords = nodeCoords[node.id] || { lat: 21.0285, lng: 105.8542 };
                let lang = document.getElementById('lang-switch').value;
                let displayName = lang === 'en' ? node.en : node.vi;
                
                updateMapMarker(node.id, displayName, coords.lat, coords.lng, data.muc_nuoc);

                const currentProvSelected = document.getElementById('province-select') ? document.getElementById('province-select').value : 'all';
                const currentNodeSelected = document.getElementById('node-select') ? document.getElementById('node-select').value : 'all';
                updateKPIs(currentProvSelected, currentNodeSelected);
            });
        });
    }
}
startLiveMapAndKPIs();


// ==========================================
// 6. LẮNG NGHE LỊCH SỬ TOÀN HỆ THỐNG
// ==========================================
let globalHistoryData = [];

// Hàm hỗ trợ tìm kiếm Tên Trạm và Tỉnh theo ID
function getNodeName(id, lang) {
    for(let p in nodesData) {
        let found = nodesData[p].find(n => n.id === id);
        if(found) return lang === 'en' ? found.en : found.vi;
    }
    return id;
}

function getProvinceForNode(nodeId) {
    for (let prov in nodesData) {
        if (nodesData[prov].find(n => n.id === nodeId)) {
            return prov;
        }
    }
    return "Unknown";
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
                allRecords.push(record);
            });
        });

        allRecords.sort((a, b) => a.timestamp - b.timestamp);
        globalHistoryData = allRecords;

        const currentProvSelected = document.getElementById('province-select') ? document.getElementById('province-select').value : 'all';
        const currentNodeSelected = document.getElementById('node-select') ? document.getElementById('node-select').value : 'all';
        renderChartAndTable(currentProvSelected, currentNodeSelected);
    });
}
startHistoryListener();


function renderChartAndTable(selectedProv, selectedNodeId) {
    if (!globalHistoryData.length) return;
    let lang = document.getElementById('lang-switch').value;

    // --- 1. LỌC VÀ RENDER BẢNG LỊCH SỬ TỔNG HỢP ---
    let filteredTableRecords = globalHistoryData.filter(record => {
        let nodeProv = getProvinceForNode(record.nodeId);
        let matchProv = (selectedProv === 'all' || nodeProv === selectedProv);
        let matchNode = (selectedNodeId === 'all' || record.nodeId === selectedNodeId);
        return matchProv && matchNode;
    });

    let tableRecords = filteredTableRecords.slice(-100);
    let tableHtml = '';
    
    for (let i = tableRecords.length - 1; i >= 0; i--) {
        let record = tableRecords[i];
        let dateObj = new Date(record.timestamp);
        let timeString = dateObj.getHours().toString().padStart(2, '0') + ':' + 
                         dateObj.getMinutes().toString().padStart(2, '0') + ':' + 
                         dateObj.getSeconds().toString().padStart(2, '0');

        let nodeProv = getProvinceForNode(record.nodeId);
        let displayProv = (lang === 'en') ? removeDiacritics(nodeProv) : nodeProv;
        let displayName = getNodeName(record.nodeId, lang);
        let statusText = record.trang_thai || "An toan";
        let statusClass = 'bg-[#20C997]/10 text-[#20C997]'; 
        
        let statusTextLower = statusText.toLowerCase();
        if (statusTextLower.includes('nguy hiem') || statusTextLower.includes('nguy hiểm')) {
            statusClass = 'bg-[#FF4757]/10 text-[#FF4757]';
        } else if (statusTextLower.includes('canh bao') || statusTextLower.includes('cảnh báo')) {
            statusClass = 'bg-[#F59E0B]/10 text-[#F59E0B]';
        }

        tableHtml += `
            <tr class="border-b border-[#8E8F9F]/10 hover:bg-white/5 transition-colors">
                <td class="py-4">${timeString}</td>
                <td class="py-4 font-medium text-[#8E8F9F]">${displayProv}</td>
                <td class="py-4">${displayName}</td>
                <td class="py-4 text-[#20C997] font-medium">${record.muc_nuoc}</td>
                <td class="py-4"><span class="px-3 py-1 rounded-full ${statusClass} text-xs font-medium">${statusText}</span></td>
            </tr>
        `;
    }
    const tableBody = document.getElementById('history-table-body');
    if(tableBody) tableBody.innerHTML = tableHtml;


    // --- 2. RENDER BIỂU ĐỒ (Chỉ vẽ nếu 1 trạm được chọn cụ thể) ---
    if (selectedNodeId === 'all') {
        // Xóa trắng biểu đồ nếu chọn "Tất Cả Trạm"
        waterChart.data.labels = [];
        waterChart.data.datasets[0].data = [];
        waterChart.update('none');
        
        const y4 = document.getElementById('y-label-4');
        const y3 = document.getElementById('y-label-3');
        const y2 = document.getElementById('y-label-2');
        if (y4) y4.innerText = "6m";
        if (y3) y3.innerText = "4m";
        if (y2) y2.innerText = "2m";
    } else {
        let chartRecords = globalHistoryData.filter(r => r.nodeId === selectedNodeId).slice(-50);
        
        let times = [];
        let levels = [];
        chartRecords.forEach(record => {
            let dateObj = new Date(record.timestamp);
            let timeString = dateObj.getHours().toString().padStart(2, '0') + ':' + 
                             dateObj.getMinutes().toString().padStart(2, '0') + ':' + 
                             dateObj.getSeconds().toString().padStart(2, '0');
            times.push(timeString);
            levels.push(record.muc_nuoc);
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
        waterChart.update('none'); 
    }
}


// ==========================================
// 7. CÁC HÀM CÒN LẠI (TỪ ĐIỂN, ĐIỀU HƯỚNG, TRỤC HOÀNH)
// ==========================================
function updateSensorCount() {
    let totalNodes = 0;
    for (const province in nodesData) totalNodes += nodesData[province].length;
    const sensorKpi = document.getElementById('kpi-sensors-val');
    if(sensorKpi) sensorKpi.innerText = totalNodes;
}
updateSensorCount();

const i18n = {
    en: {
        "nav-dashboard": "Dashboard", "nav-history": "History", "nav-map": "Node Map", "nav-settings": "Settings",
        "opt-all-prov": "All Provinces", "opt-all-nodes": "All Nodes",
        "lang-en": "English", "lang-vi": "Vietnamese",
        "title-dashboard": "Dashboard", "title-history": "History", "title-map": "Node Map", "title-settings": "Settings",
        "kpi-alerts": "Active Alerts", "kpi-water": "Current Level", "kpi-rain": "24h Rainfall", "kpi-sensors": "Active Sensors",
        "chart-title": "Water Level Trends", 
        "chart-12h": "12 hours", "chart-24h": "24 hours", "chart-1w": "1 week", "chart-1m": "1 month",
        "btn-export": "Export Data", "th-date": "Date & Time", "th-province": "Province", "th-node": "Node ID", "th-level": "Water Level",
        "th-status": "Status",
        "title-sys-config": "System Configuration", "lbl-threshold": "Flood Alert Threshold", "btn-save": "Save Changes"
    },
    vi: {
        "nav-dashboard": "Bảng Điều Khiển", "nav-history": "Lịch Sử", "nav-map": "Bản Đồ Trạm", "nav-settings": "Cài Đặt",
        "opt-all-prov": "Tất Cả Tỉnh Thành", "opt-all-nodes": "Tất Cả Trạm",
        "lang-en": "Tiếng Anh", "lang-vi": "Tiếng Việt",
        "title-dashboard": "Bảng Điều Khiển", "title-history": "Lịch Sử", "title-map": "Bản Đồ Trạm", "title-settings": "Cài Đặt",
        "kpi-alerts": "Cảnh Báo", "kpi-water": "Mực Nước", "kpi-rain": "Lượng Mưa 24h", "kpi-sensors": "Số Cảm Biến",
        "chart-title": "Xu Hướng Mực Nước", 
        "chart-12h": "12 giờ", "chart-24h": "24 giờ", "chart-1w": "1 tuần", "chart-1m": "1 tháng",
        "btn-export": "Xuất Dữ Liệu", "th-date": "Ngày & Giờ", "th-province": "Tỉnh Thành", "th-node": "Mã Trạm", "th-level": "Mực Nước",
        "th-status": "Trạng Thái",
        "title-sys-config": "Cấu Hình Hệ Thống", "lbl-threshold": "Ngưỡng Cảnh Báo Lũ", "btn-save": "Lưu Thay Đổi"
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

const chartTimeSelect = document.getElementById('chart-time-select');
const xAxisLabelsContainer = document.getElementById('x-axis-labels');

function updateChartXAxis(timeRange, lang) {
    if(!xAxisLabelsContainer) return;
    xAxisLabelsContainer.innerHTML = '';
    const points = 5;
    const nowText = lang === 'en' ? 'Now' : 'Hiện tại';
    let maxVal = 0; let unit = 'h';
    
    if (timeRange === '12h') { maxVal = 12; unit = 'h'; }
    else if (timeRange === '24h') { maxVal = 24; unit = 'h'; }
    else if (timeRange === '1w') { maxVal = 7; unit = 'd'; }
    else if (timeRange === '1m') { maxVal = 30; unit = 'd'; }
    
    const step = maxVal / (points - 1);

    for (let i = 0; i < points; i++) {
        const percent = i * 25;
        let labelText = '';
        
        if (i === points - 1) {
            labelText = nowText;
        } else {
            const timeVal = maxVal - (step * i);
            const formattedTime = Number.isInteger(timeVal) ? timeVal : timeVal.toFixed(1);
            const displayUnit = (unit === 'd' && lang === 'vi') ? ' ngày' : (unit === 'd' ? 'd' : 'h');
            labelText = `-${formattedTime}${displayUnit}`;
        }

        const span = document.createElement('span');
        span.className = 'absolute top-2 text-sm text-[#8E8F9F]';
        span.style.left = `${percent}%`;
        span.innerText = labelText;
        
        if(i === 0) span.style.transform = 'translate(0, 0)'; 
        else if(i === points - 1) span.style.transform = 'translate(-100%, 0)'; 
        else span.style.transform = 'translate(-50%, 0)'; 

        xAxisLabelsContainer.appendChild(span);
    }
}

if(chartTimeSelect) {
    chartTimeSelect.addEventListener('change', (e) => {
        updateChartXAxis(e.target.value, document.getElementById('lang-switch').value);
    });
}

const nodeSelect = document.getElementById('node-select');
const langSwitch = document.getElementById('lang-switch');
const provinceSelect = document.getElementById('province-select');

// KHÔI PHỤC "TẤT CẢ TRẠM"
function updateNodeDropdown(provinceVal, lang) {
    if(!nodeSelect) return;
    nodeSelect.innerHTML = ''; 

    // Thêm lại Tùy chọn "All Nodes"
    const allNodesOpt = document.createElement('option');
    allNodesOpt.value = 'all';
    allNodesOpt.innerText = lang === 'en' ? 'All Nodes' : 'Tất Cả Trạm';
    nodeSelect.appendChild(allNodesOpt);

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

    const currentProvSelected = document.getElementById('province-select') ? document.getElementById('province-select').value : 'all';
    renderChartAndTable(currentProvSelected, nodeSelect.value);
    updateKPIs(currentProvSelected, nodeSelect.value);
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
    
    if(chartTimeSelect) {
        updateChartXAxis(chartTimeSelect.value, lang);
    }
}

if(langSwitch) {
    langSwitch.addEventListener('change', (e) => updateLanguage(e.target.value));
}

updateLanguage('en');