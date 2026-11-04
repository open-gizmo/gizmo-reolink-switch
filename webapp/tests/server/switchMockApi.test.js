import { IncomingMessage, ServerResponse } from 'node:http';
import { Socket } from 'node:net';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { createEvent } from 'h3';

// Stub the Nitro globals the routes of this app use before the handler modules load (the mock of the gizmo-common layer imports its h3 functions, so it runs on real h3 events)
vi.hoisted(() => {
    globalThis.defineEventHandler = handler => handler;
    globalThis.defineNitroPlugin = plugin => plugin;
    globalThis.useAppConfig = () => ({ gizmo: { name: 'Reolink Switch' } });
});

// Load the handlers of the mock device API
const { default: getStatus } = await import('../../server/api/status/index.get.js');
const { default: postQuietMode } = await import('../../server/api/quiet-mode/index.post.js');
const { default: getNvr } = await import('../../server/api/nvr/index.get.js');
const { default: postNvr } = await import('../../server/api/nvr/index.post.js');
const { default: postNvrTest } = await import('../../server/api/nvr-test/index.post.js');
const { default: nameMockDevice } = await import('../../server/plugins/gizmo-mock.js');
const { BUSY_DURATION_MS, LOGIN_PAUSE_MS, MOCK_CAMERA_COUNT, getSwitchMockState, readHost } = await import('../../server/utils/switch-mock-state.js');
const { buildDevicePayload, getGizmoMockState, resetGizmoMockState } = await import('gizmo-common/web/server/utils/gizmo-mock.js');

// Build a real h3 request event, with a JSON body when one is given (a string is sent as it is)
const buildEvent = (body = undefined) => {
    const request = new IncomingMessage(new Socket());
    request.method = 'POST';
    request.rawBody = Buffer.from(body === undefined ? '' : (typeof body === 'string' ? body : JSON.stringify(body)));
    return createEvent(request, new ServerResponse(request));
};

// Call a handler and return its answer with the status code it set
const callHandler = async (handler, event = buildEvent()) => {
    const response = await handler(event);
    return { statusCode: event.node.res.statusCode, response, event };
};

// Send a quiet mode command, a login, a test or read the status and the login
const sendCommand = body => callHandler(postQuietMode, buildEvent(body));
const sendNvr = body => callHandler(postNvr, buildEvent(body));
const sendTest = () => callHandler(postNvrTest);
const readStatus = async () => (await callHandler(getStatus)).response.data;
const readNvr = async () => (await callHandler(getNvr)).response.data;

// Let the device finish what it is doing (a command takes about a second and a half)
const finishBusyTime = () => vi.advanceTimersByTime(BUSY_DURATION_MS + 10);

// Put the device in a state: the login of its NVR, without anything running
const setNvr = ({ host = '192.168.1.10', username = 'admin', password = 'secret' }) => {
    const state = getSwitchMockState();
    state.nvr = { host, username, password };
    state.nvrStatus = host && username && password ? 'connected' : 'not_configured';
};

// Start every test from the demo device the dev server boots with, at a fixed time
beforeEach(() => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date('2026-11-04T10:00:00Z'));
    resetGizmoMockState();
    delete process.env.MOCK_NVR_EMPTY;
});

// Leave the real clock to the other tests
afterEach(() => {
    vi.useRealTimers();
});

// Response shape: the web app reads the same { errorId, errorMessage, data } as on the device
describe('mock device responses', () => {
    // A success has no error and the data
    it('answers a success like the firmware', async () => {
        const { statusCode, response } = await callHandler(getStatus);
        expect(statusCode).toBe(200);
        expect(response.errorId).toBeNull();
        expect(response.errorMessage).toBeNull();
        expect(response.data.maxMinutes).toBe(60);
    });

    // An error is the firmware body with its status code, never an h3 error
    it('answers an error as the firmware body with its status code', async () => {
        const { statusCode, response } = await sendCommand({ action: 'start', minutes: 61 });
        expect(statusCode).toBe(400);
        expect(response).toEqual({ errorId: 'invalid_quiet_mode_field', errorMessage: 'minutes', data: null });
    });

    // Bodies are checked like gizmo-common reads them
    it('refuses invalid, non-object and oversized bodies', async () => {
        const invalidJson = await sendCommand('{nope');
        const arrayBody = await sendNvr([1]);
        const largeBody = await sendNvr({ host: 'a'.repeat(2100) });
        expect([invalidJson.statusCode, invalidJson.response.errorMessage]).toEqual([400, 'Invalid JSON']);
        expect([arrayBody.statusCode, arrayBody.response.errorMessage]).toEqual([400, 'Invalid JSON object']);
        expect([largeBody.statusCode, largeBody.response.errorId]).toEqual([413, 'request_body_too_large']);
    });

    // The device card of the layer shows the name of the mock device
    it('names the mock device like the app', () => {
        nameMockDevice();
        expect(buildDevicePayload({ state: getGizmoMockState() }).name).toBe('Reolink Switch');
    });
});

// GET /api/status: the timer, the alerts and the NVR
describe('mock status', () => {
    // The dashboard needs all of these fields to draw its pages
    it('answers a device with a working NVR and nothing running', async () => {
        const { response } = await callHandler(getStatus);
        expect(response.data).toEqual({
            timerRunning: false,
            remainingSeconds: 0,
            selectedSeconds: 0,
            maxMinutes: 60,
            alertsStillOff: false,
            busy: false,
            nvrStatus: 'connected',
            cameraCount: MOCK_CAMERA_COUNT
        });
    });

    // The dev server can start like a new device, to try the setup
    it('starts without an NVR when MOCK_NVR_EMPTY is 1', async () => {
        process.env.MOCK_NVR_EMPTY = '1';
        resetGizmoMockState();
        expect(await readStatus()).toMatchObject({ nvrStatus: 'not_configured', cameraCount: 0 });
        expect(await readNvr()).toEqual({ host: '', username: '', hasPassword: false, configured: false });
    });
});

// POST /api/quiet-mode: the validation of the firmware
describe('mock quiet mode validation', () => {
    // Every wrong body names its first wrong field
    it.each([
        [{}, 'action'],
        [{ action: 5 }, 'action'],
        [{ action: null }, 'action'],
        [{ action: 'pause' }, 'action'],
        [{ action: 'START' }, 'action'],
        [{ action: 'start' }, 'minutes'],
        [{ action: 'start', minutes: 0 }, 'minutes'],
        [{ action: 'start', minutes: -5 }, 'minutes'],
        [{ action: 'start', minutes: 61 }, 'minutes'],
        [{ action: 'start', minutes: 1.5 }, 'minutes'],
        [{ action: 'start', minutes: '5' }, 'minutes'],
        [{ action: 'start', minutes: null }, 'minutes'],
        [{ action: 'start', minutes: true }, 'minutes']
    ])('refuses %j (field %s)', async (body, field) => {
        const { statusCode, response } = await sendCommand(body);
        expect(statusCode).toBe(400);
        expect(response).toEqual({ errorId: 'invalid_quiet_mode_field', errorMessage: field, data: null });
    });

    // The edges of the range are valid
    it.each([1, 5, 60])('accepts %i minutes', async minutes => {
        const { statusCode } = await sendCommand({ action: 'start', minutes });
        expect(statusCode).toBe(202);
    });

    // The cancel and retry commands take no other field
    it('ignores the fields it does not know', async () => {
        const { statusCode } = await sendCommand({ action: 'start', minutes: 5, color: 'red', seconds: 9999 });
        expect(statusCode).toBe(202);
    });

    // The firmware reads the body before it looks at what the device is doing
    it('checks the body before the state of the device', async () => {
        await sendCommand({ action: 'start', minutes: 5 });
        const { statusCode, response } = await sendCommand({ action: 'start', minutes: 99 });
        expect([statusCode, response.errorId]).toEqual([400, 'invalid_quiet_mode_field']);
    });
});

// POST /api/quiet-mode: the checks against what the device is doing, in the order of the firmware
describe('mock quiet mode commands', () => {
    // A command is taken, and the device is busy until the main loop has run it
    it('answers 202 with the status, busy until the command has run', async () => {
        const { statusCode, response } = await sendCommand({ action: 'start', minutes: 5 });
        expect(statusCode).toBe(202);
        expect(response.errorId).toBeNull();
        expect(response.data).toMatchObject({ busy: true, timerRunning: false });
        expect((await readStatus()).busy).toBe(true);
        finishBusyTime();
        expect(await readStatus()).toMatchObject({ busy: false, timerRunning: true, selectedSeconds: 300 });
    });

    // The countdown goes down with the time
    it('counts the timer down and turns the alerts on when it ends', async () => {
        await sendCommand({ action: 'start', minutes: 2 });
        finishBusyTime();
        expect((await readStatus()).remainingSeconds).toBeLessThanOrEqual(120);
        vi.advanceTimersByTime(60 * 1000);
        const halfway = await readStatus();
        expect(halfway.remainingSeconds).toBeGreaterThan(50);
        expect(halfway.remainingSeconds).toBeLessThanOrEqual(60);
        vi.advanceTimersByTime(61 * 1000);
        expect(await readStatus()).toMatchObject({ timerRunning: false, remainingSeconds: 0, alertsStillOff: false });
    });

    // Another command while the device works used to be accepted
    it('refuses a second command while the device is busy', async () => {
        await sendCommand({ action: 'start', minutes: 5 });
        const { statusCode, response } = await sendCommand({ action: 'cancel' });
        expect([statusCode, response.errorId, response.errorMessage]).toEqual([409, 'device_busy', 'device_busy']);
    });

    // The busy check comes before the others: a cancel while busy is not "not running"
    it('says busy before anything else', async () => {
        await sendCommand({ action: 'start', minutes: 5 });
        const retry = await sendCommand({ action: 'retry' });
        expect(retry.response.errorId).toBe('device_busy');
    });

    // The timer runs once
    it('refuses to start while the timer runs', async () => {
        await sendCommand({ action: 'start', minutes: 5 });
        finishBusyTime();
        const { statusCode, response } = await sendCommand({ action: 'start', minutes: 5 });
        expect([statusCode, response.errorId]).toEqual([409, 'timer_running']);
    });

    // The timer is checked before the NVR
    it('says the timer runs before it says the NVR is missing', async () => {
        await sendCommand({ action: 'start', minutes: 5 });
        finishBusyTime();
        setNvr({ host: '' });
        const { response } = await sendCommand({ action: 'start', minutes: 5 });
        expect(response.errorId).toBe('timer_running');
    });

    // Nothing to mute without an NVR
    it('refuses to start without an NVR login', async () => {
        setNvr({ host: '', username: '', password: '' });
        const { statusCode, response } = await sendCommand({ action: 'start', minutes: 5 });
        expect([statusCode, response.errorId, response.errorMessage]).toEqual([409, 'nvr_not_configured', 'nvr_not_configured']);
    });

    // Nothing to cancel
    it('refuses to cancel when no timer runs', async () => {
        const { statusCode, response } = await sendCommand({ action: 'cancel' });
        expect([statusCode, response.errorId]).toEqual([409, 'timer_not_running']);
    });

    // Nothing to retry
    it('refuses to retry when the alerts are on', async () => {
        const { statusCode, response } = await sendCommand({ action: 'retry' });
        expect([statusCode, response.errorId]).toEqual([409, 'nothing_to_restore']);
    });

    // Cancel turns the alerts back on
    it('cancels the timer', async () => {
        await sendCommand({ action: 'start', minutes: 5 });
        finishBusyTime();
        const { statusCode } = await sendCommand({ action: 'cancel' });
        expect(statusCode).toBe(202);
        finishBusyTime();
        expect(await readStatus()).toMatchObject({ timerRunning: false, alertsStillOff: false, busy: false });
    });

    // An NVR that doesn't answer can't be muted: no timer starts
    it('does not start the timer when the NVR can not be reached', async () => {
        setNvr({ host: '192.168.1.99' });
        await sendCommand({ action: 'start', minutes: 5 });
        finishBusyTime();
        expect(await readStatus()).toMatchObject({ timerRunning: false, nvrStatus: 'unreachable' });
    });

    // The alerts that can't be restored stay off until a retry works (an address ending in .98 shows the warning)
    it('keeps the alerts off when the restore fails, until a retry works', async () => {
        setNvr({ host: '192.168.1.98' });
        await sendCommand({ action: 'start', minutes: 5 });
        finishBusyTime();
        await sendCommand({ action: 'cancel' });
        finishBusyTime();
        expect(await readStatus()).toMatchObject({ timerRunning: false, alertsStillOff: true });

        // The retry is allowed now, and works
        const { statusCode } = await sendCommand({ action: 'retry' });
        expect(statusCode).toBe(202);
        finishBusyTime();
        expect(await readStatus()).toMatchObject({ alertsStillOff: false });
    });

    // The alerts that fail to come back when the timer ends are still off
    it('keeps the alerts off when the restore fails at the end of the timer', async () => {
        setNvr({ host: '10.0.0.98' });
        await sendCommand({ action: 'start', minutes: 1 });
        finishBusyTime();
        vi.advanceTimersByTime(61 * 1000);
        expect(await readStatus()).toMatchObject({ timerRunning: false, alertsStillOff: true });
    });
});

// GET /api/nvr: the login, never the password
describe('mock NVR login', () => {
    // The password is never sent back
    it('answers the login without the password', async () => {
        const { response } = await callHandler(getNvr);
        expect(response.data).toEqual({ host: '192.168.1.10', username: 'admin', hasPassword: true, configured: true });
        expect(JSON.stringify(response)).not.toContain('secret');
    });
});

// POST /api/nvr: the validation of the firmware
describe('mock NVR validation', () => {
    // Every wrong login names its first wrong field, in the order host, username, password
    it.each([
        [{}, 'host'],
        [{ host: 5, username: 'admin', password: 'x' }, 'host'],
        [{ host: '', username: 'admin', password: 'x' }, 'host'],
        [{ host: 'http://', username: 'admin', password: 'x' }, 'host'],
        [{ host: 'https:///', username: 'admin', password: 'x' }, 'host'],
        [{ host: 'my nvr', username: 'admin', password: 'x' }, 'host'],
        [{ host: 'nvr/path', username: 'admin', password: 'x' }, 'host'],
        [{ host: 'nvr.casà', username: 'admin', password: 'x' }, 'host'],
        [{ host: 'a'.repeat(65), username: 'admin', password: 'x' }, 'host'],
        [{ host: '192.168.1.10' }, 'username'],
        [{ host: '192.168.1.10', username: '', password: 'x' }, 'username'],
        [{ host: '192.168.1.10', username: 7, password: 'x' }, 'username'],
        [{ host: '192.168.1.10', username: 'u'.repeat(33), password: 'x' }, 'username'],
        [{ host: '192.168.1.10', username: 'è'.repeat(17), password: 'x' }, 'username'],
        [{ host: '192.168.1.10', username: 'admin', password: 5 }, 'password'],
        [{ host: '192.168.1.10', username: 'admin', password: 'p'.repeat(65) }, 'password']
    ])('refuses %j (field %s)', async (body, field) => {
        const { statusCode, response } = await sendNvr(body);
        expect(statusCode).toBe(400);
        expect(response).toEqual({ errorId: 'invalid_nvr_field', errorMessage: field, data: null });
    });

    // All or nothing: a wrong password must not change the host
    it('changes nothing when a field is refused', async () => {
        await sendNvr({ host: '10.0.0.5', username: 'other', password: 'p'.repeat(65) });
        expect(await readNvr()).toMatchObject({ host: '192.168.1.10', username: 'admin' });
    });

    // The edges of the sizes are valid (in bytes)
    it('accepts the longest host, username and password', async () => {
        const { statusCode } = await sendNvr({ host: 'a'.repeat(64), username: 'u'.repeat(32), password: 'p'.repeat(64) });
        expect(statusCode).toBe(200);
    });

    // The device strips the scheme and the trailing slashes people paste from the browser
    it.each([
        ['http://192.168.1.10/', '192.168.1.10'],
        ['https://nvr.local:443//', 'nvr.local:443'],
        ['[fe80::1]', '[fe80::1]'],
        ['nvr-home.local', 'nvr-home.local']
    ])('saves %s as %s', async (typed, saved) => {
        const { statusCode, response } = await sendNvr({ host: typed, username: 'admin', password: 'x' });
        expect(statusCode).toBe(200);
        expect(response.data.host).toBe(saved);
    });

    // The same rule is exported for the app tests
    it('reads an address like the firmware', () => {
        expect(readHost('http://a.b/')).toBe('a.b');
        expect(readHost('x y')).toBeNull();
        expect(readHost('///')).toBeNull();
    });

    // An empty or missing password keeps the saved one
    it.each([[''], [undefined], [null]])('keeps the saved password when it is %j', async password => {
        const { statusCode, response } = await sendNvr({ host: '192.168.1.11', username: 'root', password });
        expect(statusCode).toBe(200);
        expect(response.data).toEqual({ host: '192.168.1.11', username: 'root', hasPassword: true, configured: true });
        expect(getSwitchMockState().nvr.password).toBe('secret');
    });

    // A new password replaces it
    it('saves a new password', async () => {
        await sendNvr({ host: '192.168.1.10', username: 'admin', password: 'fresh' });
        expect(getSwitchMockState().nvr.password).toBe('fresh');
    });

    // A login needs a password, saved or new
    it('refuses an empty password when none is saved', async () => {
        setNvr({ host: '', username: '', password: '' });
        const { statusCode, response } = await sendNvr({ host: '192.168.1.10', username: 'admin', password: '' });
        expect([statusCode, response.errorMessage]).toEqual([400, 'password']);
    });

    // The response is the saved login, never the password
    it('answers the saved login without the password', async () => {
        const { response } = await sendNvr({ host: '192.168.1.20', username: 'viewer', password: 'p4ss' });
        expect(response.data).toEqual({ host: '192.168.1.20', username: 'viewer', hasPassword: true, configured: true });
        expect(JSON.stringify(response)).not.toContain('p4ss');
    });

    // The muted cameras were muted on the saved NVR: the login can't change, whatever the body says
    it('refuses a change while the timer runs, before it checks the fields', async () => {
        await sendCommand({ action: 'start', minutes: 5 });
        finishBusyTime();
        const { statusCode, response } = await sendNvr({ host: 'not valid', username: '' });
        expect([statusCode, response.errorId, response.errorMessage]).toEqual([409, 'nvr_change_while_muted', 'nvr_change_while_muted']);
        expect(await readNvr()).toMatchObject({ host: '192.168.1.10' });
    });

    // The alerts still off need the saved NVR too
    it('refuses a change while the alerts are still off', async () => {
        setNvr({ host: '192.168.1.98' });
        await sendCommand({ action: 'start', minutes: 5 });
        finishBusyTime();
        await sendCommand({ action: 'cancel' });
        finishBusyTime();
        const { response } = await sendNvr({ host: '192.168.1.10', username: 'admin', password: 'x' });
        expect(response.errorId).toBe('nvr_change_while_muted');
    });

    // A device that can't write its flash answers 500
    it('answers 500 when the login can not be saved', async () => {
        getSwitchMockState().isNvrSaveFailing = true;
        const { statusCode, response } = await sendNvr({ host: '192.168.1.10', username: 'admin', password: 'x' });
        expect([statusCode, response.errorId]).toEqual([500, 'nvr_save_failed']);
    });
});

// The device tests the login by itself after a save, and on POST /api/nvr-test
describe('mock NVR test', () => {
    // A saved login is checked by the device: busy, then the outcome
    it('tests the login after a save', async () => {
        await sendNvr({ host: '192.168.1.10', username: 'admin', password: 'secret' });
        expect(await readStatus()).toMatchObject({ busy: true, nvrStatus: 'unknown', cameraCount: 0 });
        finishBusyTime();
        expect(await readStatus()).toMatchObject({ busy: false, nvrStatus: 'connected', cameraCount: MOCK_CAMERA_COUNT });
    });

    // An address ending in .99 is the NVR that doesn't answer
    it('reports an address ending in .99 as unreachable', async () => {
        await sendNvr({ host: '192.168.1.99', username: 'admin', password: 'secret' });
        finishBusyTime();
        expect(await readStatus()).toMatchObject({ nvrStatus: 'unreachable', cameraCount: 0 });
    });

    // The password "wrong" is the one the NVR refuses
    it('reports the password "wrong" as a refused login', async () => {
        await sendNvr({ host: '192.168.1.10', username: 'admin', password: 'wrong' });
        finishBusyTime();
        expect(await readStatus()).toMatchObject({ nvrStatus: 'login_refused', cameraCount: 0 });
    });

    // After a refusal the device waits, so the NVR doesn't lock the account
    it('pauses the login for ten minutes after a refusal', async () => {
        await sendNvr({ host: '192.168.1.10', username: 'admin', password: 'wrong' });
        finishBusyTime();
        await sendTest();
        finishBusyTime();
        expect((await readStatus()).nvrStatus).toBe('login_paused');

        // After the pause the NVR is tried again
        vi.advanceTimersByTime(LOGIN_PAUSE_MS);
        await sendTest();
        finishBusyTime();
        expect((await readStatus()).nvrStatus).toBe('login_refused');
    });

    // A new login drops the pause, like the firmware does
    it('drops the pause when the login changes', async () => {
        await sendNvr({ host: '192.168.1.10', username: 'admin', password: 'wrong' });
        finishBusyTime();
        await sendNvr({ host: '192.168.1.10', username: 'admin', password: 'secret' });
        finishBusyTime();
        expect(await readStatus()).toMatchObject({ nvrStatus: 'connected' });
    });

    // The test is a command: 202 with the status, busy until it ran
    it('answers 202 with the status, busy until the test is done', async () => {
        const { statusCode, response } = await sendTest();
        expect(statusCode).toBe(202);
        expect(response.data.busy).toBe(true);
        finishBusyTime();
        expect(await readStatus()).toMatchObject({ busy: false, nvrStatus: 'connected', cameraCount: MOCK_CAMERA_COUNT });
    });

    // Nothing to test without a login
    it('refuses to test without an NVR login', async () => {
        setNvr({ host: '', username: '', password: '' });
        const { statusCode, response } = await sendTest();
        expect([statusCode, response.errorId, response.errorMessage]).toEqual([409, 'nvr_not_configured', 'nvr_not_configured']);
    });

    // The missing login is checked before the busy state
    it('says the NVR is missing before it says the device is busy', async () => {
        await sendCommand({ action: 'start', minutes: 5 });
        setNvr({ host: '', username: '', password: '' });
        const { response } = await sendTest();
        expect(response.errorId).toBe('nvr_not_configured');
    });

    // One command at a time
    it('refuses to test while the device is busy', async () => {
        await sendTest();
        const { statusCode, response } = await sendTest();
        expect([statusCode, response.errorId, response.errorMessage]).toEqual([409, 'device_busy', 'device_busy']);
    });

    // The test is allowed while the timer runs (it only reads)
    it('tests the connection while the timer runs', async () => {
        await sendCommand({ action: 'start', minutes: 5 });
        finishBusyTime();
        const { statusCode } = await sendTest();
        expect(statusCode).toBe(202);
    });
});
