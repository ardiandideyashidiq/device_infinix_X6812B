//
// Copyright (C) 2025 LineageOS
//
// SPDX-License-Identifier: Apache-2.0
//

#include <stdint.h>

extern "C" uint64_t _ZN11unwindstack3Elf8GetRelPcEmPNS_7MapInfoE(uint64_t pc, void* map_info);

extern "C" uint64_t _ZN11unwindstack3Elf8GetRelPcEmPKNS_7MapInfoE(uint64_t pc,
                                                                  const void* map_info) {
    return _ZN11unwindstack3Elf8GetRelPcEmPNS_7MapInfoE(pc, const_cast<void*>(map_info));
}
