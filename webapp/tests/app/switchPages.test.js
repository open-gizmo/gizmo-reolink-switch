import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { readBody, setResponseStatus } from 'h3';
import { flushPromises } from '@vue/test-utils';
import { mountSuspended, registerEndpoint } from '@nuxt/test-utils/runtime';
import HomePage from '~/pages/index.vue';
import NvrPage from '~/pages/nvr.vue';
import SettingsPage from '~/pages/settings.vue';
import { initialStatus } from '~/composables/useQuietMode.js';
import { loadNvr, loadStatus, refreshStatus, runNvrTest } from '~/composables/useSwitchState.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';

// Record the error dialogs and the toasts instead of rendering them
const dialogs = vi.hoisted(() => []);
const toasts = vi.hoisted(() => []);
vi.mock('@brumaombra/ui-vintage/message-dialog', () => ({
    showMessageDialog: options => {
        dialogs.push(options);
    }
}));
vi.mock('@brumaombra/ui-vintage/message-toast', () => ({
    showMessageToast: options => {
        toasts.push(options);
    }
}));

// Status of a device with a working NVR and nothing running, as GET /api/status answers it
const buildStatus = changes => ({
    timerRunning: false,
    remainingSeconds: 0,
    selectedSeconds: 0,
    alertsStillOff: false,
    busy: false,
    nvrStatus: 'connected',
    cameraCount: 4,
    ...changes
});

// Login of the NVR, as GET /api/nvr answers it
const buildNvr = changes => ({ host: '192.168.1.10', username: 'admin', hasPassword: true, configured: true, ...changes });

// The device API: its status and login, whether it answers, the error to answer to a command, and the requests received
let deviceStatus;
let deviceNvr;
let isDeviceAvailable;
let nextError;
let nvrBodies;
let nvrTestCount;
let statusCount;

// Answer with the error of the firmware (its status code and body), once: the next request works again
const takeNextError = event => {
    if (!nextError) return null;
    const { statusCode, errorId, errorMessage } = nextError;
    nextError = null;
    setResponseStatus(event, statusCode);
    return { errorId, errorMessage: errorMessage || errorId, data: null };
};

// Answer like the device, or fail like a device that doesn't answer
const registerDeviceEndpoints = () => {
    registerEndpoint('/api/status', () => {
        statusCount += 1;
        if (!isDeviceAvailable) throw createError({ statusCode: 500, data: { errorId: null, errorMessage: 'Unreachable', data: null } });
        return { errorId: null, errorMessage: null, data: deviceStatus };
    });
    registerEndpoint('/api/nvr', {
        method: 'GET',
        handler: () => ({ errorId: null, errorMessage: null, data: deviceNvr })
    });
    registerEndpoint('/api/nvr', {
        method: 'POST',
        handler: async event => {
            nvrBodies.push(await readBody(event));
            const refusal = takeNextError(event);
            if (refusal) return refusal;
            const body = nvrBodies[nvrBodies.length - 1];

            // The saved password only works for the same address and username
            const host = body.host.replace(/^https?:\/\//, '').replace(/\/+$/, '');
            if (!body.password && (host !== deviceNvr.host || body.username !== deviceNvr.username)) {
                setResponseStatus(event, 400);
                return { errorId: 'invalid_nvr_field', errorMessage: 'password', data: null };
            }
            deviceNvr = { host, username: body.username, hasPassword: Boolean(body.password) || deviceNvr.hasPassword, configured: true };
            deviceStatus = { ...deviceStatus, nvrStatus: 'connected', cameraCount: 4 };
            return { errorId: null, errorMessage: null, data: deviceNvr };
        }
    });
    registerEndpoint('/api/nvr-test', {
        method: 'POST',
        handler: event => {
            nvrTestCount += 1;
            const refusal = takeNextError(event);
            if (refusal) return refusal;
            return { errorId: null, errorMessage: null, data: deviceStatus };
        }
    });
    registerEndpoint('/api/device', () => ({
        errorId: null,
        errorMessage: null,
        data: { name: 'Reolink Switch', version: 'v2026.11.04.1', uptimeSeconds: 93784, freeHeap: 187432, ssid: 'Home', accessPointActive: false, ip: '192.168.1.42', hostname: 'gizmo-reolink-switch.local', signalQuality: 74 }
    }));
};

// Pages mounted by the test, unmounted afterwards (a page left mounted keeps reading the status)
const mountedWrappers = [];

// Mount a page and remember it
const mountPage = async page => {
    const wrapper = await mountSuspended(page);
    mountedWrappers.push(wrapper);
    return wrapper;
};

// Find a button by its text
const findButton = (wrapper, text) => wrapper.findAll('button').find(button => button.text().includes(text));

// Find a button of the dialogs, which render in a portal outside the mounted page
const findDialogButton = text => [...document.body.querySelectorAll('button')].find(button => button.textContent.includes(text));

// Mount the page and wait until the status is on screen
const mountLoaded = async page => {
    const wrapper = await mountPage(page);
    await vi.waitFor(() => expect(useGlobalStore().value.isStatusLoaded).toBe(true));
    await flushPromises();
    return wrapper;
};

// Start each test with the store of a page that has not loaded anything yet
beforeEach(() => {
    deviceStatus = buildStatus();
    deviceNvr = buildNvr();
    isDeviceAvailable = true;
    nextError = null;
    nvrBodies = [];
    nvrTestCount = 0;
    statusCount = 0;
    dialogs.length = 0;
    toasts.length = 0;
    const globalStore = useGlobalStore();
    globalStore.value.status = initialStatus();
    globalStore.value.statusReceivedAt = 0;
    globalStore.value.isStatusLoaded = false;
    globalStore.value.isStatusLoadFailed = false;
    globalStore.value.fastRefreshUntil = 0;
    globalStore.value.isNvrLoaded = false;
    globalStore.value.nvr = { host: '', username: '', hasPassword: false, configured: false };
    vi.spyOn(console, 'error').mockImplementation(() => {});
    registerDeviceEndpoints();
});

// Unmount the pages and close the dialogs between tests (and let the requests still on their way arrive, so they can't land in the store of the next test)
afterEach(async () => {
    while (mountedWrappers.length) mountedWrappers.pop().unmount();
    document.body.innerHTML = '';
    await new Promise(resolve => setTimeout(resolve, 80));
});

// Loading the status: the pages may only show what the device sent
describe('loadStatus', () => {
    // A loaded status replaces the defaults
    it('loads the status of the device', async () => {
        deviceStatus = buildStatus({ timerRunning: true, remainingSeconds: 90, selectedSeconds: 300 });
        await loadStatus();
        const globalStore = useGlobalStore();
        expect(globalStore.value.isStatusLoaded).toBe(true);
        expect(globalStore.value.status).toMatchObject({ timerRunning: true, remainingSeconds: 90, nvrStatus: 'connected' });
        expect(globalStore.value.statusReceivedAt).toBeGreaterThan(0);
    });

    // A failed load is remembered, and the store keeps its defaults marked as not loaded
    it('remembers a failed load', async () => {
        isDeviceAvailable = false;
        await expect(loadStatus()).rejects.toThrow();
        const globalStore = useGlobalStore();
        expect(globalStore.value.isStatusLoaded).toBe(false);
        expect(globalStore.value.isStatusLoadFailed).toBe(true);
    });

    // A refused test shows the real state again, and the user gets the reason in plain words
    it('returns false and explains the refusal when the device says no', async () => {
        await loadStatus();
        nextError = { statusCode: 409, errorId: 'device_busy' };
        const isTested = await runNvrTest();
        expect(isTested).toBe(false);
        expect(dialogs[0].message).toContain('busy with something else');
    });

    // The login is read without a password
    it('loads the NVR login', async () => {
        await loadNvr();
        expect(useGlobalStore().value.nvr).toEqual(buildNvr());
        expect(useGlobalStore().value.isNvrLoaded).toBe(true);
    });
});

// Home page: the state, read-only (the quiet mode is controlled with the button of the switch)
describe('home page', () => {
    // Every control the page used to have to command the quiet mode
    const COMMAND_SELECTORS = ['start-button', 'cancel-button', 'retry-button', 'cancel-confirm-button'];

    // Check that the page offers no way to command the quiet mode
    const expectNoCommands = wrapper => {
        for (const testId of COMMAND_SELECTORS) expect(wrapper.find(`[data-testid="${testId}"]`).exists(), testId).toBe(false);
        expect(wrapper.findAll('[role="radio"]')).toHaveLength(0);
        expect(wrapper.findAll('button')).toHaveLength(0);
    };

    // While the status is loading the page shows nothing of the device
    it('waits for the status before showing the state', async () => {
        const wrapper = await mountPage(HomePage);
        expect(wrapper.find('[data-testid="status-hero"]').exists()).toBe(false);
        expect(wrapper.text()).toContain('Reading the device');
    });

    // A device that doesn't answer gets a retry, not a page of defaults
    it('offers to try again when the status failed to load', async () => {
        isDeviceAvailable = false;
        const wrapper = await mountPage(HomePage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('Couldn\'t read the device'));
        expect(wrapper.find('[data-testid="status-hero"]').exists()).toBe(false);

        // Try again once the device answers
        isDeviceAvailable = true;
        await findButton(wrapper, 'Try again').trigger('click');
        await vi.waitFor(() => expect(wrapper.find('[data-testid="status-hero"]').exists()).toBe(true));
    });

    // The first thing the page says is that the alerts are on, and that only the button of the switch controls the quiet mode
    it('shows that the alerts are on, with no control and the explanation of the button', async () => {
        const wrapper = await mountLoaded(HomePage);
        expect(wrapper.find('[data-testid="status-hero"]').text()).toContain('Alerts on');
        expectNoCommands(wrapper);
        const help = wrapper.find('[data-testid="button-help"]').text();
        expect(help).toContain('Short press');
        expect(help).toContain('Choose how long the quiet mode lasts');
        expect(help).toContain('Hold the button');
        expect(help).toContain('Start the quiet mode, or cancel it');
        expect(help).toContain('only with the button on the switch');
        expect(wrapper.find('[data-testid="alerts-off-warning"]').exists()).toBe(false);
    });

    // The countdown shows the minutes and seconds the device sent, with nothing to cancel it
    it('shows the time left of a running timer, without a cancel button', async () => {
        deviceStatus = buildStatus({ timerRunning: true, remainingSeconds: 754, selectedSeconds: 900 });
        const wrapper = await mountLoaded(HomePage);
        expect(wrapper.find('[data-testid="countdown"]').text()).toMatch(/^12:3[345]$/);
        expect(wrapper.find('[data-testid="status-hero"]').text()).toContain('left before the alerts come back');
        expectNoCommands(wrapper);
        expect(wrapper.find('[data-testid="button-help"]').exists()).toBe(true);
    });

    // The countdown used to move only with each read of the status, every 2 seconds
    it('counts down every second between two reads', async () => {
        deviceStatus = buildStatus({ timerRunning: true, remainingSeconds: 600, selectedSeconds: 900 });
        const wrapper = await mountLoaded(HomePage);
        const first = wrapper.find('[data-testid="countdown"]').text();
        await vi.waitFor(() => expect(wrapper.find('[data-testid="countdown"]').text()).not.toBe(first), { timeout: 2500, interval: 100 });
    });

    // The way out of the warning is the button of the switch, not the page
    it('warns when the alerts are still off, and says to hold the button to try again', async () => {
        deviceStatus = buildStatus({ alertsStillOff: true });
        const wrapper = await mountLoaded(HomePage);
        expect(wrapper.find('[data-testid="status-hero"]').text()).toContain('Alerts still off');
        const warning = wrapper.find('[data-testid="alerts-off-warning"]').text();
        expect(warning).toContain('Your cameras may not be alerting you');
        expect(warning).toContain('hold the button on the switch to try again');
        expectNoCommands(wrapper);
    });

    // The state follows the device: the page shows the end of the quiet mode by itself
    it('goes back to alerts on when the device ends the quiet mode', async () => {
        deviceStatus = buildStatus({ timerRunning: true, remainingSeconds: 5, selectedSeconds: 60 });
        const wrapper = await mountLoaded(HomePage);
        expect(wrapper.find('[data-testid="status-hero"]').text()).toContain('Quiet mode');
        deviceStatus = buildStatus();
        await vi.waitFor(() => expect(wrapper.find('[data-testid="status-hero"]').text()).toContain('Alerts on'), { timeout: 4000, interval: 100 });
        expect(wrapper.find('[data-testid="countdown"]').exists()).toBe(false);
    });

    // The device works on one thing at a time
    it('shows the busy state while the device works', async () => {
        deviceStatus = buildStatus({ busy: true });
        const wrapper = await mountLoaded(HomePage);
        expect(wrapper.find('[data-testid="busy-note"]').text()).toContain('The device is working');
    });

    // Without an NVR there is nothing to mute: the setup card replaces the page
    it('shows the setup card with a link to the NVR page when the NVR is not set up', async () => {
        deviceStatus = buildStatus({ nvrStatus: 'not_configured', cameraCount: 0 });
        const wrapper = await mountLoaded(HomePage);
        expect(wrapper.find('[data-testid="setup-card"]').text()).toContain('Set up your NVR first');
        expect(wrapper.find('a[href="/nvr"]').exists()).toBe(true);
        expect(wrapper.find('[data-testid="button-help"]').exists()).toBe(false);
        for (const testId of COMMAND_SELECTORS) expect(wrapper.find(`[data-testid="${testId}"]`).exists(), testId).toBe(false);
    });

    // A problem of the NVR is told on the page, with the way to fix it
    it.each([
        ['unreachable', 'Check the address'],
        ['login_refused', 'Check the username and the password'],
        ['login_paused', 'waits 10 minutes']
    ])('points out the NVR problem %s', async (nvrStatus, text) => {
        deviceStatus = buildStatus({ nvrStatus, cameraCount: 0 });
        const wrapper = await mountLoaded(HomePage);
        expect(wrapper.find('[data-testid="nvr-problem"]').text()).toContain(text);
        expect(wrapper.find('[data-testid="nvr-problem"] a[href="/nvr"]').exists()).toBe(true);
        expect(wrapper.find('[data-testid="button-help"]').exists()).toBe(true);
    });

    // The status is polled while the page is open, and no more when it is left
    it('reads the status while mounted and stops when unmounted', async () => {
        const wrapper = await mountLoaded(HomePage);
        const countAtStart = statusCount;
        await vi.waitFor(() => expect(statusCount).toBeGreaterThan(countAtStart), { timeout: 4000, interval: 100 });
        wrapper.unmount();
        mountedWrappers.pop();
        await flushPromises();
        const countAtUnmount = statusCount;
        await new Promise(resolve => setTimeout(resolve, 2600));
        expect(statusCount).toBeLessThanOrEqual(countAtUnmount + 1);
    });
});

// NVR page: the login, the save and the test
describe('NVR page', () => {
    // Type in a field of the form
    const type = async (wrapper, selector, text) => {
        await wrapper.find(selector).setValue(text);
        await flushPromises();
    };

    // Mount the page with the login and the status loaded
    const mountForm = async () => {
        const wrapper = await mountPage(NvrPage);
        await vi.waitFor(() => expect(wrapper.find('#nvr-host').exists()).toBe(true));
        return wrapper;
    };

    // The form starts with the login of the device, and never with its password
    it('starts with the login of the device and an empty password with a saved hint', async () => {
        const wrapper = await mountForm();
        expect(wrapper.find('#nvr-host').element.value).toBe('192.168.1.10');
        expect(wrapper.find('#nvr-username').element.value).toBe('admin');
        expect(wrapper.find('#nvr-password').element.value).toBe('');
        expect(wrapper.find('#nvr-password').attributes('placeholder')).toBe('Saved');
        expect(wrapper.find('[data-testid="password-note"]').text()).toContain('A password is saved');
    });

    // The Save button needs a complete login
    it('needs a password when none is saved', async () => {
        deviceNvr = buildNvr({ host: '', username: '', hasPassword: false, configured: false });
        deviceStatus = buildStatus({ nvrStatus: 'not_configured', cameraCount: 0 });
        const wrapper = await mountForm();
        expect(wrapper.find('#nvr-password').attributes('placeholder')).toBe('Password of the NVR');
        expect(wrapper.find('[data-testid="save-button"]').attributes('disabled')).toBeDefined();
        await type(wrapper, '#nvr-host', '192.168.1.10');
        await type(wrapper, '#nvr-username', 'admin');
        expect(wrapper.find('[data-testid="save-button"]').attributes('disabled')).toBeDefined();
        await type(wrapper, '#nvr-password', 'secret');
        expect(wrapper.find('[data-testid="save-button"]').attributes('disabled')).toBeUndefined();
    });

    // A saved password lets the form be sent without typing it again
    it('lets a login with a saved password be sent without a new one', async () => {
        const wrapper = await mountForm();
        expect(wrapper.find('[data-testid="save-button"]').attributes('disabled')).toBeUndefined();
        await type(wrapper, '#nvr-host', '   ');
        expect(wrapper.find('[data-testid="save-button"]').attributes('disabled')).toBeDefined();
    });

    // The password is hidden, and the eye shows it
    it('shows and hides the password', async () => {
        const wrapper = await mountForm();
        expect(wrapper.find('#nvr-password').attributes('type')).toBe('password');
        await wrapper.find('[data-testid="password-toggle"]').trigger('click');
        expect(wrapper.find('#nvr-password').attributes('type')).toBe('text');
        expect(wrapper.find('[data-testid="password-toggle"]').attributes('aria-label')).toBe('Hide password');
        await wrapper.find('[data-testid="password-toggle"]').trigger('click');
        expect(wrapper.find('#nvr-password').attributes('type')).toBe('password');
    });

    // Save sends the login, keeps the saved password when none is typed (the same NVR), and shows the test
    it('saves the login with an empty password to keep the saved one', async () => {
        const wrapper = await mountForm();
        await type(wrapper, '#nvr-host', ' http://192.168.1.10/ ');
        await wrapper.find('form').trigger('submit');
        await vi.waitFor(() => expect(nvrBodies).toEqual([{ host: 'http://192.168.1.10/', username: 'admin', password: '' }]));
        await vi.waitFor(() => expect(toasts).toHaveLength(1));
        expect(toasts[0].type).toBe('success');
        expect(document.body.textContent).not.toContain('Change the NVR?');
    });

    // A new address or username would delete the saved password: the user is asked before, and nothing is sent
    it.each([
        ['address', '#nvr-host', '192.168.1.20'],
        ['username', '#nvr-username', 'root']
    ])('asks before a new %s deletes the saved password', async (name, selector, value) => {
        const wrapper = await mountForm();
        await type(wrapper, selector, value);
        await wrapper.find('form').trigger('submit');
        await vi.waitFor(() => expect(document.body.textContent).toContain('Change the NVR?'));
        expect(document.body.textContent).toContain('Changing the address or the username deletes it');
        expect(findDialogButton('Change and clear the password')).toBeTruthy();
        expect(nvrBodies).toEqual([]);
    });

    // Keeping the saved NVR puts the saved address and username back
    it('puts the saved login back when the user keeps the saved NVR', async () => {
        const wrapper = await mountForm();
        await type(wrapper, '#nvr-host', '192.168.1.20');
        await type(wrapper, '#nvr-username', 'root');
        await wrapper.find('form').trigger('submit');
        await vi.waitFor(() => expect(findDialogButton('Keep the saved NVR')).toBeTruthy());
        findDialogButton('Keep the saved NVR').click();
        await vi.waitFor(() => expect(document.body.textContent).not.toContain('Change the NVR?'));
        expect(wrapper.find('#nvr-host').element.value).toBe('192.168.1.10');
        expect(wrapper.find('#nvr-username').element.value).toBe('admin');
        expect(wrapper.find('#nvr-password').attributes('placeholder')).toBe('Saved');
        expect(nvrBodies).toEqual([]);
    });

    // Confirming clears the saved password: the form needs the one of the new NVR, and sends it
    it('requires a password after the user confirms, and sends it', async () => {
        const wrapper = await mountForm();
        await type(wrapper, '#nvr-host', '192.168.1.20');
        await wrapper.find('form').trigger('submit');
        await vi.waitFor(() => expect(findDialogButton('Change and clear the password')).toBeTruthy());
        findDialogButton('Change and clear the password').click();
        await vi.waitFor(() => expect(document.body.textContent).not.toContain('Change the NVR?'));

        // The saved password is gone: the field is empty and required, and nothing is sent
        expect(wrapper.find('#nvr-host').element.value).toBe('192.168.1.20');
        expect(wrapper.find('#nvr-password').attributes('placeholder')).toBe('Password of the new NVR');
        expect(wrapper.find('[data-testid="password-note"]').text()).toContain('The saved password was deleted');
        expect(wrapper.find('[data-testid="save-button"]').attributes('disabled')).toBeDefined();
        await wrapper.find('form').trigger('submit');
        expect(nvrBodies).toEqual([]);

        // With the new password the login is sent, without asking again
        await type(wrapper, '#nvr-password', 'n3w-pass');
        expect(wrapper.find('[data-testid="save-button"]').attributes('disabled')).toBeUndefined();
        await wrapper.find('form').trigger('submit');
        await vi.waitFor(() => expect(nvrBodies).toEqual([{ host: '192.168.1.20', username: 'admin', password: 'n3w-pass' }]));
        await vi.waitFor(() => expect(toasts).toHaveLength(1));
        expect(document.body.textContent).not.toContain('Change the NVR?');
        expect(wrapper.find('#nvr-password').attributes('placeholder')).toBe('Saved');
    });

    // A new password with a new address needs no confirmation
    it('saves a new address with a new password without asking', async () => {
        const wrapper = await mountForm();
        await type(wrapper, '#nvr-host', '192.168.1.20');
        await type(wrapper, '#nvr-username', 'root');
        await type(wrapper, '#nvr-password', 'n3w-pass');
        await wrapper.find('form').trigger('submit');
        await vi.waitFor(() => expect(nvrBodies).toEqual([{ host: '192.168.1.20', username: 'root', password: 'n3w-pass' }]));
        expect(document.body.textContent).not.toContain('Change the NVR?');
    });

    // Without a saved password there is nothing to delete
    it('does not ask when no password is saved', async () => {
        deviceNvr = buildNvr({ host: '', username: '', hasPassword: false, configured: false });
        deviceStatus = buildStatus({ nvrStatus: 'not_configured', cameraCount: 0 });
        const wrapper = await mountForm();
        await type(wrapper, '#nvr-host', '192.168.1.20');
        await type(wrapper, '#nvr-username', 'root');
        await type(wrapper, '#nvr-password', 'secret');
        await wrapper.find('form').trigger('submit');
        await vi.waitFor(() => expect(nvrBodies).toHaveLength(1));
        expect(document.body.textContent).not.toContain('Change the NVR?');
    });

    // A new password is sent, and forgotten by the form once saved
    it('sends a new password and clears the field after the save', async () => {
        const wrapper = await mountForm();
        await type(wrapper, '#nvr-password', 'n3w-pass');
        await wrapper.find('form').trigger('submit');
        await vi.waitFor(() => expect(nvrBodies).toEqual([{ host: '192.168.1.10', username: 'admin', password: 'n3w-pass' }]));
        await vi.waitFor(() => expect(wrapper.find('#nvr-password').element.value).toBe(''));
    });

    // The device shows the address without the scheme: so does the form after the save
    it('shows the login as the device kept it', async () => {
        const wrapper = await mountForm();
        await type(wrapper, '#nvr-host', 'http://nvr.local/');
        await type(wrapper, '#nvr-password', 'n3w-pass');
        await wrapper.find('form').trigger('submit');
        await vi.waitFor(() => expect(nvrBodies).toHaveLength(1));
        deviceNvr = buildNvr({ host: 'nvr.local' });
        await vi.waitFor(() => expect(toasts).toHaveLength(1));
    });

    // What the page says about each outcome of the test
    it.each([
        ['connected', 4, 'Connected: 4 cameras online'],
        ['connected', 1, 'Connected: 1 camera online'],
        ['unreachable', 0, 'Check the address'],
        ['login_refused', 0, 'Check the username and the password'],
        ['login_paused', 0, 'waits 10 minutes before it tries again'],
        ['unknown', 0, 'Not checked yet']
    ])('shows the outcome %s with %i cameras', async (nvrStatus, cameraCount, text) => {
        deviceStatus = buildStatus({ nvrStatus, cameraCount });
        const wrapper = await mountForm();
        await vi.waitFor(() => expect(wrapper.find(`[data-testid="nvr-status-${nvrStatus}"]`).text()).toContain(text));
    });

    // The Test connection button asks the device to log in, and the page follows the status
    it('tests the connection and shows the outcome', async () => {
        deviceStatus = buildStatus({ nvrStatus: 'unknown', cameraCount: 0 });
        const wrapper = await mountForm();
        deviceStatus = buildStatus({ nvrStatus: 'login_refused', cameraCount: 0 });
        await wrapper.find('[data-testid="test-button"]').trigger('click');
        await vi.waitFor(() => expect(nvrTestCount).toBe(1));
        await vi.waitFor(() => expect(wrapper.find('[data-testid="nvr-status-login_refused"]').exists()).toBe(true));
    });

    // The device tests one thing at a time
    it('shows the checking state and blocks the test while the device is busy', async () => {
        deviceStatus = buildStatus({ busy: true });
        const wrapper = await mountForm();
        await vi.waitFor(() => expect(wrapper.find('[data-testid="nvr-checking"]').exists()).toBe(true));
        expect(wrapper.find('[data-testid="test-button"]').attributes('disabled')).toBeDefined();
        expect(wrapper.find('[data-testid="save-button"]').attributes('disabled')).toBeDefined();
    });

    // There is nothing to test before a login is saved
    it('blocks the test while the NVR is not set up', async () => {
        deviceNvr = buildNvr({ host: '', username: '', hasPassword: false, configured: false });
        deviceStatus = buildStatus({ nvrStatus: 'not_configured', cameraCount: 0 });
        const wrapper = await mountForm();
        expect(wrapper.find('[data-testid="test-button"]').attributes('disabled')).toBeDefined();
        expect(wrapper.find('[data-testid="nvr-status-not_configured"]').text()).toContain('Not set up yet');
    });

    // The device refuses a login change while the cameras are muted, so the form says why and can't be sent
    it.each([
        ['the timer runs', { timerRunning: true, remainingSeconds: 300, selectedSeconds: 600 }],
        ['the alerts are still off', { alertsStillOff: true }]
    ])('locks the form while %s', async (name, changes) => {
        deviceStatus = buildStatus(changes);
        const wrapper = await mountForm();
        expect(wrapper.find('[data-testid="nvr-locked"]').text()).toContain('The login is locked for now');
        expect(wrapper.find('#nvr-host').attributes('disabled')).toBeDefined();
        expect(wrapper.find('#nvr-username').attributes('disabled')).toBeDefined();
        expect(wrapper.find('#nvr-password').attributes('disabled')).toBeDefined();
        expect(wrapper.find('[data-testid="save-button"]').attributes('disabled')).toBeDefined();
        await wrapper.find('form').trigger('submit');
        expect(nvrBodies).toEqual([]);
    });

    // The lock follows the status: it goes away when the timer ends
    it('unlocks the form when the timer ends', async () => {
        deviceStatus = buildStatus({ timerRunning: true, remainingSeconds: 5, selectedSeconds: 60 });
        const wrapper = await mountForm();
        expect(wrapper.find('[data-testid="nvr-locked"]').exists()).toBe(true);
        deviceStatus = buildStatus();
        await vi.waitFor(() => expect(wrapper.find('[data-testid="nvr-locked"]').exists()).toBe(false), { timeout: 4000, interval: 100 });
        expect(wrapper.find('#nvr-host').attributes('disabled')).toBeUndefined();
    });

    // Errors of the device are explained in plain words
    it.each([
        [409, 'nvr_change_while_muted', 'can\'t change while quiet mode is on'],
        [400, 'invalid_nvr_field', 'didn\'t accept the NVR address'],
        [500, 'nvr_save_failed', 'could not save the NVR login']
    ])('explains the error %i %s when saving', async (statusCode, errorId, text) => {
        const wrapper = await mountForm();
        nextError = { statusCode, errorId };
        await wrapper.find('form').trigger('submit');
        await vi.waitFor(() => expect(dialogs).toHaveLength(1));
        expect(dialogs[0].message).toContain(text);
        expect(toasts).toEqual([]);
    });

    // The errors of the test are explained too
    it.each([
        [409, 'nvr_not_configured', 'not set up yet'],
        [409, 'device_busy', 'busy with something else']
    ])('explains the error %i %s when testing', async (statusCode, errorId, text) => {
        const wrapper = await mountForm();
        nextError = { statusCode, errorId };
        await wrapper.find('[data-testid="test-button"]').trigger('click');
        await vi.waitFor(() => expect(dialogs).toHaveLength(1));
        expect(dialogs[0].message).toContain(text);
    });

    // A login that can't be read must not show an empty form that would save over the real one
    it('offers to try again when the login failed to load', async () => {
        let isLoginAvailable = false;
        registerEndpoint('/api/nvr', {
            method: 'GET',
            handler: () => {
                if (!isLoginAvailable) throw createError({ statusCode: 500, data: { errorId: null, errorMessage: 'Unreachable', data: null } });
                return { errorId: null, errorMessage: null, data: deviceNvr };
            }
        });
        const wrapper = await mountPage(NvrPage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('Couldn\'t read the device'));
        expect(wrapper.find('#nvr-host').exists()).toBe(false);

        // Try again once the device answers
        isLoginAvailable = true;
        await findButton(wrapper, 'Try again').trigger('click');
        await vi.waitFor(() => expect(wrapper.find('#nvr-host').element.value).toBe('192.168.1.10'));
    });
});

// Settings page: the device card of the layer (the language is picked in the top bar, like in every OpenGizmo app)
describe('settings page', () => {
    // The card of the layer shows the device, and there is no language row
    it('shows the device card and no language row', async () => {
        const wrapper = await mountPage(SettingsPage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('gizmo-reolink-switch.local'));
        expect(wrapper.text()).toContain('Restart');
        expect(wrapper.text()).not.toContain('Language');
    });

    // A refreshed status is stamped, so the countdown has a start
    it('keeps the status helpers in sync with the store', async () => {
        await refreshStatus();
        expect(useGlobalStore().value.statusReceivedAt).toBeGreaterThan(0);
    });
});
