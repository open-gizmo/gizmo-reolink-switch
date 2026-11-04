<script setup>
import { computed, ref, watch } from 'vue';
import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import Cancel01Icon from '@hugeicons/core-free-icons/Cancel01Icon';
import CctvIcon from '@hugeicons/core-free-icons/CctvIcon';
import NotificationOff01Icon from '@hugeicons/core-free-icons/NotificationOff01Icon';
import Notification01Icon from '@hugeicons/core-free-icons/Notification01Icon';
import RefreshIcon from '@hugeicons/core-free-icons/RefreshIcon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { Alert, AlertDescription, AlertTitle } from '@brumaombra/ui-vintage/alert';
import { Button } from '@brumaombra/ui-vintage/button';
import { EmptyStateCard } from '@brumaombra/ui-vintage/empty-state-card';
import { PageHeader } from '@brumaombra/ui-vintage/page-header';
import { ResponsiveDialog } from '@brumaombra/ui-vintage/responsive-dialog';
import { SectionTitle } from '@brumaombra/ui-vintage/section-title';
import { buildDurationOptions, clampMinutes, DEFAULT_MINUTES, getHomeState, getNvrStatusDetails, hasNvrProblem, pickDefaultMinutes } from '~/composables/useQuietMode.js';
import { cancelQuietMode, retryRestore, startQuietMode, useCountdown, useStatusPolling } from '~/composables/useSwitchState.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';
import StatusGate from '~/components/device/StatusGate.vue';
import DurationPicker from '~/components/quiet/DurationPicker.vue';
import StatusHero from '~/components/quiet/StatusHero.vue';

const { t } = useI18n();
const globalStore = useGlobalStore();
const isSending = ref(false);
const isCancelDialogOpen = ref(false);
const selectedMinutes = ref(DEFAULT_MINUTES);
let hasPickedDuration = false;

// Reactive device status and what the page shows for it
const status = computed(() => globalStore.value.status);
const homeState = computed(() => getHomeState(status.value));

// Seconds left on the timer, counting down live
const remainingSeconds = useCountdown();

// Durations to pick from, and the selected one kept inside them
const durationOptions = computed(() => buildDurationOptions(status.value.maxMinutes));
const effectiveMinutes = computed(() => clampMinutes({ options: durationOptions.value, minutes: selectedMinutes.value }));

// Whether the device is working: on a command of the device, or on one just sent from this page
const isBusy = computed(() => status.value.busy || isSending.value);

// Problem of the NVR connection to point out on the controls, with the text that explains it
const nvrProblem = computed(() => hasNvrProblem(status.value.nvrStatus) ? getNvrStatusDetails(status.value.nvrStatus) : null);

// Start from the duration the device used last, once the status is known (until the user picks one)
watch(() => globalStore.value.isStatusLoaded, isLoaded => {
    if (!isLoaded || hasPickedDuration) return;
    selectedMinutes.value = pickDefaultMinutes({ options: durationOptions.value, selectedSeconds: status.value.selectedSeconds });
}, { immediate: true });

// Pick a duration
const handleDurationChange = minutes => {
    hasPickedDuration = true;
    selectedMinutes.value = minutes;
};

// Run a command of the device and show the busy state meanwhile
const runCommand = async command => {
    try {
        isSending.value = true;
        await command();
    } finally {
        isSending.value = false;
    }
};

// Start the quiet mode for the selected duration
const handleStartPress = () => {
    if (isBusy.value) return;
    return runCommand(() => startQuietMode(effectiveMinutes.value));
};

// Ask before cancelling (it turns the alerts back on)
const handleCancelPress = () => {
    isCancelDialogOpen.value = true;
};

// Cancel the quiet mode after the confirmation
const handleCancelConfirm = () => {
    isCancelDialogOpen.value = false;
    return runCommand(() => cancelQuietMode());
};

// Try again to turn the alerts on
const handleRetryPress = () => {
    if (isBusy.value) return;
    return runCommand(() => retryRestore());
};

// Read the status while the page is visible
useStatusPolling();

// Page metadata
definePageMeta({
    layout: 'dashboard'
});
</script>

<template>
    <div class="mx-auto w-full max-w-2xl space-y-6">
        <!-- Title -->
        <PageHeader :title="t('pages.home.title')" size="medium" />

        <StatusGate>
            <!-- The NVR is not set up: nothing to mute yet -->
            <EmptyStateCard v-if="homeState === 'setup'" :icon="CctvIcon" :title="t('pages.home.setup.title')" :description="t('pages.home.setup.description')" data-testid="setup-card">
                <template #action>
                    <Button variant="primary" to="/nvr">
                        <HugeiconsIcon :icon="CctvIcon" class="size-4" />
                        {{ t('pages.home.setup.action') }}
                    </Button>
                </template>
            </EmptyStateCard>

            <div v-else class="space-y-6">
                <!-- State: alerts on, quiet mode with the countdown, or alerts still off -->
                <StatusHero :state="homeState" :remaining-seconds="remainingSeconds" :selected-seconds="status.selectedSeconds" :is-busy="isBusy" />

                <!-- Alerts still off: try to turn them on again -->
                <div v-if="homeState === 'alertsOff'" class="space-y-4">
                    <Alert color="yellow">
                        <HugeiconsIcon :icon="Alert02Icon" />
                        <AlertTitle>{{ t('pages.home.alertsOff.title') }}</AlertTitle>
                        <AlertDescription>{{ t('pages.home.alertsOff.description') }}</AlertDescription>
                    </Alert>
                    <Button variant="primary" class="w-full" size="lg" :disabled="isBusy" :loading="isBusy" data-testid="retry-button" @click="handleRetryPress">
                        <HugeiconsIcon :icon="RefreshIcon" class="size-4" />
                        {{ t('pages.home.alertsOff.retryAction') }}
                    </Button>
                </div>

                <!-- Quiet mode running: cancel it -->
                <Button v-else-if="homeState === 'running'" variant="secondary" class="w-full" size="lg" :disabled="isBusy" data-testid="cancel-button" @click="handleCancelPress">
                    <HugeiconsIcon :icon="Notification01Icon" class="size-4" />
                    {{ t('pages.home.cancelAction') }}
                </Button>

                <!-- Alerts on: pick the duration and start -->
                <div v-else class="space-y-4">
                    <!-- The NVR has a problem the user can fix -->
                    <Alert v-if="nvrProblem" :color="nvrProblem.tone" data-testid="nvr-problem">
                        <HugeiconsIcon :icon="Alert02Icon" />
                        <AlertTitle>{{ t(`pages.nvr.status.${nvrProblem.key}.title`) }}</AlertTitle>
                        <AlertDescription>
                            {{ t(`pages.nvr.status.${nvrProblem.key}.description`) }}
                            <NuxtLinkLocale to="/nvr" class="font-semibold underline">{{ t('pages.home.nvrProblemLink') }}</NuxtLinkLocale>
                        </AlertDescription>
                    </Alert>

                    <section>
                        <SectionTitle :title="t('pages.home.duration.title')" :description="t('pages.home.duration.description')" />
                        <DurationPicker :model-value="effectiveMinutes" :max-minutes="status.maxMinutes" :disabled="isBusy" @update:model-value="handleDurationChange" />
                    </section>

                    <Button variant="primary" class="w-full" size="lg" :disabled="isBusy" :loading="isBusy" data-testid="start-button" @click="handleStartPress">
                        <HugeiconsIcon :icon="NotificationOff01Icon" class="size-4" />
                        {{ t('pages.home.startAction', { minutes: effectiveMinutes }) }}
                    </Button>
                </div>
            </div>
        </StatusGate>

        <!-- Confirmation to cancel (a sheet on phones, a dialog on larger screens) -->
        <ResponsiveDialog v-model:open="isCancelDialogOpen" :title="t('pages.home.cancelDialog.title')" :description="t('pages.home.cancelDialog.description')">
            <!-- Actions (the main one first: on top in the sheet, rightmost in the dialog) -->
            <template #footer>
                <Button type="button" variant="primary" data-testid="cancel-confirm-button" @click="handleCancelConfirm">
                    <HugeiconsIcon :icon="Notification01Icon" class="size-4" />
                    {{ t('pages.home.cancelDialog.confirmAction') }}
                </Button>
                <Button type="button" variant="secondary" @click="isCancelDialogOpen = false">
                    <HugeiconsIcon :icon="Cancel01Icon" class="size-4" />
                    {{ t('pages.home.cancelDialog.keepAction') }}
                </Button>
            </template>
        </ResponsiveDialog>
    </div>
</template>
