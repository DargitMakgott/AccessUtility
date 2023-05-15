#include <iostream>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <boost/program_options.hpp>
#include <dirent.h>

namespace po = boost::program_options;


void checkAccess(struct passwd *pwd,  struct group* groupInfo, const char *path) {
    struct stat currentDir{};
    if (lstat(path, &currentDir) != 0){
        std::cerr << "Error getting stat of: " << path << '\n';
        return;
    }

    //Я использую тут функцию strstr(), она проверяет любое вхождение строки так что отсеиваются и пользовательские директории с таким названием
    if(strstr(path, "/sys") != nullptr) return;
    if(strstr(path, "/proc") != nullptr) return;

    DIR* dir = opendir(path);
    if(dir == nullptr) return;

    dirent* entry;
    //Пробег по содержимому директории
    while ((entry = readdir(dir)) != nullptr) {
        std::string fullPath = std::string(path) + "/" + entry->d_name;

        struct stat buf{};
        if (lstat(fullPath.c_str(), &buf) != 0) {
            std::cerr << "Failed to stat file: " << fullPath << '\n';
            continue;
        }

        if (S_ISDIR(buf.st_mode)) {
            //Необходимое условие, так как . и .. тоже являются директориями, и для них может быть вызвана checkAccess
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
                /* Проверка на наличие прав у прав на вход в каталог
                 * проверяется наличие прав у всех пользователей на запуск
                 * проверятеся явлется ли пользователь владельцем и есть ли у владельца права запуска
                 * то же проверяется и для группы
                */
                if ((buf.st_mode & S_IXOTH) != 0 || buf.st_uid == pwd->pw_uid && (buf.st_mode & S_IXUSR) != 0 || buf.st_gid == groupInfo->gr_gid && (buf.st_mode & S_IXGRP) != 0)
                    checkAccess(pwd, groupInfo, fullPath.c_str()); //Рекурсивный вызов функции, только для подкаталога
        }
            /*
             * Проверка на наличие прав на запись
             * если пользователь владелец и у владельца есть право на запись, то условие пройдено
             * если нет, то тоже самое применяется к группе
             * если и это условие не прошло, то проверяется можно ли другим пользователям записывать
             * если условие пройдено, то путь к файлу выводится в консоль
            */
        else if ((buf.st_mode & S_IWOTH) != 0 || (buf.st_uid == pwd->pw_uid && (buf.st_mode & S_IWUSR) != 0) || (buf.st_gid == groupInfo->gr_gid && (buf.st_mode & S_IWGRP) != 0))
            std::cout << fullPath << '\n';
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Error: Incorrect arguments";
        return 1;
    }
    std::string username;
    std::string groupname;
    std::string path;
    //Парсинг аргументов
    try {
        po::options_description desc("Options");
        desc.add_options()
                ("username,u", po::value<std::string>()->required())
                ("groupname,g", po::value<std::string>()->required())
                ("path,p", po::value<std::string>()->required(), "File name");
        po::variables_map variablesMap;
        po::store(po::parse_command_line(argc, argv, desc), variablesMap);
        po::notify(variablesMap);

        username = variablesMap["username"].as<std::string>();
        groupname =variablesMap["groupname"].as<std::string>();
        path = variablesMap["path"].as<std::string>();
    }
    catch (const std::exception& exeption){
        std::cerr << exeption.what() << '\n';
        return 1;
    }

    //Получение информации пользователя
    struct passwd *userInfo = getpwnam(username.c_str());
    if (userInfo == nullptr) {
        std::cerr << "Unknown username: " << username << '\n';
        return 1;
    }
    //Получение информации группы
    struct group* groupInfo = getgrnam(groupname.c_str());
    if(groupInfo == nullptr) {
        std::cerr << "Unknown groupname: " << groupname << '\n';
        return 1;
    }

    checkAccess(userInfo, groupInfo, path.c_str());
}
