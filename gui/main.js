const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const { spawn } = require('child_process');
const fs = require('fs');

let mainWindow;
let backendProcess;

// Path Handling
const isDev = !app.isPackaged;
let rootDir;
let backendPath;
let configDir; // Separate config directory for persistence

if (isDev) {
    // In Dev: gui/main.js -> root is ../
    rootDir = path.join(__dirname, '..');
    backendPath = path.join(rootDir, 'ColorTracker.exe');
    configDir = rootDir; // In dev, config is in project root
} else {
    // In Prod: Save config next to the exe file
    configDir = path.dirname(app.getPath('exe'));

    // Backend: In resources folder
    backendPath = path.join(process.resourcesPath, 'ColorTracker.exe');

    // For cwd of backend process, use configDir
    rootDir = configDir;
}

console.log('Root Dir:', rootDir);
console.log('Backend Path:', backendPath);

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 420,
        height: 750,
        frame: false, // Frameless for custom title bar
        transparent: true, // Transparent for glassy effect
        resizable: true, // Allow user to resize
        minWidth: 380, // Prevent breaking layout
        minHeight: 500,
        useContentSize: true, // Use content size for accurate sizing
        webPreferences: {
            nodeIntegration: true,
            contextIsolation: false, // Simplified for local prototype
            devTools: true // Keep devtools for debugging even in prod if needed, or set isDev
        },
        icon: path.join(__dirname, 'icon.png') // We might need an icon
    });

    mainWindow.loadFile('index.html');
    // if (isDev) mainWindow.webContents.openDevTools({ mode: 'detach' });
}

app.whenReady().then(() => {
    createWindow();

    // Spawn C++ Backend
    // Use 'cwd' as rootDir so it finds config.json there
    backendProcess = spawn(backendPath, [], {
        cwd: rootDir
    });

    backendProcess.stdout.on('data', (data) => {
        const str = data.toString();
        // Parse stdout for STATE: commands
        const lines = str.split(/[\r\n]+/);
        lines.forEach(line => {
            if (line.startsWith('STATE:')) {
                if (mainWindow && !mainWindow.isDestroyed()) {
                    mainWindow.webContents.send('backend-state', line);
                }
            }
        });
        console.log(`Backend: ${str}`);
    });

    backendProcess.stderr.on('data', (data) => {
        console.error(`Backend Error: ${data}`);
    });

    backendProcess.on('close', (code) => {
        console.log(`Backend process exited with code ${code}`);
    });
});

ipcMain.handle('get-app-paths', () => {
    return {
        rootDir: rootDir,
        configPath: path.join(rootDir, 'config.json')
    };
});

ipcMain.on('update-config', (event, newConfig) => {
    // Write config to the dynamic rootDir
    const configPath = path.join(rootDir, 'config.json');
    try {
        fs.writeFileSync(configPath, JSON.stringify(newConfig, null, 2));
        console.log('Config saved to:', configPath);
    } catch (err) {
        console.error('Failed to save config:', err);
    }
    // Backend polls config, so it should pick it up automatically
});

ipcMain.on('minimize-window', () => {
    if (mainWindow) mainWindow.minimize();
});

ipcMain.on('close-window', () => {
    if (backendProcess) backendProcess.kill();
    app.quit();
});

ipcMain.on('toggle-always-on-top', (event, shouldBeOnTop) => {
    if (mainWindow) {
        mainWindow.setAlwaysOnTop(shouldBeOnTop);
    }
});
