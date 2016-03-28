
#include "SDL.h"
//#include "SDL_mixer.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846
#endif

unsigned round_up_to_power_of_2(unsigned n); // Needed for initializers.

const char[] instructions =
  "Welcome to Pitch Black.\n"
  "AWSD to move.\n"
  "Hold shift to run.\n"
  "Hold JKL to point sonar left, back or right.\n"
  "Control-Q to quit.\n"
  "Control-H to repeat instructions.\n"
;

const char[] instructions_image_filename = "instructions.bmp";
const char[] gameboard_image_filename = "gameboard.bmp";

const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;

// Game parameters.
const int FPS = 5;
const int FRAME_DUR_MS = 1000 / FPS;    // 1 frame = 1/5 sec.
const double TURN_CIRCLE_UNITS = 10.0;  // It takes 2 secs to turn 360.
const double MOVE_RATE = 5.0;           // Move 5 pixels per frame.
const double PLAYER_RADIUS = 3.0;

// Audio parameters.
const int AUDIO_AMPLITUDE = 28000;
const int AUDIO_FREQUENCY = 44100;
const int AUDIO_CHANNELS = 2; // stereo
// Buffer should be big enough for a frame worth of audio.
const int AUDIO_SAMPLES = round_up_to_power_of_2(AUDIO_FREQUENCY / FPS);

// SDL setup info.
SDL_Renderer* renderer = NULL;
SDL_Texture* instructions_image = NULL;
SDL_Texture* gameboard_image = NULL;
SDL_Texture* player_image = NULL;
SDL_AudioDeviceID audio_device_id;
SDL_AudioSpec audio_spec;
//MixChunk* footstep_chunk;
//MixChunk* sonar_chunk;

// Game state.
int quitting = 0;                          // 1 = quit the game.
int show_game = 0;                         // Set at startup.
int player_facing = TURN_CIRCLE_UNITS / 4; // Due north.
double player_x = 100.0, player_y = 100.0; // FIXME: Improve this.
double sonar_facing = 0.0;                 // This is set in every frame.
int player_moved = 0;                      // This is set in every frame.

void init();
void main_loop();
void pump_events();
void poll_keyboard();
void play_sounds();
void update_display();

int main(int argc, char** argv) {
  printf(instructions);
  fflush(stdout);
  init();
  main_loop();
  return 0;
}

void error(const char* msg) {
  fprintf(stderr, msg);
  fprintf(stderr, ": %s\n", SDL_GetError());
  fprintf(stderr, "Aborting the game.\n");
  exit(1);
}

void init() {
  SDL_Surface* gameboard_image;
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0)
    error("Unable to initialize SDL");
  atexit(SDL_Quit);

  SDL_AudioSpec requested_audio_spec;
  SDL_memset(&requested_audio_spec, 0, sizeof(requested_audio_spec));
  requested_audio_spec.freq = AUDIO_FREQUENCY;
  requested_audio_spec.format = AUDIO_F32;
  requested_audio_spec.channels = AUDIO_CHANNELS;
  requested_audio_spec.samples = AUDIO_SAMPLES;
  requested_audio_spec.callback = NULL;
  audio_device_id = SDL_OpenAudioDevice(NULL, 0, &requested_audio_spec,
      &audio_spec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
  SDL_PauseAudioDevice(audio_device_id, 0);

  SDL_Window* win = SDL_CreateWindow("PitchBlack",
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      SCREEN_WIDTH, SCREEN_HEIGHT,
      SDL_WINDOW_INPUT_GRABBED);
  if (win == NULL)
    error("Unable to open a window");
  renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
  if (renderer == NULL)
    error("Unable to create SDL renderer");

  SDL_Surface* instructions_surface = SDL_LoadBMP(instructions_image_filename);
  if (instructions_surface == NULL)
    error("Unable to load instructions image file");
  instructions_image = SDL_CreateTextureFromSurface(renderer, instructions_surface);
  if (instructions_image == NULL)
    error("Unable to create instructions image renderer");
  SDL_FreeSurface(instructions_surface);

  SDL_Surface* gameboard_surface = SDL_LoadBMP(gameboard_image_filename);
  if (gameboard_surface == NULL)
    error("Unable to load gameboard image file");
  gameboard_image = SDL_CreateTextureFromSurface(renderer, gameboard_surface);
  if (gameboard_image == NULL)
    error("Unable to create gameboard image renderer");
  SDL_FreeSurface(gameboard_surface);
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
      SDL_BlitSurface(screen, NULL, instructions_image, NULL);
    }
    SDL_UpdateWindowSurface(screen);

    play_sounds();

    // Wait for the next frame.
    SDL_Delay(FRAME_DUR_MS); // FIXME: Implement properly.
  }
}

void pump_events() {
  SDL_Event e;
  while(SDL_PollEvent(&e) != 0) {
    switch (e.type) {
      case SQL_QUIT:
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
          SDL_KeyboardEvent* ke = (SDL_KeyboardEvent)&e;
          if (ke->mod & KMOD_CTRL != 0) {
            switch (e->keysym->sym) {
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
  int* numkeys;
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
    2.0 * PI * ((double)player_facing / (double)TURN_CIRCLE_UNITS);
  // Decompose player movement into N and E vectors.
  double player_move_n = MOVE_RATE * sin(player_heading);
  double player_move_e = MOVE_RATE * cos(player_heading);

  // Move the player.
  if (keyboard_state[SDL_SCANCODE_W]) {
    // Move forward.
    double dest_x = player_x + player_move_e;
    double dest_y = player_y - player_move_n;
  }
  if (keyboard_state[SDL_SCANCODE_S]) {
    // Move back.
    double dest_x = player_x - player_move_e;
    double dest_y = player_y + player_move_n;
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

  // Sonar points straight ahead unless altered.
  sonar_facing = player_heading;
  if (keyboard_state[SDL_SCANCODE_J]) {
    // Point sonar left.
    sonar_facing = player_heading + 0.5 * PI;
  }
  if (keyboard_state[SDL_SCANCODE_K]) {
    // Point sonar back.
    sonar_facing = player_heading + 1.0 * PI;
  }
  if (keyboard_state[SDL_SCANCODE_L]) {
    // Point sonar right.
    sonar_facing = player_heading + 1.5 * PI;
  }
  // Keep facing in range [0, 2*PI).
  if (sonar_facing >= 2.0 * PI) {
    sonar_facing -= 2.0 * PI;
  }
  // TODO: Round facing to fixed points to prevent
  // cumulative rounding errors.
}

void play_sounds() {

  // Play sonar.
  void* sonar_audio_data = generate_beep(1000.0, FRAME_DUR_MS / 2);
  if (0 != SDL_QueueAudio(audio_device_id, sonar_audio_data, sonar_audio_len))
    error("Audio error");

  // Play footstep.
  if (player_moved) {
    // No footstep for now.
  }
}

void* generate_beep(double beep_frequency, int beep_duration_ms) {
  static double v = 0; // Not sure what this does. Is it the phase of the sin wave?
  static char audio_buffer[AUDIO_SAMPLES];
  memset(audio_buffer, 0, sizeof(audio_buffer));
  int n_samples = beep_duration_ms * FREQUENCY / 1000;
  for (int i=0; i < n_samples; i++) {
    buf[i] = AMPLITUDE * sin(v * 2 * M_PI / FREQUENCY);
    v += beep_frequency;
  }
  return audio_buffer;
}

void update_display() {
  // Draw the base of the gameboard.
  SDL_BlitSurface(screen, NULL, gameboard_image, NULL);
  // Draw the players.
  // TODO: Multiple players.
  SDL_Rect dest_rect;
  dest_rect.x = (int)player_x;
  dest_rect.y = (int)player_y;
  SDL_BlitSurface(screen, NULL, player_image, &dest_rect);
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

