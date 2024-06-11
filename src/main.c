#include "vulkan.h"
#include <stdio.h>

struct global_s {
  vulkan_t vk;
  int test;
} g;

int main(void) {

  vk_init(&g.vk);

  printf("Hello World\n");
  return 0;
}
