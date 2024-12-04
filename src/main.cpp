#include <glad/glad.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "graphics/fps_camera/fps_camera.hpp"
#include "graphics/texture_packer/texture_packer.hpp"
#include "graphics/window/window.hpp"
#include "input/glfw_lambda_callback_manager/glfw_lambda_callback_manager.hpp"
#include "utility/texture_packer_model_loading/texture_packer_model_loading.hpp"
#include "utility/model_loading/model_loading.hpp"
#include "graphics/batcher/generated/batcher.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include "graphics/shader_cache/shader_cache.hpp"

unsigned int SCREEN_WIDTH = 640;
unsigned int SCREEN_HEIGHT = 480;

static void error_callback(int error, const char *description) { fprintf(stderr, "Error: %s\n", description); }

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

// Wrapper that automatically creates a lambda for member functions
template <typename T, typename R, typename... Args> auto wrap_member_function(T &obj, R (T::*f)(Args...)) {
    // Return a std::function that wraps the member function in a lambda
    return std::function<R(Args...)>{[&obj, f](Args &&...args) { return (obj.*f)(std::forward<Args>(args)...); }};
}

int main() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("mwe_shader_cache_logs.txt", true);
    file_sink->set_level(spdlog::level::info);

    std::vector<spdlog::sink_ptr> sinks = {console_sink, file_sink};

    GLFWwindow *window =
        initialize_glfw_glad_and_return_window(SCREEN_WIDTH, SCREEN_HEIGHT, "glfw window", false, true, false);

    std::vector<ShaderType> requested_shaders = {
        ShaderType::TEXTURE_PACKER_CWL_V_TRANSFORMATION_UBOS_1024_AMBIENT_AND_DIFFUSE_LIGHTING};
    ShaderCache shader_cache(requested_shaders, sinks);
    Batcher batcher(shader_cache);

    FPSCamera camera(glm::vec3(0, 0, 3), 50, SCREEN_WIDTH, SCREEN_HEIGHT, 90, 0.1, 50);
    std::function<void(unsigned int)> char_callback = [](unsigned int _) {};
    std::function<void(int, int, int, int)> key_callback = [](int _, int _1, int _2, int _3) {};
    std::function<void(double, double)> mouse_pos_callback = wrap_member_function(camera, &FPSCamera::mouse_callback);
    std::function<void(int, int, int)> mouse_button_callback = [](int _, int _1, int _2) {};
    GLFWLambdaCallbackManager glcm(window, char_callback, key_callback, mouse_pos_callback, mouse_button_callback);

    TexturePacker texture_packer(
        "assets/packed_textures/packed_texture.json",
        {"assets/packed_textures/packed_texture_0.png", "assets/packed_textures/packed_texture_1.png"});

    std::vector<IVPNTextured> backpack = parse_model_into_ivpnts("assets/models/backpack/backpack.obj", false);
    std::vector<IVPNTexturePacked> packed_backpack = convert_ivpnt_to_ivpntp(backpack, texture_packer);

    std::vector<IVPNTextured> lightbulb = parse_model_into_ivpnts("assets/models/lightbulb/lightbulb.obj", true);
    std::vector<IVPNTexturePacked> packed_lightbulb = convert_ivpnt_to_ivpntp(lightbulb, texture_packer);

    int width, height;

    glm::vec4 color = glm::vec4(.5, .5, .5, 1);

    double previous_time = glfwGetTime();

    GLuint ltw_matrices_gl_name;
    glm::mat4 ltw_matrices[1024];

    // initialize all matrices to identity matrices
    for (int i = 0; i < 1024; ++i) {
        ltw_matrices[i] = glm::mat4(1.0f);
    }

    glGenBuffers(1, &ltw_matrices_gl_name);
    glBindBuffer(GL_UNIFORM_BUFFER, ltw_matrices_gl_name);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ltw_matrices), ltw_matrices, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ltw_matrices_gl_name);

    while (!glfwWindowShouldClose(window)) {

        double current_time = glfwGetTime();
        double delta_time = current_time - previous_time;
        previous_time = current_time;

        camera.process_input(window, delta_time);

        glfwGetFramebufferSize(window, &width, &height);

        glViewport(0, 0, width, height);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = camera.get_projection_matrix();
        glm::mat4 view = camera.get_view_matrix();

        shader_cache.set_uniform(ShaderType::TEXTURE_PACKER_CWL_V_TRANSFORMATION_UBOS_1024_AMBIENT_AND_DIFFUSE_LIGHTING,
                                 ShaderUniformVariable::CAMERA_TO_CLIP, projection);
        shader_cache.set_uniform(ShaderType::TEXTURE_PACKER_CWL_V_TRANSFORMATION_UBOS_1024_AMBIENT_AND_DIFFUSE_LIGHTING,
                                 ShaderUniformVariable::WORLD_TO_CAMERA, view);
        /*shader_cache.set_uniform(ShaderType::TEXTURE_PACKER_CWL_V_TRANSFORMATION_UBOS_1024,*/
        /*                         ShaderUniformVariable::LOCAL_TO_WORLD, glm::mat4(1.0));*/

        // Variables for light movement
        float radius = 5.0f;                 // Orbit radius
        float lightSpeed = 1.0f;             // Speed of orbit
        float verticalShiftAmplitude = 2.0f; // Amplitude of sine wave for y-axis movement

        // Update light position based on time
        float currentTime = glfwGetTime(); // Get elapsed time in seconds

        // Calculate new light position
        float lightPosX = radius * cos(lightSpeed * currentTime); // Orbit in X-Z plane
        float lightPosZ = radius * sin(lightSpeed * currentTime);
        float lightPosY = verticalShiftAmplitude * sin(currentTime); // Vertical sine wave motion

        float ambient_light_strength = .5;

        float r = (sin(currentTime * 0.5f) + 1.0f) / 2.0f;        // Cycles red from 0 to 1
        float g = (sin(currentTime * 0.7f + 2.0f) + 1.0f) / 2.0f; // Cycles green from 0 to 1 (phase shifted)
        float b = (sin(currentTime * 0.9f + 4.0f) + 1.0f) / 2.0f; // Cycles blue from 0 to 1 (phase shifted)

        r = 1;
        g = 1;
        b = 1;

        glm::vec3 ambient_light_color(r, g, b);

        glm::vec3 diffuse_light_position(lightPosX, lightPosY, lightPosZ);

        shader_cache.set_uniform(ShaderType::TEXTURE_PACKER_CWL_V_TRANSFORMATION_UBOS_1024_AMBIENT_AND_DIFFUSE_LIGHTING,
                                 ShaderUniformVariable::AMBIENT_LIGHT_STRENGTH, ambient_light_strength);
        shader_cache.set_uniform(ShaderType::TEXTURE_PACKER_CWL_V_TRANSFORMATION_UBOS_1024_AMBIENT_AND_DIFFUSE_LIGHTING,
                                 ShaderUniformVariable::AMBIENT_LIGHT_COLOR, ambient_light_color);
        shader_cache.set_uniform(ShaderType::TEXTURE_PACKER_CWL_V_TRANSFORMATION_UBOS_1024_AMBIENT_AND_DIFFUSE_LIGHTING,
                                 ShaderUniformVariable::DIFFUSE_LIGHT_POSITION, diffuse_light_position);

        unsigned int count = 0;
        for (auto &ivptp : packed_backpack) {
            std::vector<unsigned int> ltw_indices(ivptp.xyz_positions.size(), 0);
            std::vector<int> ptis(ivptp.xyz_positions.size(), ivptp.packed_texture_index);
            batcher.texture_packer_cwl_v_transformation_ubos_1024_ambient_and_diffuse_lighting_shader_batcher
                .queue_draw(count, ivptp.indices, ivptp.xyz_positions, ltw_indices, ptis,
                            ivptp.packed_texture_coordinates, ivptp.normals);
            count++;
        }

        for (auto &ivptp : packed_lightbulb) {
            std::vector<unsigned int> ltw_indices(ivptp.xyz_positions.size(), 0);
            std::vector<int> ptis(ivptp.xyz_positions.size(), ivptp.packed_texture_index);
            batcher.texture_packer_cwl_v_transformation_ubos_1024_ambient_and_diffuse_lighting_shader_batcher
                .queue_draw(count, ivptp.indices, ivptp.xyz_positions, ltw_indices, ptis,
                            ivptp.packed_texture_coordinates, ivptp.normals);
            count++;
        }

        batcher.texture_packer_cwl_v_transformation_ubos_1024_ambient_and_diffuse_lighting_shader_batcher
            .draw_everything();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);

    glfwTerminate();
    exit(EXIT_SUCCESS);
}
