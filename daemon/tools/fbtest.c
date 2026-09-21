/* fbtest: poke the framebuffer one operation at a time, to find out what is safe on a given phone.
 * Output is unbuffered so the last line survives if the phone hangs.
 *
 * usage: fbtest info
 *        fbtest strip <rows> <seconds>   fill the top <rows> rows white (mmap write only)
 *        fbtest wstrip <rows> <seconds>  same, but using pwrite() instead of mmap
 *        fbtest bars <seconds>           red/green/blue bars with a white frame (mmap write only)
 *        fbtest pan | blank              issue FBIOPAN_DISPLAY / FBIOBLANK(unblank) only
 * Run as root, with SurfaceFlinger and the hwcomposer stopped.
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static struct fb_var_screeninfo v;
static struct fb_fix_screeninfo f;

/* Try one screen first (the driver rejected the full double-buffered smem_len with EINVAL),
 * then the full length. */
static uint8_t *map(int fd, size_t *len) {
    size_t one = (size_t)f.line_length * v.yres;
    size_t tries[2] = { one, f.smem_len };
    for (int i = 0; i < 2; i++) {
        if (!tries[i]) continue;
        printf("mmap %zu bytes... ", tries[i]);
        uint8_t *fb = mmap(NULL, tries[i], PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (fb != MAP_FAILED) { puts("ok"); *len = tries[i]; return fb; }
        printf("failed: %s\n", strerror(errno));
    }
    exit(1);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    const char *mode = argc > 1 ? argv[1] : "info";
    int fd = open("/dev/graphics/fb0", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }
    if (ioctl(fd, FBIOGET_VSCREENINFO, &v) || ioctl(fd, FBIOGET_FSCREENINFO, &f)) { perror("ioctl get"); return 1; }
    printf("res %ux%u virt %ux%u bpp %u stride %u smem_len %u\n", v.xres, v.yres, v.xres_virtual, v.yres_virtual, v.bits_per_pixel, f.line_length, f.smem_len);
    printf("offsets r%u g%u b%u a%u yoffset %u rotate %u\n", v.red.offset, v.green.offset, v.blue.offset, v.transp.offset, v.yoffset, v.rotate);

    if (!strcmp(mode, "info")) return 0;

    if (!strcmp(mode, "strip") || !strcmp(mode, "bars")) {
        int strip = !strcmp(mode, "strip");
        unsigned rows = strip ? (argc > 2 ? atoi(argv[2]) : 64) : v.yres;
        int secs = strip ? (argc > 3 ? atoi(argv[3]) : 10) : (argc > 2 ? atoi(argv[2]) : 10);
        size_t len; uint8_t *fb = map(fd, &len);
        for (unsigned y = 0; y < rows && y < v.yres; y++) {
            for (unsigned x = 0; x < v.xres; x++) {
                uint32_t r = 255, g = 255, b = 255;
                if (!strip) {
                    r = g = b = 0;
                    if (y < v.yres / 3) r = 255; else if (y < 2 * v.yres / 3) g = 255; else b = 255;
                    if (x < 24 || y < 24 || x >= v.xres - 24 || y >= v.yres - 24) r = g = b = 255;
                }
                *(uint32_t *)(fb + (size_t)y * f.line_length + (size_t)x * 4) =
                    (r << v.red.offset) | (g << v.green.offset) | (b << v.blue.offset);
            }
        }
        printf("drawn %u rows; holding %d s\n", rows, secs);
        sleep(secs);
        memset(fb, 0, len);
        puts("cleared");
        munmap(fb, len);
    } else if (!strcmp(mode, "wstrip")) {   /* same as strip, but with pwrite() instead of mmap */
        unsigned rows = argc > 2 ? atoi(argv[2]) : 64; int secs = argc > 3 ? atoi(argv[3]) : 10;
        size_t rowbytes = f.line_length; uint8_t *row = malloc(rowbytes); memset(row, 0xff, rowbytes);
        for (unsigned y = 0; y < rows; y++)
            if (pwrite(fd, row, rowbytes, (off_t)y * rowbytes) != (ssize_t)rowbytes) { perror("pwrite"); return 1; }
        printf("pwrite %u rows ok; holding %d s\n", rows, secs); sleep(secs);
        memset(row, 0, rowbytes);
        for (unsigned y = 0; y < rows; y++) pwrite(fd, row, rowbytes, (off_t)y * rowbytes);
        puts("cleared");
    } else if (!strcmp(mode, "pan")) {
        puts("FBIOPAN_DISPLAY..."); int r = ioctl(fd, FBIOPAN_DISPLAY, &v); printf("pan -> %d\n", r);
    } else if (!strcmp(mode, "blank")) {
        puts("FBIOBLANK unblank..."); int r = ioctl(fd, FBIOBLANK, FB_BLANK_UNBLANK); printf("blank -> %d\n", r);
    } else { puts("unknown mode"); return 2; }
    close(fd);
    puts("done");
    return 0;
}
