/*
 * Source code for the NPGR019 lab practices. Copyright Martin Kahoun 2021.
 * Licensed under the zlib license, see LICENSE.txt in the root directory.
 */

#pragma once

#include <Camera.h>
#include <Geometry.h>
#include <Textures.h>

// Textures we'll be using
namespace LoadedTextures
{
  enum
  {
    White, Grey, Blue, CheckerBoard, Diffuse, Normal, Specular, Occlusion, NumTextures
  };
}

// Render mode structure
struct RenderMode
{
  // Vsync on?
  bool vsync;
  // Draw wireframe?
  bool wireframe;
  // Tonemapping on?
  bool tonemapping;
  // Used MSAA samples
  GLsizei msaaLevel;
};

// Very simple scene abstraction class
class Scene
{
public:
  // Draw passes over the scene
  enum class RenderPass : int
  {
    // Single passes
    DepthPass = 0x0001,
    ShadowVolume = 0x0002,
    DirectLight = 0x0004,
    AmbientLight = 0x0008,
    ShadowMap = 0x0010,
    // Combinations
    LightPass = 0x000c, // diffuse | ambient
    ShadowPass = 0x0012 // sh.map | sh.volume
  };

  // Data for a single object instance
  struct InstanceData
  {
    // In this simple example just a transformation matrix, transposed for efficient storage
    glm::mat3x4 transformation;
  };

  // Maximum number of allowed instances - must match the instancing vertex shader!
  static const unsigned int MAX_INSTANCES = 1024;

  // Get and create instance for this singleton
  static Scene& GetInstance();
  // Initialize the test scene
  void Init(int numCubes, int numPointLights, int numSpotLights);
  // Updates positions
  void Update(float dt);
  // Draw the scene
  void Draw(const Camera& camera, const RenderMode& renderMode, bool carmackReverse);
  
  // Depth pass for lights
  void DrawDepthSingleSpotLight(const Camera& camera, const RenderMode& renderMode, int);
  void DrawDepthSinglePointLight(const Camera& camera, const RenderMode& renderMode, int);
 
  void DrawLightSinglePointLight(const Camera& camera, const RenderMode& renderMode, int);
 
  // Return the generic VAO for rendering
  GLuint GetGenericVAO() { return _vao; }

private:
  // Structure describing a point light
  struct PointLight
  {
    // Position of the light
    glm::vec3 position;
    // Color and ambient intensity of the light
    glm::vec4 color;
    // Parameters for the light movement
    glm::vec4 movement;
  };

  // Structure describing a spot light
  struct SpotLight
  {
      // Position of the light
      glm::vec3 position;
      // Color and ambient intensity of the light
      glm::vec4 color;
      // Parameters for the light movement
      glm::vec4 movement;
      // Direction of the light cone
      glm::vec3 direction;
      // cutoff (as a cosine of angle, as per learnOpenGl)
      float cutOff;
      float outerCutOff;
  };

  // All is private, instance is created in GetInstance()
  Scene();
  ~Scene();
  // No copies allowed
  Scene(const Scene &);
  Scene & operator = (const Scene &);

  // Helper function for binding the appropriate textures
  void BindTextures(const GLuint &diffuse, const GLuint &normal, const GLuint &specular, const GLuint &occlusion);
  // Helper function for creating and updating the instance data
  void UpdateInstanceData();
  // Helper function for updating shader program data
  void UpdateProgramData(GLuint program, RenderPass renderPass, const Camera &camera, const glm::vec3 &lightPosition, const glm::vec4 &lightColor,
      const glm::vec3& lightDirection, const float& cutOff, const float& outerCutOff);
  // Helper method to update transformation uniform block
  void UpdateTransformBlock(const Camera& camera);
  // Helper method to update transformation uniform block
  void UpdateTransformBlockSingleSpotLight(const int&);
  // Draw the backdrop, floor and walls
  void DrawBackground(GLuint program, RenderPass renderPass, const Camera &camera, const glm::vec3 &lightPosition, const glm::vec4 &lightColor,
      const glm::vec3& lightDirection, const float& cutOff, const float& outerCutOff);
  // Draw cubes
  void DrawObjects(GLuint program, RenderPass renderPass, const Camera &camera, const glm::vec3 &lightPosition, const glm::vec4 &lightColor,
      const glm::vec3& lightDirection, const float& cutOff, const float& outerCutOff);

  // Textures helper instance
  Textures &_textures;
  // Loaded textures
  GLuint _loadedTextures[LoadedTextures::NumTextures] = {0};
  // Number of cubes in the scene
  int _numCubes = 10;
  // Cube positions
  std::vector<glm::vec3> _cubePositions;

  // Number of lights in the scene
  int _numPointLights;
  // Lights positions
  std::vector<PointLight> _pointLights;

  // Number of spot lights in the scene
  int _numSpotLights;
  // Lights positions
  std::vector<SpotLight> _spotLights;

  // General use VAO
  GLuint _vao = 0;
  // Quad instance
  Mesh<Vertex_Pos_Nrm_Tgt_Tex> *_quad = nullptr;
  // Cube instance
  Mesh<Vertex_Pos_Nrm_Tgt_Tex>* _cube = nullptr;
  // Cube instance w/ adjacency information
  Mesh<Vertex_Pos> *_cubeAdjacency = nullptr;
  // Instancing buffer handle
  GLuint _instancingBuffer = 0;
  // Transformation matrices uniform buffer object
  GLuint _transformBlockUBO = 0;
};
