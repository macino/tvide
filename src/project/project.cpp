#include "project.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>
#include <climits>
#include <cstdlib>
#include <cstring>

ProjectManager &ProjectManager::instance()
{
    static ProjectManager pm;
    return pm;
}

bool ProjectManager::isProjectDir(const std::string &path)
{
    struct stat st;
    std::string ideaDir = path + "/.idea";
    return stat(ideaDir.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string ProjectManager::findProjectRoot(const std::string &startPath)
{
    char resolved[PATH_MAX];
    if (!realpath(startPath.c_str(), resolved))
        return "";

    std::string path = resolved;
    while (!path.empty() && path != "/") {
        if (isProjectDir(path))
            return path;
        auto pos = path.rfind('/');
        if (pos == std::string::npos) break;
        path = path.substr(0, pos);
    }
    return "";
}

bool ProjectManager::openProject(const std::string &path)
{
    closeProject();

    char resolved[PATH_MAX];
    if (!realpath(path.c_str(), resolved))
        return false;

    std::string rootPath = resolved;

    if (!isProjectDir(rootPath))
        return false;

    project.rootPath = rootPath;
    project.ideaPath = rootPath + "/.idea";

    // Parse .idea contents
    parseIdeaDir(project.ideaPath);

    // If no name found, use directory name
    if (project.name.empty()) {
        auto pos = rootPath.rfind('/');
        project.name = (pos != std::string::npos)
                        ? rootPath.substr(pos + 1) : rootPath;
    }

    // Default source root is project root
    if (project.sourceRoots.empty())
        project.sourceRoots.push_back(rootPath);

    project.isValid = true;
    return true;
}

void ProjectManager::closeProject()
{
    project = ProjectInfo();
}

bool ProjectManager::parseIdeaDir(const std::string &ideaPath)
{
    parseDotName(ideaPath + "/.name");
    parseModulesXml(ideaPath + "/modules.xml");
    parseVcsXml(ideaPath + "/vcs.xml");
    return true;
}

bool ProjectManager::parseDotName(const std::string &path)
{
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::getline(f, project.name);
    // Trim whitespace
    while (!project.name.empty() && isspace(project.name.back()))
        project.name.pop_back();
    return !project.name.empty();
}

// Simple XML attribute extractor: find attr="value" in a line
std::string ProjectManager::extractAttr(const std::string &line,
                                         const std::string &attr)
{
    std::string needle = attr + "=\"";
    auto pos = line.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    auto end = line.find('"', pos);
    if (end == std::string::npos) return "";
    return line.substr(pos, end - pos);
}

bool ProjectManager::parseModulesXml(const std::string &path)
{
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string line;
    while (std::getline(f, line)) {
        // Look for <module fileurl="..." filepath="..." />
        if (line.find("<module") != std::string::npos) {
            std::string filepath = extractAttr(line, "filepath");
            if (!filepath.empty()) {
                // Replace $PROJECT_DIR$ with actual path
                std::string key = "$PROJECT_DIR$";
                auto pos = filepath.find(key);
                if (pos != std::string::npos)
                    filepath.replace(pos, key.size(), project.rootPath);
                parseImlFile(filepath);
            }
        }
    }
    return true;
}

bool ProjectManager::parseImlFile(const std::string &path)
{
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string line;
    while (std::getline(f, line)) {
        // <sourceFolder url="..." isTestSource="false" />
        if (line.find("<sourceFolder") != std::string::npos) {
            std::string url = extractAttr(line, "url");
            if (!url.empty()) {
                // Replace file://$MODULE_DIR$ with module dir
                std::string moduleDir = path;
                auto pos = moduleDir.rfind('/');
                if (pos != std::string::npos)
                    moduleDir = moduleDir.substr(0, pos);

                std::string key1 = "file://$MODULE_DIR$";
                auto p = url.find(key1);
                if (p != std::string::npos)
                    url.replace(p, key1.size(), moduleDir);

                project.sourceRoots.push_back(url);
            }
        }

        // <excludeFolder url="..." />
        if (line.find("<excludeFolder") != std::string::npos) {
            std::string url = extractAttr(line, "url");
            if (!url.empty()) {
                std::string moduleDir = path;
                auto pos = moduleDir.rfind('/');
                if (pos != std::string::npos)
                    moduleDir = moduleDir.substr(0, pos);

                std::string key1 = "file://$MODULE_DIR$";
                auto p = url.find(key1);
                if (p != std::string::npos)
                    url.replace(p, key1.size(), moduleDir);

                project.excludedDirs.push_back(url);
            }
        }
    }
    return true;
}

bool ProjectManager::parseVcsXml(const std::string &path)
{
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string line;
    while (std::getline(f, line)) {
        // <mapping directory="..." vcs="Git" />
        if (line.find("<mapping") != std::string::npos) {
            std::string vcs = extractAttr(line, "vcs");
            std::string dir = extractAttr(line, "directory");
            if (!vcs.empty()) project.vcsType = vcs;
            if (!dir.empty()) {
                std::string key = "$PROJECT_DIR$";
                auto pos = dir.find(key);
                if (pos != std::string::npos)
                    dir.replace(pos, key.size(), project.rootPath);
                project.vcsRoots.push_back(dir);
            }
        }
    }
    return true;
}
