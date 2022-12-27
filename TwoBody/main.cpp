
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define _USE_MATH_DEFINES
#include <math.h>

#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

std::string resource_folder_dir;

std::string readFromFile(std::string path) {
    std::ifstream fileStream(path);
    std::string content;

    fileStream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
        std::stringstream contentStream;
        contentStream << fileStream.rdbuf();
        fileStream.close();
        return contentStream.str();

    } catch (std::ifstream::failure e) {
        std::cout << "ERROR::FAILED_TO_READ_FILE" << std::endl;
        std::cout << "Path is " << path << std::endl;

        throw;
    }

    return "";
}

GLuint generateTexture(std::string path, int textureUnitIndex) {
    GLuint texture;
    glActiveTexture(GL_TEXTURE0 + textureUnitIndex);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // set the texture wrapping/filtering options on the currently bound texture object
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // load and generate the texture
    stbi_set_flip_vertically_on_load(true);
    
    int width, height, nrChannels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
    
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        auto message = std::string("ERROR::TEXTURE::LOADING_FAILED\n") + "Path is " + path + "\n";

        std::cout << message << std::endl;

        throw std::runtime_error(message);
    }

    stbi_image_free(data);

    return texture;
}

GLuint generateCubemap(const std::vector<std::string>& faces, int textureUnitIndex) {
    assert(faces.size() == 6);

    GLuint texture;
    glActiveTexture(GL_TEXTURE0 + textureUnitIndex);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        } else {
            auto message = std::string("ERROR::CUBEMAP_TEXTURE::LOADING_FAILED\n") + "Path is " + faces[i] + "\n";

            std::cout << message << std::endl;

            throw std::runtime_error(message);
        }

        stbi_image_free(data);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return texture;
}

class ShaderProgram {
public:
    GLuint id;
    bool hollow;

public:
    ShaderProgram() : hollow(true) {

    }

    ShaderProgram(std::string vertexSourcePath, std::string fragmentSourcePath) : hollow(false) {
        auto vertexShader = loadAndCompileShader(vertexSourcePath, GL_VERTEX_SHADER);
        auto fragmentShader = loadAndCompileShader(fragmentSourcePath, GL_FRAGMENT_SHADER);

        id = glCreateProgram();
        glAttachShader(id, vertexShader);
        glAttachShader(id, fragmentShader);
        glLinkProgram(id);

        int linkingSuccess;
        char linkingInfoLog[512];
        glGetProgramiv(id, GL_LINK_STATUS, &linkingSuccess);
        if (!linkingSuccess) {
            glGetProgramInfoLog(id, 512, NULL, linkingInfoLog);
            
            auto message = std::string("ERROR::SHADER::PROGRAM::LINKING_FAILED\n") + linkingInfoLog;
            
            std::cout << message << std::endl;

            throw std::runtime_error(message);
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    GLuint loadAndCompileShader(std::string sourcePath, int shaderType) {
        assert(shaderType == GL_VERTEX_SHADER || shaderType == GL_FRAGMENT_SHADER);

        auto source = readFromFile(sourcePath);
        const GLchar* glSourcePtr = (const GLchar*)source.c_str();

        auto shader = glCreateShader(shaderType);
        glShaderSource(shader, 1, &glSourcePtr, NULL);
        glCompileShader(shader);

        int compiliationSuccess;
        char compiliationInfoLog[512];

        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiliationSuccess);
        if (!compiliationSuccess) {
            glGetShaderInfoLog(shader, 512, NULL, compiliationInfoLog);

            std::string shaderTypeString = shaderType == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT";
            std::string message = "ERROR::SHADER::" + shaderTypeString + "::COMPILATION_FAILED\n" + compiliationInfoLog;

            std::cout << message << std::endl;
        
            throw std::runtime_error(message);
        }

        return shader;
    }

    void use() {
        glUseProgram(id);
    }

    void setMatrix4fv(const std::string& name, glm::mat4 value) {
        auto uniformLocation = glGetUniformLocation(id, name.c_str());
        glUniformMatrix4fv(uniformLocation, 1, GL_FALSE, glm::value_ptr(value));
    }

    void setVec3(const std::string& name, glm::vec3 value) {
        auto uniformLocation = glGetUniformLocation(id, name.c_str());
        glUniform3f(uniformLocation, value.x, value.y, value.z);
    }

    void setVec4(const std::string& name, glm::vec4 value) {
        auto uniformLocation = glGetUniformLocation(id, name.c_str());
        glUniform4f(uniformLocation, value.x, value.y, value.z, value.w);
    }

    void setFloat(const std::string& name, float value) {
        auto uniformLocation = glGetUniformLocation(id, name.c_str());
        glUniform1f(uniformLocation, value);
    }
};

struct TexturedVertex {
    glm::vec3 p;
    glm::vec2 texp;

    TexturedVertex(glm::vec3 p, glm::vec2 texp) : p(p), texp(texp) {

    }

    TexturedVertex(float x, float y, float z, float tx, float ty) : p(glm::vec3(x, y, z)), texp(glm::vec2(tx, ty)) {

    }
};

class Camera {
public:
    glm::vec3 pos;
    glm::vec3 front;
    glm::vec3 right;
    glm::vec3 up;

    float pitch;
    float yaw;
    float distance;
    float fov;

    Camera(float pitch, float yaw, float distance, float fov) : pitch(pitch), yaw(yaw), distance(distance), fov(fov) {
        update();
    }

    void update() {
        // normalize camera params
        distance = glm::clamp(distance, 1.0f, 30.0f);
        pitch = glm::clamp(pitch, -0.9f * 90.0f, 0.9f * 90.0f);

        while (yaw < 0) yaw += 360;
        while (yaw > 360) yaw -= 360;

        // compute
        auto rad_pitch = glm::radians(pitch);
        auto rad_yaw = glm::radians(yaw);
        
        glm::vec3 direction = glm::normalize(glm::vec3(
            cos(rad_pitch) * cos(rad_yaw),
            sin(rad_pitch),
            cos(rad_pitch) * sin(rad_yaw)
        ));

        glm::vec3 target(0, 0, 0);
        pos = -direction * distance;

        glm::vec3 worldUp(0, 1, 0);

        front = glm::normalize(direction);
        right = glm::cross(worldUp, direction);
        up = glm::cross(direction, right);
    }

    glm::mat4 viewTransform() {
        update();

        return glm::lookAt(pos, pos + front, up);
    }
};

class PolyLine {
public:
    glm::mat4 modelTransform;
    std::vector<glm::vec3> vertices;
    glm::vec4 color;

    static bool isPrepared;
    static ShaderProgram shaderProgram;
    static GLuint vertex_buffer_obj;
    static GLuint vertex_array_obj;

    PolyLine(glm::mat4 modelTransform, std::vector<glm::vec3> vertices, glm::vec4 color)
        : modelTransform(modelTransform), vertices(vertices), color(color)
    {
        prepare();
    }

    static void prepare() {
        if (isPrepared) return;

        // load shaders
        shaderProgram = ShaderProgram(
            resource_folder_dir + "polyline.vs",
            resource_folder_dir + "polyline.fs"
        );

        // prepare prog
        shaderProgram.use();
        glGenBuffers(1, &vertex_buffer_obj);
        glGenVertexArrays(1, &vertex_array_obj);

        // set prepared
        isPrepared = true;
    }

    void generateCircle(int size) {
        vertices.clear();

        for (int i = 0; i < size; i++) {
            float angle = 2 * M_PI * i / (size - 1);

            vertices.emplace_back(glm::sin(angle), 0, glm::cos(angle));
        }
    }

    PolyLine& setTransform(glm::mat4 transform) {
        modelTransform = transform;

        return *this;
    }

    PolyLine& resetTransform() {
        modelTransform = glm::mat4(1.0f);

        return *this;
    }

    PolyLine& translate(glm::vec3 v) {
        modelTransform = glm::translate(modelTransform, v);

        return *this;
    }

    PolyLine& scale(float x, float y, float z) {
        modelTransform = glm::scale(modelTransform, glm::vec3(x, y, z));

        return *this;
    }

    PolyLine& rotate(float angle, glm::vec3 axis) {
        modelTransform = glm::rotate(modelTransform, angle, axis);

        return *this;
    }

    void draw() {
        shaderProgram.use();

        glBindVertexArray(vertex_array_obj);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_obj);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STREAM_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);

        shaderProgram.setVec4("color", color);

        glDrawArrays(GL_LINE_STRIP, 0, vertices.size());
    }
};

bool PolyLine::isPrepared;
ShaderProgram PolyLine::shaderProgram;
GLuint PolyLine::vertex_buffer_obj;
GLuint PolyLine::vertex_array_obj;

class SkyBox {
public:
    GLuint texture;

    static float vertices[];
    static bool isPrepared;
    static ShaderProgram shaderProgram;
    static GLuint vertex_buffer_obj;
    static GLuint vertex_array_obj;

    SkyBox(GLuint texture) : texture(texture) {
        prepare();
    }

    static void prepare() {
        if (isPrepared) return;

        // load shaders
        shaderProgram = ShaderProgram(
            resource_folder_dir + "skybox.vs",
            resource_folder_dir + "skybox.fs"
        );

        // prepare prog
        shaderProgram.use();
        glGenBuffers(1, &vertex_buffer_obj);
        glGenVertexArrays(1, &vertex_array_obj);

        glBindVertexArray(vertex_array_obj);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_obj);
        glBufferData(GL_ARRAY_BUFFER, 108 * sizeof(float), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);

        // set prepared
        isPrepared = true;
    }

    void draw() {
        glDepthMask(GL_FALSE);
        shaderProgram.use();

        glBindVertexArray(vertex_array_obj);

        glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthMask(GL_TRUE);
    }
};

bool SkyBox::isPrepared;
ShaderProgram SkyBox::shaderProgram;
GLuint SkyBox::vertex_buffer_obj;
GLuint SkyBox::vertex_array_obj;
float SkyBox::vertices[] = {
    // positions          
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

class Sphere {
public:
    glm::mat4 modelTransform;
    float r;
    GLuint texture;

    static bool isPrepared;
    static std::vector<TexturedVertex> vertices;
    static std::vector<unsigned int> vertex_indexes;
    static int parallel_count;
    static int meridian_count;
    static ShaderProgram shaderProgram;
    static GLuint vertex_buffer_obj;
    static GLuint vertex_array_obj;
    static GLuint element_buffer_obj;

    Sphere(glm::mat4 modelTransform, float r, GLuint texture) : modelTransform(modelTransform), r(r), texture(texture) {
        prepare();

        
    }

    glm::vec4 center() {
        glm::vec4 modelCenter(0, 0, 0, 1);
        return modelTransform * modelCenter;
    }

    std::vector<glm::vec3> getAxisSegment(glm::vec3 axis) {
        auto v = glm::normalize(axis) * r * 1.5f;

        return {-v, +v};
    }

    static void prepare() {
        if (isPrepared) return;

        // set parameters
        parallel_count = 50;
        meridian_count = 50;

        // generate points
        for (int parallel_idx = 0; parallel_idx < parallel_count; parallel_idx++) {
            for (int meridian_idx = 0; meridian_idx < meridian_count; meridian_idx++) {
                float latitude = -M_PI_2 + M_PI * parallel_idx / (parallel_count - 1);
                float longitude = M_PI * 2 * meridian_idx / (meridian_count - 1);

                vertices.emplace_back(
                    cos(latitude) * sin(longitude),
                    sin(latitude),
                    cos(latitude) * cos(longitude),
                    float(meridian_idx) / (meridian_count - 1),
                    float(parallel_idx) / (parallel_count - 1)
                );
            }
        }

        // generate triangles
        auto combine_idx = [](int parallel_idx, int meridian_idx) {
            return parallel_idx * meridian_count + meridian_idx;
        };
        auto add_triangle = [](int a, int b, int c) {
            vertex_indexes.push_back(a);
            vertex_indexes.push_back(b);
            vertex_indexes.push_back(c);
        };
        
        for (int parallel_idx = 0; parallel_idx + 1 < parallel_count; parallel_idx++) {
            for (int meridian_idx = 0; meridian_idx + 1 < meridian_count; meridian_idx++) {
                int v00 = combine_idx(parallel_idx + 0, meridian_idx + 0);
                int v01 = combine_idx(parallel_idx + 0, meridian_idx + 1);
                int v10 = combine_idx(parallel_idx + 1, meridian_idx + 0);
                int v11 = combine_idx(parallel_idx + 1, meridian_idx + 1);

                bool not_with_south_pole = (parallel_idx > 0);
                bool not_with_north_pole = (parallel_idx < parallel_count - 2);

                if (not_with_south_pole) add_triangle(v00, v01, v11);
                if (not_with_north_pole) add_triangle(v00, v10, v11);
            }
        }

        std::cout << "Prepared sphere. Vertices: " << vertices.size() << " Indexes: " << vertex_indexes.size() << std::endl;

        // load shader
        shaderProgram = ShaderProgram(
            resource_folder_dir + "sphere.vs",
            resource_folder_dir + "sphere.fs"
        );

        // prepare prog
        shaderProgram.use();
        glGenBuffers(1, &vertex_buffer_obj);
        glGenVertexArrays(1, &vertex_array_obj);

        glBindVertexArray(vertex_array_obj);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_obj);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(TexturedVertex), vertices.data(), GL_STATIC_DRAW);

        glGenBuffers(1, &element_buffer_obj);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer_obj);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, vertex_indexes.size() * sizeof(int), vertex_indexes.data(), GL_STATIC_DRAW);

        // position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex), (void*)(0 * sizeof(float)));
        glEnableVertexAttribArray(0);
        // texture attrib
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // set prepared
        isPrepared = true;
    }

    Sphere& setTransform(glm::mat4 transform) {
        modelTransform = transform;

        return *this;
    }

    Sphere& resetTransform() {
        modelTransform = glm::mat4(1.0f);

        return *this;
    }

    Sphere& translate(glm::vec3 v) {
        modelTransform = glm::translate(modelTransform, v);

        return *this;
    }

    Sphere& rotate(float angle, glm::vec3 axis) {
        modelTransform = glm::rotate(modelTransform, angle, axis);

        return *this;
    }

    void draw() {
        shaderProgram.use();
        shaderProgram.setFloat("r", r);
        shaderProgram.setMatrix4fv("modelTransform", modelTransform);
        glBindVertexArray(vertex_array_obj);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer_obj);
        glBindTexture(GL_TEXTURE_2D, texture);
        glDrawElements(GL_TRIANGLES, vertex_indexes.size(), GL_UNSIGNED_INT, 0);
    }
};

bool Sphere::isPrepared;
std::vector<TexturedVertex> Sphere::vertices;
std::vector<unsigned int> Sphere::vertex_indexes;
int Sphere::parallel_count;
int Sphere::meridian_count;
ShaderProgram Sphere::shaderProgram;
GLuint Sphere::vertex_buffer_obj;
GLuint Sphere::vertex_array_obj;
GLuint Sphere::element_buffer_obj;

Camera camera(-25, 275, 16, M_PI_4);
bool camera_position_locked = true;

bool mouseDragStart = true;
float mouseLastX;
float mouseLastY;

float executionLastFrame = 0;
float executionCurrentFrame = 0;
float executionDeltaTime;

void find_resource_location() {
    const std::vector<std::string> location_candidates{
        "src/",
        "../../src/",
        "../../../TwoBody/src/"
    };

    for (const auto& location_candidate : location_candidates) {
        std::string testFileName = location_candidate + "sphere.vs";

        std::ifstream infile(testFileName);

        if (infile.good()) {
            resource_folder_dir = location_candidate;
            return;
        }
    }

    std::cout << "Program resources not found"  << std::endl;

    throw std::runtime_error("Program resources not found");
}

void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    if (!camera_position_locked) {
        if (mouseDragStart) {
            mouseLastX = xpos;
            mouseLastY = ypos;
            mouseDragStart = false;
        }

        float xoffset = xpos - mouseLastX;
        float yoffset = (ypos - mouseLastY) * -1;
        mouseLastX = xpos;
        mouseLastY = ypos;

        float sensitivity = 0.1f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        camera.yaw += xoffset;
        camera.pitch += yoffset;
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            camera_position_locked = false;
            mouseDragStart = true;
        } else {
            camera_position_locked = true;
        }
    }
}

void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    float sensitivity = 0.75f;

    camera.distance += -1 * yoffset * sensitivity;
}

int main() {
    find_resource_location();

    glfwSetErrorCallback(glfw_error_callback);

    glfwInit();
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    const char* glsl_version = "#version 130";

    const GLFWvidmode* glfw_vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    int monitor_width = glfw_vidmode->width;
    int monitor_height = glfw_vidmode->height;

    GLFWwindow* window = glfwCreateWindow(monitor_width, monitor_height, "ATSTNG's Two Body", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    //glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, mouse_scroll_callback);

    // prepare imGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    glViewport(0, 0, monitor_width, monitor_height);
    glEnable(GL_DEPTH_TEST);

    // prepare objects
    glm::vec3 world_up(0, 1, 0);
    
    bool show_imgui_settings_window = true;

    glm::vec3 light_source_dir(1, 1, 1);
    glm::vec3 light_source_color(1, 1, 1);
    
    bool show_earth_axis = true;
    float earth_rotation_speed = 1.35f * 90;
    float earth_angle = 0;
    
    float moon_orbit_position = 0;
    float moon_angle = 0;
    float moon_rotation_speed = 11.0f * 90;
    glm::vec3 moon_rotation_axis(0.5f, 1.0f, 0.05f);
    float moon_orbit_traverse_speed = 1.0f;
    float moon_orbit_radius_x = 7;
    float moon_orbit_radius_z = 3;
    float moon_orbit_pitch = 90 / 8;
    float moon_orbit_roll = 90 / 5;
    bool ignore_textures = false;
    bool show_orbit = true;
    bool show_moon_axis = true;

    auto earth_texture = generateTexture(resource_folder_dir + "earth2048.bmp", 0);
    auto moon_texture = generateTexture(resource_folder_dir + "moon1024.bmp", 0);
    auto skybox_texture = generateCubemap({
        resource_folder_dir + "bkg1_right.png",
        resource_folder_dir + "bkg1_left.png",
        resource_folder_dir + "bkg1_bot.png", // swapped tex 3 and 4 for whatever reason
        resource_folder_dir + "bkg1_top.png",
        resource_folder_dir + "bkg1_front.png",
        resource_folder_dir + "bkg1_back.png"
    }, 0);

    SkyBox skybox(skybox_texture);

    Sphere earth(glm::mat4(1), 1, earth_texture);
    Sphere moon(glm::mat4(1), 0.5, moon_texture);

    std::vector<std::reference_wrapper<Sphere>> spheres{earth, moon};

    const glm::vec4 lightBlueColor(0.5, 0.5, 1, 1);
    const glm::vec4 lightRedColor(1, 0.5, 0.5, 1);

    PolyLine earth_axis(glm::mat4(1), earth.getAxisSegment(world_up), lightRedColor);
    PolyLine moon_axis(glm::mat4(1), moon.getAxisSegment(moon_rotation_axis), lightRedColor);
    PolyLine moon_orbit(glm::mat4(1), {}, lightBlueColor);
    moon_orbit.generateCircle(256);

    std::vector<std::reference_wrapper<PolyLine>> polylines;

    // main loop
    while (!glfwWindowShouldClose(window)) {
        // prepare
        executionLastFrame = executionCurrentFrame;
        executionCurrentFrame = glfwGetTime();
        executionDeltaTime = executionCurrentFrame - executionLastFrame;

        int display_width, display_height;
        glfwGetFramebufferSize(window, &display_width, &display_height);
        glViewport(0, 0, display_width, display_height);

        glfwPollEvents();

        // update
        earth_angle += executionDeltaTime * earth_rotation_speed;
        moon_angle += executionDeltaTime * moon_rotation_speed;

        if (earth_angle > 360) earth_angle -= 360;
        if (moon_angle > 360) moon_angle -= 360;

        moon_orbit_position += executionDeltaTime * moon_orbit_traverse_speed;
        
        // transformations applied in reverse order (why opengl!?)
        earth
            .resetTransform()
            .rotate(glm::radians(earth_angle), world_up);

        moon_orbit
            .resetTransform()
            .rotate(glm::radians(moon_orbit_pitch), glm::vec3(1, 0, 0))
            .rotate(glm::radians(moon_orbit_roll), glm::vec3(0, 0, 1))
            .scale(moon_orbit_radius_x, 1, moon_orbit_radius_z);

        moon
            .resetTransform()
            .rotate(glm::radians(moon_orbit_pitch), glm::vec3(1, 0, 0))
            .rotate(glm::radians(moon_orbit_roll), glm::vec3(0, 0, 1))
            .translate(glm::vec3(moon_orbit_radius_x * glm::sin(moon_orbit_position), 0, 0))
            .translate(glm::vec3(0, 0, moon_orbit_radius_z * glm::cos(moon_orbit_position)))
            .rotate(glm::radians(moon_angle), moon_rotation_axis);

        polylines.clear();

        if (show_earth_axis) {
            earth_axis.modelTransform = earth.modelTransform;
            earth_axis.vertices = earth.getAxisSegment(world_up);
            polylines.emplace_back(earth_axis);
        }
        
        if (show_moon_axis) {
            moon_axis.modelTransform = moon.modelTransform;
            moon_axis.vertices = moon.getAxisSegment(moon_rotation_axis);
            polylines.emplace_back(moon_axis);
        }

        if (show_orbit) {
            polylines.emplace_back(moon_orbit);
        }

        // draw
        glClearColor(0.2f, 0.1f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto viewTransform = camera.viewTransform();
        auto projTransform = glm::perspective(camera.fov, float(display_width) / display_height, 0.1f, 100.0f);

        {
            auto vertexTransform = projTransform * glm::mat4(glm::mat3(viewTransform)); // no translation in viewTransform

            skybox.shaderProgram.use();
            skybox.shaderProgram.setMatrix4fv("vertexTransform", vertexTransform);
            skybox.draw();
        }

        PolyLine::shaderProgram.use();
        for (PolyLine& polyline : polylines) {
            auto modelTransform = polyline.modelTransform;

            auto vertexTransform = projTransform * viewTransform * modelTransform;

            polyline.shaderProgram.use();
            polyline.shaderProgram.setMatrix4fv("vertexTransform", vertexTransform);

            polyline.draw();
        }

        Sphere::shaderProgram.use();
        for (Sphere& sphere : spheres) {
            auto modelTransform = sphere.modelTransform;
            auto vertexTransform = projTransform * viewTransform * modelTransform;

            sphere.shaderProgram.use();
            sphere.shaderProgram.setMatrix4fv("vertexTransform", vertexTransform);
            sphere.shaderProgram.setVec3("cameraPos", camera.pos);
            sphere.shaderProgram.setVec3("lightDirection", light_source_dir);
            sphere.shaderProgram.setVec3("lightColor", light_source_color);
            sphere.shaderProgram.setVec3("cameraPos", camera.pos);
            sphere.shaderProgram.setFloat("ignoreTextures", ignore_textures);

            sphere.draw();
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (show_imgui_settings_window) {
            ImGui::SetNextWindowSize({445, 645}, ImGuiCond_Once);
            ImGui::Begin("Scene settings");

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

            ImGui::Checkbox("Ignore textures", &ignore_textures);

            ImGui::DragFloat3("Light direction", (float*)&light_source_dir, 0.01f, -1.0f, 1.0f);
            ImGui::ColorEdit3("Light color", (float*)&light_source_color);

            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::CollapsingHeader("Camera")) {
                ImGui::SliderFloat("Camera pitch", &camera.pitch, -0.9f * 90.0f, 0.9f * 90.0f);
                ImGui::SliderFloat("Camera yaw", &camera.yaw, 0.0f, 360.0f);
                ImGui::Text("Drag scene holding LMB to rotate camera");

                ImGui::SliderFloat("Camera distance", &camera.distance, 1.0f, 30.0f);
                ImGui::Text("Use mouse scroll to adjust camera distance");
            }

            if (ImGui::CollapsingHeader("Earth")) {
                ImGui::SliderFloat("Earth size", &earth.r, 0.1f, 10.0f);
                ImGui::SliderFloat("Earth angle", &earth_angle, 0.0f, 360.0f);
                ImGui::SliderFloat("Earth rotation speed", &earth_rotation_speed, 0.0f, 20.0f * 180.0f);
                ImGui::Checkbox("Show Earth axis", &show_earth_axis);
            }

            if (ImGui::CollapsingHeader("Moon")) {
                ImGui::SliderFloat("Moon size", &moon.r, 0.1f, 10.0f);
                ImGui::SliderFloat("Moon angle", &moon_angle, 0.0f, 360.0f);

                ImGui::SliderFloat("Moon rotation speed", &moon_rotation_speed, 0.0f, 20.0f * 180.0f);
                ImGui::SliderFloat("Moon traverse speed", &moon_orbit_traverse_speed, 0.0f, 5.0f);

                ImGui::Checkbox("Show Moon orbit", &show_orbit);
                ImGui::SliderFloat("Orbit radius X", &moon_orbit_radius_x, 1.0f, 20.0f);
                ImGui::SliderFloat("Orbit radius Z", &moon_orbit_radius_z, 1.0f, 20.0f);
                ImGui::SliderFloat("Orbit pitch", &moon_orbit_pitch, 0.0f, 180.0f);
                ImGui::SliderFloat("Orbit roll", &moon_orbit_roll, 0.0f, 180.0f);

                ImGui::Checkbox("Show Moon axis", &show_moon_axis);
                ImGui::DragFloat3("Moon axis", (float*)&moon_rotation_axis, 0.01f, -1.0f, 1.0f);
            }

            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    glfwTerminate();

    return 0;
}

