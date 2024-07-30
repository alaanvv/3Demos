#include "canvas.h"
#include <unistd.h>
#include <time.h>

#define SCREEN_SIZE 1
#define FULLSCREEN 1
#define SPEED 5 
#define UPSCALE 0.1
#define SENSITIVITY 0.001
#define CAMERA_LOCK PI2 * 0.99
#define FOV PI4 * 0.7

#define LOADED_SCENARIOS 3
#define SCENARIO_SIZE 50

// ---

Camera cam = { FOV, 0.1, 100, { 0, 2, 0 } };
f32 fps, tick = 0;
vec3 mouse;
u32 shader;

Material m_street    = { { 1.00, 1.00, 1.00 }, 0.5, 0.5, 0.0, 255, 2,  0, 1, 0, 0 };
Material m_grass     = { { 1.00, 1.00, 1.00 }, 0.5, 0.5, 0.1, 255, 3,  0, 1, 0, 0 };
Material m_bush      = { { 1.00, 1.00, 1.00 }, 0.5, 0.5, 0.0, 255, 4,  0, 1, 0, 1 };
Material m_tree      = { { 1.00, 1.00, 1.00 }, 0.5, 0.5, 0.0, 255, 5,  0, 1, 0, 1 };
Material m_car       = { { 1.00, 1.00, 1.00 }, 0.5, 0.5, 0.5, 255, 6,  0, 1, 0, 1 };
Material m_inc_car_1 = { { 1.00, 1.00, 1.00 }, 0.5, 0.5, 0.5, 255, 7,  0, 1, 0, 1 };
Material m_inc_car_2 = { { 1.00, 1.00, 1.00 }, 0.5, 0.5, 0.5, 255, 8,  0, 1, 0, 1 };
Material m_inc_car_3 = { { 1.00, 1.00, 1.00 }, 0.5, 0.5, 0.5, 255, 9,  0, 1, 0, 1 };
Material m_inc_car_4 = { { 1.00, 1.00, 1.00 }, 0.5, 0.5, 0.5, 255, 10, 0, 1, 0, 1 };
Material m_inc_car_5 = { { 1.00, 1.00, 1.00 }, 0.5, 0.5, 0.5, 255, 11, 0, 1, 0, 1 };


Material m_track   = { { 0.90, 1.00, 0.35 }, 0.0, 0.0, 0.0, 000, 0, 0, 1, 1, 0 };
Material m_block_l = { { 1.00, 0.50, 0.35 }, 0.0, 0.0, 0.0, 000, 0, 0, 1, 1, 0 };
Material m_block_d = { { 0.35, 0.35, 0.35 }, 0.0, 0.0, 0.0, 000, 0, 0, 1, 1, 0 };

Material* ms_street[]  = { &m_street };
Material* ms_grass[]   = { &m_grass  };
Material* ms_bush[]    = { &m_bush   };
Material* ms_tree[]    = { &m_tree   };
Material* ms_car[]     = { &m_car    };
Material* ms_inc_car[] = { &m_inc_car_1, &m_inc_car_2, &m_inc_car_3, &m_inc_car_4, &m_inc_car_5 };

Material* ms_cube[]   = { &m_track, &m_block_l, &m_block_d };

PntLig light = { { 1, 1, 1 }, { 0, 2, 3 }, 1, 0.07, 0.017 };

// --- DRIVE ---

typedef struct {
  f32 spd, acc, x, max_spd, max_acc, d_spd, dir;
} Car;

Car p_car = { 0, 0, 0, 0.8, 0.3 };
f32 scenario_offset = 0;
f32 incoming_cars[2][4] = { 0 };

void init_car(u8 i) {
  incoming_cars[i][0] = (f32) RAND( 1000, 3000) / 1e3;
  incoming_cars[i][1] = (f32) RAND(-3000, 3000) / 1e3;
  incoming_cars[i][2] = SCENARIO_SIZE * (RAND(0, 2) + LOADED_SCENARIOS) + RAND(0, SCENARIO_SIZE * 3);
  incoming_cars[i][3] = RAND(0, 5);
}

void accelerate() {
  p_car.acc = MIN(p_car.acc + 1e-3, p_car.max_acc);
  if (p_car.acc < 0) p_car.acc += p_car.spd;
}

void decelerate(f32 force) {
  p_car.acc = MAX(-p_car.max_acc, p_car.acc - force);
  if (p_car.spd <= 0 && p_car.acc < 0) p_car.acc = 0;
}

void move_car() {
  p_car.d_spd = CLAMP(0, p_car.spd + p_car.acc / fps, p_car.max_spd) - p_car.spd;
  p_car.spd += p_car.d_spd;
  cam.fov = FOV + p_car.spd * 3e-1;
  generate_proj_mat(&cam, shader);
  scenario_offset += p_car.spd * cos(p_car.dir);

  if (scenario_offset <= SCENARIO_SIZE) return;
  scenario_offset = 0;
  for (u8 c = 0; c < 2; c++)
    incoming_cars[c][2] -= SCENARIO_SIZE;
}

void fix_car_dir() {
  p_car.dir -= 1e-1 * p_car.spd * p_car.dir;
  p_car.dir = CLAMP(-PI4 / 4, p_car.dir, PI4 / 4);
  p_car.x = CLAMP(-3, p_car.x + p_car.dir * p_car.spd, 3);
}

void turn_car_l() {
  if (p_car.x == 3) { fix_car_dir(); return; }
  if (p_car.x <  3) p_car.dir += 1e-2 * p_car.spd;
  p_car.dir = CLAMP(-PI4 / 4, p_car.dir, PI4 / 4);
  p_car.x = CLAMP(-3, p_car.x + p_car.dir * p_car.spd, 3);
}

void turn_car_r() {
  if (p_car.x == -3) { fix_car_dir(); return; }
  if (p_car.x > -3) p_car.dir -= 1e-2 * p_car.spd;
  p_car.dir = CLAMP(-PI4 / 4, p_car.dir, PI4 / 4);
  p_car.x = CLAMP(-3, p_car.x + p_car.dir * p_car.spd, 3);
}

// --- TETRIS ---

u8 game[10][10] = { 0 };
u8 dropping[4][4];
u8 piece_w, piece_h, dropping_x;

u8 pieces[7][5][4] = {
  { { 1, 1, 1, 1 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 4, 1 } },
  { { 1, 1, 0, 0 }, { 1, 1, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 2, 2 } },
  { { 1, 1, 1, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 3, 2 } },
  { { 0, 1, 0, 0 }, { 0, 1, 0, 0 }, { 1, 1, 0, 0 }, { 0, 0, 0, 0 }, { 2, 3 } },
  { { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 1, 0, 0 }, { 0, 0, 0, 0 }, { 2, 3 } },
  { { 0, 1, 1, 0 }, { 1, 1, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 3, 2 } },
  { { 1, 1, 0, 0 }, { 0, 1, 1, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 3, 2 } }
};

void randomize_piece() {
  u8 chosen = RAND(0, 7);
  u8 color  = RAND(1, 3);
  for (u8 x = 0; x < 4; x++)
    for (u8 y = 0; y < 4; y++)
      dropping[y][x] = pieces[chosen][y][x] * color;
  piece_w = pieces[chosen][4][0];
  piece_h = pieces[chosen][4][1];
  dropping_x = 2;
}

void check_row_breaking() {
  for (u8 y = 0; y < 10; y++)
    for (u8 x = 0; x < 10; x++)
      if (!game[y][x]) break;
      else if (x == 9)
        for (i8 _y = y; _y >= 0; _y--)
          for (u8 _x = 0; _x < 10; _x++)
            game[_y][_x] = _y == 0 ? 0 : game[_y - 1][_x];
}

void move_piece_l() {
  dropping_x = MAX(0, dropping_x - 1);
}

void move_piece_r() {
  dropping_x = MIN(dropping_x + 1, 10 - piece_w);
}

void rotate_piece() {
  u8 piece_size = MAX(piece_w, piece_h);
  for (u8 layer = 0; layer < piece_size / 2; ++layer)
    for (u8 i = layer; i < piece_size - layer - 1; ++i) {
      u8 temp = dropping[layer][i];
      dropping[layer][i] = dropping[piece_size - i - 1][layer];
      dropping[piece_size - i - 1][layer] = dropping[piece_size - layer - 1][piece_size - i - 1];
      dropping[piece_size - layer - 1][piece_size - i - 1] = dropping[i][piece_size - layer - 1];
      dropping[i][piece_size - layer - 1] = temp;
    }

  u8 temp = piece_w;
  piece_w = piece_h;
  piece_h = temp;
  dropping_x = MIN(dropping_x, 10 - piece_w);

  while (1) {
    for (u8 y = 0; y < piece_h; y++)
      if (dropping[y][0]) return;

    for (u8 y = 0; y < piece_h; y++)
      for (u8 x = 0; x < piece_size; x++)
        dropping[y][x] = (x + 1) >= piece_size ? 0 : dropping[y][x + 1];
  }
}

void drop_piece() {
  for (u8 depth = 0; depth < 12; depth++) 
    for (u8 px = 0; px < piece_w; px++) 
      for (u8 py = 0; py < piece_h; py++)
        if (dropping[py][px] && (game[MAX(0, depth - piece_h + py)][dropping_x + px] || depth == 11)) {
          for (u8 _x = 0; _x < piece_w; _x++)
            for (u8 _y = 0; _y < piece_h; _y++)
              if (dropping[_y][_x] && (depth - 1 - piece_h + _y) >= 0)
                game[depth - 1 - piece_h + _y][dropping_x + _x] = dropping[_y][_x];

          if (depth - piece_h <= 0) // Loose
            for (u8 y = 0; y < 10; y++)
              for (u8 x = 0; x < 10; x++)
                game[y][x] = 0;

          randomize_piece();
          check_row_breaking();
          return;
        }
}

// ---

void handle_keys(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods) {
  if (action != GLFW_PRESS)  return;
  if (key == GLFW_KEY_LEFT)  move_piece_l();
  if (key == GLFW_KEY_RIGHT) move_piece_r();
  if (key == GLFW_KEY_UP)    rotate_piece();
  if (key == GLFW_KEY_DOWN)  drop_piece();
}

void handle_inputs(GLFWwindow* window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, 1);

  if      (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) accelerate(); 
  else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) decelerate(1e-2); 
  else                                                   decelerate(1e-3);
  if      (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) turn_car_l();
  else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) turn_car_r();
  else                                                   fix_car_dir();
  move_car();
}

void main() {
  canvas_init(&cam, (CanvasInitConfig) { "Tetris Drive", 1, FULLSCREEN, SCREEN_SIZE });
  glfwSetKeyCallback(cam.window, handle_keys);
  srand(time(0));

  Model* street  = model_create("obj/street.obj", 1e-1, ms_street);
  Model* grass   = model_create("obj/grass.obj",  1e-1, ms_grass);
  Model* bushes  = model_create("obj/bushes.obj", 1e-1, ms_bush);
  Model* trees   = model_create("obj/trees.obj",  1e-1, ms_tree);
  Model* car     = model_create("obj/car.obj",    1,    ms_car);
  Model* cube    = model_create("obj/cube.obj",   1,    ms_cube);
  Model* inc_cars[5] = {
    model_create("obj/car-1.obj", 1, ms_inc_car),
    model_create("obj/car-2.obj", 1, ms_inc_car),
    model_create("obj/car-3.obj", 1, ms_inc_car),
    model_create("obj/car-4.obj", 1, ms_inc_car),
    model_create("obj/car-5.obj", 1, ms_inc_car)
  };

  u32 lowres_fbo = canvas_create_FBO(cam.width * UPSCALE, cam.height * UPSCALE, GL_NEAREST, GL_NEAREST);
  u32 drive_fbo  = canvas_create_FBO(cam.width, cam.height, GL_NEAREST, GL_NEAREST);
  u32 tetris_fbo = canvas_create_FBO(cam.width, cam.height, GL_NEAREST, GL_NEAREST);

  canvas_create_texture(GL_TEXTURE0,  "img/w.ppm",      TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE1,  "img/b.ppm",      TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE2,  "img/street.ppm", TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE3,  "img/grass.ppm",  TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE4,  "img/bush.ppm",   TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE5,  "img/tree.ppm",   TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE6,  "img/car.ppm",    TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE7,  "img/car-1.ppm",  TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE8,  "img/car-2.ppm",  TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE9,  "img/car-3.ppm",  TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE10, "img/car-4.ppm",  TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE11, "img/car-5.ppm",  TEXTURE_DEFAULT);

  shader = shader_create_program("shd/obj.v", "shd/obj.f");

  generate_proj_mat(&cam, shader);
  generate_view_mat(&cam, shader);

  canvas_set_pnt_lig(shader, light, 0);

  init_car(0);
  init_car(1);
  randomize_piece();

  while (!glfwWindowShouldClose(cam.window)) {
    update_fps(&fps, &tick);

    glBindFramebuffer(GL_FRAMEBUFFER, drive_fbo);
    for (u8 s = 0; s < LOADED_SCENARIOS; s++) {
      model_bind(street, shader, 1);
      glm_translate(street->model, (vec3) { p_car.x, 0, scenario_offset - (SCENARIO_SIZE * s) });
      model_draw(street, shader);

      model_bind(grass, shader, 1);
      glm_translate(grass->model, (vec3) { p_car.x, 0, scenario_offset - (SCENARIO_SIZE * s) });
      model_draw(grass, shader);

      model_bind(trees, shader, 1);
      glm_translate(trees->model, (vec3) { p_car.x, 0, scenario_offset - (SCENARIO_SIZE * s) });
      model_draw(trees, shader);

      model_bind(bushes, shader, 1);
      glm_translate(bushes->model, (vec3) { p_car.x, 0, scenario_offset - (SCENARIO_SIZE * s) });
      model_draw(bushes, shader);
    }

    for (u8 c = 0; c < 2; c++) {
      model_bind(inc_cars[(u8) incoming_cars[c][3]], shader, incoming_cars[c][3] + 1);
      glm_translate(inc_cars[(u8) incoming_cars[c][3]]->model, (vec3) { p_car.x + incoming_cars[c][1], 0, -incoming_cars[c][2] + scenario_offset });
      model_draw(inc_cars[(u8) incoming_cars[c][3]], shader);
      incoming_cars[c][2] -= incoming_cars[c][0] * 0.1;
      if (incoming_cars[c][2] <= 0) init_car(c);
    }

    model_bind(car, shader, 1);
    glm_rotate(car->model, PI + p_car.dir, (vec3) { 0, 1, 0 });
    glm_rotate(car->model, -p_car.d_spd * -(((f32)(cos(((p_car.spd + 0.5) / p_car.max_spd) * PI)))) * (p_car.acc >= 0 ? 10 : 0.5), (vec3) { 1, 0, 0 });
    glm_rotate(car->model, sin(scenario_offset) * (1 - p_car.spd / p_car.max_spd) * 1e-2, (vec3) { 0, 0, 1 });
    model_draw(car, shader);

    glBindFramebuffer(GL_FRAMEBUFFER, tetris_fbo);
    use_screen_space(&cam, shader, 1);

    for (u8 x = dropping_x; x < dropping_x + piece_w; x++) {
      model_bind(cube, shader, 1);
      glm_scale(cube->model, (vec3) { 0.2, cam.width * 0.4 / cam.height * 0.2 * 20 });
      glm_translate(cube->model, (vec3) { -5 + x, -0.5 });
      model_draw(cube, shader);
    }

    for (u8 x = 0; x < piece_w; x++)
      for (u8 y = 0; y < piece_h; y++) {
        if (!dropping[y][x]) continue;
        model_bind(cube, shader, 1 + dropping[y][x]);
        glm_scale(cube->model, (vec3) { 0.2, cam.width * 0.4 / cam.height * 0.2 });
        glm_translate(cube->model, (vec3) { x + dropping_x - 5, piece_h - y + 3 });
        model_draw(cube, shader);
      }

    for (u8 x = 0; x < 10; x++) 
      for (u8 y = 0; y < 10; y++) {
        if (!game[y][x]) continue;
        model_bind(cube, shader, 1 + game[y][x]);
        glm_scale(cube->model, (vec3) { 0.2, cam.width * 0.4 / cam.height * 0.2, 0 });
        glm_translate(cube->model, (vec3) { x - 5, 2 - y, 0 });
        model_draw(cube, shader);
      }

    use_screen_space(&cam, shader, 0);

    glBlitNamedFramebuffer(drive_fbo, lowres_fbo, 0, 0, cam.width * 0.6, cam.height, 0, 0, cam.width * 0.6 * UPSCALE, cam.height * UPSCALE, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitNamedFramebuffer(lowres_fbo, drive_fbo, 0, 0, cam.width * 0.6 * UPSCALE, cam.height * UPSCALE, 0, 0, cam.width * 0.6, cam.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBlitNamedFramebuffer(drive_fbo,  0, 0, 0, cam.width, cam.height, 0, 0, cam.width * 0.6, cam.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitNamedFramebuffer(tetris_fbo, 0, 0, 0, cam.width, cam.height, cam.width * 0.6, 0, cam.width, cam.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glfwPollEvents();
    handle_inputs(cam.window);
    glfwSwapBuffers(cam.window); 

    glClearColor(0.05, 0.05, 0.08, 1);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, drive_fbo);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, tetris_fbo);
    glClearColor(1.00, 0.85, 0.35, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }
  glfwTerminate();
}
