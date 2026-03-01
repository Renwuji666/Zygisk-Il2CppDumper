#include <cstring>
#include <thread>
#include <string>
#include <cstdio>
#include <cctype>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cinttypes>
#include "hack.h"
#include "zygisk.hpp"
#include "log.h"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

namespace {

constexpr const char *kConfigPath = "/data/local/tmp/.il2cppdump/config.txt";

std::string Trim(std::string value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

std::string ToLower(std::string value) {
    for (char &ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

bool ParseBool(const std::string &value, bool default_value) {
    const std::string lowered = ToLower(Trim(value));
    if (lowered == "1" || lowered == "true" || lowered == "on" || lowered == "yes" || lowered == "enabled") {
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "off" || lowered == "no" || lowered == "disabled") {
        return false;
    }
    return default_value;
}

bool LoadConfig(std::string &package_name, bool &dump_enabled) {
    FILE *fp = fopen(kConfigPath, "r");
    if (fp == nullptr) {
        LOGW("failed to open config: %s", kConfigPath);
        return false;
    }

    char line[1024];
    bool has_package_name = false;
    bool has_dump_enabled = false;

    while (fgets(line, sizeof(line), fp) != nullptr) {
        std::string row = Trim(line);
        if (row.empty() || row[0] == '#') {
            continue;
        }

        const size_t eq = row.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        std::string key = ToLower(Trim(row.substr(0, eq)));
        std::string value = Trim(row.substr(eq + 1));

        if (key == "package" || key == "package_name" || key == "pkg") {
            package_name = value;
            has_package_name = !package_name.empty();
        } else if (key == "enable" || key == "enabled" || key == "switch" || key == "dump") {
            dump_enabled = ParseBool(value, dump_enabled);
            has_dump_enabled = true;
        }
    }

    fclose(fp);

    if (!has_package_name) {
        LOGW("missing package name in config: %s", kConfigPath);
        return false;
    }
    if (!has_dump_enabled) {
        LOGW("missing dump switch in config: %s", kConfigPath);
        return false;
    }
    return true;
}

}  // namespace

class MyModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
        enable_hack = false;
        game_data_dir = nullptr;
        data = nullptr;
        length = 0;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        auto package_name = env->GetStringUTFChars(args->nice_name, nullptr);
        auto app_data_dir = env->GetStringUTFChars(args->app_data_dir, nullptr);
        preSpecialize(package_name, app_data_dir);
        env->ReleaseStringUTFChars(args->nice_name, package_name);
        env->ReleaseStringUTFChars(args->app_data_dir, app_data_dir);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        if (enable_hack) {
            std::thread hack_thread(hack_prepare, game_data_dir, data, length);
            hack_thread.detach();
        }
    }

private:
    Api *api;
    JNIEnv *env;
    bool enable_hack;
    char *game_data_dir;
    void *data;
    size_t length;

    void preSpecialize(const char *package_name, const char *app_data_dir) {
        std::string target_package_name;
        bool dump_enabled = false;
        if (!LoadConfig(target_package_name, dump_enabled)) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        if (!dump_enabled) {
            LOGI("dump disabled by config switch");
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        if (strcmp(package_name, target_package_name.c_str()) == 0) {
            LOGI("detect game: %s", package_name);
            enable_hack = true;
            game_data_dir = new char[strlen(app_data_dir) + 1];
            strcpy(game_data_dir, app_data_dir);

#if defined(__i386__)
            auto path = "zygisk/armeabi-v7a.so";
#endif
#if defined(__x86_64__)
            auto path = "zygisk/arm64-v8a.so";
#endif
#if defined(__i386__) || defined(__x86_64__)
            int dirfd = api->getModuleDir();
            int fd = openat(dirfd, path, O_RDONLY);
            if (fd != -1) {
                struct stat sb{};
                fstat(fd, &sb);
                length = sb.st_size;
                data = mmap(nullptr, length, PROT_READ, MAP_PRIVATE, fd, 0);
                close(fd);
            } else {
                LOGW("Unable to open arm file");
            }
#endif
        } else {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }
};

REGISTER_ZYGISK_MODULE(MyModule)
