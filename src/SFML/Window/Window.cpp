////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2024 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/GlContext.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/Window.hpp>
#include <SFML/Window/WindowImpl.hpp>

#include <SFML/System/Clock.hpp>
#include <SFML/System/Err.hpp>
#include <SFML/System/Sleep.hpp>
#include <SFML/System/String.hpp>
#include <SFML/System/Time.hpp>


namespace sf
{
////////////////////////////////////////////////////////////
struct Window::Window::CommonImpl
{
    explicit CommonImpl(GraphicsContext& theGraphicsContext) : graphicsContext(&theGraphicsContext)
    {
    }

    GraphicsContext*                 graphicsContext;
    priv::UniquePtr<priv::GlContext> context;        //!< Platform-specific implementation of the OpenGL context
    Clock                            clock;          //!< Clock for measuring the elapsed time between frames
    Time                             frameTimeLimit; //!< Current framerate limit
};


////////////////////////////////////////////////////////////
Window::Window(GraphicsContext& graphicsContext) : m_commonImpl(graphicsContext)
{
}


////////////////////////////////////////////////////////////
Window::Window(GraphicsContext&       graphicsContext,
               VideoMode              mode,
               const String&          title,
               std::uint32_t          style,
               State                  state,
               const ContextSettings& settings) :
Window(graphicsContext)
{
    Window::create(mode, title, style, state, settings);
}


////////////////////////////////////////////////////////////
Window::Window(GraphicsContext&       graphicsContext,
               VideoMode              mode,
               const char*            title,
               std::uint32_t          style,
               State                  state,
               const ContextSettings& settings) :
Window(graphicsContext, mode, String(title), style, state, settings)
{
}


////////////////////////////////////////////////////////////
Window::Window(GraphicsContext& graphicsContext, VideoMode mode, const String& title, State state, const ContextSettings& settings) :
Window(graphicsContext)
{
    Window::create(mode, title, sf::Style::Default, state, settings);
}


////////////////////////////////////////////////////////////
Window::Window(GraphicsContext& graphicsContext, VideoMode mode, const char* title, State state, const ContextSettings& settings) :
Window(graphicsContext, mode, String(title), state, settings)
{
}


////////////////////////////////////////////////////////////
Window::Window(GraphicsContext& graphicsContext, WindowHandle handle, const ContextSettings& settings) :
Window(graphicsContext)
{
    Window::create(handle, settings);
}


////////////////////////////////////////////////////////////
Window::~Window() = default;


////////////////////////////////////////////////////////////
Window::Window(Window&&) noexcept = default;


////////////////////////////////////////////////////////////
Window& Window::operator=(Window&&) noexcept = default;


////////////////////////////////////////////////////////////
void Window::create(VideoMode mode, const String& title, std::uint32_t style, State state)
{
    Window::create(mode, title, style, state, ContextSettings{});
}


////////////////////////////////////////////////////////////
void Window::create(VideoMode mode, const String& title, std::uint32_t style, State state, const ContextSettings& settings)
{
    // Delegate to base class for creation logic
    WindowBase::create(mode, style, state);

    // Recreate the window implementation
    m_impl = priv::WindowImpl::create(mode, title, style, state, settings);

    // Recreate the context
    m_commonImpl->context = priv::GlContext::create(*m_commonImpl->graphicsContext, settings, *m_impl, mode.bitsPerPixel);

    // Perform common initializations
    initialize();
}


////////////////////////////////////////////////////////////
void Window::create(WindowHandle handle)
{
    Window::create(handle, ContextSettings{});
}


////////////////////////////////////////////////////////////
void Window::create(WindowHandle handle, const ContextSettings& settings)
{
    // Destroy the previous window implementation
    close();

    // Recreate the window implementation
    WindowBase::create(handle);

    // Recreate the context
    m_commonImpl->context = priv::GlContext::create(*m_commonImpl->graphicsContext,
                                                    settings,
                                                    *m_impl,
                                                    VideoMode::getDesktopMode().bitsPerPixel);

    // Perform common initializations
    initialize();
}


////////////////////////////////////////////////////////////
void Window::close()
{
    // Delete the context
    m_commonImpl->context.reset();

    // Close the base window
    WindowBase::close();
}


////////////////////////////////////////////////////////////
const ContextSettings& Window::getSettings() const
{
    static constexpr ContextSettings empty{/* depthBits */ 0, /* stencilBits */ 0, /* antialiasingLevel */ 0};

    return m_commonImpl->context ? m_commonImpl->context->getSettings() : empty;
}


////////////////////////////////////////////////////////////
void Window::setVerticalSyncEnabled(bool enabled)
{
    if (setActive())
        m_commonImpl->context->setVerticalSyncEnabled(enabled);
}


////////////////////////////////////////////////////////////
void Window::setFramerateLimit(unsigned int limit)
{
    m_commonImpl->frameTimeLimit = limit > 0 ? seconds(1.f / static_cast<float>(limit)) : Time::Zero;
}


////////////////////////////////////////////////////////////
bool Window::setActive(bool active) const
{
    if (m_commonImpl->context)
    {
        if (m_commonImpl->context->setActive(*m_commonImpl->graphicsContext, active))
        {
            return true;
        }

        priv::err() << "Failed to activate the window's context" << priv::errEndl;
        return false;
    }

    return false;
}


////////////////////////////////////////////////////////////
void Window::display()
{
    // Display the backbuffer on screen
    if (setActive())
        m_commonImpl->context->display();

    // Limit the framerate if needed
    if (m_commonImpl->frameTimeLimit != Time::Zero)
    {
        sleep(m_commonImpl->frameTimeLimit - m_commonImpl->clock.getElapsedTime());
        m_commonImpl->clock.restart();
    }
}


////////////////////////////////////////////////////////////
void Window::initialize()
{
    // Setup default behaviors (to get a consistent behavior across different implementations)
    setVerticalSyncEnabled(false);
    setFramerateLimit(0);

    // Reset frame time
    m_commonImpl->clock.restart();

    // Activate the window
    if (!setActive())
    {
        priv::err() << "Failed to set window as active during initialization" << priv::errEndl;
    }

    WindowBase::initialize();
}

} // namespace sf
