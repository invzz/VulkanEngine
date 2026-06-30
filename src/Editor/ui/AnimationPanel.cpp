#include "Editor/ui/AnimationPanel.hpp"

#include "IconsFontAwesome6.h"
#include <imgui.h>

#include <algorithm>
#include <imgui_internal.h>
#include <iomanip>
#include <sstream>
#include <string>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"

#include "Editor/ui/UI.hpp"

namespace engine {

    AnimationPanel::AnimationPanel(Scene& scene) : scene_(scene) {}

    void AnimationPanel::renderClipControls(EntityAnimData& data, uint32_t entityId, int clipIndex) {
        if (clipIndex < 0 || clipIndex >= static_cast<int>(data.clipNames.size()))
            return;

        const auto& clipName = data.clipNames[clipIndex];
        bool        selected = (data.selectedClipIndex == clipIndex);

        ImGui::PushID(static_cast<int>(entityId) * 1000 + clipIndex);
        if (ui::UI::Selectable(clipName.c_str(), selected)) {
            data.selectedClipIndex = clipIndex;
        }
        if (selected && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Weight: %.2f | Speed: %.2f | Crossfade: %.2fs",
                data.selectedClipWeight, data.selectedClipSpeed, data.selectedCrossfadeDuration);
        }

        // Weight slider
        if (selected) {
            ImGui::SameLine();
            std::string weightLabel = "Weight##w_" + std::to_string(entityId) + "_" + std::to_string(clipIndex);
            if (ui::UI::DragFloat(weightLabel.c_str(), &data.selectedClipWeight, 0.01f, 0.0f, 1.0f)) {
                data.selectedClipWeight = std::clamp(data.selectedClipWeight, 0.0f, 1.0f);
            }

            // Speed slider
            ImGui::SameLine();
            std::string speedLabel = "Speed##s_" + std::to_string(entityId) + "_" + std::to_string(clipIndex);
            if (ui::UI::DragFloat(speedLabel.c_str(), &data.selectedClipSpeed, 0.01f, 0.01f, 5.0f)) {
                data.selectedClipSpeed = std::max(0.01f, data.selectedClipSpeed);
            }

            // Crossfade slider (visual only for now)
            ImGui::SameLine();
            std::string xfLabel = "XF##xf_" + std::to_string(entityId) + "_" + std::to_string(clipIndex);
            if (ui::UI::DragFloat(xfLabel.c_str(), &data.selectedCrossfadeDuration, 0.01f, 0.0f, 10.0f)) {
                data.selectedCrossfadeDuration = std::max(0.0f, data.selectedCrossfadeDuration);
            }
        }

        ImGui::PopID();
    }

    void AnimationPanel::render(FrameInfo& /*frameInfo*/) {
        if (!visible_)
            return;

        // Push theme style
        ui::UI::PushThemeStyle();

        // Use Section for the collapsible header
        bool open = ui::UI::Section((std::string(ICON_FA_STOPWATCH) + " Animation").c_str());
        if (!open) {
            ui::UI::PopThemeStyle();
            return;
        }

        // Collect per-entity animation data
        entityAnimData_.clear();
        {
            auto view = scene_.getRegistry().view<AnimationComponent, ModelComponent>();
            for (auto entity : view) {
                auto& anim = scene_.getRegistry().get<AnimationComponent>(entity);
                if (!anim.controller || !anim.controller->hasActiveClips())
                    continue;

                EntityAnimData ead;
                ead.entityId = static_cast<uint32_t>(entity);

                const auto& clips = anim.controller->getClips();
                for (const auto& clip : clips) {
                    if (clip.active) {
                        std::string name = clip.name.empty() ? ("Clip " + std::to_string(clip.clipIndex)) : clip.name;
                        ead.clipNames.push_back(name);
                        ead.clipDuration.push_back(clip.duration);
                        ead.clipCurrentTime.push_back(clip.currentTime);

                        // Create track data
                        EntityTrackData track;
                        track.name              = name;
                        track.duration          = clip.duration;
                        track.startOffset       = 0.0f;
                        track.weight            = clip.weight;
                        track.speed             = clip.speed;
                        track.crossfadeDuration = 0.0f;
                        track.selected          = false;

                        // Extract keyframe times from clip
                        auto view2 = scene_.getRegistry().view<AnimationComponent>();
                        for (auto ent : view2) {
                            auto& a = scene_.getRegistry().get<AnimationComponent>(ent);
                            if (static_cast<uint32_t>(ent) == ead.entityId && a.controller) {
                                const auto& c = a.controller->getClips();
                                for (const auto& cl : c) {
                                    if (cl.clipIndex == clip.clipIndex) {
                                        track.keyframeTimes.push_back(cl.currentTime);
                                    }
                                }
                            }
                        }

                        ead.tracks.push_back(track);

                        // Persist user selection from previous frame
                        if (ead.selectedClipIndex < 0 || static_cast<size_t>(ead.selectedClipIndex) >= ead.clipNames.size()) {
                            ead.selectedClipIndex = 0;
                        }
                        if (static_cast<size_t>(ead.selectedClipIndex) < ead.clipNames.size()) {
                            ead.selectedClipWeight = clips[ead.selectedClipIndex].weight;
                            ead.selectedClipSpeed  = clips[ead.selectedClipIndex].speed;
                            // Crossfade duration is UI-only for now
                            ead.selectedCrossfadeDuration = 0.0f;
                        }
                    }
                }

                if (!ead.clipNames.empty()) {
                    entityAnimData_.push_back(std::move(ead));
                }
            }
        }

        // Update timeline duration from active clips
        updateTimelineState();

        // ── Timeline ────────────────────────────────────────────────────
        ui::UI::Separator();
        ui::UI::TextDisabled((std::string(ICON_FA_TIMELINE) + " Timeline").c_str());

        float maxDuration = timeline_.totalDuration;
        if (maxDuration < 0.1f)
            maxDuration = 10.0f;
        timeline_.totalDuration = maxDuration;

        // Visible window scales inversely with zoom: wider when zoomed out
        float windowDuration = std::max(5.0f, 20.0f / timeline_.zoom);
        float startTime      = timeline_.currentTime - windowDuration / 2.0f;
        float endTime        = timeline_.currentTime + windowDuration / 2.0f;

        // Timeline bar
        ImVec2 timelineMin = ImGui::GetCursorScreenPos();
        float  timelineW   = ImGui::GetContentRegionAvail().x;
        ImVec2 timelineMax(timelineMin.x + timelineW, timelineMin.y + 60.0f);
        ImGui::InvisibleButton("TimelineBar", ImVec2(timelineW, 60.0f));

        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        // Background
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(timelineMin, timelineMax, IM_COL32(40, 40, 50, 255));

        // Grid lines
        float timeStep = 1.0f / timeline_.zoom;
        if (timeStep > 5.0f)
            timeStep = 5.0f;
        else if (timeStep > 1.0f)
            timeStep = 1.0f;
        else if (timeStep > 0.25f)
            timeStep = 0.25f;
        else
            timeStep = 0.1f;

        for (float t = std::floor(std::max(0.0f, startTime) / timeStep) * timeStep; t < endTime; t += timeStep) {
            float    norm  = (t - startTime) / windowDuration;
            float    x     = timelineMin.x + norm * (timelineMax.x - timelineMin.x);
            uint32_t color = (std::fmod(t, 5.0f) < 0.01f || std::fmod(t, 5.0f) > 5.0f - 0.01f)
                                 ? IM_COL32(80, 80, 100, 255)
                                 : IM_COL32(60, 60, 70, 255);
            drawList->AddLine(ImVec2(x, timelineMin.y), ImVec2(x, timelineMax.y), color, 1.0f);
        }

        // ── Animated Clip Tracks ────────────────────────────────────────
        float trackHeight  = 12.0f;
        float trackSpacing = 18.0f;
        float trackStartY  = timelineMin.y + 4.0f;

        for (const auto& ead : entityAnimData_) {
            for (int i = 0; i < static_cast<int>(ead.tracks.size()); ++i) {
                const auto& track = ead.tracks[i];
                if (track.duration <= 0.0f)
                    continue;

                float trackLeftNorm  = std::max(0.0f, std::min(1.0f, (track.startOffset - startTime) / windowDuration));
                float trackRightNorm = std::max(0.0f, std::min(1.0f, (track.startOffset + track.duration - startTime) / windowDuration));

                float trackLeftX  = timelineMin.x + trackLeftNorm * (timelineMax.x - timelineMin.x);
                float trackRightX = timelineMin.x + trackRightNorm * (timelineMax.x - timelineMin.x);
                float trackW      = std::max(2.0f, trackRightX - trackLeftX);
                float trackY      = trackStartY + (i * trackSpacing);

                bool     isCurrentTrack = (ead.selectedClipIndex == i);
                uint32_t trackColor     = isCurrentTrack ? IM_COL32(100, 180, 255, 200) : IM_COL32(150, 130, 255, 120);
                uint32_t borderColor    = isCurrentTrack ? IM_COL32(150, 200, 255, 255) : IM_COL32(200, 180, 255, 180);

                drawList->AddRectFilled(ImVec2(trackLeftX, trackY), ImVec2(trackRightX, trackY + trackHeight), trackColor, 4.0f);
                drawList->AddRect(ImVec2(trackLeftX, trackY), ImVec2(trackRightX, trackY + trackHeight), borderColor, 4.0f, 0, 1.5f);

                for (float kf : track.keyframeTimes) {
                    if (kf >= startTime && kf <= endTime) {
                        float kfNorm = (kf - startTime) / windowDuration;
                        float kfX    = timelineMin.x + kfNorm * (timelineMax.x - timelineMin.x);
                        float s      = 3.0f;
                        drawList->AddTriangleFilled(ImVec2(kfX, trackY - 2.0f), ImVec2(kfX - s, trackY + 2.0f), ImVec2(kfX + s, trackY + 2.0f), IM_COL32(255, 255, 200, 255));
                    }
                }
            }
        }

        // Playhead
        float playheadNorm = (timeline_.currentTime - startTime) / windowDuration;
        float playheadX    = timelineMin.x + playheadNorm * (timelineMax.x - timelineMin.x);
        drawList->AddLine(ImVec2(playheadX, timelineMin.y), ImVec2(playheadX, timelineMax.y), IM_COL32(100, 200, 255, 255), 2.0f);
        drawList->AddTriangleFilled(ImVec2(playheadX, timelineMax.y), ImVec2(playheadX - 5.0f, timelineMax.y - 8.0f), ImVec2(playheadX + 5.0f, timelineMax.y - 8.0f), IM_COL32(100, 200, 255, 255));

        // Time labels
        for (float t = std::floor(std::max(0.0f, startTime) / timeStep) * timeStep; t < endTime; t += timeStep) {
            float norm = (t - startTime) / windowDuration;
            float x    = timelineMin.x + norm * (timelineMax.x - timelineMin.x);
            if (x > timelineMin.x + 30.0f && x < timelineMax.x - 30.0f) {
                std::stringstream ss;
                ss << std::fixed << std::setprecision(1) << t << "s";
                drawList->AddText(ImVec2(x + 4.0f, timelineMax.y - 12.0f), IM_COL32(150, 150, 180, 255), ss.str().c_str());
            }
        }

        // Scrubbing
        if (ImGui::IsItemHovered() && ImGui::IsMouseDown(0)) {
            float mouseNorm = (ImGui::GetIO().MousePos.x - timelineMin.x) / (timelineMax.x - timelineMin.x);
            mouseNorm       = std::max(0.0f, std::min(1.0f, mouseNorm));
            float mouseTime = startTime + mouseNorm * windowDuration;
            if (mouseTime >= 0.0f && mouseTime <= timeline_.totalDuration) {
                timeline_.currentTime = mouseTime;
                timeline_.isScrubbing = true;

                for (const auto& data : entityAnimData_) {
                    auto view = scene_.getRegistry().view<AnimationComponent>();
                    for (auto entity : view) {
                        auto& anim = scene_.getRegistry().get<AnimationComponent>(entity);
                        if (static_cast<uint32_t>(entity) == data.entityId && anim.controller) {
                            const auto& clips = anim.controller->getClips();
                            for (const auto& clip : clips) {
                                if (clip.active) {
                                    anim.controller->setClipTime(clip.clipIndex, mouseTime);
                                }
                            }
                        }
                    }
                }
            }
        } else {
            timeline_.isScrubbing = false;
        }

        // Time display
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
        std::string timeStr;
        if (!entityAnimData_.empty() && !entityAnimData_[0].clipCurrentTime.empty()) {
            timeStr = std::to_string(entityAnimData_[0].clipCurrentTime[0]);
        }
        ui::UI::Property("Time", timeStr.substr(0, 6).c_str());

        // Zoom controls
        ImGui::SameLine();
        if (ui::UI::SmallButton((std::string(ICON_FA_MAGNIFYING_GLASS_PLUS) + "##anim_zoom_in").c_str())) {
            timeline_.zoom = std::min(timeline_.zoom * 1.5f, 10.0f);
        }
        ImGui::SameLine();
        if (ui::UI::SmallButton((std::string(ICON_FA_MAGNIFYING_GLASS_MINUS) + "##anim_zoom_out").c_str())) {
            timeline_.zoom = std::max(timeline_.zoom / 1.5f, 0.1f);
        }

        // ── Clip List ───────────────────────────────────────────────────
        ui::UI::Separator();
        ui::UI::TextDisabled((std::string(ICON_FA_LIST) + " Active Clips").c_str());

        if (entityAnimData_.empty()) {
            ImGui::Indent();
            ui::UI::TextDisabled("No animations playing");
            ImGui::Unindent();
        }

        for (auto& data : entityAnimData_) {
            std::string entityLabel = "Entity " + std::to_string(data.entityId);

            if (ui::UI::TreeNode("▶", entityLabel.c_str())) {
                std::string playheadTime;
                if (!data.clipCurrentTime.empty()) {
                    playheadTime = std::to_string(data.clipCurrentTime[0]);
                }
                ui::UI::Property("Time", playheadTime.empty() ? "0.0" : playheadTime.substr(0, 6).c_str());

                for (int i = 0; i < static_cast<int>(data.clipNames.size()); ++i) {
                    ImGui::Indent();
                    renderClipControls(data, data.entityId, i);
                    ImGui::Unindent();
                }

                if (data.selectedClipIndex >= 0 && data.selectedClipIndex < static_cast<int>(data.clipNames.size())) {
                    std::string applyId = "Apply##apply_" + std::to_string(data.entityId);
                    if (ui::UI::SmallButton((std::string(ICON_FA_ARROW_RIGHT) + " Apply").c_str())) {
                        auto view = scene_.getRegistry().view<AnimationComponent>();
                        for (auto entity : view) {
                            auto& anim = scene_.getRegistry().get<AnimationComponent>(entity);
                            if (static_cast<uint32_t>(entity) == data.entityId && anim.controller) {
                                const auto& clips = anim.controller->getClips();
                                if (static_cast<size_t>(data.selectedClipIndex) < clips.size()) {
                                    anim.controller->setClipWeight(clips[data.selectedClipIndex].clipIndex,
                                        data.selectedClipWeight);
                                    anim.controller->setClipSpeed(clips[data.selectedClipIndex].clipIndex,
                                        data.selectedClipSpeed);
                                }
                            }
                        }
                    }
                    ImGui::SameLine();
                    ui::UI::TextDisabled("Apply");
                }

                ImGui::TreePop();
            }
        }

        // ── Playback Controls ───────────────────────────────────────────
        ui::UI::Separator();
        ui::UI::TextDisabled((std::string(ICON_FA_CLOCK) + " Playback").c_str());

        if (ui::UI::SmallButton((std::string(ICON_FA_PLAY) + " Play All##anim_play").c_str())) {
            for (auto& data : entityAnimData_) {
                auto view = scene_.getRegistry().view<AnimationComponent>();
                for (auto entity : view) {
                    auto& anim = scene_.getRegistry().get<AnimationComponent>(entity);
                    if (static_cast<uint32_t>(entity) == data.entityId && anim.controller) {
                        anim.controller->stopAll();
                        if (data.selectedClipIndex >= 0) {
                            const auto& clips = anim.controller->getClips();
                            if (static_cast<size_t>(data.selectedClipIndex) < clips.size()) {
                                anim.controller->play(clips[data.selectedClipIndex].clipIndex, *anim.model);
                            }
                        }
                        anim.isPlaying = true;
                    }
                }
            }
        }
        ImGui::SameLine();
        if (ui::UI::SmallButton((std::string(ICON_FA_STOP) + " Stop All##anim_stop").c_str())) {
            for (auto& data : entityAnimData_) {
                auto view = scene_.getRegistry().view<AnimationComponent>();
                for (auto entity : view) {
                    auto& anim = scene_.getRegistry().get<AnimationComponent>(entity);
                    if (static_cast<uint32_t>(entity) == data.entityId && anim.controller) {
                        anim.controller->stopAll();
                        anim.isPlaying = false;
                    }
                }
            }
        }

        // ── Animation Graph Editor ─────────────────────────────────────
        renderGraphEditor();

        ui::UI::PopThemeStyle();
    }

    void AnimationPanel::updateTimelineState() {
        float maxDuration = 0.0f;
        auto  view        = scene_.getRegistry().view<AnimationComponent>();
        for (auto entity : view) {
            const auto& anim = scene_.getRegistry().get<AnimationComponent>(entity);
            if (anim.controller) {
                for (const auto& clip : anim.controller->getClips()) {
                    if (clip.active && clip.duration > maxDuration) {
                        maxDuration = clip.duration;
                    }
                }
            }
        }
        if (maxDuration > 0.0f) {
            timeline_.totalDuration = maxDuration;
        }
    }

    // ── Animation Graph Editor ───────────────────────────────────────────
    void AnimationPanel::renderGraphEditor() {
        if (!showGraphEditor_ || entityAnimData_.empty())
            return;

        // Get the first entity's graph and controller
        std::shared_ptr<AnimationGraph>      graph;
        std::shared_ptr<AnimationController> controller;

        for (const auto& ead : entityAnimData_) {
            auto view = scene_.getRegistry().view<AnimationComponent>();
            for (auto entity : view) {
                auto& anim = scene_.getRegistry().get<AnimationComponent>(entity);
                if (static_cast<uint32_t>(entity) == ead.entityId) {
                    if (anim.graph)
                        graph = anim.graph;
                    if (anim.controller)
                        controller = anim.controller;
                }
            }
        }

        if (!graph)
            return;

        // Toggle button in the panel
        ImGui::SameLine();
        if (ui::UI::SmallButton((std::string(ICON_FA_LINK) + " Graph Editor##anim_graph").c_str())) {
            showGraphEditor_ = !showGraphEditor_;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Toggle animation graph editor");
        }

        if (showGraphEditor_ && graph) {
            // Calculate delta time from timeline
            float delta = 1.0f / 60.0f;  // Assume 60fps for graph stepping
            graphEditor_.render(graph, controller, delta);
        }
    }

}  // namespace engine
