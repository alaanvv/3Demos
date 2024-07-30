#include "canvas.h"

#define SCREEN_SIZE 1
#define FULLSCREEN 0
#define UPSCALE 0.2
#define SPEED 4 
#define FOV PI4
#define FAR 100
#define NEAR 0.1
#define SENSITIVITY 0.001
#define CAM_BASE_HEIGHT 1.7
#define CAMERA_LOCK PI4 * 0.99
#define HORIZONTAL_CAMERA_LOCK PI2 * 0.8

void handle_inputs(GLFWwindow*);

// ---

Camera cam = { FOV, NEAR, FAR, { 0, CAM_BASE_HEIGHT, 2.2 } };
vec3 mouse;
u32 shader;
f32 fps, tick = 0;

Material m_floor = { { 1.00, 1.00, 1.00 }, 0.5, 0.5, 0.6, 255, 2, 0, 1, 0, 0 };
Material m_walls = { { 1.00, 1.00, 1.00 }, 0.5, 0.5, 0.8, 255, 3, 0, 1, 0, 0 };
Material m_grids = { { 1.00, 1.00, 1.00 }, 0.5, 0.5, 0.6, 255, 4, 0, 1, 0, 0 };
Material m_hand  = { { 0.60, 0.60, 0.60 }, 1.0, 0.5, 0.0, 255, 5, 0, 1, 0, 1 };
Material m_body  = { { 1.00, 1.00, 1.00 }, 0.2, 0.0, 0.0, 255, 7, 0, 1, 0, 0 };
Material m_head  = { { 1.00, 1.00, 1.00 }, 0.2, 0.0, 0.0, 255, 8, 0, 1, 0, 0 };

PntLig light = { { 1, 1.0, 1.0 }, { 0, CAM_BASE_HEIGHT, 2.2 }, 1, 0.3, 0.9 };
PntLig fire  = { { 2, 0.25, 0.25 }, { 0, CAM_BASE_HEIGHT, 2.2 }, 1, 0.22, 0.20 };

i8 moving = 0;
Animation moving_anim = { 0, 1, 0 };

u8 lighter_active = 0;
Animation lighter_anim = { 0, 2, 0 };
Animation fire_anim = { 0, 1, 0 };

u8 devil_pos = 4;
vec3 devil_translate[5] = { { 0, 10, 0 }, { 1.5, 0, 3 }, { 3, 0, -2 }, { -3, 0, -2 }, { -2.8, 0, 0.8 } };
f32 devil_rotate[5] = { PI2 + PI4, PI2 + PI4, PI4, PI4 * 3, PI4 };

// ---

void main() {
  canvas_init(&cam, (CanvasInitConfig) { "Room", 1, FULLSCREEN, SCREEN_SIZE });

  u32 lowres_fbo = canvas_create_FBO(cam.width * UPSCALE, cam.height * UPSCALE, GL_NEAREST, GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  Model* hud   = model_create("obj/hud.obj",   1,    NULL);
  Model* walls = model_create("obj/walls.obj", 1e-2, &m_walls);
  Model* floor = model_create("obj/floor.obj", 1e-2, &m_floor);
  Model* grids = model_create("obj/grids.obj", 1e-2, &m_grids);
  Model* body  = model_create("obj/body.obj",  4e-3, &m_body);

  Model* head  = model_create("obj/head.obj",  4e-3, &m_head);

  canvas_create_texture(GL_TEXTURE0, "img/w.ppm",           TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE1, "img/b.ppm",           TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE2, "img/floor.ppm",       TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE3, "img/walls.ppm",       TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE4, "img/grids.ppm",       TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE5, "img/lighter-off.ppm", TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE6, "img/lighter-on.ppm",  TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE7, "img/body.ppm",        TEXTURE_DEFAULT);
  canvas_create_texture(GL_TEXTURE8, "img/head.ppm",        TEXTURE_DEFAULT);

  shader = shader_create_program("shd/obj.v", "shd/obj.f");

  canvas_set_pnt_lig(shader, light, 0);
  canvas_set_pnt_lig(shader, fire,  1);
  generate_proj_mat(&cam, shader);
  generate_view_mat(&cam, shader);
  canvas_uni3f(shader, "PNT_LIGS[1].COL", 0, 0, 0);

  while (!glfwWindowShouldClose(cam.window)) {
    update_fps(&fps, &tick);

    model_bind(walls, shader);
    model_draw(walls, shader);

    model_bind(floor, shader);
    model_draw(floor, shader);

    model_bind(grids, shader);
    model_draw(grids, shader);

    model_bind(body, shader);
    glm_translate(body->model, devil_translate[devil_pos]);
    glm_rotate(body->model, devil_rotate[devil_pos], (vec3) { 0, 1, 0 });
    model_draw(body, shader);

    model_bind(head, shader);
    glm_mat4_copy(body->model, head->model);
    model_draw(head, shader);

    if (lighter_active || lighter_anim.stage) {
      use_screen_space(&cam, shader, 1);
      canvas_set_material(shader, m_hand);
      if (lighter_active && !lighter_anim.stage) canvas_uni1i(shader, "MAT.S_DIF", 6);
      model_bind(hud, shader);

      glm_scale(hud->model, (vec3) { (f32) cam.height / cam.width * 1.5, 1.5, 0 });
      glm_translate(hud->model, (vec3) { ((f32) .5 * cam.height / cam.width), -0.75, 0 });

      if (lighter_anim.stage == 1)
        glm_translate(hud->model, (vec3) { lighter_active ? (1 - lighter_anim.pos) : lighter_anim.pos, lighter_active ? (-1 + lighter_anim.pos) : -lighter_anim.pos, 0 });
      if (lighter_anim.stage == 2) {
        if (lighter_active)
          glm_translate(hud->model, (vec3) { 0, -sin(lighter_anim.pos * PI) * 1e-1, 0 });
        else 
          lighter_anim.stage = 0;
      }

      if (moving_anim.stage) 
        glm_translate(hud->model, (vec3) { 0, -sin(moving_anim.pos * PI) * 5e-2, 0 });
     
      if (lighter_active || lighter_anim.stage)
        model_draw(hud, shader);

      use_screen_space(&cam, shader, 0);
      u8 ended = 0;
      if (lighter_anim.stage)
        ended = animation_run(&lighter_anim, 3 / fps);
      if (lighter_active && ended && !fire_anim.stage)
        animation_start(&fire_anim);
    }

    if (fire_anim.stage) {
      if (lighter_active) 
        canvas_uni3f(shader, "PNT_LIGS[1].COL", 0.2 + (fire.col[0] * fire_anim.pos * 0.8), fire.col[1] * fire_anim.pos, fire.col[2] * fire_anim.pos);
      else 
        canvas_uni3f(shader, "PNT_LIGS[1].COL", fire.col[0] * (1-fire_anim.pos), fire.col[1] * (1-fire_anim.pos), fire.col[2] * (1-fire_anim.pos));
      if (fire_anim.stage) animation_run(&fire_anim, 1 / fps);
    }

    if (moving || moving_anim.stage) {
      cam.pos[1] = CAM_BASE_HEIGHT + sin(moving_anim.pos * TAU) * 3e-2;
      generate_view_mat(&cam, shader);
      if (moving && !moving_anim.stage) animation_start(&moving_anim);
      animation_run(&moving_anim, (moving ? 3 : 10) / fps);
    }

    glBlitNamedFramebuffer(0, lowres_fbo, 0, 0, cam.width, cam.height, 0, 0, cam.width * UPSCALE, cam.height * UPSCALE, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitNamedFramebuffer(lowres_fbo, 0, 0, 0, cam.width * UPSCALE, cam.height * UPSCALE, 0, 0, cam.width, cam.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glfwPollEvents();
    handle_inputs(cam.window);
    glfwSwapBuffers(cam.window); 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }
  glfwTerminate();
}

void handle_inputs(GLFWwindow* window) {
  moving = (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS);

  if (moving) {
    glm_vec3_add(cam.pos, (vec3) { moving * SPEED / fps, 0, 0 },  cam.pos);
    cam.pos[0] = CLAMP(-2, cam.pos[0], 2);
    generate_view_mat(&cam, shader);
    canvas_uni3f(shader, "CAM", cam.pos[0], cam.pos[1], cam.pos[2]);
    canvas_uni3f(shader, "PNT_LIGS[0].POS", cam.pos[0], CAM_BASE_HEIGHT, 2.2);
    canvas_uni3f(shader, "PNT_LIGS[1].POS", cam.pos[0], CAM_BASE_HEIGHT, 2.2);
  }

  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, 1);
  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS && !lighter_anim.stage && !fire_anim.stage) {
    lighter_active = !lighter_active;
    animation_start(&lighter_anim);
    if (!lighter_active && !fire_anim.stage) {
      animation_start(&fire_anim);
      fire_anim.pos = 1 - fire_anim.pos;
      if (fire_anim.pos == 1) fire_anim.pos = 0;
    }
  }

  f64 pos[2];
  glfwGetCursorPos(window, &pos[0], &pos[1]);
  if (mouse[0] == 0) {
    mouse[0] = pos[0];
    mouse[1] = pos[1];
  }
  if (VEC2_COMPARE(pos, mouse)) return;

  cam.yaw   = CLAMP(-HORIZONTAL_CAMERA_LOCK, cam.yaw + (pos[0] - mouse[0]) * SENSITIVITY, HORIZONTAL_CAMERA_LOCK);
  cam.pitch = CLAMP(-CAMERA_LOCK, cam.pitch + (mouse[1] - pos[1]) * SENSITIVITY, CAMERA_LOCK);

  glm_vec3_copy((vec3) { cos(cam.yaw - PI2) * cos(cam.pitch), sin(cam.pitch), sin(cam.yaw - PI2) * cos(cam.pitch) }, cam.dir);
  glm_vec3_copy((vec3) { cos(cam.yaw) * cos(cam.pitch), 0, sin(cam.yaw) * cos(cam.pitch) }, cam.rig);
  glm_normalize(cam.rig);
  generate_view_mat(&cam, shader);

  mouse[0] = pos[0];
  mouse[1] = pos[1];
}
