#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>

void setupStorage();
uint32_t getMutedChannelMask();
bool saveMutedChannelMask(uint32_t channelMask);

#endif