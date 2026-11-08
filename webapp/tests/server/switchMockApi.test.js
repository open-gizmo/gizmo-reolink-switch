import { readdirSync } from 'node:fs';
import { join } from 'node:path';
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
const { default: getNvr } = await import('../../server/api/nvr/index.get.js');
const { default: postNvr } = await import('../../server/api/nvr/index.post.js');
const { default: postNvrTest } = await import('../../server/api/nvr-test/index.post.js');
const { default: nameMockDevice } = await import('../../server/plugins/gizmo-mock.js');
const { BUSY_DURATION_MS, LOGIN_PAUSE_MS, MOCK_CAMERA_COUNT, MOCK_RUNNING_MINUTES, getSwitchMockState, readHost } = await import('../../server/utils/switch-mock-state.js');
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

// Send a login, a test or read the status and the login
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

// Put the device in a state with a timer running for some minutes (only the button of the device starts it)
const startTimer = minutes => {
    const state = getSwitchMockState();
    state.isTimerRunning = true;
    state.selectedSeconds = minutes * 60;
    state.timerEndsAt = Date.now() + (minutes * 60 * 1000);
};

// Start every test from the demo device the dev server boots with, at a fixed time
beforeEach(() => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date('2026-11-04T10:00:00Z'));
    resetGizmoMockState();
    delete process.env.MOCK_NVR_EMPTY;
    delete process.env.MOCK_STATE;
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
        expect(response.data).not.toHaveProperty('maxMinutes');
    });

    // An error is the firmware body with its status code, never an h3 error
    it('answers an error as the firmware body with its status code', async () => {
        const { statusCode, response } = await sendNvr({ host: 'my nvr', username: 'admin', password: 'x' });
        expect(statusCode).toBe(400);
        expect(response).toEqual({ errorId: 'invalid_nvr_field', errorMessage: 'host', data: null });
    });

    // Bodies are checked like gizmo-common reads them
    it('refuses invalid, non-object and oversized bodies', async () => {
        const invalidJson = await sendNvr('{nope');
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

// The quiet mode is controlled only with the button of the device: there is no route for the web app
describe('mock quiet mode', () => {
    // The dashboard can't start, cancel or retry it
    it('has no quiet mode route', () => {
        const files = readdirSync(join(process.cwd(), 'server', 'api'));
        expect(files.sort()).toEqual(['nvr', 'nvr-test', 'status']);
    });

    // The timer of the device runs down and turns the alerts back on
    it('counts the timer down and turns the alerts on when it ends', async () => {
        startTimer(2);
        expect((await readStatus()).remainingSeconds).toBeLessThanOrEqual(120);
        vi.advanceTimersByTime(60 * 1000);
        const halfway = await readStatus();
        expect(halfway.remainingSeconds).toBeGreaterThan(50);
        expect(halfway.remainingSeconds).toBeLessThanOrEqual(60);
        vi.advanceTimersByTime(61 * 1000);
        expect(await readStatus()).toMatchObject({ timerRunning: false, remainingSeconds: 0, alertsStillOff: false });
    });

    // The alerts that fail to come back when the timer ends are still off (an address ending in .98)
    it('keeps the alerts off when the restore fails at the end of the timer', async () => {
        setNvr({ host: '10.0.0.98' });
        startTimer(1);
        vi.advanceTimersByTime(61 * 1000);
        expect(await readStatus()).toMatchObject({ timerRunning: false, alertsStillOff: true });
    });

    // The dev server can start with a running timer or with the alerts still off, to look at those pages
    it.each([
        ['running', { timerRunning: true, selectedSeconds: MOCK_RUNNING_MINUTES * 60 }],
        ['alerts_off', { timerRunning: false, alertsStillOff: true }]
    ])('starts in the state %s when MOCK_STATE says so', async (name, expected) => {
        process.env.MOCK_STATE = name;
        resetGizmoMockState();
        expect(await readStatus()).toMatchObject(expected);
        delete process.env.MOCK_STATE;
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

    // An empty or missing password keeps the saved one, while the address and the username stay the same
    it.each([[''], [undefined], [null]])('keeps the saved password when it is %j', async password => {
        const { statusCode, response } = await sendNvr({ host: '192.168.1.10', username: 'admin', password });
        expect(statusCode).toBe(200);
        expect(response.data).toEqual({ host: '192.168.1.10', username: 'admin', hasPassword: true, configured: true });
        expect(getSwitchMockState().nvr.password).toBe('secret');
    });

    // The scheme and the slashes of the address do not make it a different NVR
    it('keeps the saved password when the address only differs by the scheme', async () => {
        const { statusCode } = await sendNvr({ host: 'http://192.168.1.10/', username: 'admin', password: '' });
        expect(statusCode).toBe(200);
        expect(getSwitchMockState().nvr.password).toBe('secret');
    });

    // The saved password only works for the NVR it was entered for: a new address or username needs a new one, and nothing is saved without it
    it.each([
        ['address', { host: '192.168.1.11', username: 'admin' }],
        ['username', { host: '192.168.1.10', username: 'root' }]
    ])('refuses a new %s without a new password', async (name, login) => {
        for (const password of ['', undefined, null]) {
            const { statusCode, response } = await sendNvr({ ...login, password });
            expect(statusCode).toBe(400);
            expect(response).toEqual({ errorId: 'invalid_nvr_field', errorMessage: 'password', data: null });
        }
        expect(await readNvr()).toMatchObject({ host: '192.168.1.10', username: 'admin' });
    });

    // A new password lets the address and the username change
    it('saves a new address and username with a new password', async () => {
        const { statusCode, response } = await sendNvr({ host: '192.168.1.11', username: 'root', password: 'fresh' });
        expect(statusCode).toBe(200);
        expect(response.data).toEqual({ host: '192.168.1.11', username: 'root', hasPassword: true, configured: true });
        expect(getSwitchMockState().nvr.password).toBe('fresh');
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
        startTimer(5);
        const { statusCode, response } = await sendNvr({ host: 'not valid', username: '' });
        expect([statusCode, response.errorId, response.errorMessage]).toEqual([409, 'nvr_change_while_muted', 'nvr_change_while_muted']);
        expect(await readNvr()).toMatchObject({ host: '192.168.1.10' });
    });

    // The alerts still off need the saved NVR too
    it('refuses a change while the alerts are still off', async () => {
        getSwitchMockState().hasPendingRestore = true;
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
        await sendTest();
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
        startTimer(5);
        const { statusCode } = await sendTest();
        expect(statusCode).toBe(202);
    });
});
