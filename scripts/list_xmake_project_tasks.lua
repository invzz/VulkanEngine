import("core.project.task")

local names = task.names()
if not names or #names == 0 then
    print("(no project tasks found)")
    return
end

table.sort(names)
for _, name in ipairs(names) do
    print(name)
end
