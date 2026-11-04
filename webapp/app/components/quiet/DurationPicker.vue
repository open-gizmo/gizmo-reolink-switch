<script setup>
import { computed } from 'vue';
import { buildDurationOptions } from '~/composables/useQuietMode.js';

// Props: the selected minutes, the maximum of the device and whether the picker can't be used now
const props = defineProps({
    modelValue: { type: Number, required: true },
    maxMinutes: { type: Number, default: 60 },
    disabled: { type: Boolean, default: false }
});

// Emits
const emits = defineEmits(['update:modelValue']);

const { t } = useI18n();

// Durations to pick from
const options = computed(() => buildDurationOptions(props.maxMinutes));

// Pick a duration
const handleSelect = minutes => {
    if (!props.disabled) emits('update:modelValue', minutes);
};
</script>

<template>
    <div class="flex flex-wrap gap-2" role="radiogroup" :aria-label="t('pages.home.duration.label')">
        <button v-for="minutes in options"
            :key="minutes"
            type="button"
            role="radio"
            :aria-checked="minutes === props.modelValue"
            :disabled="props.disabled"
            class="min-h-11 min-w-16 cursor-pointer rounded border px-3 py-2 text-sm font-semibold tabular-nums transition-colors disabled:cursor-not-allowed disabled:opacity-50"
            :class="minutes === props.modelValue ? 'border-primary bg-primary text-primary-foreground' : 'border-border bg-card text-foreground hover:bg-accent'"
            :data-testid="`duration-${minutes}`"
            @click="handleSelect(minutes)">
            {{ t('pages.home.duration.chip', { minutes }) }}
        </button>
    </div>
</template>
