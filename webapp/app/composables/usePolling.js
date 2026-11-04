import { onBeforeUnmount, onMounted } from 'vue';

// Call a function again and again while the page is visible: the next call waits for the previous one, and nothing runs while the tab is hidden (the interval can be a function, read before each wait, to poll faster for a while)
export const createPoller = ({ callback, intervalMs }) => {
    let isStarted = false;
    let isRunning = false;
    let timerId = null;

    // Get the wait before the next call
    const readInterval = () => typeof intervalMs === 'function' ? intervalMs() : intervalMs;

    // Check if the page is on screen
    const isPageVisible = () => typeof document === 'undefined' || document.visibilityState !== 'hidden';

    // Run the callback, then schedule the next call
    const tick = async () => {
        timerId = null;
        if (!isStarted || !isPageVisible()) return;
        isRunning = true;

        try {
            await callback();
        } catch {
            // A failed call is just skipped: the next one tries again
        } finally {
            isRunning = false;
        }

        // Schedule the next call unless the poller stopped meanwhile
        if (isStarted && isPageVisible() && !timerId) {
            timerId = setTimeout(tick, readInterval());
        }
    };

    // Stop while hidden, and call again right away when the page comes back
    const handleVisibilityChange = () => {
        if (!isStarted) return;
        if (!isPageVisible()) {
            clearTimeout(timerId);
            timerId = null;
        } else if (!isRunning && !timerId) {
            tick();
        }
    };

    // Start polling (the first call runs right away)
    const start = () => {
        if (isStarted) return;
        isStarted = true;
        document.addEventListener('visibilitychange', handleVisibilityChange);
        tick();
    };

    // Call again now instead of waiting for the scheduled call, so a shorter interval applies at once (a call already running schedules the next one with it)
    const refresh = () => {
        if (!isStarted || isRunning || !isPageVisible()) return;
        clearTimeout(timerId);
        tick();
    };

    // Stop polling and forget the pending call
    const stop = () => {
        isStarted = false;
        clearTimeout(timerId);
        timerId = null;
        document.removeEventListener('visibilitychange', handleVisibilityChange);
    };

    return { start, stop, refresh };
};

// Poll while the component is mounted
export const usePolling = ({ callback, intervalMs }) => {
    const poller = createPoller({ callback, intervalMs });

    // Start when the page opens
    onMounted(() => poller.start());

    // Stop when leaving the page
    onBeforeUnmount(() => poller.stop());

    return poller;
};
