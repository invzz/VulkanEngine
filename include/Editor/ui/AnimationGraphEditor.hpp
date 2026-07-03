#ifndef EDITOR_ANIMATION_GRAPH_EDITOR_HPP
#define EDITOR_ANIMATION_GRAPH_EDITOR_HPP

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <imgui_internal.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Engine/Scene/Components/AnimationController.hpp"
#include "Engine/Scene/Components/AnimationGraph.hpp"

namespace engine::ui {

    /**
 * @brief Node-based editor for AnimationGraph in ImGui
 *
 * Features:
 * - Render nodes as colored rectangles with name and clip info
 * - Draw bezier curve connections between nodes
 * - Drag nodes to reposition (ImGui::IsMouseDragging on node bounds)
 * - Drag-from-output-port to create new transitions (drag-to-connect)
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
            std::shared_ptr<AnimationController>    controller,
            float                                   delta);

       private:
        struct NodeEntry {
            const AnimationGraphNode* node{nullptr};
            ImVec2                    pos{0.0f, 0.0f};
            ImVec2                    size{180.0f, 60.0f};
            bool                      selected{false};
            bool                      hovered{false};
            bool                      dragging{false};

            ImVec2 inputPort{0.0f, 30.0f};
            ImVec2 outputPort{0.0f, 30.0f};
        };

        std::unordered_map<int, NodeEntry> nodeMap_;
        ImVec2                             canvasOffset_{0.0f, 0.0f};
        float                              zoom_{1.0f};
        bool                               autoLayout_{true};

        int selectedNodeId_{-1};
        int selectedTransitionId_{-1};
        int hoveredNodeId_{-1};
        int draggingNodeId_{-1};

        int    draggingOutputNode_{-1};
        bool   isConnecting_{false};
        int    connectTargetNodeId_{-1};
        ImVec2 connectMousePos_{0.0f, 0.0f};

        bool  isPlaying_{false};
        float previewTime_{0.0f};
        bool  graphModified_{false};

        bool                showAddNode_{false};
        bool                showAddTransition_{false};
        std::string         newNodeName_;
        int                 newClipIndex_{-1};
        bool                newNodeIsEntry_{false};
        int                 newTransitionSource_{-1};
        int                 newTransitionTarget_{-1};
        std::string         newTransitionName_;
        TransitionCondition newTransitionCondition_{TransitionCondition::NONE};
        float               newTransitionTimeThreshold_{0.0f};

        void updateNodePositions(std::shared_ptr<AnimationGraph> graph);
        void renderNodes();
        void renderConnections(std::shared_ptr<AnimationGraph> graph);
        void renderPortConnections(std::shared_ptr<AnimationGraph> graph);
        void renderPropertyPanel(std::shared_ptr<AnimationGraph> graph,
            std::shared_ptr<AnimationController>                 controller);
        void renderPlayControls(std::shared_ptr<AnimationGraph> graph,
            std::shared_ptr<AnimationController>                controller,
            float                                               delta);
        void handleNodeInteraction();
        void handleCanvasInteraction();
        void autoLayoutGraph(std::shared_ptr<AnimationGraph> graph);
        void handleAddNodeDialog(std::shared_ptr<AnimationGraph> graph);
        void handleAddTransitionDialog(std::shared_ptr<AnimationGraph> graph);
        void handleConnectInteraction(std::shared_ptr<AnimationGraph> graph);
        void renderConnectPreview();

        ImVec4 getNodeColor(const AnimationGraphNode& node) const;
        ImVec4 getNodeBorderColor(const AnimationGraphNode& node) const;
        bool   isNodeSelected(const AnimationGraphNode& node) const;
    };

    inline ImVec4 AnimationGraphEditor::getNodeColor(const AnimationGraphNode& node) const {
        if (node.isEntry)
            return ImVec4(0.2f, 0.6f, 0.3f, 0.8f);
        if (node.isExit)
            return ImVec4(0.7f, 0.2f, 0.2f, 0.8f);
        if (node.isBlendNode)
            return ImVec4(0.6f, 0.5f, 0.2f, 0.8f);
        return ImVec4(0.25f, 0.3f, 0.45f, 0.8f);
    }

    inline ImVec4 AnimationGraphEditor::getNodeBorderColor(const AnimationGraphNode& node) const {
        if (isNodeSelected(node))
            return ImVec4(0.9f, 0.8f, 0.2f, 1.0f);
        if (node.active)
            return ImVec4(0.4f, 0.8f, 0.4f, 1.0f);
        return ImVec4(0.35f, 0.4f, 0.55f, 1.0f);
    }

    inline bool AnimationGraphEditor::isNodeSelected(const AnimationGraphNode& node) const {
        return selectedNodeId_ == node.id;
    }

    inline void AnimationGraphEditor::autoLayoutGraph(std::shared_ptr<AnimationGraph> graph) {
        if (!graph)
            return;

        std::unordered_set<int> visited;

        const auto* entry = graph->getEntryNode();
        if (!entry)
            return;

        std::vector<std::pair<int, int>> nodesByLayer;
        nodesByLayer.emplace_back(entry->id, 0);
        visited.insert(entry->id);

        int maxLayer = 0;
        for (size_t i = 0; i < nodesByLayer.size(); ++i) {
            int         nid   = nodesByLayer[i].first;
            int         layer = nodesByLayer[i].second;
            const auto* n     = graph->getNode(nid);
            if (!n)
                continue;

            auto trans = graph->getTransitions(nid);
            for (const auto* t : trans) {
                if (!visited.count(t->targetNodeId)) {
                    visited.insert(t->targetNodeId);
                    nodesByLayer.emplace_back(t->targetNodeId, layer + 1);
                    maxLayer = std::max(maxLayer, layer + 1);
                }
            }
        }

        std::vector<std::vector<int>> layerNodes(maxLayer + 1);
        for (const auto& pair : nodesByLayer) {
            layerNodes[pair.second].push_back(pair.first);
        }

        const float nodeWidth  = 180.0f;
        const float nodeHeight = 60.0f;
        const float hSpacing   = 240.0f;
        const float vSpacing   = 100.0f;
        const float startX     = 50.0f;
        const float startY     = 50.0f;

        for (int layer = 0; layer <= maxLayer; ++layer) {
            float y     = startY + layer * vSpacing;
            int   count = static_cast<int>(layerNodes[layer].size());
            for (int i = 0; i < count; ++i) {
                float x  = startX + (i - (count - 1) / 2.0f) * hSpacing;
                auto  it = nodeMap_.find(layerNodes[layer][i]);
                if (it != nodeMap_.end()) {
                    it->second.pos  = ImVec2(x, y);
                    it->second.size = ImVec2(nodeWidth, nodeHeight);
                }
            }
        }
    }

    inline void AnimationGraphEditor::updateNodePositions(std::shared_ptr<AnimationGraph> graph) {
        if (!graph)
            return;

        if (autoLayout_) {
            autoLayoutGraph(graph);
        }

        std::unordered_set<int> nodeIds;
        if (graph->getEntryNode()) {
            nodeIds.insert(graph->getEntryNode()->id);
        }
        for (const auto& entry : nodeMap_) {
            if (entry.second.node) {
                nodeIds.insert(entry.second.node->id);
            }
        }

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
                    entry.node    = n;
                    entry.size    = ImVec2(180.0f, 60.0f);
                    nodeMap_[nid] = entry;
                }
            }
        }
    }

    inline void AnimationGraphEditor::renderNodes() {
        for (auto& entry : nodeMap_) {
            const int  id   = entry.first;
            NodeEntry& node = entry.second;
            if (!node.node)
                continue;

            ImVec2 screenPos(node.pos.x + canvasOffset_.x, node.pos.y + canvasOffset_.y);
            ImVec2 screenSize(node.size.x * zoom_, node.size.y * zoom_);

            ImVec4 bgColor = getNodeColor(*node.node);
            ImGui::GetWindowDrawList()->AddRectFilled(
                screenPos,
                ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y),
                ImGui::ColorConvertFloat4ToU32(bgColor),
                6.0f);

            ImVec4 borderColor = getNodeBorderColor(*node.node);
            ImGui::GetWindowDrawList()->AddRect(
                screenPos,
                ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y),
                ImGui::ColorConvertFloat4ToU32(borderColor),
                6.0f,
                0,
                2.0f);

            ImVec2 inputPos(screenPos.x, screenPos.y + screenSize.y * 0.5f);
            ImGui::GetWindowDrawList()->AddCircleFilled(inputPos, 5.0f * zoom_,
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.7f, 0.8f, 0.9f, 1.0f)));

            ImVec2 outputPos(screenPos.x + screenSize.x, screenPos.y + screenSize.y * 0.5f);
            ImGui::GetWindowDrawList()->AddCircleFilled(outputPos, 5.0f * zoom_,
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.7f, 0.8f, 0.9f, 1.0f)));

            std::string name = node.node->name.empty() ? ("Node " + std::to_string(node.node->id)) : node.node->name;
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(screenPos.x + 8.0f * zoom_, screenPos.y + 8.0f * zoom_),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.95f, 0.95f, 0.95f, 1.0f)),
                name.c_str());

            if (node.node->clipIndex >= 0) {
                std::string clipInfo = "Clip " + std::to_string(node.node->clipIndex);
                if (!node.node->clipName.empty()) {
                    clipInfo += ": " + node.node->clipName;
                }
                ImGui::GetWindowDrawList()->AddText(
                    ImVec2(screenPos.x + 8.0f * zoom_, screenPos.y + 24.0f * zoom_),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.7f, 0.7f, 0.8f, 0.8f)),
                    clipInfo.c_str());
            }

            if (node.node->active) {
                std::string status = "Active";
                ImGui::GetWindowDrawList()->AddText(
                    ImVec2(screenPos.x + 8.0f * zoom_, screenPos.y + 40.0f * zoom_),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.4f, 0.9f, 0.4f, 0.8f)),
                    status.c_str());
            }

            ImGui::InvisibleButton(("node_" + std::to_string(id)).c_str(), screenSize);
            node.hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                selectedNodeId_       = id;
                selectedTransitionId_ = -1;
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(0)) {
                draggingNodeId_ = id;
            }

            ImVec2 portCenter(screenPos.x + screenSize.x, screenPos.y + screenSize.y * 0.5f);
            float  portRadius = 8.0f * zoom_;
            ImVec2 mouseDelta(ImGui::GetMousePos().x - portCenter.x, ImGui::GetMousePos().y - portCenter.y);
            float  distToMouse = ImSqrt(mouseDelta.x * mouseDelta.x + mouseDelta.y * mouseDelta.y);
            if (distToMouse < portRadius && ImGui::IsMouseDragging(0, 1.0f)) {
                draggingOutputNode_ = id;
                isConnecting_       = true;
                connectMousePos_    = ImGui::GetMousePos();
            }
        }
    }

    inline void AnimationGraphEditor::renderConnections(std::shared_ptr<AnimationGraph> graph) {
        if (!graph)
            return;

        std::unordered_set<int> processedTransitions;

        if (graph->getEntryNode()) {
            auto trans = graph->getTransitions(graph->getEntryNode()->id);
            for (const auto* t : trans) {
                if (processedTransitions.count(t->id))
                    continue;
                processedTransitions.insert(t->id);

                auto srcIt = nodeMap_.find(t->sourceNodeId);
                auto dstIt = nodeMap_.find(t->targetNodeId);
                if (srcIt == nodeMap_.end() || dstIt == nodeMap_.end())
                    continue;

                ImVec2 srcScreenPos(srcIt->second.pos.x + canvasOffset_.x, srcIt->second.pos.y + canvasOffset_.y);
                ImVec2 srcSize(srcIt->second.size.x * zoom_, srcIt->second.size.y * zoom_);
                ImVec2 dstScreenPos(dstIt->second.pos.x + canvasOffset_.x, dstIt->second.pos.y + canvasOffset_.y);
                ImVec2 dstSize(dstIt->second.size.x * zoom_, dstIt->second.size.y * zoom_);

                ImVec2 srcPos(srcScreenPos.x + srcSize.x, srcScreenPos.y + srcSize.y * 0.5f);
                ImVec2 dstPos(dstScreenPos.x, dstScreenPos.y + dstSize.y * 0.5f);

                ImVec2 cp1(srcPos.x + 50.0f * zoom_, srcPos.y);
                ImVec2 cp2(dstPos.x - 50.0f * zoom_, dstPos.y);

                int segments = 20;
                for (int i = 0; i < segments; ++i) {
                    float tt  = i / static_cast<float>(segments);
                    float tt2 = tt * tt;
                    float tt3 = tt2 * tt;
                    float mt  = 1.0f - tt;
                    float mt2 = mt * mt;
                    float mt3 = mt2 * mt;

                    float x = mt3 * srcPos.x + 3 * mt2 * tt * cp1.x + 3 * mt * tt2 * cp2.x + tt3 * dstPos.x;
                    float y = mt3 * srcPos.y + 3 * mt2 * tt * cp1.y + 3 * mt * tt2 * cp2.y + tt3 * dstPos.y;

                    float ntt  = (i + 1) / static_cast<float>(segments);
                    float ntt2 = ntt * ntt;
                    float ntt3 = ntt2 * ntt;
                    float mnt  = 1.0f - ntt;
                    float mnt2 = mnt * mnt;
                    float mnt3 = mnt2 * mnt;

                    float nx = mnt3 * srcPos.x + 3 * mnt2 * ntt * cp1.x + 3 * mnt * ntt2 * cp2.x + ntt3 * dstPos.x;
                    float ny = mnt3 * srcPos.y + 3 * mnt2 * ntt * cp1.y + 3 * mnt * ntt2 * cp2.y + ntt3 * dstPos.y;

                    bool   isSelected = (selectedTransitionId_ == t->id);
                    ImVec4 connColor  = isSelected ? ImVec4(1.0f, 0.8f, 0.1f, 1.0f) : ImVec4(0.4f, 0.6f, 0.8f, 0.6f);

                    ImGui::GetWindowDrawList()->AddLine(ImVec2(x, y), ImVec2(nx, ny),
                        ImGui::ColorConvertFloat4ToU32(connColor),
                        isSelected ? 3.0f : 2.0f);
                }

                float midT  = 0.5f;
                float midMt = 1.0f - midT;
                float midX  = midMt * midMt * midMt * srcPos.x + 3 * midMt * midMt * midT * cp1.x + 3 * midMt * midT * midT * cp2.x + midT * midT * midT * dstPos.x;
                float midY  = midMt * midMt * midMt * srcPos.y + 3 * midMt * midMt * midT * cp1.y + 3 * midMt * midT * midT * cp2.y + midT * midT * midT * dstPos.y;

                ImGui::GetWindowDrawList()->AddText(
                    ImVec2(midX, midY),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.8f, 0.85f, 0.95f, 0.9f)),
                    t->name.c_str());
            }
        }
    }

    inline void AnimationGraphEditor::renderConnectPreview() {
        if (!isConnecting_ || draggingOutputNode_ < 0)
            return;

        auto srcIt = nodeMap_.find(draggingOutputNode_);
        if (srcIt == nodeMap_.end() || !srcIt->second.node)
            return;

        ImVec2 srcScreenPos(srcIt->second.pos.x + canvasOffset_.x, srcIt->second.pos.y + canvasOffset_.y);
        ImVec2 srcSize(srcIt->second.size.x * zoom_, srcIt->second.size.y * zoom_);
        ImVec2 srcPos(srcScreenPos.x + srcSize.x, srcScreenPos.y + srcSize.y * 0.5f);

        ImVec2 dstPos(connectMousePos_);

        ImVec2 cp1(srcPos.x + 50.0f * zoom_, srcPos.y);
        ImVec2 cp2(dstPos.x - 50.0f * zoom_, dstPos.y);

        int segments = 20;
        for (int i = 0; i < segments; ++i) {
            float tt  = i / static_cast<float>(segments);
            float tt2 = tt * tt;
            float tt3 = tt2 * tt;
            float mt  = 1.0f - tt;
            float mt2 = mt * mt;
            float mt3 = mt2 * mt;

            float x = mt3 * srcPos.x + 3 * mt2 * tt * cp1.x + 3 * mt * tt2 * cp2.x + tt3 * dstPos.x;
            float y = mt3 * srcPos.y + 3 * mt2 * tt * cp1.y + 3 * mt * tt2 * cp2.y + tt3 * dstPos.y;

            float ntt  = (i + 1) / static_cast<float>(segments);
            float ntt2 = ntt * ntt;
            float ntt3 = ntt2 * ntt;
            float mnt  = 1.0f - ntt;
            float mnt2 = mnt * mnt;
            float mnt3 = mnt2 * mnt;

            float nx = mnt3 * srcPos.x + 3 * mnt2 * ntt * cp1.x + 3 * mnt * ntt2 * cp2.x + ntt3 * dstPos.x;
            float ny = mnt3 * srcPos.y + 3 * mnt2 * ntt * cp1.y + 3 * mnt * ntt2 * cp2.y + ntt3 * dstPos.y;

            bool dash = (i % 3) == 0;
            if (!dash) {
                ImGui::GetWindowDrawList()->AddLine(ImVec2(x, y), ImVec2(nx, ny),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.7f, 0.95f, 0.8f)),
                    2.0f);
            }
        }

        for (auto& entry : nodeMap_) {
            if (entry.second.node == nullptr)
                continue;
            ImVec2 nScreenPos(entry.second.pos.x + canvasOffset_.x, entry.second.pos.y + canvasOffset_.y);
            ImVec2 nSize(entry.second.size.x * zoom_, entry.second.size.y * zoom_);
            ImVec2 mousePos = ImGui::GetMousePos();
            if (mousePos.x > nScreenPos.x && mousePos.x < nScreenPos.x + nSize.x &&
                mousePos.y > nScreenPos.y && mousePos.y < nScreenPos.y + nSize.y) {
                if (entry.first != draggingOutputNode_) {
                    connectTargetNodeId_ = entry.first;
                    ImVec2 nScreenPosMax(nScreenPos.x + nSize.x, nScreenPos.y + nSize.y);
                    ImGui::GetWindowDrawList()->AddRect(nScreenPos, nScreenPosMax,
                        ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.9f, 0.3f, 0.6f)),
                        6.0f, 0, 3.0f);
                } else {
                    connectTargetNodeId_ = -1;
                }
            } else {
                connectTargetNodeId_ = -1;
            }
        }
    }

    inline void AnimationGraphEditor::renderPropertyPanel(std::shared_ptr<AnimationGraph> graph,
        std::shared_ptr<AnimationController>                                              controller) {
        ImGui::TextDisabled("Properties");
        ImGui::Separator();

        auto getMutableTrans = [&](int transId) -> const AnimationTransition* {
            if (!graph || !graph->getEntryNode())
                return nullptr;
            auto trans = graph->getTransitions(graph->getEntryNode()->id);
            for (const auto* t : trans) {
                if (t->id == transId)
                    return t;
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
                        graphModified_ = true;
                    }
                }
            }
        } else if (selectedTransitionId_ >= 0 && graph) {
            const AnimationTransition* transOrig = getMutableTrans(selectedTransitionId_);
            if (!transOrig)
                return;
            AnimationTransition* trans = const_cast<AnimationTransition*>(transOrig);

            ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.2f, 1.0f), "Transition: %s", trans->name.c_str());
            ImGui::Text("ID: %d", trans->id);
            ImGui::Text("From: %d -> To: %d", trans->sourceNodeId, trans->targetNodeId);

            int condInt = static_cast<int>(trans->condition);
            ImGui::Combo("Condition",
                const_cast<int*>(&condInt),
                "None\0Time-Based\0Event-Based\0Param-Based\0Blend-Complete\0");

            if (condInt != static_cast<int>(trans->condition)) {
                graphModified_ = true;
            }

            if (trans->condition == TransitionCondition::TIME_BASED) {
                ImGui::DragFloat("Time Threshold", const_cast<float*>(&trans->timeThreshold), 0.1f, 0.1f, 100.0f, "%.1f s");
            } else if (trans->condition == TransitionCondition::EVENT_BASED) {
                std::string tempEventName = trans->eventName;
                char*       buf           = const_cast<char*>(tempEventName.data());
                if (ImGui::InputText("Event Name", buf, tempEventName.capacity() + 1)) {
                    trans->eventName = tempEventName;
                    graphModified_   = true;
                }
            } else if (trans->condition == TransitionCondition::PARAM_BASED) {
                std::string tempParamName = trans->paramName;
                char*       buf           = const_cast<char*>(tempParamName.data());
                if (ImGui::InputText("Param Name", buf, tempParamName.capacity() + 1)) {
                    trans->paramName = tempParamName;
                    graphModified_   = true;
                }
                ImGui::DragFloat("Param Value", const_cast<float*>(&trans->paramValue), 0.01f, -100.0f, 100.0f, "%.3f");
            }

            int blendInt = 0;
            if (trans->blendMode == "crossfade")
                blendInt = 1;
            else if (trans->blendMode == "instant")
                blendInt = 2;
            ImGui::Combo("Blend Mode", const_cast<int*>(&blendInt),
                "Fade\0Crossfade\0Instant\0");
            if (blendInt == 1)
                trans->blendMode = "crossfade";
            else if (blendInt == 2)
                trans->blendMode = "instant";
            else
                trans->blendMode = "fade";

            ImGui::DragFloat("Blend Duration", const_cast<float*>(&trans->blendDuration), 0.01f, 0.0f, 10.0f, "%.3f s");

            ImGui::Checkbox("Default Transition", const_cast<bool*>(&trans->isDefault));
        } else {
            ImGui::TextDisabled("No selection");
            ImGui::TextDisabled("Click a node or connection to inspect");
        }

        ImGui::Separator();
        if (ImGui::SmallButton("+ Add Node")) {
            showAddNode_ = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Add new animation state node");
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("+ Add Transition")) {
            showAddTransition_ = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Add transition between nodes");
        }

        ImGui::SameLine();
        if (selectedNodeId_ >= 0 && ImGui::SmallButton("Delete")) {
            graphModified_  = true;
            selectedNodeId_ = -1;
        }
        if (selectedTransitionId_ >= 0 && ImGui::SmallButton("Delete")) {
            graphModified_        = true;
            selectedTransitionId_ = -1;
        }
    }

    inline void AnimationGraphEditor::renderPlayControls(std::shared_ptr<AnimationGraph> graph,
        std::shared_ptr<AnimationController>                                             controller,
        float                                                                            delta) {
        ImGui::TextDisabled("Preview");
        ImGui::Separator();

        if (graph) {
            const auto* current      = graph->getCurrentNode();
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

        if (ImGui::SmallButton("Play")) {
            isPlaying_ = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Step")) {
            graphModified_ = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Stop")) {
            isPlaying_   = false;
            previewTime_ = 0.0f;
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Play/step the animation graph to visualize state transitions");
        }

        ImGui::Text("Preview Time: %.3fs", previewTime_);
        ImGui::DragFloat("Preview Speed", &previewTime_, 0.1f, 0.0f, 100.0f, "%.2f s");
    }

    inline void AnimationGraphEditor::handleConnectInteraction(std::shared_ptr<AnimationGraph> graph) {
        if (isConnecting_ && connectTargetNodeId_ >= 0) {
            if (ImGui::IsMouseReleased(0)) {
                newTransitionSource_        = draggingOutputNode_;
                newTransitionTarget_        = connectTargetNodeId_;
                newTransitionName_          = "Transition_" + std::to_string(draggingOutputNode_) + "_to_" + std::to_string(connectTargetNodeId_);
                newTransitionCondition_     = TransitionCondition::TIME_BASED;
                newTransitionTimeThreshold_ = 0.0f;
                showAddTransition_          = true;
                isConnecting_               = false;
                draggingOutputNode_         = -1;
                connectTargetNodeId_        = -1;
            }
        } else if (isConnecting_ && ImGui::IsMouseReleased(0)) {
            isConnecting_        = false;
            draggingOutputNode_  = -1;
            connectTargetNodeId_ = -1;
        }
    }

    inline void AnimationGraphEditor::handleNodeInteraction() {
        if (draggingNodeId_ >= 0) {
            auto it = nodeMap_.find(draggingNodeId_);
            if (it != nodeMap_.end()) {
                ImVec2 mousePos = ImGui::GetMousePos();

                ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                float  px        = mousePos.x - canvasPos.x - it->second.size.x * zoom_ * 0.5f;
                float  py        = mousePos.y - canvasPos.y - it->second.size.y * zoom_ * 0.5f;
                it->second.pos   = ImVec2(px, py);
            }
        }

        if (ImGui::IsMouseDown(0) && !ImGui::IsAnyItemHovered()) {
            ImVec2 canvasMin = ImGui::GetWindowPos();
            ImVec2 canvasMax(canvasMin.x + ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x,
                canvasMin.y + ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y);
            ImVec2 mousePos = ImGui::GetMousePos();
            if (mousePos.x > canvasMin.x && mousePos.x < canvasMax.x &&
                mousePos.y > canvasMin.y && mousePos.y < canvasMax.y) {
                canvasOffset_.x += ImGui::GetIO().MouseDelta.x;
                canvasOffset_.y += ImGui::GetIO().MouseDelta.y;
            }
        }

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
        std::shared_ptr<AnimationController>                                 controller,
        float                                                                delta) {
        graphModified_        = false;
        selectedNodeId_       = -1;
        selectedTransitionId_ = -1;
        hoveredNodeId_        = -1;
        draggingNodeId_       = -1;
        isConnecting_         = false;
        draggingOutputNode_   = -1;
        connectTargetNodeId_  = -1;

        if (!ImGui::Begin("Animation Graph##graph_editor", nullptr, ImGuiWindowFlags_NoScrollbar)) {
            ImGui::End();
            return false;
        }

        updateNodePositions(graph);

        ImVec2 canvasMin = ImGui::GetCursorScreenPos();
        ImVec2 avail     = ImGui::GetContentRegionAvail();
        ImVec2 canvasMax(canvasMin.x + avail.x, canvasMin.y + avail.y);

        ImGui::GetWindowDrawList()->AddRectFilled(canvasMin, canvasMax, IM_COL32(30, 30, 40, 255));

        float gridSize = 30.0f * zoom_;
        for (float x = fmodf(canvasOffset_.x, gridSize); x < avail.x; x += gridSize) {
            ImVec2   p1(canvasMin.x + x, canvasMin.y);
            ImVec2   p2(canvasMin.x + x, canvasMax.y);
            uint32_t color = IM_COL32(50, 50, 60, 150);
            ImGui::GetWindowDrawList()->AddLine(p1, p2, color);
        }
        for (float y = fmodf(canvasOffset_.y, gridSize); y < avail.y; y += gridSize) {
            ImVec2   p1(canvasMin.x, canvasMin.y + y);
            ImVec2   p2(canvasMax.x, canvasMin.y + y);
            uint32_t color = IM_COL32(50, 50, 60, 150);
            ImGui::GetWindowDrawList()->AddLine(p1, p2, color);
        }

        ImGui::InvisibleButton("graph_canvas", avail);

        renderConnections(graph);

        renderNodes();

        renderConnectPreview();

        handleNodeInteraction();
        handleConnectInteraction(graph);

        handleAddNodeDialog(graph);
        handleAddTransitionDialog(graph);

        if (ImGui::BeginChild("properties_panel", ImVec2(250.0f, 0), true)) {
            renderPropertyPanel(graph, controller);
            renderPlayControls(graph, controller, delta);
        }
        ImGui::EndChild();

        ImGui::End();
        return graphModified_;
    }

    inline void AnimationGraphEditor::handleAddNodeDialog(std::shared_ptr<AnimationGraph> graph) {
        if (!showAddNode_)
            return;

        if (!ImGui::BeginPopupModal("Add Node###add_node_modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            showAddNode_ = false;
            return;
        }

        char nameBuf[256] = "";
        if (!newNodeName_.empty()) {
            strncpy(nameBuf, newNodeName_.c_str(), sizeof(nameBuf) - 1);
            nameBuf[sizeof(nameBuf) - 1] = '\0';
        }

        ImGui::Text("New Node Name:");
        ImGui::InputText("##new_node_name", nameBuf, sizeof(nameBuf));
        if (nameBuf[0] != '\0') {
            newNodeName_ = nameBuf;
        }

        ImGui::Text("Clip Index (-1 for entry/exit):");
        ImGui::InputInt("##new_clip_index", &newClipIndex_);

        ImGui::Checkbox("Entry Node", &newNodeIsEntry_);

        ImGui::Spacing();
        if (ImGui::SmallButton("OK")) {
            if (graph && !newNodeName_.empty()) {
                int newId       = graph->addNode(newNodeName_, newClipIndex_, newNodeIsEntry_);
                graphModified_  = true;
                selectedNodeId_ = newId;
                showAddNode_    = false;
                newNodeName_.clear();
                newClipIndex_   = -1;
                newNodeIsEntry_ = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel")) {
            showAddNode_ = false;
            newNodeName_.clear();
            newClipIndex_   = -1;
            newNodeIsEntry_ = false;
        }

        ImGui::EndPopup();
    }

    inline void AnimationGraphEditor::handleAddTransitionDialog(std::shared_ptr<AnimationGraph> graph) {
        if (!showAddTransition_)
            return;

        if (!ImGui::BeginPopupModal("Add Transition###add_trans_modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            showAddTransition_ = false;
            return;
        }

        std::vector<const AnimationGraphNode*> nodes;
        if (graph) {
            auto allNodes = graph->getAllNodes();
            for (const auto& n : allNodes) {
                nodes.push_back(&n);
            }
        }

        if (nodes.empty()) {
            ImGui::TextDisabled("No nodes available");
        } else {
            int sourceIdx = -1;
            if (newTransitionSource_ >= 0) {
                for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
                    if (nodes[i]->id == newTransitionSource_) {
                        sourceIdx = i;
                        break;
                    }
                }
            }
            if (sourceIdx < 0)
                sourceIdx = 0;
            ImGui::Combo("From Node", &sourceIdx, [](void* data, int idx) -> const char* {
                const auto* nodes = static_cast<const std::vector<const AnimationGraphNode*>*>(data);
                return nodes->at(idx)->name.empty() ? ("Node " + std::to_string(nodes->at(idx)->id)).c_str() : nodes->at(idx)->name.c_str(); }, this, static_cast<int>(nodes.size()));
            newTransitionSource_ = nodes[sourceIdx]->id;

            int targetIdx = -1;
            if (newTransitionTarget_ >= 0) {
                for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
                    if (nodes[i]->id == newTransitionTarget_) {
                        targetIdx = i;
                        break;
                    }
                }
            }
            if (targetIdx < 0)
                targetIdx = 1;
            ImGui::Combo("To Node", &targetIdx, [](void* data, int idx) -> const char* {
                const auto* nodes = static_cast<const std::vector<const AnimationGraphNode*>*>(data);
                return nodes->at(idx)->name.empty() ? ("Node " + std::to_string(nodes->at(idx)->id)).c_str() : nodes->at(idx)->name.c_str(); }, this, static_cast<int>(nodes.size()));
            newTransitionTarget_ = nodes[targetIdx]->id;

            ImGui::Text("Transition Name:");
            char transNameBuf[256] = "";
            if (!newTransitionName_.empty()) {
                strncpy(transNameBuf, newTransitionName_.c_str(), sizeof(transNameBuf) - 1);
                transNameBuf[sizeof(transNameBuf) - 1] = '\0';
            }
            ImGui::InputText("##trans_name", transNameBuf, sizeof(transNameBuf));
            if (transNameBuf[0] != '\0') {
                newTransitionName_ = transNameBuf;
            }

            int condInt = static_cast<int>(newTransitionCondition_);
            ImGui::Combo("Condition", &condInt, "None\0Time-Based\0Event-Based\0Param-Based\0");
            newTransitionCondition_ = static_cast<TransitionCondition>(condInt);

            if (newTransitionCondition_ == TransitionCondition::TIME_BASED) {
                ImGui::DragFloat("Time Threshold (s)", &newTransitionTimeThreshold_, 0.1f, 0.01f, 100.0f, "%.2f");
            }
        }

        ImGui::Spacing();
        if (ImGui::SmallButton("OK")) {
            if (graph && newTransitionSource_ >= 0 && newTransitionTarget_ >= 0) {
                graph->addTransition(
                    newTransitionSource_, newTransitionTarget_,
                    newTransitionName_.empty() ? ("Transition_" + std::to_string(newTransitionSource_) + "_to_" + std::to_string(newTransitionTarget_)) : newTransitionName_,
                    newTransitionCondition_,
                    newTransitionCondition_ == TransitionCondition::TIME_BASED ? newTransitionTimeThreshold_ : 0.0f,
                    newTransitionCondition_ == TransitionCondition::EVENT_BASED ? newTransitionName_ : "",
                    "", 0.0f, 0.25f, false);
                graphModified_       = true;
                showAddTransition_   = false;
                newTransitionSource_ = -1;
                newTransitionTarget_ = -1;
                newTransitionName_.clear();
                newTransitionCondition_ = TransitionCondition::NONE;
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel")) {
            showAddTransition_   = false;
            newTransitionSource_ = -1;
            newTransitionTarget_ = -1;
            newTransitionName_.clear();
            newTransitionCondition_ = TransitionCondition::NONE;
        }

        ImGui::EndPopup();
    }

}  // namespace engine::ui

#endif
