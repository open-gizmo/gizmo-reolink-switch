// The request wrapper (fetchEsp) and the Wi-Fi and device calls come from the gizmo-common layer: only the calls of the Reolink Switch are here

// Fetch the status: the timer, the alerts and the NVR connection
export const getStatus = async () => {
    const response = await fetchEsp('/api/status', { retry: 0 });
    return response.data || {};
};

// Fetch the NVR login (without the password)
export const getNvr = async () => {
    const response = await fetchEsp('/api/nvr', { retry: 0 });
    return response.data || {};
};

// Save the NVR login ({ host, username, password }, an empty password keeps the saved one only if the address and the username are unchanged) and get the login as the device kept it
export const saveNvr = async login => {
    // Call the device API to save the login
    const response = await fetchEsp('/api/nvr', {
        method: 'POST',
        body: login,
        retry: 0
    });

    // Return the saved login
    return response.data || {};
};

// Ask the device to log in to the NVR and count the online cameras, and get the status (busy until the test is done)
export const testNvr = async () => {
    const response = await fetchEsp('/api/nvr-test', { method: 'POST', retry: 0 });
    return response.data || {};
};
