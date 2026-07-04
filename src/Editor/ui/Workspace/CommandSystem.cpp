#include "Editor/ui/Workspace/CommandSystem.hpp"
namespace engine {
    CommandSystem::CommandSystem() = default;
    void CommandSystem::executeCommand(std::unique_ptr<Command> command, const std::string& name) {
        command->execute();
        HistoryEntry entry{std::move(command), name};
        undoStack_.push_back(std::move(entry));
        redoStack_.clear();
        if (undoStack_.size() > MAX_HISTORY) {
            undoStack_.pop_front();
        }
    }
    bool CommandSystem::undo() {
        if (undoStack_.empty())
            return false;
        auto entry = std::move(undoStack_.back());
        entry.command->undo();
        redoStack_.push_back(std::move(entry));
        if (redoStack_.size() > MAX_HISTORY) {
            redoStack_.pop_front();
        }
        return true;
    }
    bool CommandSystem::redo() {
        if (redoStack_.empty())
            return false;
        auto entry = std::move(redoStack_.back());
        entry.command->execute();
        undoStack_.push_back(std::move(entry));
        if (undoStack_.size() > MAX_HISTORY) {
            undoStack_.pop_front();
        }
        return true;
    }
    void CommandSystem::clear() {
        undoStack_.clear();
        redoStack_.clear();
    }
    std::string CommandSystem::getLastUndoName() const {
        if (undoStack_.empty())
            return "";
        return undoStack_.back().name;
    }
    std::string CommandSystem::getLastRedoName() const {
        if (redoStack_.empty())
            return "";
        return redoStack_.back().name;
    }
}  // namespace engine
