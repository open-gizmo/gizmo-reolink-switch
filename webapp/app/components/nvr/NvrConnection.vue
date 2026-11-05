<script setup>
import { computed, ref } from 'vue';
import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import CheckmarkCircle02Icon from '@hugeicons/core-free-icons/CheckmarkCircle02Icon';
import Clock01Icon from '@hugeicons/core-free-icons/Clock01Icon';
import HelpCircleIcon from '@hugeicons/core-free-icons/HelpCircleIcon';
import PlugSocketIcon from '@hugeicons/core-free-icons/PlugSocketIcon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { Button } from '@brumaombra/ui-vintage/button';
import { ListGroup } from '@brumaombra/ui-vintage/list';
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

// Icon of each outcome, and the color of the icon for its tone
const STATUS_ICONS = { connected: CheckmarkCircle02Icon, unreachable: Alert02Icon, loginRefused: Alert02Icon, loginPaused: Clock01Icon, unknown: HelpCircleIcon, notConfigured: HelpCircleIcon };
const TONE_CLASSES = { green: 'text-success', red: 'text-destructive', yellow: 'text-warning', blue: 'text-primary', gray: 'text-muted-foreground' };
const icon = computed(() => STATUS_ICONS[details.value.key]);
const toneClass = computed(() => TONE_CLASSES[details.value.tone] || TONE_CLASSES.gray);

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
            <li v-if="isBusy" data-slot="list-row" class="flex items-start gap-2 px-4 py-3 md:gap-3 md:px-5" data-testid="nvr-checking">
                <HugeiconsIcon :icon="PlugSocketIcon" class="mt-0.5 size-4 shrink-0 text-primary" />
                <div class="min-w-0 flex-1">
                    <p class="text-[0.8125rem] font-semibold text-foreground md:text-sm">{{ t('pages.nvr.status.checking.title') }}</p>
                    <p class="mt-1 text-[0.6875rem] text-muted-foreground md:text-xs">{{ t('pages.nvr.status.checking.description') }}</p>
                </div>
            </li>

            <!-- Outcome of the last test -->
            <li v-else data-slot="list-row" class="flex items-start gap-2 px-4 py-3 md:gap-3 md:px-5" :data-testid="`nvr-status-${status.nvrStatus}`">
                <HugeiconsIcon :icon="icon" class="mt-0.5 size-4 shrink-0" :class="toneClass" />
                <div class="min-w-0 flex-1">
                    <p class="text-[0.8125rem] font-semibold text-foreground md:text-sm">{{ t(`pages.nvr.status.${details.key}.title`, { count: cameraCount }, cameraCount) }}</p>
                    <p class="mt-1 text-[0.6875rem] text-muted-foreground md:text-xs">{{ t(`pages.nvr.status.${details.key}.description`) }}</p>
                </div>
            </li>
        </ListGroup>

        <!-- Test button -->
        <Button variant="secondary" class="w-full" :loading="isBusy" :disabled="isBusy || !globalStore.nvr.configured" data-testid="test-button" @click="handleTestPress">
            <HugeiconsIcon :icon="PlugSocketIcon" class="size-4" />
            {{ t('pages.nvr.testAction') }}
        </Button>
    </div>
</template>
