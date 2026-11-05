<script setup>
import { onMounted, ref } from 'vue';
import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import RefreshIcon from '@hugeicons/core-free-icons/RefreshIcon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { Button } from '@brumaombra/ui-vintage/button';
import { EmptyStateCard } from '@brumaombra/ui-vintage/empty-state-card';
import { LoadingStateCard } from '@brumaombra/ui-vintage/loading-state-card';
import { loadNvr, useStatusPolling } from '~/composables/useSwitchState.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';
import StatusGate from '~/components/device/StatusGate.vue';
import NvrConnection from '~/components/nvr/NvrConnection.vue';
import NvrForm from '~/components/nvr/NvrForm.vue';

const { t } = useI18n();
const globalStore = useGlobalStore();
const isNvrLoadFailed = ref(false);

// Load the saved login (the form shows it, so it waits for it)
const loadLogin = async () => {
    try {
        isNvrLoadFailed.value = false;
        await loadNvr();
    } catch {
        // Remember the failure, so the page offers to try again
        isNvrLoadFailed.value = true;
    }
};

// Read the status while the page is visible (the test outcome comes from it)
useStatusPolling();

// Load the login once if the page has not got it yet
onMounted(() => {
    if (!globalStore.value.isNvrLoaded) loadLogin();
});

// Page metadata
definePageMeta({
    layout: 'dashboard'
});
</script>

<template>
    <GizmoPage :title="t('pages.nvr.title')">
        <StatusGate>
            <!-- The login couldn't be read: offer to try again (an empty form would look like a new device) -->
            <EmptyStateCard v-if="isNvrLoadFailed && !globalStore.isNvrLoaded" :icon="Alert02Icon" :title="t('common.loadErrorTitle')" :description="t('common.loadErrorDescription')">
                <template #action>
                    <Button variant="primary" @click="loadLogin">
                        <HugeiconsIcon :icon="RefreshIcon" class="size-4" />
                        {{ t('common.loadRetryAction') }}
                    </Button>
                </template>
            </EmptyStateCard>

            <!-- Still reading the login from the device -->
            <LoadingStateCard v-else-if="!globalStore.isNvrLoaded" :title="t('common.syncingDevice')" />

            <div v-else class="space-y-6">
                <!-- Address, username and password -->
                <NvrForm />

                <!-- Connection test -->
                <NvrConnection />
            </div>
        </StatusGate>
    </GizmoPage>
</template>
