/**
 * ScareCrowWeb Frontend Application
 * Handles API communication and dynamic content updates
 */

// API Configuration
// Change this to your Cloud Run URL when deployed, or leave as localhost for local development
// Example Cloud Run URL: https://scarecrow-api-xxxxx-as.a.run.app
const API_BASE_URL = window.SCARECROW_API_URL || 'http://localhost:8000';

// Log which API we're using
console.log('🔗 API Base URL:', API_BASE_URL);

// State
let currentPage = 'dashboard';
let refreshInterval = null;
let alertCount = 0;

// ==================== API Functions ====================

async function fetchAPI(endpoint) {
    try {
        const response = await fetch(`${API_BASE_URL}${endpoint}`);
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        return await response.json();
    } catch (error) {
        console.error(`API Error (${endpoint}):`, error);
        return null;
    }
}

async function deleteAPI(endpoint) {
    try {
        const response = await fetch(`${API_BASE_URL}${endpoint}`, {
            method: 'DELETE'
        });
        return await response.json();
    } catch (error) {
        console.error(`Delete Error (${endpoint}):`, error);
        return null;
    }
}

// ==================== Data Fetching ====================

async function fetchStats() {
    const data = await fetchAPI('/stats');
    if (data && data.success) {
        return data.stats;
    }
    return null;
}

async function fetchDetections(limit = 100) {
    const data = await fetchAPI(`/detections?limit=${limit}`);
    if (data && data.success) {
        return data.detections;
    }
    return [];
}

async function fetchDevices() {
    const data = await fetchAPI('/devices');
    if (data && data.success) {
        return data.devices;
    }
    return [];
}

async function fetchAlerts(limit = 100) {
    const data = await fetchAPI(`/alerts?limit=${limit}`);
    if (data && data.success) {
        alertCount = data.count;
        updateAlertBadge();
        return data.alerts;
    }
    return [];
}

// ==================== UI Update Functions ====================

function updateStats(stats) {
    if (!stats) return;

    const totalDetections = document.getElementById('total-detections');
    const activeDevices = document.getElementById('active-devices');
    const totalAlerts = document.getElementById('total-alerts');

    if (totalDetections) {
        animateNumber(totalDetections, parseInt(totalDetections.textContent) || 0, stats.total_detections);
    }
    if (activeDevices) {
        animateNumber(activeDevices, parseInt(activeDevices.textContent) || 0, stats.active_devices);
    }
    if (totalAlerts) {
        animateNumber(totalAlerts, parseInt(totalAlerts.textContent) || 0, stats.total_alerts);
    }
}

function animateNumber(element, start, end, duration = 500) {
    const startTime = performance.now();
    const update = (currentTime) => {
        const elapsed = currentTime - startTime;
        const progress = Math.min(elapsed / duration, 1);
        const current = Math.floor(start + (end - start) * easeOutQuad(progress));
        element.textContent = current;
        if (progress < 1) {
            requestAnimationFrame(update);
        }
    };
    requestAnimationFrame(update);
}

function easeOutQuad(t) {
    return t * (2 - t);
}

function updateAlertBadge() {
    const badge = document.getElementById('alert-badge');
    if (badge) {
        badge.textContent = alertCount;
        badge.style.display = alertCount > 0 ? 'block' : 'none';
    }
}

function renderDetections(detections, containerId = 'detection-list') {
    const container = document.getElementById(containerId);
    if (!container) return;

    if (detections.length === 0) {
        container.innerHTML = `
            <div class="empty-state">
                <i class="fas fa-camera"></i>
                <h3>No Detections Yet</h3>
                <p>When ESP32-CAM devices capture images, detections will appear here.</p>
            </div>
        `;
        return;
    }

    container.innerHTML = detections.map((detection, index) => `
        <div class="detection-card animate-fade-in" style="animation-delay: ${index * 0.1}s">
            <img src="${getImageUrl(detection.image_path)}" 
                 alt="${detection.animal}" 
                 class="detection-image"
                 onerror="this.src='data:image/svg+xml,<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 100 100%22><rect fill=%22%231a1a25%22 width=%22100%22 height=%22100%22/><text x=%2250%22 y=%2250%22 text-anchor=%22middle%22 fill=%22%2371717a%22>No Image</text></svg>'">
            <div class="detection-info">
                <div class="detection-animal">${detection.animal || 'Unknown'}</div>
                <div class="detection-meta">
                    <span><i class="fas fa-microchip"></i> ${detection.device_id}</span>
                    <span><i class="fas fa-clock"></i> ${formatTime(detection.timestamp)}</span>
                    <span><i class="fas fa-percentage"></i> ${(detection.confidence * 100).toFixed(1)}%</span>
                </div>
                <div class="confidence-bar">
                    <div class="confidence-fill" style="width: ${detection.confidence * 100}%"></div>
                </div>
            </div>
        </div>
    `).join('');
}

function renderAlerts(alerts, containerId = 'alert-list') {
    const container = document.getElementById(containerId);
    if (!container) return;

    if (alerts.length === 0) {
        container.innerHTML = `
            <div class="empty-state">
                <i class="fas fa-bell-slash"></i>
                <h3>No Alerts</h3>
                <p>Everything is quiet. Alerts will appear when animals are detected.</p>
            </div>
        `;
        return;
    }

    container.innerHTML = alerts.map(alert => `
        <div class="alert-item" data-id="${alert.id}">
            <div class="alert-icon">
                <i class="fas fa-exclamation-triangle"></i>
            </div>
            <div class="alert-content">
                <div class="alert-message">${alert.message}</div>
                <div class="alert-time">
                    <i class="fas fa-microchip"></i> ${alert.device_id} • ${formatTime(alert.timestamp)}
                </div>
            </div>
            <button class="alert-dismiss" onclick="dismissAlert(${alert.id})">
                <i class="fas fa-times"></i>
            </button>
        </div>
    `).join('');
}

function renderDevices(devices, containerId = 'device-grid') {
    const container = document.getElementById(containerId);
    if (!container) return;

    if (devices.length === 0) {
        container.innerHTML = `
            <div class="empty-state" style="grid-column: 1 / -1;">
                <i class="fas fa-video-slash"></i>
                <h3>No Devices Connected</h3>
                <p>ESP32-CAM devices will appear here once they start sending images.</p>
            </div>
        `;
        return;
    }

    container.innerHTML = devices.map(device => `
        <div class="device-card">
            <div class="device-header">
                <span class="device-name"><i class="fas fa-video"></i> ${device.device_id}</span>
                <span class="device-status ${device.status}">
                    <span class="status-dot ${device.status === 'offline' ? 'offline' : ''}"></span>
                    ${device.status}
                </span>
            </div>
            <div class="device-info">
                <span><i class="fas fa-clock"></i> Last seen: ${device.last_seen ? formatTime(device.last_seen) : 'Never'}</span>
            </div>
        </div>
    `).join('');
}

function renderDetectionTable(detections, containerId = 'detection-table-body') {
    const container = document.getElementById(containerId);
    if (!container) return;

    if (detections.length === 0) {
        container.innerHTML = `
            <tr>
                <td colspan="6" class="empty-state">
                    <i class="fas fa-inbox"></i>
                    <h3>No Detection History</h3>
                </td>
            </tr>
        `;
        return;
    }

    container.innerHTML = detections.map(detection => `
        <tr>
            <td>
                <img src="${getImageUrl(detection.image_path)}" 
                     alt="${detection.animal}" 
                     class="thumbnail"
                     onerror="this.src='data:image/svg+xml,<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 60 45%22><rect fill=%22%231a1a25%22 width=%2260%22 height=%2245%22/></svg>'">
            </td>
            <td><strong style="text-transform: capitalize;">${detection.animal || 'Unknown'}</strong></td>
            <td>${(detection.confidence * 100).toFixed(1)}%</td>
            <td>${detection.device_id}</td>
            <td>${formatTime(detection.timestamp)}</td>
            <td>
                <button class="btn btn-icon" onclick="viewImage('${detection.image_path}')">
                    <i class="fas fa-eye"></i>
                </button>
            </td>
        </tr>
    `).join('');
}

// ==================== Actions ====================

async function dismissAlert(alertId) {
    const result = await deleteAPI(`/alerts/${alertId}`);
    if (result && result.success) {
        const alertItem = document.querySelector(`.alert-item[data-id="${alertId}"]`);
        if (alertItem) {
            alertItem.style.animation = 'fadeOut 0.3s ease forwards';
            setTimeout(() => alertItem.remove(), 300);
        }
        alertCount = Math.max(0, alertCount - 1);
        updateAlertBadge();
        showToast('Alert dismissed', 'success');
    }
}

function viewImage(imagePath) {
    window.open(getImageUrl(imagePath), '_blank');
}

/**
 * Get the full URL for an image path.
 * Handles both local paths and full Supabase Storage URLs.
 */
function getImageUrl(imagePath) {
    if (!imagePath) return '';
    // If it's already a full URL (Supabase Storage), return as-is
    if (imagePath.startsWith('http://') || imagePath.startsWith('https://')) {
        return imagePath;
    }
    // Otherwise, assume it's a local path
    return `${API_BASE_URL}/uploads/${imagePath}`;
}

// ==================== Utilities ====================

function formatTime(timestamp) {
    if (!timestamp) return 'Unknown';
    const date = new Date(timestamp);
    const now = new Date();
    const diff = now - date;

    // Less than 1 minute
    if (diff < 60000) {
        return 'Just now';
    }

    // Less than 1 hour
    if (diff < 3600000) {
        const minutes = Math.floor(diff / 60000);
        return `${minutes}m ago`;
    }

    // Less than 24 hours
    if (diff < 86400000) {
        const hours = Math.floor(diff / 3600000);
        return `${hours}h ago`;
    }

    // Format as date
    return date.toLocaleDateString('en-US', {
        month: 'short',
        day: 'numeric',
        hour: '2-digit',
        minute: '2-digit'
    });
}

function showToast(message, type = 'info') {
    const container = document.getElementById('toast-container') || createToastContainer();
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.innerHTML = `
        <i class="fas fa-${type === 'success' ? 'check-circle' : type === 'error' ? 'exclamation-circle' : 'info-circle'}"></i>
        <span>${message}</span>
    `;
    container.appendChild(toast);

    setTimeout(() => {
        toast.style.animation = 'fadeOut 0.3s ease forwards';
        setTimeout(() => toast.remove(), 300);
    }, 3000);
}

function createToastContainer() {
    const container = document.createElement('div');
    container.id = 'toast-container';
    container.className = 'toast-container';
    document.body.appendChild(container);
    return container;
}

// ==================== Navigation ====================

function navigateTo(page) {
    currentPage = page;

    // Update active nav item
    document.querySelectorAll('.nav-item').forEach(item => {
        item.classList.remove('active');
    });
    document.querySelector(`.nav-item[data-page="${page}"]`)?.classList.add('active');

    // Update page content
    refreshData();
}

// ==================== Data Refresh ====================

async function refreshData() {
    switch (currentPage) {
        case 'dashboard':
            const stats = await fetchStats();
            updateStats(stats);
            const recentDetections = await fetchDetections(5);
            renderDetections(recentDetections);
            const recentAlerts = await fetchAlerts(10);
            renderAlerts(recentAlerts);
            break;

        case 'history':
            const allDetections = await fetchDetections(100);
            renderDetectionTable(allDetections);
            break;

        case 'devices':
            const devices = await fetchDevices();
            renderDevices(devices);
            break;

        case 'alerts':
            const alerts = await fetchAlerts(100);
            renderAlerts(alerts, 'alerts-page-list');
            break;
    }
}

function startAutoRefresh(intervalMs = 5000) {
    stopAutoRefresh();
    refreshInterval = setInterval(refreshData, intervalMs);
}

function stopAutoRefresh() {
    if (refreshInterval) {
        clearInterval(refreshInterval);
        refreshInterval = null;
    }
}

// ==================== Connection Status ====================

async function checkConnection() {
    const statusDot = document.querySelector('.status-dot');
    const statusText = document.querySelector('.connection-status span:last-child');

    try {
        const response = await fetch(`${API_BASE_URL}/`);
        if (response.ok) {
            statusDot?.classList.remove('offline');
            if (statusText) statusText.textContent = 'Connected';
            return true;
        }
    } catch (error) {
        statusDot?.classList.add('offline');
        if (statusText) statusText.textContent = 'Offline';
    }
    return false;
}

// ==================== Initialization ====================

document.addEventListener('DOMContentLoaded', async () => {
    console.log('🚀 ScareCrowWeb Frontend loaded');

    // Check connection
    const connected = await checkConnection();
    if (!connected) {
        showToast('Backend server is offline. Please start the server.', 'error');
    }

    // Initial data load
    await refreshData();

    // Start auto-refresh
    startAutoRefresh(5000);

    // Setup navigation - only prevent default for # links (dashboard on main page)
    document.querySelectorAll('.nav-item').forEach(item => {
        item.addEventListener('click', (e) => {
            const href = item.getAttribute('href');
            // Only prevent default if it's the dashboard link on the dashboard page
            // or if it's a # link. Otherwise, let it navigate normally.
            if (href === '#' || href === '#dashboard') {
                e.preventDefault();
                navigateTo('dashboard');
            }
            // Let other links navigate to their pages normally
        });
    });

    // Refresh button
    document.getElementById('refresh-btn')?.addEventListener('click', () => {
        showToast('Refreshing data...', 'info');
        refreshData();
    });
});

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    stopAutoRefresh();
});

// Add fadeOut animation
const style = document.createElement('style');
style.textContent = `
    @keyframes fadeOut {
        from { opacity: 1; transform: translateY(0); }
        to { opacity: 0; transform: translateY(-10px); }
    }
`;
document.head.appendChild(style);
