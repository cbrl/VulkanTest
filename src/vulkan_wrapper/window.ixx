module;

#include <exception>
#include <list>
#include <memory>
#include <mutex>
#include <utility>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan_raii.hpp>

export module vkw.window;


export namespace vkw {

class window {
public:
	enum class event {
		title,
		move,
		resize,
		minimize,
		maximize,
		hide,
		show,
		focus,
		close,
		key_down,
		key_up,
	};

	using event_handler = std::function<void(window&, event, uint64_t, void*)>;

	window() = default;
	window(const window&) = delete;
	window(window&&) noexcept = default;

	virtual ~window() = default;

	auto operator=(const window&) -> window = delete;
	auto operator=(window&&) noexcept -> window& = default;

	// Process any events or messages (e.g. input events)
	virtual auto update() -> void = 0;

	// Present the current frame
	virtual auto present() -> void = 0;

	[[nodiscard]]
	virtual auto get_surface() const noexcept -> const vk::raii::SurfaceKHR& = 0;

	[[nodiscard]]
	virtual auto get_title() const -> std::string = 0;
	virtual auto set_title(const std::string& title) -> void = 0;

	[[nodiscard]]
	virtual auto get_frame_size() const -> vk::Extent2D = 0;
	virtual auto set_frame_size(vk::Extent2D size) -> void = 0;

	[[nodiscard]]
	virtual auto get_window_size() const -> vk::Extent2D = 0;
	virtual auto set_window_size(vk::Extent2D size) -> void = 0;

	[[nodiscard]]
	virtual auto get_frame_position() const -> vk::Offset2D = 0;
	virtual auto set_frame_position(vk::Offset2D position) -> void = 0;

	[[nodiscard]]
	virtual auto get_window_position() const -> vk::Offset2D = 0;
	virtual auto set_window_position(vk::Offset2D position) -> void = 0;

	[[nodiscard]]
	virtual auto get_frame_rect() const -> vk::Rect2D = 0;
	virtual auto set_frame_rect(vk::Rect2D rect) -> void = 0;

	[[nodiscard]]
	virtual auto get_window_rect() const -> vk::Rect2D = 0;
	virtual auto set_window_rect(vk::Rect2D rect) -> void = 0;

	[[nodiscard]]
	virtual auto is_minimized() const -> bool = 0;
	virtual auto set_minimized(bool state) -> void = 0;

	[[nodiscard]]
	virtual auto is_maximized() const -> bool = 0;
	virtual auto set_maximized(bool state) -> void = 0;

	[[nodiscard]]
	virtual auto is_hidden() const -> bool = 0;
	virtual auto set_hidden(bool state) -> void = 0;

	[[nodiscard]]
	virtual auto is_focused() const -> bool = 0;
	virtual auto set_focused(bool state) -> void = 0;

	[[nodiscard]]
	virtual auto should_close() const -> bool = 0;
	virtual auto set_should_close(bool state) -> void = 0;

	auto add_event_handler(const event_handler& handler) -> std::list<event_handler>::iterator {
		assert(handler != nullptr);
		event_callbacks.push_back(handler);
		return std::next(event_callbacks.end(), -1);
	}

	auto remove_event_handler(std::list<event_handler>::iterator handler) -> void {
		event_callbacks.erase(handler);
	}

protected:
	auto proc_event(event e, uint64_t param, void* data) -> void {
		for (auto& handler : event_callbacks) {
			handler(*this, e, param, data);
		}
	}

private:
	std::list<event_handler> event_callbacks;
};


class glfw_window : public window {
	static inline std::mutex window_mut;
	static inline size_t window_count{0};

public:
	glfw_window(
		const std::string& title,
		const vk::Extent2D& size,
		const vk::raii::Instance& instance,
		const std::vector<std::pair<int, int>>& int_hints = {},
		const std::vector<std::pair<int, std::string>>& string_hints = {},
		GLFWmonitor* monitor = nullptr
	) :
		handle(create_handle(title, size, int_hints, string_hints, monitor)),
		surface(create_surface(instance, handle)) {
		glfwMakeContextCurrent(handle);

		glfwSetWindowUserPointer(handle, this);
		glfwSetWindowPosCallback(handle, &glfw_window::window_pos_callback);
		glfwSetFramebufferSizeCallback(handle, &glfw_window::window_size_callback);
		glfwSetWindowIconifyCallback(handle, &glfw_window::window_minimize_callback);
		glfwSetWindowMaximizeCallback(handle, &glfw_window::window_maximize_callback);
		glfwSetWindowFocusCallback(handle, &glfw_window::window_focus_callback);
		glfwSetWindowCloseCallback(handle, &glfw_window::window_close_callback);
		glfwSetKeyCallback(handle, &glfw_window::key_callback);
	}

	~glfw_window() {
		glfwDestroyWindow(std::exchange(handle, nullptr));
		deinit_glfw();
	}

	virtual void update() override {
		glfwPollEvents();
	}

	virtual void present() override {
		//glfwSwapBuffers(handle);
	}

	[[nodiscard]]
	auto get_handle() const noexcept -> GLFWwindow* {
		return handle;
	}

	[[nodiscard]]
	virtual auto get_surface() const noexcept -> const vk::raii::SurfaceKHR& override {
		return surface;
	}

	[[nodiscard]]
	virtual auto get_title() const -> std::string override {
		return title;
	}
	virtual auto set_title(const std::string& title) -> void override {
		this->title = title;
		glfwSetWindowTitle(handle, title.c_str());
		proc_event(event::title, 0, nullptr);
	}

	[[nodiscard]]
	virtual auto get_frame_size() const -> vk::Extent2D override {
		int left   = 0;
		int top    = 0;
		int right  = 0;
		int bottom = 0;
		glfwGetWindowFrameSize(handle, &left, &top, &right, &bottom);
		return {static_cast<uint32_t>(right - left), static_cast<uint32_t>(bottom - top)};
	}
	virtual auto set_frame_size(vk::Extent2D size) -> void override {
		const auto frame_size    = get_frame_size();
		const auto window_size   = get_window_size();
		const auto frame_padding = vk::Extent2D{frame_size.width - window_size.width, frame_size.height - window_size.height};

		const auto new_size = vk::Extent2D{size.width - frame_padding.width, size.height - frame_padding.height};
		set_window_size(new_size);
	}

	[[nodiscard]]
	virtual auto get_window_size() const -> vk::Extent2D override {
		int width  = 0;
		int height = 0;
		glfwGetWindowSize(handle, &width, &height);
		return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
	}
	virtual auto set_window_size(vk::Extent2D size) -> void override {
		glfwSetWindowSize(handle, size.width, size.height);
	}

	[[nodiscard]]
	virtual auto get_frame_position() const -> vk::Offset2D override {
		int left   = 0;
		int top    = 0;
		int right  = 0;
		int bottom = 0;
		glfwGetWindowFrameSize(handle, &left, &top, &right, &bottom);
		return {left, top};
	}
	virtual auto set_frame_position(vk::Offset2D position) -> void override {
		const auto window_pos = get_window_position();
		const auto frame_pos  = get_frame_position();
		const auto frame_offset = vk::Offset2D{window_pos.x - frame_pos.x, window_pos.y - frame_pos.y};

		const auto new_pos = vk::Offset2D{position.y + frame_offset.x, position.y + frame_offset.y};
		set_window_position(new_pos);
	}

	[[nodiscard]]
	virtual auto get_window_position() const -> vk::Offset2D override {
		int x = 0;
		int y = 0;
		glfwGetWindowPos(handle, &x, &y);
		return {x, y};
	}
	virtual auto set_window_position(vk::Offset2D position) -> void override {
		glfwSetWindowPos(handle, position.x, position.y);
	}

	[[nodiscard]]
	virtual auto get_frame_rect() const -> vk::Rect2D override {
		int left   = 0;
		int top    = 0;
		int right  = 0;
		int bottom = 0;
		glfwGetWindowFrameSize(handle, &left, &top, &right, &bottom);
		return {vk::Offset2D{left, top}, vk::Extent2D{static_cast<uint32_t>(right - left), static_cast<uint32_t>(bottom - top)}};
	}
	virtual auto set_frame_rect(vk::Rect2D rect) -> void override {
		set_frame_position(rect.offset);
		set_frame_size(rect.extent);
	}

	[[nodiscard]]
	virtual auto get_window_rect() const -> vk::Rect2D override {
		return {get_window_position(), get_window_size()};
	}
	virtual auto set_window_rect(vk::Rect2D rect) -> void override {
		set_window_position(rect.offset);
		set_window_size(rect.extent);
	}

	[[nodiscard]]
	virtual auto is_minimized() const -> bool override {
		return glfwGetWindowAttrib(handle, GLFW_ICONIFIED) == GLFW_TRUE;
	}
	virtual auto set_minimized(bool state) -> void override {
		if (state) {
			glfwIconifyWindow(handle);
		}
		else {
			glfwRestoreWindow(handle);
		}
	}

	[[nodiscard]]
	virtual auto is_maximized() const -> bool override {
		return glfwGetWindowAttrib(handle, GLFW_MAXIMIZED) == GLFW_TRUE;
	}
	virtual auto set_maximized(bool state) -> void override {
		if (state) {
			glfwMaximizeWindow(handle);
		}
		else {
			glfwRestoreWindow(handle);
		}
	}

	[[nodiscard]]
	virtual auto is_hidden() const -> bool override {
		return glfwGetWindowAttrib(handle, GLFW_VISIBLE) == GLFW_FALSE;
	}
	virtual auto set_hidden(bool state) -> void override {
		if (state) {
			glfwHideWindow(handle);
		}
		else {
			glfwShowWindow(handle);
		}

		// No GLFW callback for this function, so call proc_event here.
		proc_event(event::hide, 0, nullptr);
	}

	[[nodiscard]]
	virtual auto is_focused() const -> bool override {
		return glfwGetWindowAttrib(handle, GLFW_FOCUSED) == GLFW_TRUE;
	}
	virtual auto set_focused(bool state) -> void override {
		if (state) {
			glfwFocusWindow(handle);
		}
		else {
			// Can't unfocus window
		}
	}

	[[nodiscard]]
	virtual auto should_close() const -> bool override {
		return glfwWindowShouldClose(handle) == GLFW_TRUE;
	}
	virtual auto set_should_close(bool state) -> void override {
		glfwSetWindowShouldClose(handle, state ? GLFW_TRUE : GLFW_FALSE);
	}

private:

	[[nodiscard]]
	static auto get_class_pointer(GLFWwindow* handle) -> glfw_window* {
		return static_cast<glfw_window*>(glfwGetWindowUserPointer(handle));
	}

	static auto window_pos_callback(GLFWwindow* handle, int x, int y) -> void {
		(void)x;
		(void)y;
		get_class_pointer(handle)->proc_event(event::move, 0, nullptr);
	}

	static auto window_size_callback(GLFWwindow* handle, int width, int height) -> void {
		(void)width;
		(void)height;
		get_class_pointer(handle)->proc_event(event::resize, 0, nullptr);
	}

	static auto window_minimize_callback(GLFWwindow* handle, int minimized) -> void {
		(void)minimized;
		get_class_pointer(handle)->proc_event(event::minimize, 0, nullptr);
	}

	static auto window_maximize_callback(GLFWwindow* handle, int maximized) -> void {
		(void)maximized;
		get_class_pointer(handle)->proc_event(event::maximize, 0, nullptr);
	}

	static auto window_focus_callback(GLFWwindow* handle, int focused) -> void {
		(void)focused;
		get_class_pointer(handle)->proc_event(event::focus, 0, nullptr);
	}

	static auto window_close_callback(GLFWwindow* handle) -> void {
		get_class_pointer(handle)->proc_event(event::close, 0, nullptr);
	}

	static auto key_callback(GLFWwindow* handle, int key, int scancode, int action, int mods) -> void {
		auto* window_cls = get_class_pointer(handle);

		if (action == GLFW_PRESS) {
			window_cls->proc_event(event::key_down, static_cast<uint64_t>(key), nullptr);
		}
		else if (action == GLFW_RELEASE) {
			window_cls->proc_event(event::key_up, static_cast<uint64_t>(key), nullptr);
		}
	}

	[[nodiscard]]
	static auto create_handle(
		const std::string& title,
		const vk::Extent2D& size, 
		const std::vector<std::pair<int, int>>& int_hints = {},
		const std::vector<std::pair<int, std::string>>& string_hints = {},
		GLFWmonitor* monitor = nullptr
	) -> GLFWwindow* {
		init_glfw();

		// This hint is required for Vulkan usage
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		for (const auto& pair : int_hints) {
			glfwWindowHint(pair.first, pair.second);
		}
		for (const auto& pair : string_hints) {
			glfwWindowHintString(pair.first, pair.second.c_str());
		}

		auto* handle = glfwCreateWindow(size.width, size.height, title.c_str(), monitor, nullptr);
		if (not handle) {
			throw std::runtime_error{"Failed to create window"};
		}

		return handle;
	}

	[[nodiscard]]
	static auto create_surface(const vk::raii::Instance& instance, GLFWwindow* handle) -> vk::raii::SurfaceKHR {
		auto surface_khr = VkSurfaceKHR{};

		const auto err = glfwCreateWindowSurface(static_cast<VkInstance>(*instance), handle, nullptr, &surface_khr);
		if (err != VK_SUCCESS) {
			throw std::runtime_error{"Failed to create window surface"};
		}

		return vk::raii::SurfaceKHR{instance, surface_khr};
	}

	static auto init_glfw() -> void {
		auto lock = std::scoped_lock{window_mut};

		const auto status = glfwInit();
		if (status != GLFW_TRUE) {
			throw std::runtime_error{"Failed to initialize GLFW"};
		}

		window_count++;
	}

	static auto deinit_glfw() -> void {
		auto lock = std::scoped_lock{window_mut};

		window_count--;
		if (window_count == 0) {
			glfwTerminate();
		}
	}

	GLFWwindow* handle = nullptr;
	vk::raii::SurfaceKHR surface;

	std::string title;
};

} //namespace vkw
