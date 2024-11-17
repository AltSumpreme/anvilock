#ifndef EGL_H
#define EGL_H

#include "client_state.h"
#include "config.h"
#include "log.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static void render_password_field(struct client_state* state);

static const GLfloat quad_vertices[] = {
  -1.0f, 1.0f,  // Top left
  -1.0f, -1.0f, // Bottom left
  1.0f,  1.0f,  // Top right
  1.0f,  -1.0f  // Bottom right
};

static const GLfloat triangle_vertices[] = {
  0.0f,  0.5f,  // Top vertex
  -0.5f, -0.5f, // Bottom left vertex
  0.5f,  -0.5f  // Bottom right vertex
};

static const GLfloat tex_coords[] = {
  0.0f, 0.0f, // Top left
  0.0f, 1.0f, // Bottom left
  1.0f, 0.0f, // Top right
  1.0f, 1.0f  // Bottom right
};

// Add these new vertex arrays for the password field
static const GLfloat password_field_vertices[] = {
  // Main rectangle (x, y coordinates for two triangles forming a rectangle)
  -0.4f, 0.1f,  // Top left
  -0.4f, -0.1f, // Bottom left
  0.4f,  0.1f,  // Top right
  0.4f,  -0.1f  // Bottom right
};

// Define vertices for a single dot (small square)
static const GLfloat dot_vertices[] = {
  -0.015f, 0.015f,  // Top left
  -0.015f, -0.015f, // Bottom left
  0.015f,  0.015f,  // Top right
  0.015f,  -0.015f  // Bottom right
};

static GLuint load_texture(const char* filepath)
{
  int            width, height, channels;
  unsigned char* image = stbi_load(filepath, &width, &height, &channels, STBI_rgb_alpha);
  if (!image)
  {
    log_message(LOG_LEVEL_ERROR, "Failed to load image: %s", filepath);
    exit(EXIT_FAILURE);
  }

  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  stbi_image_free(image);
  return texture;
}

static void init_egl(struct client_state* state)
{
  // Get the EGL display connection using Wayland's display
  state->egl_display = eglGetDisplay((EGLNativeDisplayType)state->wl_display);
  if (state->egl_display == EGL_NO_DISPLAY)
  {
    log_message(LOG_LEVEL_ERROR, "Failed to get EGL display\n");
    exit(EXIT_FAILURE);
  }

  // Initialize the EGL display
  if (!eglInitialize(state->egl_display, NULL, NULL))
  {
    log_message(LOG_LEVEL_ERROR, "Failed to initialize EGL");
    exit(EXIT_FAILURE);
  }

  // Bind the OpenGL ES API
  if (!eglBindAPI(EGL_OPENGL_ES_API))
  {
    log_message(LOG_LEVEL_ERROR, "Failed to bind OpenGL ES API");
    exit(EXIT_FAILURE);
  }

  // EGL configuration: specifies rendering type and color depth
  EGLint    attribs[] = {EGL_RENDERABLE_TYPE,
                         EGL_OPENGL_ES2_BIT,
                         EGL_SURFACE_TYPE,
                         EGL_WINDOW_BIT,
                         EGL_RED_SIZE,
                         8,
                         EGL_GREEN_SIZE,
                         8,
                         EGL_BLUE_SIZE,
                         8,
                         EGL_NONE};
  EGLConfig config;
  EGLint    num_configs;
  if (!eglChooseConfig(state->egl_display, attribs, &config, 1, &num_configs) || num_configs < 1)
  {
    log_message(LOG_LEVEL_ERROR, "Failed to choose EGL config");
    exit(EXIT_FAILURE);
  }

  // Create an EGL context for OpenGL ES 2.0
  EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  state->egl_context =
    eglCreateContext(state->egl_display, config, EGL_NO_CONTEXT, context_attribs);
  if (state->egl_context == EGL_NO_CONTEXT)
  {
    log_message(LOG_LEVEL_ERROR, "Failed to create EGL context");
    exit(EXIT_FAILURE);
  }

  // Ensure Wayland surface events are processed before creating EGL window
  wl_display_roundtrip(state->wl_display);

  // Validate output dimensions and create EGL window surface
  int width         = state->output_state.width > 0 ? state->output_state.width : 1920;
  int height        = state->output_state.height > 0 ? state->output_state.height : 1080;
  state->egl_window = wl_egl_window_create(state->wl_surface, width, height);
  if (!state->egl_window)
  {
    log_message(LOG_LEVEL_ERROR, "Failed to create wl_egl_window");
    exit(EXIT_FAILURE);
  }

  state->egl_surface = eglCreateWindowSurface(state->egl_display, config,
                                              (EGLNativeWindowType)state->egl_window, NULL);
  if (state->egl_surface == EGL_NO_SURFACE)
  {
    EGLint error = eglGetError();
    log_message(LOG_LEVEL_ERROR, "Failed to create EGL surface, error code: %x", error);
    exit(EXIT_FAILURE);
  }

  // Make the EGL context current
  if (!eglMakeCurrent(state->egl_display, state->egl_surface, state->egl_surface,
                      state->egl_context))
  {
    log_message(LOG_LEVEL_ERROR, "Failed to make EGL context current");
    exit(EXIT_FAILURE);
  }

  // Set the OpenGL viewport to match the window size
  glViewport(0, 0, width, height);

  // Load the image and create a texture
  GLuint texture = load_texture(state->user_configs.background_path); // Use the bg path here

  // Use the texture for rendering
  glBindTexture(GL_TEXTURE_2D, texture);

  // Clear color buffer
  glClear(GL_COLOR_BUFFER_BIT);

  // Render the quad with the texture
  const char* vertex_shader_source = "attribute vec2 position;\n"
                                     "attribute vec2 texCoord;\n"
                                     "varying vec2 vTexCoord;\n"
                                     "void main() {\n"
                                     "    vTexCoord = texCoord;\n"
                                     "    gl_Position = vec4(position, 0.0, 1.0);\n"
                                     "}\n";
  GLuint      vertex_shader        = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
  glCompileShader(vertex_shader);

  // Check for vertex shader compile errors
  GLint compile_status;
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compile_status);
  if (compile_status == GL_FALSE)
  {
    log_message(LOG_LEVEL_ERROR, "Vertex shader compilation failed");
    exit(EXIT_FAILURE);
  }

  const char* fragment_shader_source = "precision mediump float;\n"
                                       "varying vec2 vTexCoord;\n"
                                       "uniform sampler2D uTexture;\n"
                                       "void main() {\n"
                                       "    gl_FragColor = texture2D(uTexture, vTexCoord);\n"
                                       "}\n";
  GLuint      fragment_shader        = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
  glCompileShader(fragment_shader);

  // Check for fragment shader compile errors
  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compile_status);
  if (compile_status == GL_FALSE)
  {
    log_message(LOG_LEVEL_ERROR, "Fragment shader compilation failed");
    exit(EXIT_FAILURE);
  }

  GLuint shader_program = glCreateProgram();
  glAttachShader(shader_program, vertex_shader);
  glAttachShader(shader_program, fragment_shader);
  glLinkProgram(shader_program);

  GLint link_status;
  glGetProgramiv(shader_program, GL_LINK_STATUS, &link_status);
  if (link_status == GL_FALSE)
  {
    log_message(LOG_LEVEL_ERROR, "Shader program linking failed");
    exit(EXIT_FAILURE);
  }

  glUseProgram(shader_program);

  GLint position_location = glGetAttribLocation(shader_program, "position");
  GLint texCoord_location = glGetAttribLocation(shader_program, "texCoord");
  glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, 0, quad_vertices);
  glEnableVertexAttribArray(position_location);
  glVertexAttribPointer(texCoord_location, 2, GL_FLOAT, GL_FALSE, 0, tex_coords);
  glEnableVertexAttribArray(texCoord_location);

  glUniform1i(glGetUniformLocation(shader_program, "uTexture"), 0);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  render_password_field(state);

  eglSwapBuffers(state->egl_display, state->egl_surface);
}

static void render_password_field(struct client_state* state)
{
  const char* vertex_shader_source = "attribute vec2 position;\n"
                                     "uniform vec2 offset;\n"
                                     "void main() {\n"
                                     "    vec2 pos = position + offset;\n"
                                     "    gl_Position = vec4(pos, 0.0, 1.0);\n"
                                     "}\n";

  const char* fragment_shader_source = "precision mediump float;\n"
                                       "uniform vec4 color;\n"
                                       "void main() {\n"
                                       "    gl_FragColor = color;\n"
                                       "}\n";

  // Create and compile shaders
  GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
  glCompileShader(vertex_shader);

  GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
  glCompileShader(fragment_shader);

  // Create and link program
  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);

  // Enable blending for transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Use the shader program
  glUseProgram(program);

  // Get uniform locations
  GLint color_location    = glGetUniformLocation(program, "color");
  GLint offset_location   = glGetUniformLocation(program, "offset");
  GLint position_location = glGetAttribLocation(program, "position");

  // Draw main field background with rounded corners (using GL_TRIANGLE_STRIP)
  glUniform4f(color_location, 1.0f, 1.0f, 1.0f, 0.95f); // light background with transparency
  glUniform2f(offset_location, 0.0f, 0.0f);

  glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, 0, password_field_vertices);
  glEnableVertexAttribArray(position_location);

  // Draw the background of the password field
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  // Draw the border with a subtle shadow effect
  glUniform4f(color_location, 0.8f, 0.8f, 0.8f, 1.0f);
  glDrawArrays(GL_LINE_LOOP, 0, 4);

  // Draw password dots with a nicer visual
  glUniform4f(color_location, 0.3f, 0.3f, 0.3f, 1.0f); // gray dots

  // Set up vertices for dots
  glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, 0, dot_vertices);

  // Adjust dot positions based on password input
  float field_width = 0.7f; // Width of the password field (can be adjusted)
  float dot_spacing = field_width / (state->pam.password_index + 1);
  for (int i = 0; i < state->pam.password_index; i++)
  {
    float x_position = -0.35f + (i + 1) * dot_spacing; // Position the dots dynamically
    glUniform2f(offset_location, x_position, 0.0f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  // Handle Authentication Failure (Red border for failure)
  if (state->pam.auth_state.auth_failed)
  {
    float failColor[] = {1.0f, 0.0f, 0.0f, 1.0f}; // Red for failure

    // Add a pulsing effect (optional, can be enhanced with animations)
    glUniform4fv(color_location, 1, failColor);
    glUniform2f(offset_location, 0.0f, 0.0f);
    glDrawArrays(GL_LINE_LOOP, 0, 4); // Re-draw border with failure color

    // Optionally add a blinking effect here by alternating between red and white for a short
    // period.
  }

  // Handle Authentication Success (Green border for success)
  if (!state->pam.auth_state.auth_failed && state->pam.password_index > 0)
  {
    // Animation or color change to indicate success (e.g., green border)
    float successColor[] = {0.0f, 1.0f, 0.0f, 1.0f}; // Green for success

    // Animate the border to show success
    glUniform4fv(color_location, 1, successColor);
    glUniform2f(offset_location, 0.0f, 0.0f);
    glDrawArrays(GL_LINE_LOOP, 0, 4); // Re-draw border with success color
  }

  // Disable blending and clean up
  glDisable(GL_BLEND);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glDeleteProgram(program);

  // Swap buffers to render the final frame
  eglSwapBuffers(state->egl_display, state->egl_surface);

  if (state->pam.auth_state.auth_failed)
    sleep(1);
}

/*static void render_triangle(struct client_state* state)*/
/*{*/
/*  // Do NOT clear the screen here to preserve the background*/
/*  // Remove any calls to glClear() in this function*/
/**/
/*  // Render the triangle on top of the background*/
/*  const char* vertex_shader_source = "attribute vec2 position;\n"*/
/*                                     "void main() {\n"*/
/*                                     "    gl_Position = vec4(position, 0.0, 1.0);\n"*/
/*                                     "}\n";*/
/*  GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);*/
/*  glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);*/
/*  glCompileShader(vertex_shader);*/
/**/
/*  GLint compile_status;*/
/*  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compile_status);*/
/*  if (compile_status == GL_FALSE)*/
/*  {*/
/*    log_message(LOG_LEVEL_ERROR, "Vertex shader compilation failed");*/
/*    exit(EXIT_FAILURE);*/
/*  }*/
/**/
/*  GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);*/
/*  if (state->pam.password_index != 0 && !state->pam.authenticated)*/
/*  {*/
/*    const char* fragment_shader_source_1 =*/
/*      "void main() {\n"*/
/*      "    gl_FragColor = vec4(1.0, 1.0, 0.0, 1.0);\n" // Yellow for input*/
/*      "}\n";*/
/*    glShaderSource(fragment_shader, 1, &fragment_shader_source_1, NULL);*/
/*  }*/
/*  else if (state->pam.authenticated)*/
/*  {*/
/*    const char* fragment_shader_source_1 =*/
/*      "void main() {\n"*/
/*      "    gl_FragColor = vec4(0.0, 1.0, 1.0, 1.0);\n" // Cyan for success*/
/*      "}\n";*/
/*    glShaderSource(fragment_shader, 1, &fragment_shader_source_1, NULL);*/
/*  }*/
/*  else*/
/*  {*/
/*    const char* fragment_shader_source_2 =*/
/*      "void main() {\n"*/
/*      "    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n" // Red for error*/
/*      "}\n";*/
/*    glShaderSource(fragment_shader, 1, &fragment_shader_source_2, NULL);*/
/*  }*/
/*  glCompileShader(fragment_shader);*/
/**/
/*  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compile_status);*/
/*  if (compile_status == GL_FALSE)*/
/*  {*/
/*    log_message(LOG_LEVEL_ERROR, "Fragment shader compilation failed");*/
/*    exit(EXIT_FAILURE);*/
/*  }*/
/**/
/*  GLuint shader_program = glCreateProgram();*/
/*  glAttachShader(shader_program, vertex_shader);*/
/*  glAttachShader(shader_program, fragment_shader);*/
/*  glLinkProgram(shader_program);*/
/**/
/*  glGetProgramiv(shader_program, GL_LINK_STATUS, &compile_status);*/
/*  if (compile_status == GL_FALSE)*/
/*  {*/
/*    log_message(LOG_LEVEL_ERROR, "Shader program linking failed");*/
/*    exit(EXIT_FAILURE);*/
/*  }*/
/**/
/*  glUseProgram(shader_program);*/
/**/
/*  GLint position_location = glGetAttribLocation(shader_program, "position");*/
/*  glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, 0, triangle_vertices);*/
/*  glEnableVertexAttribArray(position_location);*/
/**/
/*  glDrawArrays(GL_TRIANGLES, 0, 3);*/
/**/
/*  eglSwapBuffers(state->egl_display, state->egl_surface);*/
/*}*/

static GLuint create_texture_shader_program()
{
  const char* vertex_shader_source = "attribute vec2 position;\n"
                                     "attribute vec2 texCoord;\n"
                                     "varying vec2 vTexCoord;\n"
                                     "void main() {\n"
                                     "    vTexCoord = texCoord;\n"
                                     "    gl_Position = vec4(position, 0.0, 1.0);\n"
                                     "}\n";

  const char* fragment_shader_source = "precision mediump float;\n"
                                       "varying vec2 vTexCoord;\n"
                                       "uniform sampler2D uTexture;\n"
                                       "void main() {\n"
                                       "    gl_FragColor = texture2D(uTexture, vTexCoord);\n"
                                       "}\n";

  // Create and compile vertex shader
  GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
  glCompileShader(vertex_shader);

  // Create and compile fragment shader
  GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
  glCompileShader(fragment_shader);

  // Create and link shader program
  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);

  // Clean up shaders
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  return program;
}

void render_lock_screen(struct client_state* state)
{
  // Bind the context to ensure it's current
  if (!eglMakeCurrent(state->egl_display, state->egl_surface, state->egl_surface,
                      state->egl_context))
  {
    log_message(LOG_LEVEL_ERROR, "Failed to make EGL context current for rendering");
    return;
  }

  // Initialize static resources on first run
  static GLuint texture                = 0;
  static GLuint texture_shader_program = 0;
  static int    initialized            = 0;

  if (!initialized)
  {
    // Load the background image path from the TOML configuration
    log_message(LOG_LEVEL_WARN, "EGL not initialized.");
    texture = load_texture(state->user_configs.background_path); // Use the bg path here
    // Create shader program for texture
    texture_shader_program = create_texture_shader_program();
    initialized            = 1;
  }

  // Clear the screen
  glClear(GL_COLOR_BUFFER_BIT);

  // First render the texture
  glUseProgram(texture_shader_program);

  // Set up texture vertices and coordinates
  GLint position_loc = glGetAttribLocation(texture_shader_program, "position");
  GLint texcoord_loc = glGetAttribLocation(texture_shader_program, "texCoord");

  glVertexAttribPointer(position_loc, 2, GL_FLOAT, GL_FALSE, 0, quad_vertices);
  glEnableVertexAttribArray(position_loc);

  glVertexAttribPointer(texcoord_loc, 2, GL_FLOAT, GL_FALSE, 0, tex_coords);
  glEnableVertexAttribArray(texcoord_loc);

  // Bind and render texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glUniform1i(glGetUniformLocation(texture_shader_program, "uTexture"), 0);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  // Then render the triangle
  render_password_field(state);
}

#endif
