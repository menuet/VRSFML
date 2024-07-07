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
#include <SFML/Window/Context.hpp>
#include <SFML/Window/ContextSettings.hpp>
#include <SFML/Window/GlContext.hpp>
#include <SFML/Window/GraphicsContext.hpp>

#include <SFML/System/AlgorithmUtils.hpp>
#include <SFML/System/Err.hpp>
#include <SFML/System/Macros.hpp>
#include <SFML/System/UniquePtr.hpp>

#include <glad/gl.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>


#if defined(SFML_SYSTEM_WINDOWS)

#if defined(SFML_OPENGL_ES)

#include <SFML/Window/EglContext.hpp>
using ContextType = sf::priv::EglContext;

#else

#include <SFML/Window/Win32/WglContext.hpp>
using ContextType = sf::priv::WglContext;

#endif

#elif defined(SFML_SYSTEM_LINUX) || defined(SFML_SYSTEM_FREEBSD) || defined(SFML_SYSTEM_OPENBSD) || \
    defined(SFML_SYSTEM_NETBSD)

#if defined(SFML_USE_DRM)

#include <SFML/Window/DRM/DRMContext.hpp>
using ContextType = sf::priv::DRMContext;

#elif defined(SFML_OPENGL_ES)

#include <SFML/Window/EglContext.hpp>
using ContextType = sf::priv::EglContext;

#else

#include <SFML/Window/Unix/GlxContext.hpp>
using ContextType = sf::priv::GlxContext;

#endif

#elif defined(SFML_SYSTEM_MACOS)

#include <SFML/Window/macOS/SFContext.hpp>
using ContextType = sf::priv::SFContext;

#elif defined(SFML_SYSTEM_IOS)

#include <SFML/Window/iOS/EaglContext.hpp>
using ContextType = sf::priv::EaglContext;

#elif defined(SFML_SYSTEM_ANDROID)

#include <SFML/Window/EglContext.hpp>
using ContextType = sf::priv::EglContext;

#endif

#if defined(SFML_SYSTEM_WINDOWS)

using glEnableFuncType      = void(APIENTRY*)(GLenum);
using glGetErrorFuncType    = GLenum(APIENTRY*)();
using glGetIntegervFuncType = void(APIENTRY*)(GLenum, GLint*);
using glGetStringFuncType   = const GLubyte*(APIENTRY*)(GLenum);
using glGetStringiFuncType  = const GLubyte*(APIENTRY*)(GLenum, GLuint);
using glIsEnabledFuncType   = GLboolean(APIENTRY*)(GLenum);

#else

using glEnableFuncType      = void (*)(GLenum);
using glGetErrorFuncType    = GLenum (*)();
using glGetIntegervFuncType = void (*)(GLenum, GLint*);
using glGetStringFuncType   = const GLubyte* (*)(GLenum);
using glGetStringiFuncType  = const GLubyte* (*)(GLenum, GLuint);
using glIsEnabledFuncType   = GLboolean (*)(GLenum);

#endif

#if !defined(GL_MULTISAMPLE)
#define GL_MULTISAMPLE 0x809D
#endif

#if !defined(GL_MAJOR_VERSION)
#define GL_MAJOR_VERSION 0x821B
#endif

#if !defined(GL_MINOR_VERSION)
#define GL_MINOR_VERSION 0x821C
#endif

#if !defined(GL_NUM_EXTENSIONS)
#define GL_NUM_EXTENSIONS 0x821D
#endif

#if !defined(GL_CONTEXT_FLAGS)
#define GL_CONTEXT_FLAGS 0x821E
#endif

#if !defined(GL_FRAMEBUFFER_SRGB)
#define GL_FRAMEBUFFER_SRGB 0x8DB9
#endif

#if !defined(GL_CONTEXT_FLAG_DEBUG_BIT)
#define GL_CONTEXT_FLAG_DEBUG_BIT 0x00000002
#endif

#if !defined(GL_CONTEXT_PROFILE_MASK)
#define GL_CONTEXT_PROFILE_MASK 0x9126
#endif

#if !defined(GL_CONTEXT_CORE_PROFILE_BIT)
#define GL_CONTEXT_CORE_PROFILE_BIT 0x00000001
#endif

#if !defined(GL_CONTEXT_COMPATIBILITY_PROFILE_BIT)
#define GL_CONTEXT_COMPATIBILITY_PROFILE_BIT 0x00000002
#endif


namespace
{
// A nested named namespace is used here to allow unity builds of SFML.
namespace GlContextImpl
{
////////////////////////////////////////////////////////////
// This structure contains all the state necessary to
// track current context information for each thread
////////////////////////////////////////////////////////////
struct CurrentContext
{
    std::uint64_t        id{};
    sf::priv::GlContext* ptr{};
    unsigned int         transientCount{};

    // This per-thread variable holds the current context information for each thread
    static CurrentContext& get()
    {
        thread_local CurrentContext currentContext;
        return currentContext;
    }

private:
    // Private constructor to prevent CurrentContext from being constructed outside of get()
    CurrentContext() = default;
};

} // namespace GlContextImpl
} // namespace


namespace sf::priv
{
namespace
{
////////////////////////////////////////////////////////////
// This structure contains all the state necessary to
// track TransientContext usage
////////////////////////////////////////////////////////////
struct [[nodiscard]] TransientContext
{
    ////////////////////////////////////////////////////////////
    [[nodiscard]] explicit TransientContext(GraphicsContext& theGraphicsContext) :
    graphicsContext(theGraphicsContext),
    sharedContextLock(graphicsContext.getMutex())
    {
        // TransientContext should never be created if there is
        // already a context active on the current thread
        assert(!GlContext::hasActiveContext() && "Another context is active on the current thread");

        // Lock the shared context for temporary use
        if (!graphicsContext.setActive(true))
            err() << "Error enabling shared context in TransientContext()" << errEndl;
    }

    ////////////////////////////////////////////////////////////
    ~TransientContext()
    {
        if (sharedContextLock)
        {
            if (!graphicsContext.setActive(false))
                err() << "Error disabling shared context in ~TransientContext()" << errEndl;
        }
    }

    ////////////////////////////////////////////////////////////
    TransientContext(const TransientContext&)            = delete;
    TransientContext& operator=(const TransientContext&) = delete;

    ////////////////////////////////////////////////////////////
    TransientContext(TransientContext&&)            = delete;
    TransientContext& operator=(TransientContext&&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Get the thread local TransientContext
    ///
    /// This per-thread variable tracks if and how a transient
    /// context is currently being used on the current thread
    ///
    /// \return The thread local TransientContext
    ///
    ////////////////////////////////////////////////////////////
    static std::optional<TransientContext>& get()
    {
        thread_local std::optional<TransientContext> transientContext;
        return transientContext;
    }

    ///////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    GraphicsContext&                       graphicsContext;
    std::optional<sf::Context>             context;
    std::unique_lock<std::recursive_mutex> sharedContextLock;
};

} // namespace


// This structure contains all the implementation data we
// don't want to expose through the visible interface
struct GlContext::Impl
{
    ////////////////////////////////////////////////////////////
    /// \brief Constructor
    ///
    ////////////////////////////////////////////////////////////
    Impl()
    {
        auto& weakUnsharedGlObjects = getWeakUnsharedGlObjects();
        unsharedGlObjects           = weakUnsharedGlObjects.lock();

        if (!unsharedGlObjects)
        {
            unsharedGlObjects = std::make_shared<UnsharedGlObjects>();

            weakUnsharedGlObjects = unsharedGlObjects;
        }
    }

    // Structure to track which unshared object belongs to which context
    struct UnsharedGlObject
    {
        std::uint64_t         contextId{};
        std::shared_ptr<void> object;
    };

    using UnsharedGlObjects = std::vector<UnsharedGlObject>;

    ////////////////////////////////////////////////////////////
    /// \brief Get weak_ptr to unshared objects
    ///
    /// \return weak_ptr to unshared objects
    ///
    ////////////////////////////////////////////////////////////
    static std::weak_ptr<UnsharedGlObjects>& getWeakUnsharedGlObjects()
    {
        static std::weak_ptr<UnsharedGlObjects> weakUnsharedGlObjects;
        return weakUnsharedGlObjects;
    }

    ////////////////////////////////////////////////////////////
    /// \brief Get mutex protecting unshared objects
    ///
    /// \return Mutex protecting unshared objects
    ///
    ////////////////////////////////////////////////////////////
    static std::mutex& getUnsharedGlObjectsMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    ///////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    std::shared_ptr<UnsharedGlObjects> unsharedGlObjects; //!< The current object's handle to unshared objects
    const std::uint64_t                id{
        []
        {
            static std::atomic<std::uint64_t> atomicId(1); // start at 1, zero is "no context"
            return atomicId.fetch_add(1);
        }()}; //!< Unique identifier, used for identifying contexts when managing unshareable OpenGL resources
};


////////////////////////////////////////////////////////////
void GlContext::registerUnsharedGlObject(void* objectSharedPtr)
{
    const std::lock_guard lock(Impl::getUnsharedGlObjectsMutex());

    if (const std::shared_ptr unsharedGlObjects = Impl::getWeakUnsharedGlObjects().lock())
        unsharedGlObjects->emplace_back(getActiveContextId(),
                                        SFML_MOVE(*static_cast<std::shared_ptr<void>*>(objectSharedPtr)));
}


////////////////////////////////////////////////////////////
void GlContext::unregisterUnsharedGlObject(void* objectSharedPtr)
{
    const std::lock_guard lock(Impl::getUnsharedGlObjectsMutex());

    if (const std::shared_ptr unsharedGlObjects = Impl::getWeakUnsharedGlObjects().lock())
    {
        const auto currentContextId = getActiveContextId();

        // Find the object in unshared objects and remove it if its associated context is currently active
        // This will trigger the destructor of the object since shared_ptr
        // in unshared objects should be the only one existing
        const auto iter = findIf(unsharedGlObjects->begin(),
                                 unsharedGlObjects->end(),
                                 [&](const Impl::UnsharedGlObject& obj) {
                                     return obj.contextId == currentContextId &&
                                            obj.object == *static_cast<std::shared_ptr<void>*>(objectSharedPtr);
                                 });

        if (iter != unsharedGlObjects->end())
            unsharedGlObjects->erase(iter);
    }
}


////////////////////////////////////////////////////////////
void GlContext::acquireTransientContext(GraphicsContext& graphicsContext)
{
    auto& [id, ptr, transientCount] = GlContextImpl::CurrentContext::get();

    // Fast path if we already have a context active on this thread
    if (id != 0)
    {
        ++transientCount;
        return;
    }

    // If we don't already have a context active on this thread the count should be 0
    assert(transientCount == 0 && "Transient count cannot be non-zero");

    // If currentContextId is not set, this must be the first
    // TransientContextLock on this thread, construct the state object
    assert(!TransientContext::get().has_value());
    TransientContext::get().emplace(graphicsContext);

    // Make sure a context is active at this point
    assert(id != 0 && "Current context ID cannot be zero");
}


////////////////////////////////////////////////////////////
void GlContext::releaseTransientContext()
{
    auto& [id, ptr, transientCount] = GlContextImpl::CurrentContext::get();

    // Make sure a context was left active after acquireTransientContext() was called
    assert(id != 0 && "Current context ID cannot be zero");

    // Fast path if we already had a context active on this thread before acquireTransientContext() was called
    if (transientCount > 0)
    {
        --transientCount;
        return;
    }

    // If currentContextId is set and currentContextTransientCount is 0,
    // this is the last TransientContextLock that is released, destroy the state object
    assert(TransientContext::get().has_value());
    TransientContext::get().reset();
}


////////////////////////////////////////////////////////////
UniquePtr<GlContext> GlContext::create(GraphicsContext& graphicsContext)
{
    const std::lock_guard lock(graphicsContext.getMutex());

    // We don't use acquireTransientContext here since we have
    // to ensure we have exclusive access to the shared context
    // in order to make sure it is not active during context creation
    if (!graphicsContext.setActive(true))
        err() << "Error enablind shared context in GlContext::create()" << errEndl;

    // Create the context
    UniquePtr<GlContext> context;
    graphicsContext.makeContextType(context);

    if (!graphicsContext.setActive(false))
        err() << "Error disabling shared context in GlContext::create()" << errEndl;

    if (!context->initialize(graphicsContext, ContextSettings{}))
    {
        err() << "Could not initialize  context in GlContext::create()" << errEndl;
        return nullptr;
    }

    return context;
}


////////////////////////////////////////////////////////////
UniquePtr<GlContext> GlContext::create(GraphicsContext&       graphicsContext,
                                       const ContextSettings& settings,
                                       const WindowImpl&      owner,
                                       unsigned int           bitsPerPixel)
{
    const std::lock_guard lock(graphicsContext.getMutex());

    // TODO: ?
    // If use_count is 2 (GlResource + sharedContext) we know that we are inside sf::Context or sf::Window
    // Only in this situation we allow the user to indirectly re-create the shared context as a core context

    // // Check if we need to convert our shared context into a core context
    // if ((SharedContext::getUseCount() == 2) && (settings.attributeFlags & ContextSettings::Core) &&
    //     !(sharedContext.context->m_settings.attributeFlags & ContextSettings::Core))
    // {
    //     // Re-create our shared context as a core context
    //     const ContextSettings sharedSettings{/* depthBits */ 0,
    //                                          /* stencilBits */ 0,
    //                                          /* antialiasingLevel */ 0,
    //                                          settings.majorVersion,
    //                                          settings.minorVersion,
    //                                          settings.attributeFlags};

    //     sharedContext.context.emplace(nullptr, sharedSettings, Vector2u{1, 1});
    //     if (!sharedContext.context->initialize(sharedSettings))
    //     {
    //         err() << "Could not initialize shared context in GlContext::create()" << errEndl;
    //         return nullptr;
    //     }

    //     // Reload our extensions vector
    //     sharedContext.loadExtensions();
    // }

    // We don't use acquireTransientContext here since we have
    // to ensure we have exclusive access to the shared context
    // in order to make sure it is not active during context creation
    if (!graphicsContext.setActive(true))
        err() << "Error enabling shared context in GlContext::create()" << errEndl;

    // Create the context
    UniquePtr<GlContext> context;
    graphicsContext.makeContextType(context, settings, owner, bitsPerPixel);

    if (!graphicsContext.setActive(false))
        err() << "Error disabling shared context in GlContext::create()" << errEndl;

    if (!context->initialize(graphicsContext, settings))
    {
        err() << "Could not initialize context in GlContext::create()" << errEndl;
        return nullptr;
    }

    context->checkSettings(settings);

    return context;
}


////////////////////////////////////////////////////////////
UniquePtr<GlContext> GlContext::create(GraphicsContext& graphicsContext, const ContextSettings& settings, const Vector2u& size)
{
    const std::lock_guard lock(graphicsContext.getMutex());

    // TODO: ?
    // If use_count is 2 (GlResource + sharedContext) we know that we are inside sf::Context or sf::Window
    // Only in this situation we allow the user to indirectly re-create the shared context as a core context

    // Check if we need to convert our shared context into a core context
    // if ((SharedContext::getUseCount() == 2) && (settings.attributeFlags & ContextSettings::Core) &&
    //     !(sharedContext.context->m_settings.attributeFlags & ContextSettings::Core))
    // {
    //     // Re-create our shared context as a core context
    //     const ContextSettings sharedSettings{/* depthBits */ 0,
    //                                          /* stencilBits */ 0,
    //                                          /* antialiasingLevel */ 0,
    //                                          settings.majorVersion,
    //                                          settings.minorVersion,
    //                                          settings.attributeFlags};

    //     sharedContext.context.emplace(nullptr, sharedSettings, Vector2u{1, 1});
    //     if (!sharedContext.context->initialize(sharedSettings))
    //     {
    //         err() << "Could not initialize shared context in GlContext::create()" << errEndl;
    //         return nullptr;
    //     }

    //     // Reload our extensions vector
    //     sharedContext.loadExtensions();
    // }

    // We don't use acquireTransientContext here since we have
    // to ensure we have exclusive access to the shared context
    // in order to make sure it is not active during context creation
    if (!graphicsContext.setActive(true))
        err() << "Error enabling shared context in GlContext::create()" << errEndl;

    // Create the context
    UniquePtr<GlContext> context;
    graphicsContext.makeContextType(context, settings, size);

    if (!graphicsContext.setActive(false))
        err() << "Error disabling shared context in GlContext::create()" << errEndl;

    if (!context->initialize(graphicsContext, settings))
    {
        err() << "Could not initialize  context in GlContext::create()" << errEndl;
        return nullptr;
    }

    context->checkSettings(settings);

    return context;
}


////////////////////////////////////////////////////////////
bool GlContext::isExtensionAvailable(GraphicsContext& graphicsContext, const char* name)
{
    return graphicsContext.isExtensionAvailable(name);
}


////////////////////////////////////////////////////////////
GlFunctionPointer GlContext::getFunction(GraphicsContext& graphicsContext, const char* name)
{
    // We can't and don't need to lock when we are currently creating the shared context
    std::unique_lock<std::recursive_mutex> lock(graphicsContext.getMutex());
    return ContextType::getFunction(name);
}


////////////////////////////////////////////////////////////
const GlContext* GlContext::getActiveContext()
{
    return GlContextImpl::CurrentContext::get().ptr;
}


////////////////////////////////////////////////////////////
std::uint64_t GlContext::getActiveContextId()
{
    return GlContextImpl::CurrentContext::get().id;
}


////////////////////////////////////////////////////////////
bool GlContext::hasActiveContext()
{
    return getActiveContextId() != 0;
}


////////////////////////////////////////////////////////////
GlContext::~GlContext()
{
    auto& [id, ptr, transientCount] = GlContextImpl::CurrentContext::get();

    if (m_impl->id == id)
    {
        id  = 0;
        ptr = nullptr;
    }
}


////////////////////////////////////////////////////////////
const ContextSettings& GlContext::getSettings() const
{
    return m_settings;
}


////////////////////////////////////////////////////////////
bool GlContext::setActive(GraphicsContext& graphicsContext, bool active)
{
    auto& [id, ptr, transientCount] = GlContextImpl::CurrentContext::get();

    // If this context is already the active one on this thread, don't do anything
    if (active && m_impl->id == id)
        return true;

    // If this context is not the active one on this thread, don't do anything
    if (!active && m_impl->id != id)
        return true;

    // Make sure we don't try to create the shared context here since
    // setActive can be called during construction and lead to infinite recursion

    const auto activate = [&]
    {
        // We can't and don't need to lock when we are currently creating the shared context
        std::unique_lock<std::recursive_mutex> lock(graphicsContext.getMutex());

        // Activate the context
        if (!makeCurrent(true))
        {
            err() << "makeCurrent(true) failure in GlContext::setActive" << errEndl;
            return false;
        }

        // Set it as the new current context for this thread
        id  = m_impl->id;
        ptr = this;

        return true;
    };

    const auto deactivate = [&]
    {
        // We can't and don't need to lock when we are currently creating the shared context
        std::unique_lock<std::recursive_mutex> lock(graphicsContext.getMutex());

        // Deactivate the context
        if (!makeCurrent(false))
        {
            err() << "makeCurrent(false) failure in GlContext::setActive" << errEndl;
            return false;
        }

        id  = 0;
        ptr = nullptr;

        return true;
    };

    return active ? activate() : deactivate();
}


////////////////////////////////////////////////////////////
GlContext::GlContext(const ContextSettings& settings) : m_settings(settings)
{
}


////////////////////////////////////////////////////////////
int GlContext::evaluateFormat(
    unsigned int           bitsPerPixel,
    const ContextSettings& settings,
    int                    colorBits,
    int                    depthBits,
    int                    stencilBits,
    int                    antialiasing,
    bool                   accelerated,
    bool                   sRgb)
{
    int colorDiff        = static_cast<int>(bitsPerPixel) - colorBits;
    int depthDiff        = static_cast<int>(settings.depthBits) - depthBits;
    int stencilDiff      = static_cast<int>(settings.stencilBits) - stencilBits;
    int antialiasingDiff = static_cast<int>(settings.antialiasingLevel) - antialiasing;

    // Weight sub-scores so that better settings don't score equally as bad as worse settings
    colorDiff *= ((colorDiff > 0) ? 100000 : 1);
    depthDiff *= ((depthDiff > 0) ? 100000 : 1);
    stencilDiff *= ((stencilDiff > 0) ? 100000 : 1);
    antialiasingDiff *= ((antialiasingDiff > 0) ? 100000 : 1);

    // Aggregate the scores
    int score = std::abs(colorDiff) + std::abs(depthDiff) + std::abs(stencilDiff) + std::abs(antialiasingDiff);

    // If the user wants an sRGB capable format, try really hard to get one
    if (settings.sRgbCapable && !sRgb)
        score += 10000000;

    // Make sure we prefer hardware acceleration over features
    if (!accelerated)
        score += 100000000;

    return score;
}


////////////////////////////////////////////////////////////
void GlContext::cleanupUnsharedResources(GraphicsContext& graphicsContext)
{
    const auto& [id, ptr, transientCount] = GlContextImpl::CurrentContext::get();

    // Save the current context so we can restore it later
    GlContext* contextToRestore = ptr;

    // If this context is already active there is no need to save it
    if (m_impl->id == id)
        contextToRestore = nullptr;

    // Make this context active so resources can be freed
    if (!setActive(graphicsContext, true))
        err() << "Could not enable context in GlContext::cleanupUnsharedResources()" << errEndl;

    {
        const std::lock_guard lock(Impl::getUnsharedGlObjectsMutex());

        // Destroy the unshared objects contained in this context
        for (auto iter = m_impl->unsharedGlObjects->begin(); iter != m_impl->unsharedGlObjects->end();)
        {
            if (iter->contextId == m_impl->id)
            {
                iter = m_impl->unsharedGlObjects->erase(iter);
            }
            else
            {
                ++iter;
            }
        }
    }

    // Make the originally active context active again
    if (contextToRestore)
        if (!contextToRestore->setActive(graphicsContext, true))
            err() << "Could not restore context in GlContext::cleanupUnsharedResources()" << errEndl;
}


////////////////////////////////////////////////////////////
bool GlContext::initialize(GraphicsContext& graphicsContext, const ContextSettings& requestedSettings)
{
    // Activate the context
    if (!setActive(graphicsContext, true))
        err() << "Error enabling context in GlContext::initalize()" << errEndl;

    // Retrieve the context version number
    int majorVersion = 0;
    int minorVersion = 0;

    // Try the new way first
    auto glGetIntegervFunc = reinterpret_cast<glGetIntegervFuncType>(getFunction(graphicsContext, "glGetIntegerv"));
    auto glGetErrorFunc    = reinterpret_cast<glGetErrorFuncType>(getFunction(graphicsContext, "glGetError"));
    auto glGetStringFunc   = reinterpret_cast<glGetStringFuncType>(getFunction(graphicsContext, "glGetString"));
    auto glEnableFunc      = reinterpret_cast<glEnableFuncType>(getFunction(graphicsContext, "glEnable"));
    auto glIsEnabledFunc   = reinterpret_cast<glIsEnabledFuncType>(getFunction(graphicsContext, "glIsEnabled"));

    if (!glGetIntegervFunc || !glGetErrorFunc || !glGetStringFunc || !glEnableFunc || !glIsEnabledFunc)
    {
        err() << "Could not load necessary function to initialize OpenGL context" << errEndl;
        return false;
    }

    glGetIntegervFunc(GL_MAJOR_VERSION, &majorVersion);
    glGetIntegervFunc(GL_MINOR_VERSION, &minorVersion);

    if (glGetErrorFunc() != GL_INVALID_ENUM)
    {
        m_settings.majorVersion = static_cast<unsigned int>(majorVersion);
        m_settings.minorVersion = static_cast<unsigned int>(minorVersion);
    }
    else
    {
        // Try the old way

        // If we can't get the version number, assume 1.1
        m_settings.majorVersion = 1;
        m_settings.minorVersion = 1;

        const char* version = reinterpret_cast<const char*>(glGetStringFunc(GL_VERSION));
        if (version)
        {
            // OpenGL ES Common Lite profile: The beginning of the returned string is "OpenGL ES-CL major.minor"
            // OpenGL ES Common profile:      The beginning of the returned string is "OpenGL ES-CM major.minor"
            // OpenGL ES Full profile:        The beginning of the returned string is "OpenGL ES major.minor"
            // Desktop OpenGL:                The beginning of the returned string is "major.minor"

            // Helper to parse OpenGL version strings
            static const auto parseVersionString =
                [](const char* versionString, const char* prefix, unsigned int& major, unsigned int& minor)
            {
                const std::size_t prefixLength = std::strlen(prefix);

                if ((std::strlen(versionString) >= (prefixLength + 3)) &&
                    (std::strncmp(versionString, prefix, prefixLength) == 0) && std::isdigit(versionString[prefixLength]) &&
                    (versionString[prefixLength + 1] == '.') && std::isdigit(versionString[prefixLength + 2]))
                {
                    major = static_cast<unsigned int>(versionString[prefixLength] - '0');
                    minor = static_cast<unsigned int>(versionString[prefixLength + 2] - '0');

                    return true;
                }

                return false;
            };

            if (!parseVersionString(version, "OpenGL ES-CL ", m_settings.majorVersion, m_settings.minorVersion) &&
                !parseVersionString(version, "OpenGL ES-CM ", m_settings.majorVersion, m_settings.minorVersion) &&
                !parseVersionString(version, "OpenGL ES ", m_settings.majorVersion, m_settings.minorVersion) &&
                !parseVersionString(version, "", m_settings.majorVersion, m_settings.minorVersion))
            {
                err() << "Unable to parse OpenGL version string: \"" << version << '"' << ", defaulting to 1.1" << errEndl;
            }
        }
        else
        {
            err() << "Unable to retrieve OpenGL version string, defaulting to 1.1" << errEndl;
        }
    }

    // 3.0 contexts only deprecate features, but do not remove them yet
    // 3.1 contexts remove features if ARB_compatibility is not present
    // 3.2+ contexts remove features only if a core profile is requested

    // If the context was created with wglCreateContext, it is guaranteed to be compatibility.
    // If a 3.0 context was created with wglCreateContextAttribsARB, it is guaranteed to be compatibility.
    // If a 3.1 context was created with wglCreateContextAttribsARB, the compatibility flag
    // is set only if ARB_compatibility is present
    // If a 3.2+ context was created with wglCreateContextAttribsARB, the compatibility flag
    // would have been set correctly already depending on whether ARB_create_context_profile is supported.

    // If the user requests a 3.0 context, it will be a compatibility context regardless of the requested profile.
    // If the user requests a 3.1 context and its creation was successful, the specification
    // states that it will not be a compatibility profile context regardless of the requested
    // profile unless ARB_compatibility is present.

    m_settings.attributeFlags = ContextSettings::Default;

    if (m_settings.majorVersion >= 3)
    {
        // Retrieve the context flags
        int flags = 0;
        glGetIntegervFunc(GL_CONTEXT_FLAGS, &flags);

        if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
            m_settings.attributeFlags |= ContextSettings::Debug;

        if ((m_settings.majorVersion == 3) && (m_settings.minorVersion == 1))
        {
            m_settings.attributeFlags |= ContextSettings::Core;

            auto glGetStringiFunc = reinterpret_cast<glGetStringiFuncType>(getFunction(graphicsContext, "glGetStringi"));

            if (glGetStringiFunc)
            {
                int numExtensions = 0;
                glGetIntegervFunc(GL_NUM_EXTENSIONS, &numExtensions);

                for (unsigned int i = 0; i < static_cast<unsigned int>(numExtensions); ++i)
                {
                    const char* extensionString = reinterpret_cast<const char*>(glGetStringiFunc(GL_EXTENSIONS, i));

                    if (std::strstr(extensionString, "GL_ARB_compatibility"))
                    {
                        m_settings.attributeFlags &= ~static_cast<std::uint32_t>(ContextSettings::Core);
                        break;
                    }
                }
            }
        }
        else if ((m_settings.majorVersion > 3) || (m_settings.minorVersion >= 2))
        {
            // Retrieve the context profile
            int profile = 0;
            glGetIntegervFunc(GL_CONTEXT_PROFILE_MASK, &profile);

            if (profile & GL_CONTEXT_CORE_PROFILE_BIT)
                m_settings.attributeFlags |= ContextSettings::Core;
        }
    }

    // Enable anti-aliasing if requested by the user and supported
    if ((requestedSettings.antialiasingLevel > 0) && (m_settings.antialiasingLevel > 0))
    {
        glEnableFunc(GL_MULTISAMPLE);
    }
    else
    {
        m_settings.antialiasingLevel = 0;
    }

    // Enable sRGB if requested by the user and supported
    if (requestedSettings.sRgbCapable && m_settings.sRgbCapable)
    {
        glEnableFunc(GL_FRAMEBUFFER_SRGB);

        // Check to see if the enable was successful
        if (glIsEnabledFunc(GL_FRAMEBUFFER_SRGB) == GL_FALSE)
        {
            err() << "Warning: Failed to enable GL_FRAMEBUFFER_SRGB" << errEndl;
            m_settings.sRgbCapable = false;
        }
    }
    else
    {
        m_settings.sRgbCapable = false;
    }

    return true;
}


////////////////////////////////////////////////////////////
void GlContext::checkSettings(const ContextSettings& requestedSettings) const
{
    // Perform checks to inform the user if they are getting a context they might not have expected
    const int version = static_cast<int>(m_settings.majorVersion * 10u + m_settings.minorVersion);
    const int requestedVersion = static_cast<int>(requestedSettings.majorVersion * 10u + requestedSettings.minorVersion);

    if ((m_settings.attributeFlags != requestedSettings.attributeFlags) || (version < requestedVersion) ||
        (m_settings.stencilBits < requestedSettings.stencilBits) ||
        (m_settings.antialiasingLevel < requestedSettings.antialiasingLevel) ||
        (m_settings.depthBits < requestedSettings.depthBits) || (!m_settings.sRgbCapable && requestedSettings.sRgbCapable))
    {
        err() << "Warning: The created OpenGL context does not fully meet the settings that were requested" << '\n'
              << "Requested: version = " << requestedSettings.majorVersion << "." << requestedSettings.minorVersion
              << " ; depth bits = " << requestedSettings.depthBits << " ; stencil bits = " << requestedSettings.stencilBits
              << " ; AA level = " << requestedSettings.antialiasingLevel << std::boolalpha
              << " ; core = " << ((requestedSettings.attributeFlags & ContextSettings::Core) != 0)
              << " ; debug = " << ((requestedSettings.attributeFlags & ContextSettings::Debug) != 0)
              << " ; sRGB = " << requestedSettings.sRgbCapable << std::noboolalpha << '\n'
              << "Created: version = " << m_settings.majorVersion << "." << m_settings.minorVersion
              << " ; depth bits = " << m_settings.depthBits << " ; stencil bits = " << m_settings.stencilBits
              << " ; AA level = " << m_settings.antialiasingLevel << std::boolalpha
              << " ; core = " << ((m_settings.attributeFlags & ContextSettings::Core) != 0)
              << " ; debug = " << ((m_settings.attributeFlags & ContextSettings::Debug) != 0)
              << " ; sRGB = " << m_settings.sRgbCapable << std::noboolalpha << errEndl;
    }
}

} // namespace sf::priv
