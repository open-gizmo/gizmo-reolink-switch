#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>

#include "channels/channels.h"

void setupStorage();
MutedChannelMasks getMutedChannelMasks();
bool saveMutedChannelMasks(const MutedChannelMasks &masks);

#endif