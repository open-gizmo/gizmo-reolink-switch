// Limits of the firmware, counted in bytes like the C strings on the device (nvr_settings.h)
export const NVR_HOST_MAX_LENGTH = 64;
export const NVR_USERNAME_MAX_LENGTH = 32;
export const NVR_PASSWORD_MAX_LENGTH = 64;

// Get the length of a text in bytes, like the device counts it
const getByteLength = text => new TextEncoder().encode(String(text ?? '')).length;

// Find the first field of the login that the device would refuse (host, username or password), or an empty text when all are fine
export const findInvalidNvrField = ({ host, username, password, hasSavedPassword }) => {
    const trimmedHost = String(host ?? '').trim();
    const trimmedUsername = String(username ?? '').trim();

    // The address needs at least one character and fits the device
    if (!trimmedHost || getByteLength(trimmedHost) > NVR_HOST_MAX_LENGTH) return 'host';

    // The username needs at least one character and fits the device
    if (!trimmedUsername || getByteLength(trimmedUsername) > NVR_USERNAME_MAX_LENGTH) return 'username';

    // The password can stay empty only to keep the saved one
    if (getByteLength(password) > NVR_PASSWORD_MAX_LENGTH || (!password && !hasSavedPassword)) return 'password';
    return '';
};

// Build the body to send to the device (the address and the username without the spaces around them, the password as typed)
export const buildNvrBody = ({ host, username, password }) => ({
    host: String(host ?? '').trim(),
    username: String(username ?? '').trim(),
    password: password || ''
});
