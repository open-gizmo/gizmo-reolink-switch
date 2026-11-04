// Durations offered by the picker, in minutes (the ones above the maximum of the device are left out)
export const DURATION_PRESETS = [1, 2, 3, 5, 10, 15, 30, 45, 60];

// Duration selected when the device has none to suggest
export const DEFAULT_MINUTES = 10;

// How often the status is read while the page is visible, and right after a command
export const STATUS_REFRESH_MS = 2000;
export const FAST_STATUS_REFRESH_MS = 500;

// How long the status is read faster after a command (the device is busy for about a second)
export const FAST_REFRESH_DURATION_MS = 5000;

// Status the dashboard shows until the device has answered (the firmware defaults)
export const initialStatus = () => ({
    timerRunning: false,
    remainingSeconds: 0,
    selectedSeconds: 0,
    maxMinutes: 60,
    alertsStillOff: false,
    busy: false,
    nvrStatus: 'unknown',
    cameraCount: 0
});

// Get the durations to pick from: the presets up to the maximum of the device, plus the maximum itself when it is not a preset
export const buildDurationOptions = maxMinutes => {
    const maximum = Math.max(1, Math.floor(Number(maxMinutes) || 0));
    const options = DURATION_PRESETS.filter(minutes => minutes <= maximum);
    if (!options.includes(maximum)) options.push(maximum);
    return options;
};

// Get the duration to select: the one the device used last when it can be picked, otherwise the default (or the closest one)
export const pickDefaultMinutes = ({ options, selectedSeconds = 0 }) => {
    const lastMinutes = Math.round(selectedSeconds / 60);
    if (options.includes(lastMinutes)) return lastMinutes;
    if (options.includes(DEFAULT_MINUTES)) return DEFAULT_MINUTES;
    return options[options.length - 1];
};

// Keep a selected duration inside the options (the maximum of the device can change)
export const clampMinutes = ({ options, minutes }) => options.includes(minutes) ? minutes : pickDefaultMinutes({ options });

// Show a number of seconds as minutes and seconds, like the countdown of the LCD (12:34)
export const formatCountdown = seconds => {
    const totalSeconds = Math.max(0, Math.floor(Number(seconds) || 0));
    const minutes = String(Math.floor(totalSeconds / 60)).padStart(2, '0');
    const remainder = String(totalSeconds % 60).padStart(2, '0');
    return `${minutes}:${remainder}`;
};

// Get the seconds left now: the device sent them at a moment, and the time since then is taken off (never below zero)
export const computeRemainingSeconds = ({ remainingSeconds, receivedAt, now }) => {
    const elapsedSeconds = Math.max(0, Math.floor((now - receivedAt) / 1000));
    return Math.max(0, remainingSeconds - elapsedSeconds);
};

// Get the share of the timer already gone, from 0 to 1 (0 when the total is unknown)
export const computeElapsedShare = ({ remainingSeconds, selectedSeconds }) => {
    if (!selectedSeconds) return 0;
    return Math.min(1, Math.max(0, 1 - (remainingSeconds / selectedSeconds)));
};

// Get what the home page shows: the setup card, the running timer, the alerts still off, or the controls
export const getHomeState = status => {
    if (status.nvrStatus === 'not_configured') return 'setup';
    if (status.timerRunning) return 'running';
    if (status.alertsStillOff) return 'alertsOff';
    return 'idle';
};

// Check if the cameras are muted or not restored yet (the NVR login can't change then)
export const isMuted = status => status.timerRunning || status.alertsStillOff;

// Get the wait before the next status read: short for a while after a command and while the device is busy
export const getStatusRefreshInterval = ({ now, fastUntil, isBusy }) => {
    return now < fastUntil || isBusy ? FAST_STATUS_REFRESH_MS : STATUS_REFRESH_MS;
};

// Translation key and tone of each state of the NVR connection (what the NVR page and the home page say about it)
const NVR_STATUS_DETAILS = {
    not_configured: { key: 'notConfigured', tone: 'gray' },
    unknown: { key: 'unknown', tone: 'gray' },
    connected: { key: 'connected', tone: 'green' },
    unreachable: { key: 'unreachable', tone: 'red' },
    login_refused: { key: 'loginRefused', tone: 'red' },
    login_paused: { key: 'loginPaused', tone: 'yellow' }
};

// Describe a state of the NVR connection (an unknown one reads as not checked)
export const getNvrStatusDetails = nvrStatus => NVR_STATUS_DETAILS[nvrStatus] || NVR_STATUS_DETAILS.unknown;

// Check if the NVR connection has a problem the user can fix
export const hasNvrProblem = nvrStatus => ['unreachable', 'login_refused', 'login_paused'].includes(nvrStatus);
