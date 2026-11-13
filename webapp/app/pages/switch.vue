<script setup>
import { computed } from 'vue';
import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import CctvIcon from '@hugeicons/core-free-icons/CctvIcon';
import NotificationOff01Icon from '@hugeicons/core-free-icons/NotificationOff01Icon';
import Timer01Icon from '@hugeicons/core-free-icons/Timer01Icon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { Alert, AlertDescription, AlertTitle } from '@brumaombra/ui-vintage/alert';
import { Button } from '@brumaombra/ui-vintage/button';
import { EmptyStateCard } from '@brumaombra/ui-vintage/empty-state-card';
import { ListGroup, ListRow } from '@brumaombra/ui-vintage/list';
import { getHomeState, getNvrStatusDetails, hasNvrProblem } from '~/composables/useQuietMode.js';
import { useCountdown, useStatusPolling } from '~/composables/useSwitchState.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';
import StatusGate from '~/components/device/StatusGate.vue';
import StatusHero from '~/components/quiet/StatusHero.vue';

const { t } = useI18n();
const globalStore = useGlobalStore();

// Reactive device status and what the page shows for it
const status = computed(() => globalStore.value.status);
const homeState = computed(() => getHomeState(status.value));

// Seconds left on the timer, counting down live
const remainingSeconds = useCountdown();

// Problem of the NVR connection to point out, with the text that explains it
const nvrProblem = computed(() => hasNvrProblem(status.value.nvrStatus) ? getNvrStatusDetails(status.value.nvrStatus) : null);

// Read the status while the page is visible
useStatusPolling();

// Page metadata
definePageMeta({
    layout: 'dashboard'
});
</script>

<template>
    <GizmoPage :title="t('pages.switch.title')">
        <StatusGate>
            <!-- The NVR is not set up: nothing to mute yet -->
            <EmptyStateCard v-if="homeState === 'setup'" :icon="CctvIcon" :title="t('pages.switch.setup.title')" :description="t('pages.switch.setup.description')" data-testid="setup-card">
                <template #action>
                    <Button variant="primary" to="/nvr">
                        <HugeiconsIcon :icon="CctvIcon" class="size-4" />
                        {{ t('pages.switch.setup.action') }}
                    </Button>
                </template>
            </EmptyStateCard>

            <div v-else class="space-y-6">
                <!-- State: alerts on, quiet mode with the countdown, or alerts still off -->
                <StatusHero :state="homeState" :remaining-seconds="remainingSeconds" :selected-seconds="status.selectedSeconds" :is-busy="status.busy" />

                <!-- Alerts still off: the way out is the button on the switch -->
                <Alert v-if="homeState === 'alertsOff'" color="yellow" data-testid="alerts-off-warning">
                    <HugeiconsIcon :icon="Alert02Icon" />
                    <AlertTitle>{{ t('pages.switch.alertsOff.title') }}</AlertTitle>
                    <AlertDescription>{{ t('pages.switch.alertsOff.description') }}</AlertDescription>
                </Alert>

                <!-- The NVR has a problem the user can fix -->
                <Alert v-else-if="nvrProblem && homeState === 'idle'" :color="nvrProblem.tone" data-testid="nvr-problem">
                    <HugeiconsIcon :icon="Alert02Icon" />
                    <AlertTitle>{{ t(`pages.nvr.status.${nvrProblem.key}.title`) }}</AlertTitle>
                    <AlertDescription>
                        {{ t(`pages.nvr.status.${nvrProblem.key}.description`) }}
                        <NuxtLinkLocale to="/nvr" class="font-semibold underline">{{ t('pages.switch.nvrProblemLink') }}</NuxtLinkLocale>
                    </AlertDescription>
                </Alert>

                <!-- How to control the quiet mode: only with the button on the switch, this page just shows it -->
                <ListGroup :title="t('pages.switch.buttonHelp.title')" :hint="t('pages.switch.buttonHelp.hint')" data-testid="button-help">
                    <ListRow :label="t('pages.switch.buttonHelp.shortPress.label')" :description="t('pages.switch.buttonHelp.shortPress.description')" :icon="Timer01Icon" />
                    <ListRow :label="t('pages.switch.buttonHelp.longPress.label')" :description="t('pages.switch.buttonHelp.longPress.description')" :icon="NotificationOff01Icon" />
                </ListGroup>
            </div>
        </StatusGate>
    </GizmoPage>
</template>
