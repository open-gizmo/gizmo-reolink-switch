import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue';
import { getNvr, getStatus, saveNvr, testNvr } from '~/composables/useSwitchApi.js';
import { computeRemainingSeconds, FAST_REFRESH_DURATION_MS, getStatusRefreshInterval } from '~/composables/useQuietMode.js';
import { usePolling } from '~/composables/usePolling.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';

// Put a status read from the device in the store and remember when it arrived (the countdown counts from that moment)
const storeStatus = status => {
    const globalStore = useGlobalStore();
    globalStore.value.status = { ...globalStore.value.status, ...status };
    globalStore.value.statusReceivedAt = Date.now();
};

// Read the status again, without telling anyone if it fails (the next read tries again)
export const refreshStatus = async () => {
    const status = await getStatus();
    const globalStore = useGlobalStore();
    storeStatus(status);
    globalStore.value.isStatusLoaded = true;
    globalStore.value.isStatusLoadFailed = false;
};

// Load the status into the store, and remember a failure (so the pages never show the defaults as if they were the device)
export const loadStatus = async () => {
    const globalStore = useGlobalStore();

    try {
        await refreshStatus();
    } catch (error) {
        // Remember the failure, so the pages offer to try again
        globalStore.value.isStatusLoadFailed = true;
        throw error;
    }
};

// Read the status faster for a few seconds (after a command, while the device is busy)
export const startFastRefresh = () => {
    const globalStore = useGlobalStore();
    globalStore.value.fastRefreshUntil = Date.now() + FAST_REFRESH_DURATION_MS;
};

// Run a request that changes the device: the answer is the new status, a refusal is explained to the user and the status is read again (it may have changed meanwhile)
const runDeviceRequest = async request => {
    try {
        // Send the request and show the status it answers
        const status = await request();
        storeStatus(status);
        startFastRefresh();

        // Return that the device took it
        return true;
    } catch (error) {
        // Tell the user, then show the real state of the device again
        handleBackendErrors({ error, defaultMessage: translate('pages.nvr.testError'), showDialog: true });
        await refreshStatus().catch(() => {});
        return false;
    }
};

// Ask the device to test the connection to the NVR
export const runNvrTest = () => runDeviceRequest(() => testNvr());

// Load the NVR login into the store
export const loadNvr = async () => {
    const globalStore = useGlobalStore();
    const nvr = await getNvr();
    globalStore.value.nvr = { ...globalStore.value.nvr, ...nvr };
    globalStore.value.isNvrLoaded = true;
};

// Save the NVR login: the device then tests it by itself (true when it was saved)
export const saveNvrLogin = async login => {
    const globalStore = useGlobalStore();

    try {
        // Save it and show the login as the device kept it
        const nvr = await saveNvr(login);
        globalStore.value.nvr = { ...globalStore.value.nvr, ...nvr };

        // Follow the test the device starts
        startFastRefresh();
        await refreshStatus().catch(() => {});
        return true;
    } catch (error) {
        // Tell the user, then show the real state of the device again
        handleBackendErrors({ error, defaultMessage: translate('pages.nvr.saveError'), showDialog: true });
        await refreshStatus().catch(() => {});
        return false;
    }
};

// Read the status for the poller: the first read also remembers a failure, the next ones just try again
export const pollStatus = async () => {
    const globalStore = useGlobalStore();
    if (globalStore.value.isStatusLoaded) return refreshStatus();
    return loadStatus();
};

// Read the status while the page is visible: every 2 seconds, every 500 ms for a few seconds after a command and while the device is busy
export const useStatusPolling = () => {
    const globalStore = useGlobalStore();

    // Poll while the page is mounted
    const poller = usePolling({
        callback: pollStatus,
        intervalMs: () => getStatusRefreshInterval({
            now: Date.now(),
            fastUntil: globalStore.value.fastRefreshUntil,
            isBusy: globalStore.value.status.busy
        })
    });

    // Read again right away after a command, so the short interval starts now and not after the slow wait that was already scheduled
    watch(() => globalStore.value.fastRefreshUntil, () => poller.refresh());
    return poller;
};

// Get the seconds left on the timer, counting down every second between two reads of the status
export const useCountdown = () => {
    const globalStore = useGlobalStore();
    const now = ref(Date.now());
    let timerId = null;

    // Tick every 250 ms, so the seconds change as soon as they are over
    onMounted(() => {
        timerId = setInterval(() => {
            now.value = Date.now();
        }, 250);
    });

    // Stop ticking when leaving the page
    onBeforeUnmount(() => clearInterval(timerId));

    // Seconds left now (zero while no timer runs)
    return computed(() => {
        if (!globalStore.value.status.timerRunning) return 0;
        return computeRemainingSeconds({
            remainingSeconds: globalStore.value.status.remainingSeconds,
            receivedAt: globalStore.value.statusReceivedAt,
            now: now.value
        });
    });
};
