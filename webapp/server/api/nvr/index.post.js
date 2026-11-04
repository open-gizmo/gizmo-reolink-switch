import { EspMockError, createSuccessResponse, handleEspMockError, readMockJsonBody } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { advanceSwitchState, applyNvrBody, buildNvrPayload, getSwitchMockState, saveNvr, submitMockCommand } from '~~/server/utils/switch-mock-state.js';

export default defineEventHandler(async event => {
    try {
        // Get the request body
        const body = await readMockJsonBody(event);

        // Refuse while the cameras are muted (they were muted on the saved NVR, which must restore them)
        const state = getSwitchMockState();
        advanceSwitchState({ state });
        if (state.isTimerRunning || state.hasPendingRestore) {
            throw new EspMockError(409, 'nvr_change_while_muted', 'nvr_change_while_muted');
        }

        // Check the fields in the firmware order (nothing is saved if one is wrong)
        const nvr = applyNvrBody({ body, current: state.nvr });

        // Save the login
        if (state.isNvrSaveFailing) {
            throw new EspMockError(500, 'nvr_save_failed', 'Failed to save the NVR login');
        }
        saveNvr({ state, nvr });

        // Check the new login (skipped if another command is waiting: the web app can test it again)
        if (!state.pendingCommand) {
            submitMockCommand({ state, command: { type: 'test' } });
        }

        // Answer with the saved login
        return createSuccessResponse(buildNvrPayload(state));
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'mock_nvr_save_failed', errorMessage: 'Failed to save the NVR login' });
    }
});
