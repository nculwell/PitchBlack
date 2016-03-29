
#include "SDL.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

unsigned round_up_to_power_of_2(unsigned n); // Needed for initializers.

char instructions[] =
  "Welcome to Pitch Black.\n"
  "AWSD to move.\n"
  "Hold shift to run.\n"
  "Hold JKL to point sonar left, back or right.\n"
  "Control-Q to quit.\n"
  "Control-H to repeat instructions.\n"
;

char instructions_image_filename[] = "instructions.bmp";
char gameboard_image_filename[] = "gameboard.bmp";
char player_image_filename[] = "player.bmp";

const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;

// Game parameters.
#define FPS (5)                         // 1 frame = 1/5 sec.
const int FRAME_DUR_MS = 1000 / FPS;
const double TURN_CIRCLE_UNITS = 10.0;  // It takes 2 secs to turn 360.
const double MOVE_RATE = 5.0;           // Move 5 pixels per frame.
const double PLAYER_RADIUS = 3.0;

// Audio parameters.
int AUDIO_AMPLITUDE = 28000;
int AUDIO_FREQUENCY = 22050; // or 44100?
int AUDIO_CHANNELS = 2; // stereo
int AUDIO_SAMPLES = 512;

void init_parameters() {
}

// SDL setup info.
SDL_Window* window;
SDL_Renderer* renderer = NULL;
SDL_Texture* instructions_image = NULL;
SDL_Texture* gameboard_image = NULL;
SDL_Texture* player_image = NULL;
SDL_AudioDeviceID audio_device_id;
//MixChunk* footstep_chunk;
//MixChunk* sonar_chunk;

// Game state.
int quitting = 0;                          // 1 = quit the game.
int show_game = 1;                         // Set at startup.
int player_facing;                         // TURN_CIRCLE_UNITS, 0 = east, increases ccw.
double player_x = 100.0, player_y = 100.0; // FIXME: Improve this.
volatile double sonar_frequency = 0.0;     // This is set in every frame.
int player_moved = 0;                      // This is set in every frame.

void init();
SDL_Texture* load_image_as_texture(const char* image_filename);
void main_loop();
void pump_events();
void poll_keyboard();
void play_sounds();
void audio_callback(void* userdata, Uint8* stream, int len);
//void set_sonar_frequency(double beep_frequency);
void update_display();

int main(int argc, char** argv) {
  init_parameters();
  printf(instructions);
  fflush(stdout);
  init();
  main_loop();
  return 0;
}

void error(const char* msg, const char* detail) {
  fprintf(stderr, "Fatal error, aborting the game.\n");
  fprintf(stderr, msg);
  if (detail != NULL)
    fprintf(stderr, ": %s\n", detail);
  else
    fprintf(stderr, ".\n");
  exit(1);
}

void init() {
  // Initialize some constants.
  player_facing = TURN_CIRCLE_UNITS / 4; // Due north.

  // Set up SDL.
  if (0 != SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    error("Unable to initialize SDL", SDL_GetError());
  atexit(SDL_Quit);
  window = SDL_CreateWindow("PitchBlack",
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      SCREEN_WIDTH, SCREEN_HEIGHT,
      SDL_WINDOW_INPUT_GRABBED);
  if (window == NULL)
    error("Unable to open a window", SDL_GetError());
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (renderer == NULL)
    error("Unable to create SDL renderer", SDL_GetError());

  // Set up audio.
  SDL_AudioSpec requested_audio_spec;
  SDL_memset(&requested_audio_spec, 0, sizeof(requested_audio_spec));
  requested_audio_spec.freq = AUDIO_FREQUENCY;
  requested_audio_spec.format = AUDIO_S16SYS; // code is written for int16_t
  requested_audio_spec.channels = AUDIO_CHANNELS;
  requested_audio_spec.samples = AUDIO_SAMPLES;
  requested_audio_spec.callback = audio_callback;
  audio_device_id = SDL_OpenAudioDevice(
      NULL, // no specifically requested device
      0, // is not a recording device
      &requested_audio_spec, // input parameters
      NULL, // ignore output parameters (automatically convert format)
      SDL_AUDIO_ALLOW_FORMAT_CHANGE // allow chosen format to be different
      );                            // from requested format
  SDL_PauseAudioDevice(audio_device_id, 0); // start playing immediately

  // Load image files.
  instructions_image = load_image_as_texture(instructions_image_filename);
  gameboard_image = load_image_as_texture(gameboard_image_filename);
  player_image = load_image_as_texture(player_image_filename);
}

SDL_Texture* load_image_as_texture(const char* image_filename) {
  char path[100];
  sprintf(path, "assets/%s", image_filename);
  SDL_Surface* surface = SDL_LoadBMP(path);
  if (surface == NULL) {
    fprintf(stderr, "Failed to load image: %s\n", image_filename);
    error("Load failed", SDL_GetError());
  }
  SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (texture == NULL) {
    fprintf(stderr, "Failed to load image: %s\n", image_filename);
    error("Failed to convert image to texture", SDL_GetError());
  }
  SDL_FreeSurface(surface);
  return texture;
}

void main_loop()
{
  while (!quitting) {
    pump_events();   // Check control keypresses.
    poll_keyboard(); // Movement and turning.

    if (show_game) {
      // Blit the game display.
      update_display();
    } else {
      // Blit the instructions on the screen.
      SDL_RenderCopy(renderer, instructions_image, NULL, NULL);
    }
    SDL_UpdateWindowSurface(window);

    play_sounds();

    // Wait for the next frame.
    SDL_Delay(FRAME_DUR_MS); // FIXME: Implement properly.
  }
}

void pump_events() {
  SDL_Event e;
  while(SDL_PollEvent(&e) != 0) {
    switch (e.type) {
      case SDL_QUIT:
        quitting = 1;
        return;
        break;
      case SDL_KEYDOWN:
        {
          // This just handles control key presses.
          // None of these are the same as the keys that
          // are handled by the keyboard state poll, so
          // the state poll shouldn't have to worry about
          // the state of the control key.
          SDL_KeyboardEvent* ke = (SDL_KeyboardEvent*)&e;
          if ((ke->keysym.mod & KMOD_CTRL) != 0) {
            switch (ke->keysym.sym) {
              case SDLK_h:
                // Play help.
                break;
              case SDLK_q:
                quitting = 1;
                return;
            }
          }
        }
        break;
    }
  }
}

void poll_keyboard() {
  int numkeys;
  const Uint8* keyboard_state = SDL_GetKeyboardState(&numkeys);

  // Facing 0 = due east (right side of the screen).
  // Increasing values rotate counterclockwise.

  // Turn the player.
  if (keyboard_state[SDL_SCANCODE_A]) {
    // Turn left.
    player_facing++;
  }
  if (keyboard_state[SDL_SCANCODE_D]) {
    // Turn right.
    player_facing--;
  }
  if (player_facing < 0) {
    player_facing = TURN_CIRCLE_UNITS - 1;
  } else if (player_facing == TURN_CIRCLE_UNITS) {
    player_facing = 0;
  }

  // Convert player heading to radians.
  double player_heading =
    2.0 * M_PI * ((double)player_facing / (double)TURN_CIRCLE_UNITS);
  // Decompose player movement into N and E vectors.
  double player_move_n = MOVE_RATE * sin(player_heading);
  double player_move_e = MOVE_RATE * cos(player_heading);

  // Move the player.
  double dest_x = player_x;
  double dest_y = player_y;
  if (keyboard_state[SDL_SCANCODE_W]) {
    // Move forward.
    dest_x = player_x + player_move_e;
    dest_y = player_y - player_move_n;
  }
  if (keyboard_state[SDL_SCANCODE_S]) {
    // Move back.
    dest_x = player_x - player_move_e;
    dest_y = player_y + player_move_n;
  }
  // Do collision detection, and change movement if needed.
  // TODO
  // Update the player's position.
  if (player_x == dest_x && player_y == dest_y) {
    player_moved = 0;
  } else {
    player_x = dest_x, player_y = dest_y;
    player_moved = 1;
  }
  // TODO: Round facing to fixed points to prevent
  // cumulative rounding errors.

  // Sonar points straight ahead unless altered.
  double sonar_facing = player_heading;
  if (keyboard_state[SDL_SCANCODE_J]) {
    // Point sonar left.
    sonar_facing = player_heading + 0.5 * M_PI;
  }
  if (keyboard_state[SDL_SCANCODE_K]) {
    // Point sonar back.
    sonar_facing = player_heading + 1.0 * M_PI;
  }
  if (keyboard_state[SDL_SCANCODE_L]) {
    // Point sonar right.
    sonar_facing = player_heading + 1.5 * M_PI;
  }
  // Keep facing in range [0, 2*M_PI).
  if (sonar_facing >= 2.0 * M_PI) {
    sonar_facing -= 2.0 * M_PI;
  }

#if 0
  // Calculate sonar distance.
  if (detect_collision_point(
    double origin_x, double origin_y,
    double dest_x, double dest_y,
    double* collision_x, double* collision_y
    ))
  {
    // do nothing for now.
  }
#endif
}

#if 0
int detect_collision_point(
    double origin_x, double origin_y,
    double dest_x, double dest_y,
    double* collision_x, double* collision_y
    )
{
  double delta_x = dest_x - origin_x;
  double delta_y = dest_y - origin_y;
  double direction = arctan(delta_y / delta_x);
  double distance_x = abs(delta_x);
  double distance_y = abs(delta_y);
  double move_x = 0, move_y = 0;
  while (move_x < distance_x && move_y < distance_y) {
    if (
  }
}
#endif

void play_sounds() {

  SDL_LockAudioDevice(audio_device_id);

  // Play sonar.
  if (sonar_frequency == 261.63) {
    sonar_frequency = 293.66; // D4
  } else {
    sonar_frequency = 261.63; // middle C (C4)
  }

  // Play footstep.
  if (player_moved) {
    // No footstep for now.
  }

  SDL_UnlockAudioDevice(audio_device_id);
}

#if 0
void audio_callback(void* userdata, Uint8* stream, int len) {
  //fprintf(stderr, "Len: %d, Freq: %g\n", len, sonar_frequency);
  static double sonar_phase[4] = { 0, 0, 0, 0 };
  memset(stream, 0, len); // fill buffer with silence
  int16_t chord[4];
  chord[0] = sonar_frequency;
  if (chord[0] == 261.63) {
    chord[1] = 329.63;
    chord[2] = 392.00;
    chord[3] = 523.25;
  } else if (chord[0] == 293.66) {
    chord[1] = 349.23;
    chord[2] = 440.00;
    chord[3] = 587.33;
  } else {
    chord[1] = 0;
    chord[2] = 0;
    chord[3] = 0;
  }
  double current_phase = sonar_phase;
  unsigned n_samples = len / sizeof(int16_t);
  int16_t* stream16 = (int16_t*)stream; // use 16-bit samples
  for (int note = 0; note < 4; note++) {
    double frequency = chord[note];
    current_phase = sonar_phase;
    if (frequency > 0) {
      // 16-bit samples, stereo order LRLRLR
      double wave_period_samples = (double)AUDIO_FREQUENCY / frequency;
      double sonar_phase_increment = 2 * M_PI / wave_period_samples;
      double volume = AUDIO_AMPLITUDE / 8;
      unsigned i = 0;
      while (i < n_samples) {
        int16_t sample_value = volume * sin(current_phase);
        // sample_value = sample_value > 0 ? volume : -volume; // square wave
        stream16[i++] += sample_value;
        stream16[i++] += sample_value;
        current_phase += sonar_phase_increment;
        if (current_phase >= 2 * M_PI)
          current_phase = 0.0;
      }
    }
  }
  sonar_phase = current_phase;
}
#endif

void audio_callback(void* userdata, Uint8* stream, int len) {
  //fprintf(stderr, "Len: %d, Freq: %g\n", len, sonar_frequency);
  static double sonar_phase[4] = { 0, 0, 0, 0 };
  memset(stream, 0, len); // fill buffer with silence
  double chord[4];
  chord[0] = sonar_frequency;
  if (chord[0] == 261.63) {
    chord[1] = 329.63;
    chord[2] = 392.00;
    chord[3] = 523.25;
  } else if (chord[0] == 293.66) {
    chord[1] = 349.23;
    chord[2] = 440.00;
    chord[3] = 587.33;
    //chord[0] = 261.63;
    //chord[1] = 311.13;
    //chord[2] = 392.00;
    //chord[3] = 523.25;
  } else {
    fprintf(stderr, "<PLAYING NOTHING>\n");
    chord[0] = 0;
    chord[1] = 0;
    chord[2] = 0;
    chord[3] = 0;
  }
  // 16-bit samples, stereo order LRLRLR
  unsigned n_samples = len / sizeof(int16_t);
  int16_t* stream16 = (int16_t*)stream; // use 16-bit samples
  double volume = AUDIO_AMPLITUDE / 8;
  for (int note = 0; note < 4; note++) {
    double frequency = chord[note];
    if (frequency > 0) {
      double wave_period_samples = (double)AUDIO_FREQUENCY / frequency;
      double sonar_phase_increment = 2 * M_PI / wave_period_samples;
      double phase = sonar_phase[note];
      unsigned i = 0;
      while (i < n_samples) {
        int16_t sample_value = volume * sin(phase);
        // sample_value = sample_value > 0 ? volume : -volume; // square wave
        stream16[i++] += sample_value;
        stream16[i++] += sample_value;
        phase += sonar_phase_increment;
        if (phase >= 2 * M_PI)
          phase = 0.0;
      }
      sonar_phase[note] = phase;
    }
  }
}

void update_display() {
  // Draw the base of the gameboard.
  SDL_RenderCopy(renderer, gameboard_image, NULL, NULL);
  // Draw the players.
  // TODO: Multiple players.
  SDL_Rect dest_rect;
  dest_rect.x = (int)player_x;
  dest_rect.y = (int)player_y;
  SDL_RenderCopy(renderer, player_image, NULL, &dest_rect);
}

unsigned round_up_to_power_of_2(unsigned n) {
  if (n == 0)
    return 0;
  unsigned rounded = 1;
  while (rounded < n) {
    rounded <<= 1;
  }
  return rounded;
}

