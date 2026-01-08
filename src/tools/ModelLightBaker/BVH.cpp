#include "BVH.hpp"

#include <algorithm>

namespace ModelLightBaker {

  static inline float edgeFunc(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
  {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
  }

  bool AABB::intersectRay(const glm::vec3& orig, const glm::vec3& invDir, const glm::bvec3& sign, float tmin, float tmax) const
  {
    // slab method
    for (int i = 0; i < 3; ++i)
    {
      float t1 = ((sign[i] ? max[i] : min[i]) - orig[i]) * invDir[i];
      float t2 = ((sign[i] ? min[i] : max[i]) - orig[i]) * invDir[i];
      tmin     = std::max(tmin, std::min(t1, t2));
      tmax     = std::min(tmax, std::max(t1, t2));
      if (tmax < tmin) return false;
    }
    return true;
  }

  void BVH::build(const std::vector<Tri>& tris, float padding)
  {
    trisPtr_ = &tris;
    triIndices_.resize(tris.size());
    for (size_t i = 0; i < tris.size(); ++i)
      triIndices_[i] = static_cast<int>(i);

    nodes_.clear();
    nodes_.reserve(256);

    // compute per-tri bounds
    // Use recursive median split
    buildRecursive(0, static_cast<int>(tris.size()));

    // apply padding to node bounds if requested
    if (padding > 0.0f)
    {
      for (auto& n : nodes_)
      {
        n.bounds.min -= glm::vec3(padding);
        n.bounds.max += glm::vec3(padding);
      }
    }
  }

  int BVH::buildRecursive(int start, int end)
  {
    BVHNode node;
    node.start = start;
    node.count = end - start;

    // compute bounds
    AABB bounds;
    for (int i = start; i < end; ++i)
    {
      const Tri& t = (*trisPtr_)[triIndices_[i]];
      bounds.expand(t.p0);
      bounds.expand(t.p1);
      bounds.expand(t.p2);
    }
    node.bounds = bounds;

    int nodeIdx = static_cast<int>(nodes_.size());
    nodes_.push_back(node);

    if (node.count <= 4)
    {
      // leaf
      nodes_[nodeIdx].left  = -1;
      nodes_[nodeIdx].right = -1;
      return nodeIdx;
    }

    // choose split axis
    glm::vec3 extents = bounds.max - bounds.min;
    int       axis    = 0;
    if (extents.y > extents.x && extents.y >= extents.z)
      axis = 1;
    else if (extents.z > extents.x && extents.z > extents.y)
      axis = 2;

    // partition by centroid median
    int mid = (start + end) / 2;
    std::nth_element(triIndices_.begin() + start, triIndices_.begin() + mid, triIndices_.begin() + end, [&](int a, int b) {
      glm::vec3 ca = ((*trisPtr_)[a].p0 + (*trisPtr_)[a].p1 + (*trisPtr_)[a].p2) / 3.0f;
      glm::vec3 cb = ((*trisPtr_)[b].p0 + (*trisPtr_)[b].p1 + (*trisPtr_)[b].p2) / 3.0f;
      return ca[axis] < cb[axis];
    });

    int left              = buildRecursive(start, mid);
    int right             = buildRecursive(mid, end);
    nodes_[nodeIdx].left  = left;
    nodes_[nodeIdx].right = right;
    return nodeIdx;
  }

  bool BVH::intersectAny(const glm::vec3& orig, const glm::vec3& dir, float eps, float tmax) const
  {
    if (nodes_.empty()) return false;

    glm::vec3  invDir(1.0f / dir.x, 1.0f / dir.y, 1.0f / dir.z);
    glm::bvec3 sign(invDir.x < 0.0f, invDir.y < 0.0f, invDir.z < 0.0f);

    // stack
    int stack[64];
    int sp      = 0;
    stack[sp++] = 0; // root

    const auto& tris = *trisPtr_;

    while (sp > 0)
    {
      int            nodeIdx = stack[--sp];
      const BVHNode& node    = nodes_[nodeIdx];
      if (!node.bounds.intersectRay(orig, invDir, sign, eps, tmax)) continue;

      if (node.left == -1 && node.right == -1)
      {
        // leaf - test triangles
        for (int i = node.start; i < node.start + node.count; ++i)
        {
          const Tri& tri = tris[triIndices_[i]];
          // Moller-Trumbore
          const glm::vec3 e1 = tri.p1 - tri.p0;
          const glm::vec3 e2 = tri.p2 - tri.p0;
          glm::vec3       h  = glm::cross(dir, e2);
          float           a  = glm::dot(e1, h);
          if (fabs(a) < 1e-9f) continue; // parallel
          float     f = 1.0f / a;
          glm::vec3 s = orig - tri.p0;
          float     u = f * glm::dot(s, h);
          if (u < 0.0f - 1e-6f || u > 1.0f + 1e-6f) continue;
          glm::vec3 q = glm::cross(s, e1);
          float     v = f * glm::dot(dir, q);
          if (v < 0.0f - 1e-6f || u + v > 1.0f + 1e-6f) continue;
          float t = f * glm::dot(e2, q);
          if (t > eps && t < tmax) return true;
        }
      }
      else
      {
        // push children
        if (node.right != -1) stack[sp++] = node.right;
        if (node.left != -1) stack[sp++] = node.left;
      }
    }

    return false;
  }

} // namespace ModelLightBaker
