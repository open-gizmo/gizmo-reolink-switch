import { createSuccessResponse, handleEspMockError } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { buildNvrPayload, getSwitchMockState } from '~~/server/utils/switch-mock-state.js';

export default defineEventHandler(async event => {
    try {
        // Return the login, never the password, like the firmware
        return createSuccessResponse(buildNvrPayload(getSwitchMockState()));
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'mock_nvr_read_failed', errorMessage: 'Failed to read the NVR login' });
    }
});
