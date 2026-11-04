<script setup>
import { computed, ref } from 'vue';
import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import CheckmarkCircle02Icon from '@hugeicons/core-free-icons/CheckmarkCircle02Icon';
import SaveIcon from '@hugeicons/core-free-icons/SaveIcon';
import ViewIcon from '@hugeicons/core-free-icons/ViewIcon';
import ViewOffIcon from '@hugeicons/core-free-icons/ViewOffIcon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { Alert, AlertDescription, AlertTitle } from '@brumaombra/ui-vintage/alert';
import { Button } from '@brumaombra/ui-vintage/button';
import { Field, FieldContent, FieldDescription, FieldLabel } from '@brumaombra/ui-vintage/field';
import { Input } from '@brumaombra/ui-vintage/input';
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
    <form class="space-y-4" novalidate @submit.prevent="handleSave">
        <!-- The login can't change now -->
        <Alert v-if="isLocked" color="yellow" data-testid="nvr-locked">
            <HugeiconsIcon :icon="Alert02Icon" />
            <AlertTitle>{{ t('pages.nvr.locked.title') }}</AlertTitle>
            <AlertDescription>{{ t('pages.nvr.locked.description') }}</AlertDescription>
        </Alert>

        <!-- Address -->
        <Field>
            <FieldLabel for="nvr-host">{{ t('pages.nvr.host.label') }}</FieldLabel>
            <FieldContent>
                <Input id="nvr-host" v-model="host" type="text" inputmode="url" autocapitalize="none" autocomplete="off" spellcheck="false" :maxlength="NVR_HOST_MAX_LENGTH" :disabled="isLocked" :placeholder="t('pages.nvr.host.placeholder')" />
                <FieldDescription>{{ t('pages.nvr.host.note') }}</FieldDescription>
            </FieldContent>
        </Field>

        <!-- Username -->
        <Field>
            <FieldLabel for="nvr-username">{{ t('pages.nvr.username.label') }}</FieldLabel>
            <FieldContent>
                <Input id="nvr-username" v-model="username" type="text" autocapitalize="none" autocomplete="off" spellcheck="false" :maxlength="NVR_USERNAME_MAX_LENGTH" :disabled="isLocked" :placeholder="t('pages.nvr.username.placeholder')" />
            </FieldContent>
        </Field>

        <!-- Password (never sent back by the device: empty, with a hint when one is saved) -->
        <Field>
            <FieldLabel for="nvr-password">{{ t('pages.nvr.password.label') }}</FieldLabel>
            <FieldContent>
                <div class="relative">
                    <Input id="nvr-password" v-model="password" class="pr-12" :type="isPasswordVisible ? 'text' : 'password'" autocomplete="new-password" autocapitalize="none" spellcheck="false" :maxlength="NVR_PASSWORD_MAX_LENGTH" :disabled="isLocked" :placeholder="nvr.hasPassword ? t('pages.nvr.password.savedPlaceholder') : t('pages.nvr.password.placeholder')" />
                    <button type="button" class="absolute inset-y-0 right-0 flex w-11 cursor-pointer items-center justify-center text-muted-foreground hover:text-foreground disabled:cursor-not-allowed disabled:opacity-50" :aria-label="isPasswordVisible ? t('pages.nvr.password.hide') : t('pages.nvr.password.show')" :aria-pressed="isPasswordVisible" :disabled="isLocked" data-testid="password-toggle" @click="isPasswordVisible = !isPasswordVisible">
                        <HugeiconsIcon :icon="isPasswordVisible ? ViewOffIcon : ViewIcon" class="size-4" />
                    </button>
                </div>
                <FieldDescription data-testid="password-note">{{ nvr.hasPassword ? t('pages.nvr.password.savedNote') : t('pages.nvr.password.note') }}</FieldDescription>
            </FieldContent>
        </Field>

        <!-- Save button -->
        <Button variant="primary" type="submit" class="w-full" :loading="isSaving" :disabled="Boolean(invalidField) || isLocked || isBusy" data-testid="save-button">
            <HugeiconsIcon :icon="SaveIcon" class="size-4" />
            {{ t('pages.nvr.saveAction') }}
        </Button>
    </form>
</template>
