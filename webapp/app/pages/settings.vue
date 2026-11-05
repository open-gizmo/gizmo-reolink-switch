<script setup>
import { computed, ref } from 'vue';
import GlobeIcon from '@hugeicons/core-free-icons/GlobeIcon';
import { ListGroup, ListRow } from '@brumaombra/ui-vintage/list';

const { locale, locales, setLocale, t } = useI18n();
const isLanguagePickerOpen = ref(false);

// Languages of the app, to pick from
const languageOptions = computed(() => locales.value.map(language => ({ value: language.code, label: language.name })));

// Name of the language in use, shown in its row
const selectedLanguageName = computed(() => languageOptions.value.find(option => option.value === locale.value)?.label || '');

// Apply a selected language
const handleSelectLanguage = async language => {
    await setLocale(language);
};

// Page metadata
definePageMeta({
    layout: 'dashboard'
});
</script>

<template>
    <GizmoPage :title="t('pages.settings.title')">
        <!-- Language -->
        <ListGroup :title="t('pages.settings.languageTitle')">
            <ListRow :label="t('pages.settings.language.label')" :icon="GlobeIcon" :value="selectedLanguageName" action @click="isLanguagePickerOpen = true" />
        </ListGroup>

        <!-- Device info, restart and forget network (from the gizmo-common layer, it reads the device by itself) -->
        <GizmoDeviceCard />

        <!-- Language picker -->
        <OptionPickerDialog :visible="isLanguagePickerOpen" :title="t('pages.settings.language.label')" :description="t('pages.settings.language.note')" :options="languageOptions" :model-value="locale" @update:model-value="handleSelectLanguage" @close="isLanguagePickerOpen = false" />
    </GizmoPage>
</template>
