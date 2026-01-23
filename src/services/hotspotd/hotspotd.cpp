#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace {
const char *kDefaultSocketPath = "/run/opendash/hotspotd.sock";
const char *kSocketEnvVar = "OPENDASH_HOTSPOTD_SOCKET";
const char *kSystemctlActiveCmd = "systemctl is-active hostapd 2>/dev/null";
const char *kHostapdStatusCmd = "hostapd_cli status 2>/dev/null";

volatile sig_atomic_t g_running = 1;
int g_server_fd = -1;
std::string g_socket_path;

void handle_signal(int)
{
    g_running = 0;
    if (g_server_fd >= 0)
        close(g_server_fd);
}

std::string trim(const std::string &value)
{
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return std::string();
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string run_command(const char *command)
{
    std::string result;
    FILE *pipe = popen(command, "r");
    if (!pipe)
        return result;

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe))
        result.append(buffer);

    pclose(pipe);
    return result;
}

bool run_system(const char *command)
{
    const int code = std::system(command);
    return code == 0;
}

std::string parse_hostapd_state(const std::string &output)
{
    std::string state;
    size_t start = 0;
    while (start < output.size()) {
        size_t end = output.find('\n', start);
        if (end == std::string::npos)
            end = output.size();

        std::string line = output.substr(start, end - start);
        if (line.rfind("state=", 0) == 0)
            state = line.substr(std::strlen("state="));
        else if (line.rfind("wpa_state=", 0) == 0)
            state = line.substr(std::strlen("wpa_state="));

        if (!state.empty())
            break;
        start = end + 1;
    }

    return trim(state);
}

std::string status_line()
{
    std::string systemd_state = trim(run_command(kSystemctlActiveCmd));
    if (systemd_state.empty())
        systemd_state = "unknown";

    std::string hostapd_state = parse_hostapd_state(run_command(kHostapdStatusCmd));
    if (hostapd_state.empty())
        hostapd_state = "unknown";

    return "ok state=" + systemd_state + " hostapd_state=" + hostapd_state + "\n";
}

void send_response(int client_fd, const std::string &response)
{
    const char *data = response.c_str();
    size_t remaining = response.size();
    while (remaining > 0) {
        ssize_t sent = write(client_fd, data, remaining);
        if (sent <= 0)
            return;
        data += sent;
        remaining -= static_cast<size_t>(sent);
    }
}

std::string read_command(int client_fd)
{
    char buffer[256];
    const ssize_t bytes = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes <= 0)
        return std::string();

    buffer[bytes] = '\0';
    std::string cmd(buffer);
    const auto line_end = cmd.find_first_of("\r\n");
    if (line_end != std::string::npos)
        cmd = cmd.substr(0, line_end);
    return trim(cmd);
}

void ensure_socket_dir(const std::string &path)
{
    const auto slash = path.find_last_of('/');
    if (slash == std::string::npos)
        return;
    const std::string dir = path.substr(0, slash);
    if (dir.empty())
        return;
    if (mkdir(dir.c_str(), 0755) == -1 && errno != EEXIST)
        std::perror("mkdir");
}

void handle_client(int client_fd)
{
    const std::string cmd = read_command(client_fd);
    if (cmd == "status") {
        send_response(client_fd, status_line());
        return;
    }

    if (cmd == "start") {
        if (!run_system("systemctl start hostapd"))
            send_response(client_fd, "error systemctl_start_failed\n");
        else
            send_response(client_fd, status_line());
        return;
    }

    if (cmd == "stop") {
        if (!run_system("systemctl stop hostapd"))
            send_response(client_fd, "error systemctl_stop_failed\n");
        else
            send_response(client_fd, status_line());
        return;
    }

    if (cmd == "restart") {
        if (!run_system("systemctl restart hostapd"))
            send_response(client_fd, "error systemctl_restart_failed\n");
        else
            send_response(client_fd, status_line());
        return;
    }

    send_response(client_fd, "error unknown_command\n");
}
} // namespace

int main()
{
    const char *env_path = std::getenv(kSocketEnvVar);
    g_socket_path = env_path && *env_path ? env_path : kDefaultSocketPath;

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    ensure_socket_dir(g_socket_path);
    unlink(g_socket_path.c_str());

    g_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        std::perror("socket");
        return 1;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", g_socket_path.c_str());

    if (bind(g_server_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        std::perror("bind");
        close(g_server_fd);
        return 1;
    }

    chmod(g_socket_path.c_str(), 0666);

    if (listen(g_server_fd, 5) < 0) {
        std::perror("listen");
        close(g_server_fd);
        unlink(g_socket_path.c_str());
        return 1;
    }

    while (g_running) {
        int client_fd = accept(g_server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EINTR)
                continue;
            break;
        }

        handle_client(client_fd);
        close(client_fd);
    }

    if (g_server_fd >= 0)
        close(g_server_fd);
    unlink(g_socket_path.c_str());
    return 0;
}
