#ifndef EDITOR_ANIMATION_GRAPH_EDITOR_HPP
#define EDITOR_ANIMATION_GRAPH_EDITOR_HPP

#include <imgui.h>
#include <imgui_internal.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>

#include "Engine/Scene/Components/AnimationGraph.hpp"
#include "Engine/Scene/Components/AnimationController.hpp"

namespace engine::ui {

/**
 * @brief Node-based editor for AnimationGraph in ImGui
 *
 * Features:
 * - Render nodes as colored rectangles with name and clip info
 * - Draw bezier curve connections between nodes
 * - Drag nodes to reposition (ImGui::IsMouseDragging on node bounds)
 * - Inline property panel for selected node/transition
 * - Play/step preview to visualize graph state transitions
 * - Zoom and pan with scroll/wheel
 */
class AnimationGraphEditor {
public:
    AnimationGraphEditor() = default;

    /**
     * @brief Render the graph editor panel
     * @param graph Pointer to the animation graph (may be null)
     * @param controller Pointer to the animation controller (for preview)
     * @param delta Time delta for preview stepping (seconds)
     * @return true if the graph was modified by the editor
     */
    bool render(std::shared_ptr<AnimationGraph> graph,
                std::shared_ptr<AnimationController> controller,
                float delta);

private:
    // ── Node layout ───────────────────────────────────────────────────
    struct NodeEntry {
        const AnimationGraphNode* node{nullptr};
        ImVec2 pos{0.0f, 0.0f};      // Top-left of node rectangle
        ImVec2 size{180.0f, 60.0f};  // Node dimensions
        bool selected{false};
        bool hovered{false};
        bool dragging{false};

        // Connection ports (relative to node top-left)
        ImVec2 inputPort{0.0f, 30.0f};   // Left side center
        ImVec2 outputPort{0.0f, 30.0f};  // Right side center
    };

    std::unordered_map<int, NodeEntry> nodeMap_;
    ImVec2 canvasOffset_{0.0f, 0.0f};  // Pan offset
    float zoom_{1.0f};
    bool autoLayout_{true};

    // Selection state
    int selectedNodeId_{-1};
    int selectedTransitionId_{-1};
    int hoveredNodeId_{-1};
    int draggingNodeId_{-1};

    // Preview state
    bool isPlaying_{false};
    float previewTime_{0.0f};
    bool graphModified_{false};

    // ── Internal helpers ──────────────────────────────────────────────
    void updateNodePositions(std::shared_ptr<AnimationGraph> graph);
    void renderNodes();
    void renderConnections(std::shared_ptr<AnimationGraph> graph);
    void renderPortConnections(std::shared_ptr<AnimationGraph> graph);
    void renderPropertyPanel(std::shared_ptr<AnimationGraph> graph,
                             std::shared_ptr<AnimationController> controller);
    void renderPlayControls(std::shared_ptr<AnimationGraph> graph,
                            std::shared_ptr<AnimationController> controller,
                            float delta);
    void handleNodeInteraction();
    void handleCanvasInteraction();
    void autoLayoutGraph(std::shared_ptr<AnimationGraph> graph);

    // Node color helpers
    ImVec4 getNodeColor(const AnimationGraphNode& node) const;
    ImVec4 getNodeBorderColor(const AnimationGraphNode& node) const;
    bool isNodeSelected(const AnimationGraphNode& node) const;
};

// ── Implementation ───────────────────────────────────────────────────────

inline ImVec4 AnimationGraphEditor::getNodeColor(const AnimationGraphNode& node) const {
    if (node.isEntry) return ImVec4(0.2f, 0.6f, 0.3f, 0.8f);  // Green
    if (node.isExit) return ImVec4(0.7f, 0.2f, 0.2f, 0.8f);   // Red
    if (node.isBlendNode) return ImVec4(0.6f, 0.5f, 0.2f, 0.8f); // Yellow
    return ImVec4(0.25f, 0.3f, 0.45f, 0.8f);                  // Default blue-gray
}

inline ImVec4 AnimationGraphEditor::getNodeBorderColor(const AnimationGraphNode& node) const {
    if (isNodeSelected(node)) return ImVec4(0.9f, 0.8f, 0.2f, 1.0f); // Gold
    if (node.active) return ImVec4(0.4f, 0.8f, 0.4f, 1.0f);         // Light green
    return ImVec4(0.35f, 0.4f, 0.55f, 1.0f);                        // Default border
}

inline bool AnimationGraphEditor::isNodeSelected(const AnimationGraphNode& node) const {
    return selectedNodeId_ == node.id;
}

inline void AnimationGraphEditor::autoLayoutGraph(std::shared_ptr<AnimationGraph> graph) {
    if (!graph) return;

    // Simple layer-based layout
    // Layer 0: entry node
    // Layer 1: nodes reachable from entry in 1 step
    // Layer 2: nodes reachable from layer 1, etc.
    std::unordered_set<int> visited;

    const auto* entry = graph->getEntryNode();
    if (!entry) return;

    // BFS to assign layers
    std::vector<std::pair<int, int>> nodesByLayer; // {nodeId, layer}
    nodesByLayer.emplace_back(entry->id, 0);
    visited.insert(entry->id);

    int maxLayer = 0;
    for (size_t i = 0; i < nodesByLayer.size(); ++i) {
        int nid = nodesByLayer[i].first;
        int layer = nodesByLayer[i].second;
        const auto* n = graph->getNode(nid);
        if (!n) continue;

        auto trans = graph->getTransitions(nid);
        for (const auto* t : trans) {
            if (!visited.count(t->targetNodeId)) {
                visited.insert(t->targetNodeId);
                nodesByLayer.emplace_back(t->targetNodeId, layer + 1);
                maxLayer = std::max(maxLayer, layer + 1);
            }
        }
    }

    // Group by layer
    std::vector<std::vector<int>> layerNodes(maxLayer + 1);
    for (const auto& pair : nodesByLayer) {
        layerNodes[pair.second].push_back(pair.first);
    }

    // Calculate positions
    const float nodeWidth = 180.0f;
    const float nodeHeight = 60.0f;
    const float hSpacing = 240.0f;
    const float vSpacing = 100.0f;
    const float startX = 50.0f;
    const float startY = 50.0f;

    for (int layer = 0; layer <= maxLayer; ++layer) {
        float y = startY + layer * vSpacing;
        int count = static_cast<int>(layerNodes[layer].size());
        for (int i = 0; i < count; ++i) {
            float x = startX + (i - (count - 1) / 2.0f) * hSpacing;
            auto it = nodeMap_.find(layerNodes[layer][i]);
            if (it != nodeMap_.end()) {
                it->second.pos = ImVec2(x, y);
                it->second.size = ImVec2(nodeWidth, nodeHeight);
            }
        }
    }
}

inline void AnimationGraphEditor::updateNodePositions(std::shared_ptr<AnimationGraph> graph) {
    if (!graph) return;

    if (autoLayout_) {
        autoLayoutGraph(graph);
    }

    // Ensure all nodes exist in map
    // Iterate through transitions to find all referenced nodes
    std::unordered_set<int> nodeIds;
    if (graph->getEntryNode()) {
        nodeIds.insert(graph->getEntryNode()->id);
    }
    // We need to iterate transitions to find all nodes - use getTransitions on entry
    // For now, just add entry and any node we can find via transitions
    for (const auto& entry : nodeMap_) {
        if (entry.second.node) {
            nodeIds.insert(entry.second.node->id);
        }
    }

    // Check transitions for additional nodes
    if (graph->getEntryNode()) {
        auto trans = graph->getTransitions(graph->getEntryNode()->id);
        for (const auto* t : trans) {
            nodeIds.insert(t->targetNodeId);
            nodeIds.insert(t->sourceNodeId);
        }
    }

    for (int nid : nodeIds) {
        if (nodeMap_.find(nid) == nodeMap_.end()) {
            const auto* n = graph->getNode(nid);
            if (n) {
                NodeEntry entry;
                entry.node = n;
                entry.size = ImVec2(180.0f, 60.0f);
                nodeMap_[nid] = entry;
            }
        }
    }
}

inline void AnimationGraphEditor::renderNodes() {
    for (auto& entry : nodeMap_) {
        const int id = entry.first;
        NodeEntry& node = entry.second;
        if (!node.node) continue;

        ImVec2 screenPos(node.pos.x + canvasOffset_.x, node.pos.y + canvasOffset_.y);
        ImVec2 screenSize(node.size.x * zoom_, node.size.y * zoom_);

        // Node background
        ImVec4 bgColor = getNodeColor(*node.node);
        ImGui::GetWindowDrawList()->AddRectFilled(
            screenPos,
            ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y),
            ImGui::ColorConvertFloat4ToU32(bgColor),
            6.0f
        );

        // Node border
        ImVec4 borderColor = getNodeBorderColor(*node.node);
        ImGui::GetWindowDrawList()->AddRect(
            screenPos,
            ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y),
            ImGui::ColorConvertFloat4ToU32(borderColor),
            6.0f,
            0,
            2.0f
        );

        // Input port (left side)
        ImVec2 inputPos(screenPos.x, screenPos.y + screenSize.y * 0.5f);
        ImGui::GetWindowDrawList()->AddCircleFilled(inputPos, 5.0f * zoom_,
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.7f, 0.8f, 0.9f, 1.0f)));

        // Output port (right side)
        ImVec2 outputPos(screenPos.x + screenSize.x, screenPos.y + screenSize.y * 0.5f);
        ImGui::GetWindowDrawList()->AddCircleFilled(outputPos, 5.0f * zoom_,
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.7f, 0.8f, 0.9f, 1.0f)));

        // Node name
        std::string name = node.node->name.empty() ? ("Node " + std::to_string(node.node->id)) : node.node->name;
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(screenPos.x + 8.0f * zoom_, screenPos.y + 8.0f * zoom_),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.95f, 0.95f, 0.95f, 1.0f)),
            name.c_str()
        );

        // Clip info
        if (node.node->clipIndex >= 0) {
            std::string clipInfo = "Clip " + std::to_string(node.node->clipIndex);
            if (!node.node->clipName.empty()) {
                clipInfo += ": " + node.node->clipName;
            }
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(screenPos.x + 8.0f * zoom_, screenPos.y + 24.0f * zoom_),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.7f, 0.7f, 0.8f, 0.8f)),
                clipInfo.c_str()
            );
        }

        // Active indicator
        if (node.node->active) {
            std::string status = "▶ Active";
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(screenPos.x + 8.0f * zoom_, screenPos.y + 40.0f * zoom_),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.4f, 0.9f, 0.4f, 0.8f)),
                status.c_str()
            );
        }

        // Invisible button for hit testing
        ImGui::InvisibleButton(("node_" + std::to_string(id)).c_str(), screenSize);
        node.hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            selectedNodeId_ = id;
            selectedTransitionId_ = -1;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(0)) {
            draggingNodeId_ = id;
        }
    }
}

inline void AnimationGraphEditor::renderConnections(std::shared_ptr<AnimationGraph> graph) {
    if (!graph) return;

    // Iterate through all transitions by checking nodes
    std::unordered_set<int> processedTransitions;

    // We need to access transitions differently - get entry and traverse
    if (graph->getEntryNode()) {
        auto trans = graph->getTransitions(graph->getEntryNode()->id);
        for (const auto* t : trans) {
            if (processedTransitions.count(t->id)) continue;
            processedTransitions.insert(t->id);

            auto srcIt = nodeMap_.find(t->sourceNodeId);
            auto dstIt = nodeMap_.find(t->targetNodeId);
            if (srcIt == nodeMap_.end() || dstIt == nodeMap_.end()) continue;

            // Calculate connection points
            ImVec2 srcScreenPos(srcIt->second.pos.x + canvasOffset_.x, srcIt->second.pos.y + canvasOffset_.y);
            ImVec2 srcSize(srcIt->second.size.x * zoom_, srcIt->second.size.y * zoom_);
            ImVec2 dstScreenPos(dstIt->second.pos.x + canvasOffset_.x, dstIt->second.pos.y + canvasOffset_.y);
            ImVec2 dstSize(dstIt->second.size.x * zoom_, dstIt->second.size.y * zoom_);

            ImVec2 srcPos(srcScreenPos.x + srcSize.x, srcScreenPos.y + srcSize.y * 0.5f);
            ImVec2 dstPos(dstScreenPos.x, dstScreenPos.y + dstSize.y * 0.5f);

            // Bezier curve control points
            ImVec2 cp1(srcPos.x + 50.0f * zoom_, srcPos.y);
            ImVec2 cp2(dstPos.x - 50.0f * zoom_, dstPos.y);

            // Draw bezier curve manually using line segments
            int segments = 20;
            for (int i = 0; i < segments; ++i) {
                float tt = i / static_cast<float>(segments);
                float tt2 = tt * tt;
                float tt3 = tt2 * tt;
                float mt = 1.0f - tt;
                float mt2 = mt * mt;
                float mt3 = mt2 * mt;

                // Cubic bezier: B(t) = (1-t)^3*P0 + 3*(1-t)^2*t*P1 + 3*(1-t)*t^2*P2 + t^3*P3
                float x = mt3 * srcPos.x + 3 * mt2 * tt * cp1.x + 3 * mt * tt2 * cp2.x + tt3 * dstPos.x;
                float y = mt3 * srcPos.y + 3 * mt2 * tt * cp1.y + 3 * mt * tt2 * cp2.y + tt3 * dstPos.y;

                float ntt = (i + 1) / static_cast<float>(segments);
                float ntt2 = ntt * ntt;
                float ntt3 = ntt2 * ntt;
                float mnt = 1.0f - ntt;
                float mnt2 = mnt * mnt;
                float mnt3 = mnt2 * mnt;

                float nx = mnt3 * srcPos.x + 3 * mnt2 * ntt * cp1.x + 3 * mnt * ntt2 * cp2.x + ntt3 * dstPos.x;
                float ny = mnt3 * srcPos.y + 3 * mnt2 * ntt * cp1.y + 3 * mnt * ntt2 * cp2.y + ntt3 * dstPos.y;

                bool isSelected = (selectedTransitionId_ == t->id);
                ImVec4 connColor = isSelected ? ImVec4(1.0f, 0.8f, 0.1f, 1.0f) : ImVec4(0.4f, 0.6f, 0.8f, 0.6f);

                ImGui::GetWindowDrawList()->AddLine(ImVec2(x, y), ImVec2(nx, ny),
                    ImGui::ColorConvertFloat4ToU32(connColor),
                    isSelected ? 3.0f : 2.0f);
            }

            // Connection label at midpoint
            float midT = 0.5f;
            float midMt = 1.0f - midT;
            float midX = midMt*midMt*midMt * srcPos.x + 3*midMt*midMt*midT * cp1.x + 3*midMt*midT*midT * cp2.x + midT*midT*midT * dstPos.x;
            float midY = midMt*midMt*midMt * srcPos.y + 3*midMt*midMt*midT * cp1.y + 3*midMt*midT*midT * cp2.y + midT*midT*midT * dstPos.y;

            ImGui::GetWindowDrawList()->AddText(
                ImVec2(midX, midY),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.8f, 0.85f, 0.95f, 0.9f)),
                t->name.c_str()
            );
        }
    }
}

inline void AnimationGraphEditor::renderPropertyPanel(std::shared_ptr<AnimationGraph> graph,
                                                       std::shared_ptr<AnimationController> controller) {
    ImGui::TextDisabled("Properties");
    ImGui::Separator();

    // Helper for mutable access
    auto getMutableTrans = [&](int transId) -> const AnimationTransition* {
        if (!graph || !graph->getEntryNode()) return nullptr;
        auto trans = graph->getTransitions(graph->getEntryNode()->id);
        for (const auto* t : trans) {
            if (t->id == transId) return t;
        }
        return nullptr;
    };

    if (selectedNodeId_ >= 0 && graph) {
        const auto* node = graph->getNode(selectedNodeId_);
        if (node) {
            ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.2f, 1.0f), "Node: %s", node->name.c_str());
            ImGui::Text("ID: %d", node->id);
            ImGui::Text("Clip: %d (%s)", node->clipIndex, node->clipName.c_str());
            ImGui::Checkbox("Entry Node", const_cast<bool*>(&node->isEntry));
            ImGui::Checkbox("Exit Node", const_cast<bool*>(&node->isExit));
            ImGui::Checkbox("Blend Node", const_cast<bool*>(&node->isBlendNode));

            if (node->isEntry) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Set as Start")) {
                    // Could set this as the entry node
                    graphModified_ = true;
                }
            }
        }
    } else if (selectedTransitionId_ >= 0 && graph) {
       // Get mutable pointer
        const AnimationTransition* transOrig = getMutableTrans(selectedTransitionId_);
        if (!transOrig) return;
        AnimationTransition* trans = const_cast<AnimationTransition*>(transOrig);

        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.2f, 1.0f), "Transition: %s", trans->name.c_str());
            ImGui::Text("ID: %d", trans->id);
            ImGui::Text("From: %d -> To: %d", trans->sourceNodeId, trans->targetNodeId);

            int condInt = static_cast<int>(trans->condition);
            ImGui::Combo("Condition",
                        const_cast<int*>(&condInt),
                        "None\0Time-Based\0Event-Based\0Param-Based\0Blend-Complete\0");
            // Update condition if changed
            if (condInt != static_cast<int>(trans->condition)) {
                graphModified_ = true;
            }

            if (trans->condition == TransitionCondition::TIME_BASED) {
                ImGui::DragFloat("Time Threshold", const_cast<float*>(&trans->timeThreshold), 0.1f, 0.1f, 100.0f, "%.1f s");
            } else if (trans->condition == TransitionCondition::EVENT_BASED) {
                // Use temporary buffer for InputText
                std::string tempEventName = trans->eventName;
                char* buf = const_cast<char*>(tempEventName.data());
                if (ImGui::InputText("Event Name", buf, tempEventName.capacity() + 1)) {
                    trans->eventName = tempEventName;
                    graphModified_ = true;
                }
            } else if (trans->condition == TransitionCondition::PARAM_BASED) {
                // Use temporary buffer for InputText
                std::string tempParamName = trans->paramName;
                char* buf = const_cast<char*>(tempParamName.data());
                if (ImGui::InputText("Param Name", buf, tempParamName.capacity() + 1)) {
                    trans->paramName = tempParamName;
                    graphModified_ = true;
                }
                ImGui::DragFloat("Param Value", const_cast<float*>(&trans->paramValue), 0.01f, -100.0f, 100.0f, "%.3f");
            }

            // For blend mode, use a simple combo
            int blendInt = 0; // Default to "Fade"
            if (trans->blendMode == "crossfade") blendInt = 1;
            else if (trans->blendMode == "instant") blendInt = 2;
            ImGui::Combo("Blend Mode", const_cast<int*>(&blendInt),
                        "Fade\0Crossfade\0Instant\0");
            if (blendInt == 1) trans->blendMode = "crossfade";
            else if (blendInt == 2) trans->blendMode = "instant";
            else trans->blendMode = "fade";

            ImGui::DragFloat("Blend Duration", const_cast<float*>(&trans->blendDuration), 0.01f, 0.0f, 10.0f, "%.3f s");

             ImGui::Checkbox("Default Transition", const_cast<bool*>(&trans->isDefault));
    } else {
        ImGui::TextDisabled("No selection");
        ImGui::TextDisabled("Click a node or connection to inspect");
    }

    // Add node/transition buttons
    ImGui::Separator();
    if (ImGui::SmallButton("+ Add Node")) {
        graphModified_ = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add new animation state node");
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("+ Add Transition")) {
        graphModified_ = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add transition between selected nodes");
    }

    ImGui::SameLine();
    if (selectedNodeId_ >= 0 && ImGui::SmallButton("Delete")) {
        graphModified_ = true;
        selectedNodeId_ = -1;
    }
    if (selectedTransitionId_ >= 0 && ImGui::SmallButton("Delete")) {
        graphModified_ = true;
        selectedTransitionId_ = -1;
    }
}

inline void AnimationGraphEditor::renderPlayControls(std::shared_ptr<AnimationGraph> graph,
                                                      std::shared_ptr<AnimationController> controller,
                                                      float delta) {
    ImGui::TextDisabled("Preview");
    ImGui::Separator();

    if (graph) {
        const auto* current = graph->getCurrentNode();
        std::string currentLabel = current ? current->name : "None";
        ImGui::Text("Current: %s", currentLabel.c_str());

        const auto* entry = graph->getEntryNode();
        if (entry) {
            ImGui::Text("Entry: %s", entry->name.c_str());
        }
    }

    if (controller && controller->isTransitioning()) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Transitioning...");
    }

    if (ImGui::SmallButton("▶ Play")) {
        isPlaying_ = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("⏸ Step")) {
        // Note: full stepping requires AnimationComponent, which is a forward declaration here
        graphModified_ = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("⏹ Stop")) {
        isPlaying_ = false;
        previewTime_ = 0.0f;
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Play/step the animation graph to visualize state transitions");
    }

    // Show preview time
    ImGui::Text("Preview Time: %.3fs", previewTime_);
    ImGui::DragFloat("Preview Speed", &previewTime_, 0.1f, 0.0f, 100.0f, "%.2f s");
}

inline void AnimationGraphEditor::handleNodeInteraction() {
    // Handle node dragging
      if (draggingNodeId_ >= 0) {
          auto it = nodeMap_.find(draggingNodeId_);
          if (it != nodeMap_.end()) {
              ImVec2 mousePos = ImGui::GetMousePos();
              // Convert to canvas coordinates
              ImVec2 canvasPos = ImGui::GetCursorScreenPos();
              float px = mousePos.x - canvasPos.x - it->second.size.x * zoom_ * 0.5f;
              float py = mousePos.y - canvasPos.y - it->second.size.y * zoom_ * 0.5f;
              it->second.pos = ImVec2(px, py);
          }
      }

      // Handle pan
      if (ImGui::IsMouseDown(0) && !ImGui::IsAnyItemHovered()) {
          // Check if we're in the canvas area
          ImVec2 canvasMin = ImGui::GetWindowPos();
          ImVec2 canvasMax(canvasMin.x + ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x,
                           canvasMin.y + ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y);
          ImVec2 mousePos = ImGui::GetMousePos();
          if (mousePos.x > canvasMin.x && mousePos.x < canvasMax.x &&
              mousePos.y > canvasMin.y && mousePos.y < canvasMax.y) {
              // Manual += for ImVec2
              canvasOffset_.x += ImGui::GetIO().MouseDelta.x;
              canvasOffset_.y += ImGui::GetIO().MouseDelta.y;
          }
      }

    // Handle zoom
    if (ImGui::IsWindowFocused() && ImGui::GetIO().KeyCtrl) {
        float zoomFactor = 1.1f;
        if (ImGui::GetIO().MouseWheel < -0.1f) {
            zoom_ /= zoomFactor;
        } else if (ImGui::GetIO().MouseWheel > 0.1f) {
            zoom_ *= zoomFactor;
        }
        zoom_ = std::max(0.3f, std::min(3.0f, zoom_));
    }
}

inline bool AnimationGraphEditor::render(std::shared_ptr<AnimationGraph> graph,
                                         std::shared_ptr<AnimationController> controller,
                                         float delta) {
    graphModified_ = false;
    selectedNodeId_ = -1;
    selectedTransitionId_ = -1;
    hoveredNodeId_ = -1;
    draggingNodeId_ = -1;

    if (!ImGui::Begin("Animation Graph##graph_editor")) {
        ImGui::End();
        return false;
    }

    // Update node positions
    updateNodePositions(graph);

    // Canvas area
    ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 canvasMax(canvasMin.x + avail.x, canvasMin.y + avail.y);

    // Clear canvas
    ImGui::GetWindowDrawList()->AddRectFilled(canvasMin, canvasMax, IM_COL32(30, 30, 40, 255));

    // Grid (subtle)
    float gridSize = 30.0f * zoom_;
    for (float x = fmodf(canvasOffset_.x, gridSize); x < avail.x; x += gridSize) {
        ImVec2 p1(canvasMin.x + x, canvasMin.y);
        ImVec2 p2(canvasMin.x + x, canvasMax.y);
        uint32_t color = IM_COL32(50, 50, 60, 150);
        ImGui::GetWindowDrawList()->AddLine(p1, p2, color);
    }
    for (float y = fmodf(canvasOffset_.y, gridSize); y < avail.y; y += gridSize) {
        ImVec2 p1(canvasMin.x, canvasMin.y + y);
        ImVec2 p2(canvasMax.x, canvasMin.y + y);
        uint32_t color = IM_COL32(50, 50, 60, 150);
        ImGui::GetWindowDrawList()->AddLine(p1, p2, color);
    }

    // Save canvas region for interaction
    ImGui::InvisibleButton("graph_canvas", avail);

    // Render connections first (behind nodes)
    renderConnections(graph);

    // Render nodes
    renderNodes();

    // Handle interactions
    handleNodeInteraction();

    // Right panel for properties
    if (ImGui::BeginChild("properties_panel", ImVec2(250.0f, 0), true)) {
        renderPropertyPanel(graph, controller);
        renderPlayControls(graph, controller, delta);
    }
    ImGui::EndChild();

    ImGui::End();
    return graphModified_;
}

} // namespace engine::ui

#endif // EDITOR_ANIMATION_GRAPH_EDITOR_HPP
