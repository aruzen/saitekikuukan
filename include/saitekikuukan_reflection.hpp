#pragma once

#ifndef SAITEKIKUUKAN_REFLECTION_HPP
#define SAITEKIKUUKAN_REFLECTION_HPP

#if !defined(__cpp_impl_reflection) || __cpp_impl_reflection < 202506L
#error "saitekikuukan_reflection.hpp requires C++26 reflection (GCC 16: -std=c++26 -freflection)"
#else
#include "saitekikuukan.hpp"
#endif

#endif // SAITEKIKUUKAN_REFLECTION_HPP
