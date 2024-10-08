#pragma once
#include <SFML/Copyright.hpp> // LICENSE AND COPYRIGHT (C) INFORMATION

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "SFML/Graphics/Export.hpp"

#include "SFML/Graphics/BlendMode.hpp"
#include "SFML/Graphics/CoordinateType.hpp"
#include "SFML/Graphics/StencilMode.hpp"
#include "SFML/Graphics/Transform.hpp"


namespace sf
{
class Shader;
class Texture;

////////////////////////////////////////////////////////////
/// \brief Define the states used for drawing to a RenderTarget
///
////////////////////////////////////////////////////////////
struct [[nodiscard]] SFML_GRAPHICS_API RenderStates
{
    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// Constructing a default set of render states is equivalent
    /// to using sf::RenderStates::Default.
    /// The default set defines:
    /// \li the BlendAlpha blend mode
    /// \li the default StencilMode (no stencil)
    /// \li the identity transform
    /// \li a null texture
    /// \li a null shader
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] explicit RenderStates() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Construct a default set of render states with a custom blend mode
    ///
    /// \param theBlendMode Blend mode to use
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] explicit RenderStates(const BlendMode& theBlendMode);

    ////////////////////////////////////////////////////////////
    /// \brief Construct a default set of render states with a custom stencil mode
    ///
    /// \param theStencilMode Stencil mode to use
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] explicit RenderStates(const StencilMode& theStencilMode);

    ////////////////////////////////////////////////////////////
    /// \brief Construct a default set of render states with a custom transform
    ///
    /// \param theTransform Transform to use
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] explicit RenderStates(const Transform& theTransform);

    ////////////////////////////////////////////////////////////
    /// \brief Construct a default set of render states with a custom texture
    ///
    /// \param theTexture Texture to use
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] explicit RenderStates(const Texture* theTexture);

    ////////////////////////////////////////////////////////////
    /// \brief Construct a default set of render states with a custom shader
    ///
    /// \param theShader Shader to use
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] explicit RenderStates(const Shader* theShader);

    ////////////////////////////////////////////////////////////
    /// \brief Construct a set of render states with all its attributes
    ///
    /// \param theBlendMode      Blend mode to use
    /// \param theStencilMode    Stencil mode to use
    /// \param theTransform      Transform to use
    /// \param theCoordinateType Texture coordinate type to use
    /// \param theTexture        Texture to use
    /// \param theShader         Shader to use
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] explicit RenderStates(
        const BlendMode&   theBlendMode,
        const StencilMode& theStencilMode,
        const Transform&   theTransform,
        CoordinateType     theCoordinateType,
        const Texture*     theTexture,
        const Shader*      theShader);

    ////////////////////////////////////////////////////////////
    // Static member data
    ////////////////////////////////////////////////////////////
    // NOLINTNEXTLINE(readability-identifier-naming)
    static const RenderStates Default; //!< Special instance holding the default render states

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    BlendMode      blendMode{BlendAlpha};                  //!< Blending mode
    StencilMode    stencilMode;                            //!< Stencil mode
    Transform      transform;                              //!< Transform
    CoordinateType coordinateType{CoordinateType::Pixels}; //!< Texture coordinate type
    const Texture* texture{};                              //!< Texture
    const Shader*  shader{};                               //!< Shader
};

} // namespace sf


////////////////////////////////////////////////////////////
/// \class sf::RenderStates
/// \ingroup graphics
///
/// There are six global states that can be applied to
/// the drawn objects:
/// \li the blend mode: how pixels of the object are blended with the background
/// \li the stencil mode: how pixels of the object interact with the stencil buffer
/// \li the transform: how the object is positioned/rotated/scaled
/// \li the texture coordinate type: how texture coordinates are interpreted
/// \li the texture: what image is mapped to the object
/// \li the shader: what custom effect is applied to the object
///
/// High-level objects such as sprites or text force some of
/// these states when they are drawn. For example, a sprite
/// will set its own texture, so that you don't have to care
/// about it when drawing the sprite.
///
/// The transform is a special case: sprites, texts and shapes
/// (and it's a good idea to do it with your own drawable classes
/// too) combine their transform with the one that is passed in the
/// RenderStates structure. So that you can use a "global" transform
/// on top of each object's transform.
///
/// Most objects, especially high-level drawables, can be drawn
/// directly without defining render states explicitly -- the
/// default set of states is ok in most cases.
/// \code
/// window.draw(sprite);
/// \endcode
///
/// If you want to use a single specific render state,
/// for example a shader, you can pass it directly to the Draw
/// function: sf::RenderStates has an implicit one-argument
/// constructor for each state.
/// \code
/// window.draw(sprite, shader);
/// \endcode
///
/// When you're inside the draw function of a drawable
/// object, you can either pass the render states unmodified,
/// or change some of them.
/// For example, a transformable object will combine the
/// current transform with its own transform. A sprite will
/// set its texture. Etc.
///
/// \see sf::RenderTarget
///
////////////////////////////////////////////////////////////
