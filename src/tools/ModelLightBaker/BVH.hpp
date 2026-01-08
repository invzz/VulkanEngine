#pragma once

#include <glm/glm.hpp>
#include <limits>
#include <vector>

namespace ModelLightBaker {

  struct Tri
  {
    glm::vec3 p0, p1, p2;
    glm::vec3 n0, n1, n2;
    glm::vec2 uv0, uv1, uv2;
    int       materialId = -1;
  };

  struct AABB
  {
    glm::vec3 min{std::numeric_limits<float>::max()};
    glm::vec3 max{std::numeric_limits<float>::lowest()};

    void expand(const glm::vec3& p)
    {
      min = glm::min(min, p);
      max = glm::max(max, p);
    }

    void expand(const AABB& other)
    {
      min = glm::min(min, other.min);
      max = glm::max(max, other.max);
    }

    bool intersectRay(const glm::vec3& orig, const glm::vec3& invDir, const glm::bvec3& sign, float tmin, float tmax) const;
  };

  struct BVHNode
  {
    AABB bounds;
    int  left  = -1;
    int  right = -1;
    int  start = 0; // start index in triangle list
    int  count = 0; // number of triangles
  };

  class BVH
  {
  public:
    BVH() = default;
    // Build BVH from triangles with optional padding (adds padding to bounds)
    void build(const std::vector<Tri>& tris, float padding = 0.0f);

    // Ray intersection: returns true if any hit is found at t>eps and t < tmax
    bool intersectAny(const glm::vec3& orig, const glm::vec3& dir, float eps, float tmax) const;
    // Accessors for GPU export
    [[nodiscard]] const std::vector<BVHNode>& getNodes() const { return nodes_; }
    [[nodiscard]] const std::vector<int>&     getTriIndices() const { return triIndices_; }

  private:
    std::vector<BVHNode>    nodes_;
    std::vector<int>        triIndices_;
    std::vector<Tri> const* trisPtr_ = nullptr;

    int buildRecursive(int start, int end);
  };

} // namespace ModelLightBaker
