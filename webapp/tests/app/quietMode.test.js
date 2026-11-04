import { describe, expect, it } from 'vitest';
import { buildDurationOptions, clampMinutes, computeElapsedShare, computeRemainingSeconds, DEFAULT_MINUTES, FAST_STATUS_REFRESH_MS, formatCountdown, getHomeState, getNvrStatusDetails, getStatusRefreshInterval, hasNvrProblem, initialStatus, isMuted, pickDefaultMinutes, STATUS_REFRESH_MS } from '~/composables/useQuietMode.js';
import { buildNvrBody, findInvalidNvrField } from '~/composables/useNvrForm.js';

// The chips of the duration picker
describe('buildDurationOptions', () => {
    // The device allows up to an hour: every preset is there
    it('offers every preset up to 60 minutes', () => {
        expect(buildDurationOptions(60)).toEqual([1, 2, 3, 5, 10, 15, 30, 45, 60]);
    });

    // A device with a lower maximum must not offer what it would refuse
    it('leaves out the presets above the maximum of the device', () => {
        expect(buildDurationOptions(20)).toEqual([1, 2, 3, 5, 10, 15, 20]);
        expect(buildDurationOptions(5)).toEqual([1, 2, 3, 5]);
        expect(buildDurationOptions(1)).toEqual([1]);
    });

    // A maximum that is not a preset is still reachable
    it('adds the maximum when it is not a preset', () => {
        expect(buildDurationOptions(90)).toEqual([1, 2, 3, 5, 10, 15, 30, 45, 60, 90]);
    });

    // A missing maximum never leaves the picker empty
    it('keeps at least one minute for a missing maximum', () => {
        expect(buildDurationOptions(undefined)).toEqual([1]);
        expect(buildDurationOptions(0)).toEqual([1]);
    });
});

// The duration selected when the page opens
describe('pickDefaultMinutes', () => {
    const options = buildDurationOptions(60);

    // The last duration used on the device is the most likely one
    it('starts from the duration the device used last', () => {
        expect(pickDefaultMinutes({ options, selectedSeconds: 30 * 60 })).toBe(30);
    });

    // A device that never ran a timer has no suggestion
    it('falls back to the default', () => {
        expect(pickDefaultMinutes({ options, selectedSeconds: 0 })).toBe(DEFAULT_MINUTES);
    });

    // A duration the picker doesn't offer is not selected
    it('ignores a duration that is not an option', () => {
        expect(pickDefaultMinutes({ options, selectedSeconds: 7 * 60 })).toBe(DEFAULT_MINUTES);
    });

    // A low maximum has no default: the longest option is used
    it('uses the longest option when the default is above the maximum', () => {
        expect(pickDefaultMinutes({ options: buildDurationOptions(5), selectedSeconds: 0 })).toBe(5);
    });

    // The maximum of the device can drop while the page is open
    it('brings a selection above the new maximum back inside the options', () => {
        expect(clampMinutes({ options: buildDurationOptions(15), minutes: 45 })).toBe(10);
        expect(clampMinutes({ options, minutes: 45 })).toBe(45);
    });
});

// The countdown shown on the page
describe('formatCountdown', () => {
    // Minutes and seconds, always two digits, like the LCD
    it('writes minutes and seconds', () => {
        expect(formatCountdown(754)).toBe('12:34');
        expect(formatCountdown(65)).toBe('01:05');
        expect(formatCountdown(5)).toBe('00:05');
        expect(formatCountdown(0)).toBe('00:00');
        expect(formatCountdown(3600)).toBe('60:00');
    });

    // Bad values never show NaN
    it('reads a missing or negative value as zero', () => {
        expect(formatCountdown(undefined)).toBe('00:00');
        expect(formatCountdown(-4)).toBe('00:00');
        expect(formatCountdown('abc')).toBe('00:00');
        expect(formatCountdown(59.9)).toBe('00:59');
    });
});

// The countdown goes down between two reads of the status
describe('computeRemainingSeconds', () => {
    // The device said 100 seconds 3.4 seconds ago
    it('takes off the whole seconds since the status arrived', () => {
        expect(computeRemainingSeconds({ remainingSeconds: 100, receivedAt: 10000, now: 13400 })).toBe(97);
        expect(computeRemainingSeconds({ remainingSeconds: 100, receivedAt: 10000, now: 10999 })).toBe(100);
    });

    // A clock that is a little behind the arrival must not add time
    it('never counts a negative time', () => {
        expect(computeRemainingSeconds({ remainingSeconds: 100, receivedAt: 10000, now: 9000 })).toBe(100);
    });

    // The countdown stops at zero while the next status is on its way
    it('stops at zero', () => {
        expect(computeRemainingSeconds({ remainingSeconds: 3, receivedAt: 0, now: 60000 })).toBe(0);
    });
});

// The bar under the countdown
describe('computeElapsedShare', () => {
    // Half of the time is gone
    it('is the share of the timer already gone', () => {
        expect(computeElapsedShare({ remainingSeconds: 150, selectedSeconds: 300 })).toBe(0.5);
        expect(computeElapsedShare({ remainingSeconds: 300, selectedSeconds: 300 })).toBe(0);
        expect(computeElapsedShare({ remainingSeconds: 0, selectedSeconds: 300 })).toBe(1);
    });

    // A device that doesn't tell the total gets an empty bar, not NaN
    it('is zero without a total, and never above one', () => {
        expect(computeElapsedShare({ remainingSeconds: 10, selectedSeconds: 0 })).toBe(0);
        expect(computeElapsedShare({ remainingSeconds: 0, selectedSeconds: 0 })).toBe(0);
        expect(computeElapsedShare({ remainingSeconds: -5, selectedSeconds: 60 })).toBe(1);
    });
});

// What the home page shows
describe('getHomeState', () => {
    // Start from a device with a working NVR
    const build = changes => ({ ...initialStatus(), nvrStatus: 'connected', ...changes });

    // The four states of the page
    it('tells the setup, the running timer, the alerts still off and the idle state', () => {
        expect(getHomeState(build({ nvrStatus: 'not_configured' }))).toBe('setup');
        expect(getHomeState(build({ timerRunning: true, remainingSeconds: 30 }))).toBe('running');
        expect(getHomeState(build({ alertsStillOff: true }))).toBe('alertsOff');
        expect(getHomeState(build({}))).toBe('idle');
    });

    // A missing NVR comes first, whatever else the device says
    it('shows the setup before everything else', () => {
        expect(getHomeState(build({ nvrStatus: 'not_configured', timerRunning: true }))).toBe('setup');
    });

    // A running timer is not the warning
    it('shows the timer when both the timer and the pending restore are set', () => {
        expect(getHomeState(build({ timerRunning: true, alertsStillOff: true }))).toBe('running');
    });

    // An NVR that can't be reached still lets the user try: the device is the one to say no
    it('shows the controls when the NVR has a problem', () => {
        expect(getHomeState(build({ nvrStatus: 'unreachable' }))).toBe('idle');
        expect(getHomeState(build({ nvrStatus: 'unknown' }))).toBe('idle');
    });

    // The NVR login can't change while muted
    it('tells when the cameras are muted or not restored', () => {
        expect(isMuted(build({ timerRunning: true }))).toBe(true);
        expect(isMuted(build({ alertsStillOff: true }))).toBe(true);
        expect(isMuted(build({}))).toBe(false);
    });
});

// How often the status is read
describe('getStatusRefreshInterval', () => {
    // Slow when nothing happens
    it('reads every 2 seconds at rest', () => {
        expect(getStatusRefreshInterval({ now: 10000, fastUntil: 0, isBusy: false })).toBe(STATUS_REFRESH_MS);
        expect(STATUS_REFRESH_MS).toBe(2000);
    });

    // Fast for a few seconds after a command
    it('reads every 500 ms after a command', () => {
        expect(getStatusRefreshInterval({ now: 10000, fastUntil: 14000, isBusy: false })).toBe(FAST_STATUS_REFRESH_MS);
        expect(getStatusRefreshInterval({ now: 14000, fastUntil: 14000, isBusy: false })).toBe(STATUS_REFRESH_MS);
        expect(FAST_STATUS_REFRESH_MS).toBe(500);
    });

    // Fast too while the device works, even if it takes longer than the few seconds
    it('reads every 500 ms while the device is busy', () => {
        expect(getStatusRefreshInterval({ now: 99999, fastUntil: 0, isBusy: true })).toBe(FAST_STATUS_REFRESH_MS);
    });
});

// What the NVR page says about the connection
describe('getNvrStatusDetails', () => {
    // Each state of the firmware has its texts and its tone
    it('describes every NVR state of the firmware', () => {
        expect(getNvrStatusDetails('connected')).toEqual({ key: 'connected', tone: 'green' });
        expect(getNvrStatusDetails('unreachable')).toEqual({ key: 'unreachable', tone: 'red' });
        expect(getNvrStatusDetails('login_refused')).toEqual({ key: 'loginRefused', tone: 'red' });
        expect(getNvrStatusDetails('login_paused')).toEqual({ key: 'loginPaused', tone: 'yellow' });
        expect(getNvrStatusDetails('not_configured').key).toBe('notConfigured');
        expect(getNvrStatusDetails('unknown').key).toBe('unknown');
    });

    // A state of a newer firmware reads as not checked, not as a broken page
    it('reads an unknown state as not checked', () => {
        expect(getNvrStatusDetails('something_new').key).toBe('unknown');
        expect(getNvrStatusDetails(undefined).key).toBe('unknown');
    });

    // The home page points out the problems the user can fix
    it('tells the problems to fix', () => {
        expect(['unreachable', 'login_refused', 'login_paused'].every(hasNvrProblem)).toBe(true);
        expect(['connected', 'unknown', 'not_configured'].some(hasNvrProblem)).toBe(false);
    });
});

// The checks of the NVR form (the device checks again, this saves a request)
describe('findInvalidNvrField', () => {
    // A complete login
    const login = { host: '192.168.1.10', username: 'admin', password: 'secret', hasSavedPassword: false };

    // Everything filled in
    it('finds nothing in a complete login', () => {
        expect(findInvalidNvrField(login)).toBe('');
    });

    // The first wrong field, in the order of the device
    it('finds the first wrong field', () => {
        expect(findInvalidNvrField({ ...login, host: '  ' })).toBe('host');
        expect(findInvalidNvrField({ ...login, host: 'a'.repeat(65) })).toBe('host');
        expect(findInvalidNvrField({ ...login, host: '', username: '' })).toBe('host');
        expect(findInvalidNvrField({ ...login, username: '' })).toBe('username');
        expect(findInvalidNvrField({ ...login, username: 'è'.repeat(17) })).toBe('username');
        expect(findInvalidNvrField({ ...login, password: '' })).toBe('password');
        expect(findInvalidNvrField({ ...login, password: 'p'.repeat(65) })).toBe('password');
    });

    // An empty password keeps the saved one
    it('accepts an empty password when one is saved', () => {
        expect(findInvalidNvrField({ ...login, password: '', hasSavedPassword: true })).toBe('');
    });

    // The body sent to the device
    it('builds the body without the spaces around the address and the username', () => {
        expect(buildNvrBody({ host: ' 192.168.1.10 ', username: ' admin ', password: ' p ' })).toEqual({ host: '192.168.1.10', username: 'admin', password: ' p ' });
        expect(buildNvrBody({ host: 'a', username: 'b', password: undefined })).toEqual({ host: 'a', username: 'b', password: '' });
    });
});
