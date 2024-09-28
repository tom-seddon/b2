/* TODO

manage static 6502_internal.inl

}

*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <shared/system.h>
#include "../beeb/include/beeb/BBCMicro.h"
#include "../beeb/include/beeb/sound.h"
#include "../beeb/include/beeb/TVOutput.h"
#include "core.h"
#include "libretro.h"
#include "adapters.h"

#define MAX_DISK_COUNT 10
static struct retro_log_callback logging;
static retro_log_printf_t log_cb;
static retro_environment_t environ_cb;

const char *retro_save_directory;
const char *retro_system_directory;
const char *retro_content_directory;
char retro_system_data_directory[512];
char retro_system_bios_directory[512];
char retro_system_save_directory[512];
char retro_content_filepath[512];
uint16_t audioBuffer[B2_SAMPLE_RATE*1000*2];

retro_usec_t curr_frame_time = 0;
retro_usec_t prev_frame_time = 0;
float waitPeriod = 0.001;
bool useSwFb = false;
bool useHalfFrame = false;
int borderSize = 0;
bool soundHq = true;
bool canSkipFrames = false;
bool enhancedRom = false;

unsigned maxUsers;
bool maxUsersSupported = true;

unsigned diskIndex = 0;
unsigned diskIndexInitial = 0;
unsigned diskCount = 1;
bool diskEjected = false;

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_set_led_state_t led_state_cb;


int autoboot=0;
bool sound_ddnoise=false;
bool sound_tape=false;
    uint64_t version;

BBCMicro *core = (BBCMicro *) 0;
TVOutput tv;
VideoDataUnit vdu;
SoundDataUnit sdu;

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
  (void)level;
  va_list va;
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  va_end(va);
}


void log_fatal(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  log_cb(RETRO_LOG_ERROR, fmt, va);
  va_end(va);
}

void log_error(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  log_cb(RETRO_LOG_ERROR, fmt, va);
  va_end(va);
}
void log_warn(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  log_cb(RETRO_LOG_WARN, fmt, va);
  va_end(va);
}
void log_info(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  log_cb(RETRO_LOG_INFO, fmt, va);
  va_end(va);
}
void log_info_OUTPUT(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  log_cb(RETRO_LOG_INFO, fmt, va);
  va_end(va);
}
void log_debug(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  log_cb(RETRO_LOG_DEBUG, fmt, va);
  va_end(va);
}

void set_frame_time_cb(retro_usec_t usec)
{
  if (usec == 0 || usec > 2*1000000/50)
  {
    curr_frame_time = 1000000/50;
  }
  else
  {
    curr_frame_time = usec;
  }
  prev_frame_time = curr_frame_time;

}

static void update_keyboard_cb(bool down, unsigned keycode,
                               uint32_t character, uint16_t key_modifiers)
{
  /*if(keycode != RETROK_UNKNOWN && core)
    core->update_keyboard(down,keycode,character,key_modifiers);*/
}

static void check_variables(void)
{
/*  struct retro_variable var =
  {
    .key = "ep128emu_wait",
  };
  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
  {
    waitPeriod = 0.001f * std::atoi(var.value);
  }

  var.key = "ep128emu_swfb";
  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
  {
    useSwFb = std::atoi(var.value) == 1 ? true : false;
  }

  var.key = "ep128emu_sdhq";
  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
  {
    bool soundHq_;
    soundHq_ = std::atoi(var.value) == 1 ? true : false;
    if (soundHq != soundHq_)
    {
      soundHq = soundHq_;
      if(core)
      {
        core->config->sound.highQuality = soundHq;
        core->config->soundSettingsChanged = true;
        core->config->applySettings();
      }
    }
  }

  var.key = "ep128emu_useh";
  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
  {
    useHalfFrame = std::atoi(var.value) == 1 ? true : false;
  }

  var.key = "ep128emu_brds";
  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
  {
    borderSize = std::atoi(var.value);
    if(core)
      core->borderSize = borderSize*2;
  }

  var.key = "ep128emu_romv";
  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
  {
    if(var.value[0] == 'E') { enhancedRom = true;}
    else { enhancedRom = false;}
  }

  std::string zoomKey;
  var.key = "ep128emu_zoom";
  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
  {
    zoomKey = var.value;
    Ep128Emu::stringToLowerCase(zoomKey);
  }

  std::string infoKey;
  var.key = "ep128emu_info";
  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
  {
    infoKey = var.value;
    Ep128Emu::stringToLowerCase(infoKey);
  }

  std::string autofireKey;
  var.key = "ep128emu_afbt";
  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
  {
    autofireKey = var.value;
    Ep128Emu::stringToLowerCase(autofireKey);
  }

  int autofireSpeed = -1;
  var.key = "ep128emu_afsp";
  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
  {
    autofireSpeed = std::atoi(var.value);
  }

  // If function is not supported, use all users (and don't interrogate again)
  if(maxUsersSupported && !environ_cb(RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS,&maxUsers)) {
    maxUsers = EP128EMU_MAX_USERS;
    maxUsersSupported = false;
    log_cb(RETRO_LOG_INFO, "GET_INPUT_MAX_USERS not supported, using fixed %d\n", EP128EMU_MAX_USERS);
  }

  if(core)
    core->initialize_joystick_map(zoomKey,infoKey,autofireKey, autofireSpeed,
    Ep128Emu::joystick_type.at("DEFAULT"), Ep128Emu::joystick_type.at("DEFAULT"), Ep128Emu::joystick_type.at("DEFAULT"),
    Ep128Emu::joystick_type.at("DEFAULT"), Ep128Emu::joystick_type.at("DEFAULT"), Ep128Emu::joystick_type.at("DEFAULT"));

  if(vmThread) vmThread->resetKeyboard();*/
}

/* If ejected is true, "ejects" the virtual disk tray.
 */
static bool set_eject_state_cb(bool ejected) {
  log_cb(RETRO_LOG_DEBUG, "Disk control: eject (%d)\n",ejected?1:0);
  diskEjected = ejected;
  return true;
}

/* Gets current eject state. The initial state is 'not ejected'. */
static bool get_eject_state_cb(void) {
//  log_cb(RETRO_LOG_DEBUG, "Disk control: get eject status (%d)\n",diskEjected?1:0);
  return diskEjected;
}

/* Gets current disk index. First disk is index 0.
 * If return value is >= get_num_images(), no disk is currently inserted.
 */
static unsigned get_image_index_cb(void) {
//  log_cb(RETRO_LOG_DEBUG, "Disk control: get image index (%d)\n",diskIndex);
  return diskIndex;
}

/* Sets image index. Can only be called when disk is ejected.
 */
static bool set_image_index_cb(unsigned index) {
  return true;
}

/* Gets total number of images which are available to use. */
static unsigned get_num_images_cb(void) {return diskCount;}

/* Replaces the disk image associated with index.
 * Arguments to pass in info have same requirements as retro_load_game().
 */
static bool replace_image_index_cb(unsigned index,
      const struct retro_game_info *info) {

  return true;
}

/* Adds a new valid index (get_num_images()) to the internal disk list.
 * This will increment subsequent return values from get_num_images() by 1.
 * This image index cannot be used until a disk image has been set
 * with replace_image_index. */
static bool add_image_index_cb(void) {
  log_cb(RETRO_LOG_DEBUG, "Disk control: add image index (current %d)\n",diskCount);
  return true;
}

/* Sets initial image to insert in drive when calling
 * core_load_game().
 * Returns 'false' if index or 'path' are invalid, or core
 * does not support this functionality
 */
static bool set_initial_image_cb(unsigned index, const char *path) {
  log_cb(RETRO_LOG_DEBUG, "Disk control: set initial image index to %d\n",diskCount);
  diskIndexInitial = index;
  return true;
}

/* Fetches the path of the specified disk image file.
 * Returns 'false' if index is invalid (index >= get_num_images())
 * or path is otherwise unavailable.
 */
static bool get_image_path_cb(unsigned index, char *path, size_t len) {
  if (index >= diskCount) return false;
/*  if(diskPaths[index].length() > 0)
  strncpy(path, diskPaths[index].c_str(), len);
  log_cb(RETRO_LOG_DEBUG, "Disk control: get image path (%d) %s\n",index,path);*/
  return true;
}

/* Fetches a core-provided 'label' for the specified disk
 * image file. In the simplest case this may be a file name
 * Returns 'false' if index is invalid (index >= get_num_images())
 * or label is otherwise unavailable.
 */
static bool get_image_label_cb(unsigned index, char *label, size_t len) {
  if(index >= diskCount) return false;
/*  if(diskNames[index].length() > 0)
  strncpy(label, diskNames[index].c_str(), len);*/
  //log_cb(RETRO_LOG_DEBUG, "Disk control: get image label (%d) %s\n",index,label);
  return true;
}

static bool add_new_image_auto(const char *path) {

  unsigned index = diskCount;
  if (diskCount >= MAX_DISK_COUNT) return false;
  diskCount++;
  log_cb(RETRO_LOG_DEBUG, "Disk control: add new image (%d) as %s\n",diskCount,path);

/*  diskPaths[index] = path;
  std::string contentPath;
  Ep128Emu::splitPath(diskPaths[index],contentPath,diskNames[index]);*/
  return true;
}


void retro_init(void)
{
  struct retro_log_callback log;
  printf("retro_init \n");
  // Init log
  if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
    log_cb = log.log;
  else
    log_cb = fallback_log;

  log_cb(RETRO_LOG_DEBUG, "retro_init started\n");

  struct retro_disk_control_callback dccb =
  {
   set_eject_state_cb,
   get_eject_state_cb,

   get_image_index_cb,
   set_image_index_cb,
   get_num_images_cb,

   replace_image_index_cb,
   add_image_index_cb
  };

  struct retro_disk_control_ext_callback dccb_ext =
  {
   set_eject_state_cb,
   get_eject_state_cb,

   get_image_index_cb,
   set_image_index_cb,
   get_num_images_cb,

   replace_image_index_cb,
   add_image_index_cb,
   set_initial_image_cb,

   get_image_path_cb,
   get_image_label_cb
  };

  unsigned dci;
  if (environ_cb(RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION, &dci)) {
    environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE, &dccb_ext);
    log_cb(RETRO_LOG_DEBUG, "Using extended disk control interface\n");
  } else {
    environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &dccb);
    log_cb(RETRO_LOG_DEBUG, "Using basic disk control interface\n");
  }

  struct retro_led_interface led_interface;
  if(environ_cb(RETRO_ENVIRONMENT_GET_LED_INTERFACE, &led_interface)) {
   if (led_interface.set_led_state && !led_state_cb) {
      led_state_cb = led_interface.set_led_state;
      log_cb(RETRO_LOG_DEBUG, "LED interface supported\n");
    } else {
      log_cb(RETRO_LOG_DEBUG, "LED interface not supported\n");
    }
  } else {
    log_cb(RETRO_LOG_DEBUG, "LED interface not present\n");
  }

  const char *system_dir = NULL;

  if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
  {
    // if defined, use the system directory
    retro_system_directory=system_dir;
  }

  const char *content_dir = NULL;
  if (environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir) && content_dir)
  {
    // if defined, use the system directory
    retro_content_directory=content_dir;
  }

  const char *save_dir = NULL;
  if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir)
  {
    // If save directory is defined use it, otherwise use system directory
    retro_save_directory = *save_dir ? save_dir : retro_system_directory;
  }
  else
  {
    // make retro_save_directory the same in case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY is not implemented by the frontend
    retro_save_directory=retro_system_directory;
  }

  if(system_dir == NULL)
  {
    strcpy(retro_system_bios_directory, ".");
  }
  else
  {
    strncpy(
      retro_system_bios_directory,
      retro_system_directory,
      sizeof(retro_system_bios_directory) - 1
    );
  }
  strncpy(
    retro_system_save_directory,
    retro_save_directory,
    sizeof(retro_system_save_directory) - 1
  );

  log_cb(RETRO_LOG_DEBUG, "Retro ROM DIRECTORY %s\n", retro_system_bios_directory);
  log_cb(RETRO_LOG_DEBUG, "Retro SAVE_DIRECTORY %s\n", retro_system_save_directory);
  log_cb(RETRO_LOG_DEBUG, "Retro CONTENT_DIRECTORY %s\n", retro_content_directory);

/*   static const struct retro_controller_info ports[EP128EMU_MAX_USERS+1] = {
      { Ep128Emu::controller_description, 11  }, // port 1
      { Ep128Emu::controller_description, 11  }, // port 2
      { Ep128Emu::controller_description, 11  }, // port 3
      { Ep128Emu::controller_description, 11  }, // port 4
      { Ep128Emu::controller_description, 11  }, // port 5
      { Ep128Emu::controller_description, 11  }, // port 6
      { NULL, 0 }
   };

   environ_cb( RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports );
*/
  struct retro_keyboard_callback kcb = { update_keyboard_cb };
  environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &kcb);
  struct retro_frame_time_callback ftcb = { set_frame_time_cb };
  environ_cb(RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK, &ftcb);

  environ_cb(RETRO_ENVIRONMENT_GET_CAN_DUPE,&canSkipFrames);
  //environ_cb(RETRO_ENVIRONMENT_GET_OVERSCAN,&showan);
  // safe mode
  canSkipFrames = false;



  enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
  if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
  {
    log_cb(RETRO_LOG_ERROR, "XRGB8888 is not supported.\n");
  }


  check_variables();
  log_cb(RETRO_LOG_DEBUG, "Creating core...\n");
  //core = new BBCMicro(&BBC_MICRO_TYPE_B,&DISC_INTERFACE_ACORN_1770,BBCMicroParasiteType_None,{},nullptr,0,nullptr,{0});
  core = new BBCMicro(&BBC_MICRO_TYPE_B,nullptr,BBCMicroParasiteType_None,{},nullptr,0,nullptr,{0});
    core->SetOSROM(LoadROM("OS12.ROM"));
    core->SetSidewaysROM(15, LoadROM("BASIC2.ROM"));
   //core->SetSidewaysROM(14, LoadROM("DFS-2.26.rom"));

  core->Update(&vdu,&sdu);
//  tv = TVOutput();

  check_variables();
  log_cb(RETRO_LOG_DEBUG, "Starting core...\n");
/*  core->start();
  core->change_resolution(core->currWidth,core->currHeight,environ_cb);*/
}

void retro_deinit(void)
{
}

void retro_get_system_info(struct retro_system_info *info)
{
  memset(info, 0, sizeof(*info));
  info->library_name     = "b2";
  info->library_version  = "v0.1";
  info->need_fullpath    = true;
  info->valid_extensions = "img|dsk|tap";
  printf("retro_get_system_info \n");
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
  printf("retro_get_system_av_info \n");

  float aspect = 4.0f / 3.0f;
  aspect = 4.0f / (3.0f);
  //aspect = 4.0f / (3.0f / (float) (EP128EMU_LIBRETRO_SCREEN_HEIGHT/(float)core->currHeight));
  info->timing = (struct retro_system_timing)
  {
    .fps = 50.0,
    .sample_rate = B2_SAMPLE_RATE_FLOAT,
  };

  info->geometry = (struct retro_game_geometry)
  {
    .base_width   = TV_TEXTURE_WIDTH,
    .base_height  = TV_TEXTURE_HEIGHT,
    .max_width    = TV_TEXTURE_WIDTH,
    .max_height   = TV_TEXTURE_HEIGHT,
    .aspect_ratio = aspect,
  };
  printf("retro_get_system_av_info \n");
}

void retro_set_environment(retro_environment_t cb)
{
  printf("retro_set_environment \n");

  environ_cb = cb;

  bool no_content = true;
  environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);

  if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
    log_cb = logging.log;
  else
    log_cb = fallback_log;

  static const struct retro_variable vars[] =
  {
    { "b2_sdhq", "High sound quality; 1|0" },
    { NULL, NULL },
  };
  environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);
  log_cb(RETRO_LOG_DEBUG, "Set environment end\n");
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
  audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
  audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
  input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
  input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
  video_cb = cb;
}

void retro_reset(void)
{
  printf("retro_reset \n");

  /*if(vmThread) vmThread->reset(true);*/
}

static void update_input(void)
{
//  log_cb(RETRO_LOG_DEBUG, "update input\n");
  input_poll_cb();
  /*core->update_input(input_state_cb, environ_cb, maxUsers);*/
}

static void render(void)
{
//  printf("retro_render \n");

  /*core->render(video_cb, environ_cb);*/


}

/*
static void audio_callback(void)
{
    audio_cb(0, 0);
}
*/
static void audio_callback_batch(void)
{
  size_t nFrames=0;
  int exp = int(float(1000000/50*B2_SAMPLE_RATE)/1000000.0f+0.5f);

  /*core->audioOutput->forwardAudioData((int16_t*)audioBuffer,&nFrames,exp);*/
  //printf("sending frames: %d exp %d frame_time: %d\n",nFrames,exp, curr_frame_time);
  //if (nFrames != exp)
  // printf("sending diff frames: %d exp %d frame_time: %d\n",nFrames,exp, curr_frame_time);
  audio_batch_cb((int16_t*)audioBuffer, exp);
}

void retro_run(void)
{
//  printf("retro_run \n");

  bool updated = false;
  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
    check_variables();

  void *buf = NULL;
static constexpr uint16_t WRCHV = 0x20e;
static constexpr uint16_t WORDV = 0x20c;
static constexpr uint16_t CLIV = 0x208;


    const uint32_t *pixels = tv.GetTexturePixels(&version);
    const uint8_t *ram = core->GetRAM();
    const M6502 *cpu = core->GetM6502();
//    M6502 *cpu2 = core->GetM6502_noconst();
   char disassembleMsg[255];
        for (size_t i = 0; i < 1024; ++i) {
            uint32_t update_result = core->Update(&vdu,
                                                  &sdu);

            if (update_result & BBCMicroUpdateResultFlag_VideoUnit) {
               //printf("retro_run tv update\n");
               tv.Update(&vdu, 1);
            }

        if (update_result & BBCMicroUpdateResultFlag_Host) {
            //printf("retro_run host update\n");
            if (M6502_IsAboutToExecute(cpu)) {
//                M6502_DisassembleLastInstruction(cpu2, disassembleMsg, 255, nullptr, nullptr);
//                printf("retro_run cpu exec %x %s\n", cpu2->pc.w, disassembleMsg);

                /*if (cpu->abus.b.l == ram[WORDV + 0] &&
                    cpu->abus.b.h == ram[WORDV + 1] &&
                    cpu->a == 0) {
                    printf("CPU instruction found %x %x\n", ram[0xffff],ram[0xfffe]);
                }*/
            }
        }

        }


        uint64_t new_version;
        pixels = tv.GetTexturePixels(&new_version);
        if (new_version > version) {
            version = new_version;
            printf("Frame advance %d\n",version);
        }

    std::vector<uint32_t> result(pixels, pixels + TV_TEXTURE_WIDTH * TV_TEXTURE_HEIGHT);

  unsigned stride  = TV_TEXTURE_WIDTH;
      video_cb(pixels, TV_TEXTURE_WIDTH, TV_TEXTURE_HEIGHT, stride << 2);
/*      for (int i=0;i<TV_TEXTURE_WIDTH * TV_TEXTURE_HEIGHT;i++)
         if (result[i]>0)
            printf("nonzero pixel %d %d \n",i,result[i]);*/

  /*if (useSwFb)
  {
    struct retro_framebuffer fb = {0};
    fb.width = core->currWidth;
    fb.height = core->currHeight;
    fb.access_flags = RETRO_MEMORY_ACCESS_WRITE;
#ifdef EP128EMU_USE_XRGB8888
    if (environ_cb(RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER, &fb) && fb.format == RETRO_PIXEL_FORMAT_XRGB8888)
#else
    if (environ_cb(RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER, &fb) && fb.format == RETRO_PIXEL_FORMAT_RGB565)
#endif // EP128EMU_USE_XRGB8888
    {
      buf = fb.data;
    }
  }*/
  update_input();
  /*core->run_for(curr_frame_time,waitPeriod,buf);*/
  audio_callback_batch();
  /*core->sync_display();*/
  render();
   /* LED interface */
/*   if (led_state_cb)
      update_led_interface();*/
}

bool header_match(const char* buf1, const unsigned char* buf2, size_t length)
{
  for (size_t i = 0; i < length; i++)
  {
    if ((unsigned char)buf1[i] != buf2[i])
    {
      return false;
    }
  }
  return true;
}

bool zx_header_match(const unsigned char* buf2)
{
  // as per original spec, "13 00 00 00" would fit, but it doesn't always match
  // https://sinclair.wiki.zxnet.co.uk/wiki/TAP_format
  // empirical boundaries are from scanning the tosec collection
  if (buf2[0]>0xe && buf2[0]<0x22 && buf2[1] == 0x0 && (buf2[2] == 0x0 || buf2[2] == 0xff))
    return true;
  return false;
}

bool retro_load_game(const struct retro_game_info *info)
{
  printf("retro_load_game \n");

  enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
  if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
  {
    log_cb(RETRO_LOG_ERROR, "XRGB8888 is not supported.\n");
    return false;
  }

  check_variables();
/*  if(info != nullptr)
  {
    if (core)
    {
      delete core;
      core = (Ep128Emu::LibretroCore *) 0;
      vmThread = (Ep128Emu::VMThread *) 0;
      config = (Ep128Emu::EmulatorConfiguration *) 0;
    }
    log_cb(RETRO_LOG_INFO, "Loading game: %s \n",info->path);
    std::string filename(info->path);
    std::string contentExt;
    std::string contentPath;
    std::string contentFile;
    std::string contentBasename;
    std::string configFile;
    std::string configFileExt(".ep128cfg");
    bool useConfigFile = false;

    size_t idx = filename.rfind('.');
    if(idx != std::string::npos)
    {
      contentExt = filename.substr(idx+1);
      configFile = filename.substr(0,idx);
      configFile += configFileExt;
      Ep128Emu::stringToLowerCase(contentExt);
    }
    else
    {
      contentExt=""; // No extension found
    }
    log_cb(RETRO_LOG_DEBUG, "Content extension: %s \n",contentExt.c_str());
    Ep128Emu::splitPath(filename,contentPath,contentFile);
    contentBasename = contentFile;
    diskPaths[0] = filename;
    diskNames[0] = contentBasename;
    Ep128Emu::stringToLowerCase(contentBasename);

    int contentLocale = Ep128Emu::LOCALE_UK;
    for(int i=1;i<Ep128Emu::LOCALE_AMOUNT;i++) {
      idx = filename.rfind(Ep128Emu::locale_identifiers[i]);
      if(idx != std::string::npos) {
        contentLocale = i;
        log_cb(RETRO_LOG_INFO, "Locale detected: %s \n",Ep128Emu::locale_identifiers[i].c_str());
        break;
      }
    }

    if(Ep128Emu::does_file_exist(configFile.c_str()))
    {
      useConfigFile = true;
      log_cb(RETRO_LOG_INFO, "Content specific configuration file: %s \n",configFile.c_str());
    }
    else
    {
      useConfigFile = false;
      log_cb(RETRO_LOG_DEBUG, "No content specific config file exists\n");
    }

    std::string diskExt = "img";
    std::string tapeExt = "tap";
    std::string tapeExtEp = "ept";
    std::string fileExtDtf = "dtf";
    std::string fileExtTvc = "cas";
    std::string diskExtTvc = "dsk";
    //std::string tapeExtSnd = "notwav";
    //std::string tapeExtZx = "tzx";
    std::string fileExtZx = "tap";
    std::string tapeExtCpc = "cdt";
    std::string tapeExtTvc = "tvcwav";

    std::FILE *imageFile;
    const size_t nBytes = 64;
    uint8_t tmpBuf[nBytes];
    uint8_t tmpBufOffset128[nBytes];
    uint8_t tmpBufOffset512[nBytes];
    static const char zeroBytes[nBytes] = "\0";

    imageFile = Ep128Emu::fileOpen(info->path, "rb");
    std::fseek(imageFile, 0L, SEEK_SET);
    if(std::fread(&(tmpBuf[0]), sizeof(uint8_t), nBytes, imageFile) != nBytes)
    {
      throw Ep128Emu::Exception("error reading game content file");
    };
    // TODO: handle seek / read failures
    std::fseek(imageFile, 128L, SEEK_SET);
    if(std::fread(&(tmpBufOffset128[0]), sizeof(uint8_t), nBytes, imageFile) != nBytes)
    {
      log_cb(RETRO_LOG_DEBUG, "Game content file too short for full header analysis\n");
    };
    std::fseek(imageFile, 512L, SEEK_SET);
    if(std::fread(&(tmpBufOffset512[0]), sizeof(uint8_t), nBytes, imageFile) != nBytes)
    {
      log_cb(RETRO_LOG_DEBUG, "Game content file too short for full header analysis\n");
    };
    std::fclose(imageFile);

    static const char *cpcDskFileHeader = "MV - CPCEMU";
    static const char *cpcExtFileHeader = "EXTENDED CPC DSK File";
    static const char *ep128emuTapFileHeader = "\x02\x75\xcd\x72\x1c\x44\x51\x26";
    static const char *epteFileMagic = "ENTERPRISE 128K TAPE FILE       ";
    static const char *TAPirFileMagic = "\x00\x6A\xFF";
    static const char *waveFileMagic = "RIFF";
    static const char *tzxFileMagic = "ZXTape!\032\001";
    static const char *tvcDskFileHeader = "\xeb\xfe\x90";
    static const char *epDskFileHeader1 = "\xeb\x3c\x90";
    static const char *epDskFileHeader2 = "\xeb\x4c\x90";
    static const char *epComFileHeader = "\x00\x05";
    static const char *epComFileHeader2 = "\x00\x06";
    static const char *epBasFileHeader = "\x00\x04";
    static const char *mp3FileHeader1 = "\x49\x44\x33";
    static const char *mp3FileHeader2 = "\xff\xfb";
    // Startup sequence may contain:
    // - chars on the keyboard (a-z, 0-9, few symbols like :
    // - 0xff as wait character
    // - 0xfe as "
    // - 0xfd as F1 (START)
    const char* startupSequence = "";
    tapeContent = false;
    diskContent = false;
    fileContent = false;
    int detectedMachineDetailedType = Ep128Emu::VM_config.at("VM_CONFIG_UNKNOWN");

    // start with longer magic strings - less chance of mis-detection
    if(header_match(cpcDskFileHeader,tmpBuf,11) or header_match(cpcExtFileHeader,tmpBuf,21))
    {
      detectedMachineDetailedType = Ep128Emu::VM_config.at("CPC_DISK");
      diskContent=true;
      startupSequence ="cat\r\xff\xff\xff\xff\xff\xffrun\xfe";
    }
    else if(header_match(tzxFileMagic,tmpBuf,9))
    {
      // if tzx format is called cdt, it is for CPC
      if (contentExt == tapeExtCpc)
      {
        detectedMachineDetailedType = Ep128Emu::VM_config.at("CPC_TAPE");
        tapeContent = true;
        startupSequence ="run\xfe\r\r";
      }
      // TODO: replace with something else?
      else if (contentExt == tapeExtEp)
      {
        detectedMachineDetailedType = Ep128Emu::VM_config.at("EP128_TAPE");
        tapeContent=true;
        startupSequence =" \xff\xff\xfd";
      }
      else
      {
        detectedMachineDetailedType = Ep128Emu::VM_config.at("ZX128_TAPE");
        tapeContent = true;
        startupSequence ="\r";
      }
    }
    // tvcwav extension is made up, it is to avoid clash with normal wave file and also with retroarch's own wave player
    else if(contentExt == tapeExtTvc && header_match(waveFileMagic,tmpBuf,4))
    {
      detectedMachineDetailedType = Ep128Emu::VM_config.at("TVC64_TAPE");
      tapeContent=true;
      startupSequence =" \xffload\r";
    }
    else if (contentExt == fileExtZx && zx_header_match(tmpBuf))
    {
      detectedMachineDetailedType = Ep128Emu::VM_config.at("ZX128_FILE");
      fileContent=true;
      startupSequence ="\r";
    }
    // All .tap files will fall back to be interpreted as EP128_TAPE
    else if(header_match(epteFileMagic,tmpBufOffset128,32) || header_match(ep128emuTapFileHeader,tmpBuf,8) ||
            header_match(waveFileMagic,tmpBuf,4) || header_match(TAPirFileMagic,tmpBufOffset512,3) ||
            header_match(mp3FileHeader1,tmpBuf,3) || header_match(mp3FileHeader2,tmpBufOffset512,2) ||
            contentExt == tapeExt )
    {
      detectedMachineDetailedType = Ep128Emu::VM_config.at("EP128_TAPE");
      tapeContent=true;
      startupSequence =" \xff\xff\xfd";
    }
    else if (contentExt == fileExtTvc && header_match(zeroBytes,&(tmpBuf[5]),nBytes-6))
    {
      detectedMachineDetailedType = Ep128Emu::VM_config.at("TVC64_FILE");
      fileContent=true;
      startupSequence =" \xffload\r";
    }
    // EP and TVC disks may have similar extensions
    else if (contentExt == diskExt || contentExt == diskExtTvc)
    {
      if (header_match(tvcDskFileHeader,tmpBuf,3))
      {
        detectedMachineDetailedType = Ep128Emu::VM_config.at("TVC64_DISK");
        diskContent=true;
        // ext 2 - dir - esc - load"
        startupSequence =" ext 2\r dir\r \x1bload\xfe";
      }
      else if (header_match(epDskFileHeader1,tmpBuf,3) || header_match(epDskFileHeader2,tmpBuf,3))
      {
        detectedMachineDetailedType = Ep128Emu::VM_config.at("EP128_DISK");
        diskContent=true;
      }
      else {
        log_cb(RETRO_LOG_ERROR, "Content format not recognized!\n");
        return false;
      }
    }
    else if (contentExt == fileExtDtf) {
      detectedMachineDetailedType = Ep128Emu::VM_config.at("EP128_FILE_DTF");
      fileContent=true;
      startupSequence =" \xff\xff\xff\xff\xff:dl ";
    }
    // last resort: EP file, first 2 bytes
    else if (header_match(epComFileHeader,tmpBuf,2) || header_match(epComFileHeader2,tmpBuf,2) || header_match(epBasFileHeader,tmpBuf,2))
    {
      detectedMachineDetailedType = Ep128Emu::VM_config.at("EP128_FILE");
      fileContent=true;
      startupSequence =" \xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd";
    }
    else
    {
      log_cb(RETRO_LOG_ERROR, "Content format not recognized!\n");
      return false;
    }
    try
    {
      log_cb(RETRO_LOG_DEBUG, "Creating core\n");
      check_variables();
      core = new Ep128Emu::LibretroCore(log_cb, detectedMachineDetailedType, contentLocale, canSkipFrames,
                                        retro_system_bios_directory, retro_system_save_directory,
                                        startupSequence,configFile.c_str(), useConfigFile, useHalfFrame, enhancedRom);
      log_cb(RETRO_LOG_DEBUG, "Core created\n");
      config = core->config;
      check_variables();
      if (core->machineDetailedType == Ep128Emu::VM_config.at("EP128_TAPE")) {
        tapeContent = true;
        fileContent = false;
        diskContent = false;
        log_cb(RETRO_LOG_DEBUG, "Tape content override\n");
      }
      if (diskContent)
      {
        config->floppy.a.imageFile = info->path;
        config->floppyAChanged = true;
      }
      if (tapeContent)
      {
        config->tape.imageFile = info->path;
        config->tapeFileChanged = true;*/
        // Todo: add tzx based advanced detection here
        /*    tape = openTapeFile(fileName.c_str(), 0,
                        defaultTapeSampleRate, bitsPerSample);*/
      /*}
      if (diskContent || tapeContent) {
        scan_multidisk_files(info->path);
        if (diskIndexInitial > 0)
          set_image_index_cb(diskIndexInitial);
      }
      if (fileContent)
      {
        config->fileio.workingDirectory = contentPath;
        contentFileName=contentPath+contentFile;
        core->vm->setFileNameCallback(&fileNameCallback, NULL);
        config->fileioSettingsChanged = true;
        config->vm.enableFileIO=true;
        config->vmConfigurationChanged = true;
        if( detectedMachineDetailedType == Ep128Emu::VM_config.at("EP128_FILE_DTF") ) {
          core->startSequence += contentBasename+"\r";
        }
      }
      config->applySettings();

      if (tapeContent)
      {
        // ZX tape will be started at the end of the startup sequence
        if (core->machineType == Ep128Emu::MACHINE_ZX || config->tape.forceMotorOn)
        {
        }
        // for other machines, remote control will take care of actual tape control, just start it
        else
        {
          core->vm->tapePlay();
        }
      }
    }
    catch (...)
    {
      log_cb(RETRO_LOG_ERROR, "Exception in load_game\n");
      throw;
    }

    // ep128emu allocates memory per 16 kB segments
    // actual place in the address map differs between ep/tvc/cpc/zx
    // so all slots are scanned, but only 576 kB is offered as map
    // to cover some new games that require RAM extension
    struct retro_memory_descriptor desc[36];
    memset(desc, 0, sizeof(desc));
    int dindex=0;
    for(uint8_t segment=0; dindex<32 ; segment++) {
       if(core->vm->getSegmentPtr(segment)) {
         desc[dindex].start=segment << 14;
         desc[dindex].select=0xFF << 14;
         desc[dindex].len= 0x4000;
         desc[dindex].ptr=core->vm->getSegmentPtr(segment);
         desc[dindex].flags=RETRO_MEMDESC_SYSTEM_RAM;
         dindex++;
       }
       if(segment==0xFF) break;
    }
    struct retro_memory_map retromap = {
        desc,
        sizeof(desc)/sizeof(desc[0])
    };
    environ_cb(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &retromap);

    config->setErrorCallback(&cfgErrorFunc, (void *) 0);
    vmThread = core->vmThread;
    log_cb(RETRO_LOG_DEBUG, "Starting core\n");
    core->start();
  }
*/
  return true;
}

void retro_unload_game(void)
{
/*  try
  {
    config->floppy.a.imageFile = "";
    config->floppyAChanged = true;
    config->applySettings();
  }
  catch (...)
  {
    log_cb(RETRO_LOG_ERROR, "Exception in unload_game\n");
    throw;
  }
*/
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
  printf("retro_load_game_special \n");
  if (type != 0x200)
    return false;
  if (num != 2)
    return false;
  return retro_load_game(NULL);
}

size_t retro_serialize_size(void)
{
/*  if(core && core->config) {
    return (size_t)EP128EMU_SNAPSHOT_SIZE + (size_t)(core->config->memory.ram.size > 128 ? (core->config->memory.ram.size-128)*1024 : 0);
  } else*/
    return B2_SNAPSHOT_SIZE;
}

bool retro_serialize(void *data_, size_t size)
{
 /* if (size < retro_serialize_size())
    return false;

  memset( data_, 0x00,size);

  Ep128Emu::File  f;
  core->vm->saveState(f);
  f.writeMem(data_, size);
*/
  return true;
}

bool retro_unserialize(const void *data_, size_t size)
{
  if (size < retro_serialize_size())
    return false;
/*
  unsigned char *buf= (unsigned char*)data_;

  // workaround: find last non-zero byte - which will be crc32 of the end-of-file chunk type, 6A 50 08 5E, so essentially the end of content
  size_t lastNonZeroByte = size-1;
  for (size_t i=size-1; i>0; i--)
  {
    if(buf[i] != 0)
    {
      lastNonZeroByte = i;
      break;
    }
  }
  Ep128Emu::File  f((unsigned char *)data_,lastNonZeroByte+1);
  core->vm->registerChunkTypes(f);
  f.processAllChunks();
  core->config->applySettings();
  core->startSequenceIndex = core->startSequence.length();
  if(vmThread) vmThread->resetKeyboard();
*/
  // todo: restore filenamecallback if file is used?
  return true;
}

void *retro_get_memory_data(unsigned id)
{
  (void)id;
  return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
  (void)id;
  return 0;
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
  (void)index;
  (void)enabled;
  (void)code;
}

unsigned retro_api_version(void)
{
  return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
  printf("retro_set_controller_port_device \n");

  //log_cb(RETRO_LOG_INFO, "Plugging device %u into port %u.\n", device, port);
/*  std::map< unsigned, std::string>::const_iterator  iter_joytype;
  iter_joytype = Ep128Emu::joystick_type_retrodev.find(device);
  if (port < EP128EMU_MAX_USERS && iter_joytype != Ep128Emu::joystick_type_retrodev.end())
  {
    int userMap[EP128EMU_MAX_USERS] = {
      Ep128Emu::joystick_type.at("DEFAULT"), Ep128Emu::joystick_type.at("DEFAULT"),
      Ep128Emu::joystick_type.at("DEFAULT"), Ep128Emu::joystick_type.at("DEFAULT"),
      Ep128Emu::joystick_type.at("DEFAULT"), Ep128Emu::joystick_type.at("DEFAULT")};

    unsigned mappedDev = Ep128Emu::joystick_type.at((*iter_joytype).second);
    log_cb(RETRO_LOG_DEBUG, "Mapped device %s for user %u \n", (*iter_joytype).second.c_str(), port);

    userMap[port] = mappedDev;
    if(core)
      core->initialize_joystick_map(std::string(""),std::string(""),std::string(""),-1,userMap[0],userMap[1],userMap[2],userMap[3],userMap[4],userMap[5]);
  }*/
}

unsigned retro_get_region(void)
{
  return RETRO_REGION_PAL;
}
