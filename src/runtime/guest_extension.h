/* Guest memory extension for tables relocated by the project's
 * config/guest-code-patches tables (US Xbox 45410066). The lift checks every
 * relocation target against config/guest-code-patches/00-header.json
 * "extension"; keep these values identical. The image ends at 0x0089CD60, so
 * the block starts at 0x008A0000. Shared by the Windows and native Mac boots. */
#ifndef TSFP_GUEST_EXTENSION_H
#define TSFP_GUEST_EXTENSION_H
#include "guest_image.h"

#define TSFP_GUEST_EXTENSION_VA   0x008A0000u
#define TSFP_GUEST_EXTENSION_SIZE 0x00080000u

/* Before xbox_MemoryLayoutInit. */
static inline void tsfp_guest_extension_reserve(void)
{
    tsfp_image_set_extension(TSFP_GUEST_EXTENSION_SIZE);
}

/* After xbox_MemoryLayoutInit: 1 when the block is where the code expects. */
static inline int tsfp_guest_extension_ready(uint32_t *va, uint32_t *size)
{
    uint32_t bytes = 0, at = tsfp_image_extension(&bytes);
    if (va) *va = at;
    if (size) *size = bytes;
    return at == TSFP_GUEST_EXTENSION_VA && bytes >= TSFP_GUEST_EXTENSION_SIZE;
}
#endif
