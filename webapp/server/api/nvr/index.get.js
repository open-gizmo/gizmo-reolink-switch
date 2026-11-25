import { createSuccessResponse, handleEspMockError } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { buildNvrPayload, getEspMockState } from '~~/server/utils/esp-mock-state.js';

export default defineEventHandler(async event => {
    try {
        // Return the login, never the password, like the firmware
        return createSuccessResponse(buildNvrPayload(getEspMockState()));
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'mock_nvr_read_failed', errorMessage: 'Failed to read the NVR login' });
    }
});
