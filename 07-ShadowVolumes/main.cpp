/*
 * Source code for the NPGR019 lab practices. Copyright Martin Kahoun 2021.
 * Licensed under the zlib license, see LICENSE.txt in the root directory.
 */

#include <cstdio>
#include <vector>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include <MathSupport.h>
#include <Camera.h>

#include "shaders.h"
#include "scene.h"

#include <string>
#include <iostream>
using namespace std;

// Set to 1 to create debugging context that reports errors, requires OpenGL 4.3!
#define _ENABLE_OPENGL_DEBUG 0

// ----------------------------------------------------------------------------
// GLM optional parameters:
// GLM_FORCE_LEFT_HANDED       - use the left handed coordinate system
// GLM_FORCE_XYZW_ONLY         - simplify vector types and use x, y, z, w only
// ----------------------------------------------------------------------------
// For remapping depth to [0, 1] interval use GLM option below with glClipControl
// glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // requires version >= 4.5
//
// GLM_FORCE_DEPTH_ZERO_TO_ONE - force projection depth mapping to [0, 1]
//                               must use glClipControl(), requires OpenGL 4.5
//
// More information about the matter here:
// https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_clip_control.txt
// ----------------------------------------------------------------------------

// Structure for holding window parameters
struct Window
{
  // Window default width
  static const int DefaultWidth = 800;
  // Window default height
  static const int DefaultHeight = 600;

  // Width in pixels
  int width;
  // Height in pixels
  int height;
  // Main window handle
  GLFWwindow *handle;
} mainWindow = {0};

// ----------------------------------------------------------------------------

// Near clip plane settings
float nearClipPlane = 0.1f;
// Far clip plane settings
float farClipPlane = 1000.1f;
// Camera FOV
float fov = 45.0f;

const int SHADOW_SIZE = 1024;

// ----------------------------------------------------------------------------

// Mouse movement
struct MouseStatus
{
  // Current position
  double x, y;
  // Previous position
  double prevX, prevY;

  // Updates the status - called once per frame
  void Update(double &moveX, double &moveY)
  {
    moveX = x - prevX;
    prevX = x;
    moveY = y - prevY;
    prevY = y;
  }
} mouseStatus = {0.0};

// ----------------------------------------------------------------------------

// Camera movement speeds
static constexpr float CameraNormalSpeed = 5.0f;
static constexpr float CameraTurboSpeed = 50.0f;

// ----------------------------------------------------------------------------

// Max buffer length
static const unsigned int MAX_TEXT_LENGTH = 256;
// MSAA samples (turned off, as was causing a lot of headache)
static const GLsizei MSAA_SAMPLES = 1;

// Camera instance
Camera camera;
// Scene helper instance
Scene &scene(Scene::GetInstance());
// Render modes
RenderMode renderMode = {true, false, true, MSAA_SAMPLES};
// Enable/disable light movement
bool animate = false;
// Enable/disable Carcmack's reverse
bool carmackReverse = true;

// Our framebuffer object
GLuint fboRender = 0;
// Our render target for rendering the final image
GLuint renderTarget = 0;
GLuint depthRenderMap = 0;

GLuint fboCubeMapDepth = 0;
GLuint fboDepth = 0;

GLuint depthRenderTarget = 0;
// The depth map texture to be used in spotlight shadow maps
GLuint depthMap = 0;
// The depth cube map texture to be used in pointlight shadow maps
GLuint depthCubemap = 0;

// ----------------------------------------------------------------------------

// Forward declaration for the framebuffer creation
void createFramebuffer(int width, int height, GLsizei MSAA);
void createDepthFramebuffer(int width, int height);
void createCubeMapDepthFramebuffer(int width, int height);

// Callback for handling GLFW errors
void errorCallback(int error, const char* description)
{
  printf("GLFW Error %i: %s\n", error, description);
}

#if _ENABLE_OPENGL_DEBUG
void APIENTRY debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
  if (type == GL_DEBUG_TYPE_ERROR)
    printf("OpenGL error: %s\n", message);
}
#endif

// Callback for handling window resize events
void resizeCallback(GLFWwindow* window, int width, int height)
{
  mainWindow.width = width;
  mainWindow.height = height;
  glViewport(0, 0, width, height);
  camera.SetProjection(fov, (float)width / (float)height, nearClipPlane, farClipPlane);

  createFramebuffer(width, height, renderMode.msaaLevel);
}

// Callback for handling mouse movement over the window - called when mouse movement is detected
void mouseMoveCallback(GLFWwindow* window, double x, double y)
{
  // Update the current position
  mouseStatus.x = x;
  mouseStatus.y = y;
}

// Keyboard callback for handling system switches
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  // Notify the window that user wants to exit the application
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  // Enable/disable MSAA - note that it still uses the MSAA buffer
  if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
  {
    renderMode.msaaLevel = 1;
   
    createFramebuffer(mainWindow.width, mainWindow.height, renderMode.msaaLevel);
  }

  // Enable/disable wireframe rendering
  if (key == GLFW_KEY_F2 && action == GLFW_PRESS)
  {
    renderMode.wireframe = !renderMode.wireframe;
  }

  // Enable/disable vsync
  if (key == GLFW_KEY_F3 && action == GLFW_PRESS)
  {
    renderMode.vsync = !renderMode.vsync;
    if (renderMode.vsync)
      glfwSwapInterval(1);
    else
      glfwSwapInterval(0);
  }

  // Enable/disable tonemapping
  if (key == GLFW_KEY_F4 && action == GLFW_PRESS)
  {
    renderMode.tonemapping = !renderMode.tonemapping;
  }

  // Enable/disable light movement
  if (key == GLFW_KEY_F5 && action == GLFW_PRESS)
  {
    animate = !animate;
  }

  // Enable/disable Carmack's reverse
  if (key == GLFW_KEY_F6 && action == GLFW_PRESS)
  {
    carmackReverse = !carmackReverse;
  }

  // Zoom in
  if (key == GLFW_KEY_KP_ADD || key == GLFW_KEY_EQUAL && action == GLFW_PRESS)
  {
    fov -= 1.0f;
    if (fov < 5.0f)
      fov = 5.0f;
  }

  // Zoom out
  if (key == GLFW_KEY_KP_SUBTRACT || key == GLFW_KEY_MINUS && action == GLFW_PRESS)
  {
    fov += 1.0f;
    if (fov >= 180.0f)
      fov = 179.0f;
  }

  if (key == GLFW_KEY_BACKSPACE && action == GLFW_PRESS)
  {
    fov = 45.0f;
  }

  // Set the camera projection
  camera.SetProjection(fov, (float)mainWindow.width / (float)mainWindow.height, nearClipPlane, farClipPlane);
}

// ----------------------------------------------------------------------------

// Helper method for OpenGL initialization
bool initOpenGL()
{
  // Set the GLFW error callback
  glfwSetErrorCallback(errorCallback);

  // Initialize the GLFW library
  if (!glfwInit()) return false;

  // Request OpenGL 3.3 core profile upon window creation
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_SAMPLES, 0); // Disable MSAA, we'll handle it ourselves
#if _ENABLE_OPENGL_DEBUG
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // Create the window
  mainWindow.handle = glfwCreateWindow(Window::DefaultWidth, Window::DefaultHeight, "", nullptr, nullptr);
  if (mainWindow.handle == nullptr)
  {
    printf("Failed to create the GLFW window!");
    return false;
  }

  // Make the created window with OpenGL context current for this thread
  glfwMakeContextCurrent(mainWindow.handle);

  // Check that GLAD .dll loader and symbol imported is ready
  if (!gladLoadGL(glfwGetProcAddress)) {
    printf("GLAD failed!\n");
    return false;
  }

#if _ENABLE_OPENGL_DEBUG
  // Enable error handling callback function - context must be created with DEBUG flags
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageCallback(debugCallback, nullptr);
  GLuint unusedIds = 0;
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, &unusedIds, true);
#endif

  // Enable vsync
  if (renderMode.vsync)
    glfwSwapInterval(1);
  else
    glfwSwapInterval(0);

  // Enable automatic sRGB color conversion
  glEnable(GL_FRAMEBUFFER_SRGB);

  // Register a window resize callback
  glfwSetFramebufferSizeCallback(mainWindow.handle, resizeCallback);

  // Register keyboard callback
  glfwSetKeyCallback(mainWindow.handle, keyCallback);

  // Register mouse movement callback
  glfwSetCursorPosCallback(mainWindow.handle, mouseMoveCallback);

  // Set the OpenGL viewport and camera projection
  resizeCallback(mainWindow.handle, Window::DefaultWidth, Window::DefaultHeight);

  // Set the initial camera position and orientation
  camera.SetTransformation(glm::vec3(-3.0f, 3.0f, -5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

  return true;
}

// Helper function for creating the HDR framebuffer
void createDepthFramebuffer(int width, int height)
{
    // Bind the default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Generate the FBO if necessary
    if (!fboDepth)
    {
        glGenFramebuffers(1, &fboDepth);
    }

    // Bind it and recreate textures
    glBindFramebuffer(GL_FRAMEBUFFER, fboDepth);
    
    // --------------------------------------------------------------------------
    // Depth render target texture (single light):
    // --------------------------------------------------------------------------

    if (glIsTexture(depthMap))
    {
        glDeleteTextures(1, &depthMap);
        depthMap = 0;
    }

    // Create texture name
    if (depthMap == 0)
    {
        glGenTextures(1, &depthMap);
    }

    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    // Attach the texture to framebuffer
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthMap, 0);

    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    // Check for completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        printf("Failed to create cubemap framebuffer: 0x%04X\n", status);
    }

    // Bind back the window system provided framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Helper function for creating the HDR framebuffer
void createCubeMapDepthFramebuffer(int width, int height)
{
    // Bind the default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Generate the FBO if necessary
    if (!fboCubeMapDepth)
    {
        glGenFramebuffers(1, &fboCubeMapDepth);
    }

    // Bind it and recreate textures
    glBindFramebuffer(GL_FRAMEBUFFER, fboCubeMapDepth);

    // --------------------------------------------------------------------------
    // Depth cubemap
    // --------------------------------------------------------------------------

    if (glIsTexture(depthCubemap))
    {
        glDeleteTextures(1, &depthCubemap);
        depthCubemap = 0;
    }

    // Create texture name
    if (depthCubemap == 0)
    {
        glGenTextures(1, &depthCubemap);
    }

    // Bind and recreate the render target texture
    {
        glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
        for (unsigned int i = 0; i < 6; ++i)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // attach depth texture as FBO's depth buffer
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap, 0);
    }
    
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    // Check for completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        printf("Failed to create cubemap framebuffer: 0x%04X\n", status);
    }

    // Bind back the window system provided framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Helper function for creating the HDR framebuffer
void createFramebuffer(int width, int height, GLsizei MSAA)
{
    // Bind the default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Generate the FBO if necessary
    if (!fboRender)
    {
        glGenFramebuffers(1, &fboRender);
    }

    // Bind it and recreate textures
    glBindFramebuffer(GL_FRAMEBUFFER, fboRender);

    // --------------------------------------------------------------------------
    // Render target texture:
    // --------------------------------------------------------------------------

    // Delete it if necessary
    if (glIsTexture(renderTarget))
    {
        glDeleteTextures(1, &renderTarget);
        renderTarget = 0;
    }

    // Create the texture name
    if (renderTarget == 0)
    {
        glGenTextures(1, &renderTarget);
    }

    // Bind and recreate the render target texture
    if (MSAA > 1)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, renderTarget);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA, GL_RGB16F, width, height, GL_TRUE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, renderTarget, 0);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, renderTarget);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderTarget, 0);
    }

    // --------------------------------------------------------------------------
    // Depth render target texture (render):
    // --------------------------------------------------------------------------

    if (glIsTexture(depthRenderMap))
    {
        glDeleteTextures(1, &depthRenderMap);
        depthRenderMap = 0;
    }

    // Create texture name
    if (depthRenderMap == 0)
    {
        glGenTextures(1, &depthRenderMap);
    }

    glBindTexture(GL_TEXTURE_2D, depthRenderMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    // Attach the texture to framebuffer
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthRenderMap, 0);
  
    // Set the list of draw buffers.
    GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, drawBuffers);
  
    // Check for completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        printf("Failed to create framebuffer: 0x%04X\n", status);
    }

    // Bind back the window system provided framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Helper method for graceful shutdown
void shutDown()
{
  // Release shader programs
  for (int i = 0; i < ShaderProgram::NumShaderPrograms; ++i)
  {
    glDeleteProgram(shaderProgram[i]);
  }

  // Release the textures
  glDeleteTextures(1, &renderTarget);
  glDeleteTextures(1, &depthMap);
  glDeleteTextures(1, &depthCubemap);
  
  // Release the framebuffer
  glDeleteFramebuffers(1, &fboRender);
  glDeleteFramebuffers(1, &fboDepth);
  glDeleteFramebuffers(1, &fboCubeMapDepth);

  // Release the window
  glfwDestroyWindow(mainWindow.handle);

  // Close the GLFW library
  glfwTerminate();
}

// ----------------------------------------------------------------------------

// Helper method for handling input events
void processInput(float dt)
{
  // Camera movement - keyboard events
  int direction = (int)MovementDirections::None;
  if (glfwGetKey(mainWindow.handle, GLFW_KEY_W) == GLFW_PRESS)
    direction |= (int)MovementDirections::Forward;

  if (glfwGetKey(mainWindow.handle, GLFW_KEY_S) == GLFW_PRESS)
    direction |= (int)MovementDirections::Backward;

  if (glfwGetKey(mainWindow.handle, GLFW_KEY_A) == GLFW_PRESS)
    direction |= (int)MovementDirections::Left;

  if (glfwGetKey(mainWindow.handle, GLFW_KEY_D) == GLFW_PRESS)
    direction |= (int)MovementDirections::Right;

  if (glfwGetKey(mainWindow.handle, GLFW_KEY_R) == GLFW_PRESS)
    direction |= (int)MovementDirections::Up;

  if (glfwGetKey(mainWindow.handle, GLFW_KEY_F) == GLFW_PRESS)
    direction |= (int)MovementDirections::Down;

  // Camera speed
  if (glfwGetKey(mainWindow.handle, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    camera.SetMovementSpeed(CameraTurboSpeed);
  else
    camera.SetMovementSpeed(CameraNormalSpeed);

  // Update the mouse status
  double dx, dy;
  mouseStatus.Update(dx, dy);

  // Camera orientation - mouse movement
  glm::vec2 mouseMove(0.0f, 0.0f);
  if (glfwGetMouseButton(mainWindow.handle, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
  {
    mouseMove.x = (float)(dx);
    mouseMove.y = (float)(dy);
  }

  // Update the camera movement
  camera.Move((MovementDirections)direction, mouseMove, dt);

  // Reset camera state
  if (glfwGetKey(mainWindow.handle, GLFW_KEY_ENTER) == GLFW_PRESS)
  {
    camera.SetProjection(fov, (float)mainWindow.width / (float)mainWindow.height, nearClipPlane, farClipPlane);
    camera.SetTransformation(glm::vec3(-3.0f, 3.0f, -5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
  }
}

void renderScene(int pointLights, int spotLights)
{
    // Clean the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, fboRender);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Save the screen size
    int width, height;
    glfwGetFramebufferSize(mainWindow.handle, &width, &height);
    
    // Change viewport to depth map size for shadowmaps
    //resizeCallback(mainWindow.handle, SHADOW_SIZE, SHADOW_SIZE);

    createCubeMapDepthFramebuffer(SHADOW_SIZE, SHADOW_SIZE);
    // The shadowmap pass for point lights
    for (int light = 0; light < pointLights; light++)
    {
        glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
        
        // Bind the framebuffer of cubemaps
        glBindFramebuffer(GL_FRAMEBUFFER, fboCubeMapDepth);
        glClear(GL_DEPTH_BUFFER_BIT);

        //GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
        //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, face, depthCubemap, 0);

        scene.DrawDepthSinglePointLight(camera, renderMode, light);

        // Export the texture
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);

        glViewport(0, 0, width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, fboRender);
        glClear(GL_DEPTH_BUFFER_BIT);

        // Additively draw the light using the depth texture
        // Create a separate light pass for spotlights 
        scene.DrawLightSinglePointLight(camera, renderMode, light);
            
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        // Bind back the default framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    createDepthFramebuffer(SHADOW_SIZE, SHADOW_SIZE);
    // The shadowmap pass for spotlights
    for (int light = 0; light < spotLights; light++)
    {
        glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);

        // Deal with the shadow maps
        // Bind the framebuffer and render the light source to depth map
        glBindFramebuffer(GL_FRAMEBUFFER, fboDepth);
        glClear(GL_DEPTH_BUFFER_BIT);
        
        /*// Attempt to blit the 07-Volumes* render buffer to a texture, apparently that is not an option?
        glDrawBuffer(depthMap);
        glReadBuffer(depthStencil);
        glBlitFramebuffer(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT, 0, 0, SHADOW_WIDTH, SHADOW_HEIGHT, GL_DEPTH_BUFFER_BIT, GL_LINEAR);*/
        
        scene.DrawDepthSingleSpotLight(camera, renderMode, light);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthMap);

        // Additively draw the light using the depth texture
        // It is a separate light pass for spotlight

        glViewport(0, 0, width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, fboRender);
        glClear(GL_DEPTH_BUFFER_BIT);

        // Additively draw the light using the depth texture
        // Create a separate light pass for spotlights 
        //scene.DrawLightSinglePointLight(camera, renderMode, light);

        glBindTexture(GL_TEXTURE_2D, 0);

        // Bind back the default framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    //resizeCallback(mainWindow.handle, width, height);
    
    // Change viewport back
    glViewport(0, 0, width, height);
    
    // Bind the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, fboRender);

    // Draw our scene
    //scene.Draw(camera, renderMode, carmackReverse);

    glColorMask(true, true, true, true);

    // Unbind the shader program and other resources
    glBindVertexArray(0);
    glUseProgram(0);

    {
        // Just copy the render target to the screen
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fboRender);
        glDrawBuffer(GL_BACK);
        glBlitFramebuffer(0, 0, mainWindow.width, mainWindow.height, 0, 0, mainWindow.width, mainWindow.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
}

// Helper method for implementing the application main loop
void mainLoop(int pointLights, int spotLights)
{
  static double prevTime = 0.0;
  while (!glfwWindowShouldClose(mainWindow.handle))
  {
    // Calculate delta time
    double time = glfwGetTime();
    float dt = (float)(time - prevTime);
    prevTime = time;

    // Print it to the title bar
    static char title[MAX_TEXT_LENGTH];
    static char instacing[] = "[Instancing] ";
    snprintf(title, MAX_TEXT_LENGTH, "dt = %.2fms, FPS = %.1f", dt * 1000.0f, 1.0f / dt);
    glfwSetWindowTitle(mainWindow.handle, title);

    // Poll the events like keyboard, mouse, etc.
    glfwPollEvents();

    // Process keyboard input
    processInput(dt);

    // Update scene
    if (animate)
      scene.Update(dt);

    // Render the scene
    renderScene(pointLights, spotLights);

    // Swap actual buffers on the GPU
    glfwSwapBuffers(mainWindow.handle);
  }
}

int main()
{
  // Initialize the OpenGL context and create a window
  if (!initOpenGL())
  {
    printf("Failed to initialize OpenGL!\n");
    shutDown();
    return -1;
  }

  // Compile shaders needed to run
  if (!compileShaders())
  {
    printf("Failed to compile shaders!\n");
    shutDown();
    return -1;
  }

  int pointLights = 2;
  int spotLights = 1;

  // Scene initialization
  scene.Init(10, pointLights, spotLights);

  // Enter the application main loop
  mainLoop(pointLights, spotLights);

  // Release used resources and exit
  shutDown();
  return 0;
}
