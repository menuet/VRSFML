#include <SFML/Copyright.hpp> // LICENSE AND COPYRIGHT (C) INFORMATION

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "SFML/Graphics/Transform.hpp" // NOLINT(misc-header-include-cycle)

#include "SFML/Base/SizeT.hpp"


namespace sf
{
////////////////////////////////////////////////////////////
// clang-format off
constexpr Transform::Transform(float a00, float a01, float a02,
                               float a10, float a11, float a12)
    : m_matrix{a00, a10, 0.f, 0.f,
               a01, a11, 0.f, 0.f,
               0.f, 0.f, 1.f, 0.f,
               a02, a12, 0.f, 1.f}
{
}
// clang-format on


////////////////////////////////////////////////////////////
constexpr const float* Transform::getMatrix() const
{
    return m_matrix;
}


////////////////////////////////////////////////////////////
constexpr Transform Transform::getInverse() const
{
    // clang-format off
    // Compute the determinant
    const float det = m_matrix[0] * m_matrix[5] - m_matrix[1] * m_matrix[4];
    // clang-format on

    // Compute the inverse if the determinant is not zero
    // (don't use an epsilon because the determinant may *really* be tiny)
    if (det != 0.f)
    {
        // clang-format off
		return {(               m_matrix[5]                             ) / det,
               -(               m_matrix[4]                             ) / det,
				(m_matrix[13] * m_matrix[4] - m_matrix[5] * m_matrix[12]) / det,
               -(               m_matrix[1]                             ) / det,
                (               m_matrix[0]                             ) / det,
               -(m_matrix[13] * m_matrix[0] - m_matrix[1] * m_matrix[12]) / det};
        // clang-format on
    }

    return Identity;
}


////////////////////////////////////////////////////////////
constexpr Vector2f Transform::transformPoint(Vector2f point) const
{
    return {m_matrix[0] * point.x + m_matrix[4] * point.y + m_matrix[12],
            m_matrix[1] * point.x + m_matrix[5] * point.y + m_matrix[13]};
}


////////////////////////////////////////////////////////////
constexpr FloatRect Transform::transformRect(const FloatRect& rectangle) const
{
    // Transform the 4 corners of the rectangle
    const Vector2f points[] = {transformPoint(rectangle.position),
                               transformPoint(rectangle.position + Vector2f(0.f, rectangle.size.y)),
                               transformPoint(rectangle.position + Vector2f(rectangle.size.x, 0.f)),
                               transformPoint(rectangle.position + rectangle.size)};

    // Compute the bounding rectangle of the transformed points
    Vector2f pmin = points[0];
    Vector2f pmax = points[0];

    for (base::SizeT i = 1; i < 4; ++i)
    {
        // clang-format off
        if      (points[i].x < pmin.x) pmin.x = points[i].x;
        else if (points[i].x > pmax.x) pmax.x = points[i].x;

        if      (points[i].y < pmin.y) pmin.y = points[i].y;
        else if (points[i].y > pmax.y) pmax.y = points[i].y;
        // clang-format on
    }

    return {pmin, pmax - pmin};
}


////////////////////////////////////////////////////////////
constexpr Transform& Transform::combine(const Transform& transform)
{
    *this = sf::operator*(*this, transform);
    return *this;
}


////////////////////////////////////////////////////////////
constexpr Transform& Transform::translate(Vector2f offset)
{
    // clang-format off
    const Transform translation(1, 0, offset.x,
                                0, 1, offset.y);
    // clang-format on

    return combine(translation);
}


////////////////////////////////////////////////////////////
constexpr Transform& Transform::scale(Vector2f factors)
{
    // clang-format off
    const Transform scaling(factors.x, 0,         0,
                            0,         factors.y, 0);
    // clang-format on

    return combine(scaling);
}


////////////////////////////////////////////////////////////
constexpr Transform& Transform::scale(Vector2f factors, Vector2f center)
{
    // clang-format off
    const Transform scaling(factors.x, 0,         center.x * (1 - factors.x),
                            0,         factors.y, center.y * (1 - factors.y));
    // clang-format on

    return combine(scaling);
}


////////////////////////////////////////////////////////////
constexpr Transform operator*(const Transform& left, const Transform& right)
{
    const float* a = left.getMatrix();
    const float* b = right.getMatrix();

    // clang-format off
    return {a[0] * b[0]  + a[4] * b[1],
            a[0] * b[4]  + a[4] * b[5],
            a[0] * b[12] + a[4] * b[13] + a[12],
            a[1] * b[0]  + a[5] * b[1],
            a[1] * b[4]  + a[5] * b[5],
            a[1] * b[12] + a[5] * b[13] + a[13]};
    // clang-format on
}


////////////////////////////////////////////////////////////
constexpr Transform& operator*=(Transform& left, const Transform& right)
{
    return left.combine(right);
}


////////////////////////////////////////////////////////////
constexpr Vector2f operator*(const Transform& left, Vector2f right)
{
    return left.transformPoint(right);
}


////////////////////////////////////////////////////////////
constexpr bool operator==(const Transform& left, const Transform& right)
{
    const float* a = left.getMatrix();
    const float* b = right.getMatrix();

    // clang-format off
    return ((a[0]  == b[0])  && (a[1]  == b[1])
	     && (a[4]  == b[4])  && (a[5]  == b[5])
		 && (a[12] == b[12]) && (a[13] == b[13]));
    // clang-format on
}


////////////////////////////////////////////////////////////
constexpr bool operator!=(const Transform& left, const Transform& right)
{
    return !(left == right);
}


////////////////////////////////////////////////////////////
// Static member data
////////////////////////////////////////////////////////////

// Note: the 'inline' keyword here is technically not required, but VS2019 fails
// to compile with a bogus "multiple definition" error if not explicitly used.
inline constexpr Transform Transform::Identity;

} // namespace sf
