import { configureGizmoMock } from 'gizmo-common/web/server/utils/gizmo-mock.js';

// Name the mock device like the app, so the device card of the layer shows it (the name comes from the app.config.ts)
export default defineNitroPlugin(() => {
    configureGizmoMock({ name: useAppConfig().gizmo.name });
});