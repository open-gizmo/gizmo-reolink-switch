<script setup>
import { computed, ref, watch } from 'vue';
import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import CctvIcon from '@hugeicons/core-free-icons/CctvIcon';
import Cancel01Icon from '@hugeicons/core-free-icons/Cancel01Icon';
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
import { ResponsiveDialog } from '@brumaombra/ui-vintage/responsive-dialog';
import { buildNvrBody, findInvalidNvrField, hasNvrLoginChanged, NVR_HOST_MAX_LENGTH, NVR_PASSWORD_MAX_LENGTH, NVR_USERNAME_MAX_LENGTH } from '~/composables/useNvrForm.js';
import { isMuted } from '~/composables/useQuietMode.js';
import { saveNvrLogin } from '~/composables/useSwitchState.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';

const { t } = useI18n();
const globalStore = useGlobalStore();
const isSaving = ref(false);
const isPasswordVisible = ref(false);
const isChangeDialogOpen = ref(false);
const isPasswordCleared = ref(false);

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

// Whether the address or the username differ from the saved ones
const isLoginChanged = computed(() => hasNvrLoginChanged({ host: host.value, username: username.value, savedHost: nvr.value.host, savedUsername: nvr.value.username }));

// Whether the saved password can still be used: the device forgets it when the address or the username change
const hasSavedPassword = computed(() => nvr.value.hasPassword && !isPasswordCleared.value);

// Texts of the password field: a saved one, one deleted by the change, or none yet
const passwordPlaceholder = computed(() => {
    if (hasSavedPassword.value) return t('pages.nvr.password.savedPlaceholder');
    return t(isPasswordCleared.value ? 'pages.nvr.password.clearedPlaceholder' : 'pages.nvr.password.placeholder');
});
const passwordNote = computed(() => {
    if (hasSavedPassword.value) return t('pages.nvr.password.savedNote');
    return t(isPasswordCleared.value ? 'pages.nvr.password.clearedNote' : 'pages.nvr.password.note');
});

// Whether the form can be sent, and the first field that is wrong
const invalidField = computed(() => findInvalidNvrField({ host: host.value, username: username.value, password: password.value, hasSavedPassword: hasSavedPassword.value }));
const isBusy = computed(() => isSaving.value || globalStore.value.status.busy);

// Back to the saved login, the password is usable again
watch(isLoginChanged, isChanged => {
    if (!isChanged) isPasswordCleared.value = false;
});

// Send the login to the device, which then tests it by itself
const saveLogin = async () => {
    try {
        isSaving.value = true;
        const isSaved = await saveNvrLogin(buildNvrBody({ host: host.value, username: username.value, password: password.value }));

        // Show the login as the device kept it (without the scheme and the slashes), and forget the typed password
        if (isSaved) {
            host.value = nvr.value.host;
            username.value = nvr.value.username;
            password.value = '';
            isPasswordVisible.value = false;
            isPasswordCleared.value = false;
            showMessageToast({ type: 'success', icon: CheckmarkCircle02Icon, message: t('pages.nvr.saveSuccess') });
        }
    } finally {
        isSaving.value = false;
    }
};

// Save the login, after asking if it would delete the saved password (a changed address or username without a new password)
const handleSave = async () => {
    if (invalidField.value || isLocked.value || isBusy.value) return;
    if (nvr.value.hasPassword && isLoginChanged.value && !password.value) {
        isChangeDialogOpen.value = true;
        return;
    }
    await saveLogin();
};

// Clear the saved password: the form now needs the password of the new NVR
const handleChangeConfirm = () => {
    isChangeDialogOpen.value = false;
    isPasswordCleared.value = true;
};

// Keep the saved NVR: the address and the username go back to the saved ones
const handleChangeCancel = () => {
    isChangeDialogOpen.value = false;
    host.value = nvr.value.host;
    username.value = nvr.value.username;
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
                <Input id="nvr-password" v-model="password" :type="isPasswordVisible ? 'text' : 'password'" autocomplete="new-password" autocapitalize="none" spellcheck="false" :maxlength="NVR_PASSWORD_MAX_LENGTH" :disabled="isLocked" :placeholder="passwordPlaceholder" />
                <p class="mt-2 text-[0.6875rem] text-muted-foreground md:text-xs" data-testid="password-note">{{ passwordNote }}</p>
            </ListRow>
        </ListGroup>

        <!-- Save button -->
        <Button variant="primary" type="submit" class="w-full" :loading="isSaving" :disabled="Boolean(invalidField) || isLocked || isBusy" data-testid="save-button">
            <HugeiconsIcon :icon="SaveIcon" class="size-4" />
            {{ t('pages.nvr.saveAction') }}
        </Button>

        <!-- Confirmation to change the NVR (a sheet on phones, a dialog on larger screens) -->
        <ResponsiveDialog v-model:open="isChangeDialogOpen" :title="t('pages.nvr.changeDialog.title')" :description="t('pages.nvr.changeDialog.description')">
            <!-- Actions (the main one first: on top in the sheet, rightmost in the dialog) -->
            <template #footer>
                <Button type="button" variant="primary" data-testid="change-confirm-button" @click="handleChangeConfirm">
                    <HugeiconsIcon :icon="LockPasswordIcon" class="size-4" />
                    {{ t('pages.nvr.changeDialog.confirmAction') }}
                </Button>
                <Button type="button" variant="secondary" data-testid="change-keep-button" @click="handleChangeCancel">
                    <HugeiconsIcon :icon="Cancel01Icon" class="size-4" />
                    {{ t('pages.nvr.changeDialog.keepAction') }}
                </Button>
            </template>
        </ResponsiveDialog>
    </form>
</template>
