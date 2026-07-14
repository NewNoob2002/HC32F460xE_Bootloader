#ifndef BOOT_UPDATE_SERVICE_H
#define BOOT_UPDATE_SERVICE_H

#include "boot_protocol_types.h"

#include <stdint.h>

typedef struct {
    uint32_t validated_frames;
    uint32_t responses_published;
    uint32_t response_busy_drop;
    uint32_t unsupported_commands;
} boot_update_service_stats_t;

/** @brief Initializes protocol command-service state. */
void BootUpdateServiceInit(void);

/**
 * @brief Handles one validated frame and publishes any protocol response through the TX buffer API.
 * @param frame CRC-validated protocol frame.
 */
void BootUpdateServiceHandleFrame(const boot_protocol_frame_t* frame);

/**
 * @brief Parser callback matching the legacy CustomDataProcess handoff pattern.
 * @param frame CRC-validated protocol frame.
 * @param context Unused callback context.
 */
void BootUpdateServiceFrameCallback(const boot_protocol_frame_t* frame, void* context);

/** @brief Copies service statistics into caller-owned storage. */
void BootUpdateServiceGetStats(boot_update_service_stats_t* stats);

#endif
