import { initialStatus } from '~/composables/useQuietMode.js';

// Shared device and UI state for the webapp
export const useGlobalStore = () => useState('global', () => ({
    // Cached device status (what GET /api/status answers, with the defaults of the firmware until it is read)
    status: initialStatus(),

    // Moment the status was read (milliseconds), to count the countdown down between two reads
    statusReceivedAt: 0,

    // Flag to track whether the status has been fetched at least once
    isStatusLoaded: false,

    // Flag set when the status couldn't be fetched (the pages then offer to try again)
    isStatusLoadFailed: false,

    // Moment until which the status is read faster, after a command (milliseconds)
    fastRefreshUntil: 0,

    // Cached NVR login (what GET /api/nvr answers, the password is never sent back)
    nvr: {
        host: '',
        username: '',
        hasPassword: false,
        configured: false
    },

    // Flag to track whether the NVR login has been fetched at least once
    isNvrLoaded: false
}));
