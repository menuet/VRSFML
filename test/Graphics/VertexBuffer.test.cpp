#include <SFML/Graphics/VertexBuffer.hpp>

#include <SFML/Window/GraphicsContext.hpp>

// Other 1st party headers
#include <SFML/Graphics/Vertex.hpp>

#include <Doctest.hpp>

#include <GraphicsUtil.hpp>

#include <type_traits>

// Skip these tests with [.display] because they produce flakey failures in CI when using xvfb-run
TEST_CASE("[Graphics] sf::VertexBuffer", "[.display]")
{
    sf::GraphicsContext graphicsContext;

    SECTION("Type traits")
    {
        STATIC_CHECK(std::is_copy_constructible_v<sf::VertexBuffer>);
        STATIC_CHECK(std::is_copy_assignable_v<sf::VertexBuffer>);
        STATIC_CHECK(std::is_move_constructible_v<sf::VertexBuffer>);
        STATIC_CHECK(!std::is_nothrow_move_constructible_v<sf::VertexBuffer>);
        STATIC_CHECK(std::is_move_assignable_v<sf::VertexBuffer>);
        STATIC_CHECK(!std::is_nothrow_move_assignable_v<sf::VertexBuffer>);
        STATIC_CHECK(std::is_nothrow_swappable_v<sf::VertexBuffer>);
    }

    // Skip tests if vertex buffers aren't available
    if (!sf::VertexBuffer::isAvailable(graphicsContext))
        return;

    SECTION("Construction")
    {
        SECTION("Default constructor")
        {
            const sf::VertexBuffer vertexBuffer(graphicsContext);
            CHECK(vertexBuffer.getVertexCount() == 0);
            CHECK(vertexBuffer.getNativeHandle() == 0);
            CHECK(vertexBuffer.getPrimitiveType() == sf::PrimitiveType::Points);
            CHECK(vertexBuffer.getUsage() == sf::VertexBuffer::Usage::Stream);
        }

        SECTION("Primitive type constructor")
        {
            const sf::VertexBuffer vertexBuffer(graphicsContext, sf::PrimitiveType::Triangles);
            CHECK(vertexBuffer.getVertexCount() == 0);
            CHECK(vertexBuffer.getNativeHandle() == 0);
            CHECK(vertexBuffer.getPrimitiveType() == sf::PrimitiveType::Triangles);
            CHECK(vertexBuffer.getUsage() == sf::VertexBuffer::Usage::Stream);
        }

        SECTION("Usage constructor")
        {
            const sf::VertexBuffer vertexBuffer(graphicsContext, sf::VertexBuffer::Usage::Static);
            CHECK(vertexBuffer.getVertexCount() == 0);
            CHECK(vertexBuffer.getNativeHandle() == 0);
            CHECK(vertexBuffer.getPrimitiveType() == sf::PrimitiveType::Points);
            CHECK(vertexBuffer.getUsage() == sf::VertexBuffer::Usage::Static);
        }

        SECTION("Primitive type and usage constructor")
        {
            const sf::VertexBuffer vertexBuffer(graphicsContext, sf::PrimitiveType::LineStrip, sf::VertexBuffer::Usage::Dynamic);
            CHECK(vertexBuffer.getVertexCount() == 0);
            CHECK(vertexBuffer.getNativeHandle() == 0);
            CHECK(vertexBuffer.getPrimitiveType() == sf::PrimitiveType::LineStrip);
            CHECK(vertexBuffer.getUsage() == sf::VertexBuffer::Usage::Dynamic);
        }
    }

    SECTION("Copy semantics")
    {
        const sf::VertexBuffer vertexBuffer(graphicsContext, sf::PrimitiveType::LineStrip, sf::VertexBuffer::Usage::Dynamic);

        SECTION("Construction")
        {
            const sf::VertexBuffer vertexBufferCopy(vertexBuffer); // NOLINT(performance-unnecessary-copy-initialization)
            CHECK(vertexBufferCopy.getVertexCount() == 0);
            CHECK(vertexBufferCopy.getNativeHandle() == 0);
            CHECK(vertexBufferCopy.getPrimitiveType() == sf::PrimitiveType::LineStrip);
            CHECK(vertexBufferCopy.getUsage() == sf::VertexBuffer::Usage::Dynamic);
        }

        SECTION("Assignment")
        {
            sf::VertexBuffer vertexBufferCopy(graphicsContext);
            vertexBufferCopy = vertexBuffer;
            CHECK(vertexBufferCopy.getVertexCount() == 0);
            CHECK(vertexBufferCopy.getNativeHandle() == 0);
            CHECK(vertexBufferCopy.getPrimitiveType() == sf::PrimitiveType::LineStrip);
            CHECK(vertexBufferCopy.getUsage() == sf::VertexBuffer::Usage::Dynamic);
        }
    }

    SECTION("create()")
    {
        sf::VertexBuffer vertexBuffer(graphicsContext);
        CHECK(vertexBuffer.create(100));
        CHECK(vertexBuffer.getVertexCount() == 100);
    }

    SECTION("update()")
    {
        sf::VertexBuffer vertexBuffer(graphicsContext);
        sf::Vertex       vertices[128]{};

        SECTION("Vertices")
        {
            SECTION("Uninitialized buffer")
            {
                CHECK(!vertexBuffer.update(vertices));
            }

            CHECK(vertexBuffer.create(128));

            SECTION("Null vertices")
            {
                CHECK(!vertexBuffer.update(nullptr));
            }

            CHECK(vertexBuffer.update(vertices));
            CHECK(vertexBuffer.getVertexCount() == 128);
            CHECK(vertexBuffer.getNativeHandle() != 0);
        }

        SECTION("Vertices, count, and offset")
        {
            CHECK(vertexBuffer.create(128));

            SECTION("Count + offset too large")
            {
                CHECK(!vertexBuffer.update(vertices, 100, 100));
            }

            CHECK(vertexBuffer.update(vertices, 128, 0));
            CHECK(vertexBuffer.getVertexCount() == 128);
        }

        SECTION("Another buffer")
        {
            sf::VertexBuffer otherVertexBuffer(graphicsContext);

            CHECK(!vertexBuffer.update(otherVertexBuffer));
            CHECK(otherVertexBuffer.create(42));
            CHECK(!vertexBuffer.update(otherVertexBuffer));
        }
    }

    SECTION("swap()")
    {
        sf::VertexBuffer vertexBuffer1(graphicsContext, sf::PrimitiveType::LineStrip, sf::VertexBuffer::Usage::Dynamic);
        CHECK(vertexBuffer1.create(50));

        sf::VertexBuffer vertexBuffer2(graphicsContext, sf::PrimitiveType::TriangleStrip, sf::VertexBuffer::Usage::Stream);
        CHECK(vertexBuffer2.create(60));

        sf::swap(vertexBuffer1, vertexBuffer2);

        CHECK(vertexBuffer1.getVertexCount() == 60);
        CHECK(vertexBuffer1.getNativeHandle() != 0);
        CHECK(vertexBuffer1.getPrimitiveType() == sf::PrimitiveType::TriangleStrip);
        CHECK(vertexBuffer1.getUsage() == sf::VertexBuffer::Usage::Stream);

        CHECK(vertexBuffer2.getVertexCount() == 50);
        CHECK(vertexBuffer2.getNativeHandle() != 0);
        CHECK(vertexBuffer2.getPrimitiveType() == sf::PrimitiveType::LineStrip);
        CHECK(vertexBuffer2.getUsage() == sf::VertexBuffer::Usage::Dynamic);
    }

    SECTION("Set/get primitive type")
    {
        sf::VertexBuffer vertexBuffer(graphicsContext);
        vertexBuffer.setPrimitiveType(sf::PrimitiveType::TriangleFan);
        CHECK(vertexBuffer.getPrimitiveType() == sf::PrimitiveType::TriangleFan);
    }

    SECTION("Set/get usage")
    {
        sf::VertexBuffer vertexBuffer(graphicsContext);
        vertexBuffer.setUsage(sf::VertexBuffer::Usage::Dynamic);
        CHECK(vertexBuffer.getUsage() == sf::VertexBuffer::Usage::Dynamic);
    }
}
