#ifndef C_VULKAN_H
#define C_VULKAN_H

// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
//                              C_VULKAN_HEADER
// -------------------------------------------------------------------------
// -------------------------------------------------------------------------

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

#define NO_INDEX 999

typedef struct vulkan_s {
  VkInstance inst;
  VkPhysicalDevice pdevice;
  VkDevice ldevice;
  struct extensions_s {
    uint32_t count;
    const char **names;
  } extensions;
  struct layers_s {
    uint32_t count;
    const char **names;
  } layers;
  struct queue_s {
    VkQueue graphics_queue;
    struct family_indicies_s {
      uint32_t graphics_family;
    } family_indices;
  } queues;
} vulkan_t;

void vk_quit(vulkan_t *vk);
int vk_init(vulkan_t *vk);

// ------
// NDEBUG
// ------
#ifdef NDEBUG
const int validation_layers_active = 1;
#define C_VULKAN_DEB(...)
#define C_VULKAN_LOG(...)
#define C_VULKAN_WARN(...)
#define C_VULKAN_ERR(...)
#else
const int validation_layers_active = 0;
#endif // NDEBUG

// -----
// MACOS
// -----
#ifdef MACOS
const int macos_extensions_active = 1;
#else
const int macos_extensions_active = 0;
#endif // DEBUG

// ---------------
// C_VULKAN_ASSERT
// ---------------
#if !defined(C_VULKAN_ASSERT)
#define C_VULKAN_ASSERT(_e, ...)                                               \
  if (!(_e)) {                                                                 \
    fprintf(stderr, __VA_ARGS__);                                              \
    exit(1);                                                                   \
  }
#endif // C_VULKAN_ASSERT

// ---------------
// C_VULKAN_LOGGER
// ---------------
#if !defined(N_C_VULKAN_LOGGER) && !defined(NDEBUG)

#define C_VULKAN_LOG_MAX_BUF 1024
typedef enum { DEBUG, INFO, WARNING, ERROR } LOG_LEVEL;

void logger_init(LOG_LEVEL base_level, char *base_fpath);
void logger_log(LOG_LEVEL log_level, const char *log_fname, const size_t line,
                const char *fmt, ...);
void logger_deinit(void);

#ifndef C_VULKAN_DEB
#define C_VULKAN_DEB(...) logger_log(DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#endif // C_VULKAN_DEB

#ifndef C_VULKAN_LOG
#define C_VULKAN_LOG(...) logger_log(INFO, __FILE__, __LINE__, __VA_ARGS__)
#endif // C_VULKAN_LOG

#ifndef C_VULKAN_WARN
#define C_VULKAN_WARN(...) logger_log(WARNING, __FILE__, __LINE__, __VA_ARGS__)
#endif // C_VULKAN_WARN

#ifndef C_VULKAN_ERR
#define C_VULKAN_ERR(...) logger_log(ERROR, __FILE__, __LINE__, __VA_ARGS__)
#endif // C_VULKAN_ERR
#endif // C_VULKAN_LOGGER

#endif // C_VULKAN_H

// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
//                          C_VULKAN_IMPLEMENTATION
// -------------------------------------------------------------------------
// -------------------------------------------------------------------------

#ifndef C_VULKAN_IMPLEMENTATION
#include <string.h>

static int vk_create_logical_device(vulkan_t *vk) {
  float queuePriority = 1.0f;
  VkDeviceQueueCreateInfo ldev_queue_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .pNext = NULL,
      .queueFamilyIndex = vk->queues.family_indices.graphics_family,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority,
  };

  VkPhysicalDeviceFeatures pdev_feats = {};

  // TODO fix on other platforms
#ifdef __MACH__
  const char *names[1];
  names[0] = "VK_KHR_portability_subset";
#else
  const char **names = NULL;
#endif

  VkDeviceCreateInfo ldev_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .pQueueCreateInfos = &ldev_queue_info,
      .queueCreateInfoCount = 1,
      .pEnabledFeatures = &pdev_feats,
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = NULL,
      .enabledExtensionCount = 1,
      .ppEnabledExtensionNames = names,
  };

  if (validation_layers_active) {
    ldev_create_info.enabledLayerCount = vk->layers.count;
    ldev_create_info.ppEnabledLayerNames = vk->layers.names;
  }

  if (vkCreateDevice(vk->pdevice, &ldev_create_info, NULL, &vk->ldevice) !=
      VK_SUCCESS) {
    return -1;
  }
  C_VULKAN_LOG("VK LOGIKAL DEVICE CREATED\n");

  vkGetDeviceQueue(vk->ldevice, vk->queues.family_indices.graphics_family, 0,
                   &vk->queues.graphics_queue);
  C_VULKAN_LOG("VK LOGICAL DEVICE QUEUE CREATED\n");

  return 0;
}

static bool vk_queue_indices_is_complete(vulkan_t *vk) {
  if (vk->queues.family_indices.graphics_family != NO_INDEX)
    return true;
  else
    return false;
}

static void vk_find_queue_families(vulkan_t *vk, VkPhysicalDevice pdev) {
  uint32_t queue_fam_count;
  vkGetPhysicalDeviceQueueFamilyProperties(pdev, &queue_fam_count, NULL);
  VkQueueFamilyProperties queue_fam_props[queue_fam_count];
  vkGetPhysicalDeviceQueueFamilyProperties(pdev, &queue_fam_count,
                                           queue_fam_props);

  C_VULKAN_LOG("%d queue families:\n", queue_fam_count);
  for (uint32_t i = 0; i < queue_fam_count; i++) {
    if (queue_fam_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      vk->queues.family_indices.graphics_family = i;
      C_VULKAN_LOG("\t%d : loaded\n", queue_fam_props[i].queueCount);
      if (vk_queue_indices_is_complete(vk)) {
        C_VULKAN_LOG("all needed queue family indices found\n");
        break;
      }
    } else {
      C_VULKAN_LOG("\t%d : not loaded\n", queue_fam_props[i].queueCount);
    }
  }
}

static bool vk_is_device_suitable(vulkan_t *vk, VkPhysicalDevice pdev) {
  // TODO rating system
  // TODO integrated gpu only on mac -> should be discrete
  VkPhysicalDeviceProperties pdev_props;
  VkPhysicalDeviceFeatures pdev_feats;

  vkGetPhysicalDeviceProperties(pdev, &pdev_props);
  vkGetPhysicalDeviceFeatures(pdev, &pdev_feats);

  vk_find_queue_families(vk, pdev);

  if (pdev_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU &&
      vk_queue_indices_is_complete(vk)) {
    C_VULKAN_LOG("Device (%s) is suitable and will be used\n",
                 pdev_props.deviceName);
    return true;
  } else {
    return false;
  }
}

static int vk_pick_pDevice(vulkan_t *vk) {
  uint32_t physical_devices_count;
  vkEnumeratePhysicalDevices(vk->inst, &physical_devices_count, NULL);
  if (physical_devices_count == 0) {
    C_VULKAN_ERR("Couldn't find any GPU\n");
    return -1;
  }

  VkPhysicalDevice physical_devices[physical_devices_count];
  vkEnumeratePhysicalDevices(vk->inst, &physical_devices_count,
                             physical_devices);

  // TODO rating system
  for (size_t i = 0; i < physical_devices_count; i++) {
    if (vk_is_device_suitable(vk, physical_devices[i])) {
      vk->pdevice = physical_devices[i];
      break;
    }
  }

  if (vk->pdevice == VK_NULL_HANDLE) {
    C_VULKAN_ERR("Couldn't find suitable GPU\n");
    return -1;
  }
  return 0;
}

static int vk_check_validation_layers(uint32_t layer_count,
                                      const char **layer_names) {
  uint32_t inst_layer_count;
  vkEnumerateInstanceLayerProperties(&inst_layer_count, NULL);
  VkLayerProperties inst_layer_props[inst_layer_count];
  vkEnumerateInstanceLayerProperties(&inst_layer_count, inst_layer_props);

  C_VULKAN_LOG("checking %d validation layer(s):\n", layer_count);
  for (size_t i = 0; i < layer_count; i++) {
    bool found = false;
    for (size_t a = 0; a < inst_layer_count; a++) {
      if (strcmp(layer_names[i], inst_layer_props[i].layerName)) {
        found = true;
        break;
      } else {
        continue;
      }
    }

    if (found) {
      C_VULKAN_LOG("\t- %s : found\n", layer_names[i]);
    } else {
      C_VULKAN_ERR("\t- %s : not found; exiting\n", layer_names[i]);
      return -1;
    }
  }
  C_VULKAN_LOG("validation Layer(s) found\n");
  return 0;
}

static int vk_create_validation_layers(vulkan_t *vk) {
  // TODO proper DEBUGGER
  const uint32_t validation_layer_count = 1;
  const char *validation_layer_names[validation_layer_count];

  // custom validation layer names
  validation_layer_names[0] = "VK_LAYER_KHRONOS_validation";

  if (vk_check_validation_layers(validation_layer_count,
                                 validation_layer_names) != 0) {
    return -1;
  }
  vk->layers.count = validation_layer_count;
  vk->layers.names = validation_layer_names;
  return 0;
}

static int vk_check_extensions(uint32_t ext_count, const char **ext_names) {
  uint32_t inst_ext_count;
  vkEnumerateInstanceExtensionProperties(NULL, &inst_ext_count, NULL);
  VkExtensionProperties inst_ext_props[inst_ext_count];
  vkEnumerateInstanceExtensionProperties(NULL, &inst_ext_count, inst_ext_props);

  C_VULKAN_LOG("checking %d extension(s):\n", ext_count);
  for (size_t i = 0; i < ext_count; i++) {
    bool found = false;
    for (size_t a = 0; a < inst_ext_count; a++) {
      if (strcmp(ext_names[i], inst_ext_props[i].extensionName)) {
        found = true;
        break;
      } else {
        continue;
      }
    }

    if (found) {
      C_VULKAN_LOG("\t- %s : found\n", ext_names[i]);
    } else {
      C_VULKAN_ERR("\t- %s : not found; exiting\n", ext_names[i]);
      return -1;
    }
  }
  C_VULKAN_LOG("Extension(s) found\n");
  return 0;
}

// TODO: make intercompatible
int vk_find_extensions(vulkan_t *vk, uint32_t i_extension_count,
                       const char *i_extension_names[i_extension_count]) {
  if (macos_extensions_active) {
    i_extension_count += 2;
    const char *extension_names[i_extension_count];
    memcpy(i_extension_names, extension_names, sizeof(i_extension_names));
    // macos portability options REQUIRED
    extension_names[i_extension_count - 2] = "VK_KHR_portability_enumeration";
    extension_names[i_extension_count - 1] =
        "VK_KHR_get_physical_device_properties2";

    if (vk_check_extensions(i_extension_count, extension_names) != 0) {
      return -1;
    }

    vk->extensions.count = i_extension_count;
    vk->extensions.names = extension_names;

  } else {
    C_VULKAN_LOG("here\n");
    const char *extension_names[vk->extensions.count];
    // SDL_Vulkan_GetInstanceExtensions(vk->sdl.window,
    // &vk->vk.extensions.count,
    //                                      extension_names);

    vk->extensions.names = extension_names;
  }

  return 0;
}

int vk_init(vulkan_t *vk) {
  const VkApplicationInfo app_info = (VkApplicationInfo){
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = NULL,

#ifndef APPLICATION_NAME
      .pApplicationName = "NDEF",
#else
      .pApplicationName = APPLICATION_NAME,
#endif

#ifndef APPLICATION_NAME
      .applicationVersion = 0,
#else
      .applicationVersion = APPLICATION_VERSION,
#endif
#ifndef APPLICATION_NAME
      .pEngineName = "NDEF",
#else
      .pEngineName = ENGINE_NAME,
#endif
#ifndef APPLICATION_NAME
      .engineVersion = 0,
#else
      .engineVersion = ENGINE_VERSION,
#endif
      .apiVersion = VK_API_VERSION_1_3,
  };

  // vkextensions
  if (vk_find_extensions(vk) != 0) {
    vk_quit(vk);
    return -1;
  }

  VkInstanceCreateInfo create_info = (VkInstanceCreateInfo){
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = NULL,
      .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
      .pApplicationInfo = &app_info,
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = NULL,
      .enabledExtensionCount = vk->extensions.count,
      .ppEnabledExtensionNames = vk->extensions.names,
  };

  // enable validation layers
  if (validation_layers_active) {
    if (vk_create_validation_layers(vk) != 0) {
      vk_quit(vk);
      return -1;
    }
    create_info.enabledLayerCount = vk->layers.count;
    create_info.ppEnabledLayerNames = vk->layers.names;
  }

  VkResult instance_result = vkCreateInstance(&create_info, NULL, &vk->inst);
  if (instance_result != VK_SUCCESS) {
    C_VULKAN_ERR("vkInstance couldn't get created: error: %d\n",
                 instance_result);
    vk_quit(vk);
    return -1;
  }

  if (vk_pick_pDevice(vk) != 0) {
    vk_quit(vk);
    return -1;
  }

  if (vk_create_logical_device(vk) != 0) {
    vk_quit(vk);
    return -1;
  }

  C_VULKAN_LOG("VK INITIALIZED\n");
  return 0;
}

void vk_quit(vulkan_t *vk) {
  vkDestroyDevice(vk->ldevice, NULL);
  C_VULKAN_LOG("VK LDEVICE DESTROYED\n");
  vkDestroyInstance(vk->inst, NULL);
  C_VULKAN_LOG("VK INSTANCE DESTROYED\n");

  // TODO: make api independent
  // SDL_Vulkan_UnloadLibrary();
  //
  C_VULKAN_LOG("SDL_VULKAN LIBRARY UNLOADED\n");
}

// ---------------
// C_VULKAN_LOGGER
// ---------------
#ifndef N_C_VULKAN_LOGGER
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// DEBUG    =   0
// OFF      =  -1
// INFO     =   1
// WARNING  =   2
// ERROR    =   3
static int active_level = -1;

// if NULL: no file logging (Default)
static char *logfile_path = NULL;

static const char *LOG_LEVEL_STRINGS[4] = {
    "[DEBUG]",
    "[C_VULKAN_LOG]",
    "[WARNING]",
    "[ERROR]",
};

static void add_time_to_string(char *string) {
  // add time
  time_t rawtime;
  struct tm *timeinfo;

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  strftime(string, sizeof(char *) * C_VULKAN_LOG_MAX_BUF, "[%H:%M:%S]",
           timeinfo);
}

static void add_info_to_string(char *string, LOG_LEVEL log_level,
                               const char *log_fname, const size_t line) {
  // add level
  strcat(string, LOG_LEVEL_STRINGS[log_level]);

  // add filename and linenumber
  size_t size = snprintf(NULL, 0, "[%s:%zu]", log_fname, line);
  char src[size + 1];
  snprintf(src, size + 1, "[%s:%zu]", log_fname, line);
  strcat(string, src);
}

static void add_frmt_to_string(char *string, const char *fmt, va_list fargv) {
  // add fmt
  size_t size = vsnprintf(NULL, 0, fmt, fargv);
  char src[size + 1];
  vsnprintf(src, size + 1, fmt, fargv);
  strcat(string, src);
}

void logger_log(LOG_LEVEL log_level, const char *log_fname, const size_t line,
                const char *fmt, ...) {
  C_VULKAN_ASSERT(active_level != -1, "error:%s:%zu: logger not initialized\n",
                  log_fname, line);
  if ((int)log_level < active_level)
    return;

  char string[C_VULKAN_LOG_MAX_BUF] = {0};
  va_list fargv;
  va_start(fargv, fmt);

  add_time_to_string(string);
  add_info_to_string(string, log_level, log_fname, line);
  add_frmt_to_string(string, fmt, fargv);

  // init terminal ptr
  FILE *tptr = stdout;
  if (log_level == ERROR) {
    tptr = stderr;
  }
  // output to stdout/stderr
  fprintf(tptr, "%s", string);

  // init file ptr
  if (logfile_path) {
    FILE *fptr = fopen(logfile_path, "a");
    C_VULKAN_ASSERT(fptr, "error: couldn't open log file\n")
    // output to file
    fprintf(fptr, "%s", string);
    fclose(fptr);
  }
}

void logger_init(LOG_LEVEL base_level, char *base_fpath) {
  C_VULKAN_ASSERT(active_level == -1,
                  "error; logger can only be initialized once\n");

  active_level = base_level;
  if (base_fpath != NULL) {
    logfile_path = base_fpath;
  }
}

void logger_deinit(void) {
  C_VULKAN_ASSERT(active_level != -1, "error: no Logger initialized\n")
  C_VULKAN_WARN("logger deinitialized");

  active_level = -1;
  logfile_path = NULL;
}

#endif // N_C_VULKAN_LOGGER
#endif // C_VULKAN_IMPLEMENTATION
