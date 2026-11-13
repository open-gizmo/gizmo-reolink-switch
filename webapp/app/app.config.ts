import CctvIcon from '@hugeicons/core-free-icons/CctvIcon';
import CpuIcon from '@hugeicons/core-free-icons/CpuIcon';
import Home01Icon from '@hugeicons/core-free-icons/Home01Icon';

// Name, logo and pages of the app, read by the layout and the pages of the gizmo-common layer
export default defineAppConfig({
    gizmo: {
        name: 'Reolink Switch',

        // mDNS name of the firmware (http://gizmo-reolink-switch.local)
        hostname: 'gizmo-reolink-switch',

        logo: '/images/logo/gizmo-reolink-switch-mark.svg',
        logoDark: '/images/logo/gizmo-reolink-switch-mark-dark.svg',
        homePath: '/',

        // Pages of the dashboard, top to bottom in the sidebar and left to right in the bottom navigation ('wifi' is the page of the layer)
        nav: [
            { id: 'home', labelKey: 'nav.home.label', descriptionKey: 'nav.home.description', icon: Home01Icon, to: '/' },
            { id: 'nvr', labelKey: 'nav.nvr.label', descriptionKey: 'nav.nvr.description', icon: CctvIcon, to: '/nvr' },
            { id: 'device', labelKey: 'nav.device.label', descriptionKey: 'nav.device.description', icon: CpuIcon, to: '/device' },
            'wifi'
        ]
    }
});
