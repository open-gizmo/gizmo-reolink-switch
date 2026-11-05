<script setup>
import { computed, ref } from 'vue';
import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import CctvIcon from '@hugeicons/core-free-icons/CctvIcon';
import CheckmarkCircle02Icon from '@hugeicons/core-free-icons/CheckmarkCircle02Icon';
import LockPasswordIcon from '@hugeicons/core-free-icons/LockPasswordIcon';
import SaveIcon from '@hugeicons/core-free-icons/SaveIcon';
import UserIcon from '@hugeicons/core-free-icons/UserIcon';
import ViewIcon from '@hugeicons/core-free-icons/ViewIcon';
import ViewOffIcon from '@hugeicons/core-free-icons/ViewOffIcon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { Alert, AlertDescription, AlertTitle } from '@brumaombra/ui-vintage/alert';
import { Button } from '@brumaombra/ui-vintage/button';
import { Input } from '@brumaombra/ui-vintage/input';
import { ListGroup, ListRow } from '@brumaombra/ui-vintage/list';
import { showMessageToast } from '@brumaombra/ui-vintage/message-toast';
import { buildNvrBody, findInvalidNvrField, NVR_HOST_MAX_LENGTH, NVR_PASSWORD_MAX_LENGTH, NVR_USERNAME_MAX_LENGTH } from '~/composables/useNvrForm.js';
import { isMuted } from '~/composables/useQuietMode.js';
import { saveNvrLogin } from '~/composables/useSwitchState.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';

const { t } = useI18n();
const globalStore = useGlobalStore();
const isSaving = ref(false);
const isPasswordVisible = ref(false);

// The login being edited (it starts as the one of the device, and is only sent with Save; the password is never sent back, so it starts empty)
const host = ref(globalStore.value.nvr.host);
const username = ref(globalStore.value.nvr.username);
const password = ref('');

// Help under the group: what the login is, and what the address can be (a field has no room for its own note)
const loginHint = computed(() => `${t('pages.nvr.loginDescription')} ${t('pages.nvr.host.note')}`);

// Reactive device state
const nvr = computed(() => globalStore.value.nvr);

// The login can't change while the cameras are muted (the device would lose the NVR that must restore them)
const isLocked = computed(() => isMuted(globalStore.value.status));

// Whether the form can be sent, and the first field that is wrong
const invalidField = computed(() => findInvalidNvrField({ host: host.value, username: username.value, password: password.value, hasSavedPassword: nvr.value.hasPassword }));
const isBusy = computed(() => isSaving.value || globalStore.value.status.busy);

// Save the login on the device, which then tests it by itself
const handleSave = async () => {
    if (invalidField.value || isLocked.value || isBusy.value) return;

    try {
        isSaving.value = true;
        const isSaved = await saveNvrLogin(buildNvrBody({ host: host.value, username: username.value, password: password.value }));

        // Show the login as the device kept it (without the scheme and the slashes), and forget the typed password
        if (isSaved) {
            host.value = nvr.value.host;
            username.value = nvr.value.username;
            password.value = '';
            isPasswordVisible.value = false;
            showMessageToast({ type: 'success', icon: CheckmarkCircle02Icon, message: t('pages.nvr.saveSuccess') });
        }
    } finally {
        isSaving.value = false;
    }
};
</script>

<template>
    <form class="space-y-6" novalidate @submit.prevent="handleSave">
        <!-- The login can't change now -->
        <Alert v-if="isLocked" color="yellow" data-testid="nvr-locked">
            <HugeiconsIcon :icon="Alert02Icon" />
            <AlertTitle>{{ t('pages.nvr.locked.title') }}</AlertTitle>
            <AlertDescription>{{ t('pages.nvr.locked.description') }}</AlertDescription>
        </Alert>

        <!-- The three fields of the login, one row each: icon and label, the field full width under them -->
        <ListGroup :title="t('pages.nvr.loginTitle')" :hint="loginHint">
            <!-- Address -->
            <ListRow :label="t('pages.nvr.host.label')" :icon="CctvIcon" label-for="nvr-host">
                <Input id="nvr-host" v-model="host" autocapitalize="none" spellcheck="false" :maxlength="NVR_HOST_MAX_LENGTH" :disabled="isLocked" :placeholder="t('pages.nvr.host.placeholder')" />
            </ListRow>

            <!-- Username -->
            <ListRow :label="t('pages.nvr.username.label')" :icon="UserIcon" label-for="nvr-username">
                <Input id="nvr-username" v-model="username" autocapitalize="none" spellcheck="false" :maxlength="NVR_USERNAME_MAX_LENGTH" :disabled="isLocked" :placeholder="t('pages.nvr.username.placeholder')" />
            </ListRow>

            <!-- Password (never sent back by the device: empty, with a hint when one is saved), with its show/hide toggle on the label line -->
            <ListRow :label="t('pages.nvr.password.label')" :icon="LockPasswordIcon" label-for="nvr-password">
                <template #trailing>
                    <Button variant="ghost" size="icon-sm" type="button" :aria-label="isPasswordVisible ? t('pages.nvr.password.hide') : t('pages.nvr.password.show')" :aria-pressed="isPasswordVisible" :disabled="isLocked" data-testid="password-toggle" @click="isPasswordVisible = !isPasswordVisible">
                        <HugeiconsIcon :icon="isPasswordVisible ? ViewOffIcon : ViewIcon" class="size-4" />
                    </Button>
                </template>

                <!-- Password -->
                <Input id="nvr-password" v-model="password" :type="isPasswordVisible ? 'text' : 'password'" autocomplete="new-password" autocapitalize="none" spellcheck="false" :maxlength="NVR_PASSWORD_MAX_LENGTH" :disabled="isLocked" :placeholder="nvr.hasPassword ? t('pages.nvr.password.savedPlaceholder') : t('pages.nvr.password.placeholder')" />
                <p class="mt-2 text-[0.6875rem] text-muted-foreground md:text-xs" data-testid="password-note">{{ nvr.hasPassword ? t('pages.nvr.password.savedNote') : t('pages.nvr.password.note') }}</p>
            </ListRow>
        </ListGroup>

        <!-- Save button -->
        <Button variant="primary" type="submit" class="w-full" :loading="isSaving" :disabled="Boolean(invalidField) || isLocked || isBusy" data-testid="save-button">
            <HugeiconsIcon :icon="SaveIcon" class="size-4" />
            {{ t('pages.nvr.saveAction') }}
        </Button>
    </form>
</template>
