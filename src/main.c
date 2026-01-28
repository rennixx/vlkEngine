/**
 * @file main.c
 * @brief Vulkan Engine entry point
 */

#include "core/logger.h"
#include "core/memory.h"
#include "core/timer.h"
#include "platform/platform.h"
#include "renderer/vulkan_core.h"
#include "renderer/swapchain.h"
#include "renderer/sync.h"
#include "renderer/command_buffer.h"
#include "renderer/render_pass.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <GLFW/glfw3.h>

/* Window state */
static struct {
    GLFWwindow* window;
    uint32_t width;
    uint32_t height;
    bool resized;
    bool framebuffer_resized;
} g_window = {0};

/* Forward declarations */
static void glfw_framebuffer_resize_callback(GLFWwindow* window, int width, int height);
static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
static void glfw_cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
static void glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
static void glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
static void glfw_error_callback(int error, const char* description);

/* Initialize window */
static bool init_window(uint32_t width, uint32_t height, const char* title) {
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        VE_LOG_ERROR("Failed to initialize GLFW");
        return false;
    }

    /* GLFW requires no Vulkan API calls before creating window */
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    g_window.window = glfwCreateWindow((int)width, (int)height, title, NULL, NULL);
    if (!g_window.window) {
        VE_LOG_ERROR("Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    g_window.width = width;
    g_window.height = height;

    /* Set callbacks */
    glfwSetWindowUserPointer(g_window.window, &g_window);
    glfwSetFramebufferSizeCallback(g_window.window, glfw_framebuffer_resize_callback);
    glfwSetKeyCallback(g_window.window, glfw_key_callback);
    glfwSetCursorPosCallback(g_window.window, glfw_cursor_position_callback);
    glfwSetMouseButtonCallback(g_window.window, glfw_mouse_button_callback);
    glfwSetScrollCallback(g_window.window, glfw_scroll_callback);

    return true;
}

/* Shutdown window */
static void shutdown_window(void) {
    if (g_window.window) {
        glfwDestroyWindow(g_window.window);
        g_window.window = NULL;
    }
    glfwTerminate();
}

/* Initialize engine */
static bool init_engine(void) {
    /* Initialize core systems */
    if (!ve_memory_init()) {
        fprintf(stderr, "Failed to initialize memory system\n");
        return false;
    }

    ve_logger_config logger_config = {
        .level = VE_LOG_TRACE,
        .targets = VE_LOG_TARGET_CONSOLE | VE_LOG_TARGET_DEBUGGER,
        .color_output = true,
        .timestamps = true,
        .thread_ids = false,
    };

    if (!ve_logger_init(&logger_config)) {
        fprintf(stderr, "Failed to initialize logger\n");
        return false;
    }

    if (!ve_timer_init()) {
        VE_LOG_ERROR("Failed to initialize timer system");
        return false;
    }

    if (!ve_platform_init()) {
        VE_LOG_ERROR("Failed to initialize platform layer");
        return false;
    }

    /* Initialize Vulkan */
    bool enable_validation = true;
#ifdef NDEBUG
    enable_validation = false;
#endif

    if (!ve_vulkan_init("Vulkan Engine", VK_MAKE_VERSION(0, 1, 0), enable_validation)) {
        VE_LOG_ERROR("Failed to initialize Vulkan");
        return false;
    }

    /* Create surface */
    if (ve_vulkan_create_surface(g_window.window) != VK_SUCCESS) {
        VE_LOG_ERROR("Failed to create Vulkan surface");
        return false;
    }

    /* Initialize sync primitives */
    if (ve_sync_init() != VK_SUCCESS) {
        VE_LOG_ERROR("Failed to initialize sync primitives");
        return false;
    }

    /* Initialize command buffers */
    if (ve_command_buffer_init() != VK_SUCCESS) {
        VE_LOG_ERROR("Failed to initialize command buffers");
        return false;
    }

    /* Create swapchain */
    ve_swapchain_config swapchain_config = {
        .width = g_window.width,
        .height = g_window.height,
        .vsync = true,
        .triple_buffering = true,
        .preferred_format = {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        .preferred_present_mode = VK_PRESENT_MODE_MAILBOX_KHR,
        .additional_usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    };

    if (ve_swapchain_create(&swapchain_config) != VK_SUCCESS) {
        VE_LOG_ERROR("Failed to create swapchain");
        return false;
    }

    /* Create basic render pass */
    VkFormat depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
    if (!ve_vulkan_is_format_supported(depth_format, VK_IMAGE_TILING_OPTIMAL,
                                       VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
        depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    }

    VkRenderPass render_pass = VK_NULL_HANDLE;
    if (ve_render_pass_create_basic(ve_swapchain_get_format(), depth_format, &render_pass) != VK_SUCCESS) {
        VE_LOG_ERROR("Failed to create render pass");
        return false;
    }

    VE_LOG_INFO("Engine initialized successfully");
    return true;
}

/* Shutdown engine */
static void shutdown_engine(void) {
    VE_LOG_INFO("Shutting down engine...");

    ve_vulkan_wait_idle();

    /* TODO: Destroy render pass, framebuffers, etc. */

    ve_command_buffer_shutdown();
    ve_swapchain_destroy();
    ve_sync_shutdown();
    ve_vulkan_shutdown();
    ve_platform_shutdown();
    ve_timer_shutdown();
    ve_logger_shutdown();
    ve_memory_shutdown();
}

/* Main loop */
static void main_loop(void) {
    ve_frame_time frame_time = {0};
    ve_frame_time_init(&frame_time);

    VE_LOG_INFO("Entering main loop");

    while (!glfwWindowShouldClose(g_window.window)) {
        glfwPollEvents();

        /* Update frame time */
        ve_frame_time_update(&frame_time);

        /* Handle window resize */
        if (g_window.framebuffer_resized) {
            int width, height;
            glfwGetFramebufferSize(g_window.window, &width, &height);

            while (width == 0 || height == 0) {
                glfwGetFramebufferSize(g_window.window, &width, &height);
                glfwWaitEvents();
            }

            ve_swapchain_config config = {
                .width = (uint32_t)width,
                .height = (uint32_t)height,
                .vsync = true,
                .triple_buffering = true,
            };

            if (ve_swapchain_recreate(&config) == VK_SUCCESS) {
                g_window.width = (uint32_t)width;
                g_window.height = (uint32_t)height;
                VE_LOG_INFO("Swapchain resized: %ux%u", width, height);
            }

            g_window.framebuffer_resized = false;
        }

        /* TODO: Render frame */
        /* For now, just clear to screen */
        ve_frame_time* ft = &frame_time;
        (void)ft;
    }

    ve_vulkan_wait_idle();

    VE_LOG_INFO("Main loop terminated");
    VE_LOG_INFO("Average FPS: %.2f", frame_time.frame_rate);
}

/* GLFW callbacks */
static void glfw_framebuffer_resize_callback(GLFWwindow* window, int width, int height) {
    (void)width;
    (void)height;
    struct window_state* win = glfwGetWindowUserPointer(window);
    if (win) {
        win->framebuffer_resized = true;
    }
}

static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window;
    (void)scancode;
    (void)mods;

    if (action == GLFW_PRESS) {
        VE_LOG_TRACE("Key pressed: %d", key);

        if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }
}

static void glfw_cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    (void)xpos;
    (void)ypos;
}

static void glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    (void)window;
    (void)button;
    (void)action;
    (void)mods;
}

static void glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;
    (void)xoffset;
    (void)yoffset;
}

static void glfw_error_callback(int error, const char* description) {
    VE_LOG_ERROR("GLFW Error %d: %s", error, description);
}

/* Entry point */
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    VE_LOG_INFO("Vulkan Engine starting...");
    VE_LOG_INFO("Version: 0.1.0");
    VE_LOG_INFO("Build: %s %s", __DATE__, __TIME__);

    /* Initialize window */
    if (!init_window(1280, 720, "Vulkan Engine")) {
        fprintf(stderr, "Failed to initialize window\n");
        return EXIT_FAILURE;
    }

    /* Initialize engine */
    if (!init_engine()) {
        fprintf(stderr, "Failed to initialize engine\n");
        shutdown_window();
        return EXIT_FAILURE;
    }

    /* Run main loop */
    main_loop();

    /* Shutdown */
    shutdown_engine();
    shutdown_window();

    VE_LOG_INFO("Engine terminated successfully");
    return EXIT_SUCCESS;
}
