<script setup>
import { computed } from 'vue';
import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import Notification01Icon from '@hugeicons/core-free-icons/Notification01Icon';
import NotificationOff01Icon from '@hugeicons/core-free-icons/NotificationOff01Icon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { computeElapsedShare, formatCountdown } from '~/composables/useQuietMode.js';

// Props: what the home page shows (idle, running or alertsOff), the seconds left and the total of the timer
const props = defineProps({
    state: { type: String, required: true },
    remainingSeconds: { type: Number, default: 0 },
    selectedSeconds: { type: Number, default: 0 },
    isBusy: { type: Boolean, default: false }
});

const { t } = useI18n();

// Look of each state: the icon, the colors of the card and of the icon
const LOOKS = {
    idle: { icon: Notification01Icon, card: 'border-green-200 bg-green-50 dark:border-green-800/50 dark:bg-green-900/10', badge: 'bg-green-100 text-green-700 dark:bg-green-900/30 dark:text-green-400' },
    running: { icon: NotificationOff01Icon, card: 'border-primary/40 bg-primary/10', badge: 'bg-primary/20 text-primary' },
    alertsOff: { icon: Alert02Icon, card: 'border-yellow-300 bg-yellow-50 dark:border-yellow-800/50 dark:bg-yellow-900/10', badge: 'bg-yellow-100 text-yellow-700 dark:bg-yellow-900/30 dark:text-yellow-400' }
};
const look = computed(() => LOOKS[props.state] || LOOKS.idle);

// The countdown text and the share of the timer already gone
const countdown = computed(() => formatCountdown(props.remainingSeconds));
const elapsedShare = computed(() => computeElapsedShare({ remainingSeconds: props.remainingSeconds, selectedSeconds: props.selectedSeconds }));
</script>

<template>
    <section class="rounded border p-5 text-center" :class="look.card" aria-live="polite" data-testid="status-hero">
        <!-- Icon -->
        <div class="mx-auto flex size-16 items-center justify-center rounded-full" :class="look.badge">
            <HugeiconsIcon :icon="look.icon" class="size-8" />
        </div>

        <!-- Alerts on -->
        <template v-if="props.state === 'idle'">
            <h2 class="mt-4 text-2xl font-bold text-foreground">{{ t('pages.home.state.idle.title') }}</h2>
            <p class="mt-1 text-sm text-muted-foreground">{{ t('pages.home.state.idle.description') }}</p>
        </template>

        <!-- Quiet mode with the live countdown -->
        <template v-else-if="props.state === 'running'">
            <h2 class="mt-4 text-sm font-semibold uppercase tracking-wide text-muted-foreground">{{ t('pages.home.state.running.title') }}</h2>
            <p class="mt-1 text-5xl font-bold tabular-nums text-foreground" data-testid="countdown">{{ countdown }}</p>
            <p class="mt-1 text-sm text-muted-foreground">{{ t('pages.home.state.running.description') }}</p>
            <div class="mt-4 h-2 overflow-hidden rounded-full bg-primary/20" role="progressbar" :aria-valuenow="Math.round(elapsedShare * 100)" aria-valuemin="0" aria-valuemax="100" :aria-label="t('pages.home.state.running.title')">
                <div class="h-full rounded-full bg-primary transition-[width] duration-500" :style="{ width: `${Math.round(elapsedShare * 100)}%` }" />
            </div>
        </template>

        <!-- Alerts still off -->
        <template v-else>
            <h2 class="mt-4 text-2xl font-bold text-foreground">{{ t('pages.home.state.alertsOff.title') }}</h2>
            <p class="mt-1 text-sm text-muted-foreground">{{ t('pages.home.state.alertsOff.description') }}</p>
        </template>

        <!-- The device is working on a command -->
        <p v-if="props.isBusy" class="mt-3 text-xs font-semibold text-muted-foreground" data-testid="busy-note">{{ t('pages.home.busy') }}</p>
    </section>
</template>
