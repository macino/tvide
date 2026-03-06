#ifndef TVIDE_PROJECT_H
#define TVIDE_PROJECT_H

#include <string>
#include <vector>

// Represents a JetBrains-compatible project (.idea directory)
struct ProjectInfo {
    std::string name;
    std::string rootPath;
    std::string ideaPath;       // .idea directory path
    std::vector<std::string> sourceRoots;
    std::vector<std::string> excludedDirs;
    std::vector<std::string> vcsRoots;
    std::string vcsType;        // "Git", "svn", etc.
    bool isValid = false;
};

class ProjectManager {
public:
    static ProjectManager &instance();

    bool openProject(const std::string &path);
    void closeProject();
    bool hasProject() const { return project.isValid; }
    const ProjectInfo &getProject() const { return project; }

    // Check if a directory contains a JetBrains .idea directory
    static bool isProjectDir(const std::string &path);
    static std::string findProjectRoot(const std::string &startPath);

private:
    ProjectInfo project;

    bool parseIdeaDir(const std::string &ideaPath);
    bool parseDotName(const std::string &path);
    bool parseModulesXml(const std::string &path);
    bool parseImlFile(const std::string &path);
    bool parseVcsXml(const std::string &path);

    // Simple XML tag/attribute extraction (no full parser needed)
    static std::string extractAttr(const std::string &line,
                                   const std::string &attr);
};

#endif // TVIDE_PROJECT_H
