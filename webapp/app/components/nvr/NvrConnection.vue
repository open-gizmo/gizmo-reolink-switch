<script setup>
import { computed, ref } from 'vue';
import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import CheckmarkCircle02Icon from '@hugeicons/core-free-icons/CheckmarkCircle02Icon';
import Clock01Icon from '@hugeicons/core-free-icons/Clock01Icon';
import HelpCircleIcon from '@hugeicons/core-free-icons/HelpCircleIcon';
import PlugSocketIcon from '@hugeicons/core-free-icons/PlugSocketIcon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { Button } from '@brumaombra/ui-vintage/button';
import { ListGroup, ListRow } from '@brumaombra/ui-vintage/list';
import { getNvrStatusDetails } from '~/composables/useQuietMode.js';
import { runNvrTest } from '~/composables/useSwitchState.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';

const { t } = useI18n();
const globalStore = useGlobalStore();
const isSending = ref(false);

// Reactive device status and what to say about the NVR connection
const status = computed(() => globalStore.value.status);
const details = computed(() => getNvrStatusDetails(status.value.nvrStatus));
const isBusy = computed(() => status.value.busy || isSending.value);

// Icon of each outcome
const STATUS_ICONS = { connected: CheckmarkCircle02Icon, unreachable: Alert02Icon, loginRefused: Alert02Icon, loginPaused: Clock01Icon, unknown: HelpCircleIcon, notConfigured: HelpCircleIcon };
const icon = computed(() => STATUS_ICONS[details.value.key]);

// Number of online cameras for the plural texts
const cameraCount = computed(() => status.value.cameraCount || 0);

// Ask the device to test the connection
const handleTestPress = async () => {
    if (isBusy.value) return;

    try {
        isSending.value = true;
        await runNvrTest();
    } finally {
        isSending.value = false;
    }
};
</script>

<template>
    <div class="space-y-6">
        <!-- Whether the device can reach the NVR -->
        <ListGroup :title="t('pages.nvr.connectionTitle')" :hint="t('pages.nvr.connectionDescription')">
            <!-- The device is logging in -->
            <ListRow v-if="isBusy" :label="t('pages.nvr.status.checking.title')" :description="t('pages.nvr.status.checking.description')" :icon="PlugSocketIcon" data-testid="nvr-checking" />

            <!-- Outcome of the last test -->
            <ListRow v-else :label="t(`pages.nvr.status.${details.key}.title`, { count: cameraCount }, cameraCount)" :description="t(`pages.nvr.status.${details.key}.description`)" :icon="icon" :data-testid="`nvr-status-${status.nvrStatus}`" />
        </ListGroup>

        <!-- Test button -->
        <Button variant="secondary" class="w-full" :loading="isBusy" :disabled="isBusy || !globalStore.nvr.configured" data-testid="test-button" @click="handleTestPress">
            <HugeiconsIcon :icon="PlugSocketIcon" class="size-4" />
            {{ t('pages.nvr.testAction') }}
        </Button>
    </div>
</template>
