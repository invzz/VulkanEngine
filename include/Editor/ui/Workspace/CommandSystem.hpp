#ifndef EDITOR_WORKSPACE_COMMAND_SYSTEM_HPP
#define EDITOR_WORKSPACE_COMMAND_SYSTEM_HPP

#include <deque>
#include <memory>
#include <string>

namespace engine {

    /**
 * @brief A single undoable command.
 *
 * Commands have an execute() and undo() function. They are stored in a
 * deque and can be pushed to the undo stack.
 */
    class Command {
       public:
        virtual ~Command()                  = default;
        virtual void        execute()       = 0;
        virtual void        undo()          = 0;
        virtual std::string getName() const = 0;
    };

    /**
 * @brief Command system for undo/redo in the editor.
 *
 * Manages a stack of undoable commands. Commands are pushed via execute(),
 * and can be undone/redone via undo() and redo().
 *
 * Thread-safe for reads; writes must be done on the main thread.
 */
    class CommandSystem {
       public:
        CommandSystem();
        ~CommandSystem() = default;

        /**
     * @brief Execute a command and push it to the undo stack.
     * @param command Command to execute.
     * @param name Human-readable name for the command.
     */
        void executeCommand(std::unique_ptr<Command> command, const std::string& name);

        /**
     * @brief Undo the last command.
     * @return true if a command was undone.
     */
        bool undo();

        /**
     * @brief Redo the last undone command.
     * @return true if a command was redone.
     */
        bool redo();

        /**
     * @brief Check if undo is available.
     */
        bool canUndo() const {
            return !undoStack_.empty();
        }

        /**
     * @brief Check if redo is available.
     */
        bool canRedo() const {
            return !redoStack_.empty();
        }

        /**
     * @brief Get the number of undoable commands.
     */
        size_t getUndoCount() const {
            return undoStack_.size();
        }

        /**
     * @brief Get the number of redoable commands.
     */
        size_t getRedoCount() const {
            return redoStack_.size();
        }

        /**
     * @brief Clear all undo/redo history.
     */
        void clear();

        /**
     * @brief Get the name of the last undone command.
     */
        std::string getLastUndoName() const;

        /**
     * @brief Get the name of the last redone command.
     */
        std::string getLastRedoName() const;

       private:
        struct HistoryEntry {
            std::unique_ptr<Command> command;
            std::string              name;
        };

        std::deque<HistoryEntry> undoStack_;
        std::deque<HistoryEntry> redoStack_;

        static constexpr size_t MAX_HISTORY = 64;
    };

}  // namespace engine

#endif
