import { setResponseStatus } from 'h3';
import { EspMockError, createSuccessResponse, handleEspMockError } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { advanceSwitchState, buildStatusPayload, getSwitchMockState, isNvrConfigured, submitMockCommand } from '~~/server/utils/switch-mock-state.js';

export default defineEventHandler(async event => {
    try {
        // Move the device forward in time
        const state = getSwitchMockState();
        advanceSwitchState({ state });

        // Nothing to test without a login
        if (!isNvrConfigured(state.nvr)) {
            throw new EspMockError(409, 'nvr_not_configured', 'nvr_not_configured');
        }

        // Hand the test to the main loop (not while it is busy)
        if (state.pendingCommand) {
            throw new EspMockError(409, 'device_busy', 'device_busy');
        }
        submitMockCommand({ state, command: { type: 'test' } });

        // Answer with the status (busy until the test has run)
        setResponseStatus(event, 202);
        return createSuccessResponse(buildStatusPayload({ state }));
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'mock_nvr_test_failed', errorMessage: 'Failed to test the NVR' });
    }
});
