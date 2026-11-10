//
// Copyright (C) 2025 LineageOS
//
// SPDX-License-Identifier: Apache-2.0
//
// Masked ABI shim for unwindstack::Elf::GetRelPc.
//
// The MTK AEE blobs (libudf.so) were built when the signature took a
// non-const MapInfo*, so they import
//   _ZN11unwindstack3Elf8GetRelPcEmPKNS_7MapInfoE   (const MapInfo*)
// while current libunwindstack only defines
//   _ZN11unwindstack3Elf8GetRelPcEmPNS_7MapInfoE    (MapInfo*)
// Constness does not change the calling convention, so forward to the real
// implementation instead of stubbing it out.
//

#include <stdint.h>

extern "C" uint64_t _ZN11unwindstack3Elf8GetRelPcEmPNS_7MapInfoE(uint64_t pc, void* map_info);

extern "C" uint64_t _ZN11unwindstack3Elf8GetRelPcEmPKNS_7MapInfoE(uint64_t pc,
                                                                  const void* map_info) {
    return _ZN11unwindstack3Elf8GetRelPcEmPNS_7MapInfoE(pc, const_cast<void*>(map_info));
}
