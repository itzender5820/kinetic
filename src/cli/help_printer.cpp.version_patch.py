with open('/home/claude/kinetic/src/cli/help_printer.cpp', 'r') as f:
    content = f.read()

old = """void print_version() {
    std::cout << c(col::BCYAN, "⚡ Kinetic") << " version "
              << c(col::BOLD,  "1.0.0") << "\\n";
}"""

new = """void print_version() {
    // BUILD_DATE is injected by CMake at compile time
    std::cout << c(col::BCYAN, "⚡ Kinetic") << " version "
              << c(col::BOLD, "1.0.0")
              << c(col::DIM,  "  build:" KINETIC_BUILD_DATE)
              << "\\n";
}"""

content = content.replace(old, new)
with open('/home/claude/kinetic/src/cli/help_printer.cpp', 'w') as f:
    f.write(content)
print("OK")
