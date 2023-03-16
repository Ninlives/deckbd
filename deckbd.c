#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

#define ARRLEN(s) (sizeof(s)/sizeof(*s))

struct managed_device {
  struct libevdev* controller;
  struct libevdev* input;
  struct libevdev_uinput* uinput;
};

static const uint8_t modifiers[] = {
  KEY_LEFTSHIFT,
  KEY_LEFTCTRL,
  KEY_LEFTALT,
  KEY_RIGHTSHIFT,
  KEY_RIGHTCTRL,
  KEY_RIGHTALT,
};
#define MOD_LEFTSHIFT  (0x1)
#define MOD_LEFTCTRL   (0x1 << 1)
#define MOD_LEFTALT    (0x1 << 2)
#define MOD_RIGHTSHIFT (0x1 << 3)
#define MOD_RIGHTCTRL  (0x1 << 4)
#define MOD_RIGHTALT   (0x1 << 5)

struct mapping {
  uint16_t button;
  uint16_t modifier;
  uint16_t key;
  bool     pressed;
} mappings[] = {
  { .button = BTN_DPAD_UP,    .modifier = 0, .key = KEY_1, .pressed = false },
  { .button = BTN_DPAD_LEFT,  .modifier = 0, .key = KEY_2, .pressed = false },
  { .button = BTN_DPAD_DOWN,  .modifier = 0, .key = KEY_3, .pressed = false },
  { .button = BTN_DPAD_RIGHT, .modifier = 0, .key = KEY_4, .pressed = false },

  { .button = BTN_Y, .modifier = 0, .key = KEY_5, .pressed = false },
  { .button = BTN_X, .modifier = 0, .key = KEY_6, .pressed = false },
  { .button = BTN_A, .modifier = 0, .key = KEY_7, .pressed = false },
  { .button = BTN_B, .modifier = 0, .key = KEY_8, .pressed = false },

  { .button = BTN_TL, .modifier = 0, .key = KEY_9, .pressed = false },
  { .button = BTN_TR, .modifier = 0, .key = KEY_0, .pressed = false },

  { .button = BTN_TL2, .modifier = 0, .key = KEY_BACKSPACE, .pressed = false },
  { .button = BTN_TR2, .modifier = 0, .key = KEY_ENTER,     .pressed = false },
};

bool starts_with(const char* prefix, const char* str){
  return strncmp(prefix, str, strlen(prefix)) == 0;
}

int find_controller() {
  const uint16_t SD_TYPE = BUS_USB;
  const uint16_t SD_VID = 0x28de;
  const uint16_t SD_PID = 0x1205;

  int fd = -1;
  DIR* devs = opendir("/dev/input");
  struct dirent* dev;

  while ((dev = readdir(devs)) != NULL) {
    if (dev->d_type == DT_CHR && starts_with("event", dev->d_name)) {
      // MAX_LEN(d_name) == 256, strlen("/dev/input/") == 11
      char pre[268] = "/dev/input/";
      char* path = strcat(pre, dev->d_name);
      
      fd = open(path, O_RDONLY | O_NONBLOCK);
      if (fd < 0) {
        printf("Failed to open %s\n", path);
      } else {
        struct input_id info = {};
        int rc;

        rc = ioctl(fd, EVIOCGID, &info);
        if (rc < 0){
          printf("Failed to get device info of %s\n", path);
          goto cleanup;
        }

        if (info.bustype == SD_TYPE && info.vendor == SD_VID && info.product == SD_PID) {
          printf("Found controller: %s\n", path);
          goto final;
        }

      cleanup:
        close(fd);
      }
    }
  }
  closedir(devs);
  return -1;
final:
  closedir(devs);
  return fd;
}

struct managed_device init_evdev(int fd){
  struct managed_device device = { NULL, NULL, NULL };
  struct libevdev* controller = NULL;
  int rc = libevdev_new_from_fd(fd, &controller);
  if (rc != 0){
    printf("Failed to create libevdev.\n");
    return device;
  }
  
  struct libevdev* input = libevdev_new();
  libevdev_set_name(input, "deckbd");
  libevdev_enable_event_type(input, EV_KEY);
  for (size_t i = 0; i < ARRLEN(mappings); i++) {
    libevdev_enable_event_code(input, EV_KEY, mappings[i].key, NULL);
  };

  struct libevdev_uinput* uinput = NULL;
  rc = libevdev_uinput_create_from_device(input, LIBEVDEV_UINPUT_OPEN_MANAGED, &uinput);
  if (rc != 0) {
    libevdev_free(controller);
    libevdev_free(input);
    printf("Failed to create uinput device.\n");
    return device;
  }

  device.controller = controller;
  device.input = input;
  device.uinput = uinput;
  return device;
}

void free_evdev(struct managed_device device) {
  libevdev_uinput_destroy(device.uinput);
  libevdev_free(device.input);
  libevdev_free(device.controller);
}

void press(struct managed_device device, uint16_t modifier, uint32_t code) {
  struct libevdev_uinput* uinput = device.uinput;

  for (size_t i = 0; i < ARRLEN(modifiers); i++) {
    if (((0x1 << i) & modifier) != 0) {
      libevdev_uinput_write_event(uinput, EV_KEY, modifiers[i], 1);
      libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);
    } 
  }

  libevdev_uinput_write_event(uinput, EV_KEY, code, 1);
  libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);
  libevdev_uinput_write_event(uinput, EV_KEY, code, 0);
  libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);

  for (size_t i = 0; i < ARRLEN(modifiers); i++) {
    if (((0x1 << i) & modifier) != 0) {
      libevdev_uinput_write_event(uinput, EV_KEY, modifiers[i], 0);
      libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);
    } 
  }
}

void handle_event(struct managed_device device, struct input_event ev) {
  for (size_t i = 0; i < ARRLEN(mappings); i++) {
    struct mapping* entry = &mappings[i];
    if (entry->button != ev.code) continue;

    if (ev.value == 1) {
      entry->pressed = true;
    }

    if (ev.value == 0 && entry->pressed) {
      entry->pressed = false;
      press(device, entry->modifier, entry->key);
    }
    
    return;
  }
};

static volatile bool listening = true;
static void sig_handler(int signum) {
  listening = false;
};
static int init_signal_handler() {
  int rc;
  struct sigaction act = { 0 };
  act.sa_handler = &sig_handler;
  
  if ((rc = sigaction(SIGINT, &act, NULL)) != 0) {
    return rc;
  }

  if ((rc = sigaction(SIGTERM, &act, NULL)) != 0) {
    return rc;
  }

  return 0;
}

int main(void) {
  int rc;
  rc = init_signal_handler();
  if (rc != 0) {
    printf("Failed to initialize signal handler.\n");
    return -1;
  }

  int fd = find_controller();
  if (fd < 0) {
    printf("Failed to find controller.\n");
    return -1;
  }

  struct managed_device device = init_evdev(fd);
  if (device.uinput == NULL) {
    printf("Device initialisation failed.\n");
    close(fd);
    return -1;
  } 

  printf("Listening...\n");
  struct input_event ev;
  while(listening) {
    rc = libevdev_next_event(device.controller, LIBEVDEV_READ_FLAG_NORMAL, &ev);
    if (rc == 0 && ev.type == EV_KEY) {
      printf("type: %x, code: %x, value: %x\n", ev.type, ev.code, ev.value);
      handle_event(device, ev);
    }
  }
  
  free_evdev(device);
  printf("Deactivated.\n");
  return 0;
}
