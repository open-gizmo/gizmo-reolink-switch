import CctvIcon from '@hugeicons/core-free-icons/CctvIcon';
import CpuIcon from '@hugeicons/core-free-icons/CpuIcon';
import ToggleOnIcon from '@hugeicons/core-free-icons/ToggleOnIcon';

// Name, logo and pages of the app, read by the layout and the pages of the gizmo-common layer
export default defineAppConfig({
    gizmo: {
        name: 'Reolink Switch',

        // mDNS name of the firmware (http://gizmo-reolink-switch.local)
        hostname: 'gizmo-reolink-switch',

        logo: '/images/logo/gizmo-reolink-switch-mark.svg',
        logoDark: '/images/logo/gizmo-reolink-switch-mark-dark.svg',
        homePath: '/switch',

        // Pages of the dashboard, top to bottom in the sidebar and left to right in the bottom navigation ('wifi' is the page of the layer)
        nav: [
            { id: 'switch', labelKey: 'nav.switch.label', descriptionKey: 'nav.switch.description', icon: ToggleOnIcon, to: '/switch' },
            { id: 'nvr', labelKey: 'nav.nvr.label', descriptionKey: 'nav.nvr.description', icon: CctvIcon, to: '/nvr' },
            { id: 'device', labelKey: 'nav.device.label', descriptionKey: 'nav.device.description', icon: CpuIcon, to: '/device' },
            'wifi'
        ]
    }
});
