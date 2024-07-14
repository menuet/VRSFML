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

#pragma once


#if __has_builtin(__remove_cvref)

////////////////////////////////////////////////////////////
#define SFML_BASE_REMOVE_CVREF(...) __remove_cvref(__VA_ARGS__)

#else

namespace sf::base::priv
{
////////////////////////////////////////////////////////////
// clang-format off
template <typename T> struct RemoveCVRefImpl                     { using type = T; };
template <typename T> struct RemoveCVRefImpl<T&>                 { using type = T; };
template <typename T> struct RemoveCVRefImpl<T&&>                { using type = T; };
template <typename T> struct RemoveCVRefImpl<const T>            { using type = T; };
template <typename T> struct RemoveCVRefImpl<const T&>           { using type = T; };
template <typename T> struct RemoveCVRefImpl<const T&&>          { using type = T; };
template <typename T> struct RemoveCVRefImpl<volatile T>         { using type = T; };
template <typename T> struct RemoveCVRefImpl<volatile T&>        { using type = T; };
template <typename T> struct RemoveCVRefImpl<volatile T&&>       { using type = T; };
template <typename T> struct RemoveCVRefImpl<const volatile T>   { using type = T; };
template <typename T> struct RemoveCVRefImpl<const volatile T&>  { using type = T; };
template <typename T> struct RemoveCVRefImpl<const volatile T&&> { using type = T; };
// clang-format on

} // namespace sf::base::priv

////////////////////////////////////////////////////////////
#define SFML_BASE_REMOVE_CVREF(...) typename ::sf::base::priv::RemoveCVRefImpl<__VA_ARGS__>::type

#endif
