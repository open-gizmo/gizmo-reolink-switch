import Home01Icon from '@hugeicons/core-free-icons/Home01Icon';
import CctvIcon from '@hugeicons/core-free-icons/CctvIcon';
import Settings01Icon from '@hugeicons/core-free-icons/Settings01Icon';

// Name, logo and pages of the app, read by the layout and the pages of the gizmo-common layer
export default defineAppConfig({
    gizmo: {
        name: 'Reolink Switch',
        logo: '/logo.svg',
        homePath: '/',

        // Pages of the dashboard, top to bottom in the sidebar and left to right in the bottom navigation ('wifi' is the page of the layer)
        nav: [
            { id: 'home', labelKey: 'nav.home.label', descriptionKey: 'nav.home.description', icon: Home01Icon, to: '/' },
            { id: 'nvr', labelKey: 'nav.nvr.label', descriptionKey: 'nav.nvr.description', icon: CctvIcon, to: '/nvr' },
            { id: 'settings', labelKey: 'nav.settings.label', descriptionKey: 'nav.settings.description', icon: Settings01Icon, to: '/settings' },
            'wifi'
        ]
    }
});
