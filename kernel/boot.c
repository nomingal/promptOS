#include "boot.h"

/* Requests are found by the bootloader scanning the loaded kernel image,
   so they only need to be 8-byte aligned and kept by the compiler. */

__attribute__((used, aligned(8)))
static volatile uint64_t limine_base_revision[3] = LIMINE_BASE_REVISION_INIT(3);

__attribute__((used, aligned(8)))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = 0,
};

const struct limine_framebuffer *boot_framebuffer(void) {
    struct limine_framebuffer_response *resp = framebuffer_request.response;
    if (resp == 0 || resp->framebuffer_count == 0 || resp->framebuffers == 0) {
        return 0;
    }
    return resp->framebuffers[0];
}
