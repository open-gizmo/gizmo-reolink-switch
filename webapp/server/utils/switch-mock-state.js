// The Wi-Fi and device mock (its state, the responses and errors of the firmware) comes from the gizmo-common layer: this file adds the quiet mode timer and the NVR login of the switch
import { EspMockError, byteLength, getGizmoMockState } from 'gizmo-common/web/server/utils/gizmo-mock.js';

// Limits of the firmware (config.h, nvr_settings.h), counted in bytes like the C strings on the device
export const MAX_MINUTES = 60;
export const NVR_HOST_MAX_BYTES = 64;
export const NVR_USERNAME_MAX_BYTES = 32;
export const NVR_PASSWORD_MAX_BYTES = 64;

// Time the main loop takes to run a command or to test the NVR, during which the device is busy
export const BUSY_DURATION_MS = 1500;

// Time the login stays paused after the NVR refused it (REOLINK_LOGIN_PAUSE_MS)
export const LOGIN_PAUSE_MS = 10 * 60 * 1000;

// Cameras the mock NVR reports as online
export const MOCK_CAMERA_COUNT = 4;

// Password the mock NVR refuses, and the endings of the address it can't reach or can't restore the alerts on
export const REFUSED_PASSWORD = 'wrong';
export const UNREACHABLE_HOST_ENDING = '.99';
export const RESTORE_FAILS_HOST_ENDING = '.98';

// Characters of an NVR address: letters, digits and . - : [ ]
const HOST_PATTERN = /^[A-Za-z0-9.\-:[\]]+$/;

// Initial NVR login: a ready one, or an empty one when the dev server starts with MOCK_NVR_EMPTY=1 (to try the setup of a new device)
const buildInitialNvr = () => {
    if (process.env.MOCK_NVR_EMPTY === '1') return { host: '', username: '', password: '' };
    return { host: '192.168.1.10', username: 'admin', password: 'secret' };
};

// State of the switch: the timer, the alerts, the command waiting for the main loop and the NVR login
const createSwitchState = () => {
    const nvr = buildInitialNvr();
    return {
        nvr,
        nvrStatus: nvr.host ? 'connected' : 'not_configured',
        cameraCount: nvr.host ? MOCK_CAMERA_COUNT : 0,
        isTimerRunning: false,
        timerEndsAt: 0,
        selectedSeconds: 0,
        hasPendingRestore: false,
        restoreAttempts: 0,
        busyUntil: 0,
        pendingCommand: null,
        loginPausedUntil: 0,
        isNvrSaveFailing: false // Makes "save the login" fail like a device that can't write its flash (for tests)
    };
};

// Get the state of the switch, kept in the mock device state of the layer (Wi-Fi, uptime, kept across hot reloads)
export const getSwitchMockState = () => {
    const gizmoState = getGizmoMockState();
    gizmoState.switch ??= createSwitchState();
    return gizmoState.switch;
};

/******************************** NVR ********************************/

// Check if a login is complete enough to reach the NVR
export const isNvrConfigured = nvr => Boolean(nvr.host && nvr.username && nvr.password);

// Read an NVR address like the firmware: without the scheme and the trailing slashes, null when it is empty, too long or has other characters
export const readHost = value => {
    const withoutScheme = value.startsWith('https://') ? value.slice(8) : (value.startsWith('http://') ? value.slice(7) : value);
    const host = withoutScheme.replace(/\/+$/, '');
    if (!host || byteLength(host) > NVR_HOST_MAX_BYTES || !HOST_PATTERN.test(host)) return null;
    return host;
};

// Check a required text that fits its buffer (a string, not empty, up to a number of bytes)
const isRequiredText = ({ value, maxLength }) => typeof value === 'string' && value.length > 0 && byteLength(value) <= maxLength;

// Apply a login body like the firmware, all or nothing: returns the new login, or throws the 400 error with the name of the first wrong field (an empty or missing password keeps the saved one)
export const applyNvrBody = ({ body, current }) => {
    // Read the address
    const host = typeof body.host === 'string' ? readHost(body.host) : null;
    if (host === null) throw new EspMockError(400, 'invalid_nvr_field', 'host');

    // Read the username
    if (!isRequiredText({ value: body.username, maxLength: NVR_USERNAME_MAX_BYTES })) throw new EspMockError(400, 'invalid_nvr_field', 'username');

    // Read the password, unless it is left empty to keep the saved one (null is the same as missing)
    let password = current.password;
    if (body.password !== undefined && body.password !== null) {
        if (typeof body.password !== 'string' || byteLength(body.password) > NVR_PASSWORD_MAX_BYTES) throw new EspMockError(400, 'invalid_nvr_field', 'password');
        if (body.password !== '') password = body.password;
    }

    // A login needs a password, saved or new
    if (!password) throw new EspMockError(400, 'invalid_nvr_field', 'password');
    return { host, username: body.username, password };
};

// Build the login the dashboard shows (never the password)
export const buildNvrPayload = state => ({
    host: state.nvr.host,
    username: state.nvr.username,
    hasPassword: state.nvr.password !== '',
    configured: isNvrConfigured(state.nvr)
});

// Save a login that passed the validation and drop the session of the old one (the pause and the camera count), like the firmware
export const saveNvr = ({ state, nvr }) => {
    state.nvr = nvr;
    state.loginPausedUntil = 0;
    state.cameraCount = 0;
    state.nvrStatus = isNvrConfigured(nvr) ? 'unknown' : 'not_configured';
};

// Log in to the mock NVR and count its cameras: the outcome depends on the login (an address ending in .99 is unreachable, the password "wrong" is refused, then the login pauses)
const runNvrTest = ({ state, now }) => {
    // A refused login pauses the next ones
    if (state.loginPausedUntil > now) {
        state.nvrStatus = 'login_paused';
        return;
    }
    state.loginPausedUntil = 0;

    // Nobody answers at this address
    if (state.nvr.host.endsWith(UNREACHABLE_HOST_ENDING)) {
        state.nvrStatus = 'unreachable';
        state.cameraCount = 0;
        return;
    }

    // The NVR refuses this password
    if (state.nvr.password === REFUSED_PASSWORD) {
        state.nvrStatus = 'login_refused';
        state.cameraCount = 0;
        state.loginPausedUntil = now + LOGIN_PAUSE_MS;
        return;
    }

    // The NVR answered
    state.nvrStatus = 'connected';
    state.cameraCount = MOCK_CAMERA_COUNT;
};

/******************************** Timer and commands ********************************/

// Turn the alerts back on (the restore fails the first time on an address ending in .98, to show the warning of the dashboard)
const restoreAlerts = state => {
    state.isTimerRunning = false;
    state.selectedSeconds = 0; // Like the firmware: cancelling or finishing resets the selected duration
    state.timerEndsAt = 0;
    state.restoreAttempts += 1;
    state.hasPendingRestore = state.nvr.host.endsWith(RESTORE_FAILS_HOST_ENDING) && state.restoreAttempts === 1;
    if (!state.hasPendingRestore) state.restoreAttempts = 0;
};

// Run the command the main loop took, at the moment it stops being busy
const runPendingCommand = ({ state, now }) => {
    const command = state.pendingCommand;
    state.pendingCommand = null;
    if (command.type === 'start') {
        // The NVR can't be reached, so the alerts are not muted and no timer starts
        if (state.nvr.host.endsWith(UNREACHABLE_HOST_ENDING)) {
            state.nvrStatus = 'unreachable';
            return;
        }
        state.isTimerRunning = true;
        state.selectedSeconds = command.minutes * 60;
        state.timerEndsAt = state.busyUntil + (command.minutes * 60 * 1000);
        state.restoreAttempts = 0;
    } else if (command.type === 'cancel' || command.type === 'retry') {
        restoreAlerts(state);
    } else if (command.type === 'test') {
        runNvrTest({ state, now });
    }
};

// Move the device forward in time: run the waiting command when its busy time is over, and turn the alerts back on when the timer ends (call it before reading the state)
export const advanceSwitchState = ({ state, now = Date.now() }) => {
    if (state.pendingCommand && now >= state.busyUntil) runPendingCommand({ state, now });
    if (state.isTimerRunning && now >= state.timerEndsAt) restoreAlerts(state);
};

// Hand a command to the main loop: the device is busy until it has run it
export const submitMockCommand = ({ state, command, now = Date.now() }) => {
    state.pendingCommand = command;
    state.busyUntil = now + BUSY_DURATION_MS;
};

// Check a quiet mode command against what the switch is doing (like checkQuietModeCommand): throws the 409 error, in the order of the firmware
export const checkQuietModeCommand = ({ state, command }) => {
    if (state.pendingCommand) throw new EspMockError(409, 'device_busy', 'device_busy');
    if (command.type === 'start') {
        if (state.isTimerRunning) throw new EspMockError(409, 'timer_running', 'timer_running');
        if (!isNvrConfigured(state.nvr)) throw new EspMockError(409, 'nvr_not_configured', 'nvr_not_configured');
    } else if (command.type === 'cancel') {
        if (!state.isTimerRunning) throw new EspMockError(409, 'timer_not_running', 'timer_not_running');
    } else if (command.type === 'retry') {
        if (!state.hasPendingRestore || state.isTimerRunning) throw new EspMockError(409, 'nothing_to_restore', 'nothing_to_restore');
    }
};

// Read a quiet mode body like parseQuietModeCommand: returns the command, or throws the 400 error with the name of the first wrong field
export const parseQuietModeBody = body => {
    // The action must be a text
    if (typeof body.action !== 'string') throw new EspMockError(400, 'invalid_quiet_mode_field', 'action');

    // Start for a number of whole minutes, up to the maximum
    if (body.action === 'start') {
        if (!Number.isInteger(body.minutes) || body.minutes < 1 || body.minutes > MAX_MINUTES) throw new EspMockError(400, 'invalid_quiet_mode_field', 'minutes');
        return { type: 'start', minutes: body.minutes };
    }

    // Cancel the timer, or retry to restore the alerts
    if (body.action === 'cancel') return { type: 'cancel' };
    if (body.action === 'retry') return { type: 'retry' };

    // Unknown action
    throw new EspMockError(400, 'invalid_quiet_mode_field', 'action');
};

/******************************** Status ********************************/

// Build the state the dashboard reads (the timer, the alerts and the NVR)
export const buildStatusPayload = ({ state, now = Date.now() }) => ({
    timerRunning: state.isTimerRunning,
    remainingSeconds: state.isTimerRunning ? Math.max(0, Math.ceil((state.timerEndsAt - now) / 1000)) : 0,
    selectedSeconds: state.selectedSeconds,
    maxMinutes: MAX_MINUTES,
    alertsStillOff: state.hasPendingRestore,
    busy: state.pendingCommand !== null,
    nvrStatus: state.nvrStatus,
    cameraCount: state.cameraCount
});
