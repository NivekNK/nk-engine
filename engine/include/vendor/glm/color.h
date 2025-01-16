#pragma once

#include <glm/ext/vector_uint4_sized.hpp>

namespace glm {
    using color = glm::u8vec4;
}

static constexpr glm::color rgb(const nk::u8 r, const nk::u8 g, const nk::u8 b) {
    return glm::color{r, g, b, 255};
}

static constexpr glm::color rgba(const nk::u8 r, const nk::u8 g, const nk::u8 b, const nk::u8 a) {
    return glm::color{r, g, b, a};
}
