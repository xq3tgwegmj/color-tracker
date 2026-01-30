const { ipcRenderer } = require('electron');
const fs = require('fs');
const path = require('path');

const configPath = path.join(__dirname, '..', 'config.json');

// DOM Elements
const els = {
    enabled: document.getElementById('status-enabled'),
    color: document.getElementById('status-color'),
    rgb: document.getElementById('status-rgb'),

    inpRadius: document.getElementById('inp-radius'),
    valRadius: document.getElementById('val-radius'),

    inpTolerance: document.getElementById('inp-tolerance'),
    valTolerance: document.getElementById('val-tolerance'),

    inpSleep: document.getElementById('inp-sleep'),
    valSleep: document.getElementById('val-sleep'),

    inpEnable: document.getElementById('inp-key-enable'),
    inpToggle: document.getElementById('inp-key-toggle'),

    btnSave: document.getElementById('btn-save'),
    btnMinimize: document.getElementById('btn-minimize'),
    btnClose: document.getElementById('btn-close'),
    btnTop: document.getElementById('btn-top'),

    btnResetSettings: document.getElementById('btn-reset-settings'),
    btnResetKeybinds: document.getElementById('btn-reset-keybinds'),
    themeBtns: document.querySelectorAll('.theme-btn')
};

// State
let config = {
    searchRadius: 50,
    tolerance: 24,
    loopSleepMs: 1,
    enableKey: 'F5',
    toggleKey: 'E',
    modeKey: 'F4',
    theme: 'default'
};
let isAlwaysOnTop = false;

// Defaults
const DEFAULTS = {
    searchRadius: 50,
    tolerance: 24,
    loopSleepMs: 1,
    enableKey: 'F5',
    toggleKey: 'E',
    modeKey: 'F4',
    theme: 'default'
};

// Load Config

// Load Config
async function loadConfig() {
    try {
        const paths = await ipcRenderer.invoke('get-app-paths');
        const configPath = paths.configPath;

        if (fs.existsSync(configPath)) {
            const data = fs.readFileSync(configPath, 'utf8');
            const loaded = JSON.parse(data);
            config = { ...config, ...loaded }; // Merge to ensure new keys exist
            applyConfigToUI();
            applyTheme(config.theme);
        } else {
            console.log("Config not found at " + configPath + ", using defaults.");
            // Optionally write defaults
            saveConfig();
        }
    } catch (e) {
        console.error("Failed to load config", e);
    }
}

// ... existing code ...

// Init
(async () => {
    await loadConfig();
})();

function applyConfigToUI() {
    els.inpRadius.value = config.searchRadius;
    els.valRadius.textContent = config.searchRadius;

    els.inpTolerance.value = config.tolerance;
    els.valTolerance.textContent = config.tolerance;

    els.inpSleep.value = config.loopSleepMs;
    els.valSleep.textContent = config.loopSleepMs;

    els.inpEnable.value = config.enableKey;
    els.inpToggle.value = config.toggleKey;
}

function applyTheme(themeName) {
    document.body.className = ''; // Clear existing
    if (themeName && themeName !== 'default') {
        document.body.classList.add(`theme-${themeName}`);
    }

    // Update dropdown selector value
    const themeSelect = document.getElementById('sel-theme');
    if (themeSelect && themeSelect.value !== themeName) {
        themeSelect.value = themeName;
    }
}

function saveConfig() {
    config.searchRadius = parseInt(els.inpRadius.value);
    config.tolerance = parseInt(els.inpTolerance.value);
    config.loopSleepMs = parseInt(els.inpSleep.value);
    config.enableKey = els.inpEnable.value.toUpperCase();
    config.toggleKey = els.inpToggle.value.toUpperCase();
    // Sync theme from dropdown
    const themeSelect = document.getElementById('sel-theme');
    if (themeSelect) {
        config.theme = themeSelect.value;
    }

    ipcRenderer.send('update-config', config);
    applyConfigToUI();

    // Visual feedback
    const originalText = els.btnSave.textContent;
    els.btnSave.textContent = "Saved!";
    els.btnSave.style.borderColor = "var(--success-color)";
    setTimeout(() => {
        els.btnSave.textContent = originalText;
        els.btnSave.style.borderColor = "var(--accent-color)";
    }, 1000);
}

// Event Listeners - Inputs
// Auto-save on change for sliders
function autoSave() {
    saveConfig();
}

els.inpRadius.addEventListener('input', (e) => {
    els.valRadius.textContent = e.target.value;
    // Debounce or just save? For simplicity, save on change (mouseup) or throttled input
});
els.inpRadius.addEventListener('change', autoSave);

els.inpTolerance.addEventListener('input', (e) => {
    els.valTolerance.textContent = e.target.value;
});
els.inpTolerance.addEventListener('change', autoSave);

els.inpSleep.addEventListener('input', (e) => {
    els.valSleep.textContent = e.target.value;
});
els.inpSleep.addEventListener('change', autoSave);

// Key Recording Logic
function setupKeyRecorder(btnElement, configKey) {
    btnElement.addEventListener('click', () => {
        const originalValue = btnElement.value;
        btnElement.value = "Press key...";
        btnElement.classList.add('recording');

        // Remove existing handler to avoid stacking if clicked multiple times
        const keyHandler = (e) => {
            e.preventDefault();
            let keyName = e.key.toUpperCase();

            // Map special keys
            if (keyName === ' ') keyName = 'SPACE';
            if (keyName === 'CONTROL') keyName = 'CTRL';
            if (keyName === 'ESCAPE') keyName = 'ESC';

            // Handle key names that might be too long or tricky
            if (keyName.length > 1 && !keyName.startsWith('F') &&
                !['SPACE', 'CTRL', 'ESC', 'SHIFT', 'ALT', 'TAB', 'ENTER'].includes(keyName)) {
                // Determine if we support it. For now, let's accept it and hope C++ handles or default fallback
            }

            config[configKey] = keyName;
            btnElement.value = keyName;
            btnElement.classList.remove('recording');

            saveConfig();
            document.removeEventListener('keydown', keyHandler);
        };

        // Allow canceling with mouse click outside?
        // For simplicity, just wait for key.

        document.addEventListener('keydown', keyHandler, { once: true });
    });
}

// Convert inputs to buttons for keys
// We'll replace the input elements with buttons in JS for smoother upgrade if we changed HTML
// Actually, let's assume we change index.html to buttons, OR we just treat the inputs as read-only and click to record
els.inpEnable.readOnly = true;
els.inpToggle.readOnly = true;
els.inpEnable.style.cursor = "pointer";
els.inpToggle.style.cursor = "pointer";
els.inpEnable.value = config.enableKey;
els.inpToggle.value = config.toggleKey; // Ensure initial sync

setupKeyRecorder(els.inpEnable, 'enableKey');
setupKeyRecorder(els.inpToggle, 'toggleKey');

// Reset Logic
els.btnResetSettings.addEventListener('click', () => {
    config.searchRadius = DEFAULTS.searchRadius;
    config.tolerance = DEFAULTS.tolerance;
    config.loopSleepMs = DEFAULTS.loopSleepMs;
    applyConfigToUI();
    saveConfig();
});

els.btnResetKeybinds.addEventListener('click', () => {
    config.enableKey = DEFAULTS.enableKey;
    config.toggleKey = DEFAULTS.toggleKey;
    els.inpEnable.value = config.enableKey;
    els.inpToggle.value = config.toggleKey;
    applyConfigToUI();
    saveConfig();
});

// Theme Logic
els.selTheme = document.getElementById('sel-theme');

els.selTheme.addEventListener('change', (e) => {
    const theme = e.target.value;
    config.theme = theme;
    applyTheme(theme);
    saveConfig(); // Use saveConfig to ensure all values are synced
});

// applyTheme is defined earlier in the file

// We removed the Save Settings button (user request), so we don't need the listener
if (els.btnSave) els.btnSave.style.display = 'none';

// Window Controls
els.btnMinimize.addEventListener('click', () => ipcRenderer.send('minimize-window'));
els.btnClose.addEventListener('click', () => ipcRenderer.send('close-window'));
els.btnTop.addEventListener('click', () => {
    isAlwaysOnTop = !isAlwaysOnTop;
    ipcRenderer.send('toggle-always-on-top', isAlwaysOnTop);
    els.btnTop.style.color = isAlwaysOnTop ? 'var(--accent-color)' : 'var(--text-secondary)';
});

// Backend State Handling
ipcRenderer.on('backend-state', (event, message) => {
    // message format: STATE:KEY:VALUE or STATE:READY
    const parts = message.trim().split(':');
    if (parts.length < 2) return;

    const key = parts[1];
    const val = parts[2];

    switch (key) {
        case 'ENABLED':
            els.enabled.textContent = val;
            els.enabled.style.color = val === 'ON' ? 'var(--success-color)' : 'var(--danger-color)';
            break;
        case 'COLOR':
            if (val) {
                const [r, g, b] = val.split(',');
                els.color.style.background = `rgb(${r},${g},${b})`;
                els.rgb.textContent = `${r}, ${g}, ${b}`;
            }
            break;
    }
});

// Init
loadConfig();
