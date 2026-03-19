// ─────────────────────────────────────────────────────────────────────────────
//  utils/process.cpp
// ─────────────────────────────────────────────────────────────────────────────
#include "process.hpp"
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <climits>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <poll.h>
#include <cstdlib>

namespace kinetic {

// ── which() ──────────────────────────────────────────────────────────────────
std::string which(const std::string& name) {
    // If name contains '/', just return it directly
    if (name.find('/') != std::string::npos) {
        if (access(name.c_str(), X_OK) == 0) return name;
        return {};
    }

    const char* path_env = std::getenv("PATH");
    if (!path_env) return {};

    std::string path_str(path_env);
    size_t start = 0;
    while (start < path_str.size()) {
        size_t colon = path_str.find(':', start);
        std::string dir = (colon == std::string::npos)
                          ? path_str.substr(start)
                          : path_str.substr(start, colon - start);
        start = (colon == std::string::npos) ? path_str.size() : colon + 1;

        std::string full = dir + "/" + name;
        if (access(full.c_str(), X_OK) == 0) return full;
    }
    return {};
}

// ── expand_home() ─────────────────────────────────────────────────────────────
std::string expand_home(const std::string& path) {
    if (path.empty() || path[0] != '~') return path;
    const char* home = std::getenv("HOME");
    if (!home) return path;
    return std::string(home) + path.substr(1);
}

// ── exec_proc() ───────────────────────────────────────────────────────────────
// Uses fork/exec with two pipes (stdout, stderr) and poll() to avoid deadlock.
ProcResult exec_proc(const std::vector<std::string>& argv, const fs::path& cwd) {
    ProcResult result;
    if (argv.empty()) return result;

    // Build argv for execvp
    std::vector<const char*> c_argv;
    c_argv.reserve(argv.size() + 1);
    for (const auto& a : argv) c_argv.push_back(a.c_str());
    c_argv.push_back(nullptr);

    // Create pipes: [0]=read end, [1]=write end
    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
        result.exit_code = -1;
        result.err = "pipe() failed: " + std::string(strerror(errno));
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        result.exit_code = -1;
        result.err = "fork() failed: " + std::string(strerror(errno));
        return result;
    }

    if (pid == 0) {
        // ── Child process ──────────────────────────────────────────────────
        // Redirect stdout → out_pipe write end
        dup2(out_pipe[1], STDOUT_FILENO);
        // Redirect stderr → err_pipe write end
        dup2(err_pipe[1], STDERR_FILENO);

        // Close all unneeded fds
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);

        // Change directory if requested
        if (!cwd.empty()) {
            if (chdir(cwd.c_str()) != 0) {
                _exit(127);
            }
        }

        execvp(c_argv[0], const_cast<char* const*>(c_argv.data()));
        // execvp only returns on error
        _exit(127);
    }

    // ── Parent process ────────────────────────────────────────────────────────
    close(out_pipe[1]); // close write ends in parent
    close(err_pipe[1]);

    // Use poll() to read from both pipes without blocking
    struct pollfd fds[2];
    fds[0].fd = out_pipe[0]; fds[0].events = POLLIN;
    fds[1].fd = err_pipe[0]; fds[1].events = POLLIN;

    char buf[4096];
    int open_count = 2;

    while (open_count > 0) {
        int ready = poll(fds, 2, 5000); // 5s timeout
        if (ready < 0) {
            if (errno == EINTR) continue;
            break;
        }
        for (int i = 0; i < 2; ++i) {
            if (fds[i].fd < 0) continue;
            if (fds[i].revents & (POLLIN | POLLHUP)) {
                ssize_t n = read(fds[i].fd, buf, sizeof(buf));
                if (n > 0) {
                    if (i == 0) result.out.append(buf, (size_t)n);
                    else        result.err.append(buf, (size_t)n);
                } else if (n == 0 || (n < 0 && errno != EAGAIN)) {
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    --open_count;
                }
            }
        }
    }
    // Drain any remaining data
    for (int i = 0; i < 2; ++i) {
        if (fds[i].fd >= 0) {
            ssize_t n;
            while ((n = read(fds[i].fd, buf, sizeof(buf))) > 0) {
                if (i == 0) result.out.append(buf, (size_t)n);
                else        result.err.append(buf, (size_t)n);
            }
            close(fds[i].fd);
        }
    }

    int status = 0;
    waitpid(pid, &status, 0);
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    return result;
}

} // namespace kinetic
