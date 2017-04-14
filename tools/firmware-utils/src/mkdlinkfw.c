/*
 *  Copyright (C) 2017 liushuyu <liushuyu011@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 *
 */

#include <getopt.h>

#include "mkdlinkfw.h"

int main(int argc, char *argv[])
{
    if (argc < 3) {help_msg(argv[0]); return 1;}
    // cmdline parsing
    int opt; char *reg = "US"; char *rev = "R1"; char *swver = "1.0.0.0";
    char *bid = "0123456789"; char *mdl; char *kimg; char *rfs;
    char pdt[PRODUCT_NAME_LEN]; char *outfile;
    while ((opt = getopt(argc, argv, ":b:e:i:m:p:o:k:r:v:c:h")) != -1) {
      switch (opt) {
        case 'i':
            return inspect_fw(optarg);
        case 'b':
          if (strlen(optarg) > BOARD_ID_NAME_LEN) {
            printf("Board ID too long! Max: %d characters!\n", BOARD_ID_NAME_LEN);
            return 1;
          }
          bid = optarg;
          break;
        case 'e':
          if (strlen(optarg) > REVISION_NAME_LEN) {
            printf("Revision name too long! Max: %d characters!\n", REVISION_NAME_LEN);
            return 1;
          }
          rev = optarg;
          break;
        case 'v':
          if (strlen(optarg) > SW_VERSION_LEN) {
            printf("Version name too long! Max: %d characters!\n", SW_VERSION_LEN);
            return 1;
          }
          swver = optarg;
          break;
        case 'k':
          kimg = optarg;
          break;
        case 'r':
          rfs = optarg;
          break;
        case 'o':
          outfile = optarg;
          break;
        case 'c':
          if (strlen(optarg) > REGION_LEN) {
            printf("Region name too long! Max: %d characters!\n", REGION_LEN);
            return 1;
          }
          reg = optarg;
          break;
        case 'm':
          if (strlen(optarg) > MODEL_LEN) {
            printf("Model name too long! Max: %d characters!\n", MODEL_LEN);
            return 1;
          }
          mdl = optarg;
          break;
        case 'p':
          if (strlen(optarg) > PRODUCT_NAME_LEN) {
            printf("Product name too long! Max: %d characters!\n", PRODUCT_NAME_LEN);
            return 1;
          }
          strncat(&pdt, optarg, sizeof(unsigned char) * PRODUCT_NAME_LEN);
          break;
        case '?':
          printf("Unknown option: -%c\n", optopt);
          return 1;
        case ':':
          printf("Option -%c needs option\n", optopt);
          return 1;
      }
    }
    if (!mdl) {
      puts("Please specify model name!!");
      return 1;
    }
    //pdt = (pdt ? pdt : mdl);
    strncat(&pdt, mdl, sizeof(unsigned char) * PRODUCT_NAME_LEN);
    fw_header *header = malloc(sizeof(fw_header));
    memset(header, 0, sizeof(fw_header));
    strncat((char*)header->region, reg, sizeof(unsigned char) * REGION_LEN);
    strncat((char*)header->swversion, swver, sizeof(unsigned char) * SW_VERSION_LEN);
    strncat((char*)header->product, pdt, sizeof(unsigned char) * PRODUCT_NAME_LEN);
    strncat((char*)header->model_name, mdl, sizeof(unsigned char) * MODEL_LEN);
    strncat((char*)header->revision, rev, sizeof(unsigned char) * REVISION_NAME_LEN);
    strncat((char*)header->board_id, bid, sizeof(unsigned char) * BOARD_ID_NAME_LEN);
    // currently we can only produce single kernel images
    // (there are images that contains more than one kernel for backup and recovery)
    strncat((char*)header->img_type, "imgs", sizeof(unsigned char) * IMAGE_TYPES_NAME_LEN);
    return build_image(header, kimg, rfs);
}

// D-Link used a strange CRC32 algorithm...
unsigned long dlink_crc32(unsigned char *cp, unsigned int size)
{
    unsigned long crc = 0;
    unsigned int length = 0;

    length = size;
    while(size--)
        crc = (crc << 8) ^ crc_table[((crc >> 24) ^ (*cp++)) & 0xFF];
    do {
        crc = (crc << 8) ^ crc_table[((crc >> 24) ^ length) & 0xFF];
    } while(length >>= 8);

    crc = ~crc & 0xFFFFFFFF;

    return crc;
}

int inspect_fw(char* filename) {
  unsigned char *inspect_buf = NULL; size_t fsize = 0;
  if (mmap_file(filename, PROT_READ, &inspect_buf, &fsize) != 0) return 1;
  fw_header *inspect_fw_buf = malloc(sizeof(fw_header));
  memset(inspect_fw_buf, 0, sizeof(fw_header));
  memcpy(inspect_fw_buf, inspect_buf, sizeof(fw_header));
  unsigned int wf_chksum = 0;
  read_wholefile_crc(&inspect_buf, fsize, &wf_chksum);
  printf("--- Image information:\n"
  "Kernel offset:    0x%02x\n"
  "Kernel size:      0x%04x\n"
  "RootFS offset:    0x%04x\n"
  "RootFS size:      0x%04x\n"
  "Raw image size:   0x%04x\n"
  "Raw image chksum: %04x\n"
  "Image type:       %s\n"
  "Board ID:         %s\n"
  "Product Name:     %s\n"
  "Software rev.:    %s\n"
  "Product Model:    %s\n"
  "Product region:   %s\n"
  "Software ver.:    %s\n"
  "Whole file chksum:%04X\n",
  inspect_fw_buf->kernel_offset, inspect_fw_buf->kernel_size, inspect_fw_buf->rootfs_offset,
  inspect_fw_buf->rootfs_size, inspect_fw_buf->image_len, inspect_fw_buf->image_checksum,
  (strcmp((char*)inspect_fw_buf->img_type, "imgs") == 0) ? "Single" : "Dual", inspect_fw_buf->board_id,
  inspect_fw_buf->product, inspect_fw_buf->revision, inspect_fw_buf->model_name,
  inspect_fw_buf->region, inspect_fw_buf->swversion, wf_chksum);
  free(inspect_fw_buf);
  munmap(inspect_buf, fsize);
  return 0;
}

void read_wholefile_crc(unsigned char **mem, ssize_t file_len, unsigned int *chksum) {
  memmove(chksum, *mem + file_len - 4, 4);
  host_endian(*chksum);
}

void correct_endianness(fw_header *header) {
  host_endian(header->kernel_offset);
  host_endian(header->kernel_size);
  host_endian(header->image_len);
  host_endian(header->rootfs_offset);
  host_endian(header->rootfs_size);
  host_endian(header->image_checksum);
  return;
}

int mmap_file(char *filename, int rwflags, unsigned char **mem, size_t *len) {
  struct stat sb;
  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    perror(filename);
    return 1;
  }
  if (fstat(fd, &sb) == -1) {
    perror("fstat() failed");
    return 1;
  }
  *mem = (unsigned char*)mmap(0, sb.st_size, rwflags, MAP_SHARED, fd, 0);
  if (mem == MAP_FAILED) {
    perror("mmap() failed");
    return 1;
  }
  *len = sb.st_size;
  return 0;
}

int build_image(fw_header *header, char *kimg, char *rfs) {
  unsigned char *kernel_file; unsigned char *rfs_file;
  size_t kernel_len; size_t rfs_len;
  if (!kimg) {
    puts("Please specify kernel image file!");
    return 1;
  }
  if (!rfs) {
    puts("Please specify rootfs image file!");
    return 1;
  }
  if (mmap_file(kimg, PROT_READ, &kernel_file, &kernel_len) != 0) return 1;
  if (mmap_file(rfs, PROT_READ, &rfs_file, &rfs_len) != 0) return 1;
  munmap(rfs_file, rfs_len);
  munmap(kernel_file, kernel_len);
  return 0;
}

void help_msg(char *progname) {
    printf("Usage: %s [OPTIONS...]\n", progname);
    printf("\n"
"Options:\n"
"  -b <board>      set image board id to <board>\n"
"  -e <revision>   set image revision to <revision>\n"
"  -i <file>       inspect firmware file <file>\n"
"  -m <model>      set image model name to <model>\n"
"  -p <product>    set image product name to <product>\n"
"  -o <file>       write output to the file <file>\n"
"  -k <krnl_img>   use kernel image <krnl_img>\n"
"  -r <rootfs_img> use rootfs image <rootfs_img>\n"
"  -v <version>    set image version to <version>\n"
"  -c <region>     set image region to <region>\n"
"  -h              show this help message\n"
	);
}
