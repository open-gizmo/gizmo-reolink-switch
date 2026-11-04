import { setResponseStatus } from 'h3';
import { createSuccessResponse, handleEspMockError, readMockJsonBody } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { advanceSwitchState, buildStatusPayload, checkQuietModeCommand, getSwitchMockState, parseQuietModeBody, submitMockCommand } from '~~/server/utils/switch-mock-state.js';

export default defineEventHandler(async event => {
    try {
        // Get the request body
        const body = await readMockJsonBody(event);

        // Read the command (a wrong field is a 400)
        const command = parseQuietModeBody(body);

        // Move the device forward in time, then check that the command can run now (a 409, in the firmware order)
        const state = getSwitchMockState();
        advanceSwitchState({ state });
        checkQuietModeCommand({ state, command });

        // Hand it to the main loop, which runs it while the device is busy
        submitMockCommand({ state, command });

        // Answer with the status (busy until the command has run)
        setResponseStatus(event, 202);
        return createSuccessResponse(buildStatusPayload({ state }));
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'mock_quiet_mode_failed', errorMessage: 'Failed to run the command' });
    }
});
