<script setup>
import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import RefreshIcon from '@hugeicons/core-free-icons/RefreshIcon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { Button } from '@brumaombra/ui-vintage/button';
import { EmptyStateCard } from '@brumaombra/ui-vintage/empty-state-card';
import { LoadingStateCard } from '@brumaombra/ui-vintage/loading-state-card';
import { loadStatus } from '~/composables/useSwitchState.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';

const { t } = useI18n();
const globalStore = useGlobalStore();

// Read the status again after a failed read (the page keeps trying by itself, this is for the impatient)
const handleRetryLoadPress = async () => {
    try {
        await loadStatus();
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('common.loadErrorDescription'), showDialog: true });
    }
};
</script>

<template>
    <div>
        <!-- The status couldn't be read: offer to try again (the defaults would look like a real state) -->
        <EmptyStateCard v-if="globalStore.isStatusLoadFailed && !globalStore.isStatusLoaded" :icon="Alert02Icon" :title="t('common.loadErrorTitle')" :description="t('common.loadErrorDescription')">
            <template #action>
                <Button variant="primary" @click="handleRetryLoadPress">
                    <HugeiconsIcon :icon="RefreshIcon" class="size-4" />
                    {{ t('common.loadRetryAction') }}
                </Button>
            </template>
        </EmptyStateCard>

        <!-- Still reading the status from the device -->
        <LoadingStateCard v-else-if="!globalStore.isStatusLoaded" :title="t('common.syncingDevice')" />

        <!-- The page -->
        <slot v-else />
    </div>
</template>
