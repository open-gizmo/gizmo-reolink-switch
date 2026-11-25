import { createSuccessResponse, handleEspMockError } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { advanceSwitchState, buildStatusPayload, getEspMockState } from '~~/server/utils/esp-mock-state.js';

export default defineEventHandler(async event => {
    try {
        // Move the device forward in time (a finished command, an ended timer)
        const state = getEspMockState();
        advanceSwitchState({ state });

        // Return the timer, the alerts and the NVR state, like the firmware
        return createSuccessResponse(buildStatusPayload({ state }));
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'mock_status_read_failed', errorMessage: 'Failed to read the status' });
    }
});
